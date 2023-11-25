void watchdogSetup(void)
{
   cli();
   wdt_reset();
   wdt_enable(WDTO_8S);
   sei();
   return;
  /*
   WDTCSR conﬁguration:
   WDIE = 1: Interrupt Enable
   WDE = 1 :Reset Enable
   WDP3210
      0100 = 250ms
      0101 = 500ms
      0110 = 1000ms
      0111 = 2000ms
      1000 = 4000ms
      1001 = 8000ms
  */
  
  // Enter Watchdog Conﬁguration mode:
  WDTCSR |= (1<<WDCE) | (1<<WDE); 
  // Set Watchdog settings:
  WDTCSR = (1<<WDIE) | (1<<WDE) 
         // 8 seconds 
         | (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0);
   
  sei();
}
