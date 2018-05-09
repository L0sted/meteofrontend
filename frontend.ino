//TODO
//AT LEAST MAKE IT WORK (show ui and get data + ntp) - compiling - ... - WORKS!!11

//debug -> release
//remove useless serial debug - DONE
//show wifi info on oled - DONE
//reset on not connected - DONE
//check connectivity during work and do actions - DONE
//beep hourly - DONE
//github - oh yeahhh

//load config
//webconfig (?) - in progress
//mqtt update interval

//second screen

//reconfig wifi if not found (start webserver and AP)

//GRAPH
/*
get json from server
~12 values
hourly values + hourly display
if pressure lowers then weather gonna be bad
if pressure uppers then weather gonna be good
*/

//CONFIG
/*
beep time
on time
off time
mqtt update interval

*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SSD1306Brzo.h" 
#include <ArduinoJson.h>
#include <FS.h>
#include "wifi.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

double inTemp,outTemp,pressure,humid;
unsigned short int onTime = 5, offTime = 23, beepDelay = 60, lastNtp, lastBeep;
bool mqttAvail;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
SSD1306Brzo display(0x3C, 4, 5); //oled display w/ address 0x3C with SDA on GPIO4 and SCL on GPIO5 //address == offset

WiFiClient client;
ESP8266WebServer server(80);
Adafruit_MQTT_Client mqtt(&client, "192.168.100.102", 1883);

Adafruit_MQTT_Subscribe pressureFeed = Adafruit_MQTT_Subscribe(&mqtt, "pressure");
Adafruit_MQTT_Subscribe inFeed = Adafruit_MQTT_Subscribe(&mqtt, "bmpTemp");
Adafruit_MQTT_Subscribe outFeed = Adafruit_MQTT_Subscribe(&mqtt, "externalTemp");
Adafruit_MQTT_Subscribe humidFeed = Adafruit_MQTT_Subscribe(&mqtt, "humid");

void wifiConnect() {
  int beginMillis = millis();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && millis() - beginMillis < 30000) {
    delay(250);
    displayStatus(1);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    displayStatus(2);
    delay(10000);
    ESP.reset();
  } else {
    MDNS.begin("esp8266-frontend");
    Serial.print("Connected to " + String(ssid) + "; IP address: ");
    Serial.println(WiFi.localIP());
    displayStatus(0);
  }  
}
void setup(){
  Serial.begin(115200);
  //==DISPLAY INIT==
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  //==WIFI CONNECT==
  WiFi.mode(WIFI_STA);
  wifiConnect();
  //==READ CONFIG==
  if (!SPIFFS.begin())
    SPIFFS.format();

  if (!readConfig()) 
    defConfig();
  
  //==NTP INIT==
  timeClient.begin();
  timeClient.setTimeOffset(10800);
  //MQTT
  pressureFeed.setCallback(pressureCall);
  inFeed.setCallback(inCall);
  outFeed.setCallback(outCall);
  humidFeed.setCallback(humidCall);
  mqtt.subscribe(&pressureFeed);
  mqtt.subscribe(&outFeed);
  mqtt.subscribe(&inFeed);
  mqtt.subscribe(&humidFeed);
  server.on("/edit", editConfig);
  server.begin();
}

void loop(){
  if (WiFi.status() != WL_CONNECTED) { //check wifi status
    wifiConnect();
  }
  server.handleClient();
  MQTT_connect();//check connection and get packets for 0.5s
  mqtt.processPackets(500);

  mainScreen();//it could be cool and smooth if we could update screen independently, in some kind of separate thread or smthn similar

  if ((timeClient.getHours()*60 + timeClient.getMinutes() - lastBeep > beepDelay)&&(!nightMode())){ //beep every $lastBeep
    tone(15,1000);
    delay(100);
    noTone(15);
    lastBeep = timeClient.getHours()*60;
  }
  notifyScreen("test msg");
  delay(10000);
  updateNtp();//update time
}



//====================IN PROGRESS===================
// void 

void notifyScreen(String n_text) { //display func
  display.clear();
  
  if (!nightMode()) { //turn off screen at night
    //Center
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, n_text);
    //progress bar
    //top left
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, timeClient.getFormattedTime());
    //top right
    // display.setFont(ArialMT_Plain_10);
    // display.setTextAlignment(TEXT_ALIGN_RIGHT);
    // display.drawString(128, 0, "p:"+String(pressure));
    //bottom left
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 53, "192.168.100.8");
    //bottom right
    // display.setFont(ArialMT_Plain_10);
    // display.setTextAlignment(TEXT_ALIGN_RIGHT);
    // display.drawString(128, 53, "h:"+String(humid)+"%");
    }

    //beep beep!
    tone(15,1000);
    delay(100);
    noTone(15);
    delay(100);
    tone(15,1000);
    delay(100);
    noTone(15);
  
  display.display();
}

//====================paused===================


// void secondDisplay() {
//   display.clear();
//   display.setFont(ArialMT_Plain_16);
//   display.setTextAlignment(TEXT_ALIGN_CENTER);
//   display.drawString(0, 30, timeClient.getFormattedTime());
  
//   display.drawRect(0,18,128,53); //frame
// /*
// 128/12=10px
// 128x32 resolution
// 32-19=13px for graph 
// */
//   int x = 0;
//   for (int i=0;i<12;++i){
//     display.drawLine(x+i*10, y[x], x+i*10+10, y[x]);
//   }

