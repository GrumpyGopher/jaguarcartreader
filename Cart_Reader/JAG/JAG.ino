//******************************************************************************
//* Atari Jaguar Dumper                                                        *
//*                                                                            *
//* Author:  skaman                                                            *
//* Date:    2024-05-20                                                        *
//*                                                                            *
//******************************************************************************
// HARDWARE PROFILE
// PIN ASSIGNMENTS
// DATA D0-D7 - PORTF
// DATA D8-D15 - PORTK
// DATA D16-D23 - PORTL
// DATA D24-D31 - PORTC
// 74HC595 (ADDR) PINS - PORTE: ADDR (PE3), SRCLR (PE4), SRCLK (PE5)
//                       PORTG: RCLK (PG5)
// EEPROM PINS - EEPDO (PA5), EEPSK (PA6), EEPCS (PA7), [EEPDI = DATA D0 (PF0)]
// CONTROL PINS - PORTH: OEH (PH3), OEL (PH4), CE (PH5), WE (PH6)
// BUTTON - PORTB (PB4)
// LEDs - PORTB: RED(PB7), GREEN(PB6), BLUE(PB5)
//******************************************************************************
// OLED Library:  https://github.com/adafruit/Adafruit_SSD1306
//******************************************************************************
#define OLED // OLED Display [COMMENT OUT FOR SERIAL OUTPUT]
//******************************************************************************

// RGB LED COMMON ANODE
#define LED_RED_OFF PORTB |= (1<<7)
#define LED_RED_ON PORTB &= ~(1<<7)
#define LED_GREEN_OFF PORTB |= (1<<6)
#define LED_GREEN_ON PORTB &= ~(1<<6)
#define LED_BLUE_OFF PORTB |= (1<<5)
#define LED_BLUE_ON PORTB &= ~(1<<5)

// SD Card (Pin 50 = MISO, Pin 51 = MOSI, Pin 52 = SCK, Pin 53 = SS)
#include <SdFat.h>
#define chipSelectPin 53
SdFat sd;
File myFile;
char folder[36];
char fileCount[4];
byte sdBuffer[512];

char fileName[26];
char filePath[50];
byte currPage;
byte lastPage;
byte numPage;
boolean root = 0;
boolean filebrowse = 0;
char fileOptions[30][20];

uint16_t tempDataLO = 0;
uint16_t tempDataHI = 0;

char romName[16]; // "JAG.J64", "MemoryTrack.J64"
unsigned long cartSize; // (0x20000)/0x100000/0x200000/0x400000
byte romSize = 0; // 0 = 1MB, 1 = 2MB, 2 = 4MB

int serialinput; // Serial Data Input

// Variable to count errors
unsigned long writeErrors;

// SAVE TYPE
// 0 = SERIAL EEPROM
// 1 = FLASH
byte saveType = 0; // Serial EEPROM

// SERIAL EEPROM 93CX6
// CONTROL: EEPCS (PA7), EEPSK (PA6), EEPDI (PF0) [DATA D0]
// SERIAL DATA OUTPUT: EEPDO (PA5)
#define EEP_CS_SET PORTA |= (1<<7)
#define EEP_CS_CLEAR PORTA &= ~(1<<7)
#define EEP_SK_SET PORTA |= (1<<6)
#define EEP_SK_CLEAR PORTA &= ~(1<<6)
#define EEP_DI_SET PORTF |= (1<<0)
#define EEP_DI_CLEAR PORTF &= ~(1<<0)

// SERIAL EEPROM SIZES
// 0 = 93C46 = 128 byte = Standard
// 1 = 93C56 = 256 byte = Aftermarket
// 2 = 93C66 = 512 byte = Aftermarket
// 3 = 93C76 = 1024 byte = Aftermarket
// 4 = 93C86 = 2048 byte = Aftermarket - Battlesphere Gold
int eepSize;
byte EepBuf[2];
boolean EepBit[16];

// MEMORY TRACK CART
boolean memorytrack = 0;
char flashID[5];  // AT29C010 = "1FD5"
unsigned long flaSize = 0; // AT29C010 = 128K

// POWER FOR INT
int int_pow(int base, int exp) {
    int result = 1;
    while (exp) {
        if (exp & 1)
           result *= base;
        exp /= 2;
        base *= base;
    }
    return result;
}

//******************************************************************************
// ARDUINO EEPROM
//******************************************************************************
#include <EEPROM.h>
int foldern;

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

// JAGUAR EEPROM MAPPING
// 08 ROM SIZE
// 10 EEP SIZE

// EEPROM MAPPING
// 00-01 FOLDER #
// 02-05 SNES/GB READER SETTINGS
// 06 LED - ON/OFF [SNES/GB]
// 07 MAPPER   [FC]
// 08 PRG SIZE [FC]
// 09 CHR SIZE [FC]
// 10 RAM SIZE [FC]

void resetEEPROM() {
  EEPROM_writeAnything(0, 0); // FOLDER #
//  EEPROM_writeAnything(2, 0); // CARTMODE
//  EEPROM_writeAnything(3, 0); // RETRY
//  EEPROM_writeAnything(4, 0); // STATUS
//  EEPROM_writeAnything(5, 0); // UNKNOWNCRC
//  EEPROM_writeAnything(6, 1); // LED (RESET TO ON)
//  EEPROM_writeAnything(7, 0); // MAPPER
//  EEPROM_writeAnything(8, 0); // PRG SIZE
//  EEPROM_writeAnything(9, 0); // CHR SIZE
//  EEPROM_writeAnything(10, 0); // RAM SIZE
}

//******************************************************************************
// OLED DISPLAY
//******************************************************************************
#include <SPI.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

