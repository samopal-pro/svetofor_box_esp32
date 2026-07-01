#include "SBTN_ESP32.h"

#ifdef ESP32

// Инициализация статических членов
SBTN_ESP32* SBTN_ESP32::instances[10] = { nullptr };
uint8_t SBTN_ESP32::instancesCount = 0;

//-----------------------------------------------------------------------------
// Конструкторы
//-----------------------------------------------------------------------------
SBTN_ESP32::SBTN_ESP32(uint8_t _pin, uint32_t _bounce, bool _pullup_state, bool _press_state)
   : SBTN(_pin, _bounce, _pullup_state, _press_state), 
     semaphore(nullptr), taskHandle(nullptr), isRunning(false), 
     taskStackSize(2048), taskPriority(5)
{
   semaphore = xSemaphoreCreateBinary();
   if (semaphore == nullptr) {
      // Ошибка создания семафора
   }
   registerInstance();
}

SBTN_ESP32::SBTN_ESP32(uint8_t _pin, uint16_t _threshold, uint32_t _bounce)
   : SBTN(_pin, _threshold, _bounce), 
     semaphore(nullptr), taskHandle(nullptr), isRunning(false),
     taskStackSize(2048), taskPriority(5)
{
   semaphore = xSemaphoreCreateBinary();
   if (semaphore == nullptr) {
      // Ошибка создания семафора
   }
   registerInstance();
}

SBTN_ESP32::~SBTN_ESP32() {
   stop();
   if (semaphore) {
      vSemaphoreDelete(semaphore);
      semaphore = nullptr;
   }
   unregisterInstance();
}

//-----------------------------------------------------------------------------
// Регистрация и поиск экземпляров
//-----------------------------------------------------------------------------
void SBTN_ESP32::registerInstance() {
   if (instancesCount < 10) {
      instances[instancesCount++] = this;
   }
}

void SBTN_ESP32::unregisterInstance() {
   for (uint8_t i = 0; i < instancesCount; i++) {
      if (instances[i] == this) {
         for (uint8_t j = i; j < instancesCount - 1; j++) {
            instances[j] = instances[j + 1];
         }
         instancesCount--;
         break;
      }
   }
}

SBTN_ESP32* SBTN_ESP32::findInstanceByPin(uint8_t pin) {
   for (uint8_t i = 0; i < instancesCount; i++) {
      if (instances[i] && instances[i]->getPin() == pin) {
         return instances[i];
      }
   }
   return nullptr;
}

//-----------------------------------------------------------------------------
// Управление прерываниями
//-----------------------------------------------------------------------------
void SBTN_ESP32::enableInterrupt() {
   if (getPin() == 0) return; // Некорректный пин
   
   // Проверяем, является ли кнопка сенсорной
   if (getPin() >= 0 && getPin() <= 9) { // Обычно Touch пины ESP32: 0-9
      // Пробуем как Touch (если это touch-кнопка)
      touchAttachInterruptArg(getPin(), isrTouchHandler, this, 0);
   } else {
      // GPIO прерывание
      gpio_config_t io_conf = {};
      io_conf.pin_bit_mask = (1ULL << getPin());
      io_conf.mode = GPIO_MODE_INPUT;
      io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
      io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io_conf.intr_type = GPIO_INTR_NEGEDGE; // Спад
      gpio_config(&io_conf);
      gpio_install_isr_service(0);
      gpio_isr_handler_add((gpio_num_t)getPin(), isrHandler, this);
   }
}

void SBTN_ESP32::disableInterrupt() {
   if (getPin() == 0) return;
   
   if (getPin() >= 0 && getPin() <= 9) {
      touchDetachInterrupt(getPin());
   } else {
      gpio_isr_handler_remove((gpio_num_t)getPin());
   }
}

//-----------------------------------------------------------------------------
// Обработчики прерываний (IRAM)
//-----------------------------------------------------------------------------
void IRAM_ATTR SBTN_ESP32::isrHandler(void* arg) {
   SBTN_ESP32* self = (SBTN_ESP32*)arg;
   if (self && self->semaphore) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xSemaphoreGiveFromISR(self->semaphore, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
   }
}

void IRAM_ATTR SBTN_ESP32::isrTouchHandler(void* arg) {
   SBTN_ESP32* self = (SBTN_ESP32*)arg;
   if (self && self->semaphore) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xSemaphoreGiveFromISR(self->semaphore, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
   }
}

//-----------------------------------------------------------------------------
// Задача FreeRTOS
//-----------------------------------------------------------------------------
void SBTN_ESP32::taskWrapper(void* params) {
   SBTN_ESP32* self = (SBTN_ESP32*)params;
   self->taskLoop();
}

void SBTN_ESP32::taskLoop() {
   while (isRunning) {
      // Ожидание прерывания
      if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE) {
         // Отключаем прерывание
         disableInterrupt();
         
         // Обрабатываем все события кнопки
         bool finished = false;
         while (!finished) {
            SBTN_EVENT_t evt = loop();
            
            // Выход из цикла, если кнопка отпущена и нет событий
            if (!isPress() && evt == SB_NONE) {
               finished = true;
            } else {
               vTaskDelay(pdMS_TO_TICKS(10));
            }
         }
         
         // Включаем прерывание
         enableInterrupt();
      }
   }
}

//-----------------------------------------------------------------------------
// Запуск / Остановка
//-----------------------------------------------------------------------------
void SBTN_ESP32::begin(uint32_t stackSize, UBaseType_t priority) {
   if (taskHandle != nullptr) return;
   
   taskStackSize = stackSize;
   taskPriority = priority;
   isRunning = true;
   
   enableInterrupt();
   xTaskCreate(taskWrapper, "SBTN_ESP32", taskStackSize, this, taskPriority, &taskHandle);
}

void SBTN_ESP32::stop() {
   isRunning = false;
   if (taskHandle != nullptr) {
      vTaskDelete(taskHandle);
      taskHandle = nullptr;
   }
   disableInterrupt();
}

#endif // ESP32