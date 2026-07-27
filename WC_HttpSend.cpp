// ============================================
// Файл: WC_HttpSend.cpp
// ============================================
#define MODULE_NAME "HTTP_SEND"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "src/Slib/SDEBUG.h"
#include "WC_HttpSend.h"
#include "WC_Config.h"

//************************************************************************************************************************
// Конструктор / Деструктор
//************************************************************************************************************************

/**
 * Конструктор класса MyHttpSend
 */
MyHttpSend::MyHttpSend() {
    m_tarPath = String(WEB_TAR_PATH);
    m_httpdPath = String(WEB_HTTPD_PATH);
    m_versionFile = String(WEB_VERSION_FILE);
    parseExcludeList(String(WEB_EXCLUDE_FILES));
}

/**
 * Деструктор класса MyHttpSend
 */
MyHttpSend::~MyHttpSend() {
    stop();
}

//************************************************************************************************************************
// Инициализация и остановка
//************************************************************************************************************************

/**
 * Инициализация модуля HTTP-отправки
 */
bool MyHttpSend::begin() {
    LOG_INFOLN("HttpSend: Initializing module");

    if (!LittleFS.begin()) {
        LOG_ERRORLN("HttpSend: LittleFS mount failed");
        return false;
    }

    if (!LittleFS.exists(m_httpdPath)) {
        LOG_ERRORLN("HttpSend: httpd directory not found");
        return false;
    }

    if (!m_httpClient.begin()) {
        LOG_ERRORLN("HttpSend: HTTP client initialization failed");
        return false;
    }

    LOG_INFOLN("HttpSend: Module initialized successfully");
    return true;
}

/**
 * Остановка HTTP-клиента
 */
void MyHttpSend::stop() {
    m_httpClient.stop();
}

//************************************************************************************************************************
// Вспомогательные методы
//************************************************************************************************************************

/**
 * Парсинг строки исключенных файлов в вектор
 */
void MyHttpSend::parseExcludeList(const String& excludeStr) {
    m_excludeFiles.clear();

    int start = 0;
    int end = 0;

    while ((end = excludeStr.indexOf(',', start)) != -1) {
        String file = excludeStr.substring(start, end);
        file.trim();
        if (file.length() > 0) {
            m_excludeFiles.push_back(file);
        }
        start = end + 1;
    }

    String lastFile = excludeStr.substring(start);
    lastFile.trim();
    if (lastFile.length() > 0) {
        m_excludeFiles.push_back(lastFile);
    }

    LOG_DEBUGLN("HttpSend: Excluded files count: %d", m_excludeFiles.size());
}

/**
 * Вычисление контрольной суммы TAR-заголовка
 */
unsigned int MyHttpSend::calculateTarChecksum(unsigned char* header) {
    unsigned int sum = 0;
    for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (i >= 148 && i < 156) {
            sum += ' ';
        } else {
            sum += header[i];
        }
    }
    return sum;
}

//************************************************************************************************************************
// Свободные функции
//************************************************************************************************************************

/**
 * Получение статуса датчика
 */
int getStatus() {
    switch (SensorOn) {
        case SS_BUSY:
        case SS_NAN_BUSY:
            return 1;
        case SS_FREE:
        case SS_NAN_FREE:
            return 0;
        default:
            return -1;
    }
}

/**
 * Генерация контрольного ключа для проверки целостности данных
 */
uint16_t KeyGen(char *str) {
    uint16_t crc = 0;
    for (int i = 0; i < strlen(str); i++) {
        crc += (int)str[i];
    }
    crc = (~crc) & 0xfff;
    return crc;
}

//************************************************************************************************************************
// Отправка данных в CRM Москва
//************************************************************************************************************************

/**
 * Отправка параметров в CRM Москва
 */
