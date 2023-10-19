#include <functional>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClient.h>
  #include <ESP8266mDNS.h>
  #include <WiFiUdp.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClient.h>
  //#include <WebServer.h>
#endif

#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <ElegantOTA.h>
#include <ArduinoOTA.h>

#ifndef WIFI_SSID_DEFINED
  #define WIFI_SSID_DEFINED
  const char* ssid       = "MATRIX SOLUTIONS";
  const char* password   = "kouzerumatsukite";
#endif

/////////////////////////////////////////////////////////////////////{

const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

String outputState(int output){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>ESP Web Server</h2>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/refresh?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/refresh?output="+element.id+"&state=0", true); }
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons += "<h4>Output - GPIO 5</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"5\" " + outputState(5) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - GPIO 4</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"4\" " + outputState(4) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - GPIO 2</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - GPIO 16</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"16\" " + outputState(16) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

//////////////////////////////////////////////////////////////////////}

AsyncWebServer server(80);

void HTML_config(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Kouzerumatsukite");
  });
   // Route for root / web page
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/refresh?output=<inputMessage1>&state=<inputMessage2>
  server.on("/refresh", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/refresh?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });
}

void ArduinoOTA_config(){
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("myesp8266");
  ArduinoOTA.setPassword("kouzerumatsukite");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) { Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {  Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) { Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) { Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) { Serial.println("End Failed"); }
  });
  ArduinoOTA.begin();
}

void ElegantOTA_onStart(void (*func)()){
  Serial.println("OTA update process started.");
}

void ElegantOTA_onProgress(void (*func)(size_t,size_t)){
  Serial.println("OTA update process started.");
}

void ElegantOTA_onEnd(void (*func)(bool)){
  Serial.println("OTA update process started.");
}

void ElegantOTA_config(
    std::function<void()> onStart = nullptr,
    std::function<void(size_t, size_t)> onProgress = nullptr,
    std::function<void(bool)> onEnd = nullptr
) {
  ElegantOTA.onStart(onStart);
  ElegantOTA.onProgress(onProgress);
  ElegantOTA.onEnd(onEnd);

/*
  ElegantOTA.onStart([]() {
    Serial.println("OTA update process started.");
    // Add your initialization tasks here.
  });

  ElegantOTA.onProgress([](size_t current, size_t final) {
    Serial.printf("Progress: %u%%\n", (current * 100) / final);
  });

  ElegantOTA.onEnd([](bool success) {
    if (success) {
      Serial.println("OTA update completed successfully.");
      // Add success handling here.
    } else {
      Serial.println("OTA update failed.");
      // Add failure handling here.
    }
  });
  */
}

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

  HTML_config();
  ArduinoOTA_config();
  ElegantOTA_config();
  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.println("HTTP server started");

  return true;
}

void OTA_handle(void (*func)()){
  //server.handleClient();
  //ElegantOTA.loop();
  ArduinoOTA.handle();
}