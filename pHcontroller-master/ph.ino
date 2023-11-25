 
#define ADDRESS 0x4C // MCP3221 A5 in Dec 77 A0 = 72 A7 = 79)
                     // A0 = x48, A1 = x49, A2 = x4A, A3 = x4B,
                     // A4 = x4C, A5 = x4D, A6 = x4E, A7 = x4F
 
    //Our vRef into the ADC wont be exact
    //Since you can run VCC lower than Vref its
    //best to measure and adjust here
    const float vRef = 4.96; // Was 4.096
    const float opampGain = 5.25; //what is our Op-Amps gain (stage 1)

float getPh(){
   int adc_result = get_adc_result();
   //We have a our Raw pH reading fresh from the ADC now lets figure out what the pH is 
   calcpH(adc_result);
   return pH + ((float)params.phOffset)/10.0;
}

void setPh(float newPh) {
   float measuredPh = getPh() - ((float)params.phOffset)/10.0;
   params.phOffset = (int)lround((newPh - measuredPh) * 10);
   write_params();
}

int show_ph_param(int index) {
     //Lets read in our parameters and spit out the info!
     read_params();
     showState("                ");
     switch (index) {
      case 1:
        showState("pH 7 cal", params.pH7Cal);
        break;
      case 2:
        showState("pH 4 cal", params.pH4Cal);
        break;
      case 3:
        showState("pH slope", params.pHStep);
        break;
      default:
        showState("index error:", index);
     }
}

void showPh( float pH, bool debug ) {
   //Spit out some debugging/Info to show what our pH and raws are
   lcd.setCursor(0, 0);
   lcd.print("pH ");
   // Two dec places if debug is set
   lcd.print(pH,debug?2:1);
   lcd.print(" ");
   if (!debug) lcd.print(" ");
}

int getDecayEntries() {
  return sizeof(params.phDecay)/sizeof(float);
}

void clearPhDecay() {
    int entries = getDecayEntries();
    for (int i=0; i<entries; i++) {
       params.phDecay[i] = 0;
    }
}
 
void calcPhDecay(long pH) {
    if (params.phPumpOn > 0.5) {
      int entries = getDecayEntries();
      int elapsedSecs = now() - params.lastPumpOff;
      for (int i=0; i<entries; i++) {
        // record ph diff if it is a 10 minutes segment
        if (int(elapsedSecs / 60L) == i * 10) {
          params.phDecay[i] = pH - params.phPumpOn;
        }
      }
    }
}
 
float getPhDecay(int index) {
   return params.phDecay[index];
}
 
//Lets read our raw reading while in pH7 calibration fluid and store it
//We will store in raw int formats as this math works the same on pH step calcs
void calibratepH7()
{
  params.pH7Cal = get_adc_result();
  calcpHSlope();
  //write these settings back to eeprom
  params.phOffset = 0;
  write_params();
}
 
//Lets read our raw reading while in pH4 calibration fluid and store it
//We will store in raw int formats as this math works the same on pH step calcs
//Temperature compensation can be added by providing the temp offset per degree
//IIRC .009 per degree off 25c (temperature-25*.009 added pH@4calc)
void calibratepH4()
{
  params.pH4Cal = get_adc_result();
  calcpHSlope();
  //write these settings back to eeprom
  params.phOffset = 0;
  write_params();
}
 
//This is really the heart of the calibration proccess, we want to capture the
//probes "age" and compare it to the Ideal Probe, the easiest way to capture two readings,
//at known point(4 and 7 for example) and calculate the slope.
//If your slope is drifting too much from Ideal(59.16) its time to clean or replace!
void calcpHSlope ()
{
  //RefVoltage * our deltaRawpH / 12bit steps *mV in V / OP-Amp gain /pH step difference 7-4
   params.pHStep = ((((vRef*(float)(params.pH7Cal - params.pH4Cal))/4096)*1000)/opampGain)/3;
}
 
//Now that we know our probe "age" we can calucalate the proper pH Its really a matter of applying the math
//We will find our milivolts based on ADV vref and reading, then we use the 7 calibration
//to find out how many steps that is away from 7, then apply our calibrated slope to calcualte real pH
void calcpH(int raw)
{
 float miliVolts = (((float)raw/4096)*vRef)*1000;
 float temp = ((((vRef*(float)params.pH7Cal)/4096)*1000)- miliVolts)/opampGain;
 pH = 7-(temp/params.pHStep);
}

// Get the raw pH reading from the ADC
int get_adc_result() {
  //This is our I2C ADC interface section
  //We'll assign 2 BYTES variables to capture the LSB and MSB(or Hi Low in this case)
  byte adc_high;
  byte adc_low;
  //We'll assemble the 2 in this variable
  int adc_result;
   
  Wire.requestFrom(ADDRESS, 2);        //requests 2 bytes
  while(Wire.available() < 2);         //while two bytes to receive
  //Set em
  adc_high = Wire.read();          
  adc_low = Wire.read();
  //now assemble them, remembering our byte maths a Union works well here as well
  adc_result = (adc_high * 256) + adc_low;
  return adc_result;
}

