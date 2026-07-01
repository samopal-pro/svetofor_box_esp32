/**
 * Проект контроллера автомоек. Версия 10 от 2025
 * Copyright (C) 2020 Алексей Шихарбеев
 * http://samopal.pro
 */


#include "WC_Event.h"

// ======================================================================
// Вспомогательные функции для безопасной работы с временем
// ======================================================================

/**
 * Безопасная проверка истечения таймера с учетом переполнения millis()
 * и автоматическим сбросом таймера при истечении
 * 
 * @param timer    Ссылка на переменную таймера (будет обновлена при истечении)
 * @param interval Интервал в миллисекундах
 * @return true если интервал истек
 */
bool TEvent::checkTimer(uint32_t &timer, uint32_t interval) {
    uint32_t current = millis();
    
    // Проверка с учетом переполнения millis()
    // Вычитание беззнаковых чисел корректно обрабатывает переполнение
    if (current - timer >= interval) {
        timer = current;  // Обновляем таймер
        return true;
    }
    return false;
}

/**
 * Безопасная проверка истечения времени без изменения таймера
 * 
 * @param startTime Начальное время
 * @param interval  Интервал в миллисекундах
 * @return true если интервал истек
 */
bool TEvent::isTimeUp(uint32_t startTime, uint32_t interval) {
    return (millis() - startTime) >= interval;
}

/**
 * Безопасный запуск таймера
 * 
 * @param timer Ссылка на переменную таймера
 */
void TEvent::startTimer(uint32_t &timer) {
    timer = millis();
}

// Аналогичные методы для TEventRGB
bool TEventRGB::checkTimer(uint32_t &timer, uint32_t interval) {
    uint32_t current = millis();
    if (current - timer >= interval) {
        timer = current;
        return true;
    }
    return false;
}

bool TEventRGB::isTimeUp(uint32_t startTime, uint32_t interval) {
    return (millis() - startTime) >= interval;
}

void TEventRGB::startTimer(uint32_t &timer) {
    timer = millis();
}

// Аналогичные методы для TEventMP3
bool TEventMP3::checkTimer(uint32_t &timer, uint32_t interval) {
    uint32_t current = millis();
    if (current - timer >= interval) {
        timer = current;
        return true;
    }
    return false;
}

bool TEventMP3::isTimeUp(uint32_t startTime, uint32_t interval) {
    return (millis() - startTime) >= interval;
}

void TEventMP3::startTimer(uint32_t &timer) {
    timer = millis();
}

// ======================================================================
// Класс TEvent - Базовое управление событиями
// ======================================================================

/**
 * Конструктор класса TEvent
 * @param _delayOn   Задержка при включении события, мс
 * @param _delayOff  Задержка при выключении события, мс
 * @param _handle    Функция обратного вызова при изменении состояния
 */
TEvent::TEvent(uint32_t _delayOn, uint32_t _delayOff, void (*_handle)(bool)) {
    DelayOn   = _delayOn;
    DelayOff  = _delayOff;
    Handle    = _handle;
    State     = ES_NONE;
    SaveState = ES_NONE;
    Type      = ET_NORMAL;
    TimeOn    = 0;
    TimeOff   = 0;
    isOn      = false;
    isChangeState = false;
}

/**
 * Установка типа события
 * @param _type      Тип события (ET_NORMAL, ET_PULSE, ET_PWM и т.д.)
 * @param _timeOn    Длительность импульса включения, мс
 * @param _timeOff   Длительность паузы между импульсами, мс
 */
void TEvent::setType(TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff) {
   Type    = _type;
   TimeOn  = _timeOn;
   TimeOff = _timeOff;
   
   LOG_DEBUGLN("TEvent type=%d t1=%lu t2=%lu", Type, TimeOn, TimeOff);
}

/**
 * Проверка изменения состояния события
 * @return true если состояние изменилось с последней проверки
 */
bool TEvent::changeState() {
   bool ret = isChangeState;
   isChangeState = false;
   return ret;
}

/**
 * Включение события (без задержки)
 */
void TEvent::on() {
    if (Type == ET_DISABLE) return;
    if (State == ES_WAIT_ON || State == ES_ON) return;
    
    startTimer(msOn);
    if (DelayOn == 0) {
       State = ES_ON;
       setOn();
    } else {
       State = ES_WAIT_ON;
    }
    
    LOG_DEBUGLN("TEvent::on() State=%d DelayOn=%lu", State, DelayOn);
}

