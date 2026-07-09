#define MODULE_NAME "HTTP_SEND"
#define MODULE_DEBUG_LEVEL DEBUG_INFO
#include "src/Slib/SDEBUG.h"
#include "WC_HttpSend.h"
#include "WC_Config.h"

//*********************************************************************************************************************
// Base64Stream - потоковое Base64 кодирование
//*********************************************************************************************************************

const char Base64Stream::m_base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

Base64Stream::Base64Stream(Stream* input, size_t inputSize) {
    m_input = input;
    m_inputRemaining = inputSize;
    m_encodedSize = 4 * ((inputSize + 2) / 3);
    m_outputPos = 0;
    m_outputLen = 0;
    m_finished = false;
    memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
}

Base64Stream::~Base64Stream() {
    m_input = nullptr;
}

int Base64Stream::available() {
    if (m_outputPos < m_outputLen) {
        return m_outputLen - m_outputPos;
    }
    if (!m_finished && m_inputRemaining > 0) {
        encodeNextBlock();
        return m_outputLen - m_outputPos;
    }
    return 0;
}

int Base64Stream::read() {
    if (available() == 0) {
        return -1;
    }
    return m_outputBuffer[m_outputPos++];
}

int Base64Stream::peek() {
    if (available() == 0) {
        return -1;
    }
    return m_outputBuffer[m_outputPos];
}

void Base64Stream::encodeNextBlock() {
    if (m_finished || m_inputRemaining == 0) {
        m_finished = true;
        m_outputLen = 0;
        return;
    }
    size_t bytesRead = 0;
    while (bytesRead < 3 && m_inputRemaining > 0) {
        int b = m_input->read();
        if (b >= 0) {
            m_inputBuffer[bytesRead++] = (uint8_t)b;
            m_inputRemaining--;
        } else {
            break;
        }
    }
    if (bytesRead == 0) {
        m_finished = true;
        m_outputLen = 0;
        return;
    }
    m_outputBuffer[0] = m_base64Table[m_inputBuffer[0] >> 2];
    m_outputBuffer[1] = m_base64Table[((m_inputBuffer[0] & 0x03) << 4) | ((bytesRead > 1 ? m_inputBuffer[1] : 0) >> 4)];
    m_outputBuffer[2] = (bytesRead > 1) ? m_base64Table[((m_inputBuffer[1] & 0x0F) << 2) | ((bytesRead > 2 ? m_inputBuffer[2] : 0) >> 6)] : '=';
    m_outputBuffer[3] = (bytesRead > 2) ? m_base64Table[m_inputBuffer[2] & 0x3F] : '=';
    m_outputBuffer[4] = '\0';
    m_outputLen = 4;
    m_outputPos = 0;
    if (m_inputRemaining == 0) {
        m_finished = true;
    }
}

//*********************************************************************************************************************
// MyHttpSend - основной класс
//*********************************************************************************************************************

JsonDocument jsonData;

MyHttpSend::MyHttpSend() {
    m_tarPath = String(WEB_TAR_PATH);
    m_httpdPath = String(WEB_HTTPD_PATH);
    m_versionFile = String(WEB_VERSION_FILE);
    parseExcludeList(String(WEB_EXCLUDE_FILES));
}

/**
* Инициализация модуля
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
    LOG_INFOLN("HttpSend: Module initialized successfully");
    return true;
}

/**
* Разбор списка исключаемых файлов
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

//*********************************************************************************************************************
// Работа с TAR архивом
//*********************************************************************************************************************

/**
* Создание TAR архива из папки httpd
*/
bool MyHttpSend::createTarArchive() {
    LOG_INFOLN("HttpSend: Creating TAR archive");
    if (LittleFS.exists(m_tarPath)) {
        LittleFS.remove(m_tarPath);
        LOG_DEBUGLN("HttpSend: Removed old archive");
    }
    File tarFile = LittleFS.open(m_tarPath, FILE_WRITE);
    if (!tarFile) {
        LOG_ERRORLN("HttpSend: Failed to create archive file");
        return false;
    }
    addFilesToTar(m_httpdPath, tarFile, m_httpdPath);
    uint8_t emptyBlock[TAR_BLOCK_SIZE] = {0};
    tarFile.write(emptyBlock, TAR_BLOCK_SIZE);
    tarFile.write(emptyBlock, TAR_BLOCK_SIZE);
    tarFile.close();
    size_t archiveSize = 0;
    File checkFile = LittleFS.open(m_tarPath, FILE_READ);
    if (checkFile) {
        archiveSize = checkFile.size();
        checkFile.close();
    }
    LOG_INFOLN("HttpSend: Archive created, size: %u bytes", archiveSize);
    return true;
}

