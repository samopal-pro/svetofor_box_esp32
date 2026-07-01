#include "WC_Sensors.h"

/**
* Класс MySensorValue
*/
MySensorValue::MySensorValue(){
   init("",0,10000,1,SIMPLE_SIZE);
   ValueError = NAN;
   Values     = (float *)malloc(SIMPLE_SIZE*sizeof(float));
   clear();
}

void MySensorValue::init(String _label, float _min, float _max, uint16_t _mult, uint16_t _samples){
   Label = _label;
   LimitMin   = _min;
   LimitMax   = _max;
   if( _samples >SIMPLE_SIZE )_samples = SIMPLE_SIZE;
   if( _samples == 0 )_samples = 1;
   Samples    = SIMPLE_SIZE;
   Multiplier = _mult;

    
}

void MySensorValue::clear(){
   for( int i=0; i<Samples; i++)Values[i] = NAN;
   isFirst       = true;
   Pointer       = 0;
   Value         = NAN;
}

bool MySensorValue::set(float _val, bool _flag){
   if( !isnan(_val) )_val *= Multiplier;
   if( isnan(_val) || _val < LimitMin || _val > LimitMax ){
      Value = NAN;
      return false;  
   }
   Value = _val;
   if( !_flag )return false;
   if( isFirst ){
      isFirst = false;
      for( int i=0; i<Samples; i++)Values[i] = _val;
   }
   else {
      Values[Pointer++] = _val;
      if( Pointer >= Samples )Pointer = 0;
   }
   return true;
}


float MySensorValue::getLast(){
   if( isFirst )return ValueError;
   if( isnan(Value ) )return ValueError;
   return ( Value);
}



/**
**************************************************************************************************************************************************************************************
* Класс MySensor
*/

MySensor::MySensor(SensorType_t _type){
 
   SensorSR04    = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 10);
   SensorSR04M2  = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 500);
   SensorSR04_75 = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 500);
   SensorTFLuna  = new TFLI2C();
//   SensorTFMini  = new TFMPI2C();
   SensorLD2413  = new LD2413();
   Value         = new MySensorValue();
   isInit       = false;
   checkType(_type);
}

void MySensor::checkType(SensorType_t _type){
   if( _type != Type ){
      Serial.printf("!!! Type sensor %d %d\n",_type,Type);
      isInit        = false;
      Type          = _type;
   }
}

bool MySensor::init(){
   switch(Type){
      case SENSOR_SR04T :  
         Value->init("Дистанция, мм", 100.0, 4900.0, 1, SIMPLE_SIZE );
         SensorSR04->init();
         isInit = true;
         Name   = "Sonar SR04TM2";
         break;
      case SENSOR_SR04TM2 :  
         Value->init("Дистанция, мм", 100.0, 5900.0, 1, SIMPLE_SIZE );
         SensorSR04M2->init();
         isInit = true;
         Name   = "Sonar SR04TM2";
         break;
      case SENSOR_SR04_75 :  
         Value->init("Дистанция, мм", 100.0, 7400.0, 1, SIMPLE_SIZE );
         SensorSR04M2->init();
         isInit = true;
         Name   = "Sonar SR04_75";
         break;
      case SENSOR_TFLUNA_I2C:
         Wire.begin(PIN_SONAR_ECHO, PIN_SONAR_TRIG);
         Wire.setClock(100000);
         vTaskDelay(200);
         isInit = checkI2C(TFL_DEF_ADR);
         Name = "Lidar TFLuna";
         break;
//      case SENSOR_TFMINI_I2C:
//         Wire.begin(PIN_SONAR_ECHO, PIN_SONAR_TRIG);
//         Wire.setClock(100000);
//         isInit = checkI2C(TFMP_DEFAULT_ADDRESS);
//         Name = "Lidar TFMiniPlus";
//         break;
      case SENSOR_LD2413_UART :
         Serial2.begin(115200,SERIAL_8N1,PIN_SONAR_TRIG,PIN_SONAR_ECHO);
         SensorLD2413->begin(PIN_SONAR_TRIG,PIN_SONAR_ECHO);
         SensorLD2413->init(150, 10000, 250);
         isInit = true;
         Name = "Radar LD2413";
   }

#if (DEBUG_SENSORS>0)
   if( isInit )Serial.print(F("!!! Init normal "));
   else Serial.print(F("??? Init fail "));
   Serial.println(Name);
#endif
   return isInit;
}


bool MySensor::get(){
   float value1 = NAN, value2 = NAN;
   bool stat = false;
   if( !isInit )init();
   if( !isInit )return false;
   static int16_t _dist16, _flux16, _temp16;
   switch(Type){
      case SENSOR_SR04T:
         value1 = SensorSR04->getDistance();
         stat = Value->set(value1,true);
         break;
      case SENSOR_SR04TM2:
         value1 = SensorSR04M2->getDistance();
         stat = Value->set(value1,true);
         break;
      case SENSOR_SR04_75:
         value1 = SensorSR04_75->getDistance();
         stat = Value->set(value1,true);
         break;
      case SENSOR_TFLUNA_I2C:    
         stat = SensorTFLuna->getData(_dist16, _flux16, _temp16, TFL_DEF_ADR );
         if( stat ) value1 = (float)_dist16;
         else value1 = NAN;
         Value->set(value1, stat);
         break;
  //    case SENSOR_TFMINI_I2C:    
  //       stat = SensorTFMini->getData(_dist16, _flux16, _temp16, TFMP_DEFAULT_ADDRESS );
  //       if( stat ) value1 = (float)_dist16;
  //       else value1 = NAN;
  //       Value->set(value1, stat);
  //       break;
      case SENSOR_LD2413_UART :
         value1 = SensorLD2413->wait_data();
         if( value1!= 0 )Value->set(value1);
         break;
      default:
         Value->set(NAN);
         stat = false;   
   }
//#endif
#if (DEBUG_SENSORS>1)
   Serial.print(F("!!! "));
   Serial.print(Name);
   Serial.print(F(" = "));
   Serial.println(Value->getLast());
//   Serial.println(value1);
#endif 

return stat;
}

/**
// Проверка I2C адреса
*/
bool MySensor::checkI2C(uint8_t _addr){
   bool _ret = false;
   Wire.beginTransmission(_addr);
   if( Wire.endTransmission() == 0 )_ret = true;
   else _ret = false;
#ifdef DEBUG_SENSORS
   Serial.print(F("!!! I2C: ADDR=0x"));
   Serial.print(_addr,HEX);   
   if( _ret )Serial.println(F(" OK"));
   else Serial.println(F(" False"));
#endif  
   return _ret;
}

void MySensor::scanI2C(){
  int nDevices = 0;
  for(uint8_t address = 1; address < 127; address++ ) {
     Wire.beginTransmission(address);
     int error = Wire.endTransmission();
     switch(error){
         case 0 :
            Serial.printf(F("!!! I2C device found at address 0x"));
            if (address<16)Serial.print("0");
            Serial.println(address,HEX);
            nDevices++;
            break;
         case 4:
            Serial.print("Unknow error at address 0x");
            if (address<16)Serial.print("0");
            Serial.println(address,HEX);
            break;
      }    
  }
  if (nDevices == 0)Serial.println(F("No I2C devices found\n"));
}