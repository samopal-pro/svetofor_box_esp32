#pragma once

#include <Arduino.h>

#ifdef ESP32
  #include <esp32-hal-touch.h>
#endif

// Перечисление событий
typedef enum {
   SB_NONE        = 0,
   SB_PRESS       = 1,
   SB_RELEASE     = 2,
   SB_TIMER       = 3,
   SB_TIMER2      = 4,
   SB_TIMER_COUNT = 5
} SBTN_EVENT_t;

// Структура для передачи данных в обработчик
struct SBTN_Context {
   uint16_t pin;        // номер пина
   uint16_t count;      // счётчик нажатий
   uint32_t pressTime;  // время удержания (мс)
   bool isEarly;        // флаг для особых случаев
};

//-----------------------------------------------------------------------------
// Универсальный класс SBTN (объединяет все типы кнопок)
//-----------------------------------------------------------------------------
class SBTN {
   public:
      // Конструктор для GPIO кнопки
      SBTN(uint8_t _pin, uint32_t _bounce = 250, bool _pullup_state = true, bool _press_state = LOW);
      
      // Конструктор для Touch кнопки (ESP32)
      #ifdef ESP32
      SBTN(uint8_t _pin, uint16_t _threshold, uint32_t _bounce = 250);
      #endif
      
      ~SBTN();

      // Основные методы
      SBTN_EVENT_t loop();
      bool isPress();

      // Установка единого callback-обработчика
      void setCallback(void (*func)(SBTN_Context* ctx, SBTN_EVENT_t event));

      // Управление параметрами
      void setBounceTM(uint32_t _tm);
      void setDebug(bool flag = true);
      void setTimer(uint32_t _tm);
      void setTimer2(uint32_t _tm);
      void setTimerEventCount(uint32_t _tm);
      void setTimerCountEnable(bool enable);

      // Получение информации
      uint16_t getCountEvent() const { return count_event; }
      uint16_t getNumberBtn() const  { return number_btn; }
      uint32_t getPressTime() const  { return ms_delta; }
      uint8_t getPin() const         { return pin; }

      // Управление флагом isEarly (для внешнего доступа)
      void setEarly(bool val) { context.isEarly = val; }
      bool getEarly() const   { return context.isEarly; }
      void resetEarly()       { context.isEarly = false; }

   private:
      // Параметры кнопки
      uint8_t pin;
      bool isTouch;
      uint16_t threshold;
      bool pressState;
      bool pullupState;

      // Состояние кнопки
      uint32_t ms_press;
      bool is_press;
      bool is_timer;
      bool is_timer2;
      uint32_t tm_bounce;
      uint16_t number_btn;
      uint16_t count_event;
      uint32_t ms_delta;
      uint32_t timer_press;
      uint32_t timer_press2;
      uint32_t timer_reset_count_event;
      bool is_debug;
      bool is_timer_count_enabled;

      // Контекст для callback
      SBTN_Context context;

      // Указатель на callback-функцию
      void (*callback)(SBTN_Context* ctx, SBTN_EVENT_t event);

      // Вспомогательные методы
      bool readPin();
      void initCommon();
      static uint16_t btnCounter;
};

