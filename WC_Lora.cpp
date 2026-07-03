#include "WC_Lora.h"
// Настройка имени модуля для отладки
#define MODULE_NAME "LORA"
#include "src/Slib/SDEBUG.h"
#ifdef IS_LORA

// ============================================================
// ПРИВАТНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ (скрыты от других модулей)
// ============================================================

// Аппаратные ресурсы
static SPIClass LoraSPI(HSPI);
static SX1262 radio = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, LoraSPI);
static MyLoRaBaseClass myLora(chipID);
static uint8_t loraGate[6];

// Состояние модуля
static volatile bool loraIrq = false;
static bool isLora = false;
static bool isLoraACK = false;

// Таймеры отправки (приватные)
static uint32_t msSendLora = 0;
static uint32_t msSendLoraAttr = 0;

// Хендл задачи
static TaskHandle_t loraTaskHandle = NULL;

// Прототип обработчика событий LoRa
static void handleEventLoRa();

// ============================================================
// ПУБЛИЧНЫЙ ИНТЕРФЕЙС
// ============================================================

/**
 * Инициализация подсистемы LoRa
 * Вызывается из основного кода при старте
 */
void loraInit() {
    LOG_INFOLN("Initializing LoRa subsystem");
    
    // Создаем задачу LoRa
    xTaskCreate(
        taskLora,           // Функция задачи
        "taskLora",         // Имя задачи
        4096,               // Размер стека
        NULL,               // Параметры
        15,                 // Приоритет (высокий для приема)
        &loraTaskHandle     // Хендл задачи
    );
    
    LOG_INFOLN("LoRa task created");
}

/**
 * Проверка готовности LoRa
 */
bool loraIsReady() {
    return isLora;
}

/**
 * Принудительная отправка телеметрии
 */
void loraForceSendTelemetry() {
    msSendLora = 0;  // Сброс таймера вызовет немедленную отправку
    LOG_INFOLN("Forced telemetry send requested");
}

/**
 * Проверка наличия ACK
 */
bool loraHasAck() {
    return isLoraACK;
}

/**
 * Сброс флага ACK
 */
void loraClearAck() {
    isLoraACK = false;
}

// ============================================================
// ОБРАБОТЧИК СОБЫТИЙ LORA
// ============================================================

/**
 * Обработчик событий LoRa (аналог handleEventWiFi)
 * Вызывается при получении RPC команд через LoRa
 */
static void handleEventLoRa() {
    String s;
    
    myLora.GetJson();
    serializeJson(myLora.Json, s);
    
    LOG_DEBUGLN("Processing LoRa event: %s", s.c_str());
    
    // Проверяем тип RPC команды
    if (myLora.Json.containsKey("RPC")) {
        const char* rpc = myLora.Json["RPC"].as<const char*>();
        
        if (strcmp(rpc, "test") == 0) {
            LOG_INFOLN("!!! LoRa Event: RPC=test");
            
            // Управление таймерами отправки
            if (myLora.StateRX == NSRX_OK) {
                // Получен прямой пакет с ACK запросом
                msSendLora = millis() + random(30, 100) * 1000;
                msSendLoraAttr = msSendLora;
                LOG_DEBUGLN("Unicast RPC, scheduling reply in %lu ms", 
                           msSendLora - millis());
            }
            else {
                // Broadcast — отвечаем немедленно
                msSendLora = 0;
                msSendLoraAttr = 0;
                LOG_DEBUGLN("Broadcast RPC, sending reply immediately");
            }
            
            // Восстанавливаем состояние сенсора
            lastSensorOn = SS_RESTORE;
        }
        else if (strcmp(rpc, "reboot") == 0) {
            LOG_INFOLN("!!! LoRa Event: RPC=reboot");
            waitMP3andReboot();
        }
        else if (strcmp(rpc, "calibrate") == 0) {
            LOG_INFOLN("!!! LoRa Event: RPC=calibrate");
            startCalibrate(0, "97", 97);
        }
        else {
            LOG_INFOLN("!!! LoRa Event: Unknown RPC=%s", rpc);
        }
    }
    else {
        LOG_DEBUGLN("No RPC key in received JSON");
    }
}

