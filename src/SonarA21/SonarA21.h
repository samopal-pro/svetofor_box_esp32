#pragma once

#include <Arduino.h>
#include <Wire.h>

#define DEFAULT_SONAR_A21_ADDR    0x74

class SonarA21
{
  public:
 
    enum regAddr : uint8_t
    {
      PROG_VERSION         = 0x00,
      DISTANCE             = 0x02,
      I2C_ADDRESS          = 0x05,
      POWER_NOISE_LEVEL    = 0x06,
      ANGLE_LEVEL          = 0x07,
      HOLD                 = 0x09,
      TEMPERATURE          = 0x0A,
      COMMAND              = 0x10,
    };

    
    enum command : uint8_t
    {
      RANGE_LEVEL1         = 0xBD,
      RANGE_LEVEL2         = 0xBC,
      RANGE_LEVEL3         = 0xB8,
      RANGE_LEVEL4         = 0xB4,
      RANGE_LEVEL5         = 0xB0,
    };
    


    uint8_t last_status;

    SonarA21(uint8_t _addr = DEFAULT_SONAR_A21_ADDR);
    uint16_t getDistance();
    uint16_t getTemperature();

    uint8_t Read8(uint8_t reg);
    uint16_t Read16(uint8_t reg);
    void Write8(uint8_t reg, uint8_t val);

    bool init();


  private:
     uint8_t AddressI2C;
};
