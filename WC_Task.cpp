#include "WC_Task.h"

int MP3_ADD_DIR = MP3_SYSTEM_FULL_DIR;

MySensor *sensor;
TEvent *EventSensor, *EventRelay1, *EventRelay2;
TEvent *EventBusy1, *EventBusy2;
TEvent *EventBtnAdd1, *EventBtnAdd2;
TEventRGB *EventRGB1, *EventRGB2, *SaveRGB1, *SaveRGB2;

#if defined(IS_BTN_ADD)
   SBTN btn_add(PIN_BTN_ADD,250,false);
#endif


TEventMP3 *EventMP3 = NULL;
TEvent *EventCalibrate; 

//bool isDFPlayer = false;
bool isPlayMP3  = false;
bool isNumStatRGB = true;
//CMD_MP3_t cmdMP3 = CMP3_NONE;
//int arg1MP3 = 0, arg2MP3 = 0;

bool isChangeConfig = true;
float Distance = NAN, lastDistance = NAN;
SENSOR_STAT_t SensorOn = SS_NONE, lastSensorOn = SS_NONE;  
ES_STAT stat1 = STAT_OFF, stat2 = STAT_OFF;
uint32_t msStat1  = 0, msStat2 = 0;
bool statRelay1 = false, statRelay2 = false;
bool inverseRelay1 = false, inverseRelay2 = false;
uint16_t eventRelay1 = 0, eventRelay2 = 0;
uint32_t msRelay1 = 0, msRelay2 = 0;

char calibrCheck[5], calibrNum = -1;

CALIBRATION_MODE_t calibrMode = CM_NONE;
float calibrAvg = 0;
uint16_t calibrCount = 0, calibrError = 0;

uint32_t ms0 = 0, ms1 = 0;

bool isSensorBlock = false; //Полная блокировка сенсора до перезагрузки. Работают только звук и свет

bool isChangeNan   = true;
bool isChangeStat  = false; //Изменение отслеживания изменения состояния для немедленной отправки 
uint16_t bootCount;
SemaphoreHandle_t sensorSemaphore, soundSemaphore/*, bootSemaphore */;
uint32_t colorMP3   = COLOR_SAVE;

bool isSendNet = false; // Флаг отсылки парамтров на сервер

/**
 * Старт всех параллельных задач
 */
void tasksStart() {
  Serial.print(F("!!! Free mem: "));
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  readID();
  configInit();
  bootCount = saveCount(); 
  
  //    xTaskCreateUniversal(taskLed, "led", 2048, NULL, 2, NULL,CORE);
  sensorSemaphore = xSemaphoreCreateMutex();
  soundSemaphore  = xSemaphoreCreateMutex();
//  bootSemaphore   = xSemaphoreCreateMutex();

   EventSensor         = new TEvent(0,0,handleSensor);
   EventRelay1         = new TEvent((uint32_t)(config["config1"]["TM_ON_RELAY1"].as<float>()*1000),(uint32_t)(config["config1"]["TM_OFF_RELAY1"].as<float>()*1000),handleRelay1);
   EventRelay2         = new TEvent((uint32_t)(config["config1"]["TM_ON_RELAY2"].as<float>()*1000),(uint32_t)(config["config1"]["TM_OFF_RELAY2"].as<float>()*1000),handleRelay2);
   EventBusy1          = new TEvent(config["config3"]["MP3_BUSY1_DELAY"].as<uint32_t>(),0,handleBusy1);
   EventBusy2          = new TEvent(config["config3"]["MP3_BUSY2_DELAY"].as<uint32_t>(),0,handleBusy2);

   EventBtnAdd1        = new TEvent(config["config3"]["MP3_BTN_ADD1_DELAY"].as<uint32_t>()*1000,0,handleBtnAdd1);
   EventBtnAdd2        = new TEvent(config["config3"]["MP3_BTN_ADD2_DELAY"].as<uint32_t>()*1000,0,handleBtnAdd2);
   EventCalibrate      = new TEvent(0,0,handleCalibrate);

   EventRGB1           = new TEventRGB(PIN_RGB1, COUNT_RGB1, 2);
   EventRGB2           = new TEventRGB(PIN_RGB2, COUNT_RGB2, 0);
   isChangeConfig = true;
   checkConfig();
   setStatusRGB1();
   Serial.print(F("!!! Free mem: "));
   Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
   FPSerial.begin(9600, SERIAL_8N1, PIN_RX1, PIN_TX1);
   EventMP3 = new TEventMP3(Serial1, DEFAULT_MP3_GPIO, PIN_I2C_SDA, handleMP3);


   xTaskCreateUniversal(taskEvents, "events", 4096, NULL, 3, NULL, CORE);
   xTaskCreateUniversal(taskRGB, "rgb", 2048, NULL, 2, NULL, CORE);
   xTaskCreateUniversal(taskMP3, "mp3", 2048, NULL, 1, NULL, CORE);

    // Создание задачи управления WiFi
   xTaskCreateUniversal(
        taskWiFiManager,    // Функция задачи
        "wifi_mgr",         // Имя задачи
        4096,               // Размер стека
        NULL,               // Параметры
        1,                  // Приоритет
        NULL,               // Handle задачи
        0                   // Ядро 0 (где работает WiFi стек)
    );
    
    // Создание задачи отправки HTTP
   xTaskCreateUniversal(
        taskHttpSender,     // Функция задачи
        "http_sender",      // Имя задачи
        8192,               // Размер стека (больше для JSON операций)
        NULL,               // Параметры
        2,                  // Приоритет (выше чем WiFi менеджер)
        NULL,               // Handle задачи
        1                   // Ядро 1 (свободное ядро)
    );

    // Создание задачи отправки HTTP
   xTaskCreateUniversal(
        taskHttpServer,     // Функция задачи
        "http_server",      // Имя задачи
        4096,               // Размер стека (больше для JSON операций)
        NULL,               // Параметры
        3,                  // Приоритет (выше чем WiFi менеджер)
        NULL,               // Handle задачи
        1                   // Ядро 1 (свободное ядро)
    );

    

//   xTaskCreateUniversal(taskNet, "net", 4096, NULL, 3, NULL, CORE);
   vTaskDelay(500);
   xTaskCreateUniversal(taskSensors, "sensors", 10000, NULL, 4, NULL, CORE);
   vTaskDelay(500);
   xTaskCreateUniversal(taskButton, "btn", 4096, NULL, 4, NULL,CORE);
   vTaskDelay(500);
   xTaskCreateUniversal(taskDebug, "debug", 2048, NULL, 1, NULL,CORE);

   
//  vTaskDelay(500);
//   xTaskCreateUniversal(taskLora, "lora", 10000, NULL, 2, NULL, CORE);
}

