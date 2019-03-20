#include "EspMQTTClient.h"


// =============== Constructor / destructor ===================

// Legacy constructor - will be deleted soon or later
EspMQTTClient::EspMQTTClient(
  const char wifiSsid[], const char* wifiPassword, const char* mqttServerIp,
  const short mqttServerPort, const char* mqttUsername, const char* mqttPassword,
  const char* mqttClientName, ConnectionEstablishedCallback connectionEstablishedCallback,
  const bool enableWebUpdater, const bool enableSerialLogs)
  : mWifiSsid(wifiSsid), mWifiPassword(wifiPassword), mMqttServerIp(mqttServerIp),
  mMqttServerPort(mqttServerPort), mMqttUsername(mqttUsername), mMqttPassword(mqttPassword),
  mMqttClientName(mqttClientName), mConnectionEstablishedCallback(connectionEstablishedCallback),
  mEnableWebUpdater(enableWebUpdater), mEnableSerialLogs(enableSerialLogs)
{
  initialize();
}

// Preferred constructor (new form)
EspMQTTClient::EspMQTTClient(
  const char wifiSsid[], const char* wifiPassword,
  ConnectionEstablishedCallback connectionEstablishedCallback, const char* mqttServerIp, const short mqttServerPort,
  const char* mqttUsername, const char* mqttPassword, const char* mqttClientName,
  const bool enableWebUpdater, const bool enableSerialLogs)
  : mWifiSsid(wifiSsid), mWifiPassword(wifiPassword), mMqttServerIp(mqttServerIp),
    mMqttServerPort(mqttServerPort), mMqttUsername(mqttUsername), mMqttPassword(mqttPassword),
    mMqttClientName(mqttClientName), mConnectionEstablishedCallback(connectionEstablishedCallback),
    mEnableWebUpdater(enableWebUpdater), mEnableSerialLogs(enableSerialLogs)
{
  initialize();
}

void EspMQTTClient::initialize()
{
  mTopicSubscriptionListSize = 0;

  // WiFi connection
  mWifiConnected = false;
  mLastWifiConnectionAttemptMillis = 0;
  mLastWifiConnectionSuccessMillis = 0;
  mWifiClient = new WiFiClient();

  // Web updater
  if (mEnableWebUpdater)
  {
    mHttpServer = new ESP8266WebServer(80);
    mHttpUpdater = new ESP8266HTTPUpdateServer();
  }

  // MQTT client
  mMqttConnected = false;
  mLastMqttConnectionMillis = 0;
  mMqttClient = new PubSubClient(mMqttServerIp, mMqttServerPort, *mWifiClient);
  mMqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {this->mqttMessageReceivedCallback(topic, payload, length);});
}

EspMQTTClient::~EspMQTTClient() {}


// =============== Public functions =================

void EspMQTTClient::loop()
{
  long currentMillis = millis();
  
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!mWifiConnected)
    {
      if (mEnableSerialLogs)
        Serial.printf("WiFi: Connected, ip : %s\n", WiFi.localIP().toString().c_str());

      mLastWifiConnectionSuccessMillis = millis();
      
      // Config of web updater
      if (mEnableWebUpdater)
      {
        MDNS.begin(mMqttClientName);
        mHttpUpdater->setup(mHttpServer, "/", mMqttUsername, mMqttPassword);
        mHttpServer->begin();
        MDNS.addService("http", "tcp", 80);
      
        if (mEnableSerialLogs)
          Serial.printf("WEB: Updater ready, open http://%s.local in your browser and login with username '%s' and password '%s'.\n", mMqttClientName, mMqttUsername, mMqttPassword);
      }
  
      mWifiConnected = true;
    }
    
    // MQTT handling
    if (mMqttClient->connected())
      mMqttClient->loop();
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
    if (mEnableWebUpdater)
      mHttpServer->handleClient();
  }
  else
  {
    if (mWifiConnected) 
    {
      if (mEnableSerialLogs)
        Serial.println("WiFi! Lost connection.");
      
      mWifiConnected = false;
      WiFi.disconnect();
    }
    
    // We retry to connect to the wifi if there was no attempt since the last connection lost
    if (mLastWifiConnectionAttemptMillis == 0 || mLastWifiConnectionSuccessMillis > mLastWifiConnectionAttemptMillis)
      connectToWifi();
  }
  
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
}

bool EspMQTTClient::isConnected() const
{
  return mWifiConnected && mMqttConnected;
}

void EspMQTTClient::publish(const String &topic, const String &payload, bool retain)
{
  mMqttClient->publish(topic.c_str(), payload.c_str(), retain);

  if (mEnableSerialLogs)
    Serial.printf("MQTT << [%s] %s.\n", topic.c_str(), payload.c_str());
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
  mMqttClient->subscribe(topic.c_str());
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
        mMqttClient->unsubscribe(topic.c_str());
        if (mEnableSerialLogs)
          Serial.printf("MQTT: Unsubscribed from %s.\n", topic.c_str());
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
    Serial.printf("WiFi: Connecting to %s ... ", mWifiSsid);
  
  mLastWifiConnectionAttemptMillis = millis();
}

void EspMQTTClient::connectToMqttBroker()
{
  if (mEnableSerialLogs)
    Serial.printf("MQTT: Connecting to broker @%s ... ", mMqttServerIp);

  if (mMqttClient->connect(mMqttClientName, mMqttUsername, mMqttPassword))
  {
    mMqttConnected = true;
    
    if (mEnableSerialLogs) 
      Serial.println("ok.");

    (*mConnectionEstablishedCallback)();
  }
  else if (mEnableSerialLogs)
  {
    Serial.print("unable to connect, ");

    switch (mMqttClient->state())
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
  // Convert payload to String
  char buffer[MAX_MQTT_PAYLOAD_SIZE];

  if (length >= MAX_MQTT_PAYLOAD_SIZE)
    length = MAX_MQTT_PAYLOAD_SIZE - 1;

  strncpy(buffer, (char*)payload, length);
  buffer[length] = '\0';

  String payloadStr = buffer;

  if (mEnableSerialLogs)
    Serial.printf("MQTT >> [%s] %s.\n", topic, payloadStr.c_str());

  // Send the message to subscribers
  for (int i = 0 ; i < mTopicSubscriptionListSize ; i++)
  {
    if (mTopicSubscriptionList[i].topic.equals(topic))
      (*mTopicSubscriptionList[i].callback)(payloadStr); // Call the callback
  }
}
