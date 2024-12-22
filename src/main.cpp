#include <Arduino.h>
#include <SPI.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ezButton.h>
#include <SolarCalculator.h>
#include <EEPROM.h>

// MODES
#define MANUAL 0
#define AUTOMATIC 1

// SOLAR CALCULATOR VARIABLES
double latitude = -34.0195173;     // Observer's latitude 
double longitude = 22.779688;  // Observer's longitude
int time_zone = 2;          // UTC offset
int minSunElevation = -2;    // Minimum sun elevation for sunrise/sunset
double sunAzimuth, sunElevation;
double transit, sunrise, sunset;
 
// BUTTON VARIABLES
const int SHORT_PRESS_TIME = 1000; // 1000 milliseconds
const int LONG_PRESS_TIME  = 1000; // 1000 milliseconds

const byte buttonPin = 7;
ezButton button(buttonPin);  // create ezButton object that attach to pin 7;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

// DISPLAY VARIABLES
// Set the LCD address to 0x3F for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// CLOCK VARIABLES
RTC_DS3231 RTC;
DateTime now;// = RTC.now();

/**
 * MOSFET CONNECTION:
 * GDS
 * Gate: => gatePin
 * Drain: => LED (negative)
 * Source: => GND
 */

/**
 * RTC CONNECTION:
 * SDA: => A4
 * SCL: => A5
 * SQW: => 2 - not used
 * VCC: => 5V
 * GND: => GND
 */


/**
 * NEXT:
 * we need to store and retreive the last mode chosen by the user
 * modes include: manual (no sunrise/sunset), automatic (sunrise/sunset)
 * inside of automatic mode, the user can select the number of hours for fully lit
 */

/**
 * This code is for the LED light that will be used to simulate the sunrise and sunset.
*/
const byte ledPin = LED_BUILTIN;// for indicating modes
const byte gatePin = 5;
// const byte interruptPin = 2;// RTC interrupt pin
volatile byte brightness = 0;// pwm value to be used in the ISR
byte mode = 1;// 0 = manual, 1 = automatic
byte manualBrightness = 127;// brightness value for manual mode
const int eepromAddress = 0;// address to store the mode

// INTERNAL DECLARATIONS
char * hoursToString(double h, char *str);
void debugTime(DateTime now);
void handleButtonPress();
void changeManualBrightness();
void writeManualBrightness(byte newBrightness);
void processBrightness();
void toggleMode();
void writeMode(byte newMode);
void displayMode();
void lcdTopLine(const char *data);
void lcdBottomLine(const char *data);
void processLCD();