/**
* Рекурсивное добавление файлов в TAR архив
*/
void MyHttpSend::addFilesToTar(const String& dirPath, File& tarFile, const String& basePath) {
    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        LOG_ERRORLN("HttpSend: Failed to open directory: %s", dirPath.c_str());
        return;
    }
    File file = dir.openNextFile();
    while (file) {
        String fullPath = file.path();
        String relativePath = fullPath.substring(basePath.length());
        if (relativePath.startsWith("/")) {
            relativePath = relativePath.substring(1);
        }
        if (file.isDirectory()) {
            addFilesToTar(fullPath, tarFile, basePath);
        } else {
            if (!shouldExcludeFile(fullPath)) {
                size_t fileSize = file.size();
                LOG_DEBUGLN("HttpSend: Adding to archive: %s (%u bytes)", relativePath.c_str(), fileSize);
                writeTarHeader(tarFile, relativePath, fileSize);
                uint8_t buffer[256];
                size_t bytesRemaining = fileSize;
                while (bytesRemaining > 0) {
                    size_t toRead = min(bytesRemaining, sizeof(buffer));
                    size_t bytesRead = file.read(buffer, toRead);
                    tarFile.write(buffer, bytesRead);
                    bytesRemaining -= bytesRead;
                }
                size_t padding = TAR_BLOCK_SIZE - (fileSize % TAR_BLOCK_SIZE);
                if (padding > 0 && padding < TAR_BLOCK_SIZE) {
                    uint8_t padBuffer[TAR_BLOCK_SIZE] = {0};
                    tarFile.write(padBuffer, padding);
                }
            } else {
                LOG_DEBUGLN("HttpSend: Excluded file: %s", fullPath.c_str());
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
}

/**
* Запись заголовка TAR
*/
void MyHttpSend::writeTarHeader(File& tarFile, const String& fileName, size_t fileSize) {
    unsigned char header[TAR_BLOCK_SIZE] = {0};
    strncpy((char*)header, fileName.c_str(), 99);
    snprintf((char*)header + 100, 8, "%07o", 0644);
    snprintf((char*)header + 108, 8, "%07o", 0);
    snprintf((char*)header + 116, 8, "%07o", 0);
    snprintf((char*)header + 124, 12, "%011o", fileSize);
    snprintf((char*)header + 136, 12, "%011o", 0);
    header[156] = '0';
    unsigned int checksum = calculateTarChecksum(header);
    snprintf((char*)header + 148, 8, "%07o", checksum);
    header[155] = ' ';
    tarFile.write(header, TAR_BLOCK_SIZE);
}

/**
* Расчет контрольной суммы TAR заголовка
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

/**
* Проверка необходимости исключения файла
*/
bool MyHttpSend::shouldExcludeFile(const String& filePath) {
    for (size_t i = 0; i < m_excludeFiles.size(); i++) {
        if (filePath.equals(m_excludeFiles[i]) || filePath.endsWith("/" + m_excludeFiles[i])) {
            return true;
        }
    }
    return false;
}

//*********************************************************************************************************************
// Вспомогательные функции
//*********************************************************************************************************************

/**
* Расчет SHA-256 хеша файла
*/
String MyHttpSend::calculateSHA256(const String& filePath) {
    File file = LittleFS.open(filePath, FILE_READ);
    if (!file) {
        LOG_ERRORLN("HttpSend: Failed to open file for SHA256: %s", filePath.c_str());
        return "";
    }
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    uint8_t buffer[256];
    size_t bytesRead;
    while ((bytesRead = file.read(buffer, sizeof(buffer))) > 0) {
        mbedtls_sha256_update(&ctx, buffer, bytesRead);
    }
    file.close();
    uint8_t hash[32];
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    char hexStr[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hexStr + i * 2, "%02x", hash[i]);
    }
    hexStr[64] = '\0';
    return String(hexStr);
}

/**
* Base64 кодирование строки
*/
String MyHttpSend::encodeBase64(const String& input) {
    size_t outputLen = 4 * ((input.length() + 2) / 3) + 1;
    unsigned char* output = new unsigned char[outputLen];
    size_t actualLen = 0;
    int ret = mbedtls_base64_encode(output, outputLen, &actualLen,
        (const unsigned char*)input.c_str(), input.length());
    String result;
    if (ret == 0) {
        result = String((char*)output);
    } else {
        LOG_ERRORLN("HttpSend: Base64 encoding failed with error %d", ret);
    }
    delete[] output;
    return result;
}

/**
* Чтение или создание файла версии
*/
String MyHttpSend::readOrCreateVersionFile() {
    String versionContent = "";
    if (LittleFS.exists(m_versionFile)) {
        File versionFile = LittleFS.open(m_versionFile, FILE_READ);
        if (versionFile) {
            versionContent = versionFile.readString();
            versionFile.close();
            LOG_DEBUGLN("HttpSend: Version file read: %s", versionContent.c_str());
        }
    }
    if (versionContent.isEmpty()) {
        versionContent = "{\"version\":\"" + String(HTTPD_V) + "\"}";
        File versionFile = LittleFS.open(m_versionFile, FILE_WRITE);
        if (versionFile) {
            versionFile.print(versionContent);
            versionFile.close();
            LOG_INFOLN("HttpSend: Created version file: %s", versionContent.c_str());
        } else {
            LOG_ERRORLN("HttpSend: Failed to create version file");
        }
    }
    return versionContent;
}

/**
* Потоковая отправка данных в ThingsBoard
*/
bool MyHttpSend::sendStreamToTB(const String& tbHost, int tbPort, const String& tbToken,
                                 const String& infoJson, size_t archiveSize) {
    File tarFile = LittleFS.open(m_tarPath, FILE_READ);
    if (!tarFile) {
        LOG_ERRORLN("HttpSend: Failed to open archive for streaming");
        return false;
    }
    Base64Stream base64Stream(&tarFile, archiveSize);
    size_t encodedSize = base64Stream.encodedSize();
    LOG_DEBUGLN("HttpSend: Encoded size: %u bytes", encodedSize);
    
    const char* jsonPrefix = "{\"httpd_data\":\"";
    const char* jsonMiddle = "\",\"httpd_info\":";
    const char* jsonSuffix = "}";
    size_t prefixLen = strlen(jsonPrefix);
    size_t middleLen = strlen(jsonMiddle);
    size_t suffixLen = strlen(jsonSuffix);
    size_t totalSize = prefixLen + encodedSize + middleLen + infoJson.length() + suffixLen;
    LOG_DEBUGLN("HttpSend: Total body size: %u", totalSize);
    
    WiFiClient client;
    if (!client.connect(tbHost.c_str(), tbPort)) {
        LOG_ERRORLN("HttpSend: Connection failed to %s:%d", tbHost.c_str(), tbPort);
        tarFile.close();
        return false;
    }
    String url = "/api/v1/" + tbToken + "/attributes";
    client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "\r\n",
        url.c_str(),
        tbHost.c_str(),
        totalSize
    );
    client.print(jsonPrefix);
    uint8_t buffer[256];
    int bytesRead;
    while ((bytesRead = base64Stream.readBytes((char*)buffer, sizeof(buffer))) > 0) {
        client.write(buffer, bytesRead);
        yield();
    }
    client.print(jsonMiddle);
    client.print(infoJson);
    client.print(jsonSuffix);
    HttpResponse response;
    unsigned long timeout = millis();
    while (!client.available()) {
        if (millis() - timeout > 5000) {
            client.stop();
            tarFile.close();
            return false;
        }
        vTaskDelay(1);
    }
    String respStr = client.readString();
    SimpleHttpClient::parseHttpResponseShared(respStr, response);
    client.stop();
    tarFile.close();
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("HttpSend: Stream send successful");
        return true;
    } else {
        LOG_ERRORLN("HttpSend: Stream send failed, status: %d", response.statusCode);
        return false;
    }
}

