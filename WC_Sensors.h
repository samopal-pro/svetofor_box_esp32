#pragma once

#include <Wire.h>
#include "MyConfig.h"
#include "WC_Config.h"

//typedef enum {
//   ST_NONE      = 0,
//   ST_RANGE     = 1,   //Датчик дистанции
//   ST_TEMP      = 2,   //Датчик температуры
//   ST_OTHER     = 100  //Прочий тип датчиков

//} SensorValueType_t;



//#if (DEFAULT_SENSOR_TYPE == SENSOR_SR04T )||(DEFAULT_SENSOR_TYPE == SENSOR_SR04TM2 )
#include "src/SonarSR04/SonarSR04.h"
//#include "src/SonarA21/SonarA21.h"
//#elif ( DEFAULT_SENSOR_TYPE == SENSOR_TFLUNA_I2C )
#include "src/TF/TFLI2C.h"
//#include "src/TF/TFMPI2C.h"
#include "src/LD2413/LD2413.h"
//#endif
class MySensorValue {
   public:
      MySensorValue();
      void init(String _label, float _min, float _max, uint16_t _mult, uint16_t _samples);
      void clear();
      bool set( float _val, bool flag = true);
      float getLast();
      String Label;
      float    Value;
      uint16_t Multiplier;   
      float    LimitMin;
      float    LimitMax;
      uint16_t Samples;
   private:
      float    ValueError;
      bool     isFirst;
      uint8_t  Pointer;
      float    *Values;
};

class MySensor {
   public:
      String Name;
      MySensor(SensorType_t _type);
      void checkType(SensorType_t _type);
      bool init();
      bool get();
      uint16_t count_test;
      MySensorValue *Value;
      SensorType_t Type;
   private:
      bool isInit;
      SonarSR04 *SensorSR04;
      SonarSR04 *SensorSR04M2;
      SonarSR04 *SensorSR04_75;
      TFLI2C    *SensorTFLuna;
   //   TFMPI2C   *SensorTFMini;
      LD2413    *SensorLD2413;
      bool checkI2C(uint8_t _addr);   
      void scanI2C();
};