/**
 * Включение события с указанием задержки
 * @param _delay Задержка перед включением, мс
 */
void TEvent::on(uint32_t _delay) {
   DelayOn = _delay;
   on();
}

/**
 * Выключение события (без задержки)
 */
void TEvent::off() {
    if (Type == ET_DISABLE) return;
    if (State == ES_WAIT_OFF || State == ES_OFF) return;
    
    startTimer(msOff);
    if (DelayOff == 0) {
       State = ES_OFF;
       setOff();
    } else {
       State = ES_WAIT_OFF;
    }
    
    LOG_DEBUGLN("TEvent::off() State=%d DelayOff=%lu", State, DelayOff);
}

/**
 * Выключение события с указанием задержки
 * @param _delay Задержка перед выключением, мс
 */
void TEvent::off(uint32_t _delay) {
   DelayOff = _delay;
   off();
}

/**
 * Сброс состояния события
 */
void TEvent::reset() {
    State = ES_NONE;
    LOG_DEBUGLN("TEvent::reset()");
}

/**
 * Внутренний метод включения
 */
void TEvent::setOn() {
   if (Handle != NULL) Handle(true);
   isOn = true;
}

/**
 * Внутренний метод выключения
 */
void TEvent::setOff() {
   if (Handle != NULL && isOn) Handle(false);
   isOn = false;
}

/**
 * Основной цикл обработки события
 * Использует безопасные методы работы с временем для защиты от переполнения millis()
 * 
 * @return Текущее состояние события
 */
TEVENT_STATUS_t TEvent::loop() {
   switch (State) {
       case ES_WAIT_ON:
          // Безопасная проверка истечения задержки включения
          if (checkTimer(msOn, DelayOn)) {
             State = ES_ON;
             setOn();
             LOG_DEBUGLN("TEvent WAIT_ON -> ON");
          }
          break;
          
       case ES_WAIT_OFF:
          // Исправлено: используем msOff вместо msOn
          if (checkTimer(msOff, DelayOff)) {
             State = ES_OFF;
             setOff();
             LOG_DEBUGLN("TEvent WAIT_OFF -> OFF");
          }
          break;
          
       case ES_ON:
          // Обработка импульсных режимов
          if ((Type == ET_PULSE || Type == ET_PULSE2 || Type == ET_PWM) && 
              isOn && checkTimer(msOn, TimeOn)) {
             setOff();
             LOG_DEBUGLN("TEvent ON pulse end");
          } 
          // Обработка PWM режима
          else if (Type == ET_PWM && !isOn && checkTimer(msOn, TimeOff)) {
             setOn();
             LOG_DEBUGLN("TEvent PWM toggle");
          }
          break;
          
       case ES_OFF:
          // Обработка PULSE_OFF режима
          if (Type == ET_PULSE_OFF && !isOn) {
             startTimer(msOn);
             setOn();
             LOG_DEBUGLN("TEvent PULSE_OFF start");
          } 
          // Обработка PULSE2 режима
          else if (Type == ET_PULSE2) {
             if (!isOn && checkTimer(msOff, TimeOff)) {
                setOn();
                LOG_DEBUGLN("TEvent PULSE2 on");
             } else if (isOn && checkTimer(msOff, TimeOff)) {
                setOff();
                LOG_DEBUGLN("TEvent PULSE2 off");
             }
          } 
          // Завершение PULSE_OFF
          else if (Type == ET_PULSE_OFF && isOn && checkTimer(msOn, TimeOn)) {
             setOff();
             LOG_DEBUGLN("TEvent PULSE_OFF end");
          }
          break;
   }
   
   // Отслеживание изменения состояния
   if (SaveState != State) {
       isChangeState = true;
       SaveState = State;
   }
   
   return State;
}

/**
 * Копирование настроек события в другой объект
 * @param dist Указатель на целевой объект TEvent
 */
void TEvent::copyTo(TEvent *dist) {
    dist->Type     = Type;
    dist->DelayOn  = DelayOn;
    dist->DelayOff = DelayOff;
    dist->TimeOn   = TimeOn;
    dist->TimeOff  = TimeOff;
    dist->Handle   = Handle;
}

// ======================================================================
// Класс TEventRGB - Управление RGB лентой
// ======================================================================

