int state = STATE_IDLE;
int oldState = STATE_DEBUG;
int ivalue;
float value;
int setDateTimeIndex = 0;

// Revert to IDLE after 2 minutes
#define NO_BUTTON_TIMEOUT (20 * 1000)
long noButtonTimeout = 0;

int get_state() {
  return state;
}

void set_state(int new_state) {
    if (new_state < 0 ) { new_state = STATE_LAST - 1; }
    else if (new_state >= STATE_LAST ) { new_state = STATE_IDLE; }
    state = new_state;
    showState("                ");
}

void run_state_machine(int button) {
  // State machine starts here
  wdt_reset();
  if (button == BUTTON_NONE) {
    if (state == STATE_IDLE) {
       float h = dht.readHumidity();
       float t = dht.readTemperature() + params.tempOffset;

       // check if returns are valid, if they are NaN (not a number) then something went wrong!
       if (!isnan(t) && !isnan(h)) {
          showState("");
          lcd.print((int)t);
          lcd.print((char)223);
          lcd.print("C ");
          lcd.print((int)(9.0/5.0*t) + 32);
          lcd.print((char)223);
          lcd.print("F ");
          lcd.print((int)h);
          lcd.print("%RH");
       }
    } else {
      // We want to revert to IDLE after a perion of inactivity
      if (millis() > noButtonTimeout) {
         set_state(STATE_IDLE);
         oldState = state;
      }
    }
    return;
  }
  
  // Reset timeout
  noButtonTimeout = millis() + NO_BUTTON_TIMEOUT;
  // Reset pump if a button has been pressed
  PUMP_RELAY_OFF();
  
  if (button == BUTTON_DOWN) {
    set_state(state+1);
  }
  if (button == BUTTON_UP) {
    set_state(state-1);
  }
  
  bool newState = state != oldState;
  switch( state )
   {
      case STATE_IDLE:
      {
         break;
      }
      case STATE_SET_PH:
      {
         if (newState) {
           value = getPh();
         }
         value = adjust( button, value, .1);
         showState("Measured pH", value);
         if (button == BUTTON_SELECT) {
           setPh(value);
           set_state(STATE_IDLE);
         }
         break;
      }      
      case STATE_MAX_PUMP_ON:
      {
         if (newState) {
           ivalue = params.pumpOnMaxSecs;
         }
         ivalue = adjust( button, ivalue, 1);
         showState("Pump on", ivalue, " secs");
         if (button == BUTTON_SELECT) {
           params.pumpOnMaxSecs = ivalue;
           write_params();
           set_state(STATE_IDLE);
         }
         break;
      }      
      case STATE_MIN_PUMP_OFF:
      {
         if (newState) {
           ivalue = params.pumpOffMinMins;
         }
         ivalue = adjust( button, ivalue, 1);
         showState("Pump off", ivalue, " mins");
         if (button == BUTTON_SELECT) {
           params.pumpOffMinMins = ivalue;
           write_params();
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_SET_DATE_TIME:
      {
         if (newState) {
           showState("Set Date/Time");
           setDateTimeIndex = 0;
           break;
         }
         setDateTime(button);
         break;
      }
      case STATE_TEMP_ADJ:
      {
         if (newState) {
           ivalue = dht.readTemperature() + params.tempOffset;
         }
         ivalue = adjust( button, ivalue, 1);
         showState("Actual temp", ivalue, " C");
         if (button == BUTTON_SELECT) {
           params.tempOffset = ivalue - dht.readTemperature();
           write_params();
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_CALIBRATE_PH4:
      {
         showState("Calibrate pH 4");
         if (button == BUTTON_SELECT) {
           calibratepH4();
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_CALIBRATE_PH7:
      {
         showState("Calibrate pH 7");
         if (button == BUTTON_SELECT) {
           calibratepH7();
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_RESET_PH:
      {
         showState("Reset pH params");
         if (button == BUTTON_SELECT) {
           reset_Params();
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_REBOOT:
      {
        showState("Reboot");
        if (button == BUTTON_SELECT) {
          cli();                  // Clear interrupts
          wdt_enable(WDTO_15MS);  // Set the Watchdog to 15ms
          while(1);               // Enter an infinite loop
        }
        break;
      }
      case STATE_RUN_PUMP:
      {
         if (newState) {
           ivalue = 7;
         }
         ivalue = adjust( button, ivalue, 1);
         showState("Run pump", ivalue, " secs");
         if (button == BUTTON_SELECT) {
           PUMP_RELAY_ON();
           while (ivalue > 0) {
             ivalue--;
             // Reset the watchdog timer every 5 seconds
             if ( ivalue % 5 == 0 ) { wdt_reset(); }
             long time = millis();
             //while (millis() < time + 1000) {}
             delay(1000);
             showState("Run pump", ivalue, " secs");
           }
           showState("Run pump", ivalue, " secs");
           PUMP_RELAY_OFF(); 
           delay(350);
           set_state(STATE_IDLE);
         }
         break;
      }
      case STATE_SHOW_DECAY:
      {
         if (newState) {
           showState("Show pH decay");
           ivalue = 0;
         } else if (button == BUTTON_SELECT) {
           set_state(STATE_IDLE);
         } else {
           // We are only interested in 10 mins and up, not 0 mins
           if (button == BUTTON_LEFT) ivalue--;
           if (button == BUTTON_RIGHT) ivalue++;
           if (ivalue < 1) ivalue = getDecayEntries()-1;
           if (ivalue >= getDecayEntries()) ivalue = 1;
           showState("", ivalue * 10, " mins ");
           lcd.print(params.phDecay[ivalue]);
           lcd.print("   ");
         }
         break;
      }
      case STATE_DEBUG:
      {
         if (newState) {
           showState("Debug: Show vars");
           ivalue = -1;
         } else if (button == BUTTON_SELECT) {
           set_state(STATE_IDLE);
         } else {
           if (button == BUTTON_LEFT) ivalue--;
           if (button == BUTTON_RIGHT) ivalue++;
           ivalue = show_debug(ivalue);
         }
         break;
      }
      default:
      {
        // Call set_state() to fix any bad value state might have
        set_state(state);
        break;
      }
    }
    oldState = state;
}

// If L or R has been pressed then adjust the value accordingly
float adjust(int button, float value, float delta) {
  if (button == BUTTON_LEFT) value = value - delta;
  else if (button == BUTTON_RIGHT) value = value + delta;
  return value;
}

// If L or R has been pressed then adjust the value accordingly
int adjust(int button, int value, int delta) {
  if (button == BUTTON_LEFT) value = value - delta;
  else if (button == BUTTON_RIGHT) value = value + delta;
  return value;
}

int dtValue;
int oldDtValue = -1;
char *dtString[] = { "not used", "Minutes", "Hours", "Day", "Month", "Year" };
#define DT_ARRAY_SIZE 6

void setDateTime(int button) {
  if (button == BUTTON_SELECT) {
    // Save last value
    if (setDateTimeIndex > 0 && dtValue != oldDtValue) {
      setRtc(setDateTimeIndex, dtValue);
    }
    // Move to next value
    setDateTimeIndex++;
    if (setDateTimeIndex >= DT_ARRAY_SIZE) { setDateTimeIndex = 1; }
    dtValue = getRtc(setDateTimeIndex);
    oldDtValue = dtValue;
  }
  dtValue = adjust( button, dtValue, 1);
  showState("                ");
  showState(dtString[setDateTimeIndex], dtValue);
}

int getRtc(int dtIndex) {
  myRTC.updateTime();
  switch (dtIndex) {
    case 1: return myRTC.minutes;
    case 2: return myRTC.hours;
    case 3: return myRTC.dayofmonth;
    case 4: return myRTC.month;
    case 5: return myRTC.year;
  }
}

void setRtc(int dtIndex, int value) {
  myRTC.updateTime();
  switch (dtIndex) {
    case 1: myRTC.minutes = value; myRTC.seconds = 0; break;
    case 2: myRTC.hours = value; break;
    case 3: myRTC.dayofmonth = value; break;
    case 4: myRTC.month = value; break;
    case 5: myRTC.year = value; break;
    default: showState("Error in setRTC"); return;
  }
  myRTC.setDS1302Time(myRTC.seconds, myRTC.minutes, myRTC.hours, 1, myRTC.dayofmonth, myRTC.month, myRTC.year);
}

void showState(char *msg) {
    lcd.setCursor( 0, 1 );
    lcd.print( msg );
}

void showState(char *msg, float value) {
    showState( msg );
    lcd.print( " " );
    lcd.print( value, 1 );
}

void showState(char *msg, int value) {
    showState( msg, value, "" );
}

void showState(char *msg, int value, char *msg1) {
    showState( msg );
    lcd.print( " " );
    lcd.print( value );
    lcd.print( msg1 );
}

