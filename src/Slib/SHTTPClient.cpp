// ============================================
// Файл: src/Slib/SHTTPClient.cpp (исправленный)
// ============================================
#include "SHTTPClient.h"

//*********************************************************************************************************************
// Конструктор/деструктор
//*********************************************************************************************************************

// Конструктор
SimpleHttpClient::SimpleHttpClient() {
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
}

// Деструктор
SimpleHttpClient::~SimpleHttpClient() {
    stop();
}

// Инициализация клиента
bool SimpleHttpClient::begin() {
    stop();
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
    return true;
}

// Закрывает соединение
void SimpleHttpClient::stop() {
    if (m_client.connected()) {
        m_client.stop();
    }
    m_connected = false;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
}

//*********************************************************************************************************************
// GET запросы
//*********************************************************************************************************************

// Выполняет GET запрос
HttpResponse SimpleHttpClient::GET(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders
) {
    HttpResponse response;
    if (!m_client.connect(host, port)) {
        return response;
    }
    m_connected = true;
    m_client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        path.c_str(),
        host
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    readResponse(response);
    return response;
}

//*********************************************************************************************************************
// POST запросы
//*********************************************************************************************************************

// Выполняет POST запрос
HttpResponse SimpleHttpClient::POST(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    const String& payload,
    const String& extraHeaders
) {
    HttpResponse response;
    if (!m_client.connect(host, port)) {
        return response;
    }
    m_connected = true;
    m_client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n",
        path.c_str(),
        host,
        contentType.c_str(),
        payload.length()
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    m_client.print(payload);
    readResponse(response);
    return response;
}

// Выполняет POST запрос с JSON данными
HttpResponse SimpleHttpClient::POST_JSON(
    const char* host,
    uint16_t port,
    const String& path,
    const String& json,
    const String& extraHeaders
) {
    return POST(host, port, path, "application/json", json, extraHeaders);
}

// Выполняет POST запрос с текстовыми данными
HttpResponse SimpleHttpClient::POST_TEXT(
    const char* host,
    uint16_t port,
    const String& path,
    const String& text,
    const String& extraHeaders
) {
    return POST(host, port, path, "text/plain", text, extraHeaders);
}

