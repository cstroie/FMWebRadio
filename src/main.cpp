/*
 * FMWebRadio - FM Radio with Web Interface
 * Copyright (C) 2025 Costin Stroie <costinstroie@eridu.eu.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

// Include user configuration or use defaults
#if defined(ESP8266) || defined(ESP32)
  #include "config.h"
#endif

// Forward declarations
void updateDisplay();
void seekUp();
void seekDown();
void checkRDSData();

#if defined(ESP8266) || defined(ESP32)
void handleRoot();
void handleUp();
void handleDown();
void handleToggle();
void handleSeekUp();
void handleSeekDown();
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
#elif defined(ESP32)
// Web server
WebServer server(80);
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
const unsigned long longPressDelay = 1000; // ms for station seeking

// Radio settings
float currentFrequency = 87.5; // Start frequency in MHz
bool radioOn = false;
int volume = 5; // Volume level 0-15

// RDS data
char rdsProgramService[9] = "";     // 8 chars + null terminator
char rdsRadioText[65] = "";         // 64 chars + null terminator
char rdsProgramType[17] = "";       // 16 chars + null terminator
bool rdsTrafficProgram = false;
bool rdsTrafficAnnouncement = false;
unsigned int rdsPI = 0;             // Program Identification

// WiFi connection state (for ESP platforms)
#if defined(ESP8266) || defined(ESP32)
bool wifiConnectAttempted = false;
unsigned long wifiConnectStartTime = 0;
const unsigned long wifiConnectTimeout = 10000; // 10 seconds
#endif

/**
 * @brief Setup function - initializes all components
 * 
 * This function performs the following tasks:
 * 1. Initializes serial communication for debugging
 * 2. Sets up the Nokia 5110 display
 * 3. Configures button input pins with pull-up resistors
 * 4. For ESP platforms:
 *    - Starts WiFi Access Point mode (always available)
 *    - Sets up web server routes and starts the server
 *    - Initializes WiFi connection state variables
 * 5. Initializes the RDA5807 FM radio module
 * 6. Displays initial information on the screen
 */
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
  #ifdef AP_PASSWORD
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  #else
    WiFi.softAP(AP_SSID);  // Open AP (no password)
  #endif
  
  Serial.println("AP started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/up", handleUp);
  server.on("/down", handleDown);
  server.on("/seekup", handleSeekUp);
  server.on("/seekdown", handleSeekDown);
  server.on("/toggle", handleToggle);
  server.begin();
  
  // Initialize WiFi connection state
  wifiConnectAttempted = false;
  wifiConnectStartTime = 0;
#endif
  
  // Initialize radio
  radio.setup();
  radio.setFrequency(currentFrequency);
  radio.setVolume(volume);
  radioOn = true;
  
  // Initialize RDS data
  memset(rdsProgramService, 0, sizeof(rdsProgramService));
  memset(rdsRadioText, 0, sizeof(rdsRadioText));
  memset(rdsProgramType, 0, sizeof(rdsProgramType));
  
  // Display initial screen
  updateDisplay();
}

/**
 * @brief Main loop function - handles button presses and web requests
 * 
 * This function runs continuously and performs the following tasks:
 * 1. For ESP platforms: 
 *    - Handles incoming web server requests
 *    - Manages non-blocking WiFi station connection
 * 2. Checks for button presses with debounce logic:
 *    - UP button (short press): Increases frequency by 0.1 MHz (wraps from 108.0 to 87.5)
 *    - DOWN button (short press): Decreases frequency by 0.1 MHz (wraps from 87.5 to 108.0)
 *    - UP button (long press): Seeks up to next station
 *    - DOWN button (long press): Seeks down to next station
 *    - OK button: Toggles radio power state (ON/OFF)
 * 3. Updates the display when changes occur
 * 4. Implements button debouncing to prevent multiple triggers
 */
