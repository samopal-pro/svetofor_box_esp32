#include "SBTN.h"

uint16_t SBTN::btnCounter = 0;

//-----------------------------------------------------------------------------
// Конструкторы
//-----------------------------------------------------------------------------
SBTN::SBTN(uint8_t _pin, uint32_t _bounce, bool _pullup_state, bool _press_state)
   : pin(_pin), isTouch(false), threshold(0), pressState(_press_state), 
     pullupState(_pullup_state), tm_bounce(_bounce)
{
   initCommon();
   
   // Настройка GPIO
   if (pressState == LOW) {
      if (pullupState) pinMode(pin, INPUT_PULLUP);
      else             pinMode(pin, INPUT);
   } else {
      pinMode(pin, INPUT);
   }
}

#ifdef ESP32
SBTN::SBTN(uint8_t _pin, uint16_t _threshold, uint32_t _bounce)
   : pin(_pin), isTouch(true), threshold(_threshold), pressState(LOW),
     pullupState(false), tm_bounce(_bounce)
{
   initCommon();
}
#endif

SBTN::~SBTN() {
   // Деструктор
}

//-----------------------------------------------------------------------------
// Общая инициализация
//-----------------------------------------------------------------------------
void SBTN::initCommon() {
   ms_press = 0;
   is_press = false;
   is_timer = false;
   is_timer2 = false;
   number_btn = btnCounter++;
   count_event = 0;
   ms_delta = 0;
   timer_press = 0;
   timer_press2 = 0;
   timer_reset_count_event = 0;
   is_debug = false;
   is_timer_count_enabled = true;
   callback = nullptr;
   
   // Инициализация контекста
   context.pin = pin;
   context.count = 0;
   context.pressTime = 0;
   context.isEarly = false;
}

//-----------------------------------------------------------------------------
// Чтение состояния пина
//-----------------------------------------------------------------------------
bool SBTN::readPin() {
   if (isTouch) {
#ifdef ESP32
      return (touchRead(pin) < threshold);
#else
      return false;
#endif
   } else {
      if (pressState == LOW) return !digitalRead(pin);
      else                   return digitalRead(pin);
   }
}

//-----------------------------------------------------------------------------
// Основной метод опроса
//-----------------------------------------------------------------------------
SBTN_EVENT_t SBTN::loop() {
   bool _press_flag = readPin();
   uint32_t _ms = millis();
   ms_delta = _ms - ms_press;

   if (_press_flag) {
      // Кнопка нажата
      if (!is_press) {
         // Первое нажатие
         is_press = true;
         is_timer = true;
         is_timer2 = true;
         ms_press = _ms;
         count_event++;

         context.count = count_event;
         context.pressTime = 0;

         if (is_debug) {
            Serial.print(F("!!! Press key="));
            Serial.print(number_btn);
            Serial.print(F(" pin="));
            Serial.print(pin);
            Serial.print(F(" event="));
            Serial.print(count_event);
            Serial.println();
         }

         if (callback) callback(&context, SB_PRESS);
         return SB_PRESS;
      }

      // Проверка таймера 1
      if (is_press && timer_press > 0 && is_timer && timer_press < ms_delta) {
         is_timer = false;
         context.pressTime = ms_delta;

         if (is_debug) {
            Serial.print(F("!!! Timer key="));
            Serial.print(number_btn);
            Serial.print(F(" pin="));
            Serial.print(pin);
            Serial.print(F(" event="));
            Serial.print(count_event);
            Serial.print(F(" time="));
            Serial.print(ms_delta);
            Serial.println(F(" ms"));
         }

         if (callback) callback(&context, SB_TIMER);
         return SB_TIMER;
      }

      // Проверка таймера 2
      if (is_press && timer_press2 > 0 && is_timer2 && timer_press2 < ms_delta) {
         is_timer2 = false;
         context.pressTime = ms_delta;

         if (is_debug) {
            Serial.print(F("!!! Timer2 key="));
            Serial.print(number_btn);
            Serial.print(F(" pin="));
            Serial.print(pin);
            Serial.print(F(" event="));
            Serial.print(count_event);
            Serial.print(F(" time="));
            Serial.print(ms_delta);
            Serial.println(F(" ms"));
         }

         if (callback) callback(&context, SB_TIMER2);
         return SB_TIMER2;
      }
   } else {
      // Кнопка отпущена
      if (is_press && (ms_delta >= tm_bounce)) {
         is_press = false;
         context.pressTime = ms_delta;

         if (is_debug) {
            Serial.print(F("!!! Release key="));
            Serial.print(number_btn);
            Serial.print(F(" pin="));
            Serial.print(pin);
            Serial.print(F(" event="));
            Serial.print(count_event);
            Serial.print(F(" time="));
            Serial.print(ms_delta);
            Serial.println(F(" ms"));
         }

         if (callback) callback(&context, SB_RELEASE);
         return SB_RELEASE;
      }
   }

   // Сброс счётчика нажатий по таймауту
   if (is_timer_count_enabled && ms_delta > timer_reset_count_event && count_event > 0) {
      count_event = 0;
      if (callback) callback(&context, SB_TIMER_COUNT);
      return SB_TIMER_COUNT;
   }

   return SB_NONE;
}

//-----------------------------------------------------------------------------
// Метод isPress - возвращает текущее состояние кнопки
//-----------------------------------------------------------------------------
bool SBTN::isPress() {
   return readPin();
}

//-----------------------------------------------------------------------------
// Установка callback-обработчика
//-----------------------------------------------------------------------------
void SBTN::setCallback(void (*func)(SBTN_Context* ctx, SBTN_EVENT_t event)) {
   callback = func;
}

//-----------------------------------------------------------------------------
// Управление параметрами
//-----------------------------------------------------------------------------
void SBTN::setBounceTM(uint32_t _tm) {
   tm_bounce = _tm;
}

void SBTN::setDebug(bool flag) {
   is_debug = flag;
}

void SBTN::setTimer(uint32_t _tm) {
   timer_press = _tm;
}

void SBTN::setTimer2(uint32_t _tm) {
   timer_press2 = _tm;
}

void SBTN::setTimerEventCount(uint32_t _tm) {
   timer_reset_count_event = _tm;
}

void SBTN::setTimerCountEnable(bool enable) {
   is_timer_count_enabled = enable;
}