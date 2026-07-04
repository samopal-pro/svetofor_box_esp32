/*
  Fork samopal.pro 03.07.2026
  WebServerLite.cpp - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Original Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
  
  Changes:
  - Added "lite" suffix to avoid conflicts with stock library
  - Removed all conditional features (AUTH, CORS, ETAG, CHUNKED, SSE, RAW, MIDDLEWARE, REGEX)
  - Removed log_v debug output
*/
#include <Arduino.h>
#include <esp32-hal-log.h>

#include "esp_random.h"
#include "NetworkServer.h"
#include "NetworkClient.h"
#include "WebServerLite.h"
#include "FS.h"
#include "detail/RequestHandlersImplLite.h"

static const char Content_Length[] = "Content-Length";

WebServerLite::WebServerLite(IPAddress addr, int port) : _server(addr, port) {
}

WebServerLite::WebServerLite(int port) : _server(port) {
}

WebServerLite::~WebServerLite() {
  _server.close();
  _clearRequestHeaders();
  _clearResponseHeaders();

  RequestHandlerLite *handler = _firstHandler;
  while (handler) {
    RequestHandlerLite *next = handler->next();
    delete handler;
    handler = next;
  }
  _firstHandler = nullptr;
}

void WebServerLite::begin() {
  close();
  _server.begin();
  _server.setNoDelay(true);

  _lastRequestTime = 0;

}

void WebServerLite::begin(uint16_t port) {
  close();
  _server.begin(port);
  _server.setNoDelay(true);

  _lastRequestTime = 0;

}

RequestHandlerLite &WebServerLite::on(const UriLite &uri, WebServerLite::THandlerFunction handler) {
  return on(uri, HTTP_ANY_LITE, handler);
}

RequestHandlerLite &WebServerLite::on(const UriLite &uri, HTTPMethodLite method, WebServerLite::THandlerFunction fn) {
  return on(uri, method, fn, _fileUploadHandler);
}

RequestHandlerLite &WebServerLite::on(const UriLite &uri, HTTPMethodLite method, WebServerLite::THandlerFunction fn, WebServerLite::THandlerFunction ufn) {
  SimpleRequestHandlerLite *handler = new SimpleRequestHandlerLite(fn, ufn, uri.getUri(), method);
  _addRequestHandler(handler);
  return *handler;
}

void WebServerLite::addHandler(RequestHandlerLite *handler) {
  _addRequestHandler(handler);
}

bool WebServerLite::removeHandler(RequestHandlerLite *handler) {
  return _removeRequestHandler(handler);
}

void WebServerLite::_addRequestHandler(RequestHandlerLite *handler) {
  if (!_lastHandler) {
    _firstHandler = handler;
    _lastHandler = handler;
  } else {
    _lastHandler->next(handler);
    _lastHandler = handler;
  }
}

bool WebServerLite::_removeRequestHandler(RequestHandlerLite *handler) {
  RequestHandlerLite *current = _firstHandler;
  RequestHandlerLite *previous = nullptr;

  while (current != nullptr) {
    if (current == handler) {
      if (previous == nullptr) {
        _firstHandler = current->next();
      } else {
        previous->next(current->next());
      }

      if (current == _lastHandler) {
        _lastHandler = previous;
      }

      delete current;
      return true;
    }
    previous = current;
    current = current->next();
  }
  return false;
}

void WebServerLite::serveStatic(const char *uri, FS &fs, const char *path, const char *cache_header) {
  _addRequestHandler(new StaticRequestHandlerLite(fs, path, uri, cache_header));
}

void WebServerLite::handleClient() {
  if (_currentStatus == HC_NONE) {
    _currentClient = _server.accept();
    if (!_currentClient) {
      if (_nullDelay) {
        delay(1);
      }
      return;
    }

    _updateActivity();


    _currentStatus = HC_WAIT_READ;
    _statusChange = millis();
  }

  bool keepCurrentClient = false;
  bool callYield = false;

  if (_currentClient.connected()) {
    switch (_currentStatus) {
      case HC_NONE:
        break;
      case HC_WAIT_READ:
        if (_currentClient.available()) {
         
          _updateActivity();

          _currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
          if (_parseRequest(_currentClient)) {
            _contentLength = CONTENT_LENGTH_NOT_SET;
            _responseCode = 0;
            _clearResponseHeaders();

            _handleRequest();
          }
        } else {
          if (millis() - _statusChange <= HTTP_MAX_DATA_WAIT) {
            keepCurrentClient = true;
          }
          callYield = true;
        }
        break;
      case HC_WAIT_CLOSE:
        if (millis() - _statusChange <= HTTP_MAX_CLOSE_WAIT) {
          keepCurrentClient = true;
          callYield = true;
        }
    }
  }

  if (!keepCurrentClient) {
    _currentClient = NetworkClient();
    _currentStatus = HC_NONE;
    _currentUpload.reset();
  }

  if (callYield) {
    yield();
  }
}

