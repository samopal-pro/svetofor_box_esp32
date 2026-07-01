#include "SLed.h"
//*************************************************************************************************************************************
// GPIO Led Class 
//
/**
 * Constructor
 * @param pin - GPIO number
 * @param tm - optional, tick time, ms (Default 125 )
 */
SLed::SLed(uint8_t pin, uint16_t tm):  SLedVirtual( tm) {
  Pin = pin;
  pinMode(Pin, OUTPUT);
  digitalWrite(Pin, LOW);
}


/**
 * On/Off LED
 */
void SLed::setLed(bool state){
  digitalWrite(Pin,state);
}
//**************************************************************************************************************************************
// Base Abstract LED class 
//
/**
 * Constructor 
 * @param tm - optional, tick time, ms (Default 125 )
 */
SLedVirtual::SLedVirtual( uint16_t tm ){
  LedBlinkLoop  = 0;
  LedBlinkMode  = 0;
  LedBlinkCount = 0;
  LoopTM        = tm;  
  LoopMS        = 0;
//  SeriesTM      = 200;
}

/**
 * Set blink mode
 * @param mode - Blink mask mode 
 */
void SLedVirtual::SetBlinkMode( uint8_t mask ){
  LedBlinkMode = mask;
  Loop();
}

/**
 * On LED
 */
void SLedVirtual::On(){
  SetBlinkMode( 0B11111111 );
}

/**
 * Off LED
 */
void SLedVirtual::Off(){
  SetBlinkMode( 0B00000000 );
}

/**
 * Short flash
 */
void SLedVirtual::ShortBlink(){
  SetBlinkMode( 0B10000000 );
}

/**
 * Long flash 
 */
void SLedVirtual::LongBlink(){
  SetBlinkMode( 0B11110000 );
}

/**
 * Double flash 
 */
void SLedVirtual::DoubleBlink(){
  SetBlinkMode( 0B10100000 );
}

/**
 * Set a single flash series
 * @param num - flash number
 * @param ms  - optional, flash interval, ms. Default 200
 */
void SLedVirtual::SeriesBlink( uint8_t num, uint16_t ms ){
   LedBlinkCount = num;
   SeriesTM = ms;
   SeriesMS = 0;  
   LedStat  = false; 
   Loop();
}

/**
 * Loop LED
 */
void SLedVirtual::Loop(){
   uint32_t ms = millis();
// Single flash series mode  
   if( LedBlinkCount > 0 ){
      if( SeriesMS == 0 || ( ms - SeriesMS ) > SeriesTM || ms < SeriesMS ){
         SeriesMS = ms;
         if( LedStat == false ){
            LedStat = true;
            setLed(true);
         }
         else {
            LedStat = false;
            LedBlinkCount--;
            setLed(false);            
         }
      }
   }
// Repeatable flashing LED mode   
   else {    
      if( LoopMS == 0 || ( ms - LoopMS ) > LoopTM || ms < LoopMS ){
         LoopMS = ms;
// Bit mask parse       
         if(  LedBlinkMode & 1<<(LedBlinkLoop&0x07) ) setLed(true); 
         else  setLed(false);
         LedBlinkLoop++;    
      }
   } 
}
