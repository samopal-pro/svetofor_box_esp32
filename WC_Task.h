#pragma once

#include "MyConfig.h"
#include "WC_Config.h"
#include "WC_Sensors.h"
//#include "WC_Led.h"
#include "WC_HTTP.h"
#include "WC_Event.h"
#include "WC_Net.h"
//#include "src/Slib/SButton.h"
#include "src/Slib/SBTN.h"
//#include <WiFi.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <soc/efuse_reg.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp32-hal-touch.h>

#define DEPTH_DIST_ARRAY 5
#define SAMPLE_LEN       10
#define RELIABILITY_PROC 0.15
#define FPSerial Serial1





extern TEventRGB *EventRGB1, *EventRGB2;
extern TEventMP3 *EventMP3;
extern bool isSensorBlock;
extern CALIBRATION_MODE_t calibrMode;
extern uint16_t bootCount;
extern char strID[];
extern char serNo[];
extern uint64_t chipID;
extern SENSOR_STAT_t SensorOn, lastSensorOn;
extern bool isSendNet;
extern int MP3_ADD_DIR;
extern bool isNumStatRGB;
extern float Distance;

void tasksStart();
void taskEvents(void *pvParameters);
void taskSensors(void *pvParameters);
void taskButton(void *pvParameters);
void taskMP3(void *pvParameters);
void taskRGB(void *pvParameters);
void setVolumeMP3();

//void baseMP3(int)

//void playMP3_1(int dir, int num, bool isWait=false, uint32_t delay=30);
//void stopMP3_1();
//bool checkPlayMP3_1(char *check, int num, bool isWait=false, uint32_t delay=30);
//void waitMP3(uint32_t _delay);
//void setEventMP3( bool _enable, uint32_t _delayOn, int _dir, int _sound, bool _loop, uint32_t _color, uint32_t _tm);
//void setEventMP3( JsonObject _config, bool is_delay = true );
void baseMP3( const char * _config, bool is_delay = true );
void systemMP3( char *_check, int _num, int _priority );
void playMP3(int _dir, int _num, int _priority=DAFAULT_PRIORITY_MP3, uint32_t _color=COLOR_MP3_1);
void waitMP3(uint32_t _delay=DEFAULT_TIMER_MP3);
void waitMP3andReboot();
void readID();

void handleSensor( bool _flag  );
void handleRelay1( bool _flag  );
void handleRelay2( bool _flag  );
void handleRGB1(   bool _flag  );
void handleRGB2(   bool _flag  );
void handleMP3(    bool _flag  );
void handleBusy1(  bool _flag  );
void handleBusy2(  bool _flag  );
void handleBtnAdd1(  bool _flag  );
void handleBtnAdd2(  bool _flag  );
void handleCalibrate(bool _flag);
void resetRelay();


void btnPress(uint16_t _count, int _num);
bool btnRelease(uint32_t _tm, int _num);
void btnTimer(uint32_t _tm, int _num);
void btnTimer2(uint32_t _tm, int _num);
void btnTimerCount();

void startCalibrate(uint32_t _delay, char *_check=NULL, int _num=-1, CALIBRATION_MODE_t _mode=CM_WAIT_MP3 );

void setEventRGB1(TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff, uint32_t _color1, uint32_t _color2);
void setEventRGB2(TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff, uint32_t _color1, uint32_t _color2);
void setNanMode();



void checkChangeOn();
void processRelay1();
void processRelay2();
void  setRelay1( bool stat);
void  setRelay2( bool stat);
void  setRelayPin(uint8_t pin, bool stat, bool is_inverse);
void printStat(char *msg);
void checkConfig();
void setStatusRGB1();
//bool ProcessingCalibrate(uint32_t _tm);
//float CalibrateGround();