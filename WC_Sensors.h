#pragma once

#include <Wire.h>
#include "MyConfig.h"
#include "WC_Config.h"
#include "src/SonarSR04/SonarSR04.h"
#include "src/TF/TFLI2C.h"
#include "src/LD2413/LD2413.h"


/**
 * @brief Класс для хранения и обработки значений датчика
 * 
 * Реализует кольцевой буфер для усреднения показаний,
 * проверку лимитов и обработку ошибок
 */
class MySensorValue {
   public:
      MySensorValue();
      
      /**
       * @brief Инициализация параметров значения
       * @param _label Метка/название значения
       * @param _min Минимально допустимое значение
       * @param _max Максимально допустимое значение
       * @param _mult Множитель для масштабирования
       * @param _samples Количество семплов для усреднения
       */
      void init(String _label, float _min, float _max, uint16_t _mult, uint16_t _samples);
      
      /**
       * @brief Очистка буфера значений
       */
      void clear();
      
      /**
       * @brief Установка нового значения
       * @param _val Новое значение
       * @param flag Флаг записи в буфер (true - записывать)
       * @return true если значение валидно
       */
      bool set(float _val, bool flag = true);
      
      /**
       * @brief Получение последнего валидного значения
       * @return Последнее значение или ValueError при ошибке
       */
      float getLast();
      
      String   Label;        // Название значения
      float    Value;        // Текущее значение
      uint16_t Multiplier;   // Множитель
      float    LimitMin;     // Минимальный предел
      float    LimitMax;     // Максимальный предел
      uint16_t Samples;      // Количество семплов
      
   private:
      float    ValueError;   // Значение при ошибке (NAN)
      bool     isFirst;      // Флаг первого измерения
      uint8_t  Pointer;      // Указатель в кольцевом буфере
      float    *Values;      // Кольцевой буфер значений
};

/**
 * @brief Основной класс для работы с датчиками расстояния
 * 
 * Поддерживает различные типы датчиков:
 * - Ультразвуковые (SR04T, SR04TM2, SR04_75)
 * - Лидары (TFLuna I2C)
 * - Радары (LD2413 UART)
 */
class MySensor {
   public:
      String Name;                    // Имя датчика
      uint16_t count_test;            // Счетчик тестов
      MySensorValue *Value;           // Указатель на значение
      SensorType_t Type;              // Тип датчика
      
      /**
       * @brief Конструктор
       * @param _type Тип датчика из перечисления SensorType_t
       */
      MySensor(SensorType_t _type);
      
      /**
       * @brief Проверка и обновление типа датчика
       * @param _type Новый тип датчика
       */
      void checkType(SensorType_t _type);
      
      /**
       * @brief Инициализация датчика
       * @return true если инициализация успешна
       */
      bool init();
      
      /**
       * @brief Получение данных с датчика
       * @return true если данные получены успешно
       */
      bool get();
      
   private:
      bool isInit;              // Флаг инициализации
      
      // Указатели на объекты датчиков (создаются сразу все)
      SonarSR04 *SensorSR04;     // Ультразвуковой датчик SR04T
      SonarSR04 *SensorSR04M2;   // Ультразвуковой датчик SR04TM2
      SonarSR04 *SensorSR04_75;  // Ультразвуковой датчик SR04_75
      TFLI2C    *SensorTFLuna;   // Лидар TFLuna по I2C
      LD2413    *SensorLD2413;   // Радар LD2413 по UART
      
      /**
       * @brief Проверка наличия устройства на I2C шине
       * @param _addr I2C адрес устройства
       * @return true если устройство отвечает
       */
      bool checkI2C(uint8_t _addr);
      
      /**
       * @brief Сканирование I2C шины
       */
      void scanI2C();
};