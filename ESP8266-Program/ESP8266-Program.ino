/*************************************************** 
  Using SHT31-D Humidity & Temp Sensor
  Making requests to Shelly Smart Plug
 ****************************************************/
 
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#ifndef STASSID
#define STASSID "your-ssid-here"
#define STAPSK "password-here"
#define RELAYIP "shelly-relay-ip-here"
#endif

Adafruit_SHT31 sht31 = Adafruit_SHT31();
WiFiClient client;

const int ledPin = 2;
unsigned long lastHeaterRun = 0;
unsigned long lastRequestTime = 0;
const unsigned long cooldown = 30000; // 30 second cooldown
float highThreshold = 60; // relative humidity to turn on
float lowThreshold = 50;

void setup() {
  Serial.begin(9600);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(ledPin, LOW);

  while (!Serial)
    delay(10); // pause until serial console opens

  Serial.println("SHT31 test");
  if (! sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  Serial.println("Connecting to Network");
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}


void loop() {
  float humidity = sht31.readHumidity();
  unsigned long now = millis();

  if (now - lastRequestTime > cooldown && !isnan(humidity)) {
    Serial.println("Relative Humidity: " + String(humidity) + "%");
    if (humidity > highThreshold) {
      digitalWrite(ledPin, HIGH);
      HumidifierRun(true);
      lastRequestTime = now;
    } else if (humidity < lowThreshold) {
      digitalWrite(ledPin, LOW);
      HumidifierRun(false);
      lastRequestTime = now;
    }
  } else if (isnan(humidity)) { 
    Serial.println("Failed to read humidity");
  }

  // In high humidity trigger sensor heater for 2 seconds every 5 minutes
  if (!isnan(humidity) && humidity > 95 && (now - lastHeaterRun >= 300000UL)) {
    sht31.heater(true);
    delay(2000);
    sht31.heater(false);
    delay(1000); // pause for heater to cool
    lastHeaterRun = now;
  }
}


void HumidifierRun(bool state) {
  if (WiFi.status() == WL_CONNECTED) {
    String action = state ? "on" : "off";
    String url = String("http://") + RELAYIP + "/relay/0?turn=" + action;

    Serial.println("Sending request to turn relay " + action);

    HTTPClient http;
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("HTTP GET code: %d\n", httpCode);
      Serial.println(http.getString());
    } else {
      Serial.printf("Request failed: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}