void WebServerLite::close() {
  _server.close();
  _currentStatus = HC_NONE;
  if (!_headerKeysCount) {
    collectHeaders(0, 0);
  }
}

void WebServerLite::stop() {
  close();
}

void WebServerLite::sendHeader(const String &name, const String &value, bool first) {
  if (name.indexOf('\r') != -1 || name.indexOf('\n') != -1) {
    return;
  }

  if (value.indexOf('\r') != -1 || value.indexOf('\n') != -1) {
    return;
  }

  RequestArgumentLite *header = new RequestArgumentLite();
  header->key = name;
  header->value = value;

  if (!_responseHeaders || first) {
    header->next = _responseHeaders;
    _responseHeaders = header;
  } else {
    RequestArgumentLite *last = _responseHeaders;
    while (last->next) {
      last = last->next;
    }
    last->next = header;
  }

  _responseHeaderCount++;
}

void WebServerLite::setContentLength(const size_t contentLength) {
  _contentLength = contentLength;
}

void WebServerLite::enableDelay(boolean value) {
  _nullDelay = value;
}

void WebServerLite::_prepareHeader(String &response, int code, const char *content_type, size_t contentLength) {
  _responseCode = code;

  response.concat(version());
  response.concat(' ');
  response.concat(String(code));
  response.concat(' ');
  response.concat(responseCodeToString(code));
  response.concat(F("\r\n"));

  using namespace mime;
  if (!content_type) {
    content_type = mimeTable[html].mimeType;
  }

  sendHeader(String(F("Content-Type")), String(FPSTR(content_type)), true);
  if (_contentLength == CONTENT_LENGTH_NOT_SET) {
    sendHeader(String(FPSTR(Content_Length)), String(contentLength));
  }
  else if (_contentLength != CONTENT_LENGTH_UNKNOWN) {
    sendHeader(String(FPSTR(Content_Length)), String(_contentLength));
  }

  sendHeader(String(F("Connection")), String(F("close")));

  for (RequestArgumentLite *header = _responseHeaders; header; header = header->next) {
    response.concat(header->key);
    response.concat(F(": "));
    response.concat(header->value);
    response.concat(F("\r\n"));
  }

  response.concat(F("\r\n"));
}

void WebServerLite::send(int code, const char *content_type, const String &content) {
  String header;
  _prepareHeader(header, code, content_type, content.length());
  _currentClientWrite(header.c_str(), header.length());
  if (content.length()) {
    sendContent(content);
  }
}

void WebServerLite::send(int code, char *content_type, const String &content) {
  send(code, (const char *)content_type, content);
}

void WebServerLite::send(int code, const String &content_type, const String &content) {
  send(code, (const char *)content_type.c_str(), content);
}

void WebServerLite::send(int code, const char *content_type, const char *content) {
  const String passStr = (String)content;
  if (strlen(content) != passStr.length()) {
    // String cast failed
  }
  send(code, content_type, passStr);
}

void WebServerLite::send(int code, const char *content_type, Stream &stream, size_t content_length) {
  if (!content_length) {
    content_length = stream.available();
    if (!content_length) {
      send(204);
      return;
    }
  }
  String header;
  _prepareHeader(header, code, content_type, content_length);
  _currentClientWrite(header.c_str(), header.length());
  _currentClient.write(stream, content_length);
}

void WebServerLite::send_P(int code, PGM_P content_type, PGM_P content) {
  size_t contentLength = 0;

  if (content != NULL) {
    contentLength = strlen_P(content);
  }

  String header;
  char type[64];
  memccpy_P((void *)type, (PGM_VOID_P)content_type, 0, sizeof(type));
  _prepareHeader(header, code, (const char *)type, contentLength);
  _currentClientWrite(header.c_str(), header.length());
  sendContent_P(content);
}