// Выполняет POST запрос с потоковыми данными
HttpResponse SimpleHttpClient::POST_STREAM(
    const char* host,
    uint16_t port,
    const String& path,
    const String& contentType,
    Stream& payloadStream,
    size_t contentLength,
    const String& extraHeaders
) {
    HttpResponse response;
    if (!m_client.connect(host, port)) {
        return response;
    }
    m_connected = true;
    m_client.printf(
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n",
        path.c_str(),
        host,
        contentType.c_str(),
        contentLength
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    uint8_t buffer[512];
    size_t bytesSent = 0;
    while (bytesSent < contentLength) {
        size_t toRead = min(sizeof(buffer), contentLength - bytesSent);
        size_t bytesRead = payloadStream.readBytes(buffer, toRead);
        if (bytesRead > 0) {
            m_client.write(buffer, bytesRead);
            bytesSent += bytesRead;
        } else {
            break;
        }
        yield();
    }
    readResponse(response);
    return response;
}

//*********************************************************************************************************************
// Стриминговые запросы
//*********************************************************************************************************************

// Устанавливает соединение и отправляет GET запрос
bool SimpleHttpClient::connectAndSendRequest(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders
) {
    if (!m_client.connect(host, port)) {
        return false;
    }
    m_connected = true;
    m_client.printf(
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n",
        path.c_str(),
        host
    );
    if (extraHeaders.length()) {
        m_client.print(extraHeaders);
    }
    m_client.print("\r\n");
    return true;
}

// Выполняет GET запрос с возможностью стримингового чтения
HttpResponse SimpleHttpClient::GET_STREAM(
    const char* host,
    uint16_t port,
    const String& path,
    const String& extraHeaders,
    bool* isChunked
) {
    HttpResponse response;
    m_chunkedMode = false;
    m_currentChunkSize = 0;
    
    if (!connectAndSendRequest(host, port, path, extraHeaders)) {
        return response;
    }
    
    // Ожидаем ответ
    unsigned long timeout = millis();
    while (!m_client.available()) {
        if (millis() - timeout > 10000) {
            m_client.stop();
            m_connected = false;
            return response;
        }
        vTaskDelay(1);
    }
    
    // Читаем статусную строку
    String statusLine = m_client.readStringUntil('\n');
    if (!statusLine.startsWith("HTTP/1.1 200")) {
        m_client.stop();
        m_connected = false;
        return response;
    }
    
    // Парсим заголовки
    String line;
    int contentLength = -1;
    bool chunked = false;
    
    while ((line = m_client.readStringUntil('\n')) != "\r" && line != "\n") {
        line.trim();
        response.headers += line + "\r\n";
//        Serial.printf("!!! HttpSend: Header: %s\n", line.c_str());
        
        if (line.startsWith("Content-Length:")) {
            contentLength = line.substring(line.indexOf(':') + 1).toInt();
        } else if (line.startsWith("Transfer-Encoding:") && line.indexOf("chunked") >= 0) {
            chunked = true;
        }
    }
    
    response.statusCode = 200;
    
    // Сохраняем режим передачи
    m_chunkedMode = chunked;
    m_currentChunkSize = 0;
    
    if (isChunked != nullptr) {
        *isChunked = chunked;
    }
    
    // Для любого режима (chunked или обычный) оставляем соединение открытым
    // и НЕ читаем тело ответа, чтобы можно было читать потоково
    response.body = "";
    
    // Не закрываем клиент! Он остается открытым для потокового чтения
    // m_client.stop() не вызывается
    
    return response;
}

// Читает один чанк из стримингового ответа
bool SimpleHttpClient::readChunk(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) {
    bytesRead = 0;
    
    // Проверяем, что соединение активно и мы в chunked режиме
    if (!m_client.connected() || !m_chunkedMode) {
        return false;
    }
    
    // Если текущий чанк закончился, читаем следующий
    if (m_currentChunkSize == 0) {
        // Читаем размер чанка
        String chunkHeader = m_client.readStringUntil('\n');
        chunkHeader.trim();
        if (chunkHeader.isEmpty()) {
            return false;
        }
        
        // Парсим размер в hex
        m_currentChunkSize = strtol(chunkHeader.c_str(), NULL, 16);
        
        // Если чанк нулевого размера - конец данных
        if (m_currentChunkSize == 0) {
            // Пропускаем завершающий CRLF
            m_client.readStringUntil('\n');
            m_chunkedMode = false;
            return false;
        }
    }
    
    // Читаем данные чанка
    size_t toRead = min(bufferSize, m_currentChunkSize);
    bytesRead = m_client.readBytes(buffer, toRead);
    m_currentChunkSize -= bytesRead;
    
    // Если чанк полностью прочитан, пропускаем завершающий CRLF
    if (m_currentChunkSize == 0) {
        m_client.readStringUntil('\n');
    }
    
    return (bytesRead > 0);
}

// Читает данные из обычного (не chunked) ответа
bool SimpleHttpClient::readPlainData(uint8_t* buffer, size_t bufferSize, size_t& bytesRead) {
    bytesRead = 0;
    
    // Проверяем, что соединение активно и мы НЕ в chunked режиме
    if (!m_client.connected() || m_chunkedMode) {
        return false;
    }
    
    // Читаем доступные данные
    if (m_client.available()) {
        bytesRead = m_client.readBytes(buffer, bufferSize);
        return (bytesRead > 0);
    }
    
    return false;
}

// Проверяет, доступны ли данные для чтения
bool SimpleHttpClient::isDataAvailable() {
    return m_client.available() > 0;
}

// Проверяет, установлено ли соединение
bool SimpleHttpClient::isConnected() {
    return m_client.connected();
}

// Проверяет, находится ли клиент в chunked режиме
bool SimpleHttpClient::isChunkedMode() {
    return m_chunkedMode;
}

//*********************************************************************************************************************
// Вспомогательные методы
//*********************************************************************************************************************

// Читает ответ от сервера
void SimpleHttpClient::readResponse(HttpResponse& response) {
    unsigned long timeout = millis();
    while (!m_client.available()) {
        if (millis() - timeout > 5000) {
            m_client.stop();
            m_connected = false;
            return;
        }
        vTaskDelay(1);
    }
    String s = m_client.readString();
    parseHttpResponse(s, response);
    m_client.stop();
    m_connected = false;
}

// Разбирает HTTP ответ
bool SimpleHttpClient::parseHttpResponse(const String& Str, HttpResponse& response) {
    response.statusCode = -1;
    response.headers = "";
    response.body = "";
    
    int firstLineEnd = Str.indexOf("\r\n");
    if (firstLineEnd == -1) {
        return false;
    }
    
    String statusLine = Str.substring(0, firstLineEnd);
    int firstSpace = statusLine.indexOf(' ');
    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    if (firstSpace != -1 && secondSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1, secondSpace);
        response.statusCode = codeStr.toInt();
    } else if (firstSpace != -1) {
        String codeStr = statusLine.substring(firstSpace + 1);
        response.statusCode = codeStr.toInt();
    } else {
        return false;
    }
    
    int headersEnd = Str.indexOf("\r\n\r\n");
    if (headersEnd == -1) {
        headersEnd = Str.indexOf("\n\n");
        if (headersEnd == -1) {
            return false;
        }
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 2);
    } else {
        response.headers = Str.substring(firstLineEnd + 2, headersEnd);
        response.body = Str.substring(headersEnd + 4);
    }
    response.body.trim();
    return true;
}

// Публичный метод для разбора HTTP ответа из внешнего кода
bool SimpleHttpClient::parseHttpResponseShared(const String& Str, HttpResponse& response) {
    SimpleHttpClient client;
    return client.parseHttpResponse(Str, response);
}