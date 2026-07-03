/**
 * @file WC_Event.cpp
 * @brief Реализация классов TEvent, TEventRGB, TEventMP3.
 * 
 * Проект контроллера автомоек. Версия 10 от 2025
 * Copyright (C) 2020 Алексей Шихарбеев
 * http://samopal.pro
 */
#include "WC_Event.h"


#define MODULE_NAME "EVENT"
#define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#include "src/Slib/SDEBUG.h"

//=================================================================================================
// Реализация класса TEvent
//=================================================================================================

/**
 * @brief Конструктор класса TEvent.
 * @param _delayOn   Задержка при включении события, мс.
 * @param _delayOff  Задержка при выключении события, мс.
 * @param _handle    Функция-обработчик, вызываемая при изменении состояния (вкл/выкл).
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
 * @brief Изменение типа события и его временных параметров.
 * @param _type    Тип события (ET_NORMAL, ET_PULSE, ET_PWM и т.д.).
 * @param _timeOn  Длительность импульса/включения, мс (для PULSE, PWM и т.д.).
 * @param _timeOff Длительность паузы, мс (для PWM).
 */
void TEvent::setType(TEVENT_TYPE_t _type, uint32_t _timeOn, uint32_t _timeOff) {
   Type    = _type;
   TimeOn  = _timeOn;
   TimeOff = _timeOff;
   LOG_DEBUGLN("TEvent Type: %d, TimeOn: %d, TimeOff: %d", (int)Type, TimeOn, TimeOff);
}

/**
 * @brief Сбрасывает флаг isChangeState и возвращает его предыдущее значение.
 * @return true, если состояние изменилось с момента последнего вызова.
 */
bool TEvent::changeState() {
   bool ret = isChangeState;
   isChangeState = false;
   return ret;
}

/**
 * @brief Включает событие (с учетом DelayOn).
 */
void TEvent::on() {
    if (Type == ET_DISABLE) return;
    if (State == ES_WAIT_ON || State == ES_ON) return;
    
    msOn = millis();
    if (DelayOn == 0) {
       State = ES_ON;
       setOn();
    } else {
       State = ES_WAIT_ON;
    }
}

/**
 * @brief Включает событие, устанавливая задержку включения.
 * @param _delay Задержка включения, мс.
 */
void TEvent::on(uint32_t _delay) {
   DelayOn = _delay;
   on();
}

/**
 * @brief Выключает событие (с учетом DelayOff).
 */
void TEvent::off() {
    if (Type == ET_DISABLE) return;
    if (State == ES_WAIT_OFF || State == ES_OFF) return;
    
    msOff = millis();
    if (DelayOff == 0) {
       State = ES_OFF;
       setOff();
    } else {
       State = ES_WAIT_OFF;
    }
}

/**
 * @brief Выключает событие, устанавливая задержку выключения.
 * @param _delay Задержка выключения, мс.
 */
void TEvent::off(uint32_t _delay) {
   DelayOff = _delay;
   off();
}

/**
 * @brief Принудительно сбрасывает состояние события в ES_NONE.
 */
void TEvent::reset() {
    State = ES_NONE;
}

/**
 * @brief Внутренний метод для физического включения. Вызывает обработчик, если он задан.
 */
void TEvent::setOn() {
   if (Handle != NULL) Handle(true);
   isOn = true;
}

/**
 * @brief Внутренний метод для физического выключения. Вызывает обработчик, только если до этого был включен.
 */
void TEvent::setOff() {
   if (Handle != NULL && isOn) Handle(false);
   isOn = false;
}

/**
 * @brief Основной метод конечного автомата. Должен вызываться в loop().
 * @return Текущий статус события.
 */