/**
 * Задача работы с событиями
 * @param pvParameters
 */
void taskEvents(void *pvParameters) {
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Events task start"));
#endif

//   Serial.printf("!!! EventRelay1 Init %d %d \n",(int)EventRelay1->Type,(int)EventRelay1->State);
   while (true) {
      uint32_t ms = millis();
      EventSensor->loop();
//      if( EventSensor->changeState()  )Serial.printf("!!! EventSensor Loop %d %d \n",(int)EventSensor->Type,(int)EventSensor->State);
      EventRelay1->loop();
      if( EventRelay1->changeState()  )Serial.printf("!!! EventRelay1 Loop %d %d \n",(int)EventRelay1->Type,(int)EventRelay1->State);
      EventRelay2->loop();
      if( EventRelay2->changeState()  )Serial.printf("!!! EventRelay2 Loop %d %d \n",(int)EventRelay2->Type,(int)EventRelay2->State);
//      EventRGB1->loop();
//      EventRGB2->loop();
      EventCalibrate->loop();
      EventBusy1->loop();
      EventBusy2->loop();
      EventBtnAdd1->loop();
      EventBtnAdd2->loop();
      vTaskDelay(250);
   }
}

/**
 * Задача работы с MP3
 * @param pvParameters
 */
void taskMP3(void *pvParameters) {
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! MP3 task start"));
#endif
   isPlayMP3 = true;
   EventMP3->setVolume(config["config3"]["MP3_VOLUME"].as<int>());

   while (true) {
      if( EventMP3->loop() == ES_NONE )isPlayMP3 = false;
      else isPlayMP3 = true;
      vTaskDelay(1000);
   }
}

/**
 * Задача работы с RGB
 * @param pvParameters
 */
void taskRGB(void *pvParameters) {
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! RGB task start"));
#endif
   uint32_t ms1 = 0;
   while (true) {
      uint32_t _ms = millis();
      TEVENT_RGB_TYPE_t t = EventRGB1->loop();
      EventRGB2->loop();
      if( t != ERT_RAINBOW )vTaskDelay(250);
      else vTaskDelay(25);
//      if( ms1 == 0 || ms1 > _ms || _ms-ms1 > 1000){
//          ms1 = _ms;
//      Serial.printf("!!! Rgb status %d %d\n", t, (int)EventRGB1->isRainbow);
//      }

   }
}




/**
 * Задача работы с сенсорами
 * @param pvParameters
 */
