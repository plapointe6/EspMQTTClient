#include "EspMQTTClient.h"


// =============== Constructor / destructor ===================

// default constructor
EspMQTTClient::EspMQTTClient(
  const short mqttServerPort,
  const char* mqttClientName) :
  EspMQTTClient(nullptr, mqttServerPort, mqttClientName)
{
}

// MQTT only (no wifi connection attempt)
EspMQTTClient::EspMQTTClient(
  const char* mqttServerIp,
  const short mqttServerPort,
  const char* mqttClientName) :
  EspMQTTClient(NULL, NULL, mqttServerIp, NULL, NULL, mqttClientName, mqttServerPort)
{
}

EspMQTTClient::EspMQTTClient(
  const char* mqttServerIp,
  const short mqttServerPort,
  const char* mqttUsername,
  const char* mqttPassword,
  const char* mqttClientName) :
  EspMQTTClient(NULL, NULL, mqttServerIp, mqttUsername, mqttPassword, mqttClientName, mqttServerPort)
{
}

// Wifi and MQTT handling
EspMQTTClient::EspMQTTClient(
  const char* wifiSsid,
  const char* wifiPassword,
  const char* mqttServerIp,
  const char* mqttClientName,
  const short mqttServerPort) :
  EspMQTTClient(wifiSsid, wifiPassword, mqttServerIp, NULL, NULL, mqttClientName, mqttServerPort)
{
}

EspMQTTClient::EspMQTTClient(
  const char* wifiSsid,
  const char* wifiPassword,
  const char* mqttServerIp,
  const char* mqttUsername,
  const char* mqttPassword,
  const char* mqttClientName,
  const short mqttServerPort) :
  _mqttClient(mqttServerIp, mqttServerPort, _wifiClient),
  _mqttServerIp(mqttServerIp),
  _mqttUsername(mqttUsername),
  _mqttPassword(mqttPassword),
  _mqttClientName(mqttClientName),
  _mqttServerPort(mqttServerPort)
{
  // WiFi connection
  if (wifiSsid) {
    _wifiHandler.setWifiInfos(wifiSsid, wifiPassword);
    _wifiHandler.setAutoReconnect(true);
    _wifiConfigured = true;
  }

  if (mqttClientName)
    _wifiHandler.setHostname(mqttClientName);

  _wifiHandler.onConnectionEstablished([this]{ this->_onWifiConnected(); });
  _wifiHandler.onConnectionLost([this]{ this->_onWifiDisconnected(); });

  // MQTT client
  _mqttClient.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {this->_mqttMessageReceivedCallback(topic, payload, length);});

  // other
  _connectionEstablishedCallback = onConnectionEstablished;
}

EspMQTTClient::~EspMQTTClient(){}


// =============== Configuration functions, must be called before the first loop() call ==============

void EspMQTTClient::enableDebuggingMessages(const bool enabled) {
  _enableSerialLogs = enabled;
  _updater.enableDebuggingMessages();
  _wifiHandler.enableDebuggingMessages();
}

void EspMQTTClient::enableHTTPWebUpdater(const char* username, const char* password, const char* address) {
  _updater.enableHTTPWebUpdater(username, password, address);
}

void EspMQTTClient::enableHTTPWebUpdater(const char* address) {
  if (_mqttUsername == NULL || _mqttPassword == NULL)
    enableHTTPWebUpdater("", "", address);
  else
    enableHTTPWebUpdater(_mqttUsername, _mqttPassword, address);
}

void EspMQTTClient::enableOTA(const char *password, const uint16_t port) {
  _updater.enableOTA(password, port);
}

void EspMQTTClient::enableLastWillMessage(const char* topic, const char* message, const bool retain) {
  _mqttLastWillTopic = (char*)topic;
  _mqttLastWillMessage = (char*)message;
  _mqttLastWillRetain = retain;
}

void EspMQTTClient::setMqttClientName(const char* name) {
   _mqttClientName = name; 
   _wifiHandler.setHostname(name); 
};

void EspMQTTClient::setMqttServer(const char* server, const char* username, const char* password, const short port) {
  _mqttServerIp   = server;
  _mqttUsername   = username;
  _mqttPassword   = password;
  _mqttServerPort = port;
};


// =============== Main loop / connection state handling =================

