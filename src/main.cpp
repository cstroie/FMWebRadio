#include <Arduino.h>
#include <Wire.h>
#include <RDA5807.h>
#include <U8g2lib.h>

// Display setup (Nokia 5110)
U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 7, /* dc=*/ 6, /* reset=*/ 5);

// RDA5807 FM receiver
RDA5807 radio;

// Pin definitions
#define BTN_UP 2
#define BTN_DOWN 3
#define BTN_OK 4

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
    
  } while (u8g2.nextPage());
}