void taskSensors(void *pvParameters) {
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Sensors task start"));
#endif
   sensor = new MySensor((SensorType_t)config["config2"]["SENSOR_TYPE"].as<int>());
   sensor->init();
   pinMode(PIN_OUT1,OUTPUT);
   setRelay1(statRelay1); 
   pinMode(PIN_OUT2,OUTPUT);
   setRelay1(statRelay2); 
//   checkPlayMP3("100",100);

// Стартовое приветсвие
   EventRGB1->setRainbow(true);
   EventRGB2->setRainbow(true);
   uint32_t _ms1 = millis();
   waitMP3(DEFAULT_TIMER_MP3);
   systemMP3("100", 100, PRIORITY_MP3_MAXIMAL );
   waitMP3(30000);
   uint32_t _ms2 = millis();
   if( _ms2 - _ms1 < 3000 )vTaskDelay( 3000 - (_ms2-_ms1) );
   EventRGB1->setRainbow(false);
   EventRGB2->setRainbow(false);

   EventRGB1->set(COLOR_NAN,COLOR_NAN);
   EventRGB2->set(COLOR_NAN,COLOR_NAN);

// Если самый первый старт
   if( bootCount < 1 ){
      vTaskDelay(1000);
      systemMP3("99",99,PRIORITY_MP3_MAXIMAL);
      startCalibrate(0,"97",97,CM_WAIT_WAIT);
//      vTaskDelay(jsonConfig["MP3"]["99"]["COLOR_TM"].as<uint32_t>()*1000);
//      startCalibrate(jsonConfig["CALIBR"]["DELAY_START"].as<uint32_t>()*1000);
   }
   else {
      Distance = jsonSave["DISTANCE"].as<float>();
      SensorOn = (SENSOR_STAT_t)jsonSave["STATE_ON"].as<int>();
   }

   while (true) {
      uint32_t ms = millis();
      if(isSensorBlock){ //Сенсор заблокирован до выключения питания
         Serial.println(F("!!! Sensor blocked. Wait reboot ..."));
         vTaskDelay(5000);    
         continue;
      }

      xSemaphoreTake(sensorSemaphore, portMAX_DELAY);
      uint16_t _status = (uint16_t)sensor->get();
      sensor->checkType((SensorType_t)config["config2"]["SENSOR_TYPE"].as<int>());
      Distance = sensor->Value->getLast();
      if( calibrMode == CM_NONE ){
         if( isnan(Distance) ){ setNanMode(); }
         else {
            if( ( config["config"]["MEASURE_TYPE"].as<int>() == INSTALL_TYPE_NORMAL &&
                abs(config["config"]["GROUP_LEVEL"].as<float>() - Distance ) < config["config"]["LIMIT_DISTANCE"].as<float>() )|| 
                ( config["config"]["MEASURE_TYPE"].as<int>() == INSTALL_TYPE_OUTSIDE &&
                ( (Distance < config["config"]["MIN_DISTANCE1"].as<float>() )||(Distance > config["config"]["MAX_DISTANCE1"].as<float>() )))||
                ( config["config"]["MEASURE_TYPE"].as<int>() == INSTALL_TYPE_INSIDE &&
                ( (Distance < config["config"]["MIN_DISTANCE2"].as<float>() )||(Distance > config["config"]["MAX_DISTANCE2"].as<float>() )))){            
               SensorOn = SS_FREE;
            }
            else { SensorOn = SS_BUSY;  }
            isChangeNan = true;
         }
         checkChangeOn();
      }

// Режим калибровки
      else if( calibrMode == CM_ON){ 
         int calibrMax =  config["config"]["NUM_CALIBR"].as<int>();
//         if( calibrCount >= calibrMax )EventCalibrate->off();
//         if( calibrCount + calibrError >= calibrMax*3 )EventCalibrate->off();
// Исправление 22.04.26 
         if( calibrCount + calibrError >= calibrMax )EventCalibrate->off();
         Serial.printf("!!! Calibrate %d %d\n",(int)Distance,calibrCount);
         if( !isnan(Distance) ){
             calibrAvg += Distance;
             calibrCount++;
         }
         else {
             calibrError++;
         }
      }     
      else if( calibrMode == CM_WAIT_WAIT ){
         if( EventMP3->State == ES_NONE ){
            Serial.printf("!!! Wait Wait clibrate");
            if( calibrNum >0 )systemMP3(calibrCheck,calibrNum,PRIORITY_MP3_MAXIMAL);
            startCalibrate(0);
         } 

      }
      else if( calibrMode == CM_WAIT_MP3 ){
         Serial.printf( "!!! Calibr Wait MP3 %d\n",EventMP3->State);
         if( EventMP3->State == ES_NONE ){
            Serial.printf("!!! Start clibrate");
            if( calibrNum >0 )systemMP3(calibrCheck,calibrNum,PRIORITY_MP3_MAXIMAL);
            EventCalibrate->reset();
            EventCalibrate->setType(ET_NORMAL);
            EventCalibrate->on(0);            
         } 
      }
      else if( calibrMode == CM_WAIT_END_MP3 ){
         Serial.printf( "!!! Calibr End Wait MP3 %d\n",EventMP3->State);
         if( EventMP3->State == ES_NONE ){
            calibrMode   = CM_NONE;
            lastSensorOn = SS_NONE;
         } 
      }
      else if( calibrMode == CM_WAIT_REBOOT ){
         if( EventMP3->State == ES_NONE ){
             ESP.restart();
         }
      }
      if( ms0 == 0 || ms0 > ms || ms-ms0 > 5000 ){
         ms0 = ms;
         printStat("TM");
      } 
// Если поменялая яркость лент и громкость звуков
      checkConfig();
// Если поменялись номера статусных светодиодов
      setStatusRGB1();      
      xSemaphoreGive(sensorSemaphore);
      vTaskDelay((uint32_t)(config["config"]["TM_LOOP"].as<float>()*1000));
  }
}

/**
* Обработччик события срабатывания сенсора
*/
void handleSensor(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.printf("!!! EventSensor %d %d %d\n",(int)_flag,(int)EventSensor->Type,(int)EventSensor->State);
#endif
   EventRelay1->setType((TEVENT_TYPE_t)config["config1"]["MODE_RELAY1"].as<int>(),(uint32_t)(config["config1"]["TM_PULSE_RELAY1"].as<float>()*1000),(uint32_t)(config["config1"]["TM_PAUSE_RELAY1"].as<float>()*1000));
   EventRelay2->setType((TEVENT_TYPE_t)config["config1"]["MODE_RELAY2"].as<int>(),(uint32_t)(config["config1"]["TM_PULSE_RELAY2"].as<float>()*1000),(uint32_t)(config["config1"]["TM_PAUSE_RELAY2"].as<float>()*1000));
   if( _flag ){
      EventRelay1->on();
      EventRelay2->on();
   }
   else {
      EventRelay1->off();
      EventRelay2->off();
   }
}

/**
* Обработчик события включения/выключение Реле1
*/
void handleRelay1(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsRelay1 "));
   Serial.println((int)_flag);
#endif
   setRelay1(_flag);
}

