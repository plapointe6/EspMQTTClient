#pragma once
#include "Arduino.h"
#include "EspMQTTClient.h"

// check working example here https://github.com/dino-rider/IotDevice_test

class IotDevice;

class Peripheral: public EspMQTTCaller
{
protected:
  EspMQTTClient *client;
  String topic;
public:
  Peripheral(EspMQTTClient *_client,String _topic);
  void publish(String message, bool retain);
  void subscribe();
  void setTopic(String _topic) {topic=_topic;};
  String getTopic(){return topic};
  virtual void cMessageReceivedCallback(const String &topicStr, const String &message);
};

void Peripheral::publish(const String message, bool retain)
{
 client->publish(topic, message, retain);
}

void Peripheral::subscribe()
{
  client->subscribe(topic, static_cast<EspMQTTCaller*>(this), 0);
}

Peripheral::Peripheral(EspMQTTClient *_client, String _topic): client(_client)
{}

void Peripheral::cMessageReceivedCallback(const String &topicStr, const String &message)
{
  Serial.println(message);
}