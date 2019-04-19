#include "EspMQTTClient.h"


// =============== Constructor / destructor ===================

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

// Warning : for old constructor support, this will be deleted soon or later
EspMQTTClient::EspMQTTClient(
  const char* wifiSsid, 
  const char* wifiPassword, 
  const char* mqttServerIp,
  const short mqttServerPort, 
  const char* mqttUsername, 
  const char* mqttPassword,
  const char* mqttClientName, 
  ConnectionEstablishedCallback connectionEstablishedCallback,
  const bool enableWebUpdater, 
  const bool enableSerialLogs) :
  EspMQTTClient(wifiSsid, wifiPassword, mqttServerIp, mqttUsername, mqttPassword, mqttClientName, mqttServerPort)
{
  if (enableWebUpdater)
    enableHTTPWebUpdater();

  if (enableSerialLogs)
    enableDebuggingMessages();

  setOnConnectionEstablishedCallback(connectionEstablishedCallback);

  mShowLegacyConstructorWarning = true;
}

// Warning : for old constructor support, this will be deleted soon or later
EspMQTTClient::EspMQTTClient(
  const char* wifiSsid, 
  const char* wifiPassword,
  ConnectionEstablishedCallback connectionEstablishedCallback, 
  const char* mqttServerIp, 
  const short mqttServerPort,
  const char* mqttUsername,
  const char* mqttPassword, 
  const char* mqttClientName,
  const bool enableWebUpdater,
  const bool enableSerialLogs) :
  EspMQTTClient(wifiSsid, wifiPassword, mqttServerIp, mqttUsername, mqttPassword, mqttClientName, mqttServerPort)
{
  if (enableWebUpdater)
    enableHTTPWebUpdater();

  if (enableSerialLogs)
    enableDebuggingMessages();

  setOnConnectionEstablishedCallback(connectionEstablishedCallback);

  mShowLegacyConstructorWarning = true;
}

EspMQTTClient::EspMQTTClient(
  const char* wifiSsid,
  const char* wifiPassword,
  const char* mqttServerIp,
  const char* mqttUsername,
  const char* mqttPassword,
  const char* mqttClientName,
  const short mqttServerPort) :
  mWifiSsid(wifiSsid),
  mWifiPassword(wifiPassword),
  mMqttServerIp(mqttServerIp),
  mMqttUsername(mqttUsername),
  mMqttPassword(mqttPassword),
  mMqttClientName(mqttClientName),
  mMqttServerPort(mqttServerPort),
  mMqttClient(mqttServerIp, mqttServerPort, mWifiClient)
{
  // WiFi connection
  mWifiConnected = false;
  mLastWifiConnectionAttemptMillis = 0;
  mLastWifiConnectionSuccessMillis = 0;

  // MQTT client
  mTopicSubscriptionListSize = 0;
  mMqttConnected = false;
  mLastMqttConnectionMillis = 0;
  mMqttLastWillTopic = 0;
  mMqttLastWillMessage = 0;
  mMqttLastWillRetain = false;
  mMqttCleanSession = true;
  mMqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {this->mqttMessageReceivedCallback(topic, payload, length);});

  // Web updater
  mUpdateServerAddress = NULL;
  mHttpServer = NULL;
  mHttpUpdater = NULL;

  // other
  mEnableSerialLogs = false;
  mConnectionEstablishedCallback = onConnectionEstablished;
  mShowLegacyConstructorWarning = false;
}

EspMQTTClient::~EspMQTTClient()
{
  if (mHttpServer != NULL)
    delete mHttpServer;
  if (mHttpUpdater != NULL)
    delete mHttpUpdater;
}


// =============== Configuration functions, most of them must be called before the first loop() call ==============

void EspMQTTClient::enableDebuggingMessages(const bool enabled)
{
  mEnableSerialLogs = enabled;
}

void EspMQTTClient::enableHTTPWebUpdater(const char* username, const char* password, const char* address)
{
  if (mHttpServer == NULL)
  {
    mHttpServer = new WebServer(80);
    mHttpUpdater = new ESPHTTPUpdateServer();
    mUpdateServerUsername = (char*)username;
    mUpdateServerPassword = (char*)password;
    mUpdateServerAddress = (char*)address;
  }
  else if (mEnableSerialLogs)
    Serial.print("SYS! You can't call enableHTTPWebUpdater() more than once !\n");
}

void EspMQTTClient::enableHTTPWebUpdater(const char* address)
{
  if(mMqttUsername == NULL || mMqttPassword == NULL)
    enableHTTPWebUpdater("", "", address);
  else
    enableHTTPWebUpdater(mMqttUsername, mMqttPassword, address);
}