/**
* Обработчик события включения/выключение Реле2
*/
void handleRelay2(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsRelay2 "));
   Serial.println((int)_flag);
#endif
   setRelay2(_flag);
}

void resetRelay(){
   Serial.printf("!!! Reset Relay\n");

   switch(SensorOn){
      case SS_BUSY:
         EventRelay1->reset();
         EventRelay2->reset();
         setRelay1(false);
         setRelay2(false);
         handleSensor(true);
        break;
      case SS_FREE:   
         EventRelay1->reset();
         EventRelay2->reset();
         setRelay1(false);
         setRelay2(false);
         handleSensor(false);
         break;
   }
}


/**
* Обработчик события работы с DFPlayer Mini
*/
void handleMP3(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsMP3 "));
   Serial.println((int)_flag);
#endif

   if( _flag ){
      EventRGB2->setMP3(EventMP3->Color);
   }
   else {
      EventRGB2->setMP3(COLOR_NONE);
   }
}

/**
* Обработчик события работы с RGB1
*/
void handleBusy1(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsBUSY1 "));
   Serial.println((int)_flag);
#endif
   if( _flag ){
      if( config["config3"]["MP3_BUSY1_ENABLE"].as<bool>() )baseMP3("BUSY1",false);
   }
}

/**
* Обработчик события работы с RGB1
*/
void handleBusy2(bool _flag){
#if defined(DEBUG_SERIAL)
///   Serial.print(F("!!! EventsBUSY1 "));
///   Serial.println((int)_flag);
#endif
   if( _flag ){
      if( config["config3"]["MP3_BUSY2_ENABLE"].as<bool>() )baseMP3("BUSY2",false);
   }
}

/**
* Обработчик события таймера нажатия на GPIO35
*/
void handleBtnAdd1(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsBTN_ADD1 "));
   Serial.println((int)_flag);
#endif
   if( _flag && (SensorOn == SS_BUSY)){
      if(config["config3"]["MP3_BTN_ADD1_ENABLE"].as<bool>())baseMP3("BTN_ADD1");
   }
   if( _flag && (SensorOn == SS_FREE)){
      if(config["config3"]["MP3_BTN_ADD_FREE1_ENABLE"].as<bool>())baseMP3("BTN_ADD_FREE1");
   }
}

/**
* Обработчик события таймера2 нажатия на GPIO35
*/
void handleBtnAdd2(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! EventsBTN_ADD2 "));
   Serial.println((int)_flag);
#endif
   if( _flag && (SensorOn == SS_BUSY)){
      if(config["config3"]["MP3_BTN_ADD2_ENABLE"].as<bool>())baseMP3("BTN_ADD2");
   }
   if( _flag && (SensorOn == SS_FREE)){
      if(config["config3"]["MP3_BTN_ADD_FREE2_ENABLE"].as<bool>())baseMP3("BTN_ADD_FREE2");
   }
}

/**
* Начало калибровки через событие
*/
void startCalibrate(uint32_t _delay, char *_check, int _num, CALIBRATION_MODE_t _mode){
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Calibrate Wait ... "));
#endif
//   EventMP3->reset();
   
   if( _check!= NULL)strncpy(calibrCheck, _check, 4);
   else calibrCheck[0] = '\0';
   calibrNum = _num;
   EventCalibrate->reset();
   calibrMode = _mode;
   if( _mode == CM_WAIT_MP3){
      EventRGB1->set(COLOR_GROUND,COLOR_GROUND,COLOR_BLACK,COLOR_BLACK,250,250);
      EventRGB2->set(COLOR_GROUND,COLOR_GROUND,COLOR_BLACK,COLOR_BLACK,250,250);
   }
//   systemMP3("97",97,PRIORITY_MP3_HIGH);
//   waitMP3();

//   EventCalibrate->reset();
//   EventCalibrate->setType(ET_PULSE,30000,0);
//   EventCalibrate->on(_delay);
//   isPlayMP3 = false;
}

/**
* Обработчик события калибровки
*/
void handleCalibrate(bool _flag){
#if defined(DEBUG_SERIAL)
   Serial.print(F("!!! Calibrate "));
   Serial.println((int)_flag);
#endif
   if( _flag ){
      Serial.println("!!! Start calibrate");
      calibrMode = CM_ON;
      calibrAvg = 0;
      calibrCount = 0;
      calibrError = 0;
      EventRGB1->set(COLOR_GROUND,COLOR_GROUND);
      EventRGB2->set(COLOR_GROUND,COLOR_GROUND);
      isPlayMP3 = false;
   }
   else {
      Serial.println("!!! Stop calibrate");
      if( calibrCount > 0 ){
          bool ret = false;
          if(calibrError < calibrCount){
            systemMP3("97",93,PRIORITY_MP3_MAXIMAL);

            HTTP_sendResponse(WebResponse::combine({
               WebResponse::msg("Датчик успешно откалибровался", "success", 5000),
               WebResponse::reload(5000)
            }));
//
//
//            HTTP_setResponse("Датчик успешно откалибровался","reload5");
          }
          else {
            systemMP3("97",94,PRIORITY_MP3_MAXIMAL);
            HTTP_sendResponse(WebResponse::combine({
               WebResponse::msg("Датчик плохо видит расстояние, но откалибровался", "info", 5000),
               WebResponse::reload(5000)
            }));

//            HTTP_setResponse("Датчик плохо видит расстояние, но откалибровался","reload5");
          }
          EventRGB1->set(COLOR_SAVE,COLOR_SAVE);
          EventRGB2->set(COLOR_SAVE,COLOR_SAVE);
          float x = calibrAvg/calibrCount;
          Serial.printf("!!! Calibrate ground value %d\n",(int)x);
          config["config"]["GROUP_LEVEL"]  = (int)x;
          config["config"]["MIN_DISTANCE1"]    = (int)x;
          config["config"]["MAX_DISTANCE1"]    = (int)x;
          config["config"]["MIN_DISTANCE2"]    = (int)x;
          config["config"]["MAX_DISTANCE2"]    = (int)x;
          configWrite(); 
//          if( ret )vTaskDelay(3000);      
     }
     else {
          systemMP3("97",95,PRIORITY_MP3_MAXIMAL);
          HTTP_sendResponse(WebResponse::combine({
             WebResponse::msg("Датчик не видит расстояние и не откалибровался", "error",5000)
          }));
          
//          HTTP_setResponse("Датчик не видит расстояние и не откалибровался");
          EventRGB1->set(COLOR_NAN,COLOR_NAN);
          EventRGB2->set(COLOR_NAN,COLOR_NAN);
     }
     calibrMode   = CM_WAIT_END_MP3;
//     lastSensorOn = SS_NONE;

     vTaskDelay(250);

   }
}

