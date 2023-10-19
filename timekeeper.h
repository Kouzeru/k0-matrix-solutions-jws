//#include "ESP8266WiFiType.h"
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#ifdef ESP32
  #include <WiFi.h>
#endif

#ifndef WIFI_SSID_DEFINED
  #define WIFI_SSID_DEFINED
  const char* ssid       = "MATRIX SOLUTIONS";
  const char* password   = "kouzerumatsukite";
#endif
const char* ntpServer = "id.pool.ntp.org";
const long  gmtOffset_sec = 3600*8;
const int   daylightOffset_sec = 3600*0;

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec);
char buff[22];

time_t getEpochNTP(){
  timeClient.begin();
  timeClient.update();
  return timeClient.getEpochTime();
}

time_t syncNTP(void (*func)())
{
  //connect to WiFi
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s ", ssid);
  //WiFi.begin(ssid, password);
  WiFi.begin("SABAR", "S484R1977");
  uint32_t lastmill = millis();
  uint32_t lastmill2 = lastmill;
  while (WiFi.status() != WL_CONNECTED && millis()-lastmill < 25000) {
    func();
    if(millis()-lastmill2>500){
      Serial.print(".");
      lastmill2 += 500;
    }
    delay(1);
  }
  Serial.println(" CONNECTED");

  time_t epochTime = getEpochNTP();

  struct tm * timeinfo;
  timeinfo = localtime(&epochTime);

  strftime(buff, sizeof(buff), "%A, %B %d %Y %H:%M:%S", timeinfo);
  Serial.println(buff);

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  return epochTime;
}