void EspMQTTClient::enableMQTTPersistence()
{
  mMqttCleanSession = false;
}

void EspMQTTClient::enableLastWillMessage(const char* topic, const char* message, const bool retain)
{
  mMqttLastWillTopic = (char*)topic;
  mMqttLastWillMessage = (char*)message;
  mMqttLastWillRetain = retain;
}


// =============== Public functions =================

void EspMQTTClient::loop()
{
  long currentMillis = millis();
  
  if (WiFi.status() == WL_CONNECTED)
  {
    // If we just being connected to wifi
    if (!mWifiConnected)
    {
      if (mEnableSerialLogs)
        Serial.printf("WiFi: Connected, ip : %s\n", WiFi.localIP().toString().c_str());

      mLastWifiConnectionSuccessMillis = millis();
      
      // Config of web updater
      if (mHttpServer != NULL)
      {
        MDNS.begin(mMqttClientName);
        mHttpUpdater->setup(mHttpServer, mUpdateServerAddress, mUpdateServerUsername, mUpdateServerPassword);
        mHttpServer->begin();
        MDNS.addService("http", "tcp", 80);
      
        if (mEnableSerialLogs)
          Serial.printf("WEB: Updater ready, open http://%s.local in your browser and login with username '%s' and password '%s'.\n", mMqttClientName, mUpdateServerUsername, mUpdateServerPassword);
      }
  
      mWifiConnected = true;
    }
    
    // MQTT handling
    if (mMqttClient.connected())
      mMqttClient.loop();
    else
    {
      if (mMqttConnected)
      {
        if (mEnableSerialLogs)
          Serial.println("MQTT! Lost connection.");
        
        mTopicSubscriptionListSize = 0;
        mMqttConnected = false;
      }
      
      if (currentMillis - mLastMqttConnectionMillis > CONNECTION_RETRY_DELAY || mLastMqttConnectionMillis == 0)
        connectToMqttBroker();
    }
      
    // Web updater handling
    if (mHttpServer != NULL)
      mHttpServer->handleClient();
  }
  else // If we are not connected to wifi
  {
    if (mWifiConnected) 
    {
      if (mEnableSerialLogs)
        Serial.println("WiFi! Lost connection.");
      
      mWifiConnected = false;

      // If we handle wifi, we force disconnection to clear the last connection
      if (mWifiSsid != NULL)
        WiFi.disconnect();
    }
    
    // We retry to connect to the wifi if we handle it and there was no attempt since the last connection lost
    if (mWifiSsid != NULL && (mLastWifiConnectionAttemptMillis == 0 || mLastWifiConnectionSuccessMillis > mLastWifiConnectionAttemptMillis))
      connectToWifi();
  }
  
  // Delayed execution handling
  if (mDelayedExecutionListSize > 0)
  {
    long currentMillis = millis();
    
    for(byte i = 0 ; i < mDelayedExecutionListSize ; i++)
    {
      if (mDelayedExecutionList[i].targetMillis <= currentMillis)
      {
        (*mDelayedExecutionList[i].callback)();
        for(int j = i ; j < mDelayedExecutionListSize-1 ; j++)
          mDelayedExecutionList[j] = mDelayedExecutionList[j + 1];
        mDelayedExecutionListSize--;
        i--;
      }
    }
  }

  // Old constructor support warning
  if (mEnableSerialLogs && mShowLegacyConstructorWarning)
  {
    mShowLegacyConstructorWarning = false;
    Serial.print("SYS! You are using a constructor that will be deleted soon, please update your code with the new construction format.\n");
  }

}

void EspMQTTClient::publish(const String &topic, const String &payload, bool retain)
{
  mMqttClient.publish(topic.c_str(), payload.c_str(), retain);

  if (mEnableSerialLogs)
    Serial.printf("MQTT << [%s] %s\n", topic.c_str(), payload.c_str());
}

void EspMQTTClient::subscribe(const String &topic, MessageReceivedCallback messageReceivedCallback)
{
  // Check the possibility to add a new topic
  if (mTopicSubscriptionListSize >= MAX_TOPIC_SUBSCRIPTION_LIST_SIZE) 
  {
    if (mEnableSerialLogs)
      Serial.println("MQTT! Subscription list is full, ignored.");
    return;
  }

  // Check the duplicate of the subscription to the topic
  bool found = false;
  for (int i = 0; i < mTopicSubscriptionListSize && !found; i++)
    found = mTopicSubscriptionList[i].topic.equals(topic);

  if (found) 
  {
    if (mEnableSerialLogs)
      Serial.printf("MQTT! Subscribed to [%s] already, ignored.\n", topic.c_str());
    return;
  }

  // All checks are passed - do the job
  mMqttClient.subscribe(topic.c_str());
  mTopicSubscriptionList[mTopicSubscriptionListSize++] = { topic, messageReceivedCallback };
  
  if (mEnableSerialLogs)
    Serial.printf("MQTT: Subscribed to [%s]\n", topic.c_str());
}