/**
* Установка режима если датчик NAN
*/
void setNanMode(){
   switch( config["config"]["NAN_MODE"].as<int>() ){
      case NAN_VALUE_IGNORE: 
         SensorOn = SS_NAN;
         if(isChangeNan)Serial.println(F("!!! NAN. Skiping"));
         break;  
      case NAN_VALUE_BUSY:
         SensorOn = SS_NAN;
         if(isChangeNan)Serial.println(F("!!! NAN. BUSY"));
         break;
      case NAN_VALUE_FREE:
         SensorOn = SS_NAN;
         if(isChangeNan)Serial.println(F("!!! NAN. FREE"));
         break;         
   }
   isChangeNan = false;
}



/*
* Проверка состояния сенсора и формирование нудный событий
*/
void checkChangeOn(){
   uint32_t _color1, _color2;
   if( SensorOn == lastSensorOn )return;
   Serial.printf("!!! Stat is change %d %d\n", (int)SensorOn,(int)lastSensorOn);
   isSendNet = true;
   switch(SensorOn){
      case SS_BUSY:
//      case SS_NAN_BUSY:   
         if(lastSensorOn!=SS_RESTORE){   
            EventSensor->on();
            if(EventBusy1->State != ES_WAIT_ON && EventBusy1->State != ES_ON)EventBusy1->on(config["config3"]["MP3_BUSY1_DELAY"].as<uint32_t>()*1000);
            if(EventBusy2->State != ES_WAIT_ON && EventBusy2->State != ES_ON)EventBusy2->on(config["config3"]["MP3_BUSY2_DELAY"].as<uint32_t>()*1000);
            if(lastSensorOn != SS_NAN) baseMP3("BUSY");
         }
//         EventRGB1->set(config["config"]["BUSY_COLOR"].as<uint32_t>(),config["config"]["BUSY_COLOR"].as<uint32_t>());
//         EventRGB2->set(config["config3"]["BUSY_COLOR"].as<uint32_t>(),config["config3"]["BUSY_COLOR"].as<uint32_t>());
         EventRGB1->set(
            String2RGB(config["config"]["BUSY_COLOR"]),
            String2RGB(config["config"]["BUSY_COLOR"]) );
         EventRGB2->set(
            String2RGB(config["config3"]["BUSY_COLOR"]),
            String2RGB(config["config3"]["BUSY_COLOR"]) );
         break;
      case SS_FREE:   
//      case SS_NAN_FREE:
         if(lastSensorOn!=SS_RESTORE){   
            EventSensor->off();
            EventBusy1->reset();
            EventBusy2->reset();
            baseMP3("FREE");
         }
         if( config["config"]["CHECK_FREE_BLINK_COLOR"].as<bool>() )
            EventRGB1->set(
               String2RGB(config["config"]["FREE_COLOR"]),
               String2RGB(config["config"]["FREE_COLOR"]),
               String2RGB(config["config"]["FREE_BLINK_COLOR"]),
               String2RGB(config["config"]["FREE_BLINK_COLOR"]),
               5000,250);
         else EventRGB1->set(
               String2RGB(config["config"]["FREE_COLOR"]),
               String2RGB(config["config"]["FREE_COLOR"]) );  
         if( config["config3"]["CHECK_FREE_BLINK_COLOR"].as<bool>() )
            EventRGB2->set(
               String2RGB(config["config3"]["FREE_COLOR"]),
               String2RGB(config["config3"]["FREE_COLOR"]),
               String2RGB(config["config3"]["FREE_BLINK_COLOR"]),
               String2RGB(config["config3"]["FREE_BLINK_COLOR"]),
               5000,250);
         else EventRGB2->set(
               String2RGB(config["config3"]["FREE_COLOR"]),
               String2RGB(config["config3"]["FREE_COLOR"]) );  
         break;
      case SS_NAN:
      case SS_NAN_FREE:   
      case SS_NAN_BUSY:   
         if(config["config"]["IS_COLOR_NAN"].as<bool>()){
            switch(config["config"]["NAN_MODE"].as<int>()){
               case NAN_VALUE_IGNORE: 
                  _color1 = EventRGB1->Color1;
                  _color2 = EventRGB2->Color1;
//                  setEventRGB1( ET_NORMAL, 0, 0, COLOR_NAN, COLOR_NONE); 
//                  setEventRGB2( ET_NORMAL, 0, 0, COLOR_NAN, COLOR_NONE); 
                  EventRGB1->set( _color1, COLOR_NAN );  
                  EventRGB2->set( _color2, COLOR_NAN );  
                  break;
               case NAN_VALUE_BUSY:   
                  EventRGB1->set( String2RGB(config["config"]["BUSY_COLOR"]), COLOR_NAN); 
                  EventRGB2->set( String2RGB(config["config3"]["BUSY_COLOR"]), COLOR_NAN); 
                  break;
               case NAN_VALUE_FREE:   
                  EventRGB1->set( String2RGB(config["config"]["FREE_COLOR"]), COLOR_NAN); 
                  EventRGB2->set( String2RGB(config["config3"]["FREE_COLOR"]), COLOR_NAN); 
                  break;
            } 
         }
/*         
         if(jsonConfig["RGB2"]["IS_NAN_MODE"].as<bool>()){
            switch(jsonConfig["RGB2"]["NAN_MODE"].as<int>()){
               case NAN_VALUE_IGNORE: setEventRGB2( ET_NORMAL, 0, 0, COLOR_NONE, COLOR_NAN); break;
               case NAN_VALUE_BUSY:   setEventRGB2( ET_NORMAL, 0, 0, jsonConfig["RGB2"]["BUSY"].as<uint32_t>(), COLOR_NAN); break;
               case NAN_VALUE_FREE:   setEventRGB2( ET_NORMAL, 0, 0, jsonConfig["RGB2"]["FREE"].as<uint32_t>(), COLOR_NAN); break;
            } 
         }
         */
         if(lastSensorOn==SS_NONE){
            systemMP3("97",95,PRIORITY_MP3_HIGH);
         }
         else if(lastSensorOn!=SS_RESTORE){   
            if( lastSensorOn == SS_FREE )baseMP3("FREE_NAN");
            else baseMP3("NAN");
         }
         break;
   }
   lastSensorOn = SensorOn;
   isPlayMP3 = false;
   saveSet(Distance,SensorOn);
}



