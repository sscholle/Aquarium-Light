#include <Arduino.h>
#include <SPI.h>
#include <Wire.h> 
#include <RTClib.h>
#include <ezButton.h>
#include <SolarCalculator.h>
#include <EEPROM.h>

// SOLAR CALCULATOR VARIABLES
double latitude = -34.0195173;     // Observer's latitude 
double longitude = 22.779688;  // Observer's longitude
int time_zone = 2;          // UTC offset
double sunAzimuth, sunElevation;
double transit, sunrise, sunset;
 
// BUTTON VARIABLES
const int SHORT_PRESS_TIME = 1000; // 1000 milliseconds
const int LONG_PRESS_TIME  = 1000; // 1000 milliseconds

const byte buttonPin = 7;
ezButton button(buttonPin);  // create ezButton object that attach to pin 7;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;
volatile bool buttonPushed = false;

// CLOCK VARIABLES
RTC_DS3231 RTC;


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
const byte interruptPin = 2;// RTC interrupt pin
volatile byte brightness = 0;// pwm value to be used in the ISR
byte mode = 1;// 0 = manual, 1 = automatic
const byte manualBrightness = 127;// brightness value for manual mode
const int eepromAddress = 0;// address to store the mode

// put function declarations here:
char * hoursToString(double h, char *str);
void displayTime(DateTime now);

void setup() {
  // SERIAL
  Serial.begin(115200); //Starts serial connection

  // EEPROM
  EEPROM.get(eepromAddress, mode);// get the mode from the EEPROM
  if(mode != 0 && mode != 1) {
    mode = 1;// default to automatic mode
  }

  // BUTTON
  button.setDebounceTime(50); // set debounce time to 50 milliseconds

  // PINS
  pinMode(gatePin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // CLOCK
  RTC.begin();
  if (RTC.lostPower()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  // RTC.writeSqwPinMode(DS3231_SquareWave1Hz);

  
  // SUN

}

void setMode(byte newMode) {
  mode = newMode;
  EEPROM.put(eepromAddress, mode);// save the mode to the EEPROM
}

// setup an interrupt to update the brightness of the LED based on the time of day
void loop() {
  // LED INDICATOR CODE
  digitalWrite(ledPin, mode == 0 ? HIGH : LOW);// indicate the mode
  
  // BUTTON CODE
  button.loop(); // MUST call the loop() function first

  // IF button is pressed - start debounce
  if(button.isPressed())
    pressedTime = millis();

  if(button.isReleased()) {
    releasedTime = millis();

    long pressDuration = releasedTime - pressedTime;

    if( pressDuration < SHORT_PRESS_TIME ) {
      // short press
      if(mode == 0) {
        // if in manual mode, switch to automatic mode
        setMode(1);
      } else {
        // if in automatic mode, switch to manual mode
        setMode(0);
      }
      Serial.println("A short press is detected");
    }

    if( pressDuration > LONG_PRESS_TIME )
      Serial.println("A long press is detected");
  }


  // TIME AND BRIGHTNESS CODE
  // put your main code here, to run repeatedly:
  DateTime now = RTC.now();
  displayTime(now);

  // SUNRISE/SUNSET CODE
  // this only needs to happen every 24 hours - to recalculate the sunrise and sunset times
  
  // calcSunriseSunset(now.year(), now.month(), now.day(), latitude, longitude, transit, sunrise, sunset);
  // // Print results
  // char str[6];
  // Serial.println(hoursToString(sunrise + time_zone, str));
  // Serial.println(hoursToString(transit + time_zone, str));
  // Serial.println(hoursToString(sunset + time_zone, str));



  // NOTE: elevation calculation will be used to limit the brightness of the LED (example 90=max, 0=min)
  // Calculate the solar position, in degrees
  calcHorizontalCoordinates(now.unixtime(), latitude, longitude, sunAzimuth, sunElevation);

  // Print results
  Serial.print(F("Az: "));
  Serial.print(sunAzimuth);
  Serial.print(F("°  El: "));
  Serial.print(sunElevation);
  Serial.println(F("°"));

  if(mode == 0) {
    // manual mode
    analogWrite(gatePin, manualBrightness);
  } else {
    // automatic mode
    // calculate the brightness based on the sun's elevation
    analogWrite(gatePin, map(min(90, max(-20, sunElevation)), -20, 90, 0, 255));
  }

  delay(500);
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
void displayTime(DateTime now) {
  char data[20];
  sprintf(data, "%d/%d/%d %d:%d", now.day(), now.month(), now.year(), now.hour(), now.minute());
  Serial.println(data);
}