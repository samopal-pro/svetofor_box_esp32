
#include "SonarA21.h"

// Constructors ////////////////////////////////////////////////////////////////

SonarA21::SonarA21(uint8_t _addr){
    AddressI2C = _addr;
}


uint16_t SonarA21::getDistance(){
  Write8(COMMAND,           RANGE_LEVEL4);
  delay(120);
  return(Read16(DISTANCE));
}

uint16_t SonarA21::getTemperature(){
//  Write8(COMMAND,           RANGE_LEVEL4);
//  delay(120);
  return(Read16(TEMPERATURE));
}


// Init Sonar
bool SonarA21::init()
{
  Wire.beginTransmission(AddressI2C);
//  delay(120);
  last_status = Wire.endTransmission();
  if( last_status != 0 )return false;
  Write8(HOLD,              0x01);
//  delay(120);
  Serial.println(1);
  Write8(POWER_NOISE_LEVEL, 0x04);
//  delay(120);
  Serial.println(2);
  Write8(ANGLE_LEVEL,       0x01);
//  delay(120);
  Serial.println(3);
  Write8(COMMAND,           RANGE_LEVEL4);
//  delay(150);
  Serial.println(4);
return true;
}

uint8_t SonarA21::Read8(uint8_t reg) {
    Wire.beginTransmission(AddressI2C);
    Wire.write(reg);
    last_status = Wire.endTransmission();

    Wire.requestFrom(AddressI2C, 1);
    // return 0 if slave didn't response
    if (Wire.available() < 1) {
        return 0;
    } 
    return Wire.read();
}

uint16_t SonarA21::Read16(uint8_t reg) {
    Wire.beginTransmission(AddressI2C);
    Wire.write(reg);
    last_status = Wire.endTransmission();

    Wire.requestFrom(AddressI2C, 2);
    // return 0 if slave didn't response
    if (Wire.available() < 2) {
        return 0;
    } 
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    return (uint16_t) msb << 8 | lsb;
}

void SonarA21::Write8(uint8_t reg, uint8_t val) {
//    delay(120);
    Wire.beginTransmission(AddressI2C); // start transmission to device
    Wire.write(reg);       // send register address
    Wire.write(val);         // send value to write
    last_status = Wire.endTransmission();     // end transmission
}

//void SonarA21::Write16(uint8_t reg, uint8_t val) {
//    Wire.beginTransmission(I2C_ADDR); // start transmission to device
//    Wire.write(reg);       // send register address
//    Wire.write(val);         // send value to write
//    Wire.write(val);         // send value to write
//    Wire.endTransmission();     // end transmission
//}