TEVENT_STATUS_t TEvent::loop() {
   uint32_t _ms = millis();
   switch (State) {
       case ES_WAIT_ON:
          if (TIME_DIFF_MS(_ms, msOn) > DelayOn) {
             State = ES_ON;
             msOn  = _ms;
             setOn();
          }
          break;
          
       case ES_WAIT_OFF:
          // ИСПРАВЛЕНО: Теперь корректно проверяем разницу от msOff, а не msOn.
          if (TIME_DIFF_MS(_ms, msOff) > DelayOff) {
             State = ES_OFF;
             msOff = _ms; // Обновляем время последнего off-события
             setOff();
          }
          break;
          
       case ES_ON:
          // Логика для импульсных и ШИМ-режимов
          if ((Type == ET_PULSE || Type == ET_PULSE2 || Type == ET_PWM) && isOn && (TIME_DIFF_MS(_ms, msOn) > TimeOn)) {
             msOn = _ms;
             setOff();
          } else if ((Type == ET_PWM) && !isOn && (TIME_DIFF_MS(_ms, msOn) > TimeOff)) {
             msOn = _ms;
             setOn();
          }
          break;

       case ES_OFF:
          // Логика для ET_PULSE_OFF (импульс при выключении) и ET_PULSE2 (двойной импульс)
          if ((Type == ET_PULSE_OFF) && !isOn) {
             msOn = _ms;
             setOn();
          } else if ((Type == ET_PULSE2) && !isOn && (TIME_DIFF_MS(_ms, msOn) <= TimeOff)) {
             setOn();
          } else if ((Type == ET_PULSE2) && isOn && (TIME_DIFF_MS(_ms, msOn) > TimeOff)) {
             setOff();
          } else if ((Type == ET_PULSE_OFF) && isOn && (TIME_DIFF_MS(_ms, msOn) > TimeOn)) {
             msOn = _ms;
             setOff();
          }
          break;
   }

   if (SaveState != State) {
       isChangeState = true;
       SaveState     = State;
   }
   return State;
}

/**
 * @brief Копирует настройки в другой объект TEvent.
 * @param dist Указатель на целевой объект.
 */
void TEvent::copyTo(TEvent *dist) {
    dist->Type     = Type;
    dist->DelayOn  = DelayOn;
    dist->DelayOff = DelayOff;
    dist->TimeOn   = TimeOn;
    dist->TimeOff  = TimeOff;
    dist->Handle   = Handle;
}

//=================================================================================================
// Реализация класса TEventRGB
//=================================================================================================

/**
 * @brief Конструктор класса TEventRGB.
 * @param _pin Номер пина для управления лентой.
 * @param _num Количество светодиодов в ленте.
 * @param _first Индекс первого пользовательского светодиода (первые два могут быть служебными).
 */
TEventRGB::TEventRGB(uint8_t _pin, int _num, int _first) {
   Strip = new Adafruit_NeoPixel(_num, _pin, NEO_GRB + NEO_KHZ800);
   Num           = _num;
   First         = _first;
   TimerOn       = 0;
   TimerOff      = 0;
   Color1        = COLOR_NONE;
   Color2        = COLOR_NONE;
   ColorBlink1   = COLOR_NONE;
   ColorBlink2   = COLOR_NONE;
   ColorMP3      = COLOR_NONE;
   State         = ES_NONE;
   msOn          = 0;
   msOff         = 0;
   isRainbow     = false;
   isMP3         = false;
   HueRainbow    = 0;
   IncRainbow    = 65536 / Num * 2;
   isBlink0      = false;
   BlinkCount    = 0;
   TimerRainbow  = 0;
   NumAp         = LED_STAT_AP1;
   NumSta        = LED_STAT_STA1;
}

/**
 * @brief Устанавливает цвета для четных и нечетных светодиодов, кроме служебных.
 * @param _color1 Цвет для нечетных светодиодов.
 * @param _color2 Цвет для четных светодиодов.
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
 * @brief Устанавливает цвет для служебного светодиода 0 (например, статус AP).
 * @param _color Цвет светодиода.
 * @param _blink Если true, светодиод будет мигать при каждом вызове loop().
 */
void TEventRGB::setColor0(uint32_t _color, bool _blink) {
   Color0   = _color;
   isBlink0 = _blink;
   Strip->setPixelColor(NumAp, setBrightness(_color, LED_STAT_BRIGHTNESS));
   Strip->show();
}

/**
 * @brief Устанавливает цвет для служебного светодиода 1 (например, статус STA).
 * @param _color Цвет светодиода.
 */
