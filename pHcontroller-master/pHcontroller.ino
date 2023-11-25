/*
  Pins used by LCD & Keypad Shield:
  
    A0: Buttons, analog input from voltage ladder
    D4: LCD bit 4
    D5: LCD bit 5
    D6: LCD bit 6
    D7: LCD bit 7
    D8: LCD RS
    D9: LCD E
    
    // D3: LCD Backlight (high = on, also has pullup high so default is on)
  
*/
/*--------------------------------------------------------------------------------------
  Includes
--------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal.h>   // include LCD library
#include <avr/wdt.h>
#include <DHT.h>
#include <virtuabotixRTC.h>
#include <TimeLib.h>

// All the pins used in this sketch should be defined here
#define PUMP_RELAY_PIN      2  // controls the pump relay
#define DHT_PIN             3  // temp and humidty pin

#define PIN_LCD_D4          4
#define PIN_LCD_D5          5
#define PIN_LCD_D6          6
#define PIN_LCD_D7          7
#define PIN_LCD_RS          8
#define PIN_LCD_E           9

#define BUTTON_ADC_PIN     A0  // A0 is the button ADC input

#define PIN_RTC_CLOCK      A1
#define PIN_RTC_DATA       A2
#define PIN_RTC_CE_RST     A3

#define I2C_1              A4
#define I2C_2              A5

// End of pin definitions


#define STATE_IDLE          0
#define STATE_SET_PH        1
#define STATE_SET_DATE_TIME 2
#define STATE_TEMP_ADJ      3
#define STATE_MAX_PUMP_ON   4
#define STATE_MIN_PUMP_OFF  5
#define STATE_SHOW_DECAY    6
#define STATE_CALIBRATE_PH7 7
#define STATE_CALIBRATE_PH4 8
#define STATE_RESET_PH      9
#define STATE_REBOOT       10
#define STATE_RUN_PUMP     11
#define STATE_DEBUG        12
#define STATE_LAST         13

//return values for ReadButtons()
#define BUTTON_NONE         0  // 
#define BUTTON_RIGHT        1  // 
#define BUTTON_UP           2  // 
#define BUTTON_DOWN         3  // 
#define BUTTON_LEFT         4  // 
#define BUTTON_SELECT       5  // 

#define PUMP_RELAY_OFF()     digitalWrite( PUMP_RELAY_PIN, LOW )
#define PUMP_RELAY_ON()      digitalWrite( PUMP_RELAY_PIN, HIGH )
#define PUMP_RELAY_IS_ON()   digitalRead( PUMP_RELAY_PIN )

// Temp and humidity
#define DHTTYPE DHT11   // DHT 11 
DHT dht(DHT_PIN, DHTTYPE);

// Permenant values stored in EEPROM
//Our parameter, for ease of use and eeprom access lets use a struct
struct parameters_T
{
  unsigned int WriteCheck1;
  int pH7Cal, pH4Cal;
  float pHStep;
  int pumpOnMaxSecs;
  int pumpOffMinMins;
  int tempOffset;
  int phOffset;
  long savedClock;
  long lastReboot;
  long lastPumpOn;
  float phPumpOn;
  long lastPumpOff;
  float phDecay[20];
  unsigned int WriteCheck2;
} params;

float pH;

int led = 13;

/*--------------------------------------------------------------------------------------
  Init the LCD library with the LCD pins to be used
--------------------------------------------------------------------------------------*/
//LiquidCrystal lcd( 8, 9, 4, 5, 6, 7 );   //Pins for the freetronics 16x2 LCD shield. LCD: ( RS, E, LCD-D4, LCD-D5, LCD-D6, LCD-D7 )
LiquidCrystal lcd( PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7 );

// Initialise the RTC
virtuabotixRTC myRTC(PIN_RTC_CLOCK, PIN_RTC_DATA, PIN_RTC_CE_RST);

/*--------------------------------------------------------------------------------------
  setup()
  Called by the Arduino framework once, before the main loop begins
--------------------------------------------------------------------------------------*/
void setup()
{
  pinMode(led, OUTPUT);
  Wire.begin(); //conects I2C
  
  read_params();

   //set up the LCD number of columns and rows: 
   lcd.begin( 16, 2 );
   //Print some initial text to the LCD.
   lcd.setCursor( 0, 0 );   //top left
   //          1234567890123456
   lcd.write( "La Geneste      " );
   showState( "pH Regulator    " );
   delay(2000);   
  
  // Set up RTC
  setSyncProvider(getTime);   // the function to get the time from the RTC
  if(timeStatus()!= timeSet) {
     showState( "RTC failed sync " );
     delay(2000);
     // Set the time to the latest time we have recorded
     time_t t = max(params.lastPumpOn, params.savedClock);
     setTime(t);
  }
  lcd.clear();

  //Serial.begin(9600);
  //Lets read our Info from the eeprom and setup our params,
  //if we loose power or reset we'll still remember our settings!
  setup_buttons();
  
   // Pump relay switch
   digitalWrite( PUMP_RELAY_PIN, LOW );
   pinMode( PUMP_RELAY_PIN, OUTPUT );

   dht.begin();
   watchdogSetup();
   
   // Save the reboot time if valid time
   if (timeStatus() != timeNotSet)
     params.savedClock = now();
     
   params.lastReboot = now();
     
   write_params();
}
/*--------------------------------------------------------------------------------------
  loop()
  Arduino main loop
--------------------------------------------------------------------------------------*/
long lastCheck = 0;
bool ledOn = false;
bool dateTimeFlip = false;
bool rtcFail = false;