// JAGUAR Logo 128x44
static const unsigned char PROGMEM jaglogo [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x03, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x40, 0x00, 0x0F, 0xC0, 0x00,
0x00, 0x1F, 0xFF, 0xFC, 0x07, 0x80, 0x04, 0x00, 0x78, 0x00, 0x00, 0x78, 0x01, 0xFF, 0xE0, 0x00,
0x00, 0x3F, 0xFF, 0xF8, 0x1F, 0x80, 0x3F, 0x00, 0x7C, 0x1C, 0x00, 0x78, 0x0F, 0xFF, 0xF0, 0x00,
0x00, 0x1F, 0xFF, 0xE0, 0x1F, 0x80, 0x7F, 0x80, 0xF8, 0x3C, 0x00, 0xF8, 0x3F, 0xFF, 0xF8, 0x00,
0x00, 0x1F, 0xE7, 0xC0, 0x3F, 0x80, 0xFF, 0x80, 0xF8, 0x3E, 0x01, 0xF8, 0xFF, 0xFF, 0xFC, 0x00,
0x00, 0x00, 0x07, 0xC0, 0x3F, 0x81, 0xFC, 0x00, 0xF0, 0x7C, 0x03, 0xF9, 0xFF, 0xFB, 0xFC, 0x00,
0x00, 0x00, 0x07, 0xC0, 0x7F, 0x83, 0xF0, 0x01, 0xE0, 0x7C, 0x06, 0xF3, 0xFF, 0xC1, 0xFC, 0x00,
0x00, 0x00, 0x07, 0xC0, 0x67, 0x87, 0xC0, 0x01, 0xE0, 0xF8, 0x0C, 0xF3, 0xFF, 0x80, 0xFC, 0x00,
0x00, 0x00, 0x07, 0x80, 0xE7, 0x87, 0x80, 0x01, 0xE0, 0xF8, 0x18, 0xF0, 0xEF, 0x00, 0xFC, 0x00,
0x00, 0x00, 0x07, 0x81, 0xC7, 0x8F, 0x00, 0x03, 0xC1, 0xF8, 0x38, 0xF0, 0x0F, 0x01, 0xF8, 0x00,
0x00, 0x00, 0x0F, 0x81, 0x87, 0x8F, 0x00, 0x07, 0xC1, 0xF0, 0x33, 0xF0, 0x1E, 0x01, 0xF8, 0x00,
0x00, 0x00, 0x0F, 0x83, 0x87, 0x9E, 0x00, 0x47, 0x81, 0xF0, 0x7F, 0xF0, 0x1C, 0x23, 0xE0, 0x00,
0x00, 0x00, 0x0F, 0x03, 0x0F, 0x9C, 0x01, 0xE7, 0x81, 0xE0, 0xFF, 0xF0, 0x1C, 0x3F, 0xC0, 0x00,
0x00, 0x00, 0x0F, 0x07, 0x3F, 0x9C, 0x07, 0xEF, 0x03, 0xE0, 0xFC, 0xF0, 0x3C, 0x1F, 0x80, 0x00,
0x00, 0x00, 0x1F, 0x0F, 0xF7, 0x9C, 0x1F, 0xEE, 0x03, 0xC1, 0xF0, 0xF0, 0x38, 0x7F, 0x00, 0x00,
0x00, 0x00, 0x1E, 0x0F, 0xC7, 0x9C, 0x21, 0xCE, 0x07, 0xC3, 0xE0, 0xF0, 0x39, 0x37, 0x80, 0x00,
0x00, 0x00, 0x1E, 0x3E, 0x07, 0x9C, 0x01, 0xDE, 0x07, 0xCF, 0x80, 0xF0, 0x79, 0xC9, 0xC0, 0x00,
0x00, 0x00, 0x3E, 0x1C, 0x07, 0x9C, 0x01, 0xDC, 0x0F, 0x8F, 0x00, 0xE0, 0x78, 0x24, 0x40, 0x00,
0x00, 0x00, 0x3C, 0x18, 0x07, 0x9C, 0x01, 0xDC, 0x1F, 0x87, 0x00, 0xE0, 0x71, 0x12, 0x60, 0x00,
0x00, 0x00, 0x7C, 0x38, 0x07, 0x9E, 0x01, 0x9C, 0x2F, 0x8E, 0x00, 0xE0, 0x70, 0xC9, 0x30, 0x00,
0x00, 0x00, 0x78, 0x30, 0x07, 0x8E, 0x01, 0x98, 0xCF, 0x1E, 0x00, 0xE0, 0x70, 0x24, 0x98, 0x00,
0x00, 0x00, 0xF8, 0x70, 0x07, 0x8F, 0x01, 0x9D, 0xCF, 0x1C, 0x00, 0x60, 0xF0, 0x36, 0x64, 0x00,
0x00, 0x00, 0xF0, 0x70, 0x07, 0x87, 0xC3, 0x9F, 0x8F, 0xBC, 0x00, 0x60, 0xF0, 0x11, 0x37, 0x00,
0x00, 0x01, 0xF0, 0xE0, 0x07, 0x81, 0xFE, 0x8E, 0x0F, 0xF8, 0x00, 0x61, 0xE0, 0x08, 0x8B, 0x80,
0x00, 0x01, 0xE0, 0xE0, 0x01, 0x80, 0xF8, 0x40, 0x0E, 0xF8, 0x00, 0x21, 0xE0, 0x04, 0x64, 0xC0,
0x00, 0x03, 0xE1, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x21, 0xC0, 0x03, 0x32, 0x60,
0x40, 0x07, 0xC3, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01, 0xE0, 0x00, 0x23, 0x80, 0x01, 0x99, 0x90,
0x20, 0x0F, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0, 0x00, 0x23, 0x80, 0x00, 0xC4, 0xC8,
0x10, 0x3F, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x00, 0x00, 0x23, 0x64,
0x0F, 0xFE, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x80, 0x00, 0x06, 0x00, 0x00, 0x10, 0x92,
0x03, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00, 0x08, 0x48,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x06, 0x24,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//******************************************************************************
// BUTTON
//******************************************************************************
// 4-Way Button
//#define buttonPin 10 // PB4
// Button timing variables
static int debounce = 20;
static int DCgap = 250;
static int holdTime = 2000;
static int longHoldTime = 5000;
// Other button variables
boolean buttonVal = HIGH;
boolean buttonLast = HIGH;
boolean DCwaiting = false;
boolean DConUp = false;
boolean singleOK = true;
long downTime = -1;
long upTime = -1;
boolean ignoreUp = false;
boolean waitForUp = false;
boolean holdEventPast = false;
boolean longHoldEventPast = false;

// Button
int b = 0;
int press = 1;
int doubleclick = 2;
int hold = 3;
int longhold = 4;

int checkButton() {
  int event = 0;
  // Read the state of the button
  buttonVal = (PINB & (1<<4)); // PB4
  // Button pressed down
  if (buttonVal == LOW && buttonLast == HIGH && (millis() - upTime) > debounce) {
    downTime = millis();
    ignoreUp = false;
    waitForUp = false;
    singleOK = true;
    holdEventPast = false;
    longHoldEventPast = false;
    if ((millis() - upTime) < DCgap && DConUp == false && DCwaiting == true)
      DConUp = true;
    else
      DConUp = false;
    DCwaiting = false;
  }
  // Button released
  else if (buttonVal == HIGH && buttonLast == LOW && (millis() - downTime) > debounce) {
    if (not ignoreUp) {
      upTime = millis();
      if (DConUp == false) DCwaiting = true;
      else {
        event = 2;
        DConUp = false;
        DCwaiting = false;
        singleOK = false;
      }
    }
  }
  // Test for normal click event: DCgap expired
  if ( buttonVal == HIGH && (millis() - upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true) {
    event = 1;
    DCwaiting = false;
  }
  // Test for hold
  if (buttonVal == LOW && (millis() - downTime) >= holdTime) {
    // Trigger "normal" hold
    if (not holdEventPast) {
      event = 3;
      waitForUp = true;
      ignoreUp = true;
      DConUp = false;
      DCwaiting = false;
      //downTime = millis();
      holdEventPast = true;
    }
    // Trigger "long" hold
    if ((millis() - downTime) >= longHoldTime) {
      if (not longHoldEventPast) {
        event = 4;
        longHoldEventPast = true;
      }
    }
  }
  buttonLast = buttonVal;
  return event;
}

void wait() {
  while (1) {
    b = checkButton();
    if ((b == press) || (b == hold)) {
      break;
    }
  }
}

//******************************************************************************
// MENU
//******************************************************************************
// Menu Variables
int choice = 0;
int menu_choice = 0;
char menuChoices[8][20];

static const char menuItem1[] PROGMEM = "Read ROM";
static const char menuItem2[] PROGMEM = "Set ROM Size";
static const char menuItem3[] PROGMEM = "Read EEPROM";
static const char menuItem4[] PROGMEM = "Set EEPROM Size";
static const char menuItem5[] PROGMEM = "Write EEPROM";
static const char menuItem6[] PROGMEM = "Read MEMORY TRACK";
static const char menuItem7[] PROGMEM = "Write FLASH";
static const char* const baseMenu[] PROGMEM = {menuItem1, menuItem2, menuItem3, menuItem4, menuItem5, menuItem6, menuItem7};

static const char romItem1[] PROGMEM = "1MB ROM";
static const char romItem2[] PROGMEM = "2MB ROM";
static const char romItem3[] PROGMEM = "4MB ROM";
static const char* const romMenu[] PROGMEM = {romItem1, romItem2, romItem3};

static const char eepItem1[] PROGMEM = "128B (93C46)";
static const char eepItem2[] PROGMEM = "256B (93C56)";
static const char eepItem3[] PROGMEM = "512B (93C66)";
static const char eepItem4[] PROGMEM = "1024B (93C76)";
static const char eepItem5[] PROGMEM = "2048B (93C86)";
static const char* const saveMenu[] PROGMEM = {eepItem1, eepItem2, eepItem3, eepItem4, eepItem5};

// Converts a progmem array into a ram array
void convertPgm(const char* const pgmOptions[], byte numArrays) {
  for (byte i = 0; i < numArrays; i++) {
    strcpy_P(menuChoices[i], (char*)pgm_read_word(&(pgmOptions[i])));
  }
}

//******************************************************************************
// MAIN MENU
//******************************************************************************
void menu() {
#ifdef OLED
  // create menu with title " JAGUAR CART READER" and 7 options to choose from
  convertPgm(baseMenu, 7);
  unsigned char answer = menu_box(" JAGUAR CART READER", menuChoices, 7, 0);

  // wait for user choice to come back from the question box menu
  switch (answer) {
    // Read ROM
    case 0:
      clear();
      // Change working dir to root
      sd.chdir("/");
      readROM();
      break;

    // Set ROM Size
    case 1:
      sizeMenu();
      getCartInfo();
      break;

    // Read EEPROM
    case 2:
      clear();
      if (saveType == 0)
        readEEP();
      else {
        println_Msg(F("Cart has no EEPROM"));
        display.display();
      }
      break;

    // Set EEPROM Size
    case 3:
      eepMenu();
      getCartInfo();
      break;

    // Write EEPROM
    case 4:
      if (saveType == 0) {
        // Launch file browser
        SelectFileInSD();
        clear();
        writeEEP();
      }
      else {
        println_Msg(F("Cart has no EEPROM"));
        display.display();
      }
      break;

    // Read MEMORY TRACK
    case 5:
      readMEMORY();
      if (saveType == 1)
        readFLASH();
      else {
        println_Msg(F("Cart has no FLASH"));
        display.display();
      }
      break;

    // Write FLASH
    case 6:
      clear();
      if (saveType == 1) {
        // Launch file browser
        SelectFileInSD();
        clear();
        writeFLASH();
        verifyFLASH();
      }
      else {
        println_Msg(F("Cart has no FLASH"));
        display.display();
      }
      break;
  }
#else
  // Print menu to serial monitor
  println_Msg(F("COMMANDS:"));
  println_Msg(F("(0) Read ROM"));
  println_Msg(F("(1) Set ROM Size"));
  println_Msg(F("(2) Read EEPROM"));
  println_Msg(F("(3) Set EEPROM Size"));
  println_Msg(F("(4) Write EEPROM"));
  println_Msg(F("(5) Read MEMORY TRACK"));
  println_Msg(F("(6) Write FLASH"));
  println_Msg(F("(7) Display EEPROM"));
  println_Msg(F("(8) Erase EEPROM"));
  println_Msg("");
  print_Msg("Enter Command: ");
  while (Serial.available() == 0) {}
  serialinput = Serial.read();
//  println_Msg(serialinput);
//  println_Msg("");
  switch (serialinput) {
    case 0x30: //0
      println_Msg(F("0 (Read ROM)"));
      // Change working dir to root
      sd.chdir("/");
      readROM();
      break;

    case 0x31: //1
      println_Msg(F("1 (Set Size)"));
      sizeMenu();
      getCartInfo();
      break;

    case 0x32: //2
      println_Msg(F("2 (Read EEPROM)"));
      if (saveType == 0)
        readEEP();
      else {
        println_Msg(F("Cart has no EEPROM"));
        println_Msg(F(""));
      }
      break;

    case 0x33: //3
      println_Msg(F("3 (Set EEPROM Size)"));
      eepMenu();
      getCartInfo();
      break;

    case 0x34: //4
      println_Msg(F("4 (Write EEPROM)"));
      if (saveType == 0) {
        // Launch file browser
        SelectFileInSD();
        writeEEP();
      }
      else {
        println_Msg(F("Cart has no EEPROM"));
        println_Msg(F(""));
      }
      break;

    case 0x35: //5
      println_Msg(F("5 (Read MEMORY TRACK)"));
      readMEMORY();
      if (saveType == 1)
        readFLASH();
      else {
        println_Msg(F("Cart has no FLASH"));
        println_Msg(F(""));
      }
      break;

    case 0x36: //6
      println_Msg(F("6 (Write FLASH)"));
      if (saveType == 1) {
        // Launch file browser
        SelectFileInSD();
        writeFLASH();
        verifyFLASH();
      }
      else {
        println_Msg(F("Cart has no FLASH"));
        println_Msg(F(""));
      }
      break;

    case 0x37: //7
      println_Msg(F("7 (Display EEPROM)"));
      if (saveType == 0)
        EepromDisplay();
      else {
        println_Msg(F("Cart has no EEPROM"));
        println_Msg(F(""));
      }
      break;

    case 0x38: //8
      println_Msg(F("8 (Erase EEPROM)"));
      if (saveType == 0) {
        EepromEWEN();
        EepromERAL();
        EepromEWDS();
        println_Msg(F(""));
      }
      else {
        println_Msg(F("Cart has no EEPROM"));
        println_Msg(F(""));
      }
      break;
  }
#endif
}

//******************************************************************************
// ROM SIZE MENU
//******************************************************************************
void sizeMenu() {
#ifdef OLED
  convertPgm(romMenu, 3);
  unsigned char subMenu = menu_box("Select ROM Size", menuChoices, 3, 0);

  switch (subMenu) {
    case 0:
      romSize = 0;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x100000;
      break;

    case 1:
      romSize = 1;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x200000;
      break;

    case 2:
      romSize = 2;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x400000;
      break;
  }
#else
  println_Msg(F("Select Size:"));
  println_Msg(F("(0) 1MB ROM"));
  println_Msg(F("(1) 2MB ROM"));
  println_Msg(F("(2) 4MB ROM"));
  println_Msg("");
  print_Msg("Enter Command: ");
  while (Serial.available() == 0) {}
  serialinput = Serial.read();

  switch (serialinput) {
    case 0x30: //0
      println_Msg(F("0 (1MB ROM)"));
      romSize = 0;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x100000;
      break;

    case 0x31: //1
      println_Msg(F("1 (2MB ROM)"));
      romSize = 1;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x200000;
      break;

    case 0x32: //2
      println_Msg(F("2 (4MB ROM)"));
      romSize = 2;
      EEPROM_writeAnything(8, romSize);
      cartSize = 0x400000;
      break;
  }
#endif
}

//******************************************************************************
// EEPROM SIZE MENU
//******************************************************************************
void eepMenu() {
#ifdef OLED
  convertPgm(saveMenu, 5);
  unsigned char subMenu = menu_box("Select EEPROM Size", menuChoices, 5, 0);

  switch (subMenu) {
    case 0:
      eepSize = 0; // 128B
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("128B (93C46)"));
      display.display();
      break;

    case 1:
      eepSize = 1; // 256B
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("256B (93C56)"));
      display.display();
      break;

    case 2:
      eepSize = 2; // 512B
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("512B (93C66)"));
      display.display();
      break;

    case 3:
      eepSize = 3; // 1024B
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("1024B (93C76)"));
      display.display();
      break;

    case 4:
      eepSize = 4; // 2048B
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("2048B (93C86)"));
      display.display();
      break;
  }
