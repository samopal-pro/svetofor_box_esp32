// ============================================
// Файл: src/Slib/SHTTPClient.h (исправленный)
// ============================================
#pragma once

#include <WiFi.h>

/**
 * Структура для хранения результата HTTP запроса.
 */
struct HttpResponse {
    int statusCode = -1;  // HTTP код ответа (-1 если ошибка соединения)
    String headers;        // Заголовки ответа
    String body;           // Тело ответа
};

/**
 * Простой HTTP клиент для отправки запросов.
 * Использует WiFiClient вместо тяжелого HTTPClient.
 * WiFiClient является свойством класса для переиспользования.
 */
class SimpleHttpClient {
public:
    // Конструктор
    SimpleHttpClient();
    
    // Деструктор
    ~SimpleHttpClient();
    
    // Инициализация клиента
    bool begin();
    
    // Выполняет GET запрос
    HttpResponse GET(const char* host, uint16_t port, const String& path, const String& extraHeaders = "");
    
    // Выполняет POST запрос
    HttpResponse POST(const char* host, uint16_t port, const String& path, const String& contentType, const String& payload, const String& extraHeaders = "");
    
    // Выполняет POST запрос с JSON данными
    HttpResponse POST_JSON(const char* host, uint16_t port, const String& path, const String& json, const String& extraHeaders = "");
    
    // Выполняет POST запрос с текстовыми данными
    HttpResponse POST_TEXT(const char* host, uint16_t port, const String& path, const String& text, const String& extraHeaders = "");
    
    // Выполняет POST запрос с потоковыми данными
    HttpResponse POST_STREAM(const char* host, uint16_t port, const String& path, const String& contentType, Stream& payloadStream, size_t contentLength, const String& extraHeaders = "");
    
    // Устанавливает соединение и отправляет GET запрос
    bool connectAndSendRequest(const char* host, uint16_t port, const String& path, const String& extraHeaders = "");
   
    // Выполняет GET запрос с возможностью стримингового чтения
    HttpResponse GET_STREAM(const char* host, uint16_t port, const String& path, const String& extraHeaders = "", bool* isChunked = nullptr);
    
    // Читает один чанк из стримингового ответа (для chunked режима)
    bool readChunk(uint8_t* buffer, size_t bufferSize, size_t& bytesRead);
    
    // Читает данные из обычного (не chunked) ответа
    bool readPlainData(uint8_t* buffer, size_t bufferSize, size_t& bytesRead);
    
    // Проверяет, доступны ли данные для чтения
    bool isDataAvailable();
    
    // Проверяет, установлено ли соединение
    bool isConnected();
    
    // Проверяет, находится ли клиент в chunked режиме
    bool isChunkedMode();
    
    // Публичный метод для разбора HTTP ответа из внешнего кода
    static bool parseHttpResponseShared(const String& Str, HttpResponse& response);
    
    // Закрывает соединение
    void stop();
    
    WiFiClient m_client;  // Переиспользуемый клиент (публичный для стримингового доступа)

private:
    // Читает ответ от сервера
    void readResponse(HttpResponse& response);
    
    // Разбирает HTTP ответ
    bool parseHttpResponse(const String& Str, HttpResponse& response);
    
    bool m_connected;
    bool m_chunkedMode;
    size_t m_currentChunkSize;
};