// ============================================================
// ЗАДАЧА LORA
// ============================================================

/**
 * Основная задача LoRa
 * - Прием пакетов
 * - Отправка телеметрии по расписанию
 * - Обработка RPC команд
 */
void taskLora(void *pvParameters) {
    LOG_INFOLN("LoRa task started");
    
    // Инициализация модуля
    initLoraModule();
    
    uint32_t msCheck = 0;
    
    while (true) {
        uint32_t ms = millis();
        
        // Проверка прерывания (пакет получен)
        if (loraIrq) {
            loraIrq = false;
            readLora();
            
            // Обработка RPC команд (если есть)
            if (myLora.StateRX == NSRX_OK || myLora.StateRX == NSRX_BROADCAST) {
                uint8_t packetType = myLora.HeaderRX_V3.Type & B000111;
                
                if (packetType == PACKET_V3_TYPE_JSON_RPC) {
                    handleEventLoRa();
                }
                else if (packetType == PACKET_V3_TYPE_ACK) {
                    LOG_INFOLN("LoRa ACK received");
                    isLoraACK = true;
                }
            }
        }
        
        // Периодическая проверка необходимости отправки (каждую секунду)
        if (ms - msCheck >= 1000) {
            msCheck = ms;
            
            if (!isLora) {
                vTaskDelay(100);
                continue;
            }
            
            // Проверка включена ли отправка через конфиг
            if (!config["config2"]["LORA_ENABLE"].as<bool>()) {
                continue;
            }
            
            uint32_t sendPeriod = config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
            uint32_t retryPeriod = config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
            
            // Отправка телеметрии
            if (msSendLora == 0 || ms >= msSendLora) {
                if (isLoraACK && msSendLora != 0) {
                    // Был ACK — ждем следующий период
                    isLoraACK = false;
                    msSendLora = ms + sendPeriod;
                    LOG_DEBUGLN("ACK received, next send at %lu", msSendLora);
                }
                else {
                    // Отправляем сейчас
                    LOG_DEBUGLN("Sending telemetry");
                    if (sendLoraTelemetry()) {
                        msSendLora = ms + sendPeriod;
                    }
                    else {
                        msSendLora = ms + retryPeriod;
                    }
                }
            }
        }
        
        vTaskDelay(50);  // Небольшая задержка для предотвращения вачдога
    }
}

// ============================================================
// ПРИВАТНЫЕ ФУНКЦИИ МОДУЛЯ
// ============================================================

/**
 * Обработчик прерывания LoRa
 */