#else
  println_Msg(F("Select Size:"));
  println_Msg(F("(0) 128B"));
  println_Msg(F("(1) 256B"));
  println_Msg(F("(2) 512B"));
  println_Msg(F("(3) 1024B"));
  println_Msg(F("(4) 2048B"));
  println_Msg("");
  print_Msg("Enter Command: ");
  while (Serial.available() == 0) {}
  serialinput = Serial.read();

  switch (serialinput) {
    case 0x30: //0
      println_Msg(F("0 (128B EEPROM)"));
      eepSize = 0;
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("128B (93C46)"));
      break;

    case 0x31: //1
      println_Msg(F("1 (256B EEPROM)"));
      eepSize = 1;
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("256B (93C56)"));
      break;

    case 0x32: //2
      println_Msg(F("2 (512B EEPROM)"));
      eepSize = 2;
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("512B (93C66)"));
      break;

    case 0x33: //3
      println_Msg(F("3 (1024B EEPROM)"));
      eepSize = 3;
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("1024B (93C76)"));
      break;

    case 0x34: //4
      println_Msg(F("4 (2048B EEPROM)"));
      eepSize = 4;
      EEPROM_writeAnything(10, eepSize);
      println_Msg(F("2048B (93C86)"));
      break;
  }
#endif
}

//******************************************************************************
// CRC32
//******************************************************************************
File crcFile;
char tempCRC[9];