void TEventRGB::setColor1(uint32_t _color) {
   Strip->setPixelColor(NumSta, setBrightness(_color, LED_STAT_BRIGHTNESS));
   Strip->show();
}

/**
 * @brief Устанавливает основное состояние ленты с цветами и таймерами.
 * @param _color1, _color2 Основные цвета.
 * @param _color11, _color12 Цвета для мигания.
 * @param _timer_on Время свечения основным цветом, мс.
 * @param _timer_off Время свечения цветом мигания, мс.
 */
void TEventRGB::set(uint32_t _color1, uint32_t _color2, uint32_t _color11, uint32_t _color12, uint32_t _timer_on, uint32_t _timer_off) {
   LOG_INFOLN("SetRGB: C1=#%06lX, C2=#%06lX, B1=#%06lX, B2=#%06lX, T_on=%d, T_off=%d", 
              _color1, _color2, _color11, _color12, _timer_on, _timer_off);
   TimerOn      = _timer_on;
   TimerOff     = _timer_off;
   Color1       = _color1;
   Color2       = _color2;
   ColorBlink1  = _color11;
   ColorBlink2  = _color12;
   setColor(Color1, Color2);
   msRainbowOn  = 0;
   msOn         = millis();
   msOff        = 0;
}

/**
 * @brief Устанавливает цвет для режима MP3-мигания.
 * @param _color Цвет для мигания. COLOR_NONE отключает режим.
 */
void TEventRGB::setMP3(uint32_t _color) {
    ColorMP3 = _color;
    LOG_INFOLN("SetRGB MP3: #%06lX", _color);
    if (ColorMP3 == COLOR_NONE) {
       isMP3 = false;
       setColor(Color1, Color2);
    } else {
       isMP3 = true;
    }
}

/**
 * @brief Включает или выключает эффект радуги.
 * @param _flag true для включения.
 * @param _timer Длительность эффекта. 0 - бесконечно.
 */
void TEventRGB::setRainbow(bool _flag, uint32_t _timer) {
   LOG_INFOLN("SetRGB Rainbow: %d", _flag);
   isRainbow = _flag;
   if (isRainbow) {
      TimerRainbow = _timer;
      msRainbowOn  = millis();
      HueRainbow   = 0;   
   }
}

/**
 * @brief Устанавливает яркость ленты.
 * @param br Уровень яркости от 0 до 10.
 */
void TEventRGB::setBrightness(int br) {
   if (br < 0) Strip->setBrightness(0);
   else if (br >= 10) Strip->setBrightness(255);
   else Strip->setBrightness(br * 25);
}

/**
 * @brief Основной метод loop для обработки эффектов ленты.
 * @return Текущий тип RGB-события.
 */