void setup() {
  // SERIAL
  Serial.begin(115200); //Starts serial connection

  // EEPROM
  EEPROM.get(eepromAddress, mode);// get the mode from the EEPROM
  if(mode != MANUAL && mode != AUTOMATIC) {
    mode = AUTOMATIC;// default to automatic mode
  }
  displayMode();

  EEPROM.get(eepromAddress + 1, manualBrightness);// get the manual brightness from the EEPROM
  if(manualBrightness < 0 || manualBrightness > 255) {
    manualBrightness = 127;// default to 50% brightness
  }
  Serial.println("Manual Brightness: " + String(manualBrightness));

  // BUTTON
  button.setDebounceTime(50); // set debounce time to 50 milliseconds - not required when using a 'delay' in the loop

  // PINS
  pinMode(gatePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
  // attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, RISING);

  // CLOCK
  RTC.begin();
  if (RTC.lostPower()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  // RTC.writeSqwPinMode(DS3231_SquareWave1Hz);
  now = RTC.now();
  debugTime(now);

  lcd.init();                      // initialize the lcd
  lcd.backlight();                 // turn on backlight
  lcdTopLine("AutomaLight");
}

unsigned long lastBrightnessUpdate = 0;
unsigned long millisBetweenBrightnessUpdates = 500;// 1/2 second

unsigned long lastLCDUpdate = 0;
unsigned long millisBetweenLCDUpdates = 1500;// 1-1/2 second
// setup an interrupt to update the brightness of the LED based on the time of day
void loop() {
  // Serial.println("Looping...");
  // BUTTON CODE
  button.loop(); // MUST call the loop() function first
  if(button.isPressed())
    pressedTime = millis();
  if(button.isReleased()) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if( pressDuration < SHORT_PRESS_TIME ) {
      changeManualBrightness();
      lastBrightnessUpdate = 0;// force an update of the brightness
    }
    if( pressDuration > LONG_PRESS_TIME ) {
      toggleMode();
      lastBrightnessUpdate = 0;// force an update of the brightness
    }
  }

  // BRIGHTNESS UPDATE
  if(millis() - lastBrightnessUpdate > millisBetweenBrightnessUpdates) {
    lastBrightnessUpdate = millis();
    processBrightness();
  }

  // LCD UPDATE
  if(millis() - lastLCDUpdate > millisBetweenLCDUpdates) {
    lastLCDUpdate = millis();
    processLCD();
  }
  // delay(10);
}

void toggleMode() {
  if(mode == 0) {
    writeMode(1);
  } else {
    writeMode(0);
  }
  displayMode();
}

void writeMode(byte newMode) {
  mode = newMode;
  EEPROM.put(eepromAddress, mode);// save the mode to the EEPROM
}

/**
 * Indicates MODE via Built-in LED
 * On is MANUAL
 * Off is AUTOMATIC
 */
void displayMode() {
  if(mode == MANUAL) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  Serial.println("Mode: " + String(mode));
}

void changeManualBrightness() {
  manualBrightness += 10;
  if(manualBrightness > 255) {
    manualBrightness = 0;
  }
  writeManualBrightness(manualBrightness);
}

void writeManualBrightness(byte newBrightness) {
  manualBrightness = newBrightness;
  EEPROM.put(eepromAddress + 1, manualBrightness);// save the manual brightness to the EEPROM
  Serial.println("Manual Brightness: " + String(manualBrightness));
}

void processBrightness() {
  if(mode == MANUAL) {
    analogWrite(gatePin, manualBrightness);
    Serial.println("Manual Mode: " + String(manualBrightness));
  } else {
    now = RTC.now();
    calcHorizontalCoordinates(now.unixtime() - time_zone * 3600L, latitude, longitude, sunAzimuth, sunElevation);// remove timezone offset
    // Print results
    Serial.print(F("Az: "));
    Serial.print(sunAzimuth);
    Serial.print(F("°  El: "));
    Serial.print(sunElevation);
    Serial.println(F("°"));
    analogWrite(gatePin, map(min(90, max(minSunElevation, sunElevation)), minSunElevation, 90, 0, 255));
  }
}

void processLCD() {
  // lcd.clear();
  // lcdTopLine("AutomaLight");
  if(mode == MANUAL) {
    char data[16];
    sprintf(data, "Manual: %d%%", (int)map(manualBrightness, 0, 255, 0, 100));
    lcdBottomLine(data);
  } else {
    char data[16];
    sprintf(data, "Sun: %d%%", (int)map(min(90, max(minSunElevation, sunElevation)), minSunElevation, 90, 0, 100));
    lcdBottomLine(data);
  }
  // delay(100);
}

// Rounded HH:mm format
char * hoursToString(double h, char *str)
{
  int m = int(round(h * 60));
  int hr = (m / 60) % 24;
  int mn = m % 60;

  str[0] = (hr / 10) % 10 + '0';
  str[1] = (hr % 10) + '0';
  str[2] = ':';
  str[3] = (mn / 10) % 10 + '0';
  str[4] = (mn % 10) + '0';
  str[5] = '\0';
  return str;
}

// put function definitions here:
void debugTime(DateTime now) {
  char data[20];
  sprintf(data, "%d/%d/%d %d:%d", now.day(), now.month(), now.year(), now.hour(), now.minute());
  Serial.println(data);
}

void lcdTopLine(const char *data) {
  lcd.setCursor(0,0);
  lcd.print(data);
}

void lcdBottomLine(const char *data) {
  lcd.setCursor(0,1);
  lcd.print(data);
}
