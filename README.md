# MQTT and Wifi handling for ESP8266

This library is intended to encapsulate the handling of wifi and MQTT connections of an ESP8266.
You just need to provide your credentials and it will manage these things : 
- Connecting to a wifi network
- Connecting to a MQTT broker
- Running an HTTP server secured by a password to allow remote update.
- Automatically detects connection lost either from the wifi client of MQTT broker.
- Automatically attempt to reconnect when the connection is lost.
- Provide a callback handling to advise when we are connected to the MQTT broker.
- Allow to print usefull debug informations.

Was tested with a Wemos D1 mini, feel free to test it with another ESP'S and provide feedback.


## Dependency

The MQTT communication depends on the PubSubClient Library (https://github.com/knolleary/pubsubclient).

## Functions

- publish(String topic, String message) : publish a message to the provided MQTT topic
- subscribe(String topic, callback) : subscribe to the specified topic and call the provided callback passing the received message.
- bool isConnected() : Return true if everything is connected.
- executeDelayed(long milliseconds, callback) : As ESP8366 does not like to be interrupted too long with the delay function, this function will allow a delayed execution of a function whitout interruption the sketch.

## Exemple

See "Esp8266MQTTClient.ino" for the complete exemple

```c++
EspMQTTClient client(...);

void onConnectionEstablished(){

  client.subscribe("mytopic/test", [] (const String &payload)  {
    Serial.println(payload);
  });

  client.publish("mytopic/test", "This is a message");
}

```