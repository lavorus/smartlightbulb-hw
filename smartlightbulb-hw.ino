/*
*  Smart Light Bulb - Software
*  GNU GPLv3 License
*
*  created 01 September 2016
*  modified 27 January 2017
*  by Alvin Leonardo (alvin@lavorus.com)
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#define FASTLED_ESP8266_RAW_PIN_ORDER 12
#include <FastLED.h>

#define pinLED 12 //D6
#define pinRST 14 //D5
#define pinON 5   //D1
#define NUM_LEDS 1
#define delayTime 20

int redVal = 0;
int blueVal = 0;
int greenVal = 0;

CRGB leds[NUM_LEDS];
ESP8266WebServer ServerHTTP(19105);
String wifiSSID = "";
String wifiPassword = "password";
String esid = "";
String epass = "";
byte lastPush = 0;

void setLED(byte red, byte green, byte blue) {
  redVal = 255 - red;
  greenVal = 255 - green;
  blueVal = 255 - blue;
  if (redVal == 255 && greenVal == 255 && blueVal == 255) {
    digitalWrite(pinON, LOW);
  } else {
    digitalWrite(pinON, HIGH);
  }
  leds[0] = CRGB(redVal, greenVal, blueVal);
  FastLED.show();
}

void setLEDConnect(boolean active) {
  if (active == true) {
    setLED(0, 0, 255);
  } else {
    setLED(0, 0, 0);
  }
}

void setLEDFail() {
  setLED(255, 0, 0);
  delay(2000);
}

void setEEPROMStart() {
  EEPROM.begin(512);
}

String getEEPROMReadSSID() {
  for (int i = 0; i < 32; ++i) {
    char tmp = EEPROM.read(i);
    if (tmp == 0)
      break;
    esid += char(tmp);
  }
  return esid;
}

String getEEPROMReadPassword() {
  for (int i = 32; i < 96; ++i) {
    char tmp = EEPROM.read(i);
    if (tmp == 0)
      break;
    epass += char(tmp);
  }
  return epass;
}

void setEEPROMFormat() {
  Serial.print("FORMAT");
  for (int i = 0; i < 96; ++i) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  setLEDFail();
  ESP.reset();
}

String getUID() {
  byte mac[3];
  WiFi.macAddress(mac);

  String mac_addr_dev = "";
  for (int i = 0; i < sizeof(mac); i++) {
    mac_addr_dev += String(mac[i], HEX);
  }
  mac_addr_dev.toUpperCase();

  if (mac_addr_dev.length() == 5) {
    mac_addr_dev = "0" + mac_addr_dev;
  }

  return mac_addr_dev;
}

boolean setWifiAP() {
  int ssid_len = wifiSSID.length() + 1;
  int pass_len = wifiPassword.length() + 1;
  char char_ssid[ssid_len];
  char char_pass[pass_len];
  wifiSSID.toCharArray(char_ssid, ssid_len);
  wifiPassword.toCharArray(char_pass, pass_len);

  if (WiFi.softAP(char_ssid, char_pass)) {
    IPAddress wifi_ap_ip = WiFi.softAPIP();
    return true;
  }
  else {
    return false;
  }
}


void setWifiConnect(String connect_ssid, String connect_pass) {
  ESP8266WiFiMulti WiFiMulti;
  WiFiMulti.addAP(connect_ssid.c_str(), connect_pass.c_str());

  int retry = 0;
  while (WiFiMulti.run() != WL_CONNECTED) {
    if (retry == 5) {
      Serial.println("ERROR connect");
      delay(0);
      return;
    }

    setLEDConnect(true);
    delay(1000);
    setLEDConnect(false);
    delay(1000);

    Serial.print("Retry ");
    Serial.println(retry);
    retry++;
  }

  if (WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void getWebAvailable() {
  String content = "{\"status\":200}";
  ServerHTTP.send(200, "application/json", content);
}

void getWebStatus() {
  String content = "{\"status\":200, \"name\":\"Smart Light " + getUID() + "\", \"red\":" + String(redVal) + ", \"green\":" + String(greenVal) + ", \"blue\":" + String(blueVal) + "}";
  ServerHTTP.send(200, "application/json", content);
}

void getWebSetup() {
  String content = "{\"status\":200, \"name\":\"" + esid + "\", \"pass\":\"" + epass + "\"}";
  ServerHTTP.send(200, "application/json", content);
}

void setWebChange() {
  byte red = ServerHTTP.arg("red").toInt();
  byte green = ServerHTTP.arg("green").toInt();
  byte blue = ServerHTTP.arg("blue").toInt();
  setLED(red, green, blue);

  String content = "{\"status\":200}";
  ServerHTTP.send(200, "application/json", content);
}

void setWebSetup() {
  String content = "";
  String qssid = ServerHTTP.arg("name");
  String qpass = ServerHTTP.arg("pass");
  if (qssid.length() > 0) {
    for (int i = 0; i < 96; ++i) {
      EEPROM.write(i, 0);
    }
    for (int i = 0; i < qssid.length(); ++i) {
      EEPROM.write(i, qssid[i]);
    }
    for (int i = 0; i < qpass.length(); ++i) {
      EEPROM.write(32 + i, qpass[i]);
    }
    EEPROM.commit();
    content = "{\"status\":200}";
  } else {
    content = "{\"status\":501}";
  }
  ServerHTTP.send(200, "application/json", content);
  if (qssid.length() > 0) {
    setWifiConnect(qssid, qpass);
  }
}

void setService() {
  ServerHTTP.on("/", getWebAvailable);
  ServerHTTP.on("/status", getWebStatus);
  ServerHTTP.on("/setcolor", setWebChange);
  ServerHTTP.on("/getsetup", getWebSetup);
  ServerHTTP.on("/setup", setWebSetup);

  ServerHTTP.begin();
}

boolean getPushButton() {
  if (digitalRead(pinRST) == HIGH) {
    return true;
  } else {
    return false;
  }
}

byte getStatePushButton() {
  if (getPushButton() == false) {
    return 0;
  }
  for (int i = 0; i < 50; i++) {
    if (getPushButton() == false) {
      return 1;
    }
    delay(100);
  }
  return 2;
}

void setup() {
  pinMode(2, OUTPUT);
  pinMode(pinLED, OUTPUT);
  pinMode(pinRST, INPUT);
  pinMode(pinON, OUTPUT);
  Serial.begin(115200);

  setEEPROMStart();
  wifiSSID = "Skripsi-" + getUID();
  delay(10);

  FastLED.addLeds<WS2811, pinLED, RGB>(leds, NUM_LEDS);
  setLED(255, 255, 255);

  setWifiAP();

  esid = getEEPROMReadSSID();
  epass = getEEPROMReadPassword();
  if (esid.length() > 0) {
    setWifiConnect(esid, epass);
  }
  setService();
  setLED(255, 255, 255);
}

void loop() {
  ServerHTTP.handleClient();
  leds[0] = CRGB(redVal, greenVal, blueVal);
  FastLED.show();

  lastPush = getStatePushButton();
  if (lastPush == 1) {
    ESP.reset();
  } else if (lastPush == 2) {
    setEEPROMFormat();
  }
}