/**
 * Конструктор класса TEventRGB
 * @param _pin   Пин для управления лентой
 * @param _num   Количество светодиодов в ленте
 * @param _first Индекс первого светодиода (если 0 - все светодиоды, иначе первые _first зарезервированы)
 */
TEventRGB::TEventRGB(uint8_t _pin, int _num, int _first) {
   Strip = new Adafruit_NeoPixel(_num, _pin, NEO_GRB + NEO_KHZ800);
   Num           = _num;
   First         = _first;
   TimerOn      = 0;
   TimerOff     = 0;
   Color1       = COLOR_NONE;
   Color2       = COLOR_NONE;
   ColorBlink1  = COLOR_NONE;
   ColorBlink2  = COLOR_NONE;
   ColorMP3     = COLOR_NONE;
   State        = ES_NONE;
   msOn         = 0;
   msOff        = 0;
   isRainbow    = false;
   isMP3        = false;
   HueRainbow   = 0;
   IncRainbow   = 65536 / Num * 2;
   isBlink0     = false;
   BlinkCount   = 0;
   TimerRainbow = 0;
   NumAp        = LED_STAT_AP1;
   NumSta       = LED_STAT_STA1;
   
   LOG_DEBUGLN("TEventRGB created: pin=%d num=%d first=%d", _pin, _num, _first);
}

/**
 * Установка цветов для четных и нечетных светодиодов
 * @param _color1 Цвет для нечетных светодиодов
 * @param _color2 Цвет для четных светодиодов
 */
void TEventRGB::setColor(uint32_t _color1, uint32_t _color2) {
   for (int i = 0; i < Num; i++) {
       if (First > 0 && (i == NumAp || i == NumSta)) continue;
       if (i % 2) {
          if (_color1 != COLOR_NONE) Strip->setPixelColor(i, _color1);
       } else {
          if (_color2 != COLOR_NONE) Strip->setPixelColor(i, _color2);
       }
   }
   Strip->show();
}

/**
 * Установка цвета первого зарезервированного светодиода
 * @param _color Цвет светодиода
 * @param _blink Флаг мигания
 */
void TEventRGB::setColor0(uint32_t _color, bool _blink) {
   Color0   = _color;
   isBlink0 = _blink;
   Strip->setPixelColor(NumAp, setBrightness(_color, LED_STAT_BRIGHTNESS));
   Strip->show();
}

/**
 * Установка цвета второго зарезервированного светодиода
 * @param _color Цвет светодиода
 */
void TEventRGB::setColor1(uint32_t _color) {
   Strip->setPixelColor(NumSta, setBrightness(_color, LED_STAT_BRIGHTNESS));
   Strip->show();
}

/**
 * Полная установка параметров RGB ленты
 * @param _color1     Основной цвет 1
 * @param _color2     Основной цвет 2
 * @param _color11    Цвет мигания 1
 * @param _color12    Цвет мигания 2
 * @param _timer_on   Длительность включения при мигании
 * @param _timer_off  Длительность выключения при мигании
 */
void TEventRGB::set(uint32_t _color1, uint32_t _color2, uint32_t _color11, 
                    uint32_t _color12, uint32_t _timer_on, uint32_t _timer_off) {
   LOG_DEBUGLN("SetRGB #%lX #%lX #%lX #%lX %lu %lu", 
               _color1, _color2, _color11, _color12, _timer_on, _timer_off);
   
   TimerOn      = _timer_on;
   TimerOff     = _timer_off;
   Color1       = _color1;
   Color2       = _color2;
   ColorBlink1  = _color11;
   ColorBlink2  = _color12;
   setColor(Color1, Color2);
   msRainbowOn  = 0;
   startTimer(msOn);
   msOff        = 0;
}

/**
 * Установка цвета для режима MP3
 * @param _color Цвет для мигания в режиме MP3 (COLOR_NONE - отключить)
 */
void TEventRGB::setMP3(uint32_t _color) {
    ColorMP3 = _color;
    LOG_DEBUGLN("SetRGB MP3 #%lX", _color);
    
    if (ColorMP3 == COLOR_NONE) {
       isMP3 = false;
       setColor(Color1, Color2);
    } else {
       isMP3 = true;
    }
}

/**
 * Включение/выключение эффекта радуги
 * @param _flag  true - включить, false - выключить
 * @param _timer Длительность эффекта (0 - бесконечно)
 */
