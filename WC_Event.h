/**
 * @file WC_Event.h
 * @brief Заголовочный файл классов для работы с событиями, светодиодной лентой и MP3-плеером.
 * 
 * Проект контроллера автомоек. Версия 10 от 2025
 * Copyright (C) 2020 Алексей Шихарбеев
 * http://samopal.pro
 */
#pragma once
#include <arduino.h>
#include "MyConfig.h"
#include "src/DFPlayer/DFRobotDFPlayerMini.h"
#include "src/NeoPixel/Adafruit_NeoPixel.h" // https://github.com/adafruit/Adafruit_NeoPixel


// --- Конфигурационные макросы ---
#define MAX_RGB_STACK_ITEMS    10
#define TIMER_MP3              500
#define DEFAULT_TIMER_MP3      300000
#define DEFAULT_DELAY_MP3      0

// Приоритеты MP3-плеера
#define PRIORITY_MP3_MINIMAL   0
#define PRIORITY_MP3_MEDIUM    1
#define PRIORITY_MP3_HIGH      2
#define PRIORITY_MP3_MAXIMAL   3
#define PRIORITY_MP3_75        4
#define DAFAULT_PRIORITY_MP3   PRIORITY_MP3_MINIMAL

/**
 * @enum TEVENT_TYPE_t
 * @brief Типы поведения события.
 */
enum TEVENT_TYPE_t {
  ET_DISABLE    = 0, // Событие отключено
  ET_NORMAL     = 1, // Событие работает на ON/OFF
  ET_PULSE      = 2, // Событие выдает импульс на заданное время при включении
  ET_PULSE_OFF  = 3, // Событие выдает импульс на заданное время при выключении
  ET_PWM        = 4, // Событие выдает импульсы с заданной длительностью и паузой
  ET_PULSE2     = 5  // Событие выдает импульсы при включении и выключении с паузой
};

/**
 * @enum TEVENT_RGB_TYPE_t
 * @brief Типы поведения RGB-события.
 */
enum TEVENT_RGB_TYPE_t {
  ERT_NONE      = 0, // Событие отключено
  ERT_NORMAL    = 1, // Горит постоянно
  ERT_BLINK     = 2, // Мигает с заданной частотой
  ERT_MP3       = 3, // Мигает цветом ColorMP3 и частотой TIMER_MP3
  ERT_RAINBOW   = 4  // Проигрывает радугу
};

/**
 * @enum TEVENT_STATUS_t
 * @brief Статусы конечного автомата событий.
 */
enum TEVENT_STATUS_t {
   ES_NONE     = 0, // Ничего не происходит
   ES_WAIT_ON  = 1, // Ожидание включения
   ES_ON       = 2, // Включено
   ES_WAIT_OFF = 3, // Ожидание выключения
   ES_OFF      = 4  // Выключено
};

/**
 * @enum TEVENT_MP3_GPIO_t
 * @brief Типы управления MP3-плеером через GPIO.
 */
enum TEVENT_MP3_GPIO_t {
   ESM_NONE   = 0, // Не использовать GPIO
   ESM_ENABLE = 1, // Использовать GPIO для определения состояния
   ESM_AUTO   = 2, // Автоматическое определение
};

// Структура для стека сохранения состояний RGB (не используется в этом файле, но оставлена для обратной совместимости)
typedef struct {
   int ID;
   TEVENT_TYPE_t Type;
   uint32_t TimeOn;
   uint32_t TimeOff;
   uint32_t Color1;
   uint32_t Color2;
   bool Flag; 
} RGB_STACK_ITEM_t;

/**
 * @brief Макрос для безопасного вычисления разницы во времени с защитой от переполнения.
 * @param current Текущее время (millis())
 * @param previous Предыдущее время
 * @return Разница во времени
 */
//#define TIME_DIFF_MS(current, previous) ((uint32_t)(current - previous)) //Перенес в MyConfig.h

/**
 * @class TEvent
 * @brief Класс для управления событием с настраиваемыми задержками и типами.
 */
class TEvent {
public:
   TEvent(uint32_t _delayOn = 0, uint32_t _delayOff = 0, void (*_handle)(bool) = NULL);
   
   void setType(TEVENT_TYPE_t _type, uint32_t _timeOn = 0, uint32_t _timeOff = 0);
   void on();
   void on(uint32_t _delay);
   void off();
   void off(uint32_t _delay);
   void reset();
   void copyTo(TEvent *dist);

