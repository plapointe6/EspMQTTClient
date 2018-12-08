#include "EspMQTTClient.h"


// =============== Constructor / destructor ===================

EspMQTTClient::EspMQTTClient(const String &wifiSsid, const String &wifiPassword, const String &mqttServerIp,
	const short mqttServerPort, const String &mqttUsername, const String &mqttPassword,
	const String &mqttClientName, std::function<void()> connectionEstablishedCallback, bool enableWebUpdater, bool enableSerialLogs)
{
	mWifiConnected = false;
	mMqttConnected = false;
	mLastWifiConnectionMillis = 0;
	mLastMqttConnectionMillis = 0;
	
	mConnectionEstablishedCallback = connectionEstablishedCallback;
	
	mEnableSerialLogs = enableSerialLogs;
	mEnableWebUpdater = enableWebUpdater;
		
	mWifiSsid = wifiSsid;
	mWifiPassword = wifiPassword;

	mMqttServerIp = mqttServerIp;
	mMqttServerPort = mqttServerPort;
	mMqttUsername = mqttUsername;
	mMqttPassword = mqttPassword;
	mMqttClientName = mqttClientName;

	mCallbackListSize = 0;

	// For web updater
	if(mEnableWebUpdater)
	{
		mHttpServer = new ESP8266WebServer(80);
		mHttpUpdater = new ESP8266HTTPUpdateServer();
	}

	// Creation of the MQTT client
	mWifiClient = new WiFiClient();
	mMqttClient = new PubSubClient(mMqttServerIp.c_str(), mMqttServerPort, *mWifiClient);
	mMqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {this->mqttMessageReceivedCallback(topic, payload, length);});
}

EspMQTTClient::~EspMQTTClient() {}


// =============== Public functions =================

void EspMQTTClient::loop()
{
	long currentMillis = millis();
	
	if (WiFi.status() == WL_CONNECTED)
	{
		if(!mWifiConnected)
		{
			if(mEnableSerialLogs)
				Serial.printf("\nWifi connected, ip : %s \n", WiFi.localIP().toString().c_str());
			
			// Config of web updater
			if (mEnableWebUpdater)
			{
				MDNS.begin(mMqttClientName.c_str());
				mHttpUpdater->setup(mHttpServer, "/", mMqttUsername.c_str(), mMqttPassword.c_str());
				mHttpServer->begin();
				MDNS.addService("http", "tcp", 80);
			
				if(mEnableSerialLogs)
					Serial.printf("Web updater ready, Open http://%s.local in your browser and login with username '%s' and password '%s'\n", mMqttClientName.c_str(), mMqttUsername.c_str(), mMqttPassword.c_str());
			}
	
			mWifiConnected = true;
		}
		
		// MQTT handling
		if (mMqttClient->connected())
		{		
			mMqttClient->loop();
		}
		else
		{
			if(mMqttConnected)
			{
				if(mEnableSerialLogs)
					Serial.println("Lost MQTT connection.");
				
				mCallbackListSize = 0;
				
				mMqttConnected = false;
			}
			
			if(currentMillis - mLastMqttConnectionMillis > connection_retry_delay || mLastMqttConnectionMillis == 0)
				connectToMqttBroker();
		}
			
		// Web updater handling
		if(mEnableWebUpdater)
			mHttpServer->handleClient();
	}
	else
	{
		if(mWifiConnected) 
		{
			if(mEnableSerialLogs)
				Serial.println("Lost wifi connection.");
			
			mWifiConnected = false;
		}
		
		// We retry to connect to the wifi
		if(currentMillis - mLastWifiConnectionMillis > connection_retry_delay || mLastWifiConnectionMillis == 0)
		{
			if(mLastWifiConnectionMillis > 0)
				WiFi.disconnect();
			connectToWifi();
		}
	}
	
	if(mToExecuteListSize > 0)
	{
		long currentMillis = millis();
		
		for(byte i = 0 ; i < mToExecuteListSize ; i++)
		{
			if(mToExecuteList[i].targetMillis <= currentMillis)
			{
				mToExecuteList[i].toExecute();
				for(int j = i ; j < mToExecuteListSize-1 ; j++)
					mToExecuteList[j] = mToExecuteList[j + 1];
				mToExecuteListSize--;
				i--;
			}
		}
	}
}