void EspMQTTClient::loop() {
  // WIFI handling
  bool wifiStateChanged = _handleWiFi();

  // If there is a change in the wifi connection state, don't handle the mqtt connection state right away.
  // We will wait at least one lopp() call. This prevent the library from doing too much thing in the same loop() call.
  if (wifiStateChanged)
    return;

  // MQTT Handling
  bool mqttStateChanged = _handleMQTT();
  if (mqttStateChanged)
    return;

  // handle the web updater
  _updater.handle();

  // Process the delayed execution commands
  _processDelayedExecutionRequests(); 
}

bool EspMQTTClient::_handleWiFi() {
  // When it's the first call, reset the wifi radio and schedule the wifi connection
  static bool firstLoopCall = true;
  if (_wifiConfigured && firstLoopCall) {
    _wifiHandler.beginConnection();
    firstLoopCall = false;
    return true;
  }

  // Get the current connection status
  bool isWifiConnected = _wifiHandler.isConnected();

  // Detect and return if there was a change in the WiFi state
  if (isWifiConnected != _wifiConnected) {
    _wifiConnected = isWifiConnected;
    return true;
  } else {
    return false;
  }
}


bool EspMQTTClient::_handleMQTT() {
  // PubSubClient main loop() call
  _mqttClient.loop();

  // Get the current connection status
  bool isMqttConnected = (isWifiConnected() && _mqttClient.connected());

  // Connection established
  if (isMqttConnected && !_mqttConnected) {
    _mqttConnected = true;
    _connectionEstablishedCount++;
    _connectionEstablishedCallback();
  }

  // Connection lost
  else if (!isMqttConnected && _mqttConnected) {
    if (_enableSerialLogs) {
      Serial.printf("MQTT! Lost connection (%fs). \n", millis()/1000.0);
      Serial.printf("MQTT: Retrying to connect in %i seconds. \n", _mqttReconnectionAttemptDelay / 1000);
    }
    _nextMqttConnectionAttemptMillis = millis() + _mqttReconnectionAttemptDelay;
  }

  // It's time to connect to the MQTT broker
  else if (isWifiConnected() && _nextMqttConnectionAttemptMillis > 0 && millis() >= _nextMqttConnectionAttemptMillis) {
    // Connect to MQTT broker
    if(_connectToMqttBroker()) {
      _failedMQTTConnectionAttemptCount = 0;
      _nextMqttConnectionAttemptMillis = 0;
    } else {
      // Connection failed, plan another connection attempt
      _nextMqttConnectionAttemptMillis = millis() + _mqttReconnectionAttemptDelay;
      _mqttClient.disconnect();
      _failedMQTTConnectionAttemptCount++;

      if (_enableSerialLogs)
        Serial.printf("MQTT!: Failed MQTT connection count: %i \n", _failedMQTTConnectionAttemptCount);
    }
  }

  // Detect and return if there was a change in the MQTT state
  if (_mqttConnected != isMqttConnected) {
    _mqttConnected = isMqttConnected;
    return true;
  } else {
    return false;
  }
}

void EspMQTTClient::_onWifiConnected() {
  // At least 500 miliseconds of waiting before an mqtt connection attempt.
  // Some people have reported instabilities when trying to connect to 
  // the mqtt broker right after being connected to wifi.
  // This delay prevent these instabilities.
  _nextMqttConnectionAttemptMillis = millis() + 500;
}

void EspMQTTClient::_onWifiDisconnected() {}


// =============== Public functions for interaction with thus lib =================


bool EspMQTTClient::setMaxPacketSize(const uint16_t size) {

  bool success = _mqttClient.setBufferSize(size);

  if (!success && _enableSerialLogs)
    Serial.println("MQTT! failed to set the max packet size.");

  return success;
}

bool EspMQTTClient::publish(const String &topic, const String &payload, bool retain) {
  // Do not try to publish if MQTT is not connected.
  if (!isConnected()) {
    if (_enableSerialLogs)
      Serial.println("MQTT! Trying to publish when disconnected, skipping.");
    return false;
  }

  bool success = _mqttClient.publish(topic.c_str(), payload.c_str(), retain);

  if (_enableSerialLogs) {
    if(success)
      Serial.printf("MQTT << [%s] %s\n", topic.c_str(), payload.c_str());
    else
      Serial.println("MQTT! publish failed, is the message too long ? (see setMaxPacketSize())"); // This can occurs if the message is too long according to the maximum defined in PubsubClient.h
  }

  return success;
}

