#pragma once

#include <WiFi.h>

struct HttpResponse {
    int statusCode = -1;
    String headers;
    String body;
};

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

private:

    static void readResponse(
        WiFiClient& client,
        HttpResponse& response
    );

    static bool parseHttpResponse(const String& Str, HttpResponse& response);
};