void IRAM_ATTR onLoraIrq() {
    loraIrq = true;
    detachInterrupt(PIN_LORA_DIO1);
    
    // Уведомляем задачу о прерывании
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (loraTaskHandle) {
        vTaskNotifyGiveFromISR(loraTaskHandle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * Инициализация аппаратного модуля LoRa
 */
void initLoraModule() {
    LOG_INFOLN("LoRa hardware initialization started");
    
    pinMode(PIN_LORA_DIO1, INPUT);
    
    LoraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);
    int state = radio.begin();
    
    if (state == RADIOLIB_ERR_NONE) {
        LOG_INFOLN("LoRa module initialized successfully");
        isLora = true;
        
        radio.setFrequency(868.0);
        radio.setSpreadingFactor(7);
        radio.setBandwidth(125.0);
        radio.setCodingRate(5);
        
        MyLoRaAddress::Set(myLora.Addr, chipID);
        MyLoRaAddress::SetBroadcast(loraGate);
        
        setLoraReceive(true);
        
        LOG_DEBUGLN("Config: Freq=868.0 SF=7 BW=125.0 CR=5");
    }
    else {
        isLora = false;
        LOG_ERRORLN("LoRa init failed, error code: %d", state);
    }
}

/**
 * Чтение принятых пакетов
 */
void readLora() {
    if (!isLora) return;
    
    uint8_t buf[MAX_LEN_PAYLOAD];
    int len = radio.getPacketLength();
    
    if (len > MAX_LEN_PAYLOAD) {
        LOG_ERRORLN("Packet too large: %d > %d", len, MAX_LEN_PAYLOAD);
        len = MAX_LEN_PAYLOAD;
    }
    
    int state = radio.readData(buf, len);
    
    if (state == RADIOLIB_ERR_NONE && len > 0) {
        LOG_DEBUGLN("Received %d bytes, RSSI: %d", len, radio.getRSSI());
        myLora.RX(buf, len, radio.getRSSI());
        myLora.PrintRX_V3();
    }
    else if (state != RADIOLIB_ERR_NONE) {
        LOG_ERRORLN("Read failed, error: %d", state);
    }
    
    setLoraReceive(true);
}

/**
 * Отправка телеметрии
 */
bool sendLoraTelemetry() {
    LOG_DEBUGLN("Preparing telemetry");
    
    myLora.SetHeaderTX(PACKET_V3_TYPE_JSON_TELEMETRY, loraGate, true);
    myLora.Json["Distance"] = (int)Distance;
    myLora.Json["State"] = getStatus();
    myLora.Json["Uptime"] = esp_timer_get_time() / 1000000;
    myLora.Json["DN"] = config["config2"]["N_DOGIVOR"].as<String>();
    myLora.Json["BN"] = config["config2"]["N_BOX"].as<String>();
    
    if (serNo[0] != '\0') {
        myLora.Json["SN"] = serNo;
    }
    
    if (!myLora.SetJsonBodyTX()) {
        LOG_ERRORLN("Failed to serialize JSON");
        return false;
    }
    
    setLoraReceive(false);
    int state = radio.transmit(myLora.BufferTX, myLora.LengthTX);
    setLoraReceive(true);
    
    if (state == RADIOLIB_ERR_NONE) {
        LOG_INFOLN("Telemetry sent successfully");
        myLora.PrintTX_V3();
        return true;
    }
    else {
        LOG_ERRORLN("TX error: %d", state);
        return false;
    }
}

/**
 * Отправка атрибутов
 */
bool sendLoraAttributes() {
    LOG_DEBUGLN("Preparing attributes");
    
    myLora.SetHeaderTX(PACKET_V3_TYPE_JSON_ATTRIBUTE, myLora.AddrRX, true);
    myLora.Json["SerialNo"] = serNo;
    myLora.Json["DogovorNo"] = config["config2"]["N_DOGIVOR"].as<String>();
    myLora.Json["BoxNo"] = config["config2"]["N_BOX"].as<String>();
    
    if (!myLora.SetJsonBodyTX()) {
        LOG_ERRORLN("Failed to serialize JSON");
        return false;
    }
    
    setLoraReceive(false);
    int state = radio.transmit(myLora.BufferTX, myLora.LengthTX);
    setLoraReceive(true);
    
    if (state == RADIOLIB_ERR_NONE) {
        LOG_INFOLN("Attributes sent successfully");
        myLora.PrintTX_V3();
        return true;
    }
    else {
        LOG_ERRORLN("TX error: %d", state);
        return false;
    }
}

/**
 * Управление режимом приема
 */
void setLoraReceive(bool flag) {
    if (flag) {
        attachInterrupt(PIN_LORA_DIO1, onLoraIrq, RISING);
        radio.startReceive();
    }
    else {
        radio.standby();
        detachInterrupt(PIN_LORA_DIO1);
    }
}

/**
 * Ожидание приема с таймаутом
 */
bool waitLoraRead(uint32_t timeout_ms) {
    for (uint32_t i = 0; i < timeout_ms; i += 10) {
        if (loraIrq) {
            loraIrq = false;
            LOG_DEBUGLN("Wait triggered after %lu ms", i);
            readLora();
            return true;
        }
        vTaskDelay(10);
    }
    
    LOG_DEBUGLN("Wait timeout after %lu ms", timeout_ms);
    return false;
}

#endif // IS_LORA