bool EspMQTTClient::subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback, uint8_t qos) {
  // Do not try to subscribe if MQTT is not connected.
  if (!isConnected()) {
    if (_enableSerialLogs)
      Serial.println("MQTT! Trying to subscribe when disconnected, skipping.");
    return false;
  }

  bool success = _mqttClient.subscribe(topic.c_str(), qos);

  if (success) {
    // Add the record to the subscription list only if it does not exists.
    bool found = false;
    for (std::size_t i = 0; i < _topicSubscriptionList.size() && !found; i++)
      found = _topicSubscriptionList[i].topic.equals(topic);

    if (!found)
      _topicSubscriptionList.push_back({ topic, messageReceivedCallback, NULL });
  }
  
  if (_enableSerialLogs) {
    if (success)
      Serial.printf("MQTT: Subscribed to [%s]\n", topic.c_str());
    else
      Serial.println("MQTT! subscribe failed");
  }

  return success;
}

bool EspMQTTClient::subscribe(const String &topic, MessageReceivedCallbackWithTopic messageReceivedCallback, uint8_t qos) {
  if (subscribe(topic, (MessageReceivedCallback)NULL, qos)) {
    _topicSubscriptionList[_topicSubscriptionList.size()-1].callbackWithTopic = messageReceivedCallback;
    return true;
  } else {
    return false;
  }
}

bool EspMQTTClient::unsubscribe(const String &topic) {
  // Do not try to unsubscribe if MQTT is not connected.
  if (!isConnected()) {
    if (_enableSerialLogs)
      Serial.println("MQTT! Trying to unsubscribe when disconnected, skipping.");
    return false;
  }

  for (std::size_t i = 0; i < _topicSubscriptionList.size(); i++) {
    if (_topicSubscriptionList[i].topic.equals(topic)) {
      if (_mqttClient.unsubscribe(topic.c_str())) {
        _topicSubscriptionList.erase(_topicSubscriptionList.begin() + i);
        i--;
        if (_enableSerialLogs)
          Serial.printf("MQTT: Unsubscribed from %s\n", topic.c_str());
      } else {
        if (_enableSerialLogs)
          Serial.println("MQTT! unsubscribe failed");
        return false;
      }
    }
  }

  return true;
}

void EspMQTTClient::setWifiCredentials(const char* wifiSsid, const char* wifiPassword) {
  _wifiHandler.setWifiInfos(wifiSsid, wifiPassword);
  _wifiConfigured = true;
}

void EspMQTTClient::executeDelayed(const unsigned long delay, DelayedExecutionCallback callback) {
  DelayedExecutionRecord delayedExecutionRecord;
  delayedExecutionRecord.targetMillis = millis() + delay;
  delayedExecutionRecord.callback = callback;
  _delayedExecutionList.push_back(delayedExecutionRecord);
}


// ================== Private functions ====================-


// Try to connect to the MQTT broker and return True if the connection is successfull (blocking)
bool EspMQTTClient::_connectToMqttBroker() {
  bool success = false;

  if (_mqttServerIp != nullptr && strlen(_mqttServerIp) > 0) {
    if (_enableSerialLogs) {
      if (_mqttUsername)
        Serial.printf("MQTT: Connecting to broker \"%s\" with client name \"%s\" and username \"%s\" ... (%fs)", _mqttServerIp, _mqttClientName, _mqttUsername, millis()/1000.0);
      else
        Serial.printf("MQTT: Connecting to broker \"%s\" with client name \"%s\" ... (%fs)", _mqttServerIp, _mqttClientName, millis()/1000.0);
    }

    // explicitly set the server/port here in case they were not provided in the constructor
    _mqttClient.setServer(_mqttServerIp, _mqttServerPort);
    success = _mqttClient.connect(_mqttClientName, _mqttUsername, _mqttPassword, _mqttLastWillTopic, 0, _mqttLastWillRetain, _mqttLastWillMessage, _mqttCleanSession);
  } else {
    if (_enableSerialLogs)
      Serial.printf("MQTT: Broker server ip is not set, not connecting (%fs)\n", millis()/1000.0);
    success = false;
  }

  if (_enableSerialLogs) {
    if (success) {
      Serial.printf(" - ok. (%fs) \n", millis()/1000.0);
    } else {
      Serial.printf("unable to connect (%fs), reason: ", millis()/1000.0);

      switch (_mqttClient.state()) {
        case -4:
          Serial.println("MQTT_CONNECTION_TIMEOUT");
          break;
        case -3:
          Serial.println("MQTT_CONNECTION_LOST");
          break;
        case -2:
          Serial.println("MQTT_CONNECT_FAILED");
          break;
        case -1:
          Serial.println("MQTT_DISCONNECTED");
          break;
        case 1:
          Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
          break;
        case 2:
          Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
          break;
        case 3:
          Serial.println("MQTT_CONNECT_UNAVAILABLE");
          break;
        case 4:
          Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
          break;
        case 5:
          Serial.println("MQTT_CONNECT_UNAUTHORIZED");
          break;
      }

      Serial.printf("MQTT: Retrying to connect in %i seconds.\n", _mqttReconnectionAttemptDelay / 1000);
    }
  }

  return success;
}