/**
* Отправка веб-файлов в ThingsBoard
*/
bool MyHttpSend::sendWebFilesToTB() {
    LOG_INFOLN("HttpSend: Starting send to ThingsBoard");
    if (config["config2"]["TB_TOKEN"].isNull() || config["config2"]["TB_TOKEN"].as<String>().isEmpty()) {
        LOG_ERRORLN("HttpSend: TB_TOKEN not configured");
        return false;
    }
    if (!createTarArchive()) {
        LOG_ERRORLN("HttpSend: Failed to create archive");
        return false;
    }
    String shaHash = calculateSHA256(m_tarPath);
    LOG_INFOLN("HttpSend: Archive SHA256: %s", shaHash.c_str());
    size_t archiveSize = 0;
    File tarFile = LittleFS.open(m_tarPath, FILE_READ);
    if (tarFile) {
        archiveSize = tarFile.size();
        tarFile.close();
    }
    String versionContent = readOrCreateVersionFile();
    String versionNumber = HTTPD_V;
    JsonDocument versionDoc;
    DeserializationError error = deserializeJson(versionDoc, versionContent);
    if (!error && versionDoc.containsKey("version")) {
        versionNumber = versionDoc["version"].as<String>();
    }
    JsonDocument infoDoc;
    infoDoc["version"] = versionNumber;
    infoDoc["sha256"] = shaHash;
    String infoJson;
    serializeJson(infoDoc, infoJson);
    LOG_DEBUGLN("HttpSend: Info JSON: %s", infoJson.c_str());
    String tbHost = config["config2"]["TB_HOST"].as<String>();
    int tbPort = config["config2"]["TB_PORT"].as<int>();
    String tbToken = config["config2"]["TB_TOKEN"].as<String>();
    bool result = sendStreamToTB(tbHost, tbPort, tbToken, infoJson, archiveSize);
    cleanupTempFiles();
    return result;
}