void loop() {
  long currentSec = millis() / 1000;
  
  bool secondElapsed = (currentSec - lastCheck) >= 1;
  if (secondElapsed) {
    lastCheck = currentSec;
  }
  
  float pH = getPh();
  
  // Run this once a second
  if (secondElapsed) {
    showPh(pH, get_state() != STATE_IDLE);
    
    bool timeValid = timeStatus() != timeNotSet;
    
    if (get_state() == STATE_IDLE && (pH + 0.5 >= 7.3) && !PUMP_RELAY_IS_ON()) {
      // The pH is too high, we must reduce it, as long as we have a valid time
      // and we have let pumpOff mins elapsed
      if (timeValid && (now() - params.lastPumpOn) > (params.pumpOffMinMins * 60)) {
        PUMP_RELAY_ON();
        params.lastPumpOn = now();
        write_params();
        params.phPumpOn = pH;
        clearPhDecay();
      }
    }
    if (!timeValid) {
      // No valid time, make sure pump is off
      PUMP_RELAY_OFF();
      if (get_state() == STATE_IDLE) {
        if ((currentSec % 4) == 0) {
          lcd.setCursor(0, 0);
          lcd.print("No Time,Battery?");
        }
        showState("Pump Inoperable");
      }
    } else if (rtcFail && (currentSec % 4) == 0) {
      // We have a valid time but no RTC, show message every 4 secs
      showState("Clock Battery? ");
    }
    
    if (pH < 7.3) {
      PUMP_RELAY_OFF();
      params.lastPumpOff = now();
    }
    if (PUMP_RELAY_IS_ON()) {
      showState("Pump on", now() - params.lastPumpOn, " secs  ");
      // Check to see if it has been on for long enough
      if ((now() - params.lastPumpOn) > params.pumpOnMaxSecs) {
        PUMP_RELAY_OFF();
        // The pump has been on for the appropriate time, remember the duration
        params.lastPumpOff = now();
      }
    }
    
    // Let's do the calculations for rate of effectiveness
    calcPhDecay(pH);    
    
    // Update a little flashy display in the last characters
    markActive(get_state());
    
    // Show a heart beat
    flipLed();

    // Display time?
    // Alternate time and date every four seconds
    if (timeValid && (currentSec % 4)==0) {
      dateTimeFlip = !dateTimeFlip;
      lcd.setCursor(8, 0);
      lcd.print("        ");
      if (dateTimeFlip) {
        lcd.setCursor(8, 0);
        int mday = day();
        if (mday < 10) lcd.print(" ");
        lcd.print(mday);
        lcd.print("/");
        lcdPrint2(month());
        lcd.print("/");
        lcd.print(year() - 2000); 
      } else {
        lcd.setCursor(11, 0);
        lcdPrint2(hour());
        lcd.print(":"); 
        lcdPrint2(minute());      
      }
    }
    // If we don't have a system clock, keep the time every hour in case of a reboot
    // We don't use the actual time for anything, so it doesn't matter if it is slow
    if (rtcFail && ((now() - params.savedClock) / SECS_PER_MIN > 15)) {
      params.savedClock = now();
      write_params();
    }
  }
  
  int button = get_button_press();
  run_state_machine(button);
  
  delay(1);
}

time_t getTime() {
  tmElements_t tm;
  myRTC.updateTime();
  tm.Second = myRTC.seconds;
  tm.Minute = myRTC.minutes;
  tm.Hour = myRTC.hours;
  tm.Wday = 1; // This does not matter, it seems to be set correctly
  tm.Day = myRTC.dayofmonth;
  tm.Month = myRTC.month;
  tm.Year = myRTC.year - 1970;
  if (myRTC.year < 2014 || myRTC.year > 2024) {
    rtcFail = true;
    return 0;
  }
  rtcFail = false;
  return makeTime(tm);
}

void flipLed() {
    digitalWrite(led, ledOn ? HIGH : LOW );
    ledOn = !ledOn;
}

#define CHARSIZE 6
int lcdChars[] = {0xdf, 0xa5, 0x2e, 0xa1, 0x2e, 0xa5};
int lcdIndex = 0;

void markActive(int state) {
  if (state != STATE_IDLE) return;
  lcd.setCursor(15,1);
  lcd.print((char)lcdChars[lcdIndex]);
  lcdIndex = ++lcdIndex % CHARSIZE;
  return;
}

void lcdPrint2(int value) {
  if (value <10) lcd.print("0");
  lcd.print(value);
}


