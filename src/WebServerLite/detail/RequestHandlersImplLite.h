/*
  Fork samopal.pro 03.07.2026
  RequestHandlersImplLite.h - Request handler implementations
  
  Changes:
  - Added "lite" suffix to avoid conflicts with stock library
  - Removed all conditional features (AUTH, CORS, ETAG, CHUNKED, SSE, RAW, MIDDLEWARE, REGEX)
*/
#ifndef REQUESTHANDLERSIMPLITE_H
#define REQUESTHANDLERSIMPLITE_H

#include "RequestHandlerLite.h"
#include "mimetable.h"
#include "WString.h"
#include "../HTTP_Method_Lite.h"

using namespace mime;

bool RequestHandlerLite::process(WebServerLite &server, HTTPMethodLite requestMethod, String requestUri) {
  return handle(server, requestMethod, requestUri);
}

class SimpleRequestHandlerLite : public RequestHandlerLite {
public:
  SimpleRequestHandlerLite(WebServerLite::THandlerFunction fn, WebServerLite::THandlerFunction ufn, 
                       const String &uri, HTTPMethodLite method)
    : _fn(fn), _ufn(ufn), _uri(uri), _method(method) {}

  bool canHandle(HTTPMethodLite requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY_LITE && _method != requestMethod) return false;
    return _uri == requestUri;
  }

  bool canHandle(WebServerLite &server, HTTPMethodLite requestMethod, const String &requestUri) override {
    (void)server;
    if (_method != HTTP_ANY_LITE && _method != requestMethod) return false;
    return _uri == requestUri;
  }

  bool canUpload(const String &requestUri) override {
    if (!_ufn || !canHandle(HTTP_POST, requestUri)) return false;
    return true;
  }

  bool canUpload(WebServerLite &server, const String &requestUri) override {
    if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) return false;
    return true;
  }

  bool handle(WebServerLite &server, HTTPMethodLite requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) return false;
    _fn();
    return true;
  }

  void upload(WebServerLite &server, const String &requestUri, HTTPUploadLite &upload) override {
    (void)upload;
    if (canUpload(server, requestUri)) _ufn();
  }

protected:
  WebServerLite::THandlerFunction _fn;
  WebServerLite::THandlerFunction _ufn;
  String _uri;
  HTTPMethodLite _method;
};

class StaticRequestHandlerLite : public RequestHandlerLite {
public:
  StaticRequestHandlerLite(FS &fs, const char *path, const char *uri, const char *cache_header) 
    : _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
    File f = fs.open(path);
    _isFile = (f && (!f.isDirectory()));
    _baseUriLength = _uri.length();
  }

  bool canHandle(HTTPMethodLite requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) return false;
    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) return false;
    return true;
  }

  bool canHandle(WebServerLite &server, HTTPMethodLite requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) return false;
    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) return false;
    return true;
  }

  bool handle(WebServerLite &server, HTTPMethodLite requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) return false;

    String path(_path);
    if (!_isFile) {
      if (requestUri.endsWith("/")) {
        return handle(server, requestMethod, String(requestUri + "index.htm"));
      }
      path += requestUri.substring(_baseUriLength);
    }

    String contentType = getContentType(path);

    if (!path.endsWith(FPSTR(mimeTable[gz].endsWith)) && !_fs.exists(path)) {
      String pathWithGz = path + FPSTR(mimeTable[gz].endsWith);
      if (_fs.exists(pathWithGz)) path += FPSTR(mimeTable[gz].endsWith);
    }

    File f = _fs.open(path, "r");
    if (!f || !f.available()) return false;

    if (_cache_header.length() != 0) {
      server.sendHeader("Cache-Control", _cache_header);
    }

    server.streamFile(f, contentType);
    return true;
  }

  static String getContentType(const String &path) {
    char buff[sizeof(mimeTable[0].mimeType)];
    for (size_t i = 0; i < sizeof(mimeTable) / sizeof(mimeTable[0]) - 1; i++) {
      strcpy_P(buff, mimeTable[i].endsWith);
      if (path.endsWith(buff)) {
        strcpy_P(buff, mimeTable[i].mimeType);
        return String(buff);
      }
    }
    strcpy_P(buff, mimeTable[sizeof(mimeTable) / sizeof(mimeTable[0]) - 1].mimeType);
    return String(buff);
  }

protected:
  FS _fs;
  String _uri;
  String _path;
  String _cache_header;
  bool _isFile;
  size_t _baseUriLength;
};

#endif  //REQUESTHANDLERSIMPLITE_H