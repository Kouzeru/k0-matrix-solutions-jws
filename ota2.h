#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#ifndef WIFI_SSID_DEFINED
  #define WIFI_SSID_DEFINED
  const char* ssid       = "MATRIX SOLUTIONS";
  const char* password   = "kouzerumatsukite";
#endif

AsyncWebServer server(80);

bool OTA_connect(void (*func)()) {
  int32_t lastMillis = millis();
  int32_t lastMillis1 = lastMillis;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis()-lastMillis1>0){
      lastMillis1++;
      func();
    }
    if (millis()-lastMillis>8000){
      break;
    }
    esp_yield();
  }
  if (WiFi.status() != WL_CONNECTED){
    return false;
    Serial.println("Connection Failed! Rebooting...");
    ESP.restart();
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  return true;
}

void OTA_handle(void (*func)()){
}