// ============================================
// Файл: src/Slib/SHTTPClient.h (дополненный)
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
 */
class SimpleHttpClient {
public:
    static HttpResponse GET(
        const char* host,
        uint16_t port,
        const String& path,
        const String& extraHeaders = ""
    );

    static HttpResponse POST(
        const char* host,
        uint16_t port,
        const String& path,
        const String& contentType,
        const String& payload,
        const String& extraHeaders = ""
    );

    static HttpResponse POST_JSON(
        const char* host,
        uint16_t port,
        const String& path,
        const String& json,
        const String& extraHeaders = ""
    );

    static HttpResponse POST_TEXT(
        const char* host,
        uint16_t port,
        const String& path,
        const String& text,
        const String& extraHeaders = ""
    );

    static HttpResponse POST_STREAM(
        const char* host,
        uint16_t port,
        const String& path,
        const String& contentType,
        Stream& payloadStream,
        size_t contentLength,
        const String& extraHeaders = ""
    );

    static bool connectAndSendRequest(
        WiFiClient& client,
        const char* host,
        uint16_t port,
        const String& path,
        const String& extraHeaders = ""
    ); // Устанавливает соединение и отправляет GET запрос
   
    static HttpResponse GET_STREAM(
        const char* host,
        uint16_t port,
        const String& path,
        const String& extraHeaders = "",
        bool* isChunked = nullptr
    ); // Выполняет GET запрос с возможностью стримингового чтения
    
    /**
     * Публичный метод для разбора HTTP ответа из внешнего кода.
     * Используется при потоковой отправке данных.
     */
    static bool parseHttpResponseShared(const String& Str, HttpResponse& response);

private:
    static void readResponse(
        WiFiClient& client,
        HttpResponse& response
    );

    static bool parseHttpResponse(const String& Str, HttpResponse& response);
};