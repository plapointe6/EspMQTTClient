/*
  SimpleMQTTClientOTA.ino
  The purpose of this exemple is to illustrate a simple handling of MQTT, Wifi and OTA connection.
  Once it connects successfully to a Wifi network and a MQTT broker, it subscribe to a topic and send a message to it.
  It will also send a message delayed 5 seconds later.
  You can update firmware with Arduino IDE OTA
*/

#include "EspMQTTClient.h"
#include <ArduinoOTA.h>

// No MQTT password, default MQTT port, 
// client name will be setup later in code
EspMQTTClient client(
  "WifiSSID",
  "WifiPassword",
  "192.168.1.100"  // MQTT Broker server ip
  );

// Topic will be dynamic and client name related
// example tele/SimpleMQTTClient/ESP32-ae30
String topicbase = "tele/SimpleMQTTClient/"  ;
String hostname ;

void setup()
{
  Serial.begin(115200);
  char buff[16] ;

  // Here the hostname will be also the MQTT client name and
  #ifdef ESP32
  sprintf_P(buff, PSTR("ESP32-%04x"), (uint32_t) (ESP.getEfuseMac() & 0xFFFF) );
  #else
  sprintf_P(buff, PSTR("ESP8266-%04x"), (uint32_t) (ESP.getChipId() & 0xFFFF) );
  #endif

  hostname = buff;
  topicbase += hostname ;

  Serial.println("\r\nSimpleMQTTClientOTA");
  Serial.print("hostname : ");
  Serial.println(hostname);
  Serial.print("topic    : ");
  Serial.println(topicbase);

  // 1st thing to do because internally it will also be used for other stuff
  client.setMqttClientName(hostname.c_str());

  // Optionnal functionnalities of EspMQTTClient : 
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  client.enableOTA(); // Enable OTA here, OTA networkk name will be MQTT client hostname (so hostname here)
  //client.enableOTA("my_ota_password"); // Enable OTA with password
  //client.enableOTA("my_ota_password"; 32000); // Enable OTA with password and port

  // You can activate the retain flag by setting the third parameter to true
  client.enableLastWillMessage(topicbase+ "/LWT", "Offline", true);  

  // This is the $1 000 0000 question should we leave this here to be able to manage 
  // ourselve or do we need to to this in class adding callback if needed ?
  ArduinoOTA
    .onStart([]() {
      String type = "OTA Updating ";
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type += "sketch";
      } else { // U_SPIFFS 
        type += "filesystem";
        // if updating SPIFFS this would be the place to unmount SPIFFS 
        //SPIFFS.end();
      }

      // Publish status message 
      client.publish(topicbase + "/LWT", type); 

      // Looks like it works fine without closing MQTT
      //Serial.println("Closing MQTT");
      //client.disconnectMqtt();
      Serial.println("Starting " + type);

    })
    .onEnd([]() {
      // Publish status message 
      client.publish(topicbase + "/LWT", "Updated, restarting"); 
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      String err = "Error " + String(error) + " ";
      if (error == OTA_AUTH_ERROR) err+= "Auth";
      else if (error == OTA_BEGIN_ERROR) err+= "Begin";
      else if (error == OTA_CONNECT_ERROR) err+= "Connect";
      else if (error == OTA_RECEIVE_ERROR) err+= "Receive";
      else if (error == OTA_END_ERROR) err+= "End";

      err += " failed";
      // Publish status message 
      client.publish(topicbase + "/LWT", err); 

      Serial.println(err);
    });
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  // Publish a message to indicate we're online
  client.publish(topicbase+ "/LWT", "Online", true); 

  // Subscribe to broker information and display received message to Serial
  client.subscribe("$SYS/broker/version", [](const String & payload) {
    Serial.println(payload);
  });

  // Subscribe to "testle/#" and display received message to Serial
  client.subscribe("tele/#", [](const String & topic, const String & payload) {
    Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  });


  // Execute delayed instructions
  client.executeDelayed(5 * 1000, []() {
    client.publish(topicbase+"/test", "This is a message sent 5 seconds later");
  });
}

void loop()
{
  client.loop();
}