bool MyHttpSend::sendCrmMoscowParam() {
    char s[64];
    uint32_t tm = millis() / 1000;
    int dist = (int)Distance;
    int stat = getStatus();

    sprintf(s, "%s;%ld;%d;%d;%d", strID, tm, dist, tm, 0);
    uint16_t crc = KeyGen(s);

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

    LOG_DEBUGLN("HttpSend: CRM request URL: %s%s",
                config["config2"]["CRM_HOST"].as<const char *>(), url);

    bool ret = m_httpClient.GET(
        config["config2"]["CRM_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url);

    if (ret && m_httpClient.m_response.statusCode == 200) {
        LOG_INFOLN("HttpSend: CRM data sent successfully");
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: CRM send error, HTTP status: %d", m_httpClient.m_response.statusCode);
    }

    return ret;
}

//************************************************************************************************************************
// Отправка данных на HTTP-серверы
//************************************************************************************************************************

/**
 * Отправка параметров на все HTTP-серверы из списка
 */
bool MyHttpSend::sendHttpParam() {
    bool allSuccess = true;
    int start = -1;

    String hosts = config["config2"]["HTTP_SERVERS"].as<String>();
    for (int i = 0; i <= hosts.length(); i++) {
        bool separator = (i == hosts.length()) ||
                         (hosts[i] == ' ') ||
                         (hosts[i] == ',') ||
                         (hosts[i] == '\n') ||
                         (hosts[i] == '\r');

        if (start < 0) {
            if (!separator) start = i;
        } else {
            if (separator) {
                String host = hosts.substring(start, i);
                if (!sendHttpParamOne(host)) {
                    allSuccess = false;
                    LOG_ERRORLN("HttpSend: Failed to send data to gateway: %s", host.c_str());
                }
                start = -1;
            }
        }
    }

    return allSuccess;
}

/**
 * Отправка параметров на один HTTP-сервер
 */
bool MyHttpSend::sendHttpParamOne(String &host) {
    char s[64];
    uint32_t tm = millis() / 1000;
    int dist = (int)Distance;
    int stat = getStatus();

    sprintf(s, "%s;%ld;%d", strID, tm, dist);
    uint16_t crc = KeyGen(s);

    String path = HTTP_PATH;
    path += "?id=";
    path += strID;
    path += "&dist=";
    path += dist;
    path += "&sn=";
    path += serNo;
    path += "&dn=";
    path += config["config2"]["N_DOGIVOR"].as<String>();
    path += "&bn=";
    path += config["config2"]["N_BOX"].as<String>();
    path += "&tm=";
    path += String(millis() / 1000);
    path += "&stat=";
    path += stat;
    path += "&uptime=";
    path += String(millis() / 1000);
    path += "&rssi=";
    path += WiFi.RSSI();
    path += "&key=";
    path += (int)crc;

    int port = config["config2"]["HTTP_PORT"].as<int>();
    if (port == 0) port = 80;

    bool ret = m_httpClient.GET(host.c_str(), port, path);

    if (ret && m_httpClient.m_response.statusCode == 200) {
        LOG_INFOLN("HttpSend: HTTP data sent successfully to: %s", host.c_str());
        return true;
    } else {
        LOG_ERRORLN("HttpSend: HTTP send error to %s, status: %d", host.c_str(), m_httpClient.m_response.statusCode);
        return false;
    }
}

//************************************************************************************************************************
// Работа с конфигурацией
//************************************************************************************************************************

/**
 * Проверка и инициализация версии конфигурации
 */
void MyHttpSend::checkConfigVersion() {
    if (config["main"]["version"].isNull()) {
        config["main"]["version"] = CONFIG_V;
        configWrite();
        LOG_INFOLN("HttpSend: Config version initialized to: %s", CONFIG_V);
    } else {
        LOG_DEBUGLN("HttpSend: Config version exists: %s",
                    config["main"]["version"].as<String>().c_str());
    }
}

/**
 * Обновление конфигурации из ThingsBoard
 */
bool MyHttpSend::updateConfigTB() {
    String url = "";
    url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/attributes?clientKeys=Config";

    String tbHost = config["config2"]["TB_HOST"].as<String>();
    int tbPort = config["config2"]["TB_PORT"].as<int>();

    LOG_INFOLN("HttpSend: Get New Config");

    bool ret = m_httpClient.GET(tbHost.c_str(), tbPort, url);

    if (ret && m_httpClient.m_response.statusCode == 200) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);

        if (error) {
            LOG_DEBUG("HttpSend: Error Json Config");
            return false;
        }

        config.clear();
        config.set(respDoc["client"]["Config"]);

        INFO_JSON_DOC_COMPACT("NEW_CONFIG", config);
        configWrite();

        LOG_INFOLN("HttpSend: Config Update");
        return true;
    }
    else {
        LOG_ERRORLN("HttpSend: Error HTTPD attributes");
        return false;
    }
}

