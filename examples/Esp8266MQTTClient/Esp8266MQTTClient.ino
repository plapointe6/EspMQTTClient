#include "EspMQTTClient.h"

void onConnectionEstablished();

/*
EspMQTTClient client(
  "ssid",                 // Wifi ssid
  "pass",                 // Wifi password
  onConnectionEstablished,// Connection established callback
  "ip",                   // MQTT broker ip
  1883,                   // MQTT broker port
  "mqttusr",              // MQTT username
  "mqttpass",             // MQTT password
  "test",                 // Client name
  true,                   // Enable web updater
  true                    // Enable debug messages
);
*/

EspMQTTClient client(
  "ssid",                 // Wifi ssid
  "pass",                 // Wifi password
  onConnectionEstablished,// MQTT connection established callback
  "ip"                    // MQTT broker ip
);

void setup()
{
  Serial.begin(115200);
}

void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("mytopic/test", [](const String & payload) {
    Serial.println(payload);
  });

  // Publish a message to "mytopic/test"
  client.publish("mytopic/test", "This is a message");

  // Execute delayed instructions
  client.executeDelayed(5 * 1000, []() {
    client.publish("mytopic/test", "This is a message sent 5 seconds later");
  });
}

void loop()
{
  client.loop();
}