/**
* Очистка временных файлов
*/
void MyHttpSend::cleanupTempFiles() {
    if (LittleFS.exists(m_tarPath)) {
        LittleFS.remove(m_tarPath);
        LOG_INFOLN("HttpSend: Temporary archive removed");
    }
}

//*********************************************************************************************************************
// Глобальные вспомогательные функции
//*********************************************************************************************************************

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
* Генерация ключа (CRC)
*/
uint16_t KeyGen(char *str) {
    uint16_t crc = 0;
    for (int i = 0; i < strlen(str); i++) {
        crc += (int)str[i];
    }
    crc = (~crc) & 0xfff;
    return crc;
}

//*********************************************************************************************************************
// Отправка данных на внешние сервисы (методы класса MyHttpSend)
//*********************************************************************************************************************

/**
* Отправка в CRM Москва
*/
bool MyHttpSend::sendCrmMoscowParam() {
    bool ret = false;
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
    HttpResponse response = SimpleHttpClient::GET(
        config["config2"]["CRM_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url);
    if (response.statusCode == 200) {
        LOG_INFOLN("HttpSend: CRM data sent successfully");
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: CRM send error, HTTP status: %d", response.statusCode);
    }
    return ret;
}

/**
* Отправка на HTTP шлюзы (список серверов)
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
* Отправка на один HTTP шлюз
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
    HttpResponse response = SimpleHttpClient::GET(host.c_str(), port, path);
    if (response.statusCode == 200) {
        LOG_INFOLN("HttpSend: HTTP data sent successfully to: %s", host.c_str());
        return true;
    } else {
        LOG_ERRORLN("HttpSend: HTTP send error to %s, status: %d", host.c_str(), response.statusCode);
        return false;
    }
}

/**
* Проверка версии конфигурации
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
* Отправка конфигурации в ThingsBoard
*/
bool MyHttpSend::sendConfigToTB() {
    bool ret = false;
    checkConfigVersion();
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    
    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() ||
            config["config2"]["TB_TOKEN"] == "") {
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                LOG_ERRORLN("HttpSend: ThingsBoard authentication failed for config send");
                return false;
            }
        }
    }
    
    String url = "/api/v1/";
    if (is_over_gate) {
        url += strID;
    } else {
        url += config["config2"]["TB_TOKEN"].as<String>();
    }
    url += "/attributes";
    
    JsonDocument configDoc;
    if (!config["main"].isNull()) {
        configDoc["config_date"] = config["main"];
    }
    JsonDocument configInfo;
    if (!config["main"]["label"].isNull()) {
        configInfo["label"] = config["main"]["label"];
    }
    if (!config["main"]["version"].isNull()) {
        configInfo["version"] = config["main"]["version"];
    }
    configDoc["config_info"] = configInfo;
    String data;
    serializeJson(configDoc, data);
    LOG_DEBUGLN("HttpSend: Config attributes JSON: %s", data.c_str());
    
    String headers = "Content-Type: application/json\r\n";
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("HttpSend: Config attributes sent successfully to ThingsBoard");
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: Config attributes send failed, HTTP status: %d", response.statusCode);
    }
    return ret;
}

