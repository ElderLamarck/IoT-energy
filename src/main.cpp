#include <Wire.h>
#include "SSD1306Wire.h"
#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include "EmonLib.h"

#include "images.h"

#define SENSOR_PIN 35
#define SENSOR_OFFSET 0.06
#define VOLTAGE 220.0

SSD1306Wire display(0x3c, 5, 4);

#define AWS_IOT_PUBLISH_TOPIC "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

EnergyMonitor emon;

void messageHandler(String &topic, String &payload);

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

int counter = 1;

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

	// Configure WiFiClientSecure to use the AWS IoT device credentials
	net.setCACert(AWS_CERT_CA);
	net.setCertificate(AWS_CERT_CRT);
	net.setPrivateKey(AWS_CERT_PRIVATE);

	// Connect to the MQTT broker on the AWS endpoint we defined earlier
	client.begin(AWS_IOT_ENDPOINT, 8883, net);

	// Create a message handler
	client.onMessage(messageHandler);

	Serial.println("Connecting to AWS IOT");

	while (!client.connect(THINGNAME))
	{
		Serial.print(".");
		delay(100);
	}

	if (!client.connected())
	{
		Serial.println("AWS IoT Timeout!");
		return;
	}

	// Subscribe to a topic
	client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

	Serial.println("AWS IoT Connected!");
}

void publishMessage(double current, double power)
{
	StaticJsonDocument<200> doc;
	doc["corrente"] = current;
	doc["potencia"] = power;
	char jsonBuffer[512];
	serializeJson(doc, jsonBuffer); // print to client

	client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(String &topic, String &payload)
{
	Serial.println("incoming: " + topic + " - " + payload);
}

void setup()
{
	Serial.begin(115200);
	connectAWS();
	Serial.println();

	display.init();

	display.flipScreenVertically();
	display.setFont(ArialMT_Plain_10);
	emon.current(SENSOR_PIN, 1);
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

	Serial.print("Corrente: ");
	Serial.println(current);
	Serial.print("Potência: ");
	Serial.println(power);
	Serial.println();

	// display.drawString(0, 0, String(current));
	// display.drawString(0, 10, String(power));
	// display.drawString(128, 54, String(millis()));
	// display.display();
	publishMessage(current, power);
	client.loop();
	delay(500);
}
