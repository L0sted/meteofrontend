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
// #include <ESP8266WiFiGeneric.h>
#include <WiFiClient.h>
// #include <ESP8266WebServer.h>
// #include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SSD1306Brzo.h" 
// #include <ArduinoJson.h>
// #include <FS.h>
#include "wifi.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

double inTemp,outTemp,pressure,humid;
unsigned short int lastNtp, lastBeep, onTime=6, offTime=23, beepDelay=60;
bool mqttAvail;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
SSD1306Brzo display(0x3C, 4, 5); //oled display w/ address 0x3C with SDA on GPIO4 and SCL on GPIO5 //address == offset

WiFiClient client;
// ESP8266WebServer server(80);
Adafruit_MQTT_Client mqtt(&client, "192.168.100.100", 1883);

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
    // MDNS.begin("esp8266-frontend");
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
  netTasks();
}

void loop(){

  WiFi.disconnect();
  mainScreen();//it could be cool and smooth if we could update screen independently, in some kind of separate thread or smthn similar
  WiFi.forceSleepBegin();
  //network required tasks
  if (((timeClient.getHours()*60 + timeClient.getMinutes() + timeClient.getSeconds()) % beepDelay == 0)&&(!nightMode())){ //beep and start tasks
    tone(15,1000);
    delay(100);
    noTone(15);
    tone(15,1000);
    delay(100);
    noTone(15);
    lastBeep = timeClient.getHours()*60;
    // secondDisplay();
    netTasks();

  }
}


//====================IN PROGRESS===================
void netTasks() { //show other display before
  WiFi.forceSleepWake();
    if (WiFi.status() != WL_CONNECTED) { //check wifi status
      wifiConnect();
    }
    // server.handleClient();
    MQTT_connect();//check connection and get packets for 0.5s
    mqtt.processPackets(8000);
    updateNtp();//update time
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

void coolBeep() {
  tone(15,1000);
  delay(200);
  noTone(15);
  tone(15,1200);
  delay(200);
  noTone(15);
  tone(15,1000);
  delay(200);
  noTone(15);
  tone(15,1200);
  delay(250);
  noTone(15);
  tone(15,1400);
  delay(350);
  noTone(15);
  tone(15,1500);
  delay(350);
  noTone(15);

}

//====================paused===================



//===================WELL DONE=======================

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
  switch (state) {
  case 0: 
    display.drawString(64, 22, "Connected!");
    break;
  case 1:
    display.drawString(64, 22, "Connecting...");
    break;
  case 2:
    display.drawString(64, 22, "Not connected!");
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 53, "we will die in 10s! :0");
    break;
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