//************************************************************************************************************************
// Отправка данных в ThingsBoard
//************************************************************************************************************************

/**
 * Отправка телеметрии в ThingsBoard
 */
bool MyHttpSend::sendParamTB() {
    String url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/telemetry";

    int state = getStatus();

    jsonData.clear();
    jsonData["Distance"] = (int)Distance;
    jsonData["State"] = state;
    jsonData["Uptime"] = esp_timer_get_time() / 1000000;

    if (serNo[0] != '\0') {
        jsonData["SN"] = serNo;
    }

    String data;
    serializeJson(jsonData, data);

    String headers = "Content-Type: application/json\r\n";
    LOG_INFOLN("Send Telemetry");

    bool ret = m_httpClient.POST_JSON(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        data,
        headers);

    if (ret && m_httpClient.m_response.statusCode >= 200 && m_httpClient.m_response.statusCode < 300) {
        LOG_INFOLN("HttpSend: ThingsBoard telemetry sent successfully");

        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);
        if (!error && respDoc["status"].as<String>() == "SUCCESS") {
            config["config2"]["TB_TOKEN"] = respDoc["credentialsValue"].as<String>();
            configWrite();
            LOG_INFOLN("HttpSend: ThingsBoard token updated");
        }

        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: ThingsBoard telemetry send failed, HTTP status: %d", m_httpClient.m_response.statusCode);
    }

    return ret;
}

/**
 * Аутентификация устройства в ThingsBoard
 */
bool MyHttpSend::authTB(const char *key, const char *secret) {
    if (!config["config2"]["TB_TOKEN"].isNull() && config["config2"]["TB_TOKEN"] != "") {
        return true;
    }

    String url = "/api/v1/provision";

    jsonData.clear();
    jsonData["deviceName"] = strID;
    jsonData["provisionDeviceKey"] = key;
    jsonData["provisionDeviceSecret"] = secret;

    String data;
    serializeJson(jsonData, data);

    String headers = "Content-Type: application/json\r\n";

    LOG_DEBUGLN("HttpSend: ThingsBoard auth request to: %s:%d%s",
                config["config2"]["TB_HOST"].as<const char *>(),
                config["config2"]["TB_PORT"].as<int>(),
                url.c_str());

    m_httpClient.begin();

    bool ret = m_httpClient.POST_JSON(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        data,
        headers);

    LOG_INFOLN("HttpSend: ThingsBoard auth response status: %d %s", m_httpClient.m_response.statusCode, m_httpClient.m_response.body.c_str());

    if (m_httpClient.m_response.statusCode >= 200 && m_httpClient.m_response.statusCode < 300) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);
        if (!error && respDoc["status"].as<String>() == "SUCCESS") {
            String token = respDoc["credentialsValue"].as<String>();
            LOG_DEBUGLN("HttpSend: ThingsBoard token obtained: %s", token.c_str());

            config["config2"]["TB_TOKEN"] = token;
            configWrite();

            HTTP_sendResponse(WebResponse::combine({
                WebResponse::config("config2", "TB_TOKEN", token),
                WebResponse::msg("Получен токен ThingsBoard", "info", 3000)
            }));

            ret = true;
        } else {
            LOG_ERRORLN("HttpSend: ThingsBoard auth response parse error or not SUCCESS");
        }
    } else {
        LOG_ERRORLN("HttpSend: ThingsBoard auth failed, HTTP status: %d", m_httpClient.m_response.statusCode);
    }

    return ret;
}

/**
 * Отправка атрибутов устройства в ThingsBoard
 */
