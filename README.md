# jaguarcartreader

Modification of @sakman55 Standalone Atari Jaguar Cart Reader
Adds support for renaming game automatically when reading rom.

If match is found screen will display game name otherwise no match found will be displayed.

Programmed with Arduino IDE 1.8.19 and the following libraries:
SdFat 1.1.4
SPI 1.0
Adafruit_GFX_Library 1.2.7
Adafruit_SSD1306 2.5.10 **Note:** by default the Adafruit_SSD1306 sets the lcd height to 32 instead of 64. You will need to edit Adafruit_SSD1306.h and comment out this line:
  #define SSD1306_128_32
  and then uncomment out this line:
  //#define SSD1306_128_64

Testing was only done with Read Rom functionality

File Jag.txt needs to be copied to the root of the SD card