void WebServerLite::send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength) {
  String header;
  char type[64];
  memccpy_P((void *)type, (PGM_VOID_P)content_type, 0, sizeof(type));
  _prepareHeader(header, code, (const char *)type, contentLength);
  sendContent(header);
  sendContent_P(content, contentLength);
}

void WebServerLite::sendContent(const String &content) {
  sendContent(content.c_str(), content.length());
}

void WebServerLite::sendContent(const char *content, size_t contentLength) {
  _currentClientWrite(content, contentLength);
}

void WebServerLite::sendContent_P(PGM_P content) {
  sendContent_P(content, strlen_P(content));
}

void WebServerLite::sendContent_P(PGM_P content, size_t size) {
  _currentClientWrite_P(content, size);
}

void WebServerLite::_streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code) {
  using namespace mime;
  setContentLength(fileSize);
  if (fileName.endsWith(String(FPSTR(mimeTable[gz].endsWith))) && contentType != String(FPSTR(mimeTable[gz].mimeType))
      && contentType != String(FPSTR(mimeTable[none].mimeType))) {
    sendHeader(F("Content-Encoding"), F("gzip"));
  }
  send(code, contentType, "");
  setContentLength(CONTENT_LENGTH_NOT_SET);
}

String WebServerLite::pathArg(unsigned int i) const {
  if (_currentHandler != nullptr) {
    return _currentHandler->pathArg(i);
  }
  return "";
}

String WebServerLite::arg(const String &name) const {
  for (int j = 0; j < _postArgsLen; ++j) {
    if (_postArgs[j].key == name) {
      return _postArgs[j].value;
    }
  }
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name) {
      return _currentArgs[i].value;
    }
  }
  return "";
}

String WebServerLite::arg(int i) const {
  if (i < _currentArgCount) {
    return _currentArgs[i].value;
  }
  return "";
}

String WebServerLite::argName(int i) const {
  if (i < _currentArgCount) {
    return _currentArgs[i].key;
  }
  return "";
}

int WebServerLite::args() const {
  return _currentArgCount;
}

bool WebServerLite::hasArg(const String &name) const {
  for (int j = 0; j < _postArgsLen; ++j) {
    if (_postArgs[j].key == name) {
      return true;
    }
  }
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name) {
      return true;
    }
  }
  return false;
}

String WebServerLite::header(const String &name) const {
  for (RequestArgumentLite *current = _currentHeaders; current; current = current->next) {
    if (current->key.equalsIgnoreCase(name)) {
      return current->value;
    }
  }
  return "";
}

void WebServerLite::collectHeaders(const char *headerKeys[], const size_t headerKeysCount) {
  collectAllHeaders();
  _collectAllHeaders = false;

  _headerKeysCount += headerKeysCount;

  RequestArgumentLite *last = _currentHeaders->next;

  for (int i = 2; i < _headerKeysCount; i++) {
    last->next = new RequestArgumentLite();
    last->next->key = headerKeys[i - 2];
    last = last->next;
  }
}

String WebServerLite::header(int i) const {
  RequestArgumentLite *current = _currentHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->value : emptyString;
}

String WebServerLite::headerName(int i) const {
  RequestArgumentLite *current = _currentHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->key : emptyString;
}

int WebServerLite::headers() const {
  return _headerKeysCount;
}

bool WebServerLite::hasHeader(const String &name) const {
  return header(name).length() > 0;
}

String WebServerLite::hostHeader() const {
  return _hostHeader;
}

void WebServerLite::onFileUpload(THandlerFunction fn) {
  _fileUploadHandler = fn;
}

void WebServerLite::onNotFound(THandlerFunction fn) {
  _notFoundHandler = fn;
}

bool WebServerLite::_handleRequest() {
  bool handled = false;
  if (_currentHandler) {
    handled = _currentHandler->process(*this, _currentMethod, _currentUri);
    if (!handled) {
      // request handler failed to handle request
    }
  }
  if (!handled && _notFoundHandler) {
    _notFoundHandler();
    handled = true;
  }
  if (!handled) {
    using namespace mime;
    send(404, String(FPSTR(mimeTable[html].mimeType)), String(F("Not found: ")) + _currentUri);
    handled = true;
  }
  if (handled) {
    _finalizeResponse();
  }
  _currentUri = "";
  return handled;
}

