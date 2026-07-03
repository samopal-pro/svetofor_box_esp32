#define MODULE_NAME "NET"
#define MODULE_DEBUG_LEVEL DEBUG_DEFAULT
#include "src/Slib/SDEBUG.h"
#include "WC_Net.h"

// ============================================================
// ПРИВАТНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ NET
// ============================================================

// Состояние WiFi соединения
bool isAP = false;                // Флаг активности режима точки доступа
bool isSTA = false;               // Флаг активности режима клиента
bool isWiFiConnected = false;     // Флаг успешного подключения к WiFi сети
static uint32_t msSTA = 0;        // Время последнего запуска STA режима (для таймаута подключения)

// JSON документ для формирования HTTP запросов (переиспользуется)
static JsonDocument jsonData;

// ============================================================
// ЗАДАЧА УПРАВЛЕНИЯ WiFi
// ============================================================

/**
 * Задача управления WiFi подключением
 * 
 * Отвечает за:
 * - Включение/выключение режима точки доступа (AP)
 * - Подключение к внешним WiFi сетям (STA)
 * - Мониторинг состояния подключения
 * - Обработку ошибок подключения
 * - Индикацию состояния через RGB светодиоды
 * 
 * Не отвечает за:
 * - Отправку HTTP запросов (этим занимается taskHttpSender)
 * - Обработку веб-интерфейса
 */
