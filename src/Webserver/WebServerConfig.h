/*
  Fork samopal.pro 03.07.2026
  WebServerConfig.h - Centralized configuration for WebServer feature flags
*/

#ifndef WEBSERVERCONFIG_H
#define WEBSERVERCONFIG_H

// ===== ИСКЛЮЧЕНИЕ НЕИСПОЛЬЗУЕМОГО ФУНКЦИОНАЛА =====
// Экономия ~30-35 KB Flash памяти
#define WEBSERVER_NO_AUTH        // Без аутентификации (BASIC/DIGEST)
#define WEBSERVER_NO_CORS        // Без CORS заголовков
#define WEBSERVER_NO_ETAG        // Без ETag кеширования
#define WEBSERVER_NO_CHUNKED     // Без chunked encoding
#define WEBSERVER_NO_SSE         // Без Server-Sent Events
#define WEBSERVER_NO_RAW         // Без raw обработки запросов
#define WEBSERVER_NO_MIDDLEWARE  // Без middleware цепочки
#define WEBSERVER_NO_REGEX       // Без UriBraces/UriGlob/UriRegex (SimpleRequestHandler используется)

#define IS_SET_ACTIVITY

#endif // WEBSERVERCONFIG_H