/**
* Отправка телеметрии в ThingsBoard
*/
bool MyHttpSend::sendParamTB() {
    bool ret = false;
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    
    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() ||
            config["config2"]["TB_TOKEN"] == "") {
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                LOG_ERRORLN("HttpSend: ThingsBoard authentication failed");
                return false;
            }
        }
    }
    
    String url = "/api/v1/";
    if (is_over_gate) {
        url += strID;
    } else {
        url += config["config2"]["TB_TOKEN"].as<String>();
    }
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
    
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("HttpSend: ThingsBoard telemetry sent successfully");
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response.body);
        if (!error && respDoc["status"].as<String>() == "SUCCESS") {
            config["config2"]["TB_TOKEN"] = respDoc["credentialsValue"].as<String>();
            configWrite();
            LOG_INFOLN("HttpSend: ThingsBoard token updated");
        }
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: ThingsBoard telemetry send failed, HTTP status: %d", response.statusCode);
    }
    return ret;
}

/**
* Авторизация устройства в ThingsBoard
*/
bool MyHttpSend::authTB(const char *key, const char *secret) {
    bool ret = false;
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
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    LOG_DEBUGLN("HttpSend: ThingsBoard auth response status: %d", response.statusCode);
    if (response.statusCode >= 200 && response.statusCode < 300) {
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response.body);
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
        LOG_ERRORLN("HttpSend: ThingsBoard auth failed, HTTP status: %d", response.statusCode);
    }
    return ret;
}

/**
* Отправка атрибутов устройства в ThingsBoard
*/
bool MyHttpSend::sendAttributeTB() {
    bool ret = false;
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
    if (is_over_gate) {
        url += strID;
    } else {
        url += config["config2"]["TB_TOKEN"].as<String>();
    }
    url += "/attributes?clientKeys";
    jsonData.clear();
    jsonData["SerialNo"] = serNo;
    jsonData["DogovorNo"] = config["config2"]["N_DOGIVOR"].as<String>();
    jsonData["BoxNo"] = config["config2"]["N_BOX"].as<String>();
    String data;
    serializeJson(jsonData, data);
    String headers = "Content-Type: application/json\r\n";
    LOG_DEBUGLN("HttpSend: Sending attributes to ThingsBoard...");
    HttpResponse response = SimpleHttpClient::POST(
        config["config2"]["TB_HOST"].as<const char *>(),
        config["config2"]["TB_PORT"].as<int>(),
        url,
        "application/json",
        data,
        headers);
    if (response.statusCode >= 200 && response.statusCode < 300) {
        LOG_INFOLN("HttpSend: ThingsBoard attributes sent successfully");
        ret = true;
    } else {
        LOG_ERRORLN("HttpSend: ThingsBoard attributes send failed, HTTP status: %d", response.statusCode);
    }
    return ret;
}

// ... (предыдущий код остаётся без изменений до метода sendAttributeTB)

/**
* Проверка и обновление конфигурации с сервера ThingsBoard
*/
bool MyHttpSend::checkAndUpdateConfig() {
    LOG_INFOLN("HttpSend: Checking config_info attribute...");
    
    // Проверяем наличие атрибута config_info в конфигурации
    if (config["config_info"].isNull()) {
        LOG_INFOLN("HttpSend: config_info not found, sending config to server...");
        return sendConfigToTB();
    }
    
    // Получаем версию из config_info
    String serverVersion = config["config_info"]["version"].as<String>();
    String localVersion = config["main"]["version"].as<String>();
    
    LOG_DEBUGLN("HttpSend: Server version: %s, Local version: %s", 
        serverVersion.c_str(), localVersion.c_str());
    
    // Сравниваем версии
    if (serverVersion != localVersion) {
        LOG_INFOLN("HttpSend: Version mismatch, fetching config from server...");
        
        // Загружаем конфигурацию с сервера
        if (fetchConfigFromTB()) {
            LOG_INFOLN("HttpSend: Config updated successfully");
            return true;
        } else {
            LOG_ERRORLN("HttpSend: Failed to fetch config from server");
            return false;
        }
    } else {
        LOG_DEBUGLN("HttpSend: Config versions match, no update needed");
        return true;
    }
}