bool EspMQTTClient::isConnected()
{
	return mWifiConnected && mMqttConnected;
}

void EspMQTTClient::publish(const String &topic, const String &payload, bool retain)
{
	mMqttClient->publish(topic.c_str(), payload.c_str(), retain);
  
	if(mEnableSerialLogs)
		Serial.printf("MQTT - Message sent [%s] %s \n", topic.c_str(), payload.c_str());
}

void EspMQTTClient::subscribe(const String &topic, std::function<void(const String&)> messageReceivedCallback)
{
	mMqttClient->subscribe(topic.c_str());

	if(mEnableSerialLogs)
		Serial.printf("MQTT - subscribed to %s \n", topic.c_str());

	if (mCallbackListSize < max_callback_list_size)
	{
		mCallbackList[mCallbackListSize] = {topic, messageReceivedCallback };
		mCallbackListSize++;
	}
	else if(mEnableSerialLogs)
		Serial.println("ERROR - EspMQTTClient::subscribe - Max callback size reached.");
}

void EspMQTTClient::executeDelayed(const long delay, std::function<void()> toExecute)
{
	if(mToExecuteListSize < max_to_execute_list_size)
	{
		DelayedExecutionRecord delayedExecutionRecord;
		delayedExecutionRecord.targetMillis = millis() + delay;
		delayedExecutionRecord.toExecute = toExecute;
		
		mToExecuteList[mToExecuteListSize] = delayedExecutionRecord;
		mToExecuteListSize++;
	}
	else if(mEnableSerialLogs)
		Serial.printf("\nError, max_to_execute_list_size reached");
}

// ================== Private functions ====================-


void EspMQTTClient::connectToWifi()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(mWifiSsid.c_str(), mWifiPassword.c_str());

	if(mEnableSerialLogs)
		Serial.printf("\nConnecting to %s ", mWifiSsid.c_str());
	
	mLastWifiConnectionMillis = millis();
}

void EspMQTTClient::connectToMqttBroker()
{
	if(mEnableSerialLogs)
		Serial.printf("\nConnecting to MQTT broker at %s ", mMqttServerIp.c_str());

	if (mMqttClient->connect(mMqttClientName.c_str(), mMqttUsername.c_str(), mMqttPassword.c_str()))
	{
		mMqttConnected = true;
		
		if(mEnableSerialLogs) 
			Serial.print("ok \n");
		
		mConnectionEstablishedCallback();
	}
	else
	{
		if(mEnableSerialLogs)
			Serial.print("unable to connect, ");

		String state;
		switch (mMqttClient->state())
		{
			case -4:
				state = "MQTT_CONNECTION_TIMEOUT";
				break;
			case -3:
				state = "MQTT_CONNECTION_LOST";
				break;
			case -2:
				state = "MQTT_CONNECT_FAILED";
				break;
			case -1:
				state = "MQTT_DISCONNECTED";
				break;
			case 1:
				state = "MQTT_CONNECT_BAD_PROTOCOL";
				break;
			case 2:
				state = "MQTT_CONNECT_BAD_CLIENT_ID";
				break;
			case 3:
				state = "MQTT_CONNECT_UNAVAILABLE";
				break;
			case 4:
				state = "MQTT_CONNECT_BAD_CREDENTIALS";
				break;
			case 5:
				state = "MQTT_CONNECT_UNAUTHORIZED";
				break;
		}
	}
	
	mLastMqttConnectionMillis = millis();
}

void EspMQTTClient::mqttMessageReceivedCallback(char* topic, byte* payload, unsigned int length)
{
	if(mEnableSerialLogs)
		Serial.printf("MQTT - Message received [%s] ", topic);

	for (int i = 0 ; i < mCallbackListSize ; i++)
	{
		if (mCallbackList[i].topic.equals(topic))
		{
			// Convert payload to String
			char buffer[max_mqtt_payload_size];
			int j;
      
			for (j = 0 ; j < length && j < max_mqtt_payload_size-1 ; j++)
				buffer[j] = (char)payload[j];
        
			buffer[j] = '\0';
			String payloadStr = buffer;
     
			if(mEnableSerialLogs)
				Serial.printf("%s \n", payloadStr.c_str());

			// Call the callback
			mCallbackList[i].callback(payloadStr);
		}
	}
}