void taskWiFiManager(void *pvParameters) {
    LOG_INFOLN("WiFi Manager task started");
    
    // Инициализация WiFi в выключенном состоянии
    WiFi.mode(WIFI_OFF);
    
    // Начальная настройка светодиодов
    EventRGB1->setColor0(COLOR_BLACK);  // Индикатор AP
    EventRGB1->setColor1(COLOR_BLACK);  // Индикатор STA
    
    // Начальные флаги состояния
    isAP = true;              // По умолчанию включаем точку доступа
    isWiFiConnected = false;  // WiFi клиент ещё не подключен
    
    // Счетчики и таймеры для управления
    int wifiError = 0;                // Счетчик ошибок подключения
    uint32_t msLastStaCheck = 0;      // Время последней проверки STA
    wifi_mode_t lastWiFiMode = WIFI_OFF;  // Предыдущий режим WiFi для логирования
    
    // Настройка мощности передатчика WiFi из конфигурации
    if (!config["config2"]["WIFI_POWER"].isNull()) {
        wifi_power_t power = (wifi_power_t)config["config2"]["WIFI_POWER"].as<int>();
        WiFi.setTxPower(power);
        LOG_DEBUGLN("WiFi power set to: %d", power);
    }
    
    // Главный цикл задачи
    while (true) {
        // Блокировка при калибровке или перезагрузке датчика
        if (isSensorBlock || calibrMode == CM_WAIT_REBOOT) {
            vTaskDelay(1000);
            continue;
        }
        
        uint32_t ms = millis();
        wifi_mode_t curWiFi = WiFi.getMode();
        
        // Логирование изменений режима WiFi для отладки
        if (curWiFi != lastWiFiMode) {
            LOG_DEBUGLN("WiFi mode changed: %d -> %d", lastWiFiMode, curWiFi);
            lastWiFiMode = curWiFi;
        }
        
        // Чтение конфигурации: нужно ли быть клиентом WiFi
        isSTA = (config["config2"]["WIFI_MODE"].as<int>() != STA_OFF);
        
        // ========================================
        // УПРАВЛЕНИЕ РЕЖИМОМ ТОЧКИ ДОСТУПА (AP)
        // ========================================
        
        // Включение AP если он нужен, но ещё не активен
        if (isAP && (curWiFi != WIFI_AP && curWiFi != WIFI_AP_STA)) {
            LOG_INFOLN("Starting AP mode");
            
            // Сканирование сети для выбора оптимального канала
            WiFi_ScanNetwork();
            WiFi.enableAP(true);
            
            // Получение имени точки доступа из конфигурации
            String ap_name = config["config2"]["ESP_NAME"].as<String>();
            if (ap_name == "") {
                ap_name = deviceName();  // Если имя не задано - используем имя устройства
            }
            
            // Запуск точки доступа и веб-сервера
            WiFi.softAP(ap_name);
            HTTPD_start();
            
            LOG_INFOLN("AP started: %s", ap_name.c_str());
            LOG_INFOLN("AP IP address: %s", WiFi.softAPIP().toString().c_str());
            
            // Индикация: разный цвет при первом запуске и перезагрузке
            if (bootCount < 0) {
                EventRGB1->setColor0(COLOR_WIFI_AP);   // Первый запуск
            } else {
                EventRGB1->setColor0(COLOR_WIFI_AP1);  // Перезагрузка
            }
        }
        
        // Выключение AP если он не нужен, но ещё активен
        if (!isAP && (curWiFi == WIFI_AP || curWiFi == WIFI_AP_STA)) {
            LOG_INFOLN("Stopping AP mode");
            WiFi.enableAP(false);
            EventRGB1->setColor0(COLOR_BLACK);
            LOG_DEBUGLN("AP mode disabled");
        }
        
        // ========================================
        // УПРАВЛЕНИЕ РЕЖИМОМ КЛИЕНТА (STA)
        // ========================================
        // Проверка раз в секунду для быстрой реакции на изменения
        if (TIME_DIFF_MS(ms, msLastStaCheck) > 1000) {
            msLastStaCheck = ms;
            
            // ---- ЗАПУСК РЕЖИМА STA ----
            if (isSTA && (curWiFi != WIFI_STA && curWiFi != WIFI_AP_STA)) {
                LOG_INFOLN("Starting STA mode");
                msSTA = ms;  // Запоминаем время запуска для таймаута
                
                WiFi_ScanNetwork();
                WiFi.enableSTA(true);
                
                // Настройка статического IP если требуется
                if (config["config2"]["STATIC_IP"].as<bool>()) {
                    IPAddress ip_addr, ip_mask, ip_gate, ip_dns;
                    
                    if (ip_addr.fromString(config["config2"]["IP_ADDR"].as<String>()) &&
                        ip_mask.fromString(config["config2"]["IP_MASK"].as<String>()) &&
                        ip_gate.fromString(config["config2"]["IP_GATE"].as<String>()) &&
                        ip_dns.fromString(config["config2"]["IP_DNS"].as<String>())) {
                        
                        WiFi.config(ip_addr, ip_gate, ip_mask, ip_dns);
                        LOG_INFOLN("Static IP configured: %s", 
                                 config["config2"]["IP_ADDR"].as<String>().c_str());
                    }
                }
                
                // Выбор режима подключения: фиксированная сеть или авто-выбор
                if (config["config2"]["WIFI_MODE"].as<int>() == STA_ON) {
                    // Подключение к фиксированной сети
                    LOG_INFOLN("Connecting to primary WiFi: %s", 
                             config["config2"]["WIFI_NAME1"].as<String>().c_str());
                    WiFi.begin(config["config2"]["WIFI_NAME1"].as<String>(), 
                             config["config2"]["WIFI_PASS1"].as<String>());
                }
                else if (config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO) {
                    // Автоматический выбор сети
                    LOG_INFOLN("Auto-connecting to secondary WiFi: %s", 
                             config["config2"]["WIFI_NAME2"].as<String>().c_str());
                    WiFi.begin(config["config2"]["WIFI_NAME2"].as<String>(), 
                             config["config2"]["WIFI_PASS2"].as<String>());
                    
                    // Проверка количества ошибок подключения
                    if (wifiError > 3) {
                        LOG_ERRORLN("Too many WiFi errors (%d), disabling STA mode", wifiError);
                        systemMP3("60", 62, PRIORITY_MP3_MEDIUM);  // Звуковое оповещение
                        config["config2"]["WIFI_MODE"] = STA_OFF;
                        configWrite();
                    }
                    else {
                        systemMP3("60", 61, PRIORITY_MP3_MEDIUM);  // Звук попытки подключения
                        wifiError++;
                        LOG_DEBUGLN("WiFi error counter: %d", wifiError);
                    }
                }
                
                // Включаем индикацию ожидания подключения
                EventRGB1->setColor1(COLOR_WIFI_WAIT);
                LOG_DEBUGLN("STA mode started, waiting for connection...");
            }
            
            // ---- МОНИТОРИНГ СОСТОЯНИЯ STA ----
            if (isSTA && (curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA)) {
                wl_status_t status = WiFi.status();
                
                // Успешное подключение
                if (status == WL_CONNECTED) {
                    if (!isWiFiConnected) {
                        // Первое успешное подключение или переподключение
                        isWiFiConnected = true;
                        isSendNet = true;  // Сигнал для сброса таймеров HTTP отправки
                        
                        LOG_INFOLN("WiFi connected successfully!");
                        LOG_INFOLN("IP address: %s", WiFi.localIP().toString().c_str());
                        LOG_DEBUGLN("RSSI: %d dBm", WiFi.RSSI());
                        
                        // При авто-подключении сохраняем успешную сеть как основную
                        if (config["config2"]["WIFI_MODE"].as<int>() == STA_AUTO) {
                            systemMP3("60", 60, PRIORITY_MP3_MEDIUM);  // Звук успеха
                            EventRGB1->setColor1(COLOR_WIFI_ON);
                            
                            // Сохраняем параметры успешной сети
                            config["config2"]["WIFI_NAME1"] = config["config2"]["WIFI_NAME2"];
                            config["config2"]["WIFI_PASS1"] = config["config2"]["WIFI_PASS2"];
                            config["config2"]["WIFI_MODE"] = STA_ON;
                            configWrite();
                            
                            LOG_INFOLN("Auto-connect successful, saved as primary network");
                        }
                    }
                }
                // Подключение отсутствует или потеряно
                else {
                    if (isWiFiConnected) {
                        // Было подключение - потеряли
                        isWiFiConnected = false;
                        LOG_ERRORLN("WiFi disconnected! Status code: %d", status);
                        EventRGB1->setColor1(COLOR_WIFI_WAIT);
                        msSTA = millis();  // Сбрасываем таймер для нового отсчета таймаута
                    }
                    
                    // Проверка таймаута подключения (10 секунд)
                    if (TIME_DIFF_MS(ms, msSTA) > 10000) {
                        LOG_ERRORLN("WiFi connection timeout (10 seconds)");
                        EventRGB1->setColor1(COLOR_WIFI_OFF);
                        WiFi.enableSTA(false);  // Отключаем STA для перезапуска
                        wifiError++;
                        LOG_DEBUGLN("WiFi error counter after timeout: %d", wifiError);
                    }
                }
            }
            
            // ---- ОТКЛЮЧЕНИЕ РЕЖИМА STA ----
            if (!isSTA && (curWiFi == WIFI_STA || curWiFi == WIFI_AP_STA)) {
                LOG_INFOLN("Stopping STA mode");
                isWiFiConnected = false;
                WiFi.enableSTA(false);
                EventRGB1->setColor1(COLOR_BLACK);
                LOG_DEBUGLN("STA mode disabled");
            }
        }
        
        // Задержка для снижения нагрузки на процессор
        vTaskDelay(500);
    }
}

