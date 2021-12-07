#ifndef ESP_MQTT_CLIENT_H
#define ESP_MQTT_CLIENT_H

#include <PubSubClient.h>
#include <EspSimpleRemoteUpdate.h>
#include <EspSimpleWifiHandler.h>
#include <vector>

#ifdef ESP8266
  #define DEFAULT_MQTT_CLIENT_NAME "ESP8266"
#else // for ESP32
  #define DEFAULT_MQTT_CLIENT_NAME "ESP32"
#endif

void onConnectionEstablished(); // MUST be implemented in your sketch. Called once everythings is connected (Wifi, mqtt).

typedef std::function<void()> ConnectionEstablishedCallback;
typedef std::function<void(const String &message)> MessageReceivedCallback;
typedef std::function<void(const String &topicStr, const String &message)> MessageReceivedCallbackWithTopic;
typedef std::function<void()> DelayedExecutionCallback;

class EspMQTTClient
{
private:
  // Components
  EspSimpleWifiHandler _wifiHandler;
  EspSimpleRemoteUpdate _updater;
  PubSubClient _mqttClient;

  // Wifi related
  bool _wifiConfigured = false;
  bool _wifiConnected = false;
  WiFiClient _wifiClient;

  // MQTT related
  const char* _mqttServerIp;
  const char* _mqttUsername;
  const char* _mqttPassword;
  const char* _mqttClientName;
  short _mqttServerPort;
  bool _mqttConnected = false;
  unsigned long _nextMqttConnectionAttemptMillis = 0;
  unsigned int _mqttReconnectionAttemptDelay = 15 * 1000; // 15 seconds of waiting between each mqtt reconnection attempts by default
  char* _mqttLastWillTopic = 0;
  char* _mqttLastWillMessage = 0;
  bool _mqttLastWillRetain = false;
  bool _mqttCleanSession = true;
  unsigned int _failedMQTTConnectionAttemptCount = 0;

  struct TopicSubscriptionRecord {
    String topic;
    MessageReceivedCallback callback;
    MessageReceivedCallbackWithTopic callbackWithTopic;
  };
  std::vector<TopicSubscriptionRecord> _topicSubscriptionList;

  // Delayed execution related
  struct DelayedExecutionRecord {
    unsigned long targetMillis;
    DelayedExecutionCallback callback;
  };
  std::vector<DelayedExecutionRecord> _delayedExecutionList;

  // General behaviour related
  ConnectionEstablishedCallback _connectionEstablishedCallback;
  bool _enableSerialLogs = false;
  unsigned int _connectionEstablishedCount = 0; // Incremented before each _connectionEstablishedCallback call

public:
  EspMQTTClient(
    // port and client name are swapped here to prevent a collision with the MQTT w/o auth constructor
    const short mqttServerPort = 1883,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  /// Wifi + MQTT with no MQTT authentification
  EspMQTTClient(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServerIp,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME,
    const short mqttServerPort = 1883);

  /// Wifi + MQTT with MQTT authentification
  EspMQTTClient(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServerIp,
    const char* mqttUsername,
    const char* mqttPassword,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME,
    const short mqttServerPort = 1883);

  /// Only MQTT handling (no wifi), with MQTT authentification
  EspMQTTClient(
    const char* mqttServerIp,
    const short mqttServerPort,
    const char* mqttUsername,
    const char* mqttPassword,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  /// Only MQTT handling without MQTT authentification
  EspMQTTClient(
    const char* mqttServerIp,
    const short mqttServerPort,
    const char* mqttClientName = DEFAULT_MQTT_CLIENT_NAME);

  ~EspMQTTClient();


  /* Component getters */

  EspSimpleWifiHandler& wifi() { return _wifiHandler; };
  EspSimpleRemoteUpdate& updater() { return _updater; };


  /* Optional functionality (must be enabled before the first loop() call) */

  // Allow to display useful debugging messages. Can be set to false to disable them during program execution
  void enableDebuggingMessages(const bool enabled = true); 

  // Activate remote sketch update
  void enableHTTPWebUpdater(const char* username, const char* password, const char* address = "/");
  void enableHTTPWebUpdater(const char* address = "/");
  void enableOTA(const char *password = NULL, const uint16_t port = 0); 

  // Tell the broker to establish a persistent connection. Disabled by default.
  inline void enableMQTTPersistence() { _mqttCleanSession = false; }; 

  // Enable the MQTT last will
  void enableLastWillMessage(const char* topic, const char* message, const bool retain = false);

  // Allow to set client name manually
  void setMqttClientName(const char* name); 

  // Allow setting the MQTT info manually 
  void setMqttServer(const char* server, const char* username = "", const char* password = "", const short port = 1883);


  /* Main loop, to call at each sketch loop() */

  void loop();


  /* MQTT related */

  // Pubsubclient >= 2.8; override the default value of MQTT_MAX_PACKET_SIZE
  bool setMaxPacketSize(const uint16_t size);

  // Change the MQTT keepalive interval (15 seconds by default)
  inline void setKeepAlive(uint16_t keepAliveSeconds) { _mqttClient.setKeepAlive(keepAliveSeconds); };

  // Allow to set the minimum delay between each MQTT reconnection attempt. 15 seconds by default.
  inline void setMqttReconnectionAttemptDelay(const unsigned int milliseconds) { _mqttReconnectionAttemptDelay = milliseconds; };

  // Publish / subscribe
  bool publish(const String &topic, const String &payload, bool retain = false);
  bool subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos = 0);
  bool subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos = 0);
  bool unsubscribe(const String &topic);

  // Getters
  inline bool isMqttConnected() const { return _mqttConnected; };
  inline const char* getMqttClientName() { return _mqttClientName; };
  inline const char* getMqttServerIp() { return _mqttServerIp; };
  inline short getMqttServerPort() { return _mqttServerPort; };


  /* Wifi related */

  void setWifiCredentials(const char* wifiSsid, const char* wifiPassword);
  inline bool isWifiConnected() const { return _wifiConnected; }; // Return true if wifi is connected


  /* Others */

  // Execute a task delayed
  void executeDelayed(const unsigned long delay, DelayedExecutionCallback callback);

  // Return true if everything is connected
  inline bool isConnected() const { return isWifiConnected() && isMqttConnected(); }; 

  // Return the number of time onConnectionEstablished has been called since the beginning.
  inline unsigned int getConnectionEstablishedCount() const { return _connectionEstablishedCount; }; 

  // Default to onConnectionEstablished, you might want to override this for special cases like two MQTT connections in the same sketch
  inline void setOnConnectionEstablishedCallback(ConnectionEstablishedCallback callback) { _connectionEstablishedCallback = callback; };


private:
  // WiFi connection events handling
  void _onWifiConnected();
  void _onWifiDisconnected();

  bool _handleWiFi();
  bool _handleMQTT();

  bool _connectToMqttBroker();
  void _processDelayedExecutionRequests();
  bool _mqttTopicMatch(const String &topic1, const String &topic2);
  void _mqttMessageReceivedCallback(char* topic, uint8_t* payload, unsigned int length);
};

#endif