/*
* Установка состояние реле1
*/
void  setRelay1( bool stat){
   bool _stat;
   if( config["config1"]["INVERSE_RELAY1"].as<bool>() )_stat = !stat;
   else _stat = stat;
   Serial.printf("!!! Set Relay1 pin=%d stat=%d\n",(int)PIN_OUT1,(int)_stat);
   digitalWrite(PIN_OUT1, _stat);
// Сохраняем значения
   statRelay1    = stat;
   inverseRelay1 = config["config1"]["INVERSE_RELAY1"].as<bool>();
}

/*
* Установка состояние реле2
*/
void  setRelay2( bool stat){
   bool _stat;
   if( config["config1"]["INVERSE_RELAY2"].as<bool>() )_stat = !stat;
   else _stat = stat;
   Serial.printf("!!! Set Relay2 pin=%d stat=%d\n",(int)PIN_OUT2,(int)_stat);
   digitalWrite(PIN_OUT2, _stat);
// Сохраняем значения
   statRelay2    = stat;
   inverseRelay2 = config["config1"]["INVERSE_RELAY2"].as<bool>();
}


/*
* Установка состояния физического GPIO пина реле
*/
void  setRelayPin(uint8_t pin, bool stat, bool is_inverse){
   bool _stat;
   if( is_inverse )_stat = !stat;
   else _stat = stat;
//   Serial.printf("!!! Set Relay pin=%d stat=%d\n",(int)pin,(int)_stat);
   digitalWrite(pin, _stat);
}

/*
* Вывод на экран состояния сенсора
*/
void printStat(char *msg){
 #if defined(DEBUG_SERIAL)   
   Serial.print(F("!!! Sensor: "));
   Serial.print(Distance, 0);
   Serial.print(F(" Stat: "));
   Serial.print(SensorOn);
   Serial.print(F(" Event: "));
   Serial.print(msg);
   Serial.print(F(" Free mem: "));
   Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}


void taskDebug(void *pvParameters){
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Debug task start"));
#endif
   int n = 0;
   while(true){
//      String s = "KEY"+String(n++);
//      HTTP_sendResponse(WebResponse::combine({
//         WebResponse::config("config2", "TB_TOKEN", s),
//         WebResponse::msg("Изменение токена", "info", 3000)
//      }));
      vTaskDelay(10000);   
   }
}

