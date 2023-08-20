#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

WiFiClientSecure net = WiFiClientSecure();
BearSSL::X509List ca_cert(AWS_CERT_CA);
BearSSL::X509List device_cert(DEVICE_CERT);
BearSSL::PrivateKey device_key(DEVICE_KEY);


MQTTClient client = MQTTClient(256);

const uint8_t BUTTON = 12;
bool clicked = false;
bool doubleClicked = false;
unsigned long lastClick = millis();
const uint SINGLE_CLICK_DELAY = 200;
const uint DOUBLE_CLICK_DELAY = 800;

void ICACHE_RAM_ATTR handleClick() {
  // Limit trigger rate
  if ((millis() - lastClick) < SINGLE_CLICK_DELAY) return;
  if (!clicked) {
    clicked = true;
  } else if (!doubleClicked && (millis() - lastClick) < DOUBLE_CLICK_DELAY) {
    doubleClicked = true;
  }
  lastClick = millis();
}

void connectAWS() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  NTPConnect();

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setTrustAnchors(&ca_cert);
  net.setClientRSACert(&device_cert, &device_key);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT\n");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(500);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void publishMessage() {
  StaticJsonDocument<200> doc;
  doc["time"] = time(nullptr);
  if (doubleClicked) {
    doc["event"] = "DOUBLE_CLICK";
  } else if (clicked) {
    doc["event"] = "CLICKED";
  }
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);  // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char *message = doc["message"];
}



void setup() {
  Serial.begin(9600);
  pinMode(12, INPUT);
  attachInterrupt(digitalPinToInterrupt(12), handleClick, RISING);
  connectAWS();
}

void loop() {
  client.loop();
  if ((millis() - lastClick) > DOUBLE_CLICK_DELAY) {
    if (doubleClicked || clicked) {
      publishMessage();
      clicked = false;
      doubleClicked = false;
    }
  }
  delay(1000);
}