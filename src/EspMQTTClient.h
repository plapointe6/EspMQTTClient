#ifndef ESP_MQTT_CLIENT_H
#define ESP_MQTT_CLIENT_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

typedef void(*ConnectionEstablishedCallback) ();
typedef void(*MessageReceivedCallback) (const String &message);
typedef void(*DelayedExecutionCallback) ();

class EspMQTTClient
{
private:
  static const byte max_callback_list_size = 10;
  static const byte max_to_execute_list_size = 10;
  static const int max_mqtt_payload_size = 256;
  static const int connection_retry_delay = 10 * 1000;

  bool mWifiConnected;
  long mLastWifiConnectionMillis;
  bool mMqttConnected;
  long mLastMqttConnectionMillis;
	
  String mWifiSsid;
  String mWifiPassword;

  String mMqttServerIp;
  short mMqttServerPort;
  String mMqttUsername;
  String mMqttPassword;
  String mMqttClientName;

  ConnectionEstablishedCallback mConnectionEstablishedCallback;

  bool mEnableWebUpdater;
  bool mEnableSerialLogs;
 
  ESP8266WebServer* mHttpServer;
  ESP8266HTTPUpdateServer* mHttpUpdater;

  WiFiClient* mWifiClient;
  PubSubClient* mMqttClient;

  struct CallbackRecord {
	  String topic;
    MessageReceivedCallback callback;
  };
  CallbackRecord mCallbackList[max_callback_list_size];
  byte mCallbackListSize;
	
  struct DelayedExecutionRecord {
	  unsigned long targetMillis;
    DelayedExecutionCallback toExecute;
  };
  DelayedExecutionRecord mToExecuteList[max_to_execute_list_size];
  byte mToExecuteListSize = 0;

public:
  EspMQTTClient(const String &wifiSsid, const String &wifiPassword, const String &mqttServerIp, 
    const short mqttServerPort, const String &mqttUsername, const String &mqttPassword, 
	  const String &mqttClientName, ConnectionEstablishedCallback connectionEstablishedCallback, 
    bool enableWebUpdater = true, bool enableSerialLogs = true);
  ~EspMQTTClient();

  void loop();
  bool isConnected() const;

  void publish(const String &topic, const String &payload, bool retain = false);
  void subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback);
	
  //Unsubscribes from the topic, if it exists, and removes it from the CallbackList.
  void unsubscribe(const String &topic);
	
  void executeDelayed(const long delay, DelayedExecutionCallback toExecute);

private:
  void connectToWifi();
  void connectToMqttBroker();
  void mqttMessageReceivedCallback(char* topic, byte* payload, unsigned int length);
};

#endif