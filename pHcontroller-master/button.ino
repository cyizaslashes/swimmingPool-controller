/*--------------------------------------------------------------------------------------
  Defines
--------------------------------------------------------------------------------------*/
// ADC readings expected for the 5 buttons on the ADC input
// Found after debugging

#define RIGHT_10BIT_ADC           0  // right
#define UP_10BIT_ADC            131  // up
#define DOWN_10BIT_ADC          306  // down
#define LEFT_10BIT_ADC          477  // left
#define SELECT_10BIT_ADC        720  // select
#define BUTTON_HYSTERESIS         50  // hysteresis for valid button sensing window


byte buttonWas          = BUTTON_NONE;   //used by ReadButtons() for detection of button events
long buttonTime = 0;

void setup_buttons() {
   //button adc input
   pinMode( BUTTON_ADC_PIN, INPUT );         //ensure A0 is an input
   digitalWrite( BUTTON_ADC_PIN, LOW );      //ensure pullup is off on A0
}

int get_button_press()
{
  // If we have only just recorded a button change return nothing
  // This will get rid of button bounce
  if (buttonTime + 100 >= millis())
    return BUTTON_NONE;
    
  //get the latest button pressed
  byte button = ReadButtons();
  
  // We only want to report a button once
  if (button == buttonWas)
    return BUTTON_NONE;
  buttonWas = button;

  // If we have a new button press, record its time
  if (button != BUTTON_NONE)
    buttonTime = millis();
  
  /**
   //show character for the button pressed
   lcd.setCursor( 15, 1 );
   switch( button )
   {
     case BUTTON_NONE:  { lcd.print( " " ); break; }
     case BUTTON_RIGHT: { lcd.print( "R" ); break; }
     case BUTTON_UP:    { lcd.print( "U" ); break; }
     case BUTTON_DOWN:  { lcd.print( "D" ); break; }
     case BUTTON_LEFT:  { lcd.print( "L" ); break; }
     case BUTTON_SELECT:{ lcd.print( "S" ); break; }
     default:           { break; lcd.print( "X" ); }
   }
   **/
/* 
   //debug/test display of the adc reading for the button input voltage pin.
   lcd.setCursor(12, 0);
   lcd.print( "    " );          //quick hack to blank over default left-justification from lcd.print()
   lcd.setCursor(12, 0);         //note the value will be flickering/faint on the LCD
   lcd.print( analogRead( BUTTON_ADC_PIN ) );
/**/

   return button;
}
/*--------------------------------------------------------------------------------------
  ReadButtons()
  Detect the button pressed and return the value
  Uses global values buttonWas, buttonJustPressed, buttonJustReleased.
--------------------------------------------------------------------------------------*/
byte ReadButtons()
{
   //read the button ADC pin voltage
   int adc_key_in = analogRead(BUTTON_ADC_PIN);      // read the value from the sensor 
   delay(5); //switch debounce delay. Increase this delay if incorrect switch selections are returned.
   //give the button a slight range to allow for a little contact resistance noise
   int k = (analogRead(BUTTON_ADC_PIN) - adc_key_in); 
   // double check the keypress. If the two readings are not equal +/-k value after debounce delay, it tries again.
   if (5 < abs(k)) return BUTTON_NONE;  
   // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
   // we add approx 50 to those values and check to see if we are close
   // We make this the 1st option for speed reasons since it will be the most likely result
   if (adc_key_in > 1000) return BUTTON_NONE; 
   adc_key_in -= BUTTON_HYSTERESIS;
   if (adc_key_in < RIGHT_10BIT_ADC)   return BUTTON_RIGHT;  
   if (adc_key_in < UP_10BIT_ADC)  return BUTTON_UP; 
   if (adc_key_in < DOWN_10BIT_ADC)  return BUTTON_DOWN; 
   if (adc_key_in < LEFT_10BIT_ADC)  return BUTTON_LEFT; 
   if (adc_key_in < SELECT_10BIT_ADC)  return BUTTON_SELECT;   
   return BUTTON_NONE;  // when all others fail, return this...
}

