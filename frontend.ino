#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SSD1306Brzo.h" 
// #include <ArduinoJson.h>
// #include <FS.h>
#include "wifi.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

double inTemp,outTemp,pressure,humid;
short int onTime = 5, offTime = 23, beepDelay = 1, lastNtp, lastBeep;
bool mqttAvail;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
SSD1306Brzo display(0x3C, 4, 5); //oled display w/ address 0x3C with SDA on GPIO4 and SCL on GPIO5 //address == offset
// Ticker dispUpd;
// Ticker timeUpd;

WiFiClient client;

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
  }
  MDNS.begin("esp8266-frontend");
  Serial.println("Connected to " + String(ssid) + "; IP address: ");
  displayStatus(0);
  Serial.print(WiFi.localIP());
  
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
  // if (!readConfig()) {
  //   Serial.println("can't load config!");
  //   defConf();
  // }
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
  //debug
  Serial.println(ESP.getVcc());
  Serial.println(ESP.getFlashChipRealSize());
}

void loop(){
  if (WiFi.status() != WL_CONNECTED) {
    // displayStatus(1);
    wifiConnect();
  }
  MQTT_connect();
  mqtt.processPackets(500);
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  mainScreen();
  if (timeClient.getMinutes() == 0 && timeClient.getSeconds() < 5 && !nightMode() && lastBeep != timeClient.getHours()) { //beep
    tone(15,1000);
    delay(100);
    noTone(15);
    lastBeep = timeClient.getHours();
  }
  updateNtp();
}

//TODO
//AT LEAST MAKE IT WORK (show ui and get data + ntp) - compiling - ... - WORKS!!11

//debug -> release
//remove useless serial debug - DONE
//show wifi info on oled - DONE
//reset on not connected - DONE
//check connectivity during work and do actions - DONE
//beep hourly - DONE
//second screen
//reconfig wifi if not found (start webserver and AP)
//load config
//webconfig (?)
//mqtt update interval

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


//====================IN PROGRESS===================
void displayStatus(int state){
  display.clear();
  if (state == 0) {
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, "WL_CONNECTED");
    
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
  display.display();
}

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

//====================paused===================

// bool readConfig() {
//   File configFile = SPIFFS.open("/config.json", "r");
//   if (!configFile) {
//     Serial.println("Failed to open config file");
//     return false;
//   }

//   // size_t size = configFile.size();
//   // if (size > 1024) {
//   //   Serial.println("Config file size is too large");
//   //   return false;
//   // }

//   // Allocate a buffer to store contents of the file.
//   std::unique_ptr<char[]> buf(new char[size]);

//   // We don't use String here because ArduinoJson library requires the input buffer to be mutable. If you don't use ArduinoJson, you may as well use configFile.readString instead.
//   configFile.readBytes(buf.get(), size);

//   StaticJsonBuffer<200> jsonBuffer;
//   JsonObject& json = jsonBuffer.parseObject(buf.get());

//   if (!json.success()) {
//     Serial.println("Failed to parse config file");
//     return false;
//   }

//   offTime = int(json["offTime"]);
//   onTime = int(json["onTime"]);
//   beepDelay = int(json["beepDelay"]);
//   return true;
  
// }
// bool defConf() {
//   StaticJsonBuffer<200> jsonBuffer;
//   JsonObject& json = jsonBuffer.createObject();
//   json["onTime"] = "6";
//   json["offTime"] = "23";
//   json["beepDelay"] = "60";
//   File configFile = SPIFFS.open("/config.json", "w");
//   if (!configFile) {
//     Serial.println("Failed to open config file for writing");
//     return false;
//   }

//   json.printTo(configFile);
//   return true;
// }

//===================WELL DONE=======================
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 10 seconds...");
       mqtt.disconnect();
       delay(10000);  // wait 10 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
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
    timeClient.update();
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

