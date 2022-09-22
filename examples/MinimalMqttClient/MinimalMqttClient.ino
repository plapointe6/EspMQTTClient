/*
  BasicMQTTClient.ino
  This shows the basic handling of MQTT and Wifi connection.
  Once it connects successfully to a Wifi network and a MQTT broker,
  it subscribes to a topic and send a message to it.
*/

#include "EspMQTTClient.h"

// Change these to customize where data will go
String topic = "topicname";
char userID[] = "yourname";

EspMQTTClient client(
  "WifiSSID",
  "WifiPassword",
  "192.168.1.100",  // MQTT Broker server ip
  "MQTTUsername",   // Can be omitted if not needed
  "MQTTPassword",   // Can be omitted if not needed
  userID            // Client name that uniquely identify your device
);

void setup()
{
  Serial.begin(115200);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe(topic+"/"+userID, [](const String & payload) {
    Serial.println(payload);
  });

  // Publish a message to "mytopic/test"
  client.publish(topic+"/"+userID, "This is a message"); // You can activate the retain flag by setting the third parameter to true
}

void loop()
{
  client.loop();
}