bool MyHttpSend::sendAttributeTB() {
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();

    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() ||
            config["config2"]["TB_TOKEN"] == "") {
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                return false;
            }
        }
    }

    String url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/attributes?clientKeys";
    LOG_INFOLN("Send Attribute");

    jsonData.clear();
    jsonData["SerialNo"] = serNo;
    jsonData["DogovorNo"] = config["config2"]["N_DOGIVOR"].as<String>();
    jsonData["BoxNo"] = config["config2"]["N_BOX"].as<String>();
    jsonData["Config"] = config;
    jsonData["ConfigUUID"] = configUUID;

    String data;
    serializeJson(jsonData, data);

    String headers = "Content-Type: application/json\r\n";

    LOG_DEBUGLN("HttpSend: Sending attributes to ThingsBoard...");

    m_httpClient.begin();

    bool ret = m_httpClient.POST_JSON(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        data,
        headers);

    if (ret && m_httpClient.m_response.statusCode >= 200 && m_httpClient.m_response.statusCode < 300) {
        LOG_INFOLN("HttpSend: ThingsBoard attributes sent successfully");
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: ThingsBoard attributes send failed, HTTP status: %d", m_httpClient.m_response.statusCode);
    }

    return ret;
}

//************************************************************************************************************************
// Проверка и установка обновлений
//************************************************************************************************************************

/**
 * Проверка наличия обновлений с ThingsBoard
 */
bool MyHttpSend::checkUpdateTB(bool _flagSendAttributeTB) {
    uint32_t currentTime = millis();

    String url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/attributes?clientKeys=ConfigUUID&sharedKeys=fw_version,sw_version";

    String tbHost = config["config2"]["TB_HOST"].as<String>();
    int tbPort = config["config2"]["TB_PORT"].as<int>();

//    LOG_INFOLN("HttpSend: Check Update: %s:%d%s", tbHost.c_str(), tbPort, url.c_str());

    bool ret = m_httpClient.GET(tbHost.c_str(), tbPort, url);
    ret = true;

    if (ret && m_httpClient.m_response.statusCode == 200) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);

        String fwVersion = respDoc["shared"]["fw_version"].as<String>();
        String swVersion = respDoc["shared"]["sw_version"].as<String>();
        String serverUUID = respDoc["client"]["ConfigUUID"].as<String>();
        String httpdVersion = httpd_version["version"].as<String>();

        if (fwVersion != FW_VERSION) {
            LOG_INFOLN("HttpSend: *** NEW FIRMWARE VERSION AVAILABLE ***");
            LOG_INFOLN("HttpSend: new firmware %s", fwVersion.c_str());

            HTTP_sendResponse(WebResponse::combine({
                WebResponse::msg("Доступна новая версия прошивки: " + fwVersion +". Загружается обновление ...", "info", 5000)
            }));

            updateFirmwareFromTB();
            return true;
        }
        else if (httpdVersion != swVersion) {
            LOG_INFOLN("HttpSend: *** NEW HTTPD VERSION AVAILABLE ***");
            LOG_INFOLN("HttpSend: new HTTPD %s %s", swVersion.c_str());

            HTTP_sendResponse(WebResponse::combine({
                WebResponse::msg("Доступна новая версия HTTPD: " + swVersion + ". Загружается обновление ...", "info", 5000)
            }));

            updateHttpdTB();
            return true;
        }
        else if (_flagSendAttributeTB && serverUUID != configUUID) {
            LOG_INFOLN("HttpSend: *** CONFIG REMOTE CHANGE ***");

            HTTP_sendResponse(WebResponse::combine({
                WebResponse::msg("Удаленно изменилась конфигурация. Страница перегрузится через 5 сек ", "info", 5000),
                WebResponse::reload(5000)
            }));

            if (updateConfigTB()) configUUID = serverUUID;
            return true;
        }
        else {
            return false;
        }
    }

    LOG_ERRORLN("HttpSend: Error Check %d, %s", m_httpClient.m_response.statusCode, m_httpClient.m_response.body.c_str());
    return false;
}

/**
 * Обновление прошивки из ThingsBoard
 */
