#include "EspMQTTClient.h"

void onConnectionEstablished();

EspMQTTClient client(
  "ssid",           // Wifi ssid
  "pass",           // Wifi password
  "192.168.1.101",  // MQTT broker ip
  1883,             // MQTT broker port
  "usr",            // MQTT username
  "mqttpass",       // MQTT password
  "test1",          // Client name
  onConnectionEstablished, // Connection established callback
  true,             // Enable web updater
  true              // Enable debug messages
);

void setup()
{
  Serial.begin(115200);
}

void onConnectionEstablished()
{
  client.subscribe("mytopic/test", [] (const String &payload)
  {
    Serial.println(payload);
  });
  
  client.publish("mytopic/test", "This is a message");

  client.executeDelayed(5 * 1000, []() {
    client.publish("mytopic/test", "This is a message sent 5 seconds later");
  });
}

void loop()
{
  client.loop();
}
