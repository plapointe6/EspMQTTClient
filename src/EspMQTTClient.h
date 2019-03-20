#ifndef ESP_MQTT_CLIENT_H
#define ESP_MQTT_CLIENT_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#define MAX_TOPIC_SUBSCRIPTION_LIST_SIZE 10
#define MAX_DELAYED_EXECUTION_LIST_SIZE 10
#define MAX_MQTT_PAYLOAD_SIZE 256 // Maximum payload size, must correspond to MQTT_MAX_PACKET_SIZE of PubSubClient.h
#define CONNECTION_RETRY_DELAY 10 * 1000

typedef void(*ConnectionEstablishedCallback) ();
typedef void(*MessageReceivedCallback) (const String &message);
typedef void(*DelayedExecutionCallback) ();

class EspMQTTClient 
{
private:
  bool mWifiConnected;
  unsigned long mLastWifiConnectionAttemptMillis;
  unsigned long mLastWifiConnectionSuccessMillis;
  bool mMqttConnected;
  unsigned long mLastMqttConnectionMillis;
	
  const char* mWifiSsid;
  const char* mWifiPassword;

  const char* mMqttServerIp;
  const short mMqttServerPort;
  const char* mMqttUsername;
  const char* mMqttPassword;
  const char* mMqttClientName;

  ConnectionEstablishedCallback mConnectionEstablishedCallback;

  const bool mEnableWebUpdater;
  const bool mEnableSerialLogs;
 
  ESP8266WebServer* mHttpServer;
  ESP8266HTTPUpdateServer* mHttpUpdater;

  WiFiClient* mWifiClient;
  PubSubClient* mMqttClient;

  struct TopicSubscription {
    String topic;
    MessageReceivedCallback callback;
  };
  TopicSubscription mTopicSubscriptionList[MAX_TOPIC_SUBSCRIPTION_LIST_SIZE];
  byte mTopicSubscriptionListSize;
	
  struct DelayedExecutionRecord {
    unsigned long targetMillis;
    DelayedExecutionCallback callback;
  };
  DelayedExecutionRecord mDelayedExecutionList[MAX_DELAYED_EXECUTION_LIST_SIZE];
  byte mDelayedExecutionListSize = 0;

public:
  EspMQTTClient(
    const char wifiSsid[], const char* wifiPassword,
    ConnectionEstablishedCallback connectionEstablishedCallback, const char* mqttServerIp, const short mqttServerPort = 1883,
    const char* mqttUsername = "", const char* mqttPassword = "", const char* mqttClientName = "ESP8266",
    const bool enableWebUpdater = true, const bool enableSerialLogs = true);

  // Legacy constructor
  EspMQTTClient(
    const char wifiSsid[], const char* wifiPassword, const char* mqttServerIp,
    const short mqttServerPort, const char* mqttUsername, const char* mqttPassword,
    const char* mqttClientName, ConnectionEstablishedCallback connectionEstablishedCallback,
    const bool enableWebUpdater = true, const bool enableSerialLogs = true);

  ~EspMQTTClient();

  void loop();
  bool isConnected() const;

  void publish(const String &topic, const String &payload, bool retain = false);
  void subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback);
	
  //Unsubscribes from the topic, if it exists, and removes it from the CallbackList.
  void unsubscribe(const String &topic);
	
  void executeDelayed(const long delay, DelayedExecutionCallback callback);

private:
  void initialize();
  void connectToWifi();
  void connectToMqttBroker();
  void mqttMessageReceivedCallback(char* topic, byte* payload, unsigned int length);
};

#endif