// }

//===================WELL DONE=======================
void editConfig(){
  if (server.args() > 0 ) { 
    for ( uint8_t i = 0; i < server.args(); i++ ){
      String Argument_Name = server.argName(i);
      String client_response = server.arg(i);

      if (Argument_Name == "beepDelay"){
        beepDelay = client_response.toInt();
      }
      if (Argument_Name == "onTime"){
        onTime = client_response.toInt();
      }
      if (Argument_Name == "offTime"){
        offTime = client_response.toInt();
      }
      updateConfig();
      server.send(200, "text/plain", "updatedConfig");
    }
  } else {
    server.send(200, "text/plain", "to update config, goto " + String(WiFi.localIP()) + "/edit?parameter=value");
  }
}
bool updateConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["onTime"] = onTime;
  json["offTime"] = offTime;
  json["beepDelay"] = beepDelay;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;

}
bool readConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");

    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input buffer to be mutable. If you don't use ArduinoJson, you may as well use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  offTime = int(json["offTime"]);
  onTime = int(json["onTime"]);
  beepDelay = int(json["beepDelay"]);
  return true;
  
}
bool defConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["onTime"] = "6";
  json["offTime"] = "23";
  json["beepDelay"] = "60";
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void mainScreen() {
  display.clear();
  
  if (!nightMode()) { //turn off screen at night
    int x = (timeClient.getHours()*60+timeClient.getMinutes())/11;
    //Time
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, timeClient.getFormattedTime());
    //progress bar
    display.drawLine(0,22,x,22);
    if (mqttAvail){ //do not show info if mqtt not available
      //external temp //top left
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 0, "o:"+String(outTemp));
      //pressure //top right
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_RIGHT);
      display.drawString(128, 0, "p:"+String(pressure));
      //inside temp //bottom left
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 53, "i:"+String(inTemp));
      //humid //bottom right
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_RIGHT);
      display.drawString(128, 53, "h:"+String(humid)+"%");
    }
    else {
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 53, "mqtt unavailable");
    }
  }
  display.display();
}
void displayStatus(int state){
  display.clear();
  if (state == 0) {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, "Connected!");
    
  }
  if (state == 1) {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, "Connecting...");
  }
  if (state == 2) {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, "Not connected!");
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 53, "we will die in 10s! :0");
  }
  display.display();
}

void MQTT_connect() {
  if (mqtt.connected()) {
    return;
  }
  int8_t ret;
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    displayStatus(3);
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      mqttAvail = false;
    }
  }
  if (mqtt.connected())
    mqttAvail = true;
}
void pressureCall(double x) {
  pressure = x;
}
void inCall(double x) {
  inTemp = x;
}
void outCall(double x) {
  outTemp = x;
}
void humidCall(double x){
  humid = x;
}
void updateNtp() {
  if (lastNtp != timeClient.getHours()) {
    if (timeClient.update())
      lastNtp = timeClient.getHours();
  }
}
bool nightMode() {
  if ((timeClient.getHours() > offTime)||(timeClient.getHours() < onTime)) { //turn off screen between loaded time
    return true;
  }
  else 
    return false;
}

