// Определяем MODULE_NAME до включения SDEBUG.h
#define MODULE_NAME "SENSOR"
#define MODULE_DEBUG_LEVEL DEBUG_OFF
#include "src/Slib/SDEBUG.h"
#include "WC_Sensors.h"

// ===== MySensorValue =====

/**
 * @brief Конструктор класса MySensorValue
 * 
 * Инициализирует параметры по умолчанию и выделяет память
 * под кольцевой буфер значений
 */
MySensorValue::MySensorValue(){
   init("", 0, 10000, 1, SIMPLE_SIZE);
   ValueError = NAN;
   Values = (float *)malloc(SIMPLE_SIZE * sizeof(float));
   if (Values == nullptr) {
      LOG_ERRORLN("Failed to allocate memory for Values buffer!");
   }
   clear();
}

/**
 * @brief Инициализация параметров значения датчика
 * 
 * @param _label Метка/название значения
 * @param _min Минимально допустимое значение
 * @param _max Максимально допустимое значение
 * @param _mult Множитель для масштабирования значения
 * @param _samples Количество семплов для кольцевого буфера
 * 
 * @note Если _samples превышает SIMPLE_SIZE, используется SIMPLE_SIZE
 * @note Если _samples равен 0, устанавливается в 1
 */
void MySensorValue::init(String _label, float _min, float _max, uint16_t _mult, uint16_t _samples){
   Label = _label;
   LimitMin = _min;
   LimitMax = _max;
   
   // Проверка и коррекция количества семплов
   if (_samples > SIMPLE_SIZE) {
      _samples = SIMPLE_SIZE;
   }
   if (_samples == 0) {
      _samples = 1;
   }
   Samples = _samples;  // Исправлено: было Samples = SIMPLE_SIZE (всегда использовался максимум)
   Multiplier = _mult;
}

/**
 * @brief Очистка кольцевого буфера значений
 * 
 * Заполняет буфер значениями NAN и сбрасывает флаги
 */
void MySensorValue::clear(){
   if (Values != nullptr) {
      for (int i = 0; i < Samples; i++) {
         Values[i] = NAN;
      }
   }
   isFirst = true;
   Pointer = 0;
   Value = NAN;
}

/**
 * @brief Установка нового значения датчика
 * 
 * @param _val Новое значение
 * @param _flag Флаг записи в кольцевой буфер (true - записывать)
 * @return true если значение прошло проверку лимитов
 * 
 * Алгоритм:
 * 1. Применяет множитель к значению
 * 2. Проверяет на NAN и выход за пределы
 * 3. При первом измерении заполняет весь буфер текущим значением
 * 4. При последующих - записывает в кольцевой буфер
 */
bool MySensorValue::set(float _val, bool _flag){
   // Применяем множитель, если значение не NAN
   if (!isnan(_val)) {
      _val *= Multiplier;
   }
   
   // Проверка на выход за пределы
   if (isnan(_val) || _val < LimitMin || _val > LimitMax) {
      Value = NAN;
      return false;  
   }
   
   Value = _val;
   
   // Если не нужно записывать в буфер - выходим
   if (!_flag) {
      return false;  
   }
   
   // Первое измерение - заполняем весь буфер
   if (isFirst) {
      isFirst = false;
      for (int i = 0; i < Samples; i++) {
         Values[i] = _val;
      }
   } else {
      // Запись в кольцевой буфер
      Values[Pointer++] = _val;
      if (Pointer >= Samples) {
         Pointer = 0;
      }
   }
   return true;
}

/**
 * @brief Получение последнего валидного значения
 * 
 * @return Текущее значение или ValueError при ошибке/первом запуске
 */
float MySensorValue::getLast(){
   if (isFirst) {
      return ValueError;
   }
   if (isnan(Value)) {
      return ValueError;
   }
   return Value;
}

// ===== MySensor =====

/**
 * @brief Конструктор класса MySensor
 * 
 * Создает объекты всех поддерживаемых датчиков и инициализирует
 * хранилище значений. Выбор активного датчика происходит через checkType()
 * 
 * @param _type Тип датчика из перечисления SensorType_t
 */
MySensor::MySensor(SensorType_t _type){
   // Создаем объекты всех датчиков
   SensorSR04    = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 10);
   SensorSR04M2  = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 500);
   SensorSR04_75 = new SonarSR04(PIN_SONAR_ECHO, PIN_SONAR_TRIG, 2, 500);
   SensorTFLuna  = new TFLI2C();
   SensorLD2413  = new LD2413();
   
   Value = new MySensorValue();
   isInit = false;
   checkType(_type);
   
   LOG_INFOLN("Sensor object created");
}

/**
 * @brief Проверка и изменение типа датчика
 * 
 * Если тип изменился, сбрасывает флаг инициализации
 * 
 * @param _type Новый тип датчика
 */
void MySensor::checkType(SensorType_t _type){
   if (_type != Type) {
      LOG_INFOLN("Changing sensor type from %d to %d", Type, _type);
      isInit = false;
      Type = _type;
   }
}

/**
 * @brief Инициализация выбранного датчика
 * 
 * Настраивает параметры в зависимости от типа датчика:
 * - Ультразвуковые: настройка пинов и лимитов
 * - I2C датчики: проверка наличия на шине
 * - UART датчики: настройка последовательного порта
 * 
 * @return true если инициализация успешна
 */