static const uint32_t crc_32_tab[] PROGMEM = { /* CRC polynomial 0xedb88320 */
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

inline uint32_t updateCRC32(uint8_t ch, uint32_t crc) {
  uint32_t idx = ((crc) ^ (ch)) & 0xff;
  uint32_t tab_value = pgm_read_dword(crc_32_tab + idx);
  return tab_value ^ ((crc) >> 8);
}

uint32_t crc32(File &file, uint32_t &charcnt) {
  uint32_t oldcrc32 = 0xFFFFFFFF;
  charcnt = 0;
  while (file.available()) {
    crcFile.read(sdBuffer, 512);
    for (int x = 0; x < 512; x++) {
      uint8_t c = sdBuffer[x];
      charcnt++;
      oldcrc32 = updateCRC32(c, oldcrc32);
    }
  }
  return ~oldcrc32;
}

uint32_t crc32EEP(File &file, uint32_t &charcnt) {
  uint32_t oldcrc32 = 0xFFFFFFFF;
  charcnt = 0;
  while (file.available()) {
    crcFile.read(sdBuffer, 128);
    for (int x = 0; x < 128; x++) {
      uint8_t c = sdBuffer[x];
      charcnt++;
      oldcrc32 = updateCRC32(c, oldcrc32);
    }
  }
  return ~oldcrc32;
}

uint32_t calcCRC(char* checkFile, unsigned long filesize) {
  uint32_t crc;
  crcFile = sd.open(checkFile);
  if (filesize < 1024)
    crc = crc32EEP(crcFile, filesize);
  else
    crc = crc32(crcFile, filesize);
  crcFile.close();
  sprintf(tempCRC, "%08lX", crc);
  print_Msg(F("CRC: "));
  println_Msg(tempCRC);
  display_Update();
  return crc;
}

//******************************************************************************
// INIT
//******************************************************************************
void init_ports() {
  DDRB |= (1 << 5) | (1 << 6) | (1 << 7); // LEDS

  // Set Address Pins to Output ADDR(PE3), SRCLR(PE4), SRCLK(PE5), RCLK(PG5)
  DDRE |= (1 << 3) | (1 << 4) | (1 << 5);
  DDRG |= (1 << 5);

  // Set Control Pins to Output OEH(PH3), OEL(PH4), CE(PH5), WE(PH6)
  DDRH |=  (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

  // Set EEPROM Control Pins to Output EEPSK(PA6), EEPCS(PA7)
  DDRA |=  (1 << 6) | (1 << 7);

  // Set EEPROM Data to Input EEPDO(PA5)
  DDRA &= ~(1 << 5);

  // Set Data Pins (D0-D31) to Input
  DDRF = 0x00;
  DDRK = 0x00;
  DDRL = 0x00;
  DDRC = 0x00;

  // Set Control Pins HIGH - OEH(PH3), OEL(PH4), CE(PH5), WE(PH6)
  PORTH |= (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

  // Set 74HC595 (Address) Clear LOW - SRCLR (PE4)
  PORTE &= ~(1 << 4); // Disable Address Shift Register

  delay(200);
}

void InitSD() {
  if (!sd.begin(chipSelectPin, SPI_FULL_SPEED)) {
    LED_RED_ON;
    clear();
    println_Msg(F("SD CARD FAILED"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    LED_RED_OFF;
    reset();
  }
}

//******************************************************************************
// SETUP
//******************************************************************************
void setup() {
  init_ports();
  LED_RED_OFF;
  LED_GREEN_OFF;
  LED_BLUE_OFF;
#ifdef OLED
  //OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  clear();
  // Draw the Logo
  display.drawBitmap(0, 10, jaglogo, 128, 44, 1);
  display.display();
  delay(1000);
#else
  Serial.begin(9600);
  println_Msg(F("Jaguar Dumper"));
  println_Msg(F("2024 skaman"));
#endif
  InitSD();
#ifndef OLED
  // Print SD Info
  print_Msg(F("SD Card: "));
  print_Msg(sd.card()->cardSize() * 512E-9);
  print_Msg(F("GB FAT"));
  println_Msg(int(sd.vol()->fatType()));
  println_Msg(F(""));
#endif

  EEPROM_readAnything(8, romSize);
  EEPROM_readAnything(10, eepSize);
  // Print all the info
  if (romSize > 2) {
    romSize = 1; // default 2MB
    EEPROM_writeAnything(8, romSize);
  }
  cartSize = long(int_pow(2, romSize)) * 0x100000;
  strcpy(romName, "JAG");
  if (eepSize > 4) {
    eepSize = 0; // default 128B
    EEPROM_writeAnything(10, eepSize);
  }
  getCartInfo();
}

//******************************************************************************
// GET CART INFO
//******************************************************************************
void getCartInfo() {
  // Check for Memory Track NVRAM Cart
  readID();

  if (strcmp(flashID, "1FD5") == 0) { // AT29C010 128K FLASH
    memorytrack = 1; // Memory Track Found
    saveType = 1; // FLASH
    strcpy(romName, "MemoryTrack");
    cartSize = 0x20000;
    flaSize = 0x20000;
  }

  clear();
  println_Msg(F("CART INFO"));
  print_Msg(F("Name: "));
  println_Msg(romName);
  print_Msg(F("Size: "));
  if (memorytrack) {
    print_Msg(cartSize / 1024);
    println_Msg(F(" KB"));
  }
  else {
    print_Msg(cartSize / 1024 / 1024 );
    println_Msg(F(" MB"));
  }
  if (saveType == 0) {
    print_Msg(F("EEPROM: "));
    print_Msg(int_pow(2, eepSize) * 128); // 128/256/512/1024/2048 BYTES
    println_Msg(F(" B"));
  }
  else if (saveType == 1) {
    print_Msg(F("FLASH: "));
    print_Msg(flaSize / 1024);
    println_Msg(F(" KB"));
  }
  display_Update();

#ifdef OLED
    display.setCursor(0, 56);
    println_Msg(F("Press Button..."));
    display.display();
    wait();
#endif
}

//******************************************************************************
// SHIFT OUT
//******************************************************************************
// SHIFT OUT ADDRESS CODE
// SN74HC595 (ADDR) PINS - PORTE: ADDR (PE3), SRCLR (PE4), SRCLK (PE5)
//                         PORTG: RCLK (PG5)
// DATA (SER/DS) PIN 14 = PE3
// /OE (GND) PIN 13
// LATCH (RCLK/STCP) PIN 12 - LO/HI TO OUTPUT ADDRESS = PG5
// CLOCK (SRCLK/SHCP) PIN 11 - LO/HI TO READ ADDRESS  = PE5
// RESET (/SRCLR//MR) PIN 10  = PE4

#define SER_CLEAR    PORTE &= ~(1 << 3);
#define SER_SET      PORTE |= (1 << 3);
#define SRCLR_CLEAR  PORTE &= ~(1 << 4);
#define SRCLR_SET    PORTE |= (1 << 4);
#define CLOCK_CLEAR  PORTE &= ~(1 << 5);
#define CLOCK_SET    PORTE |= (1 << 5);
#define LATCH_CLEAR  PORTG &= ~(1 << 5);
#define LATCH_SET    PORTG |= (1 << 5);

// INPUT ADDRESS BYTE IN MSB
// LATCH LO BEFORE FIRST SHIFTOUT
// LATCH HI AFTER LAST SHIFOUT
void shiftOutFAST(byte addr) { // 
  for (int i = 7; i >= 0; i--) { // MSB
    CLOCK_CLEAR;
    if (addr & (1 << i)) {
      SER_SET; // 1
    }
    else {
      SER_CLEAR; // 0
    }
    CLOCK_SET; // shift bit
    SER_CLEAR; // reset 1
  }
  CLOCK_CLEAR;
}

//******************************************************************************
// READ DATA
//******************************************************************************
void readData(unsigned long myAddress) {
  SRCLR_CLEAR;
  SRCLR_SET;
  LATCH_CLEAR;

  shiftOutFAST((myAddress >> 16) & 0xFF);
  shiftOutFAST((myAddress >> 8) & 0xFF);
  shiftOutFAST(myAddress);
  LATCH_SET;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  __asm__("nop\n\t");

  // Setting CE(PH5) LOW
  PORTH &= ~(1 << 5);
  // Setting OEH3(PH3) + OEL(PH4) LOW
  PORTH &= ~(1 << 3) & ~(1 << 4);

  // Long delay here or there will be read errors
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Read
  tempDataLO = (((PINK & 0xFF) << 8) | (PINF & 0xFF)); // D0-D15 [ROM U1]
  tempDataHI = (((PINC & 0xFF) << 8) | (PINL & 0xFF)); // D16-D31 [ROM U2]

  // Setting OEH(PH3) + OEL(PH4) HIGH
  PORTH |= (1 << 3) | (1 << 4);
  // Setting CE(PH5) HIGH
  PORTH |= (1 << 5);
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  SRCLR_CLEAR;
}

// Switch data pins to write
void dataOut() {
  DDRF = 0xFF;
  DDRK = 0xFF;
  DDRL = 0xFF;
  DDRC = 0xFF;
}

// Switch data pins to read
void dataIn() {
  DDRF = 0x00;
  DDRK = 0x00;
  DDRL = 0x00;
  DDRC = 0x00;
}

//******************************************************************************
// READ ROM
//******************************************************************************
// Read rom and save to the SD card
void readROM() {
  // Set control
  dataIn();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".J64");

  // create a new folder
  sd.chdir();
  EEPROM_readAnything(0, foldern);
//  sprintf(folder, "JAG/ROM/%s/%d", romName, foldern);
  sprintf(folder, "JAG/ROM/%d", foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  clear();
  print_Msg(F("Saving ROM to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  int d = 0;
  for (unsigned long currBuffer = 0; currBuffer < cartSize; currBuffer += 512) {
    // Blink led
    if (currBuffer % 16384 == 0)
      PORTB = PORTB ^ 0xC0; // B11000000 = RED + GRN = YELLOW
    // MaskROM does not use A0 + A1 - Shift Address *4
    for (int currWord = 0; currWord < 512; currWord += 4) {
      readData(currBuffer + currWord);
      // Move WORD into SD Buffer
      sdBuffer[d] = (tempDataHI >> 8) & 0xFF;
      sdBuffer[d + 1] = tempDataHI & 0xFF;
      sdBuffer[d + 2] = (tempDataLO >> 8) & 0xFF;
      sdBuffer[d + 3] = tempDataLO & 0xFF;
      d += 4;
    }
    myFile.write(sdBuffer, 512);
    d = 0;
  }
  // Close the file:
  myFile.close();

#ifdef OLED
  clear();
  println_Msg(F("Calculating CRC..."));
  println_Msg(F(""));
  display_Update();
#endif

  uint32_t crc = calcCRC(fileName, cartSize);
  compareCRC("jag.txt", crc, 1);

#ifdef OLED
  display.setCursor(0, 56);
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
#endif
}

// Calculate CRC32 if needed and compare it to CRC read from database
boolean compareCRC(const char* database, uint32_t crc32sum, boolean renamerom) {
  char crcStr[9];
  // Convert precalculated crc to string
  sprintf(crcStr, "%08lX", crc32sum);

  //Search for CRC32 in file
  char gamename[96];
  char crc_search[9];
  boolean matchFound = false;

  //go to root
  sd.chdir();
  if (myFile.open(database, O_READ)) {
    //Search for same CRC in list
    while (myFile.available()) {
      //Read 2 lines (game name and CRC)
      get_line(gamename, &myFile, sizeof(gamename));
      get_line(crc_search, &myFile, sizeof(crc_search));
      skip_line(&myFile);  //Skip every 3rd line

      //if checksum search successful, rename the file and end search
      if (strcmp(crc_search, crcStr) == 0) {
        // Close the file:
        myFile.close();
        matchFound = true;
        break;
      }
    }
    if (matchFound) {
        print_Msg(F("Match Found -> "));
        display_Update();

        if (renamerom) {
          println_Msg(gamename);

          // Rename file to database name
          sd.chdir(folder);
          delay(100);
          if (myFile.open(fileName, O_READ)) {
            myFile.rename(gamename);
            // Close the file:
            myFile.close();
          }
        } else {
          println_Msg("OK");
        }
        return 1;
    } else {
      println_Msg(F("Match NOT Not found"));
      return 0;
    }
  } else {
    println_Msg(F(" -> Error Database missing"));
    return 0;
  }
}

//Skip line
void skip_line(File* readfile) {
  int i = 0;
  char str_buf;

  while (readfile->available()) {
    //Read 1 byte from file
    str_buf = readfile->read();

    //if end of file or newline found, execute command
    if (str_buf == '\r') {
      readfile->read();  //dispose \n because \r\n
      break;
    }
    i++;
  }  //End while
}

//Get line from file
void get_line(char* str_buf, File* readfile, uint8_t maxi) {
  int read_len;

  read_len = readfile->read(str_buf, maxi - 1);

  for (int i = 0; i < read_len; i++) {
    //if end of file or newline found, execute command
    if (str_buf[i] == '\r') {
      str_buf[i] = 0;
      readfile->seekCur(i - read_len + 2);  // +2 to skip over \n because \r\n
      return;
    }
  }
  str_buf[maxi - 1] = 0;
  // EOL was not found, keep looking (slower)
  while (readfile->available()) {
    if (readfile->read() == '\r') {
      readfile->read();  // read \n because \r\n
      break;
    }
  }
}

//*****************************************************************************
// MICROCHIP EEPROM 93C56/66/76/86 CODE
// SERIAL DATA OUTPUT (DO):  PIN PA5
//
// ISSI 93C46-3P EEPROM ONLY USES WORD MODE
// EEPROM CODE IS WRITTEN IN WORD MODE (16bit)
//
// 93C46 (16bit): A5..A0, EWEN/ERAL/EWDS XXXX
// 93C56 (16bit): A6..A0, EWEN/ERAL/EWDS XXXXXX
// 93C66 (16bit): A7..A0, EWEN/ERAL/EWDS XXXXXX
// 93C76 (16bit): A8..A0, EWEN/ERAL/EWDS XXXXXXXX
// 93C86 (16bit): A9..A0, EWEN/ERAL/EWDS XXXXXXXX
//
// MICROCHIP EEPROM - TIE PIN 6 (ORG) TO VCC (ORG = 1) TO ENABLE 16bit MODE
// FOR 93C76 & 93C86, TIE PIN 7 (PE) TO VCC TO ENABLE PROGRAMMING
//*****************************************************************************
void EepromClear() {
  DDRF |= (1 << 0); // DATA PIN PF0 AS OUTPUT
  DDRA &= ~(1 << 5); // DO INPUT
  EEP_CS_CLEAR;
  EEP_SK_CLEAR;
  EEP_DI_CLEAR;
}

void EepromReset() {
  DDRF &= ~(1 << 0); // DATA PIN PF0 AS INPUT
  EEP_CS_CLEAR;
  EEP_SK_CLEAR;
  EEP_DI_CLEAR;
}

void Eeprom0() {
  EEP_DI_CLEAR;
  EEP_SK_SET;
  _delay_us(1); // minimum 250ns
  EEP_DI_CLEAR;
  EEP_SK_CLEAR;
  _delay_us(1); // minimum 250ns
}

void Eeprom1() {
  EEP_DI_SET;
  EEP_SK_SET;
  _delay_us(1); // minimum 250ns
  EEP_DI_CLEAR;
  EEP_SK_CLEAR;
  _delay_us(1); // minimum 250ns
}

void EepromRead(uint16_t addr) {
  EepromClear();
  EEP_CS_SET;
  Eeprom1(); // 1
  Eeprom1(); // 1
  Eeprom0(); // 0
  if ((eepSize == 1) || (eepSize == 3))
    Eeprom0(); // Dummy 0 for 56/76
  EepromSetAddress(addr);
  _delay_us(12); // From Willem Timing
  // DATA OUTPUT
  EepromReadData();
  EEP_CS_CLEAR;
  // OR 16 bits into two bytes
  for (int j = 0; j < 16; j += 8) {
    EepBuf[(j / 8)] = EepBit[0 + j] << 7 | EepBit[1 + j] << 6 | EepBit[2 + j] << 5 | EepBit[3 + j] << 4 | EepBit[4 + j] << 3 | EepBit[5 + j] << 2 | EepBit[6 + j] << 1 | EepBit[7 + j];
  }
}

// Capture 2 bytes in 16 bits into bit array EepBit[]
void EepromReadData(void) {
  for (int i = 0; i < 16; i++) {
    EEP_SK_SET;
    _delay_us(1); // minimum 250ns
    EepBit[i] = ((PINA & 0x20) >> 5); // Read DO (PA5) - PINA with Mask 0x20
    EEP_SK_CLEAR;
    _delay_us(1); // minimum 250ns
  }
}

void EepromWrite(uint16_t addr) {
  EepromClear();
  EEP_CS_SET;
  Eeprom1(); // 1
  Eeprom0(); // 0
  Eeprom1(); // 1
  if ((eepSize == 1) || (eepSize == 3))
    Eeprom0(); // Dummy 0 for 56/76
  EepromSetAddress(addr);
  // DATA OUTPUT
  EepromWriteData();
  EEP_CS_CLEAR;
  EepromStatus();
}

void EepromWriteData(void) {
  byte UPPER = EepBuf[1];
  byte LOWER = EepBuf[0];
  for (int i = 0; i < 8; i++) {
    if (((UPPER >> 7) & 0x1) == 1) { // Bit is HIGH
      Eeprom1();
    }
    else { // Bit is LOW
      Eeprom0();
    }
    // rotate to the next bit
    UPPER <<= 1;
  }
  for (int j = 0; j < 8; j++) {
    if (((LOWER >> 7) & 0x1) == 1) { // Bit is HIGH
      Eeprom1();
    }
    else { // Bit is LOW
      Eeprom0();
    }
    // rotate to the next bit
    LOWER <<= 1;
  }
}

void EepromSetAddress(uint16_t addr) { // 16bit
  uint8_t shiftaddr = eepSize + 6; // 93C46 = 0 + 6, 93C56 = 7, 93C66 = 8, 93C76 = 9, 93C86 = 10
  for (int i = 0; i < shiftaddr; i++) {
    if (((addr >> shiftaddr) & 1) == 1) { // Bit is HIGH
      Eeprom1();
    }
    else { // Bit is LOW
      Eeprom0();
    }
    // rotate to the next bit
    addr <<= 1;
  }
}

// EWEN/ERAL/EWDS
// 93C56/93C66 - 10000xxxxxx (6 PULSES)
// 93C76/93C86 - 10000xxxxxxxx (8 PULSES)
void EepromEWEN(void) { // EWEN 10011xxxx
  EepromClear();
  EEP_CS_SET;
  Eeprom1(); // 1
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom1(); // 1
  Eeprom1(); // 1
  // 46 = 4x Trailing 0s for 16bit
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  // 56/66 = 6x Trailing 0s for 16bit
  if (eepSize > 0) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  // 76/86 = 8x Trailing 0s for 16bit
  if (eepSize > 2) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  EEP_CS_CLEAR;
  _delay_us(2);
  Serial.println(F("ERASE ENABLED"));
}

void EepromERAL(void) { // ERASE ALL 10010xxxx
  EEP_CS_SET;
  Eeprom1(); // 1
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom1(); // 1
  Eeprom0(); // 0
  // 46 = 4x Trailing 0s for 16bit
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  // 56/66 = 6x Trailing 0s for 16bit
  if (eepSize > 0) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  // 76/86 = 8x Trailing 0s for 16bit
  if (eepSize > 2) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  EEP_CS_CLEAR;
  EepromStatus();
  Serial.println(F("ERASED ALL"));
}

void EepromEWDS(void) { // DISABLE 10000xxxx
  EepromClear();
  EEP_CS_SET;
  Eeprom1(); // 1
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  // 46 = 4x Trailing 0s for 16bit
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  Eeprom0(); // 0
  // 56/66 = 6x Trailing 0s for 16bit
  if (eepSize > 0) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  // 76/86 = 8x Trailing 0s for 16bit
  if (eepSize > 2) {
    Eeprom0(); // 0
    Eeprom0(); // 0
  }
  EEP_CS_CLEAR;
  _delay_us(2);
  Serial.println(F("ERASE DISABLED"));
}

void EepromStatus(void) {// CHECK READY/BUSY
  __asm__("nop\n\t""nop\n\t"); // CS LOW for minimum 100ns
  EEP_CS_SET;
  boolean status = ((PINA & 0x20) >> 5); // Check DO
  do {
    _delay_ms(1);
    status = ((PINA & 0x20) >> 5);
  }
  while (!status); // status == 0 = BUSY
  EEP_CS_CLEAR;
}

void EepromDisplay(){ // FOR SERIAL ONLY
  word eepEnd = int_pow(2, eepSize) * 128;
  for (word address = 0; address < eepEnd; address += 2) {
    EepromRead(address);
    if ((address % 16 == 0) && (address != 0))
      Serial.println(F(""));
    if (EepBuf[1] < 0x10)
      Serial.print(F("0"));
    Serial.print(EepBuf[1], HEX);
    Serial.print(F(" "));
    if (EepBuf[0] < 0x10)
      Serial.print(F("0"));
    Serial.print(EepBuf[0], HEX);
    Serial.print(F(" "));
  }
  Serial.println(F(""));
  Serial.println(F(""));
}

//*****************************************************************************
// EEPROM
// (0) 93C46 128B STANDARD 
// (1) 93C56 256B AFTERMARKET
// (2) 93C66 512B AFTERMARKET
// (3) 93C76 1024B AFTERMARKET
// (4) 93C86 2048B AFTERMARKET - Battlesphere Gold
//*****************************************************************************
// Read EEPROM and save to the SD card
void readEEP() {
  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".eep");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sd.chdir();
//  sprintf(folder, "JAG/SAVE/%s/%d", romName, foldern);
  sprintf(folder, "JAG/SAVE/%d", foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  println_Msg(F("Reading..."));
  display_Update();

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  word eepEnd = int_pow(2, eepSize) * 64; // WORDS
  if (eepSize > 1) { // 93C66/93C76/93C86 - 256/512/1024 WORDS
    for (word currWord = 0; currWord < eepEnd; currWord += 256) {
      for (int i = 0; i < 256; i++) {
        EepromRead((currWord + i) * 2);
        sdBuffer[(i * 2)] = EepBuf[1];
        sdBuffer[(i * 2) + 1] = EepBuf[0];
      }
      myFile.write(sdBuffer, 512);
    }
  }
  else { // 93C46/93C56 - 64/128 WORDS
    for (word currWord = 0; currWord < eepEnd; currWord++) {
      EepromRead(currWord * 2);
      sdBuffer[(currWord * 2)] = EepBuf[1];
      sdBuffer[(currWord * 2) + 1] = EepBuf[0];
    }
    myFile.write(sdBuffer, eepEnd * 2);
  }
  EepromReset();
  // Close the file:
  myFile.close();
  println_Msg(F(""));
  print_Msg(F("Saved to "));
  println_Msg(folder);
  display_Update();
}

// NOTE:  TO WRITE TO 93C76 & 93C86, MUST TIE PE (PROGRAM ENABLE) PIN 7 TO VCC
void writeEEP() {
  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  println_Msg(F("Writing..."));
  println_Msg(filePath);
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    EepromEWEN(); // ERASE/WRITE ENABLE
    EepromERAL(); // ERASE ALL
    Serial.println(F("WRITING"));
    word eepEnd = int_pow(2, eepSize) * 64; // WORDS
    if (eepSize > 1) { // 93C66/93C76/93C86
      for (word currWord = 0; currWord < eepEnd; currWord += 256) {
        myFile.read(sdBuffer, 512);
        for (int i = 0; i < 256; i++) {
          EepBuf[0] = sdBuffer[(i * 2)];
          EepBuf[1] = sdBuffer[(i * 2) + 1];
          EepromWrite((currWord + i) * 2);
        }
      }
    }
    else { // 93C46/93C56
      myFile.read(sdBuffer, eepEnd * 2);
      for (word currWord = 0; currWord < eepEnd; currWord++) {
        EepBuf[0] = sdBuffer[currWord * 2];
        EepBuf[1] = sdBuffer[(currWord * 2) + 1];
        EepromWrite(currWord * 2);
      }
    }
    EepromEWDS(); // ERASE/WRITE DISABLE
    EepromReset();
    // Close the file:
    myFile.close();
    println_Msg(F(""));
    println_Msg(F("DONE"));
    display_Update();
  }
  else {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
}

//*****************************************************************************
// MEMORY TRACK NVRAM FLASH
//*****************************************************************************
void readID() { // AT29C010 Flash ID "1FD5"
  // Switch to write
  dataOut();

  // ID command sequence
  writeBYTE_FLASH(0x15554, 0xAA); // 0x5555
  writeBYTE_FLASH(0xAAA8, 0x55);  // 0x2AAA
  writeBYTE_FLASH(0x15554, 0x90); // 0x5555

  // Switch to read
  dataIn();

  // Read the two id bytes into a string
  sprintf(flashID, "%02X%02X", readBYTE_FLASH(0x0000), readBYTE_FLASH(0x0004));

  resetFLASH();
}

void eraseFLASH() { // Chip Erase (NOT NEEDED FOR WRITES)
  // Switch to write
  dataOut();

  println_Msg(F("Erasing..."));
  display_Update();

  // Erase command sequence
  writeBYTE_FLASH(0x15554, 0xAA); // 0x5555
  writeBYTE_FLASH(0xAAA8, 0x55);  // 0x2AAA
  writeBYTE_FLASH(0x15554, 0x80); // 0x5555
  writeBYTE_FLASH(0x15554, 0xAA); // 0x5555
  writeBYTE_FLASH(0xAAA8, 0x55);  // 0x2AAA
  writeBYTE_FLASH(0x15554, 0x10); // 0x5555

  // Wait for command to complete
  busyCheck();

  // Switch to read
  dataIn();
}

void resetFLASH() {
  // Switch to write
  dataOut();

  // Reset command sequence
  writeBYTE_FLASH(0x15554, 0xAA); // 0x5555
  writeBYTE_FLASH(0xAAA8, 0x55);  // 0x2AAA
  writeBYTE_FLASH(0x15554, 0xF0); // 0x5555

  // Switch to read
  dataIn();
  delayMicroseconds(10);
}

void busyCheck() {
  // Switch to read
  dataIn();

  // Read register
  readBYTE_FLASH(0x0000);

  // CE or OE must be toggled with each subsequent status read or the
  // completion of a program or erase operation will not be evident.
  while (((PINL >> 6) & 0x1) == 0) { // IO6 = PORTL PL6
    // Setting CE(PH5) HIGH
    PORTH |= (1 << 5);

    // Leave CE high for at least 60ns
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

    // Setting CE(PH5) LOW
    PORTH &= ~(1 << 5);

    // Leave CE low for at least 50ns
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

    // Read register
    readBYTE_FLASH(0x0000);
  }

  // Switch to write
  dataOut();
}

byte readBYTE_FLASH(unsigned long myAddress) {
  SRCLR_CLEAR;
  SRCLR_SET;
  LATCH_CLEAR;
  shiftOutFAST((myAddress >> 16) & 0xFF);
  shiftOutFAST((myAddress >> 8) & 0xFF);
  shiftOutFAST(myAddress);
  LATCH_SET;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Setting CE(PH5) LOW
  PORTH &= ~(1 << 5);
  // Setting OEH(PH3) HIGH
  PORTH |= (1 << 3);
  // Setting OEL(PH4) LOW
  PORTH &= ~(1 << 4);

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Read
  byte tempByte = PINL; // D16..D23

  // Setting CE(PH5) HIGH
  PORTH |= (1 << 5);
  // Setting OEL(PH4) HIGH
  PORTH |= (1 << 4);

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  return tempByte;
}

byte readBYTE_MEMROM(unsigned long myAddress) {
  SRCLR_CLEAR;
  SRCLR_SET;
  LATCH_CLEAR;
  shiftOutFAST((myAddress >> 16) & 0xFF);
  shiftOutFAST((myAddress >> 8) & 0xFF);
  shiftOutFAST(myAddress);
  LATCH_SET;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Setting CE(PH5) LOW
  PORTH &= ~(1 << 5);
  // Setting OEH(PH3) LOW
  PORTH &= ~(1 << 3);

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Read
  byte tempByte = PINF; // D0..D7

  // Setting CE(PH5) HIGH
  PORTH |= (1 << 5);
  // Setting OEH(PH3) HIGH
  PORTH |= (1 << 3);

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  return tempByte;
}

void writeBYTE_FLASH(unsigned long myAddress, byte myData) {
  SRCLR_CLEAR;
  SRCLR_SET;
  LATCH_CLEAR;
  shiftOutFAST((myAddress >> 16) & 0xFF);
  shiftOutFAST((myAddress >> 8) & 0xFF);
  shiftOutFAST(myAddress);
  LATCH_SET;

  PORTL = myData;

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Setting OEL(PH4) HIGH
  PORTH |= (1 << 4);
  // Setting CE(PH5) LOW
  PORTH &= ~(1 << 5);
  // Switch WE(PH6) LOW
  PORTH &= ~(1 << 6);
  delayMicroseconds(10);

  // Switch WE(PH6) HIGH
  PORTH |= (1 << 6);
  // Setting CE(PH5) HIGH
  PORTH |= (1 << 5);
  delayMicroseconds(10);
}

void writeSECTOR_FLASH(unsigned long myAddress) {
  dataOut();

  // Enable command sequence
  writeBYTE_FLASH(0x15554, 0xAA); // 0x5555
  writeBYTE_FLASH(0xAAA8, 0x55);  // 0x2AAA
  writeBYTE_FLASH(0x15554, 0xA0); // 0x5555

  for (int i = 0; i < 128; i++) {
    SRCLR_CLEAR;
    SRCLR_SET;
    LATCH_CLEAR;
    shiftOutFAST((((myAddress + i) * 4) >> 16) & 0xFF);
    shiftOutFAST((((myAddress + i) * 4) >> 8) & 0xFF);
    shiftOutFAST((myAddress + i) * 4); // (myAddress + i) * 4
    LATCH_SET;
  
    PORTL = sdBuffer[i];
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

    // Setting OEL(PH4) HIGH
//    PORTH |= (1 << 4);
    // Setting CE(PH5) LOW
    PORTH &= ~(1 << 5);
    // Switch WE(PH6) LOW
    PORTH &= ~(1 << 6);
    __asm__("nop\n\t""nop\n\t"); // Minimum 90ns
  
    // Switch WE(PH6) HIGH
    PORTH |= (1 << 6);
    // Setting CE(PH5) HIGH
    PORTH |= (1 << 5);

    __asm__("nop\n\t""nop\n\t"); // Minimum 100ns
  }
  delay(30);
}

//*****************************************************************************
// MEMORY TRACK ROM U1
// U1 ADDR PINS A0..A18 [USES A0..A17 = 0x20000 BYTES]
// U1 DATA PINS D0..D7
// /CE ON PIN 20A (CONTROLS BOTH U1 AND U2)
// /OEH ON PIN 23B (CONTROLS U1 ROM ONLY)
//*****************************************************************************
void readMEMORY() {
  // Set control
  dataIn();
  
  strcpy(fileName, romName);
  strcat(fileName, ".J64");

  // create a new folder
  sd.chdir();
  EEPROM_readAnything(0, foldern);
//  sprintf(folder, "JAG/ROM/%s/%d", romName, foldern);
  sprintf(folder, "JAG/ROM/%d", foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  clear();
  print_Msg(F("Saving ROM to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  // Memory Track ROM Size 0x20000
  for (unsigned long currBuffer = 0; currBuffer < cartSize; currBuffer += 512) {
    // Blink led
    if (currBuffer % 16384 == 0)
      PORTB = PORTB ^ 0xC0; // B11000000 = RED + GRN = YELLOW
    for (int currByte = 0; currByte < 512; currByte++) {
      sdBuffer[currByte] = readBYTE_MEMROM(currBuffer + currByte);
    }
    myFile.write(sdBuffer, 512);
  }
  // Close the file:
  myFile.close();

#ifdef OLED
  clear();
  println_Msg(F("Calculating CRC..."));
  println_Msg(F(""));
  display_Update();
#endif

  calcCRC(fileName, cartSize); // 0x20000

#ifdef OLED
  display.setCursor(0, 56);
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
#endif
}

//*****************************************************************************
// MEMORY TRACK FLASH U2
// AT29C010 = 0x20000 BYTES = 128 KB
// U2 ADDR PINS A2..A18
// U2 DATA PINS D16..D23
// /CE ON PIN 20A (CONTROLS BOTH U1 AND U2)
// /OEL ON PIN 22B (CONTROLS U2 FLASH ONLY)
//*****************************************************************************
void readFLASH() {
  dataIn();
  strcpy(fileName, romName);
  strcat(fileName, ".fla");

  // create a new folder for the save file
  sd.chdir();
  EEPROM_readAnything(0, foldern);
//  sprintf(folder, "JAG/SAVE/%s/%d", romName, foldern);
  sprintf(folder, "JAG/SAVE/%d", foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  clear();
  print_Msg(F("Saving FLASH to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  for (unsigned long currByte = 0; currByte < flaSize; currByte += 512) {
    // Blink led
    if (currByte % 16384 == 0)
      PORTB = PORTB ^ 0xC0; // B11000000 = RED + GRN = YELLOW
    for (int i = 0; i < 512; i++) {
      sdBuffer[i] = readBYTE_FLASH((currByte + i) * 4); // Address Shift A2..A18
    }
    myFile.write(sdBuffer, 512);
  }
  // Close the file:
  myFile.close();
  print_Msg(F("Saved to "));
  print_Msg(folder);
  println_Msg(F("/"));
  display_Update();

#ifdef OLED
  clear();
  println_Msg(F("Calculating CRC..."));
  println_Msg(F(""));
  display_Update();
#endif

  calcCRC(fileName, flaSize); // 0x20000

#ifdef OLED
  display.setCursor(0, 56);
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
#endif
}

// AT29C010 - Write in 128 BYTE Secrors
// Write Enable then Write DATA
void writeFLASH() {
  dataOut();

  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  println_Msg(F("Writing..."));
  println_Msg(filePath);
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    for (unsigned long currByte = 0; currByte < flaSize; currByte += 128) {
      // Blink led
      if (currByte % 16384 == 0)
        PORTB = PORTB ^ 0xC0; // B11000000 = RED + GRN = YELLOW
      // Load Data
      for (int i = 0; i < 128; i++) {
        sdBuffer[i] = myFile.read() & 0xFF;
      }
      writeSECTOR_FLASH(currByte);
    }
    // Close the file:
    myFile.close();
    println_Msg(F("WRITE COMPLETE"));
    display_Update();
  }
  else {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  dataIn();
}

unsigned long verifyFLASH() {
  dataIn();
  writeErrors = 0;

  println_Msg(F(""));
  println_Msg(F("Verifying..."));
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    for (unsigned long currByte = 0; currByte < flaSize; currByte += 512) {
      for (int i = 0; i < 512; i++) {
        sdBuffer[i] = readBYTE_FLASH((currByte + i) * 4);
      }
      // Check sdBuffer content against file on sd card
      for (int j = 0; j < 512; j++) {
        if (myFile.read() != sdBuffer[j]) {
          writeErrors++;
        }
      }
    }
    // Close the file:
    myFile.close();
  }
  else {
    println_Msg(F("SD ERROR"));
#ifdef OLED
    display.setCursor(0, 56);
#endif    
    println_Msg(F("Press Button to Reset"));
    display_Update();
    wait();
    reset();
  }
  if (!writeErrors)
    println_Msg(F("WRITE VERIFIED"));
  else
    println_Msg(F("WRITE ERROR"));
  display_Update();
#ifdef OLED
  display.setCursor(0, 56);
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
#endif
  // Return 0 if verified ok, or number of errors
  return writeErrors;
}

//******************************************************************************
// PRINT
//******************************************************************************
void print_Msg(const __FlashStringHelper *string) {
#ifdef OLED
  display.print(string);
#else
  Serial.print(string);
#endif
}

void print_Msg(const char string[]) {
#ifdef OLED
  display.print(string);
#else
  Serial.print(string);
#endif
}

void print_Msg(long unsigned int message) {
#ifdef OLED
  display.print(message);
#else
  Serial.print(message);
#endif
}

void print_Msg(byte message, int outputFormat) {
#ifdef OLED
  display.print(message, outputFormat);
#else
  Serial.print(message, outputFormat);
#endif
}

void print_Msg(String string) {
#ifdef OLED
  display.print(string);
#else
  Serial.print(string);
#endif
}

void println_Msg(String string) {
#ifdef OLED
  display.println(string);
#else
  Serial.println(string);
#endif
}

void println_Msg(byte message, int outputFormat) {
#ifdef OLED
  display.println(message, outputFormat);
#else
  Serial.println(message, outputFormat);
#endif
}

void println_Msg(const char message[]) {
#ifdef OLED
  display.println(message);
#else
  Serial.println(message);
#endif
}

void println_Msg(const __FlashStringHelper *string) {
#ifdef OLED
  display.println(string);
#else
  Serial.println(string);
#endif
}

void println_Msg(long unsigned int message) {
#ifdef OLED
  display.println(message);
#else
  Serial.println(message);
#endif
}

void display_Update() {
#ifdef OLED
  display.display();
#else
  Serial.println(F(""));
  delay(100);
#endif
}

void clear() {
  display.clearDisplay();
  display.setCursor(0,0);
}

//******************************************************************************
// WRITE CONFIRM
//******************************************************************************
void writeConfirm() {
  clear();
  println_Msg(F("***** WARNING ******"));
  println_Msg(F("* YOU ARE ABOUT TO *"));
  println_Msg(F("* ERASE SAVE DATA! *"));
  println_Msg(F("********************"));
  println_Msg(F(""));
  println_Msg(F("CONTINUE?"));
#ifdef OLED
  println_Msg(F(" NO"));
  println_Msg(F(" YES"));
  display.display();
  // draw selection box
  choice = 0;
  LED_RED_ON;
  display.drawPixel(0, 8 * choice + 52, WHITE);
  display.display();
  b = 0;
  while (1) {
    b = checkButton();
    if ((b == doubleclick)||(b == press)) {
      // remove selection box
      display.drawPixel(0, 8 * choice + 52, BLACK);
      display.display();
      if (choice == 0) {
        LED_RED_OFF;
        LED_GREEN_ON;
        choice = 1;
      }
      else {
        LED_GREEN_OFF;
        LED_RED_ON;
        choice = 0;
      }
      // draw selection box
      display.drawPixel(0, 8 * choice + 52, WHITE);
      display.display();
    }
    if (b == hold) // Long Press - Execute
      break;
  }
  clear();
  LED_RED_OFF;
  LED_GREEN_OFF;
  if (choice == 1)
    return;
  else {
    println_Msg(F("Press Button to Reset"));
    display.display();
    wait();
    reset();
  }
#else
  println_Msg(F("0 = NO"));
  println_Msg(F("1 = YES"));
  println_Msg(F(""));
  print_Msg("Enter Command: ");
  while (Serial.available() == 0) {}
  serialinput = Serial.read();
  switch (serialinput) {
    case 0x30: //0
      println_Msg(F("0 = NO"));
      println_Msg(F(""));
      choice = 0;
      break;

    case 0x31: //1
      println_Msg(F("1 = YES"));
      println_Msg(F(""));
      choice = 1;
      break;
  }
#endif
}

//******************************************************************************
// BROWSER
//******************************************************************************
unsigned char browser_box(char* question, char answers[7][20], int num_answers, int default_choice) {
  clear();
  // print menu
  println_Msg(question);
  for (unsigned char i = 0; i < num_answers; i++) {
    // Add space for the selection dot
    print_Msg(" ");
    // Print menu item
    println_Msg(answers[i]);
  }
  display.display();
  // start with the default choice
  choice = default_choice;
  // draw selection box
  display.drawPixel(0, 8 * choice + 12, WHITE);
  display.display();
  // wait until user makes his choice
  while (1) {
    b = checkButton();
    if (b == doubleclick) { // Previous
      // remove selection box
      display.drawPixel(0, 8 * choice + 12, BLACK);
      display.display();
      if (choice == 0) {
        if (currPage > 1) {
          lastPage = currPage;
          currPage--;
          break;
        }
        else {
          root = 1;
          break;
        }
      }
      else
        choice--;
      // draw selection box
      display.drawPixel(0, 8 * choice + 12, WHITE);
      display.display();
    }
    if (b == press) { // Next
      // remove selection box
      display.drawPixel(0, 8 * choice + 12, BLACK);
      display.display();
      if ((choice == num_answers - 1 ) && (numPage > currPage)) {
        lastPage = currPage;
        currPage++;
        break;
      }
      else if ((choice == num_answers - 1 ) && (numPage > 1) && (numPage == currPage)) {
        lastPage = currPage;
        currPage = 1;
        break;
      }
      else
        choice = (choice + 1) % num_answers;
      // draw selection box
      display.drawPixel(0, 8 * choice + 12, WHITE);
      display.display();
    }
    if (b == hold) // Execute
      break;
  }
  return choice;
}

unsigned char menu_box(const char* selection, char menu_answers[8][20], int menu_count, int menu_default) {
  clear();
  // print menu
  println_Msg(selection);
  for (unsigned char i = 0; i < menu_count; i++) {
    // Add space for the selection dot
    print_Msg(" ");
    // Print menu item
    println_Msg(menu_answers[i]);
  }
  display.display();
  // start with the default choice
  menu_choice = menu_default;
  // draw selection box
  display.drawPixel(0, 8 * menu_choice + 12, WHITE);
  display.display();
  // wait until user makes his choice
  while (1) {
    b = checkButton();
    if (b == doubleclick) { // Previous
      // remove selection box
      display.drawPixel(0, 8 * menu_choice + 12, BLACK);
      display.display();
      if (menu_choice == 0)
        menu_choice = menu_count - 1;
      else
        menu_choice--;
      // draw selection box
      display.drawPixel(0, 8 * menu_choice + 12, WHITE);
      display.display();
    }
    if (b == press) { // Next
      // remove selection box
      display.drawPixel(0, 8 * menu_choice + 12, BLACK);
      display.display();
      menu_choice = (menu_choice + 1) % menu_count;
      // draw selection box
      display.drawPixel(0, 8 * menu_choice + 12, WHITE);
      display.display();
    }
    if (b == hold) // Execute
      break;
  }
  return menu_choice;
}

//******************************************************************************
// SELECT FILE
//******************************************************************************
void SelectFileInSD() {
  char fileNames[30][26];
  sd.chdir();
  filePath[0] = '\0'; // Clear filePath
#ifdef OLED
  int currFile;
  // Temporary char array for filename
  char nameStr[26];
browserstart:
  // Set currFile back to 0
  currFile = 0;
  currPage = 1;
  lastPage = 1;
  // Read in File as long as there are files
  while (myFile.openNext(sd.vwd(), O_READ)) {
    myFile.getName(nameStr, 27); // Get name of file
    if (myFile.isHidden()) { // Ignore if hidden
    }
    // Indicate a directory.
    else if (myFile.isDir()) {
      // Copy full dirname into fileNames
      sprintf(fileNames[currFile], "%s%s", "/", nameStr);
      // Truncate to 19 letters for LCD
      nameStr[19] = '\0';
      // Copy short string into fileOptions
      sprintf(fileOptions[currFile], "%s%s", "/", nameStr);
      currFile++;
    }
    // It's just a file
    else if (myFile.isFile()) {
      // Copy full filename into fileNames
      sprintf(fileNames[currFile], "%s", nameStr);
      // Truncate to 19 letters for LCD
      nameStr[19] = '\0';
      // Copy short string into fileOptions
      sprintf(fileOptions[currFile], "%s", nameStr);
      currFile++;
    }
    myFile.close();
  }
  // "Calculate number of needed pages"
  if (currFile < 8)
    numPage = 1;
  else if (currFile < 15)
    numPage = 2;
  else if (currFile < 22)
    numPage = 3;
  else if (currFile < 29)
    numPage = 4;
  else if (currFile < 36)
    numPage = 5;
  // Fill the array "answers" with 7 options to choose from in the file browser
  char answers[7][20];
page:
  // If there are less than 7 entries, set count
  // to that number so no empty options appear
  byte count;
  if (currFile < 8)
    count = currFile;
  else if (currPage == 1)
    count = 7;
  else if (currFile < 15)
    count = currFile - 7;
  else if (currPage == 2)
    count = 7;
  else if (currFile < 22)
    count = currFile - 14;
  else if (currPage == 3)
    count = 7;
  else if (currFile < 29)
    count = currFile - 21;
  else {
    clear();
    display.println(F("TOO MANY FILES"));
    display.display();
    wait();
    reset();
  }
  for (byte i = 0; i < 8; i++ ) {
    // Copy short string into fileOptions
    sprintf( answers[i], "%s", fileOptions[ ((currPage - 1) * 7 + i)] );
  }
  // Create menu with title "SD FILE BROWSER" and 1-7 options to choose from
  unsigned char answer = browser_box("   SD FILE BROWSER", answers, count, 0);
  // Check if the page has been switched
  if (currPage != lastPage) {
    lastPage = currPage;
    goto page;
  }
  // Check if we are supposed to go back to the root dir
  if (root) {
    // Empty filePath string
    filePath[0] = '\0';
    //sd.vwd()->rewind(); // Rewind filesystem
    sd.chdir(); // root
    root = 0; // Start again
    goto browserstart;
  }
  // wait for user choice to come back from the question box menu
  switch (answer) {
    case 0:
      strcpy(fileName, fileNames[0 + ((currPage - 1) * 7)]);
      break;

    case 1:
      strcpy(fileName, fileNames[1 + ((currPage - 1) * 7)]);
      break;

    case 2:
      strcpy(fileName, fileNames[2 + ((currPage - 1) * 7)]);
      break;

    case 3:
      strcpy(fileName, fileNames[3 + ((currPage - 1) * 7)]);
      break;

    case 4:
      strcpy(fileName, fileNames[4 + ((currPage - 1) * 7)]);
      break;

    case 5:
      strcpy(fileName, fileNames[5 + ((currPage - 1) * 7)]);
      break;

    case 6:
      strcpy(fileName, fileNames[6 + ((currPage - 1) * 7)]);
      break;
  }
  // Add directory to our filepath if we just entered a new directory
  if (fileName[0] == '/') {
    strcat(filePath, fileName); // add dirname to path
    char* dirName = fileName + 1; // Remove '/' from dir name
    sd.chdir(dirName); // Change working dir
    goto browserstart; // Restart in new dir
  }
  else {
    sd.chdir(); // root
  }
#else
browserstart:
  Serial.print(F("SD PATH: "));
  Serial.println(filePath);
  sd.vwd()->rewind();
  while (myFile.openNext(sd.vwd(), O_READ)) {
    if (myFile.isHidden()) {
    }
    else {
      if (myFile.isDir()) {
        Serial.print(F("[DIR] ")); // Directory
      }
      myFile.printName(&Serial);
      Serial.println(F(""));
    }
    myFile.close();
  }
  Serial.println(F(""));
  Serial.println(F("Type '..' to return to root"));
  Serial.print(F("Enter Filename: "));
  while (Serial.available() == 0) {}
  String strBuffer;
  strBuffer = Serial.readString();
  strBuffer.toCharArray(fileName, 26);
  Serial.println(fileName);
  Serial.println(F(""));
  char sdroot[] = "..";
  if (sd.exists(fileName)) {
    myFile = (sd.open(fileName));
    if (myFile.isDir()) {
      strcat(filePath, "/");
      strcat(filePath, fileName);
      sd.chdir(fileName); // Directory
      myFile.close();
      goto browserstart;
    }
    myFile.close();
  }
  else if (strcmp(fileName, sdroot) == 0) {
    sd.chdir();
    filePath[0] = '\0';
    goto browserstart;
  }
  else {
    Serial.println(F("FILE NOT FOUND!"));
    Serial.println(F(""));
    goto browserstart;
  }
  sd.chdir();
#endif
}

//******************************************************************************
// RESET
//******************************************************************************
void reset() {
  asm volatile ("  jmp 0");
}

//******************************************************************************
// LOOP
//******************************************************************************
void loop() {
  menu();
}