   bool changeState();
   TEVENT_STATUS_t loop();

   // Публичные поля состояния и настроек
   TEVENT_TYPE_t Type;
   TEVENT_STATUS_t State;
   TEVENT_STATUS_t SaveState;
   bool isChangeState;
   
   uint32_t DelayOn;
   uint32_t DelayOff;
   uint32_t TimeOn;
   uint32_t TimeOff;
   
   void (*Handle)(bool);

private:
   void setOn();
   void setOff();

   uint32_t msOn;  // Время последнего события On/Wait
   uint32_t msOff; // Время последнего события Off/Wait
   bool isOn;      // Флаг текущего состояния (вкл/выкл)
};

/**
 * @class TEventRGB
 * @brief Класс для управления RGB-лентой (NeoPixel) с различными эффектами.
 */
class TEventRGB {
public:
    TEventRGB(uint8_t _pin, int _num, int _first);
    
    void set(uint32_t _color1, uint32_t _color2, uint32_t _color11=COLOR_NONE, uint32_t _color12=COLOR_NONE, uint32_t _timer_on=0, uint32_t _timer_off=0);
    void setNumAp(int _ap = LED_STAT_AP1);
    void setNumSta(int _sta = LED_STAT_STA1);
    void setColor(uint32_t _color1, uint32_t _color2);
    void setColor0(uint32_t _color, bool _blink=false);
    void setColor1(uint32_t _color);
    void setMP3(uint32_t _color);
    void setRainbow(bool _flag, uint32_t _timer=0);
    void setBrightness(int br);
    void copyTo(TEventRGB *dist);
    void set(TEventRGB *src);
    
    uint32_t setBrightness(uint32_t color, uint8_t level);
    TEVENT_RGB_TYPE_t loop();

    TEVENT_STATUS_t State;
    int NumAp, NumSta;

    bool isRainbow;
    bool isMP3;
    bool isBlink0;
    
    uint32_t Color0;
    uint32_t Color1;
    uint32_t Color2; 
    uint32_t ColorBlink1;
    uint32_t ColorBlink2;
    uint32_t ColorMP3;
    
    uint32_t TimerOn;
    uint32_t TimerOff;
    uint32_t TimerRainbow;

private:
    Adafruit_NeoPixel *Strip;
    int      Num;
    int      First;
    
    uint32_t msOn;
    uint32_t msOff;
    uint32_t msRainbowOn;
    
    uint8_t  BlinkCount;
    int      IncRainbow;
    int      HueRainbow;
};

/**
 * @class TEventMP3
 * @brief Класс для управления MP3-плеером (DFPlayer Mini) с приоритетами и событиями.
 */
class TEventMP3 {
public:
    TEventMP3(Stream &_stream, TEVENT_MP3_GPIO_t _gpio=ESM_AUTO, int _pin=-1, void (*_handle)(bool) = NULL);
    ~TEventMP3(); // Деструктор для очистки памяти

    void setVolume(int _volume);
    void setColor(uint32_t _color=COLOR_NONE, uint32_t _timer=0);
    void start(int _dir, int _track, int _priority = DAFAULT_PRIORITY_MP3, uint32_t _delay = DEFAULT_DELAY_MP3, uint32_t _timer = DEFAULT_TIMER_MP3, bool _is_wait = true, bool _loop=false);
    void stop();
    TEVENT_STATUS_t loop();

    // Публичные поля
    uint32_t DelayOn;
    uint32_t TimerOn;
    TEvent *ColorEvent;
    uint32_t Color;
    uint32_t ColorTimer;
    TEVENT_STATUS_t State;
    void (*Handle)(bool);

private:
    void init();
    void _start();
    void _stop();
    void _replay(uint32_t _delay);
    int  state();

    DFRobotDFPlayerMini *Player;
    Stream *SerialPlayer;
    
    bool isPlayer;
    bool isLoop;
    bool waitPlayer;
    bool isOn;
    
    int Dir;
    int Track;
    int Priority;
    
    uint32_t msOn;
    uint32_t msOff;
    uint32_t ms1;

    TEVENT_MP3_GPIO_t GPIO;
    int PIN;    
    bool isLowGpio;
    bool isHighGpio;
};