// ============================================================
// ЗАДАЧА ОТПРАВКИ HTTP ЗАПРОСОВ
// ============================================================

/**
 * Задача отправки HTTP запросов на внешние серверы
 * 
 * Отвечает за:
 * - Периодическую отправку данных в CRM Москва
 * - Отправку данных на HTTP шлюзы
 * - Отправку телеметрии в ThingsBoard
 * - Отправку атрибутов устройства в ThingsBoard
 * - Авторизацию в ThingsBoard при необходимости
 * - Соблюдение интервалов отправки и повторных попыток при ошибках
 * 
 * Не отвечает за:
 * - Управление WiFi подключением (этим занимается taskWiFiManager)
 * - Обработку входящих HTTP запросов (веб-интерфейс)
 */
void taskHttpSender(void *pvParameters) {
    LOG_INFOLN("HTTP Sender task started");
    // Таймеры для управления периодичностью отправки
    uint32_t msSendHttp    = millis();    // Таймер отправки на HTTP шлюзы
    uint32_t msSendTB      = msSendHttp;  // Таймер отправки в ThingsBoard
    uint32_t msSendCrm     = msSendHttp;  // Таймер отправки в CRM Москва
    bool isSendAttributeTB = false;       // Флаг необходимости отправки атрибутов TB
    bool isFirstConnect    = true;        // Флаг первого подключения для сброса таймеров
    
    // Главный цикл задачи
    while (true) {
        // Блокировка при калибровке или перезагрузке датчика
        if (isSensorBlock || calibrMode == CM_WAIT_REBOOT) {
            vTaskDelay(1000);
            continue;
        }
        
        uint32_t ms = millis();
        
        // Если нет подключения к WiFi - ждем и сбрасываем флаги
        if (!isWiFiConnected) {
            isFirstConnect = true;  // При следующем подключении сбросим таймеры
            vTaskDelay(1000);
            continue;
        }
        
        // При первом подключении после запуска или переподключения
        if (isFirstConnect) {
            isFirstConnect = false;
            // Сбрасываем все таймеры для немедленной отправки
            msSendHttp = ms;
            msSendCrm  = ms;
            msSendTB   = ms;  // ThingsBoard отправляем первым
            LOG_INFOLN("HTTP sender timer reset");
        }
        
        // ========================================
        // ОТПРАВКА В CRM МОСКВА
        // ========================================
        if (config["config2"]["CRM_ENABLE"].as<bool>() && 
            TIME_EXPIRED_MS(ms, msSendCrm) ) {
            
            LOG_DEBUGLN("Sending data to CRM Moscow...");
            
            if (sendCrmMoscowParam()) {
                // Успешная отправка - устанавливаем интервал до следующей
                msSendCrm = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                LOG_DEBUGLN("CRM next send in %d seconds", 
                           config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
            }
            else {
                // Ошибка отправки - сокращаем интервал для повторной попытки
                msSendCrm = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                LOG_ERRORLN("CRM send failed, retry in %d seconds", 
                           config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
            }
        }
        
        // ========================================
        // ОТПРАВКА НА HTTP ШЛЮЗЫ
        // ========================================
        if (config["config2"]["HTTP_ENABLE"].as<bool>() && 
            TIME_EXPIRED_MS(ms, msSendHttp) ) {
            
            LOG_DEBUGLN("Sending data to HTTP Gateways...");
            
            if (sendHttpParam()) {
                // Успешная отправка на все шлюзы
                msSendHttp = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                LOG_DEBUGLN("HTTP Gateways next send in %d seconds", 
                           config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
            }
            else {
                // Хотя бы один шлюз не ответил
                msSendHttp = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                LOG_ERRORLN("HTTP Gateway send failed, retry in %d seconds", 
                           config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
            }
        }
        
        // ========================================
        // ОТПРАВКА В THINGSBOARD
        // ========================================
        if (config["config2"]["TB_ENABLE"].as<bool>() && 
            TIME_EXPIRED_MS(ms, msSendTB)) {
            
            LOG_DEBUGLN("Sending telemetry to ThingsBoard...");
            
            if (sendParamTB()) {
                // Успешная отправка телеметрии
                msSendTB = ms + config["config2"]["TM_HTTP_SEND"].as<uint32_t>() * 1000;
                LOG_DEBUGLN("ThingsBoard next send in %d seconds", 
                           config["config2"]["TM_HTTP_SEND"].as<uint32_t>());
                
                // После успешной телеметрии отправляем атрибуты (если ещё не отправлены)
                if (!isSendAttributeTB) {
                    LOG_DEBUGLN("Sending device attributes to ThingsBoard...");
                    isSendAttributeTB = sendAttributeTB();
                    if (isSendAttributeTB) {
                        LOG_INFOLN("Device attributes sent successfully");
                    }
                }
            }
            else {
                // Ошибка отправки телеметрии
                msSendTB = ms + config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>() * 1000;
                LOG_ERRORLN("ThingsBoard send failed, retry in %d seconds", 
                           config["config2"]["TM_HTTP_RETRY_ERROR"].as<uint32_t>());
            }
        }
        
        // Небольшая задержка для снижения нагрузки на процессор
        vTaskDelay(100);
    }
}

// ============================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ПОЛУЧЕНИЕ СТАТУСА ДАТЧИКА
// ============================================================

/**
 * Возвращает текущий статус датчика парковки
 * 
 * @return 1 - место занято
 *         0 - место свободно
 *        -1 - ошибка/неопределенное состояние
 */
int getStatus() {
    switch (SensorOn) {
        case SS_BUSY:
        case SS_NAN_BUSY:
            return 1;  // Место занято
        case SS_FREE:
        case SS_NAN_FREE:
            return 0;  // Место свободно
        default:
            return -1; // Неопределенное состояние
    }
}

// ============================================================
// ФУНКЦИЯ ОТПРАВКИ В CRM МОСКВА
// ============================================================

/**
 * Отправляет данные о состоянии парковочного места в CRM систему Москвы
 * 
 * Формирует URL с параметрами:
 * - Идентификатор договора и бокса
 * - Дистанция до объекта
 * - Время работы
 * - Статус датчика
 * - Контрольная сумма
 * 
 * @return true - данные успешно отправлены (HTTP 200)
 *         false - ошибка отправки
 */
bool sendCrmMoscowParam() {
    bool ret = false;
    char s[64];
    
    // Подготовка параметров запроса
    uint32_t tm = millis() / 1000;
    int dist = (int)Distance;
    int stat = getStatus();
    
    // Формирование строки для расчета контрольной суммы
    sprintf(s, "%s;%ld;%d;%d;%d", strID, tm, dist, tm, 0);
    uint16_t crc = KeyGen(s);
    
    // Формирование URL запроса
    char url[512];
    snprintf(url, sizeof(url),
        "/%s?id=%s_%s&temp=0&hum=0&dist=%d&tm=%lu&btn=%d&uptime=%lu&key=%d",
        CRM_MOSCOW_PATH,
        config["config2"]["N_DOGIVOR"].as<const char *>(),
        config["config2"]["N_BOX"].as<const char *>(),
        dist,
        millis() / 1000,
        stat,
        millis() / 1000,
        crc);
    
    LOG_DEBUGLN("CRM request URL: %s%s", config["config2"]["CRM_HOST"].as<const char *>(), url);
    
    // Выполнение HTTP GET запроса
    HttpResponse response = SimpleHttpClient::GET(
        config["config2"]["CRM_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url);
    
    // Проверка результата
    if (response.statusCode == 200) {
        LOG_INFOLN("CRM data sent successfully");
        ret = true;
    }
    else {
        LOG_ERRORLN("CRM send error, HTTP status: %d", response.statusCode);
    }
    
    return ret;
}

// ============================================================
// ФУНКЦИЯ ОТПРАВКИ НА HTTP ШЛЮЗЫ
// ============================================================

/**
 * Отправляет данные на все настроенные HTTP шлюзы
 * 
 * Серверы перечислены в конфигурации HTTP_SERVERS через разделители:
 * - пробел
 * - запятая
 * - перевод строки
 * 
 * @return true - все серверы ответили успешно
 *         false - хотя бы один сервер не ответил или вернул ошибку
 */
bool sendHttpParam() {
    bool allSuccess = true;
    int start = -1;
    
    // Получение списка серверов из конфигурации
    String hosts = config["config2"]["HTTP_SERVERS"].as<String>();
    
    // Парсинг списка серверов
    for (int i = 0; i <= hosts.length(); i++) {
        // Проверка на разделитель
        bool separator =
            (i == hosts.length()) ||      // Конец строки
            (hosts[i] == ' ') ||           // Пробел
            (hosts[i] == ',') ||           // Запятая
            (hosts[i] == '\n') ||          // Перевод строки
            (hosts[i] == '\r');            // Возврат каретки
        
        if (start < 0) {
            // Начало нового сервера
            if (!separator) start = i;
        }
        else {
            // Конец сервера - отправляем данные
            if (separator) {
                String host = hosts.substring(start, i);
                
                // Отправка на конкретный сервер
                if (!sendHttpParamOne(host)) {
                    allSuccess = false;
                    LOG_ERRORLN("Failed to send data to gateway: %s", host.c_str());
                }
                
                start = -1;  // Сброс для следующего сервера
            }
        }
    }
    
    return allSuccess;
}

// ============================================================
// ФУНКЦИЯ ОТПРАВКИ НА ОДИН HTTP ШЛЮЗ
// ============================================================

/**
 * Отправляет данные на один конкретный HTTP шлюз
 * 
 * @param host Адрес сервера (IP или доменное имя)
 * @return true - данные успешно отправлены
 *         false - ошибка отправки
 */
bool sendHttpParamOne(String &host) {
    char s[64];
    uint32_t tm = millis() / 1000;
    int dist = (int)Distance;
    int stat = getStatus();
    
    // Формирование строки для контрольной суммы
    sprintf(s, "%s;%ld;%d", strID, tm, dist);
    uint16_t crc = KeyGen(s);
    
    // Формирование URL с параметрами
    String path = HTTP_PATH;
    path += "?id=";
    path += strID;                                    // ID устройства
    path += "&dist=";
    path += dist;                                     // Дистанция
    path += "&sn=";
    path += serNo;                                     // Серийный номер
    path += "&dn=";
    path += config["config2"]["N_DOGIVOR"].as<String>();  // Номер договора
    path += "&bn=";
    path += config["config2"]["N_BOX"].as<String>();      // Номер бокса
    path += "&tm=";
    path += String(millis() / 1000);                   // Время отправки
    path += "&stat=";
    path += stat;                                      // Статус датчика
    path += "&uptime=";
    path += String(millis() / 1000);                   // Время работы
    path += "&rssi=";
    path += WiFi.RSSI();                               // Уровень сигнала WiFi
    path += "&key=";
    path += (int)crc;                                  // Контрольная сумма
    
    // Определение порта (по умолчанию 80)
    int port = config["config2"]["HTTP_PORT"].as<int>();
    if (port == 0) port = 80;
    
    // Выполнение HTTP GET запроса
    HttpResponse response = SimpleHttpClient::GET(host.c_str(), port, path);
    
    // Проверка результата
    if (response.statusCode == 200) {
        LOG_INFOLN("HTTP data sent successfully to: %s", host.c_str());
        return true;
    }
    else {
        LOG_ERRORLN("HTTP send error to %s, status: %d", host.c_str(), response.statusCode);
        return false;
    }
}

// ============================================================
// ФУНКЦИЯ ГЕНЕРАЦИИ КОНТРОЛЬНОЙ СУММЫ
// ============================================================

/**
 * Генерирует контрольную сумму для проверки целостности запроса
 * 
 * Алгоритм:
 * 1. Суммирует все символы строки
 * 2. Инвертирует биты
 * 3. Оставляет только 12 младших бит (0xFFF)
 * 
 * @param str Входная строка для расчета
 * @return 16-битная контрольная сумма
 */
uint16_t KeyGen(char *str) {
    uint16_t crc = 0;
    
    // Суммирование всех символов
    for (int i = 0; i < strlen(str); i++) {
        crc += (int)str[i];
    }
    
    // Инверсия и маскирование до 12 бит
    crc = (~crc) & 0xfff;
    return crc;
}

// ============================================================
// ФУНКЦИЯ ОТПРАВКИ ТЕЛЕМЕТРИИ В THINGSBOARD
// ============================================================

/**
 * Отправляет телеметрические данные в ThingsBoard
 * 
 * Отправляемые данные:
 * - Distance: текущая дистанция
 * - State: статус датчика (занято/свободно)
 * - Uptime: время работы устройства
 * - SN: серийный номер (опционально)
 * 
 * При необходимости выполняет автоматическую авторизацию устройства
 * 
 * @return true - данные успешно отправлены
 *         false - ошибка отправки или авторизации
 */
bool sendParamTB() {
    bool ret = false;
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    
    // Проверка необходимости авторизации
    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() || 
            config["config2"]["TB_TOKEN"] == "") {
            
            // Токен отсутствует - выполняем авторизацию
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                LOG_ERRORLN("ThingsBoard authentication failed");
                return false;
            }
        }
    }
    
    // Формирование URL для отправки телеметрии
    String url = "/api/v1/";
    if (is_over_gate) {
        url += strID;  // Режим шлюза - используем ID устройства
    }
    else {
        url += config["config2"]["TB_TOKEN"].as<String>();  // Обычный режим - токен
    }
    url += "/telemetry";
    
    // Формирование JSON с данными телеметрии
    int state = getStatus();
    jsonData.clear();
    jsonData["Distance"] = (int)Distance;
    jsonData["State"] = state;
    jsonData["Uptime"] = esp_timer_get_time() / 1000000;
    
    // Добавление серийного номера если он задан
    if (serNo[0] != '\0') {
        jsonData["SN"] = serNo;
    }
    
    // Сериализация JSON в строку
    String data;
    serializeJson(jsonData, data);
    
    // Заголовки запроса
    String headers = "Content-Type: application/json\r\n";
    
    // Выполнение HTTP POST запроса
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    
    // Проверка результата (2xx коды считаются успешными)
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("ThingsBoard telemetry sent successfully");
        
        // Парсинг ответа для получения токена (при авто-регистрации)
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response.body);
        
        if (!error && respDoc["status"].as<String>() == "SUCCESS") {
            // Сохраняем полученный токен
            config["config2"]["TB_TOKEN"] = respDoc["credentialsValue"].as<String>();
            configWrite();
            LOG_INFOLN("ThingsBoard token updated");
        }
        
        ret = true;
    }
    else {
        LOG_ERRORLN("ThingsBoard telemetry send failed, HTTP status: %d", response.statusCode);
    }
    
    return ret;
}

// ============================================================
// ФУНКЦИЯ АВТОРИЗАЦИИ В THINGSBOARD
// ============================================================

/**
 * Выполняет авторизацию устройства в ThingsBoard
 * 
 * Использует механизм Provisioning для автоматической регистрации
 * устройства и получения токена доступа
 * 
 * @param key Ключ устройства для провижинга
 * @param secret Секретный ключ для провижинга
 * @return true - авторизация успешна, токен сохранен
 *         false - ошибка авторизации
 */
bool authTB(const char *key, const char *secret) {
    bool ret = false;
    
    // URL для запроса авторизации
    String url = "/api/v1/provision";
    
    // Формирование JSON с данными устройства
    jsonData.clear();
    jsonData["deviceName"] = strID;
    jsonData["provisionDeviceKey"] = key;
    jsonData["provisionDeviceSecret"] = secret;
    
    // Сериализация в строку
    String data;
    serializeJson(jsonData, data);
    String headers = "Content-Type: application/json\r\n";
    
    LOG_DEBUGLN("ThingsBoard auth request to: %s:%d%s", 
               config["config2"]["TB_HOST"].as<const char *>(), 
               config["config2"]["TB_PORT"].as<int>(),
               url.c_str());
    
    // Выполнение POST запроса
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    
    LOG_DEBUGLN("ThingsBoard auth response status: %d", response.statusCode);

    // Проверка успешности (2xx коды)
    if (response.statusCode >= 200 && response.statusCode < 300) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response.body);
        
        if (!error && respDoc["status"].as<String>() == "SUCCESS") {
            // Получение и сохранение токена
            String token = respDoc["credentialsValue"].as<String>();
            LOG_DEBUGLN("ThingsBoard token obtained: %s", token.c_str());

            config["config2"]["TB_TOKEN"] = token;
            configWrite();
            
            // Отправка уведомления через веб-интерфейс
            HTTP_sendResponse(WebResponse::combine({
               WebResponse::config("config2", "TB_TOKEN", token),
               WebResponse::msg("Получен токен ThingsBoard", "info", 3000)
            }));

            ret = true;
        }
        else {
            LOG_ERRORLN("ThingsBoard auth response parse error or not SUCCESS");
        }
    }
    else {
        LOG_ERRORLN("ThingsBoard auth failed, HTTP status: %d", response.statusCode);
    }
    
    return ret;
}