bool MySensor::init(){
   switch(Type) {
      case SENSOR_SR04T:  
         Value->init("Дистанция, мм", 100.0, 4900.0, 1, SIMPLE_SIZE);
         SensorSR04->init();
         isInit = true;
         Name = "Sonar SR04T";  // Исправлено: было "Sonar SR04TM2"
         break;
         
      case SENSOR_SR04TM2:  
         Value->init("Дистанция, мм", 100.0, 5900.0, 1, SIMPLE_SIZE);
         SensorSR04M2->init();
         isInit = true;
         Name = "Sonar SR04TM2";
         break;
         
      case SENSOR_SR04_75:  
         Value->init("Дистанция, мм", 100.0, 7400.0, 1, SIMPLE_SIZE);
         SensorSR04_75->init();  // Исправлено: было SensorSR04M2->init()
         isInit = true;
         Name = "Sonar SR04_75";
         break;
         
      case SENSOR_TFLUNA_I2C:
         Wire.begin(PIN_SONAR_ECHO, PIN_SONAR_TRIG);
         Wire.setClock(100000);
         vTaskDelay(200);
         isInit = checkI2C(TFL_DEF_ADR);
         if (isInit) {
            Value->init("Дистанция, см", 0.0, 800.0, 1, SIMPLE_SIZE);  // Добавлена инициализация
         }
         Name = "Lidar TFLuna";
         break;
         
      case SENSOR_LD2413_UART:
         Serial2.begin(115200, SERIAL_8N1, PIN_SONAR_TRIG, PIN_SONAR_ECHO);
         SensorLD2413->begin(PIN_SONAR_TRIG, PIN_SONAR_ECHO);
         SensorLD2413->init(150, 10000, 250);
         Value->init("Дистанция, см", 0.0, 1000.0, 1, SIMPLE_SIZE);  // Добавлена инициализация
         isInit = true;
         Name = "Radar LD2413";
         break;
         
      default:
         LOG_ERRORLN("Unknown sensor type: %d", Type);
         isInit = false;
         break;
   }

   if (isInit) {
      LOG_INFOLN("Init success: %s", Name.c_str());
   } else {
      LOG_ERRORLN("Init failed: %s", Name.c_str());
   }
   
   return isInit;
}

/**
 * @brief Получение данных с датчика
 * 
 * Выполняет чтение данных в зависимости от типа датчика
 * и сохраняет результат в Value
 * 
 * @return true если данные успешно получены и валидны
 */
bool MySensor::get(){
   float value1 = NAN, value2 = NAN;
   bool stat = false;
   static int16_t _dist16, _flux16, _temp16;
   
   // Автоматическая инициализация при первом вызове
   if (!isInit) {
      if (!init()) {
         return false;
      }
   }
   
   switch(Type) {
      case SENSOR_SR04T:
         value1 = SensorSR04->getDistance();
         stat = Value->set(value1, true);
         break;
         
      case SENSOR_SR04TM2:
         value1 = SensorSR04M2->getDistance();
         stat = Value->set(value1, true);
         break;
         
      case SENSOR_SR04_75:
         value1 = SensorSR04_75->getDistance();
         stat = Value->set(value1, true);
         break;
         
      case SENSOR_TFLUNA_I2C:    
         stat = SensorTFLuna->getData(_dist16, _flux16, _temp16, TFL_DEF_ADR);
         if (stat) {
            value1 = (float)_dist16;
         } else {
            value1 = NAN;
         }
         Value->set(value1, stat);
         break;
         
      case SENSOR_LD2413_UART:
         value1 = SensorLD2413->waitForData(100);
         if (value1 != 0) {
            Value->set(value1);
            stat = true;
         } else {
            Value->set(NAN);
            stat = false;
         }
         break;
         
      default:
         Value->set(NAN);
         stat = false;
         LOG_ERRORLN("Unknown sensor type: %d", Type);
   }

   LOG_DEBUGLN("Sensor reading: %s = %.1f", Name.c_str(), Value->getLast());

   return stat;
}

/**
 * @brief Проверка наличия устройства на I2C шине
 * 
 * @param _addr I2C адрес для проверки
 * @return true если устройство ответило
 */
bool MySensor::checkI2C(uint8_t _addr){
   bool _ret = false;
   Wire.beginTransmission(_addr);
   if (Wire.endTransmission() == 0) {
      _ret = true;
   }
   
   LOG_DEBUGLN("I2C check: ADDR=0x%02X - %s", _addr, _ret ? "OK" : "FAIL");
   
   return _ret;
}

/**
 * @brief Сканирование всех устройств на I2C шине
 * 
 * Выводит список найденных устройств
 */
void MySensor::scanI2C(){
   int nDevices = 0;
   
   LOG_INFOLN("Scanning I2C bus...");
   
   for (uint8_t address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      int error = Wire.endTransmission();
      
      switch(error) {
         case 0:
            LOG_INFOLN("I2C device found at address 0x%02X", address);
            nDevices++;
            break;
         case 4:
            LOG_ERRORLN("Unknown error at address 0x%02X", address);
            break;
         default:
            // Устройство не ответило - нормальная ситуация
            break;
      }    
   }
   
   if (nDevices == 0) {
      LOG_INFOLN("No I2C devices found");
   } else {
      LOG_INFOLN("Found %d I2C device(s)", nDevices);
   }
}

