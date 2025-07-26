/***************************************************
  Smart Dehumidifier Control System
  - Uses SHT31-D Humidity & Temperature Sensor
  - Controls dehumidifier via Shelly Smart Plug
  - Monitors humidity and activates/deactivates accordingly
****************************************************/

#include "Adafruit_SHT31.h"
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "credentials.h" // don't forget to copy the template as 'credentials.h' with your network info

Adafruit_SHT31 sht31 = Adafruit_SHT31();
WiFiClient client;

// Optional hardware pin
const int LED_PIN = 2;

// Timing variables
unsigned long lastHeaterRun = 0;
unsigned long lastRequestTime = 0;

// Config constants
const unsigned long MEASUREMENT_INTERVAL = 30000; // 30 seconds
const float HIGH_HUMIDITY_THRESHOLD = 50;         // relative humidity to turn on
const float LOW_HUMIDITY_THRESHOLD = 45;
const float CRITICAL_HUMIDITY_THRESHOLD = 95.0;    // % RH to turn on sensor heater
const unsigned long SENSOR_HEAT_INTERVAL = 300000; // 5 minutes
const unsigned long SENSOR_HEAT_DURATION = 2000;   // 2 seconds


void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_PIN, LOW);

  while (!Serial)
    delay(10); // pause until serial console opens

  Serial.println("Initializing Sensor");
  if (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
    while (1)
      delay(1);
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

  if (now - lastRequestTime > MEASUREMENT_INTERVAL && !isnan(humidity)) {
    Serial.println("Relative Humidity: " + String(humidity) + "%");
    if (humidity > HIGH_HUMIDITY_THRESHOLD) {
      digitalWrite(LED_PIN, HIGH);
      dehumidifierRun(true);
      lastRequestTime = now;
    } else if (humidity < LOW_HUMIDITY_THRESHOLD) {
      digitalWrite(LED_PIN, LOW);
      dehumidifierRun(false);
      lastRequestTime = now;
    }
  } else if (isnan(humidity)) {
    Serial.println("Failed to read humidity");
  }

  // Activate sensor heater in high humidity to avoid condensation
  if (!isnan(humidity) && humidity > CRITICAL_HUMIDITY_THRESHOLD &&
      (now - lastHeaterRun >= SENSOR_HEAT_INTERVAL)) {
    sht31.heater(true);
    delay(SENSOR_HEAT_DURATION);
    sht31.heater(false);
    delay(1000); // pause for heater to cool
    lastHeaterRun = now;
  }
}

void dehumidifierRun(bool state) {
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
      Serial.printf("Request failed: %s\n",
                    http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}