// ============================================================
// ФУНКЦИЯ ОТПРАВКИ АТРИБУТОВ В THINGSBOARD
// ============================================================

/**
 * Отправляет атрибуты устройства в ThingsBoard
 * 
 * Атрибуты отправляются один раз после успешной авторизации
 * и содержат:
 * - SerialNo: серийный номер устройства
 * - DogovorNo: номер договора
 * - BoxNo: номер бокса
 * 
 * @return true - атрибуты успешно отправлены
 *         false - ошибка отправки
 */
bool sendAttributeTB() {
    bool ret = false;
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    
    // Проверка наличия токена
    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() || 
            config["config2"]["TB_TOKEN"] == "") {
            
            // Токен отсутствует - авторизуемся
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                return false;
            }
        }
    }
    
    // Формирование URL для отправки атрибутов
    String url = "/api/v1/";
    if (is_over_gate) {
        url += strID;
    }
    else {
        url += config["config2"]["TB_TOKEN"].as<String>();
    }
    url += "/attributes?clientKeys";
    
    // Формирование JSON с атрибутами
    jsonData.clear();
    jsonData["SerialNo"] = serNo;
    jsonData["DogovorNo"] = config["config2"]["N_DOGIVOR"].as<String>();
    jsonData["BoxNo"] = config["config2"]["N_BOX"].as<String>();
    
    // Сериализация в строку
    String data;
    serializeJson(jsonData, data);
    
    String headers = "Content-Type: application/json\r\n";
    
    LOG_DEBUGLN("Sending attributes to ThingsBoard...");
    
    // Выполнение POST запроса
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    
    // Проверка результата
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("ThingsBoard attributes sent successfully");
        ret = true;
    }
    else {
        LOG_ERRORLN("ThingsBoard attributes send failed, HTTP status: %d", response.statusCode);
    }
    
    return ret;
}