/*
* Задача работы с кнопкой (герконом)
*/
void taskButton(void *pvParameters){
#if defined(DEBUG_SERIAL)
   Serial.println(F("!!! Button task start"));
#endif
   pinMode(PIN_BTN,INPUT);
   SBTN btn(PIN_BTN);
   btn.setTimer(2000);
   btn.setTimer2(10000);
   btn.setTimerEventCount(4000);
#if defined(IS_TOUCH)
   SBTN_touch btn_t(PIN_TOUCH, TOUCH_THRESHOLD);
   btn_t.setTimer(2000);
   btn_t.setTimer2(10000);
   btn_t.setTimerEventCount(4000);
   Serial.printf("!!! Touch emable pin=%d threshold=%d\n", PIN_TOUCH, TOUCH_THRESHOLD);
#endif
#if defined(IS_BTN_ADD)
   pinMode(PIN_BTN_ADD,INPUT);
   Serial.printf("!!! Add Btn pin=%d \n", PIN_BTN_ADD);
#endif


   bool white_flag = false;
   bool is_early = false;
   uint32_t tm;
   int btn_count;
//   HTTP_begin();
   while(true){
      if(isSensorBlock || calibrMode == CM_WAIT_REBOOT ){ //Сенсор заблокирован до выключения питания
         vTaskDelay(1000);    
         continue;
      }
#if defined(IS_BTN_ADD)
      switch(btn_add.loop()){  
          case SB_PRESS:
             Serial.println(F("!!! press add btn"));
             if(config["config3"]["MP3_BTN_ADD1_ENABLE"].as<bool>() && (SensorOn == SS_BUSY))EventBtnAdd1->on(config["config3"]["MP3_BTN_ADD1_DELAY"].as<uint32_t>()*1000);
             if(config["config3"]["MP3_BTN_ADD2_ENABLE"].as<bool>() && (SensorOn == SS_BUSY))EventBtnAdd2->on(config["config3"]["MP3_BTN_ADD2_DELAY"].as<uint32_t>()*1000);
             
             if(config["config3"]["MP3_BTN_ADD_FREE1_ENABLE"]["ENABLE"].as<bool>() && (SensorOn == SS_FREE))EventBtnAdd1->on(config["config3"]["MP3_BTN_ADD_FREE1_DELAY"].as<uint32_t>()*1000);
             if(config["config3"]["MP3_BTN_ADD_FREE2_ENABLE"]["ENABLE"].as<bool>() && (SensorOn == SS_FREE))EventBtnAdd2->on(config["config3"]["MP3_BTN_ADD_FREE2_DELAY"].as<uint32_t>()*1000);
             break;
          case SB_RELEASE:
             Serial.println(F("!!! release add btn"));
             if(!config["config3"]["BTN_ADD_OFF"].as<bool>()){
                EventMP3->stop();
                EventBtnAdd1->reset();
                EventBtnAdd2->reset();
             }
             break;
//          case SB_TIMER:
//             Serial.println(F("!!! timer add btn"));
//             if(jsonConfig["MP3"]["BTN_ADD"]["ENABLE"].as<bool>())playMP3(MP3_BASE_DIR, jsonConfig["MP3"]["BTN_ADD1"]["NUM"].as<int>(), PRIORITY_MP3_MINIMAL);
//             break;
//          case SB_TIMER2:
//             Serial.println(F("!!! timer2 add btn"));
//             if(jsonConfig["MP3"]["BTN_ADD"]["ENABLE"].as<bool>())playMP3(MP3_BASE_DIR, jsonConfig["MP3"]["BTN_ADD2"]["NUM"].as<int>(), PRIORITY_MP3_MINIMAL);
//             break;
      }
#endif
#if defined(IS_TOUCH)
      switch(btn_t.loop()){
          case SB_PRESS:
             btnPress(btn_t.getCountEvent(), PIN_TOUCH);
             break;
          case SB_RELEASE:
             is_early = btnRelease(btn_t.getPressTime(), PIN_TOUCH);
             break;
          case SB_TIMER:
             btnTimer(btn_t.getPressTime(), PIN_TOUCH);
             break;
          case SB_TIMER2:
             is_early = false;
             btnTimer2(btn_t.getPressTime(), PIN_TOUCH);
             break;
          case SB_TIMER_COUNT:
             if( is_early){
                EventMP3->stop();             
                systemMP3("97",96,PRIORITY_MP3_HIGH);
             }
             is_early = false;   
             break;   
      }
 #endif

      switch(btn.loop()){
          case SB_PRESS:
             is_early = false;
             btnPress(btn.getCountEvent(), PIN_BTN);
             break;
          case SB_RELEASE:
             is_early = btnRelease(btn.getPressTime(), PIN_BTN);
             break;   
          case SB_TIMER:
             btnTimer(btn.getPressTime(), PIN_BTN);
             break;
          case SB_TIMER2:
             is_early = false;
             btnTimer2(btn.getPressTime(), PIN_BTN);
             break;
          case SB_TIMER_COUNT:
             if( is_early){
                EventMP3->stop();             
                systemMP3("97",96,PRIORITY_MP3_HIGH);
             }
             is_early = false;   
             break;   
      }
//     HTTPD_loop();
      vTaskDelay(200);
       
   }

}


void btnPress(uint16_t _count, int _num){
   Serial.printf("!!! BTN Press %d %d\n",_num,(int)_count);
   EventRGB1->set(COLOR_SAVE,COLOR_SAVE,COLOR_BLACK,COLOR_BLACK,250,250);               
   EventRGB2->set(COLOR_SAVE,COLOR_SAVE,COLOR_BLACK,COLOR_BLACK,250,250);               
   if( _count == 5 ){
      systemMP3("92",92,PRIORITY_MP3_HIGH);
      EventRGB1->set(COLOR_SAVE,COLOR_SAVE);               
      EventRGB2->set(COLOR_SAVE,COLOR_SAVE);               
      config["config2"]["ESP_NAME"]        = deviceName();
      configWrite();  
      waitMP3andReboot();
   }   
}

bool btnRelease(uint32_t _tm, int _num){
   bool _ret = false;
   if( _tm >= 2000 && _tm < 10000 ){
      _ret = false;
      systemMP3("97",97,PRIORITY_MP3_HIGH);
      startCalibrate(5000);
      if( bootCount>=1 ){
         if( isAP )isAP = false;
         else isAP = true; 
         }
      }
   else {
      lastSensorOn = SS_RESTORE;
      _ret = true;
   }
   Serial.printf("!!! BTN Release %d %d\n",_num,(int)_tm);
   return _ret;
}

