# FMWebRadio

An ESP8266/ESP32 based FM radio receiver with a web interface and Nokia 5110 display. Features RDA5807 FM tuner, RDS support, WiFi connectivity, and both hardware button controls and web-based remote control.

## Features

- Tune through FM frequencies (87.5-108.0 MHz)
- Display current frequency on Nokia 5110 LCD
- Power on/off functionality
- Volume control (fixed at level 5 in this implementation)
- Button debounce for reliable operation
- Web interface control for ESP platforms (ESP8266, ESP32, ESP32C3)
- Dual WiFi mode (Access Point + Station) for ESP platforms
- Configuration file for WiFi credentials
- RDS data decoding and display (station name, program type, radio text)
- Automatic station seeking with signal strength detection

## Hardware Requirements

### Supported Platforms:
- Arduino Micro
- Arduino Uno
- Arduino Nano
- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- ESP32 (ESP32 Dev Kit, etc.)
- ESP32-C3

### Components:
- RDA5807M FM Receiver Module
- Nokia 5110 LCD Display (PCD8544 controller)
- 3 Push Buttons
- Jumper wires
- Breadboard

## Pin Connections

### Display (Nokia 5110)
#### Arduino Platforms (Micro, Uno, Nano):
- CS: Pin 7
- DC: Pin 6
- RESET: Pin 5
- SPI connections (MOSI, SCK) use default SPI pins

#### ESP8266:
- CS: D7
- DC: D6
- RESET: D5
- SPI connections use default SPI pins

#### ESP32:
- CS: GPIO 15
- DC: GPIO 4
- RESET: GPIO 5
- SPI connections use default SPI pins

### Buttons
#### Arduino Platforms (Micro, Uno, Nano):
- UP Button: Pin 2 (Increase frequency)
- DOWN Button: Pin 3 (Decrease frequency)
- OK Button: Pin 4 (Power on/off)

#### ESP8266:
- UP Button: D2
- DOWN Button: D3
- OK Button: D4

#### ESP32:
- UP Button: GPIO 12
- DOWN Button: GPIO 14
- OK Button: GPIO 27

### RDA5807M
- Connected via I2C (SDA, SCL using default pins for each platform)

## Dependencies

This project uses the following libraries:
- Wire
- RDA5807M
- U8g2
- WiFi (for ESP platforms)
- WebServer (for ESP platforms)

See `platformio.ini` for complete dependency list.

## Installation

1. Connect the hardware according to the pin connections above
2. Install the required libraries (automatically handled by PlatformIO)
3. For ESP platforms, rename `config.h.template` to `config.h` and update with your WiFi credentials
4. Upload the code to your microcontroller using PlatformIO
5. Power on and use the buttons or web interface to control the radio

## Usage

### Physical Controls:
- Press UP/DOWN buttons to change frequency
- Press OK button to turn the radio on/off
- Hold UP/DOWN buttons for 1 second to automatically seek to the next/previous FM station
- The display shows the current frequency, radio status, and RDS information (station name, etc.)

### Web Interface (ESP platforms only):
- When using ESP8266, ESP32, or ESP32C3, the device creates a WiFi access point named "FM_Radio_AP"
- Connect to this open AP and access the web interface at http://192.168.4.1
- The web interface provides the same controls as the physical buttons:
  - UP (+0.1): Increases frequency by 0.1 MHz
  - DOWN (-0.1): Decreases frequency by 0.1 MHz
  - SEEK UP: Automatically searches for the next strong FM station
  - SEEK DOWN: Automatically searches for the previous strong FM station
  - TOGGLE: Turns the radio on/off
- RDS information display:
  - Station name (Program Service)
  - Program type
  - Radio text (song info, etc.)
- The device will also attempt to connect to your WiFi network (configured in config.h)

## License

GNU General Public License v3.0