// Delayed execution handling. 
// Check if there is delayed execution requests to process and execute them if needed.
void EspMQTTClient::_processDelayedExecutionRequests() {
  if (_delayedExecutionList.size() > 0) {
    unsigned long currentMillis = millis();

    for (std::size_t i = 0 ; i < _delayedExecutionList.size() ; i++) {
      if (_delayedExecutionList[i].targetMillis <= currentMillis) {
        _delayedExecutionList[i].callback();
        _delayedExecutionList.erase(_delayedExecutionList.begin() + i);
        i--;
      }
    }
  }
}

/**
 * Matching MQTT topics, handling the eventual presence of a single wildcard character
 *
 * @param topic1 is the topic may contain a wildcard
 * @param topic2 must not contain wildcards
 * @return true on MQTT topic match, false otherwise
 */
bool EspMQTTClient::_mqttTopicMatch(const String &topic1, const String &topic2) {
  int i = 0;

  if ((i = topic1.indexOf('#')) >= 0) {
    String t1a = topic1.substring(0, i);
    String t1b = topic1.substring(i+1);
    if ((t1a.length() == 0 || topic2.startsWith(t1a)) &&
       (t1b.length() == 0 || topic2.endsWith(t1b)))
      return true;
  } 
  else if ((i = topic1.indexOf('+')) >= 0) {
    String t1a = topic1.substring(0, i);
    String t1b = topic1.substring(i+1);

    if ((t1a.length() == 0 || topic2.startsWith(t1a))&&
       (t1b.length() == 0 || topic2.endsWith(t1b))) {
      if (topic2.substring(t1a.length(), topic2.length()-t1b.length()).indexOf('/') == -1)
        return true;
    }
  } 
  else {
    return topic1.equals(topic2);
  }

  return false;
}

void EspMQTTClient::_mqttMessageReceivedCallback(char* topic, uint8_t* payload, unsigned int length) {
  // Convert the payload into a String
  // First, We ensure that we dont bypass the maximum size of the PubSubClient library buffer that originated the payload
  // This buffer has a maximum length of _mqttClient.getBufferSize() and the payload begin at "headerSize + topicLength + 1"
  unsigned int strTerminationPos;
  if (strlen(topic) + length + 9 >= _mqttClient.getBufferSize()) {
    strTerminationPos = length - 1;
    if (_enableSerialLogs)
      Serial.print("MQTT! Your message may be truncated, please set setMaxPacketSize() to a higher value.\n");
  }
  else
    strTerminationPos = length;

  // Second, we add the string termination code at the end of the payload and we convert it to a String object
  payload[strTerminationPos] = '\0';
  String payloadStr((char*)payload);
  String topicStr(topic);

  // Logging
  if (_enableSerialLogs)
    Serial.printf("MQTT >> [%s] %s\n", topic, payloadStr.c_str());

  // Send the message to subscribers
  for (std::size_t i = 0 ; i < _topicSubscriptionList.size() ; i++) {
    if (_mqttTopicMatch(_topicSubscriptionList[i].topic, String(topic))) {
      if(_topicSubscriptionList[i].callback != NULL)
        _topicSubscriptionList[i].callback(payloadStr); // Call the callback
      if(_topicSubscriptionList[i].callbackWithTopic != NULL)
        _topicSubscriptionList[i].callbackWithTopic(topicStr, payloadStr); // Call the callback
    }
  }
}