bool MyHttpSend::updateFirmwareFromTB() {
    String url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/attributes?sharedKeys=fw_title,fw_version,fw_checksum";

    String tbHost = config["config2"]["TB_HOST"].as<String>();
    int tbPort = config["config2"]["TB_PORT"].as<int>();

    LOG_INFOLN("HttpSend: Check Firmware: %s:%d%s", tbHost.c_str(), tbPort, url.c_str());

    bool ret = m_httpClient.GET(tbHost.c_str(), tbPort, url);

    String fw_version = "";
    String fw_title = "";
    String fw_checksum = "";

    if (ret && m_httpClient.m_response.statusCode == 200) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);
        fw_version = respDoc["shared"]["fw_version"].as<String>();
        fw_title = respDoc["shared"]["fw_title"].as<String>();
        fw_checksum = respDoc["shared"]["fw_checksum"].as<String>();
    }
    else {
        LOG_ERRORLN("HttpSend: Error firmware attributes");
        return false;
    }

    LOG_INFOLN("HttpSend: Starting firmware update from ThingsBoard...");
    LOG_INFOLN("HttpSend: Firmware version: %s", fw_title.c_str(), fw_version.c_str());
    LOG_INFOLN("HttpSend: Expected SHA256 checksum: %s", fw_checksum.c_str());

    url = "/api/v1/";
    url += config["config2"]["TB_TOKEN"].as<String>();
    url += "/firmware?title=" + fw_title + "&version=" + fw_version;

    LOG_INFOLN("HttpSend: Downloading firmware from: %s:%d%s", tbHost.c_str(), tbPort, url.c_str());

    ret = m_httpClient.GET_STREAM(tbHost.c_str(), tbPort, url, "", 10000);

    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        LOG_ERRORLN("HttpSend: Failed to begin firmware update");
        Update.printError(Serial);
        mbedtls_sha256_free(&shaCtx);
        m_httpClient.stop();
        return false;
    }

    systemMP3((char*)"89", 89, PRIORITY_MP3_MEDIUM);

    size_t totalBytes = 0;
    bool updateSuccess = false;
    uint8_t buffer[512];
    int bytesRead;

    while ((bytesRead = m_httpClient.m_client.readBytes(buffer, sizeof(buffer))) > 0) {
        mbedtls_sha256_update(&shaCtx, buffer, bytesRead);

        if (Update.write(buffer, bytesRead) != bytesRead) {
            LOG_ERRORLN("HttpSend: Update write error");
            Update.printError(Serial);
            mbedtls_sha256_free(&shaCtx);
            Update.abort();
            m_httpClient.stop();
            return false;
        }

        totalBytes += bytesRead;
        LOG_DEBUGLN("HttpSend: Written %d bytes, total: %d", bytesRead, totalBytes);
    }

    m_httpClient.stop();

    uint8_t hash[32];
    mbedtls_sha256_finish(&shaCtx, hash);
    mbedtls_sha256_free(&shaCtx);

    char hexStr[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hexStr + i * 2, "%02x", hash[i]);
    }
    hexStr[64] = '\0';

    String calculatedChecksum = String(hexStr);

    LOG_INFOLN("HttpSend: Downloaded %d bytes of firmware", totalBytes);
    LOG_INFOLN("HttpSend: Calculated SHA256: %s", calculatedChecksum.c_str());

    if (!fw_checksum.isEmpty() && fw_checksum != calculatedChecksum) {
        LOG_ERRORLN("HttpSend: SHA256 checksum mismatch!");
        LOG_ERRORLN("HttpSend: Expected: %s", fw_checksum.c_str());
        LOG_ERRORLN("HttpSend: Got: %s", calculatedChecksum.c_str());
        Update.abort();
        return false;
    }

    LOG_INFOLN("HttpSend: SHA256 checksum verification successful");

    if (Update.end(true)) {
        LOG_INFOLN("HttpSend: Firmware update successful");
        updateSuccess = true;
    } else {
        LOG_ERRORLN("HttpSend: Firmware update failed");
        Update.printError(Serial);
        updateSuccess = false;
    }

    if (updateSuccess) {
        systemMP3((char*)"89", 88, PRIORITY_MP3_MEDIUM);
        LOG_INFOLN("HttpSend: Rebooting in 2 seconds...");
        vTaskDelay(2000);
        ESP.restart();
    } else {
        systemMP3((char*)"89", 87, PRIORITY_MP3_MEDIUM);
    }

    return updateSuccess;
}