void WebServerLite::_finalizeResponse() {
}

String WebServerLite::responseCodeToString(int code) {
  switch (code) {
    case 100: return F("Continue");
    case 101: return F("Switching Protocols");
    case 200: return F("OK");
    case 201: return F("Created");
    case 202: return F("Accepted");
    case 203: return F("Non-Authoritative Information");
    case 204: return F("No Content");
    case 205: return F("Reset Content");
    case 206: return F("Partial Content");
    case 300: return F("Multiple Choices");
    case 301: return F("Moved Permanently");
    case 302: return F("Found");
    case 303: return F("See Other");
    case 304: return F("Not Modified");
    case 305: return F("Use Proxy");
    case 307: return F("Temporary Redirect");
    case 400: return F("Bad Request");
    case 401: return F("Unauthorized");
    case 402: return F("Payment Required");
    case 403: return F("Forbidden");
    case 404: return F("Not Found");
    case 405: return F("Method Not Allowed");
    case 406: return F("Not Acceptable");
    case 407: return F("Proxy Authentication Required");
    case 408: return F("Request Time-out");
    case 409: return F("Conflict");
    case 410: return F("Gone");
    case 411: return F("Length Required");
    case 412: return F("Precondition Failed");
    case 413: return F("Request Entity Too Large");
    case 414: return F("Request-URI Too Large");
    case 415: return F("Unsupported Media Type");
    case 416: return F("Requested range not satisfiable");
    case 417: return F("Expectation Failed");
    case 500: return F("Internal Server Error");
    case 501: return F("Not Implemented");
    case 502: return F("Bad Gateway");
    case 503: return F("Service Unavailable");
    case 504: return F("Gateway Time-out");
    case 505: return F("HTTP Version not supported");
    default:  return F("");
  }
}

void WebServerLite::_clearResponseHeaders() {
  _responseHeaderCount = 0;
  RequestArgumentLite *current = _responseHeaders;
  while (current) {
    RequestArgumentLite *next = current->next;
    delete current;
    current = next;
  }
  _responseHeaders = nullptr;
}

void WebServerLite::_clearRequestHeaders() {
  _headerKeysCount = 0;
  RequestArgumentLite *current = _currentHeaders;
  while (current) {
    RequestArgumentLite *next = current->next;
    delete current;
    current = next;
  }
  _currentHeaders = nullptr;
}

void WebServerLite::collectAllHeaders() {
  _clearRequestHeaders();

  _currentHeaders = new RequestArgumentLite();
  _currentHeaders->key = F("User-Agent");
  
  _currentHeaders->next = new RequestArgumentLite();
  _currentHeaders->next->key = F("Cookie");

  _headerKeysCount = 2;
  _collectAllHeaders = true;
}

const String &WebServerLite::responseHeader(String name) const {
  for (RequestArgumentLite *current = _responseHeaders; current; current = current->next) {
    if (current->key.equalsIgnoreCase(name)) {
      return current->value;
    }
  }
  return emptyString;
}

const String &WebServerLite::responseHeader(int i) const {
  RequestArgumentLite *current = _responseHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->value : emptyString;
}

const String &WebServerLite::responseHeaderName(int i) const {
  RequestArgumentLite *current = _responseHeaders;
  while (current && i--) {
    current = current->next;
  }
  return current ? current->key : emptyString;
}

bool WebServerLite::hasResponseHeader(const String &name) const {
  return header(name).length() > 0;
}

int WebServerLite::clientContentLength() const {
  return _clientContentLength;
}

const String WebServerLite::version() const {
  String v;
  v.reserve(8);
  v.concat(F("HTTP/1."));
  v.concat(_currentVersion);
  return v;
}

int WebServerLite::responseCode() const {
  return _responseCode;
}

int WebServerLite::responseHeaders() const {
  return _responseHeaderCount;
}

uint32_t WebServerLite::getLastRequestTime() const {
    return _lastRequestTime;
}
    
uint32_t WebServerLite::getIdleTime() const {
      return millis() - _lastRequestTime;
}

void WebServerLite::_updateActivity() { 
    _lastRequestTime = millis(); 
}
