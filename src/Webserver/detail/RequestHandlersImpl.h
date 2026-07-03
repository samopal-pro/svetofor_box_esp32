/*
  Fork samopal.pro 03.07.2026
  RequestHandlersImpl.h - Request handler implementations
  
  Changes:
  - Removed Middleware methods
  - SimpleRequestHandler always available
  - Conditional ETag/Raw/Regex support
*/
#include "../WebServerConfig.h"

#ifndef REQUESTHANDLERSIMPL_H
#define REQUESTHANDLERSIMPL_H

#include "RequestHandler.h"
#include "mimetable.h"
#include "WString.h"

#ifdef WEBSERVER_INCLUDE_REGEX
  #include "Uri.h"
#endif

#ifdef WEBSERVER_INCLUDE_ETAG
  #include <MD5Builder.h>
  #include <base64.h>
#endif

using namespace mime;

bool RequestHandler::process(WebServer &server, HTTPMethod requestMethod, String requestUri) {
  return handle(server, requestMethod, requestUri);
}

// ===== FUNCTION REQUEST HANDLER (with URI pattern matching) =====
#ifdef WEBSERVER_INCLUDE_REGEX

class FunctionRequestHandler : public RequestHandler {
public:
  FunctionRequestHandler(WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn, const Uri &uri, HTTPMethod method)
    : _fn(fn), _ufn(ufn), _uri(uri.clone()), _method(method) {
    _uri->initPathArgs(pathArgs);
  }

  ~FunctionRequestHandler() {
    delete _uri;
  }

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) return false;
    return _uri->canHandle(requestUri, pathArgs);
  }

  bool canUpload(const String &requestUri) override {
    if (!_ufn || !canHandle(HTTP_POST, requestUri)) return false;
    return true;
  }

  #ifdef WEBSERVER_INCLUDE_RAW
  bool canRaw(const String &requestUri) override {
    (void)requestUri;
    if (!_ufn || _method == HTTP_GET) return false;
    return true;
  }
  #endif

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) return false;
    return _uri->canHandle(requestUri, pathArgs) && (_filter != NULL ? _filter(server) : true);
  }

  bool canUpload(WebServer &server, const String &requestUri) override {
    if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) return false;
    return true;
  }

  #ifdef WEBSERVER_INCLUDE_RAW
  bool canRaw(WebServer &server, const String &requestUri) override {
    (void)requestUri;
    if (!_ufn || _method == HTTP_GET || (_filter != NULL ? _filter(server) == false : false)) return false;
    return true;
  }
  #endif

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) return false;
    _fn();
    return true;
  }

  void upload(WebServer &server, const String &requestUri, HTTPUpload &upload) override {
    (void)upload;
    if (canUpload(server, requestUri)) _ufn();
  }

  #ifdef WEBSERVER_INCLUDE_RAW
  void raw(WebServer &server, const String &requestUri, HTTPRaw &raw) override {
    (void)raw;
    if (canRaw(server, requestUri)) _ufn();
  }
  #endif

  FunctionRequestHandler &setFilter(WebServer::FilterFunction filter) {
    _filter = filter;
    return *this;
  }

protected:
  WebServer::THandlerFunction _fn;
  WebServer::THandlerFunction _ufn;
  WebServer::FilterFunction _filter;
  Uri *_uri;
  HTTPMethod _method;
};

#endif // WEBSERVER_INCLUDE_REGEX

// ===== SIMPLE REQUEST HANDLER (exact string matching) =====
class SimpleRequestHandler : public RequestHandler {
public:
  SimpleRequestHandler(WebServer::THandlerFunction fn, WebServer::THandlerFunction ufn, 
                       const String &uri, HTTPMethod method)
    : _fn(fn), _ufn(ufn), _uri(uri), _method(method) {}

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (_method != HTTP_ANY && _method != requestMethod) return false;
    return _uri == requestUri;
  }

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    (void)server;
    if (_method != HTTP_ANY && _method != requestMethod) return false;
    return _uri == requestUri;
  }

  bool canUpload(const String &requestUri) override {
    if (!_ufn || !canHandle(HTTP_POST, requestUri)) return false;
    return true;
  }

  bool canUpload(WebServer &server, const String &requestUri) override {
    if (!_ufn || !canHandle(server, HTTP_POST, requestUri)) return false;
    return true;
  }

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (!canHandle(server, requestMethod, requestUri)) return false;
    _fn();
    return true;
  }

  void upload(WebServer &server, const String &requestUri, HTTPUpload &upload) override {
    (void)upload;
    if (canUpload(server, requestUri)) _ufn();
  }

protected:
  WebServer::THandlerFunction _fn;
  WebServer::THandlerFunction _ufn;
  String _uri;
  HTTPMethod _method;
};

// ===== STATIC REQUEST HANDLER =====
class StaticRequestHandler : public RequestHandler {
public:
  StaticRequestHandler(FS &fs, const char *path, const char *uri, const char *cache_header) 
    : _fs(fs), _uri(uri), _path(path), _cache_header(cache_header) {
    File f = fs.open(path);
    _isFile = (f && (!f.isDirectory()));
    _baseUriLength = _uri.length();
  }

  bool canHandle(HTTPMethod requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) return false;
    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) return false;
    return true;
  }

  bool canHandle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
    if (requestMethod != HTTP_GET) return false;
    if ((_isFile && requestUri != _uri) || !requestUri.startsWith(_uri)) return false;
    #ifdef WEBSERVER_INCLUDE_REGEX
      if (_filter != NULL ? _filter(server) == false : false) return false;
    #endif
    return true;
  }

  bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) override {
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

    #ifdef WEBSERVER_INCLUDE_ETAG
    String eTagCode;
    if (server._eTagEnabled) {
      if (server._eTagFunction) {
        eTagCode = (server._eTagFunction)(_fs, path);
      } else {
        eTagCode = calcETag(_fs, path);
      }
      if (server.header("If-None-Match") == eTagCode) {
        server.send(304);
        return true;
      }
    }
    #endif

    if (_cache_header.length() != 0) {
      server.sendHeader("Cache-Control", _cache_header);
    }

    #ifdef WEBSERVER_INCLUDE_ETAG
    if ((server._eTagEnabled) && (eTagCode.length() > 0)) {
      server.sendHeader("ETag", eTagCode);
    }
    #endif

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

  #ifdef WEBSERVER_INCLUDE_ETAG
  static String calcETag(FS &fs, const String &path) {
    String result;
    uint8_t md5_buf[16];
    File f = fs.open(path, "r");
    MD5Builder calcMD5;
    calcMD5.begin();
    calcMD5.addStream(f, f.size());
    calcMD5.calculate();
    calcMD5.getBytes(md5_buf);
    f.close();
    result = "\"" + base64::encode(md5_buf, 16) + "\"";
    return result;
  }
  #endif

  #ifdef WEBSERVER_INCLUDE_REGEX
  StaticRequestHandler &setFilter(WebServer::FilterFunction filter) {
    _filter = filter;
    return *this;
  }
  #endif

protected:
  #ifdef WEBSERVER_INCLUDE_REGEX
    WebServer::FilterFunction _filter;
  #endif
  FS _fs;
  String _uri;
  String _path;
  String _cache_header;
  bool _isFile;
  size_t _baseUriLength;
};

#endif  //REQUESTHANDLERSIMPL_H