/**
* Распаковка TAR архива в указанную директорию с использованием ESP32-targz
* Возвращает true при успешной распаковке, false при ошибке
*/
bool MyHttpSend::extractTar(const String& tarPath, const String& destPath) {

    LOG_INFOLN("HttpSend: Extracting TAR archive: %s to %s", tarPath.c_str(), destPath.c_str());
    
    // Проверяем существование архива
    if (!LittleFS.exists(tarPath)) {
        LOG_ERRORLN("HttpSend: TAR archive not found: %s", tarPath.c_str());
        return false;
    }
    
   
    TarUnpacker tarUnpacker;

 // Лямбда-функция прямо внутри тела setup()
    tarUnpacker.setTarStatusProgressCallback([](const char* name, size_t size, size_t totalBytesDecompressed) {
      LOG_DEBUGLN("📄 File: %s | Size: %u байт | All: %u byte", name, size, totalBytesDecompressed);
    });
    
    // Выполняем распаковку
    bool success = tarUnpacker.tarExpander(LittleFS, tarPath.c_str(), LittleFS, destPath.c_str());    
 //   bool success = unpacker.extract(tarFile, LittleFS, destPath);
    
    
    // Проверяем результат
    if (success) {
        LOG_INFOLN("HttpSend: Archive successfully extracted to %s", destPath.c_str());
        
    } else {
        LOG_ERRORLN("HttpSend: Failed to extract TAR archive");
    }
    
    return success;
}

