#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#define TIME_ZONE -2

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"

WiFiClientSecure net = WiFiClientSecure();
BearSSL::X509List ca_cert(AWS_CERT_CA);
BearSSL::X509List device_cert(DEVICE_CERT);
BearSSL::PrivateKey device_key(DEVICE_KEY);


MQTTClient client = MQTTClient(256);

time_t now;
time_t nowish = 1510592825;

void NTPConnect(void) {
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
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
  doc["time"] = millis();
  doc["sensor_a0"] = analogRead(0);
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
  connectAWS();
}

void loop() {
  // publishMessage();
  client.loop();
  delay(1000);
}