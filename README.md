# MQTT and Wifi handling for ESP8266 and ESP32

This library is intended to encapsulate the handling of WiFi and MQTT connections of an ESP8266/ESP32.
You just need to provide your credentials and it will manage the following things: 
- Connecting to a WiFi network.
- Connecting to a MQTT broker.
- Automatically detecting connection lost either from the WiFi client or the MQTT broker and it will retry a connection automatically.
- Subscribing/unsubscrubing to/from MQTT topics by a friendly callback system.
- Supports a single occurrence of a '+' or '#' wildcard in subscriptions
- Provide a callback handling to advise once everything is connected (Wifi and MQTT).
- Provide a function to enable printing of useful debug information related to MQTT and Wifi connections.
- Provide some other useful utilities for MQTT and Wifi management.
- Provide a function to enable an HTTP Update server secured by a password to allow remote update.

## Dependency

The MQTT communication depends on the PubSubClient Library (https://github.com/knolleary/pubsubclient).

From PubSubClient:
"The maximum message size, including header, is 128 bytes by default. This is configurable via `MQTT_MAX_PACKET_SIZE` in `PubSubClient.h`"

## Example

```c++
#include "EspMQTTClient.h"

EspMQTTClient client(
  "WifiSSID",
  "WifiPassword",
  "192.168.1.100",  // MQTT Broker server ip
  "MQTTUsername",   // Can be omitted if not needed
  "MQTTPassword",   // Can be omitted if not needed
  "TestClient"      // Client name that uniquely identify your device
);

void setup() {}

void onConnectionEstablished() {

  client.subscribe("mytopic/test", [] (const String &payload)  {
    Serial.println(payload);
  });

  client.publish("mytopic/test", "This is a message");
}

void loop() {
  client.loop();
}
```

See "SimpleMQTTClient.ino" for the complete example.


## Documentation

### Construction

For Wifi and MQTT connection handling (Recommended) :
```c++
  EspMQTTClient(
    const char* wifiSsid,
    const char* wifiPassword,
    const char* mqttServerIp,
    const char* mqttUsername,  // Omit this parameter to disable MQTT authentification
    const char* mqttPassword,  // Omit this parameter to disable MQTT authentification
    const char* mqttClientName = "ESP8266",
    const short mqttServerPort = 1883);
```

MQTT connection handling only :
```c++
  EspMQTTClient(
    const char* mqttServerIp,
    const short mqttServerPort,  // It is mandatory here to allow these constructors to be distinct from those with the Wifi handling parameters
    const char* mqttUsername,    // Omit this parameter to disable MQTT authentification
    const char* mqttPassword,    // Omit this parameter to disable MQTT authentification
    const char* mqttClientName = "ESP8266");
```

### Functions

IMPORTANT : Must be called at each loop() of your sketch
```c++
void loop();
```

Basic functions for MQTT communications.
```c++
bool publish(const String &topic, const String &payload, bool retain = false);
bool subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback);
bool unsubscribe(const String &topic);
```

Enable the display of usefull debugging messages that will output to serial.
```c++
void enableDebuggingMessages(const bool enabled = true)
```

Enable the web updater. This will host a simple form that will allow firmware upgrade (using, e.g., the `.bin` file produced by "Export Compiled Binary" in the Arduino IDE's "Sketch" menu). Must be set before the first loop() call.
```c++
void enableHTTPWebUpdater(const char* username, const char* password, const char* address = "/");

// this one will set user and password equal to those set for the MQTT connection.
void enableHTTPWebUpdater(const char* address = "/");
```

Enable last will message. Must be set before the first loop() call.
```c++
void enableLastWillMessage(const char* topic, const char* message, const bool retain = false);
```

Connection status
```c++
bool isConnected(); // Return true if everything is connected.
bool isWifiConnected(); // Return true if WiFi is connected.
bool isMqttConnected(); // Return true if MQTT is connected.
```

Return the number of time onConnectionEstablished has been called since the beginning. Can be useful if you need to monitor the number of times the connection has dropped.
```c++
void getConnectionEstablishedCount();
```

As ESP8366 does not like to be interrupted too long with the delay() function, this function will allow a delayed execution of a function without interrupting the sketch.
```c++
void executeDelayed(const long delay, DelayedExecutionCallback callback);
```

### Connection established callback

To allow this library to work, you need to implement the onConnectionEstablished() function in your sketch.

```c++
void onConnectionEstablished()
{
  // Here you are sure that everything is connected.
}
```

In some special cases, like if you want to handle more than one MQTT connection in the same sketch, you can override this callback to another one for the second MQTT client using this function : 
```c++
void setOnConnectionEstablishedCallback(ConnectionEstablishedCallback callback);
```
See exemple "twoMQTTClientHandling.ino" for more details.

