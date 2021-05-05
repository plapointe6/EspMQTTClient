/*
  DelayedMQTTClientConfiguration.ino
  The purpose of this example is to demonstrate using the default constructor in order to delay specifying
  connection information until we are inside setup(). This allows us to load configuration data from EEPROM
  instead of relying on hardcoded information. EEPROM provisioning is outside the scope of this example but
  typically I provide an interface for setting configuration data via UART.

  This example focuses on using Arduino EEPROM along with the default constructor, if you would like more
  information on basic module usage please see SimpleMQTTClient.ino in the examples directory.
 */
#include <EEPROM.h>
#include "EspMQTTClient.h"

#define CONFIG_START 32        // position in EEPROM where our first byte gets written
#define CONFIG_VERSION "00001" // version string to let us compare current to whatever is in EEPROM

EspMQTTClient client;          // using the default constructor

/*
  This defines the struct we will use for storing our configuration data to EEPROM, it is an example however
  most any struct can be written to EEPROM if it fits of course
 */
typedef struct {
  char version[6];      // Version of the configuration in EEPROM, used in case we change the struct
  uint8_t debug;        // Debug on yes/no 1/0
  char nodename[32];    // this node name
  char ssid[32];        // WiFi SSID
  char password[64];    // WiFi Password
  char mqttip[32];      // Mosquitto server IP
  uint16_t mqttport;    // Mosquitto port
  char mqttuser[32];    // Mosquitto Username (if needed, or "")
  char mqttpass[64];    // Moqsuitto Password (if needed, or "")
} configuration_t;

/*
  Declare a default configuration_t to use in case there is no EEPROM data, otherwise this gets
  overwritten by whatever is in EEPROM
 */
configuration_t CONFIGURATION = {
  CONFIG_VERSION,
  1,
  "TestClient",
  "WiFiSSID",
  "WifiPassword",
  "192.168.1.100",
  1883,
  "MQTTUsername",
  "MQTTPassword"
};

/* 
  Load whats in EEPROM in to CONFIGURATION if the version number matches
 */
int loadConfig() {
  // validate its the correct version (and therefore the same struct...)
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2] &&
      EEPROM.read(CONFIG_START + 3) == CONFIG_VERSION[3] &&
      EEPROM.read(CONFIG_START + 4) == CONFIG_VERSION[4]) {
    // and if so read configuration into struct
    EEPROM.get(CONFIG_START, CONFIGURATION);
    return 1;
  }
  return 0;
}

/*
  Save the current CONFIGURATION definition to EEPROM
 */
void saveConfig() {
  EEPROM.put(CONFIG_START, CONFIGURATION);
  EEPROM.commit();
}

void setup()
{
  Serial.begin(115200);
  // need to make sure we have enough space for our struct and the offset
  EEPROM.begin(sizeof(configuration_t) + CONFIG_START);

  // attempt to load the current configuration from EEPROM, if it fails
  // then save the default struct to the EEPROM
  if (!loadConfig()) {
    saveConfig();
  }

  // enable debug messages if our configuration tells us to
  if (CONFIGURATION.debug)
    client.enableDebuggingMessages();

  // Set the WiFi and MQTT information that we loaded from EEPROM (or defaults as the case may be)
  client.setWifiCredentials(CONFIGURATION.ssid, CONFIGURATION.password);
  client.setMqttClientName(CONFIGURATION.nodename);
  client.setMqttServer(CONFIGURATION.mqttip, CONFIGURATION.mqttuser, CONFIGURATION.mqttpass, CONFIGURATION.mqttport);
  EEPROM.end();
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("mytopic/test", [](const String & payload) {
    Serial.println(payload);
  });

  // Publish a message to "mytopic/test"
  client.publish("mytopic/test", "This is a message"); // You can activate the retain flag by setting the third parameter to true
}

void loop()
{
  client.loop();
}