/**
* Загрузка конфигурации с ThingsBoard
*/
bool MyHttpSend::fetchConfigFromTB() {
    LOG_INFOLN("HttpSend: Fetching config from ThingsBoard...");
    
    bool is_over_gate = config["config2"]["TB"]["GATEWAY"].as<bool>();
    
    // Проверяем авторизацию
    if (!is_over_gate) {
        if (config["config2"]["TB_TOKEN"].isNull() ||
            config["config2"]["TB_TOKEN"] == "") {
            if (!authTB(TB_PROVISION_KEY, TB_PROVISION_SECRET)) {
                LOG_ERRORLN("HttpSend: ThingsBoard authentication failed for config fetch");
                return false;
            }
        }
    }
    
    // Формируем URL для запроса атрибутов
    String url = "/api/v1/";
    if (is_over_gate) {
        url += strID;
    } else {
        url += config["config2"]["TB_TOKEN"].as<String>();
    }
    url += "/attributes";
    
    // Выполняем GET запрос
    String tbHost = config["config2"]["TB_HOST"].as<String>();
    int tbPort = config["config2"]["TB_PORT"].as<int>();
    
    LOG_DEBUGLN("HttpSend: Fetching config from: %s:%d%s", 
        tbHost.c_str(), tbPort, url.c_str());
    
    HttpResponse response = SimpleHttpClient::GET(
        tbHost.c_str(),
        tbPort,
        url);
    
    if (response.statusCode != 200) {
        LOG_ERRORLN("HttpSend: Failed to fetch config, HTTP status: %d", 
            response.statusCode);
        return false;
    }
    
    // Парсим полученные данные
    JsonDocument respDoc;
    DeserializationError error = deserializeJson(respDoc, response.body);
    if (error) {
        LOG_ERRORLN("HttpSend: Failed to parse config response: %s", 
            error.c_str());
        return false;
    }
    
    // Проверяем наличие config_info в ответе
    if (!respDoc.containsKey("config_info")) {
        LOG_ERRORLN("HttpSend: No config_info in server response");
        return false;
    }
    
    // Обновляем локальную конфигурацию
    LOG_DEBUGLN("HttpSend: Server config_info: %s", 
        respDoc["config_info"].as<String>().c_str());
    
    // Копируем данные из config_info в основной конфиг
    JsonDocument newConfig;
    if (!respDoc["config_info"].isNull()) {
        // Сохраняем старые значения для тех полей, которых нет в новом конфиге
        // или используем новые значения для существующих полей
        for (JsonPair kv : respDoc["config_info"].as<JsonObject>()) {
            config["main"][kv.key()] = kv.value();
        }
    }
    
    // Сохраняем обновленную конфигурацию
    saveConfigData();
    LOG_INFOLN("HttpSend: Config saved successfully");
    return true;

}

//*********************************************************************************************************************
// Функции для проверки обновления версии прошивки на сервере ThingsBoard
//*********************************************************************************************************************

