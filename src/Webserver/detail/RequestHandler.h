/*
  Fork samopal.pro 03.07.2026
  RequestHandler.h - Request handler base class
  
  Changes:
  - Removed middleware/Middleware.h include
  - Removed MiddlewareChain member
  - Conditional Raw support
*/

#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#include <vector>
#include <assert.h>

class RequestHandler {
public:
  virtual ~RequestHandler() {
  }

  virtual bool canHandle(HTTPMethod method, const String &uri) {
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(const String &uri) {
    (void)uri;
    return false;
  }
  #ifdef WEBSERVER_INCLUDE_RAW
  virtual bool canRaw(const String &uri) {
    (void)uri;
    return false;
  }
  #endif

  virtual bool canHandle(WebServer &server, HTTPMethod method, const String &uri) {
    (void)server;
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(WebServer &server, const String &uri) {
    (void)server;
    (void)uri;
    return false;
  }
  #ifdef WEBSERVER_INCLUDE_RAW
  virtual bool canRaw(WebServer &server, const String &uri) {
    (void)server;
    (void)uri;
    return false;
  }
  #endif
  virtual bool handle(WebServer &server, HTTPMethod requestMethod, const String &requestUri) {
    (void)server;
    (void)requestMethod;
    (void)requestUri;
    return false;
  }
  virtual void upload(WebServer &server, const String &requestUri, HTTPUpload &upload) {
    (void)server;
    (void)requestUri;
    (void)upload;
  }
  #ifdef WEBSERVER_INCLUDE_RAW
  virtual void raw(WebServer &server, const String &requestUri, HTTPRaw &raw) {
    (void)server;
    (void)requestUri;
    (void)raw;
  }
  #endif

  #ifdef WEBSERVER_INCLUDE_REGEX
  virtual RequestHandler &setFilter(std::function<bool(WebServer &)> filter) {
    (void)filter;
    return *this;
  }
  #endif

  RequestHandler *next() {
    return _next;
  }
  void next(RequestHandler *r) {
    _next = r;
  }

  bool process(WebServer &server, HTTPMethod requestMethod, String requestUri);

private:
  RequestHandler *_next = nullptr;

protected:
  std::vector<String> pathArgs;

public:
  const String &pathArg(unsigned int i) {
    assert(i < pathArgs.size());
    return pathArgs[i];
  }
};

#endif  //REQUESTHANDLER_H