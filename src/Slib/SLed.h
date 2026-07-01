#ifndef SLed_h
#define SLed_h
#include <Arduino.h>
//
// Abstract Base LED Class.
//
class SLedVirtual {
  private:
    uint32_t LoopMS;
    uint32_t SeriesMS;
    uint8_t  LedBlinkLoop;
    uint8_t  LedBlinkMode;
    uint8_t  LedBlinkCount;
    uint16_t LoopTM;  
    bool     LedStat;
  public:     
    virtual void setLed( bool stat );
    uint16_t SeriesTM;
    SLedVirtual( uint16_t tm = 125 );
    void SetBlinkMode( uint8_t mode );
    void On();
    void Off();
    void ShortBlink();
    void LongBlink();
    void DoubleBlink(); 
    void SeriesBlink( uint8_t num, uint16_t ms = 200 );
    void Loop();   
     
};

//
// GPIO LED Class 
//
class SLed : public SLedVirtual {
  private:
     uint8_t Pin;
  public:
     void setLed(bool state);
     SLed( uint8_t pin , uint16_t tm = 125);
};

#endif