/**
* Проверяет наличие новой версии прошивки на сервере ThingsBoard
* Выполняется каждые TM_TB_CHECK миллисекунд
*/
bool MyHttpSend::checkFirmwareVersion() {
   // Проверяем, не пора ли выполнить проверку
   uint32_t currentTime = millis();
   LOG_INFOLN("HttpSend: Checking firmware version from server...");
   // Формируем URL для запроса атрибутов устройства
   String url = "/api/v1/";
   url += config["config2"]["TB_TOKEN"].as<String>();
   url += "/attributes?sharedKeys=fw_title,fw_version,fw_checksum";
   String tbHost = config["config2"]["TB_HOST"].as<String>();
   int tbPort = config["config2"]["TB_PORT"].as<int>();
   LOG_DEBUGLN("HttpSend: Fetching firmware version from: %s:%d%s", tbHost.c_str(), tbPort, url.c_str());
   // Выполняем GET запрос к ThingsBoard
   HttpResponse response = SimpleHttpClient::GET(
      tbHost.c_str(),
      tbPort,
      url);
   if (response.statusCode != 200) {
      LOG_ERRORLN("HttpSend: Failed to fetch firmware version, HTTP status: %d",
         response.statusCode);
      return false;
   }
   // Парсим ответ сервера
   JsonDocument respDoc;
   DeserializationError error = deserializeJson(respDoc, response.body);
   LOG_DEBUGLN("BODY %s",response.body.c_str());
   if (error) {
      LOG_ERRORLN("HttpSend: Failed to parse firmware version response: %s",
         error.c_str());
      return false;
   }
   // Проверяем наличие атрибута с версией прошивки
   if (respDoc["shared"].containsKey("fw_version")) {
      String serverVersion = respDoc["shared"]["fw_version"].as<String>();
      String serverTitle   = respDoc["shared"]["fw_title"].as<String>();
      LOG_INFOLN("HttpSend: Server firmware %s %s", serverTitle.c_str(), serverVersion.c_str());
      // Получаем текущую версию прошивки из системы
      String currentVersion = FW_VERSION;
      String currentTitle   = FW_NAME;
      LOG_INFOLN("HttpSend: Current firmware %s %s", currentTitle.c_str(), currentVersion.c_str());
      // Сравниваем версии (пока только логируем, Update не запускаем)
      if (serverVersion != currentVersion && currentTitle == serverTitle) {
         LOG_INFOLN("HttpSend: *** NEW FIRMWARE VERSION AVAILABLE ***");
         LOG_INFOLN("HttpSend: new firmware", serverTitle.c_str(), serverVersion.c_str());
         // Отправляем уведомление через Web интерфейс
         HTTP_sendResponse(WebResponse::combine({
            WebResponse::msg("Доступна новая версия прошивки: " + serverVersion, "warning", 5000),
            WebResponse::config("firmware", "new_version", serverVersion)
         }));
         updateFirmwareFromTB(serverTitle,serverVersion);
         return true;
      } else {
         LOG_DEBUGLN("HttpSend: Firmware version is up to date");
      }
   } else {
      LOG_DEBUGLN("HttpSend: No 'fw_ver' attribute found on server");
   }
   return false;
}

