//We'll want to save calibration and configration information in EEPROM
#include <avr/eeprom.h>
//EEPROM trigger check
#define Write_Check      0x1234

parameters_T defaults = {
  Write_Check,
  2167,       // pH7Cal. KPT's value, was 2048 assuming ideal probe and amp conditions 1/2 of 4096
  1286,       // pH4Cal using ideal probe slope we end up this many 12bit units away on the 4 scale
  59.16,      // pHStep ideal probe slope
  20,         // pumpOnMaxSecs
  40,         // pumpOffMinMins
  0,          // tempOffset
  0,          // phOffset x 10
  0L,         // savedClock
  0L,         // lastReboot
  0L,         // lastPumpOn
  0.0,        // phPumpOn
  0L,         // lastPumpOff
  { 0 },
  Write_Check
};

// Get the param values from backing store
void read_params() {
  eeprom_read_block(&params, (void *)0, sizeof(params));
  //if its a first time setup or our magic number in eeprom is wrong reset to default
  if (params.WriteCheck1 != Write_Check && params.WriteCheck2 != Write_Check){
    reset_Params();
  }
}

void write_params() {
  params.WriteCheck1 = Write_Check;
  params.WriteCheck2 = Write_Check;
  eeprom_write_block(&params, (void *)0, sizeof(params)); //write these settings back to eeprom
}

//This just simply applys defaults to the params incase the need to be reset or
//they have never been set before (!magicnum)
void reset_Params(void)
{
  //Restore to default set of parameters!
  eeprom_write_block(&defaults, (void *)0, sizeof(defaults));
  read_params();
  showState("**Params reset**");
  delay(5000);
}

int show_debug(int index) {
  if (index < 0) index = 4;
  if (index > 4) index = 0;
  switch (index) {
    case 1:
    case 2:
    case 3:
      show_ph_param(index);
      break;
    case 4:
      showState("uptime",(now()-params.lastReboot)/60," mins");
      break;
    default:
      index = 0;
  }
  return index;
}




