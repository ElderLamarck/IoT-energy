#include <Wire.h>
#include "SSD1306Wire.h"
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include "EmonLib.h"

#include "images.h"

#define SENSOR_PIN 34
#define RELAY_PIN 4
#define SENSOR_OFFSET 0.06
#define VOLTAGE 220.0

SSD1306Wire display(0x3c, 5, 4);

#define AWS_IOT_PUBLISH_TOPIC "energy/consumption"
#define AWS_IOT_SUBSCRIBE_TOPIC "energy/power"

EnergyMonitor emon;

void messageHandler(String &topic, String &payload);

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

int counter = 1;
int power = 1;

void connectAWS()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	Serial.println("Connecting to Wi-Fi");

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	Serial.println("\nWi-Fi connected!");
	Serial.print("IP Address: ");
	Serial.println(WiFi.localIP());

	// Configure WiFiClientSecure to use the AWS IoT device credentials
	net.setCACert(AWS_CERT_CA);
	net.setCertificate(AWS_CERT_CRT);
	net.setPrivateKey(AWS_CERT_PRIVATE);

	// Connect to the MQTT broker on the AWS endpoint we defined earlier
	client.begin(AWS_IOT_ENDPOINT, 8883, net);

	// Create a message handler
	client.onMessage(messageHandler);

	Serial.println("Connecting to AWS IoT...");
	while (!client.connect(THINGNAME))
	{
		Serial.print(".");
		Serial.print("MQTT Error: ");
		Serial.println(client.lastError());
		delay(1000);
	}

	if (!client.connected())
	{
		Serial.println("\nAWS IoT Timeout!");
		return;
	}

	// Subscribe to a topic
	if (client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC))
	{
		Serial.println("\nSubscribed to topic: " AWS_IOT_SUBSCRIBE_TOPIC);
	}
	else
	{
		Serial.println("\nFailed to subscribe to topic: " AWS_IOT_SUBSCRIBE_TOPIC);
	}

	Serial.println("AWS IoT Connected!");
}

void publishMessage(double current, double power)
{
    StaticJsonDocument<200> doc;
    doc["corrente"] = current;
    doc["potencia"] = power;
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    if (!client.connected())
    {
        Serial.println("MQTT client not connected! Attempting to reconnect...");
        connectAWS();
    }

    if (!client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer))
    {
        Serial.println("Failed to publish message!");
        Serial.print("MQTT Error: ");
        Serial.println(client.lastError());
    }
    else
    {
        Serial.println("Message published successfully!");
        Serial.println(jsonBuffer);
    }
}


void messageHandler(String &topic, String &payload)
{
	Serial.print("Power: ");
	Serial.println(payload.toInt() == 1 ? "ON" : "OFF");
	Serial.println(payload);

	payload.toInt() == 1
	? digitalWrite(RELAY_PIN, HIGH)
	: digitalWrite(RELAY_PIN, LOW);
}

void setup()
{
	Serial.begin(115200);
	connectAWS();

	display.init();
	display.flipScreenVertically();
	display.setFont(ArialMT_Plain_10);

	emon.current(SENSOR_PIN, 1);
	pinMode(RELAY_PIN, OUTPUT);

	// Synchronize time for TLS
	Serial.println("Synchronizing time...");
	configTime(0, 0, "pool.ntp.org", "time.nist.gov");
	while (time(nullptr) < 100000)
	{
		delay(100);
		Serial.print(".");
	}
	Serial.println("\nTime synchronized!");
}

void drawProgressBarDemo()
{
	int progress = map(counter, 0, 4095, 0, 100);
	display.drawProgressBar(0, 32, 120, 10, progress);

	display.setTextAlignment(TEXT_ALIGN_CENTER);
	display.drawString(64, 15, String(progress) + "%");
}

void loop()
{
	display.clear();
	display.setFont(ArialMT_Plain_10);
	display.setTextAlignment(TEXT_ALIGN_LEFT);

	double current = emon.calcIrms(1484);
	current = current - SENSOR_OFFSET > 0
	? current - SENSOR_OFFSET
	: 0;
	double power = current * VOLTAGE;

	Serial.println();

	publishMessage(current, power);
	client.loop();

	delay(1000);
}