/**
* Выполняет обновление прошивки с сервера ThingsBoard
* Загружает бинарный файл прошивки и применяет его
*/
bool MyHttpSend::updateFirmwareFromTB(const String& fwTitle, const String& fwVersion) {
   LOG_INFOLN("HttpSend: Starting firmware update from ThingsBoard...");
   LOG_INFOLN("HttpSend: Firmware title: %s, version: %s", fwTitle.c_str(), fwVersion.c_str());
   
   // Формируем URL для запроса прошивки
   String url = "/api/v1/";
   url += config["config2"]["TB_TOKEN"].as<String>();
   url += "/firmware?title=" + fwTitle + "&version=" + fwVersion;
   
   String tbHost = config["config2"]["TB_HOST"].as<String>();
   int tbPort = config["config2"]["TB_PORT"].as<int>();
   LOG_DEBUGLN("HttpSend: Downloading firmware from: %s:%d%s", 
      tbHost.c_str(), tbPort, url.c_str());
   
   // Используем WiFiClient напрямую для стриминговой загрузки
   WiFiClient client;
   if (!SimpleHttpClient::connectAndSendRequest(client, tbHost.c_str(), tbPort, url)) {
      LOG_ERRORLN("HttpSend: Failed to connect to ThingsBoard for firmware download");
      return false;
   }
   
   // Ожидаем ответ
   unsigned long timeout = millis();
   while (!client.available()) {
      if (millis() - timeout > 10000) {
         LOG_ERRORLN("HttpSend: Timeout waiting for firmware download response");
         client.stop();
         return false;
      }
      vTaskDelay(1);
   }
   
   // Парсим статусную строку
   String statusLine = client.readStringUntil('\n');
   LOG_DEBUGLN("HttpSend: Response: %s", statusLine.c_str());
   
   if (!statusLine.startsWith("HTTP/1.1 200")) {
      LOG_ERRORLN("HttpSend: Firmware download failed, status: %s", statusLine.c_str());
      client.stop();
      return false;
   }
   
   // Парсим заголовки
   String line;
   int contentLength = -1;
   bool chunked = false;
   
   while ((line = client.readStringUntil('\n')) != "\r" && line != "\n") {
      line.trim();
      LOG_DEBUGLN("HttpSend: Header: %s", line.c_str());
      
      if (line.startsWith("Content-Length:")) {
         contentLength = line.substring(line.indexOf(':') + 1).toInt();
      } else if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
         chunked = true;
      }
   }
   
   LOG_INFOLN("HttpSend: Starting firmware update, content-length: %d, chunked: %d", 
      contentLength, chunked);
   
   // Инициализируем обновление
   if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      LOG_ERRORLN("HttpSend: Failed to begin firmware update");
      Update.printError(Serial);
      client.stop();
      return false;
   }
   
   // Проигрываем звуковой сигнал начала обновления
   systemMP3((char*)"89", 89, PRIORITY_MP3_MEDIUM);
   
   size_t totalBytes = 0;
   bool updateSuccess = false;
   
   if (chunked) {
      // Обработка chunked-ответа
      while (client.connected()) {
         String chunkHeader = client.readStringUntil('\n');
         chunkHeader.trim();
         
         if (chunkHeader.isEmpty()) {
            continue;
         }
         
         int chunkSize = strtol(chunkHeader.c_str(), NULL, 16);
         LOG_DEBUGLN("HttpSend: Chunk size: %d", chunkSize);
         
         if (chunkSize == 0) {
            LOG_INFOLN("HttpSend: Chunked transfer completed");
            break;
         }
         
         uint8_t* buffer = new uint8_t[chunkSize];
         size_t bytesRead = client.readBytes(buffer, chunkSize);
         
         if (bytesRead != chunkSize) {
            LOG_ERRORLN("HttpSend: Chunk read error, expected %d, got %d", chunkSize, bytesRead);
            delete[] buffer;
            Update.abort();
            client.stop();
            return false;
         }
         
         // Пропускаем завершающий CRLF
         client.readStringUntil('\n');
         
         // Записываем данные в Update
         if (Update.write(buffer, bytesRead) != bytesRead) {
            LOG_ERRORLN("HttpSend: Update write error");
            Update.printError(Serial);
            delete[] buffer;
            Update.abort();
            client.stop();
            return false;
         }
         
         totalBytes += bytesRead;
         LOG_DEBUGLN("HttpSend: Written %d bytes, total: %d", bytesRead, totalBytes);
         delete[] buffer;
      }
   } else {
      // Обработка обычного ответа с Content-Length
      uint8_t buffer[512];
      int bytesRead;
      
      while ((bytesRead = client.readBytes(buffer, sizeof(buffer))) > 0) {
         if (Update.write(buffer, bytesRead) != bytesRead) {
            LOG_ERRORLN("HttpSend: Update write error");
            Update.printError(Serial);
            Update.abort();
            client.stop();
            return false;
         }
         totalBytes += bytesRead;
         LOG_DEBUGLN("HttpSend: Written %d bytes, total: %d", bytesRead, totalBytes);
      }
   }
   
   client.stop();
   
   LOG_INFOLN("HttpSend: Downloaded %d bytes of firmware", totalBytes);
   
   // Завершаем обновление
   if (Update.end(true)) {
      LOG_INFOLN("HttpSend: Firmware update successful");
      updateSuccess = true;
   } else {
      LOG_ERRORLN("HttpSend: Firmware update failed");
      Update.printError(Serial);
      updateSuccess = false;
   }
   
   if (updateSuccess) {
      // Проигрываем звук успешного обновления
      systemMP3((char*)"89", 88, PRIORITY_MP3_MEDIUM);
      LOG_INFOLN("HttpSend: Rebooting in 2 seconds...");
      vTaskDelay(2000);
      ESP.restart();
   } else {
      // Проигрываем звук ошибки
      systemMP3((char*)"89", 87, PRIORITY_MP3_MEDIUM);
   }
   
   return updateSuccess;
}

/**
* Возвращает текущую версию прошивки с сервера
*/
String MyHttpSend::getServerFirmwareVersion() {
//   return g_serverFirmwareVersion;
}