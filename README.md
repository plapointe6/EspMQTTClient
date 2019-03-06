# MQTT and Wifi handling for ESP8266

This library is intended to encapsulate the handling of WiFi and MQTT connections of an ESP8266.
You just need to provide your credentials and it will manage the following things: 
- Connecting to a WiFi network.
- Connecting to a MQTT broker.
- Automatically detecting connection lost either from the WiFi client of MQTT broker.
- Automatically attempting to reconnect when the either WiFi or MQTT connection is lost.
- Subscrubing/unsubscrubing to/from MQTT topics.
- Provide a callback handling to advise when we are connected to the MQTT broker or a subscribed MQTT topic received a message.
- Running an HTTP server secured by a password to allow remote update.
- Printing usefull debug informations, switch the possibility off whenever debug is no longer needed.

## Dependency

The MQTT communication depends on the PubSubClient Library (https://github.com/knolleary/pubsubclient).

## Functions

- publish(topic, message) : publish a message to the provided MQTT topic
- subscribe(topic, callback) : subscribe to the specified topic and call the provided callback passing the received message.
- unsubscribe(topic) : unsubscribe from the specified topic.
- bool isConnected() : Return true if everything is connected.
- executeDelayed(milliseconds, callback) : As ESP8366 does not like to be interrupted too long with the delay function, this function will allow a delayed execution of a function whitout interrupting the sketch.

## Example

```c++
EspMQTTClient client(...);

void onConnectionEstablished() {

  client.subscribe("mytopic/test", [] (const String &payload)  {
    Serial.println(payload);
  });

  client.publish("mytopic/test", "This is a message");
}

```

See "Esp8266MQTTClient.ino" for the complete example.
