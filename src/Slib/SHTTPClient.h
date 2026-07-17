// ============================================
// Файл: src/Slib/SHTTPClient.h
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
    
    bool GET_STREAM(const char* host, uint16_t port, const String& path, const String& extraHeaders, const uint32_t waitTM = 5000);

    // Выполняет GET запрос
    bool GET(const char* host, uint16_t port, const String& path, const String& extraHeaders = "", const uint32_t waitTM = 5000);
    
    // Выполняет POST запрос
    bool POST(const char* host, uint16_t port, const String& path, const String& contentType, const String& payload, const String& extraHeaders = "", const uint32_t waitTM = 5000);
    
    // Выполняет POST запрос с JSON данными
    bool POST_JSON(const char* host, uint16_t port, const String& path, const String& json, const String& extraHeaders = "", const uint32_t waitTM = 5000);
    
    // Выполняет POST запрос с текстовыми данными
    bool POST_TEXT(const char* host, uint16_t port, const String& path, const String& text, const String& extraHeaders = "", const uint32_t waitTM = 5000);
    
    // Выполняет POST запрос с потоковыми данными
//    HttpResponse POST_STREAM(const char* host, uint16_t port, const String& path, const String& contentType, Stream& payloadStream, size_t contentLength, const String& extraHeaders = "");
    
    // Устанавливает соединение и отправляет GET запрос
//    bool connectAndSendRequest(const char* host, uint16_t port, const String& path, const String& extraHeaders = "");
   
    // Публичный метод для разбора HTTP ответа из внешнего кода
//    static bool parseHttpResponseShared(const String& Str, HttpResponse& response);
    
    // Закрывает соединение
    void stop();
    
    // Ожидает ответ сервера tm миллисекунд
    bool waitResponse(uint32_t tm);
    
    // Читает код HTTP в m_response.statusCode
    bool readStatusCode();
    
    // Читает заголовки HTTP в m_response.headers
    bool readHeader();

    // Вернуть значение заголовка с заданным именем
    String getHeaderValue(const String& name) const;    
    
    // Читает тело HTTP в m_response.body
    bool readBody();
    
    HttpResponse m_response;  // Результат HTTP запроса
    WiFiClient m_client;      // Переиспользуемый клиент (публичный для стримингового доступа)

private:
    // Читает ответ от сервера
    void readResponse(HttpResponse& response);
    
    // Разбирает HTTP ответ
    bool parseHttpResponse(const String& Str, HttpResponse& response);
    
    // Сбрасывает m_response в начальное состояние
    void resetResponse();
    
    bool m_connected;
    bool m_chunkedMode;
    size_t m_currentChunkSize;
};