void loop() {
  unsigned long currentMillis = millis();
  
#if defined(ESP8266) || defined(ESP32)
  // Handle web server requests
  server.handleClient();
  
  // Handle non-blocking WiFi station connection
  if (!wifiConnectAttempted) {
    // Attempt to connect to WiFi station
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    wifiConnectAttempted = true;
    wifiConnectStartTime = currentMillis;
  }
  
  // Check WiFi connection status
  if (wifiConnectAttempted && WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Station IP address: ");
    Serial.println(WiFi.localIP());
    // Reset flag to prevent repeated printing
    wifiConnectAttempted = true; // Keep it true to prevent reconnection attempts
  } 
  else if (wifiConnectAttempted && (currentMillis - wifiConnectStartTime > wifiConnectTimeout) && WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi station connection failed or timed out");
    // Reset flag to prevent repeated printing
    wifiConnectAttempted = true; // Keep it true to prevent reconnection attempts
  }
  else if (wifiConnectAttempted && WiFi.status() != WL_CONNECTED && (currentMillis - wifiConnectStartTime <= wifiConnectTimeout)) {
    // Still trying to connect, print dots periodically
    static unsigned long lastDotPrint = 0;
    if (currentMillis - lastDotPrint > 500) {
      Serial.print(".");
      lastDotPrint = currentMillis;
    }
  }
  
  // Periodically check for RDS data (every 500ms)
  static unsigned long lastRdsCheck = 0;
  if (currentMillis - lastRdsCheck > 500) {
    checkRDSData();
    lastRdsCheck = currentMillis;
  }
#endif
  
  // Check for UP button press (increase frequency)
  if (digitalRead(BTN_UP) == LOW && (currentMillis - lastButtonPress > debounceDelay)) {
    unsigned long buttonPressTime = currentMillis;
    
    // Wait for button release or long press
    while (digitalRead(BTN_UP) == LOW) {
      // Check for long press (station seeking)
      if (currentMillis - buttonPressTime > longPressDelay) {
        // Seek up to next station
        seekUp();
        break;
      }
      delay(10);
      currentMillis = millis();
    }
    
    // If it wasn't a long press, do normal frequency increment
    if (currentMillis - buttonPressTime <= longPressDelay) {
      currentFrequency += 0.1;
      if (currentFrequency > 108.0) currentFrequency = 87.5;
      
      // Update radio frequency
      radio.setFrequency(currentFrequency);
      updateDisplay();
    }
    
    lastButtonPress = currentMillis;
  }
  
  // Check for DOWN button press (decrease frequency)
  if (digitalRead(BTN_DOWN) == LOW && (currentMillis - lastButtonPress > debounceDelay)) {
    unsigned long buttonPressTime = currentMillis;
    
    // Wait for button release or long press
    while (digitalRead(BTN_DOWN) == LOW) {
      // Check for long press (station seeking)
      if (currentMillis - buttonPressTime > longPressDelay) {
        // Seek down to next station
        seekDown();
        break;
      }
      delay(10);
      currentMillis = millis();
    }
    
    // If it wasn't a long press, do normal frequency decrement
    if (currentMillis - buttonPressTime <= longPressDelay) {
      currentFrequency -= 0.1;
      if (currentFrequency < 87.5) currentFrequency = 108.0;
      
      // Update radio frequency
      radio.setFrequency(currentFrequency);
      updateDisplay();
    }
    
    lastButtonPress = currentMillis;
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


/**
 * @brief Update the Nokia 5110 display with current radio information
 * 
 * This function updates the display with:
 * - Title "FM Radio"
 * - Current frequency in MHz
 * - Radio status (ON/OFF)
 * - Volume level
 * - For ESP platforms: RDS data (if available) and Access Point IP address
 * 
 * The display uses a double-buffering technique where all content is drawn
 * to a buffer first, then displayed all at once to prevent flickering.
 */
void updateDisplay() {
  u8g2.firstPage();
  do {
    // Display title or station name
    u8g2.setFont(u8g2_font_6x10_tf);
    if (strlen(rdsProgramService) > 0) {
      u8g2.drawStr(0, 10, rdsProgramService);
    } else {
      u8g2.drawStr(0, 10, "FM Radio");
    }
    
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
    
    // Display RDS information if available
    u8g2.setFont(u8g2_font_5x7_tf);
    if (strlen(rdsProgramService) > 0) {
      u8g2.setCursor(0, 55);
      u8g2.print(rdsProgramService);
    } else if (strlen(rdsRadioText) > 0) {
      // Truncate radio text to fit display width
      char truncatedText[12]; // ~11 chars fit on display
      strncpy(truncatedText, rdsRadioText, sizeof(truncatedText) - 1);
      truncatedText[sizeof(truncatedText) - 1] = '\0';
      u8g2.setCursor(0, 55);
      u8g2.print(truncatedText);
    }
    
  } while (u8g2.nextPage());
}

/**
 * @brief Check for and update RDS data from the radio
 * 
 * This function polls the RDA5807 for available RDS data and updates
 * the internal RDS data structures. It retrieves:
 * - Program Service (PS) name
 * - Radio Text (RT)
 * - Program Type (PTY)
 * - Traffic flags
 * - Program Identification (PI)
 */
void checkRDSData() {
  // Check if RDS data is available
  if (radio.getRDSready()) {
    // Get Program Service name (8 characters)
    radio.getRDS_PS(rdsProgramService);
    
    // Get Radio Text (up to 64 characters)
    radio.getRDS_RT(rdsRadioText);
    
    // Get Program Type
    uint8_t pty = radio.getRDS_PTY();
    // Convert PTY code to string (simplified)
    snprintf(rdsProgramType, sizeof(rdsProgramType), "PTY:%d", pty);
    
    // Get Traffic flags
    rdsTrafficProgram = radio.getRDS_TP();
    rdsTrafficAnnouncement = radio.getRDS_TA();
    
    // Get Program Identification
    rdsPI = radio.getRDS_PI();
    
    // Update display if RDS data changed
    updateDisplay();
  }
}

/**
 * @brief Seek up to the next valid FM station
 * 
 * This function implements station seeking by:
 * 1. Increasing frequency in small steps
 * 2. Checking the RSSI (signal strength) at each step
 * 3. Stopping when a strong enough signal is found
 * 4. Wrapping from 108.0 MHz to 87.5 MHz if needed
 */
void seekUp() {
  float originalFrequency = currentFrequency;
  float step = 0.1; // MHz
  int rssiThreshold = 30; // Minimum RSSI for a valid station
  int maxSteps = 2050; // Maximum steps to prevent infinite loop (205 MHz range / 0.1 MHz step)
  
  Serial.println("Seeking up...");
  
  for (int i = 0; i < maxSteps; i++) {
    currentFrequency += step;
    if (currentFrequency > 108.0) currentFrequency = 87.5;
    
    // Set the new frequency
    radio.setFrequency(currentFrequency);
    
    // Small delay to allow RSSI to stabilize
    delay(50);
    
    // Check signal strength
    int rssi = radio.getRssi();
    
    // If we found a strong enough signal, stop seeking
    if (rssi > rssiThreshold) {
      Serial.print("Found station at ");
      Serial.print(currentFrequency);
      Serial.print(" MHz with RSSI ");
      Serial.println(rssi);
      updateDisplay();
      return;
    }
    
    // If we've gone full circle, stop seeking
    if (abs(currentFrequency - originalFrequency) < 0.01 && i > 10) {
      Serial.println("No stations found during seek up");
      break;
    }
  }
  
  // If no station found, restore original frequency
  currentFrequency = originalFrequency;
  radio.setFrequency(currentFrequency);
  updateDisplay();
}

/**
 * @brief Seek down to the next valid FM station
 * 
 * This function implements station seeking by:
 * 1. Decreasing frequency in small steps
 * 2. Checking the RSSI (signal strength) at each step
 * 3. Stopping when a strong enough signal is found
 * 4. Wrapping from 87.5 MHz to 108.0 MHz if needed
 */
void seekDown() {
  float originalFrequency = currentFrequency;
  float step = 0.1; // MHz
  int rssiThreshold = 30; // Minimum RSSI for a valid station
  int maxSteps = 2050; // Maximum steps to prevent infinite loop (205 MHz range / 0.1 MHz step)
  
  Serial.println("Seeking down...");
  
  for (int i = 0; i < maxSteps; i++) {
    currentFrequency -= step;
    if (currentFrequency < 87.5) currentFrequency = 108.0;
    
    // Set the new frequency
    radio.setFrequency(currentFrequency);
    
    // Small delay to allow RSSI to stabilize
    delay(50);
    
    // Check signal strength
    int rssi = radio.getRssi();
    
    // If we found a strong enough signal, stop seeking
    if (rssi > rssiThreshold) {
      Serial.print("Found station at ");
      Serial.print(currentFrequency);
      Serial.print(" MHz with RSSI ");
      Serial.println(rssi);
      updateDisplay();
      return;
    }
    
    // If we've gone full circle, stop seeking
    if (abs(currentFrequency - originalFrequency) < 0.01 && i > 10) {
      Serial.println("No stations found during seek down");
      break;
    }
  }
  
  // If no station found, restore original frequency
  currentFrequency = originalFrequency;
  radio.setFrequency(currentFrequency);
  updateDisplay();
}

#if defined(ESP8266) || defined(ESP32)
/**
 * @brief Handle root web page request
 * 
 * This function generates and sends the main web interface page
 * that displays current radio status and provides control buttons.
 * The page includes:
 * - Current frequency in MHz
 * - Radio status (ON/OFF)
 * - Volume level
 * - Control buttons for UP, DOWN, and TOGGLE functions
 */
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
  
  // Add RDS information if available
  if (strlen(rdsProgramService) > 0) {
    html += "<div class='status'>Station: " + String(rdsProgramService) + "</div>";
  }
  if (strlen(rdsProgramType) > 0) {
    html += "<div class='status'>Type: " + String(rdsProgramType) + "</div>";
  }
  if (strlen(rdsRadioText) > 0) {
    html += "<div class='status'>Info: " + String(rdsRadioText) + "</div>";
  }
  
  html += "<button onclick='location.href=\"/up\"'>UP (+0.1)</button><br>";
  html += "<button onclick='location.href=\"/seekup\"'>SEEK UP</button><br>";
  html += "<button onclick='location.href=\"/down\"'>DOWN (-0.1)</button><br>";
  html += "<button onclick='location.href=\"/seekdown\"'>SEEK DOWN</button><br>";
  html += "<button onclick='location.href=\"/toggle\"'>TOGGLE</button><br>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/**
 * @brief Handle frequency increase request from web interface
 * 
 * Increases the radio frequency by 0.1 MHz with wraparound
 * from 108.0 MHz to 87.5 MHz, updates the radio module,
 * refreshes the display, and redirects back to the main page.
 */
void handleUp() {
  currentFrequency += 0.1;
  if (currentFrequency > 108.0) currentFrequency = 87.5;
  radio.setFrequency(currentFrequency);
  updateDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

/**
 * @brief Handle frequency decrease request from web interface
 * 
 * Decreases the radio frequency by 0.1 MHz with wraparound
 * from 87.5 MHz to 108.0 MHz, updates the radio module,
 * refreshes the display, and redirects back to the main page.
 */
void handleDown() {
  currentFrequency -= 0.1;
  if (currentFrequency < 87.5) currentFrequency = 108.0;
  radio.setFrequency(currentFrequency);
  updateDisplay();
  server.sendHeader("Location", "/");
  server.send(303);
}

/**
 * @brief Handle station seek up request from web interface
 * 
 * Seeks up to the next valid FM station using RSSI detection,
 * updates the radio module, refreshes the display, and 
 * redirects back to the main page.
 */
void handleSeekUp() {
  seekUp();
  server.sendHeader("Location", "/");
  server.send(303);
}

/**
 * @brief Handle station seek down request from web interface
 * 
 * Seeks down to the next valid FM station using RSSI detection,
 * updates the radio module, refreshes the display, and 
 * redirects back to the main page.
 */
void handleSeekDown() {
  seekDown();
  server.sendHeader("Location", "/");
  server.send(303);
}

/**
 * @brief Handle radio power toggle request from web interface
 * 
 * Toggles the radio power state between ON and OFF.
 * When turning ON, unmutes the radio and sets the current frequency.
 * When turning OFF, mutes the radio.
 * Updates the display and redirects back to the main page.
 */
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
