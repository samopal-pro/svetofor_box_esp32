#pragma once

#include <Arduino.h>

// Таймаут ожидания эхо-сигнала (мкс)
// 500000 мкс = 0.5 сек = максимальная дистанция ~86 метров
#define ECHO_TIMEOUT_US 500000UL

// Класс для управления ультразвуковым дальномером HC-SR04
class SonarSR04
{
public:
    /**
     * Конструктор
     * @param echoPin   - пин для приема эхо-сигнала
     * @param trigPin   - пин для отправки триггер-сигнала
     * @param trigPulseUs - длительность триггер-импульса (мкс), по умолчанию 10
     * @param preDelayUs  - задержка перед триггером (мкс), по умолчанию 2
     */
    SonarSR04(uint8_t echoPin, 
              uint8_t trigPin, 
              uint32_t trigPulseUs = 10, 
              uint32_t preDelayUs = 2);
    
    /**
     * Инициализация пинов
     * Должна быть вызвана в setup()
     */
    void init();
    
    /**
     * Получение дистанции в сантиметрах
     * @return расстояние в см, или 0 при ошибке/таймауте
     */
    float getDistance();
    
    /**
     * Получение последнего статуса измерения
     * @return 0 - успех, 1 - таймаут
     */
    uint8_t getLastStatus() const { return lastStatus; }

private:
    uint8_t trigPin;      // Пин для триггера
    uint8_t echoPin;      // Пин для эхо
    uint32_t trigPulseUs; // Длительность триггер-импульса (мкс)
    uint32_t preDelayUs;  // Задержка перед импульсом (мкс)
    uint8_t lastStatus;   // Статус последнего измерения
};