TEVENT_RGB_TYPE_t TEventRGB::loop() {
   uint32_t _ms = millis();

   // 1. Обработка эффекта радуги
   if (isRainbow) {
      if (TimerRainbow > 0 && TIME_DIFF_MS(_ms, msRainbowOn) > TimerRainbow) {
          setRainbow(false);
          setColor(Color1, Color2);
          msOff       = 0;
          msOn        = _ms;
          msRainbowOn = 0;
          return ERT_RAINBOW;
      }
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

   // 2. Мигание первого служебного светодиода с частотой вызова loop()
   if (isBlink0) {
       if (BlinkCount % 2) {
           Strip->setPixelColor(NumAp, setBrightness(Color0, LED_STAT_BRIGHTNESS));
       } else {
           Strip->setPixelColor(NumAp, setBrightness(COLOR_BLACK, LED_STAT_BRIGHTNESS));
       }
       Strip->show();
       BlinkCount++;
   }

   // 3. Мигание в режиме MP3 с частотой TIMER_MP3
   if (isMP3 && ColorMP3 != COLOR_NONE) {
      if (msOn > 0 && TIME_DIFF_MS(_ms, msOn) > TIMER_MP3) {
         msOn  = 0;
         msOff = _ms;
         setColor(ColorMP3, ColorMP3);
      }
      if (msOff > 0 && TIME_DIFF_MS(_ms, msOff) > TIMER_MP3) {
         msOff = 0;
         msOn  = _ms;
         setColor(Color1, Color2);
      }
      return ERT_MP3;
   }

   // 4. Обычное мигание с заданными таймерами
   if (TimerOn > 0) {
      if (msOn > 0 && TIME_DIFF_MS(_ms, msOn) > TimerOn) {
         msOn  = 0;
         msOff = _ms;
         setColor(ColorBlink1, ColorBlink2);
      }
      if (msOff > 0 && TIME_DIFF_MS(_ms, msOff) > TimerOff) {
         msOff = 0;
         msOn  = _ms;
         setColor(Color1, Color1);
      }
      return ERT_BLINK;
   }
   
   return ERT_NORMAL;
}

/**
 * @brief Копирует состояние эффектов в другой объект TEventRGB.
 * @param dist Указатель на целевой объект.
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
 * @brief Применяет состояние эффектов из другого объекта TEventRGB.
 * @param src Указатель на исходный объект.
 */
void TEventRGB::set(TEventRGB *src) {
   set(src->Color1, src->Color2, src->ColorBlink1, src->ColorBlink2, src->TimerOn, src->TimerOff);
}

/**
 * @brief Масштабирует яркость цвета на заданный уровень (0-10).
 * @param color Исходный цвет.
 * @param level Уровень яркости (0-10).
 * @return Результирующий цвет.
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
 * @brief Меняет позицию служебного светодиода "точки доступа" (AP).
 * @param _ap Новый индекс светодиода.
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
 * @brief Меняет позицию служебного светодиода "станции" (STA).
 * @param _sta Новый индекс светодиода.
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

//=================================================================================================
// Реализация класса TEventMP3
//=================================================================================================

/**
 * @brief Конструктор класса TEventMP3.
 * @param _stream Поток для связи с DFPlayer.
 * @param _gpio Тип управления через GPIO.
 * @param _pin Номер пина для GPIO.
 * @param _handle Функция-обработчик для управления внешними событиями (например, светодиодом).
 */
TEventMP3::TEventMP3(Stream &_stream, TEVENT_MP3_GPIO_t _gpio, int _pin, void (*_handle)(bool)) {
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
 * @brief Деструктор. Освобождает ресурсы, выделенные для Player и ColorEvent.
 */
TEventMP3::~TEventMP3() {
    if (Player) {
        delete Player;
        Player = nullptr;
    }
    if (ColorEvent) {
        delete ColorEvent;
        ColorEvent = nullptr;
    }
}

/**
 * @brief Инициализирует DFPlayer Mini.
 */
void TEventMP3::init() {
    if (!isPlayer) {
      Player->begin(*SerialPlayer, true, true);
      if (Player->readVolume() >= 0) {
         isPlayer = true;
         Player->setTimeOut(500);
         Player->outputDevice(DFPLAYER_DEVICE_SD); 

         if (GPIO != ESM_NONE && PIN >= 0) {
            pinMode(PIN, INPUT);
            if (digitalRead(PIN)) isHighGpio = true;
            LOG_INFOLN("MP3 GPIO %d state: %d", PIN, (int)digitalRead(PIN));
         }
         LOG_INFOLN("MP3 init success");
      } else {
         LOG_ERRORLN("MP3 init FAILED");
      }
    }
}

/**
 * @brief Устанавливает громкость плеера.
 * @param _volume Уровень громкости (0-30).
 */
void TEventMP3::setVolume(int _volume) {
    if (isPlayer) Player->volume(_volume);
}

/**
 * @brief Устанавливает цвет и таймер для ColorEvent (мигание светодиодом при проигрывании).
 * @param _color Цвет светодиода.
 * @param _timer Длительность импульса мигания. 0 - без мигания.
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
 * @brief Запускает воспроизведение трека с учетом приоритетов.
 * @param _dir Папка на SD-карте.
 * @param _track Номер трека.
 * @param _priority Приоритет воспроизведения.
 * @param _delay Задержка перед началом воспроизведения.
 * @param _timer Длительность воспроизведения (0 - пока не закончится).
 * @param _is_wait Ожидать ли окончания трека по таймауту или опросу состояния.
 * @param _loop Зациклить ли воспроизведение.
 */
void TEventMP3::start(int _dir, int _track, int _priority, uint32_t _delay, uint32_t _timer, bool _is_wait, bool _loop) {
    LOG_INFOLN("MP3 check: Req Prio=%d, Cur Prio=%d, Cur State=%d", _priority, Priority, State);
    
    if (_priority < Priority && (State == ES_WAIT_ON || State == ES_ON)) return;
    if (!isPlayer) return;

    msOn       = millis();
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
    LOG_INFOLN("MP3 start: %02d/%03d.mp3, Delay=%d, Timer=%d, Wait=%d, Loop=%d", Dir, Track, DelayOn, TimerOn, waitPlayer, isLoop);
}

/**
 * @brief Останавливает воспроизведение.
 */
void TEventMP3::stop() {
   if (State == ES_NONE) return;
   LOG_INFOLN("MP3 stop");
   _stop();
}

/**
 * @brief Внутренний метод запуска воспроизведения.
 */
void TEventMP3::_start() {
   init();
   if (isPlayer) Player->playFolder(Dir, Track); 
   if (Handle != NULL) ColorEvent->on();
   isOn = true;
   ms1  = 0;
   LOG_DEBUGLN("MP3 _start: Playing %02d/%03d.mp3", Dir, Track);
}

/**
 * @brief Внутренний метод остановки воспроизведения.
 */
void TEventMP3::_stop() {
    if (isPlayer) Player->stop();
    if (Handle != NULL && isOn) ColorEvent->off();
    State = ES_NONE;   
    isOn = false;
    LOG_DEBUGLN("MP3 _stop: Stopped %02d/%03d.mp3", Dir, Track);
}

/**
 * @brief Внутренний метод повтора воспроизведения с задержкой.
 * @param _delay Задержка перед повтором, мс.
 */
void TEventMP3::_replay(uint32_t _delay) {
    msOn = millis();
    DelayOn = _delay;
    State = ES_WAIT_ON;
    LOG_DEBUGLN("MP3 _replay: Delay=%d", DelayOn);
}

/**
 * @brief Опрашивает состояние плеера (закончился ли трек).
 * @return 0 - проигрывание, 1 - остановлен, -1 - ошибка.
 */
int TEventMP3::state() {
   int _state = -1;
   if (GPIO == ESM_NONE || PIN < 0) {
      Player->readState();
      _state = Player->readState();
   } else if (GPIO == ESM_ENABLE) {
      _state = digitalRead(PIN) ? 0 : 1;
   } else { // ESM_AUTO
      if (isLowGpio && isHighGpio) {
         _state = digitalRead(PIN) ? 0 : 1;
      } else {
         Player->readState();
         _state = Player->readState();
         if (_state == 1) isLowGpio = true;
         else isHighGpio = true;
      }
   }
   LOG_DEBUGLN("MP3 state=%d, GPIO=%d, Low=%d, High=%d", _state, GPIO, isLowGpio, isHighGpio);
   return _state;
}

/**
 * @brief Основной метод loop для обработки состояний MP3-плеера.
 * @return Текущий статус события.
 */
TEVENT_STATUS_t TEventMP3::loop() {
   uint32_t _ms = millis();
   
   switch (State) {
       case ES_WAIT_ON:
          if (TIME_DIFF_MS(_ms, msOn) > DelayOn) {
             State = ES_ON;
             msOn  = _ms;
             _start();
          }
          break;
          
       case ES_ON:
          if (!isPlayer) {
              _stop();
              break;
          }
          // Остановка по таймеру
          if (TimerOn > 0 && isOn && (TIME_DIFF_MS(_ms, msOn) > TimerOn)) {
             msOn = 0;
             if (isLoop) _replay(DelayOn);
             else _stop();
          }
          // Остановка по окончанию трека
          if (isOn && waitPlayer && (ms1 == 0 || TIME_DIFF_MS(_ms, ms1) > 2000)) {
             ms1 = _ms;
             int _state = state();
             if (_state <= 0) {
                if (isLoop) _replay(DelayOn);
                else _stop();
             }
          }
          break;
   }

   return State;
}