void btnTimer(uint32_t _tm, int _num){
   Serial.printf("!!! BTN Timer %d %d\n",_num,(int)_tm);
   EventRGB1->set(COLOR_GROUND,COLOR_GROUND);               
   EventRGB2->set(COLOR_GROUND,COLOR_GROUND);               
}

void btnTimer2(uint32_t _tm, int _num){
   Serial.printf("!!! BTN Timer2 %d %d\n",_num,(int)_tm);
   EventRGB1->set(COLOR_SAVE,COLOR_SAVE);               
   EventRGB2->set(COLOR_SAVE,COLOR_SAVE);  
   systemMP3("98",98,PRIORITY_MP3_HIGH);
//             jsonSave["BOOT_COUNT"]  = 0;
//             saveSave();
   config["config2"]["ESP_PASS1"]      = config_default["config2"]["ESP_PASS2"];      
   config["config2"]["ESP_PASS2"]      = config_default["config2"]["ESP_PASS2"];      
   config["config2"]["ESP_PASS2"]      = config_default["config2"]["ESP_PASS2"];      
   configWrite();
   vTaskDelay(500);
   waitMP3andReboot();
}

void btnTimerCount(){
}

void setVolumeMP3(){
   EventMP3->setVolume(config["config3"]["MP3_VOLUME"].as<int>());
}




void baseMP3( const char * _config, bool is_delay ){
    char s[32];
    sprintf(s,"MP3_%s_ENABLE",_config);
    bool _enable         = config["config3"][s].as<bool>();

    sprintf(s,"MP3_%s_NUM",_config);
    int _num             = config["config3"][s].as<int>();

    sprintf(s,"MP3_%s_LOOP",_config);
    bool _loop           = config["config3"][s].as<bool>();

    sprintf(s,"BUSY_%s_COLOR",_config);
    uint32_t _color      = config["config3"][s].as<uint32_t>();

    sprintf(s,"BUSY_%s_COLOR_TM",_config);
    uint32_t _ctimer     = config["config3"][s].as<uint32_t>()*1000;

    uint32_t _timer      = DEFAULT_TIMER_MP3 ;
    if( _loop )_timer    = 0;
    uint32_t _delay      = 0;
    sprintf(s,"MP3_%s_DELAY",_config);

    if( is_delay )_delay = config["config3"][s].as<uint32_t>()*1000;
    if(_enable){
//       EventMP3->stop();
       EventMP3->setColor( _color, _ctimer );
       EventMP3->start(MP3_BASE_DIR, _num, PRIORITY_MP3_MINIMAL, _delay, _timer, true, _loop);
    }
}
 
void systemMP3(char *_check, int _num, int _priority ){
   char s[20];
   sprintf(s,"MP3_%s_ENABLE",_check);
    if( !config["config4"][s].as<bool>() )return;
    EventMP3->setColor( COLOR_MP3_1 ,0);
    EventMP3->start(MP3_ADD_DIR, _num, _priority );

}

void playMP3(int _dir, int _num, int _priority, uint32_t _color){
 //   EventMP3->stop();
    EventMP3->setColor( _color ,0);
    EventMP3->start(_dir, _num, _priority);
    
}

void waitMP3(uint32_t _delay){ 
    uint32_t _ms = millis() + _delay;
    Serial.println(F("!!! MP3 start wait ..."));
    while( EventMP3->State != ES_NONE ){
       if( _delay > 0 && _ms < millis() )break;
       vTaskDelay(500);   
    }
    Serial.println(F("!!! MP3 stop wait ..."));
}


void waitMP3andReboot(){
   EventRGB1->set(COLOR_SAVE,COLOR_SAVE,COLOR_BLACK,COLOR_BLACK,250,250);               
   EventRGB2->set(COLOR_SAVE,COLOR_SAVE,COLOR_BLACK,COLOR_BLACK,250,250);     
   calibrMode = CM_WAIT_REBOOT;          
}


void checkConfig(){
   if( isChangeConfig ){
      isChangeConfig = false;
      EventRGB1->setBrightness( config["config"]["BRIGHTNESS"] );
      EventRGB2->setBrightness( config["config3"]["BRIGHTNESS"] );
      if( EventMP3 != NULL )EventMP3->setVolume(config["config3"]["MP3_VOLUME"].as<int>());
      if( config["config3"]["MP3_SHORT_MSG"].as<bool>() )MP3_ADD_DIR = MP3_SYSTEM_SHORT_DIR; 
      else MP3_ADD_DIR = MP3_SYSTEM_FULL_DIR; 
      if( !config["config2"]["WIFI_POWER"].isNull() )WiFi.setTxPower((wifi_power_t)config["config2"]["WIFI_POWER"].as<int>());
#if defined(IS_BTN_ADD)      
      btn_add.PressState = config["config3"]["BTN_ADD_IVERSE"].as<bool>();
#endif
   }
 }

void setStatusRGB1(){
    if ( isNumStatRGB == false )return;
    if ( !config["config"]["STAT_AP"].isNull() ){
        EventRGB1->setNumAp(config["config"]["STAT_AP"].as<int>());
    }
    if ( !config["config"]["STAT_STA"].isNull() ){
        EventRGB1->setNumSta(config["config"]["STAT_STA"].as<int>());
    }
    isNumStatRGB = false;
    
}