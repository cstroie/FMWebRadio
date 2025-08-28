#include <Arduino.h>
#include <Wire.h>
#include <RDA5807M.h>
#include <U8g2lib.h>

// Display setup (Nokia 5110)
U8G2_PCD8544_84X48_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 7, /* dc=*/ 6, /* reset=*/ 5);

// RDA5807M FM receiver
RDA5807M radio;

// Pin definitions
#define ENCODER_CLK 2
#define ENCODER_DT 3
#define ENCODER_SW 4

// Variables for encoder
volatile int encoderPos = 0;
volatile int lastEncoderPos = 0;
int lastMSB = 0;
int lastLSB = 0;

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
  
  // Initialize encoder pins
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Attach interrupt for encoder
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), updateEncoder, CHANGE);
  
  // Initialize radio
  radio.init();
  radio.setFrequency(currentFrequency);
  radio.setVolume(volume);
  radioOn = true;
  
  // Display initial screen
  updateDisplay();
}

void loop() {
  // Check for encoder position change
  if (encoderPos != lastEncoderPos) {
    // Change frequency based on encoder rotation
    if (encoderPos > lastEncoderPos) {
      currentFrequency += 0.1;
      if (currentFrequency > 108.0) currentFrequency = 87.5;
    } else {
      currentFrequency -= 0.1;
      if (currentFrequency < 87.5) currentFrequency = 108.0;
    }
    
    // Update radio frequency
    radio.setFrequency(currentFrequency);
    lastEncoderPos = encoderPos;
    updateDisplay();
  }
  
  // Check for button press (toggle radio on/off)
  if (digitalRead(ENCODER_SW) == LOW) {
    delay(50); // Debounce
    if (digitalRead(ENCODER_SW) == LOW) {
      radioOn = !radioOn;
      if (radioOn) {
        radio.setFrequency(currentFrequency);
      } else {
        radio.setMute(true);
      }
      updateDisplay();
      
      // Wait for button release
      while (digitalRead(ENCODER_SW) == LOW) delay(10);
    }
  }
  
  delay(50);
}

// Interrupt service routine for encoder
void updateEncoder() {
  int MSB = digitalRead(ENCODER_CLK);
  int LSB = digitalRead(ENCODER_DT);

  int encoded = (MSB << 1) | LSB;
  int sum = (lastMSB << 3) | (lastLSB << 2) | (MSB << 1) | LSB;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    encoderPos++;
  }
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    encoderPos--;
  }

  lastMSB = MSB;
  lastLSB = LSB;
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