void EspMQTTClient::unsubscribe(const String &topic)
{
  bool found = false;

  for (int i = 0; i < mTopicSubscriptionListSize; i++)
  {
    if (!found)
    {
      if (mTopicSubscriptionList[i].topic.equals(topic))
      {
        found = true;
        mMqttClient.unsubscribe(topic.c_str());
        if (mEnableSerialLogs)
          Serial.printf("MQTT: Unsubscribed from %s\n", topic.c_str());
      }
    }

    if (found)
    {
      if ((i + 1) < MAX_TOPIC_SUBSCRIPTION_LIST_SIZE)
        mTopicSubscriptionList[i] = mTopicSubscriptionList[i + 1];
    }
  }

  if (found)
    mTopicSubscriptionListSize--;
  else if (mEnableSerialLogs)
    Serial.println("MQTT! Topic cannot be found to unsubscribe, ignored.");
}

void EspMQTTClient::executeDelayed(const long delay, DelayedExecutionCallback callback)
{
  if (mDelayedExecutionListSize < MAX_DELAYED_EXECUTION_LIST_SIZE)
  {
    DelayedExecutionRecord delayedExecutionRecord;
    delayedExecutionRecord.targetMillis = millis() + delay;
    delayedExecutionRecord.callback = callback;
    
    mDelayedExecutionList[mDelayedExecutionListSize] = delayedExecutionRecord;
    mDelayedExecutionListSize++;
  }
  else if (mEnableSerialLogs)
    Serial.printf("SYS! The list of delayed functions is full.\n");
}


// ================== Private functions ====================-

void EspMQTTClient::connectToWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(mWifiSsid, mWifiPassword);

  if (mEnableSerialLogs)
    Serial.printf("\nWiFi: Connecting to %s ... \n", mWifiSsid);
  
  mLastWifiConnectionAttemptMillis = millis();
}

void EspMQTTClient::connectToMqttBroker()
{
  if (mEnableSerialLogs)
    Serial.printf("MQTT: Connecting to broker @%s ... ", mMqttServerIp);

  if (mMqttClient.connect(mMqttClientName, mMqttUsername, mMqttPassword, mMqttLastWillTopic, 0, mMqttLastWillRetain, mMqttLastWillMessage, mMqttCleanSession))
  {
    mMqttConnected = true;
    
    if (mEnableSerialLogs) 
      Serial.println("ok.");

    (*mConnectionEstablishedCallback)();
  }
  else if (mEnableSerialLogs)
  {
    Serial.print("unable to connect, ");

    switch (mMqttClient.state())
    {
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
  }
  
  mLastMqttConnectionMillis = millis();
}

void EspMQTTClient::mqttMessageReceivedCallback(char* topic, byte* payload, unsigned int length)
{
  // Convert the payload into a String
  // First, We ensure that we dont bypass the maximum size of the PubSubClient library buffer that originated the payload
  // This buffer has a maximum length of MQTT_MAX_PACKET_SIZE and the payload begin at "headerSize + topicLength + 1"
  unsigned int strTerminationPos;
  if (strlen(topic) + length + 9 >= MQTT_MAX_PACKET_SIZE)
  {
    strTerminationPos = length - 1;

    if (mEnableSerialLogs)
      Serial.print("MQTT! Your message may be truncated, please change MQTT_MAX_PACKET_SIZE of PubSubClient.h to a higher value.\n");
  }
  else
    strTerminationPos = length;

  // Second, we add the string termination code at the end of the payload and we convert it to a String object
  payload[strTerminationPos] = '\0';
  String payloadStr((char*)payload);

  // Logging
  if (mEnableSerialLogs)
    Serial.printf("MQTT >> [%s] %s\n", topic, payloadStr.c_str());

  // Send the message to subscribers
  for (int i = 0 ; i < mTopicSubscriptionListSize ; i++)
  {
    if (mTopicSubscriptionList[i].topic.equals(topic))
      (*mTopicSubscriptionList[i].callback)(payloadStr); // Call the callback
  }
}