/**
* Скачиваение с TB сервера и обновление каталога HTTPD
*/
bool MyHttpSend::updateHttpdTB() {
   String url = "";
   url = "/api/v1/";
   url += config["config2"]["TB_TOKEN"].as<String>();
   url += "/attributes?sharedKeys=sw_title,sw_version,sw_checksum";
   String tbHost = config["config2"]["TB_HOST"].as<String>();
   int tbPort = config["config2"]["TB_PORT"].as<int>();
   LOG_INFOLN("Update HTTP: Check HTTPD: %s:%d%s", tbHost.c_str(), tbPort, url.c_str());
   String sw_version    = "";
   String sw_title      = "";
   String sw_checksum   = "";
  
   bool ret = m_httpClient.GET(tbHost.c_str(), tbPort, url);
   if (ret && m_httpClient.m_response.statusCode == 200) {
      JsonDocument respDoc;
      DeserializationError error = deserializeJson(respDoc, m_httpClient.m_response.body);
      sw_version    = respDoc["shared"]["sw_version"].as<String>();
      sw_title      = respDoc["shared"]["sw_title"].as<String>();
      sw_checksum   = respDoc["shared"]["sw_checksum"].as<String>();
   }
   else {
      LOG_ERRORLN("Update HTTP: Error HTTPD attributes");
      return false;
   }

   LOG_INFOLN("Update HTTP: Starting HTTPD update from ThingsBoard...");
   LOG_INFOLN("Update HTTP: HTTPD title: %s, version: %s", sw_title.c_str(), sw_version.c_str());
   LOG_INFOLN("Update HTTP: Expected SHA256 checksum: %s", sw_checksum.c_str());


    // Формируем URL для запроса прошивки
   url = "/api/v1/";
   url += config["config2"]["TB_TOKEN"].as<String>();
   url += "/software?title=" + sw_title + "&version=" + sw_version;
    
   LOG_INFOLN("Update HTTP: Downloading HTTPD from: %s:%d%s",tbHost.c_str(), tbPort, url.c_str());

   ret = m_httpClient.GET_STREAM(tbHost.c_str(), tbPort, url,"",10000);
    
   if( !ret ){
       LOG_ERRORLN("Update HTTP: Error Download File");
       return false;
   } 
    
   String lenStr = m_httpClient.getHeaderValue("Content-Length");
   int contentLength = lenStr.toInt();
   LOG_INFOLN("Update HTTP: Content-Length: %s (%d)", lenStr.c_str(), contentLength);

   //******************************************************************************************************************
   // Блок 4: Подготовка временного файла и SHA256 контекста
   //******************************************************************************************************************
   String tempTarPath = String(WEB_TAR_PATH) + ".tmp";
   if (LittleFS.exists(tempTarPath)) {
      LittleFS.remove(tempTarPath);
   }
   File tempFile = LittleFS.open(tempTarPath, FILE_WRITE);
   if (!tempFile) {
      LOG_ERRORLN("Update HTTP: Failed to create temporary file: %s", tempTarPath.c_str());
      m_httpClient.stop();
      return false;
   }
   mbedtls_sha256_context shaCtx;
   mbedtls_sha256_init(&shaCtx);
   mbedtls_sha256_starts(&shaCtx, 0);
   size_t totalBytesRead = 0;
   int bytesRead = 0;
   uint8_t readBuffer[512];

   uint32_t startTime = millis();

   while (true) {
      if (millis() - startTime > DOWNLOAD_TIMEOUT) {
         LOG_ERRORLN("Update HTTP: Download timeout after %u ms, received %u bytes", DOWNLOAD_TIMEOUT, totalBytesRead);
         tempFile.close();
         LittleFS.remove(tempTarPath);
         m_httpClient.stop();
         return false;
       }

      bytesRead = m_httpClient.m_client.readBytes(readBuffer, sizeof(readBuffer));
      if (bytesRead > 0) {
         tempFile.write(readBuffer, bytesRead);
         mbedtls_sha256_update(&shaCtx, readBuffer, bytesRead);
         totalBytesRead += bytesRead;
      }

      if (contentLength > 0 && totalBytesRead >= (size_t)contentLength) {
         break;
      }

      if (bytesRead == 0) {
         if (!m_httpClient.m_client.connected()) {
            break;
         }
        vTaskDelay(1);
      }
  }

//   while ((bytesRead = m_httpClient.m_client.readBytes(readBuffer, sizeof(readBuffer))) > 0) {
//      totalBytesRead += bytesRead;
//      LOG_INFOLN("Update HTTP: Read %d bytes, total: %u", bytesRead, totalBytesRead);
//      mbedtls_sha256_update(&shaCtx, readBuffer, bytesRead);
//      tempFile.write(readBuffer, bytesRead);
//   }
   m_httpClient.stop();
   tempFile.close();

   
  // Завершаем вычисление SHA256
    uint8_t hash[32];
    mbedtls_sha256_finish(&shaCtx, hash);
    mbedtls_sha256_free(&shaCtx);
    
    // Конвертируем хеш в hex строку
    char hexStr[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hexStr + i * 2, "%02x", hash[i]);
    }
    hexStr[64] = '\0';
    String calculatedChecksum = String(hexStr);
    
    LOG_INFOLN("Update HTTP: Downloaded %d bytes of HTTPD", totalBytesRead);
    LOG_INFOLN("Update HTTP: Calculated SHA256: %s", calculatedChecksum.c_str());
    
    // Проверяем SHA256 сумму
    if (!sw_checksum.isEmpty() && sw_checksum != calculatedChecksum) {
        LOG_ERRORLN("HttpSend: SHA256 checksum mismatch!");
        LOG_ERRORLN("HttpSend: Expected: %s", sw_checksum.c_str());
        LOG_ERRORLN("HttpSend: Got:      %s", calculatedChecksum.c_str());
        LittleFS.remove(tempTarPath);
        return false;
    }
    
    LOG_INFOLN("HttpSend: SHA256 checksum verification successful");
   
   String httpdPath = String(WEB_HTTPD_PATH);
   if (!extractTar(tempTarPath, "/")) {
      LOG_ERRORLN("HttpSend: Failed to extract TAR archive");
      LittleFS.remove(tempTarPath);
      return false;
   }
   
   LittleFS.remove(tempTarPath);
   readJson(WEB_VERSION_FILE, httpd_version);
   LOG_DEBUGLN("HttpSend: Removed temporary file: %s", tempTarPath.c_str());
   LOG_INFOLN("HttpSend: Web interface updated successfully");
   HTTP_sendResponse(WebResponse::combine({
        WebResponse::msg("Новая версия HTTPD загружена. Сейчас страница обновится ...", "success", 5000),
        WebResponse::reload(5000)
   }));

   return true;
}
