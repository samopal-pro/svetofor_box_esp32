#pragma once

#include "SBTN.h"
#include <esp_intr_alloc.h>

#ifdef ESP32

//-----------------------------------------------------------------------------
// Класс SBTN_ESP32 - наследник SBTN с поддержкой прерываний и FreeRTOS
//-----------------------------------------------------------------------------
class SBTN_ESP32 : public SBTN {
public:
   // Конструктор для GPIO кнопки
   SBTN_ESP32(uint8_t _pin, uint32_t _bounce = 250, bool _pullup_state = true, bool _press_state = LOW);
   
   // Конструктор для Touch кнопки
   SBTN_ESP32(uint8_t _pin, uint16_t _threshold, uint32_t _bounce = 250);
   
   ~SBTN_ESP32();

   // Запуск/остановка задачи с прерываниями
   void begin(uint32_t stackSize = 2048, UBaseType_t priority = 5);
   void stop();

   // Управление прерываниями (для внешнего использования)
   void enableInterrupt();
   void disableInterrupt();

private:
   SemaphoreHandle_t semaphore;
   TaskHandle_t taskHandle;
   bool isRunning;
   uint32_t taskStackSize;
   UBaseType_t taskPriority;

   // Статические методы для поиска экземпляров
   static SBTN_ESP32* instances[10];
   static uint8_t instancesCount;
   
   void registerInstance();
   void unregisterInstance();
   static SBTN_ESP32* findInstanceByPin(uint8_t pin);

   // Задача FreeRTOS
   static void taskWrapper(void* params);
   void taskLoop();

   // Обработчики прерываний (IRAM)
   static void IRAM_ATTR isrHandler(void* arg);
   static void IRAM_ATTR isrTouchHandler(void* arg);
};

#endif // ESP32


