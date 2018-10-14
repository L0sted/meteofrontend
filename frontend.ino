#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SSD1306Brzo.h" 
#include "wifi.h"
#include <ESP8266HTTPClient.h>

String inTemp,outTemp,pressure,humid;
const unsigned short int onTime=6, offTime=23, beepDelay=60, timeOffset=3;
short int lastBeep;
const String httpAddr="http://192.168.100.101:1880";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
SSD1306Brzo display(0x3C, 5, 4); //oled display w/ address 0x3C with SDA on GPIO4 and SCL on GPIO5 //address == offset

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
  // wifiConnect();
  MDNS.begin("esp8266-frontend");
  //==NTP INIT==
  timeClient.begin();
  timeClient.setTimeOffset(timeOffset*3600);
  netTasks();
}

void loop(){
  mainScreen();//it could be cool and smooth if we could update screen independently, in some kind of separate thread or smthn similar
  if (((timeClient.getHours()*60 + timeClient.getMinutes()) % beepDelay == 0)&&(timeClient.getSeconds() < 1)&&(!nightMode())){
    tone(15,1000);
    delay(100);
    noTone(15);
    tone(15,1000);
    delay(100);
    noTone(15);
    lastBeep = timeClient.getHours()*60;
    netTasks();
  }
  else {
    delay(750);
  }
}
//====================IN PROGRESS===================
void netTasks() {
  HTTPClient http;
  WiFi.forceSleepWake();
  wifiConnect();
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  http.begin(httpAddr+"/inTemp");
  if (http.GET() < 0) //IDK why the fuck i cant getString without this check
    return;
  inTemp="i:"+http.getString();
  http.end();

  http.begin(httpAddr+"/outTemp");
  if (http.GET() < 0)
    return;
  outTemp="o:"+http.getString();
  http.end();

  http.begin(httpAddr+"/humid");
  if (http.GET() < 0)
    return;
  humid="h:"+http.getString()+"%";
  http.end();

  http.begin(httpAddr+"/pressure");
  if (http.GET() < 0)
    return;
  pressure="p:"+http.getString();
  http.end();

  updateNtp();
  WiFi.disconnect();
}
void mainScreen() {
  display.clear();
  if (!nightMode()) {
    int x = (timeClient.getHours()*60+timeClient.getMinutes())/11;
    //Time
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 22, timeClient.getFormattedTime());
    //progress bar
    display.drawLine(0,22,x,22);
    // external temp //top left
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, outTemp);
    //pressure //top right
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 0, pressure);
    //inside temp //bottom left
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 53, inTemp);
    //humid //bottom right
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 53, humid);
  }
  display.display();
}
void displayStatus(int state){
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
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
void updateNtp() {
  // if (lastNtp != timeClient.getHours()) {
    while (!timeClient.update())
      Serial.println(timeClient.getHours());
}
bool nightMode() {
  if ((timeClient.getHours() > offTime)||(timeClient.getHours() < onTime)) { //turn off screen between loaded time
    return true;
  }
  else 
    return false;
}