void TEventRGB::setRainbow(bool _flag, uint32_t _timer) {
   LOG_DEBUGLN("SetRGB Rainbow %d", _flag);
   isRainbow = _flag;
   
   if (isRainbow) {
      TimerRainbow = _timer;
      startTimer(msRainbowOn);
      HueRainbow = 0;   
   }
}

/**
 * Установка яркости ленты
 * @param br Уровень яркости (0-10)
 */
void TEventRGB::setBrightness(int br) {
   if (br < 0) Strip->setBrightness(0);
   else if (br >= 10) Strip->setBrightness(255);
   else Strip->setBrightness(br * 25);
}

/**
 * Основной цикл обработки RGB эффектов
 * Использует безопасные методы работы с временем для защиты от переполнения millis()
 * 
 * @return Тип текущего RGB события
 */
TEVENT_RGB_TYPE_t TEventRGB::loop() {
   // Эффект радуги (имеет наивысший приоритет)
   if (isRainbow) {
      // Проверка таймера радуги
      if (TimerRainbow > 0 && checkTimer(msRainbowOn, TimerRainbow)) {
          setRainbow(false);
          setColor(Color1, Color2);
          msOff = 0;
          startTimer(msOn);
          msRainbowOn = 0;
          return ERT_RAINBOW;
      }
      
      // Отрисовка радуги
      uint16_t h = HueRainbow;
      for (int i = 0; i < Num; i++) {
          if (First > 0 && (i == NumAp || i == NumSta)) continue;
          h += IncRainbow;
          Strip->setPixelColor(i, Strip->ColorHSV(h, 255, 100)); 
      }
      Strip->show();
      HueRainbow += IncRainbow;
      return ERT_RAINBOW;
   }
   
   // Мигание первого зарезервированного светодиода
   if (isBlink0) {
       if (BlinkCount % 2) {
           Strip->setPixelColor(NumAp, setBrightness(Color0, LED_STAT_BRIGHTNESS));
       } else {
           Strip->setPixelColor(NumAp, setBrightness(COLOR_BLACK, LED_STAT_BRIGHTNESS));
       }
       Strip->show();
       BlinkCount++;
   }

   // Мигание в режиме MP3
   if (isMP3 && ColorMP3 != COLOR_NONE) {
      if (msOn > 0 && checkTimer(msOn, TIMER_MP3)) {
         msOn  = 0;
         startTimer(msOff);
         setColor(ColorMP3, ColorMP3);
      }
      if (msOff > 0 && checkTimer(msOff, TIMER_MP3)) {
         msOff  = 0;
         startTimer(msOn);
         setColor(Color1, Color2);
      }
      return ERT_MP3;
   }
   
   // Обычное мигание
   if (TimerOn > 0) {
      if (msOn > 0 && checkTimer(msOn, TimerOn)) {
         msOn  = 0;
         startTimer(msOff);
         setColor(ColorBlink1, ColorBlink2);
      }
      if (msOff > 0 && checkTimer(msOff, TimerOff)) {
         msOff  = 0;
         startTimer(msOn);
         setColor(Color1, Color2);
      }
      return ERT_BLINK;
   }
   
   return ERT_NORMAL;
}

/**
 * Копирование настроек в другой объект RGB
 * @param dist Указатель на целевой объект TEventRGB
 */
void TEventRGB::copyTo(TEventRGB *dist) {
   dist->Color1      = Color1;
   dist->Color2      = Color2;
   dist->ColorBlink1 = ColorBlink1;
   dist->ColorBlink2 = ColorBlink2;
   dist->ColorMP3    = ColorMP3;
   dist->TimerOn     = TimerOn;
   dist->TimerOff    = TimerOff;
}

/**
 * Установка настроек из другого объекта RGB
 * @param src Указатель на исходный объект TEventRGB
 */
void TEventRGB::set(TEventRGB *src) {
   set(src->Color1, src->Color2, src->ColorBlink1, src->ColorBlink2, 
       src->TimerOn, src->TimerOff);
}

/**
 * Коррекция яркости цвета
 * @param color Исходный цвет
 * @param level Уровень яркости (0-10)
 * @return Скорректированный цвет
 */
