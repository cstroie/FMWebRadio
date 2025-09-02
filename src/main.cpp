#include <Arduino.h>
#include <Wire.h>
#include <RDA5807.h>
#include <U8g2lib.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif

// Forward declarations
void updateDisplay();

#if defined(ESP8266) || defined(ESP32)
void handleRoot();
void handleUp();
void handleDown();
void handleToggle();
#endif

// Display setup (Nokia 5110)
#if defined(ESP8266)
// ESP8266 pin mapping
U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ D7, /* dc=*/ D6, /* reset=*/ D5);
#elif defined(ESP32)
// ESP32 pin mapping
U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 15, /* dc=*/ 4, /* reset=*/ 5);
#else
// Default Arduino pin mapping
U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 7, /* dc=*/ 6, /* reset=*/ 5);
#endif

// RDA5807 FM receiver
RDA5807 radio;

#if defined(ESP8266)
// Web server
ESP8266WebServer server(80);

// WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

// AP credentials
const char* ap_ssid = "FM_Radio_AP";
const char* ap_password = "12345678";
#elif defined(ESP32)
// Web server
WebServer server(80);

// WiFi credentials
const char* ssid = "your-ssid";
const char* password = "your-password";

// AP credentials
const char* ap_ssid = "FM_Radio_AP";
const char* ap_password = "12345678";
#endif

// Pin definitions
#if defined(ESP8266)
// ESP8266 pin mapping
#define BTN_UP D2
#define BTN_DOWN D3
#define BTN_OK D4
#elif defined(ESP32)
// ESP32 pin mapping
#define BTN_UP 12
#define BTN_DOWN 14
#define BTN_OK 27
#else
// Default Arduino pin mapping
#define BTN_UP 2
#define BTN_DOWN 3
#define BTN_OK 4
#endif

// Variables for buttons
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200; // ms

// Radio settings
float currentFrequency = 87.5; // Start frequency in MHz
bool radioOn = false;
int volume = 5; // Volume level 0-15

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  
  // Initialize display
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Initialize button pins
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  
#if defined(ESP8266) || defined(ESP32)
  // Start AP mode (always available)
  #if defined(ESP8266)
  WiFi.softAP(ap_ssid, ap_password);
  #elif defined(ESP32)
  WiFi.softAP(ap_ssid, ap_password);
  #endif
  
  Serial.println("AP started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Attempt to connect to WiFi station
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  // Wait for connection or timeout
  unsigned long wifiStartTime = millis();
  const unsigned long wifiTimeout = 10000; // 10 seconds
  
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartTime < wifiTimeout)) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Station IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi station connection failed");
  }
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/up", handleUp);
  server.on("/down", handleDown);
  server.on("/toggle", handleToggle);
  server.begin();
#endif
  
  // Initialize radio
  radio.setup();
  radio.setFrequency(currentFrequency);
  radio.setVolume(volume);
  radioOn = true;
  
  // Display initial screen
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
#if defined(ESP8266) || defined(ESP32)
  // Handle web server requests
  server.handleClient();
#endif
  
  // Check for UP button press (increase frequency)
  if (digitalRead(BTN_UP) == LOW && (currentMillis - lastButtonPress > debounceDelay)) {
    currentFrequency += 0.1;
    if (currentFrequency > 108.0) currentFrequency = 87.5;
    
    // Update radio frequency
    radio.setFrequency(currentFrequency);
    updateDisplay();
    lastButtonPress = currentMillis;
    
    // Wait for button release
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  
  // Check for DOWN button press (decrease frequency)
  if (digitalRead(BTN_DOWN) == LOW && (currentMillis - lastButtonPress > debounceDelay)) {
    currentFrequency -= 0.1;
    if (currentFrequency < 87.5) currentFrequency = 108.0;
    
    // Update radio frequency
    radio.setFrequency(currentFrequency);
    updateDisplay();
    lastButtonPress = currentMillis;
    
    // Wait for button release
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }
  
  // Check for OK button press (toggle radio on/off)
  if (digitalRead(BTN_OK) == LOW && (currentMillis - lastButtonPress > debounceDelay)) {
    radioOn = !radioOn;
    if (radioOn) {
      radio.setFrequency(currentFrequency);
      radio.setMute(false);
    } else {
      radio.setMute(true);
    }
    updateDisplay();
    lastButtonPress = currentMillis;
    
    // Wait for button release
    while (digitalRead(BTN_OK) == LOW) delay(10);
  }
  
  delay(10);
}


// Update display with current information
void updateDisplay() {
  u8g2.firstPage();
  do {
    // Display title
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "FM Radio");
    
    // Display frequency
    u8g2.setFont(u8g2_font_10x20_tn);
    char freqStr[10];
    dtostrf(currentFrequency, 5, 1, freqStr);
    u8g2.drawStr(10, 30, freqStr);
    u8g2.drawStr(65, 30, "MHz");
    
    // Display status
    u8g2.setFont(u8g2_font_6x10_tf);
    if (radioOn) {
      u8g2.drawStr(0, 45, "ON ");
    } else {
      u8g2.drawStr(0, 45, "OFF");
    }
    
    // Display volume
    u8g2.setCursor(30, 45);
    u8g2.print("Vol: ");
    u8g2.print(volume);
    
#if defined(ESP8266) || defined(ESP32)
    // Display AP IP address (always available)
    u8g2.setCursor(0, 55);
    u8g2.print("AP: ");
    u8g2.print(WiFi.softAPIP());
#endif
    
  } while (u8g2.nextPage());
}

#if defined(ESP8266) || defined(ESP32)
// Web server handlers
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><title>FM Radio Control</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }";
  html += "button { font-size: 24px; padding: 15px; margin: 10px; width: 200px; }";
  html += ".freq { font-size: 36px; margin: 20px; }";
  html += ".status { font-size: 24px; margin: 20px; }";
  html += "</style></head>";
  html += "<body>";
  html += "<h1>FM Radio Control</h1>";
  html += "<div class='freq'>" + String(currentFrequency, 1) + " MHz</div>";
  html += "<div class='status'>Status: " + String(radioOn ? "ON" : "OFF") + "</div>";
  html += "<div class='status'>Volume: " + String(volume) + "</div>";
  html += "<button onclick='location.href=\"/up\"'>UP</button><br>";
  html += "<button onclick='location.href=\"/down\"'>DOWN</button><br>";
  html += "<button onclick='location.href=\"/toggle\"'>TOGGLE</button><br>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleUp() {
  currentFrequency += 0.1;
  if (currentFrequency > 108.0) currentFrequency = 87.5;
  radio.setFrequency(currentFrequency);
  updateDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDown() {
  currentFrequency -= 0.1;
  if (currentFrequency < 87.5) currentFrequency = 108.0;
  radio.setFrequency(currentFrequency);
  updateDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleToggle() {
  radioOn = !radioOn;
  if (radioOn) {
    radio.setFrequency(currentFrequency);
    radio.setMute(false);
  } else {
    radio.setMute(true);
  }
  updateDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}
#endif