uint32_t TEventRGB::setBrightness(uint32_t color, uint8_t level) {
    if (level > 10) level = 10;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    r = (r * level) / 10;
    g = (g * level) / 10;
    b = (b * level) / 10;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/**
 * Изменение позиции первого зарезервированного светодиода
 * @param _ap Новый индекс светодиода
 */
void TEventRGB::setNumAp(int _ap) {
   if (NumAp == _ap) return;
   
   if (_ap >= 0 && _ap < Num) {
      uint32_t _color1 = Strip->getPixelColor(NumAp);
      uint32_t _color2 = Strip->getPixelColor(_ap);
      Strip->setPixelColor(NumAp, _color2);
      Strip->setPixelColor(_ap, _color1);
      NumAp = _ap;
   } 
   Strip->show();
}

/**
 * Изменение позиции второго зарезервированного светодиода
 * @param _sta Новый индекс светодиода
 */
void TEventRGB::setNumSta(int _sta) {
   if (NumSta == _sta) return;
   
   if (_sta >= 0 && _sta < Num) {
      uint32_t _color1 = Strip->getPixelColor(NumSta);
      uint32_t _color2 = Strip->getPixelColor(_sta);
      Strip->setPixelColor(NumSta, _color2);
      Strip->setPixelColor(_sta, _color1);
      NumSta = _sta;
   } 
   Strip->show();
}

// ======================================================================
// Класс TEventMP3 - Управление MP3 плеером
// ======================================================================

/**
 * Конструктор класса TEventMP3
 * @param _stream  Поток для связи с плеером (обычно Serial)
 * @param _gpio    Режим определения состояния через GPIO
 * @param _pin     Пин GPIO для определения состояния
 * @param _handle  Функция обратного вызова
 */
TEventMP3::TEventMP3(Stream &_stream, TEVENT_MP3_GPIO_t _gpio, int _pin, 
                     void (*_handle)(bool)) {
   Player = new DFRobotDFPlayerMini();   
   SerialPlayer = &_stream;
   Handle       = _handle;
   DelayOn      = 0;
   TimerOn      = 0;
   Dir          = 0;
   Track        = 0;
   State        = ES_NONE;
   msOn         = 0;
   isPlayer     = false;
   isLoop       = false;
   Color        = COLOR_NONE;
   ColorEvent   = new TEvent(0, 0, _handle);
   Priority     = DAFAULT_PRIORITY_MP3;
   GPIO         = _gpio;
   PIN          = _pin;
   isLowGpio    = false;
   isHighGpio   = false;
   
   init();
}

/**
 * Инициализация MP3 плеера
 */
void TEventMP3::init() {
    if (!isPlayer) {
      Player->begin(*SerialPlayer, /*isACK = */true, /*doReset = */true);
      
      if (Player->readVolume() >= 0) {
         isPlayer = true;
         if (isPlayer) Player->setTimeOut(500);
         if (isPlayer) Player->outputDevice(DFPLAYER_DEVICE_SD); 

         if (GPIO != ESM_NONE && PIN >= 0) {
            pinMode(PIN, INPUT);
            if (digitalRead(PIN)) isHighGpio = true;
            LOG_DEBUGLN("MP3 GPIO %d", (int)digitalRead(PIN));
         }
         LOG_INFOLN("MP3 init success");
      } else {
         LOG_ERRORLN("MP3 init failed");
      }
    }
}

/**
 * Установка громкости плеера
 * @param _volume Уровень громкости (0-30)
 */
void TEventMP3::setVolume(int _volume) {
    if (isPlayer) Player->volume(_volume);
}

/**
 * Установка цвета для индикации MP3
 * @param _color Цвет индикации
 * @param _timer Длительность импульса (0 - постоянное свечение)
 */
void TEventMP3::setColor(uint32_t _color, uint32_t _timer) {
    Color      = _color;
    ColorTimer = _timer;
    
    if (_timer == 0) {
        ColorEvent->setType(ET_NORMAL);
    } else {
        ColorEvent->setType(ET_PULSE, _timer);
    }
}

/**
 * Запуск воспроизведения MP3
 * @param _dir      Директория (папка)
 * @param _track    Номер трека
 * @param _priority Приоритет воспроизведения
 * @param _delay    Задержка перед воспроизведением
 * @param _timer    Длительность воспроизведения (0 - до конца трека)
 * @param _is_wait  Ожидать окончания трека
 * @param _loop     Зациклить воспроизведение
 */
void TEventMP3::start(int _dir, int _track, int _priority, uint32_t _delay, 
                      uint32_t _timer, bool _is_wait, bool _loop) {   
    LOG_DEBUGLN("MP3 check %d %d %d", _priority, Priority, State);

    // Проверка приоритета
    if (_priority < Priority && (State == ES_WAIT_ON || State == ES_ON)) return;
    if (!isPlayer) return;

    startTimer(msOn);
    Dir        = _dir;
    Track      = _track;
    DelayOn    = _delay;
    TimerOn    = _timer;
    waitPlayer = _is_wait;
    Priority   = _priority;
    isLoop     = _loop;
    
    if (DelayOn == 0) {
       State = ES_ON;
       _start();
    } else {
       _stop();
       State = ES_WAIT_ON;
    }    
    
    LOG_INFOLN("MP3 start %02d/%03d.mp3 delay=%lu timer=%lu wait=%d loop=%d", 
               Dir, Track, DelayOn, TimerOn, (int)waitPlayer, (int)isLoop);
}

/**
 * Остановка воспроизведения
 */
void TEventMP3::stop() {
   if (State == ES_NONE) return;
   _stop();
}

/**
 * Внутренний метод запуска воспроизведения
 */
void TEventMP3::_start() {
   init();
   if (isPlayer) Player->playFolder(Dir, Track); 
   if (Handle != NULL) ColorEvent->on();
   isOn = true;
   ms1  = 0;
   LOG_DEBUGLN("MP3 _start");
}

/**
 * Внутренний метод остановки воспроизведения
 */
void TEventMP3::_stop() {
    LOG_INFOLN("MP3 stop %d/%d.mp3", Dir, Track);
   if (isPlayer) Player->stop();
   if (Handle != NULL && isOn) ColorEvent->off();
   State = ES_NONE;   
   isOn = false;
}

/**
 * Внутренний метод повторного воспроизведения
 * @param _delay Задержка перед повтором
 */
void TEventMP3::_replay(uint32_t _delay) {
    startTimer(msOn);
    DelayOn = _delay;
    
    if (DelayOn == 0) {
       State = ES_ON;
       _start();
    } else {
       State = ES_WAIT_ON;
    }    
    LOG_DEBUGLN("MP3 _replay delay=%lu", _delay);
}

/**
 * Определение состояния плеера
 * @return Состояние: 1 - играет, 0 - остановлен, -1 - ошибка
 */
int TEventMP3::state() {
   int _state = -1;
   int _flag = 0;
   
   if (GPIO == ESM_NONE || PIN < 0) {
      _state = Player->readState();
   } else if (GPIO == ESM_ENABLE) {
      _state = digitalRead(PIN) ? 0 : 1;
      _flag = 1;
   } else {
      if (isLowGpio && isHighGpio) {
         _state = digitalRead(PIN) ? 0 : 1;
         _flag = 1;
      } else {
         _state = Player->readState();
         if (_state == 1) isLowGpio = true;
         else isHighGpio = true;
      }
   }
   
   LOG_DEBUGLN("MP3 state=%d flag=%d low=%d high=%d", 
               _state, _flag, (int)isLowGpio, (int)isHighGpio);
   return _state;
}

/**
 * Основной цикл обработки MP3 плеера
 * Использует безопасные методы работы с временем для защиты от переполнения millis()
 * 
 * @return Текущее состояние
 */
TEVENT_STATUS_t TEventMP3::loop() {
   switch (State) {
       case ES_WAIT_ON:
          // Безопасная проверка задержки включения
          if (checkTimer(msOn, DelayOn)) {
             State = ES_ON;
             _start();
             LOG_DEBUGLN("MP3 WAIT_ON -> ON");
          }
          break;
          
       case ES_ON:
          if (!isPlayer) {
              _stop();
              break;
          }
          
          // Проверка таймера воспроизведения
          if (TimerOn > 0 && isOn && checkTimer(msOn, TimerOn)) {
             if (isLoop) {
                 _replay(2000);
             } else {
                 stop();
             }
             LOG_DEBUGLN("MP3 timer end");
          }
          
          // Проверка состояния плеера с защитой от переполнения
          if (isOn && waitPlayer) {
             // Защита первого запуска и переполнения
             if (ms1 == 0 || checkTimer(ms1, 2000)) {
                int _state;
                if (isPlayer) {
                   _state = state();
                } else {
                   _state = 0;
                   LOG_ERRORLN("MP3 not initialized");
                }
                
                if (_state <= 0) {
                   if (isLoop) {
                       _replay(2000);
                   } else {
                       _stop();
                   }
                   LOG_DEBUGLN("MP3 playback ended");
                }
             }
          }
          break;
   }
   
   return State;
}