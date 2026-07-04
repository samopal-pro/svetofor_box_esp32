/*
  Fork samopal.pro 03.07.2026
  RequestHandlerLite.h - Request handler base class
  
  Changes:
  - Added "lite" suffix to avoid conflicts with stock library
  - Removed all conditional features (AUTH, CORS, ETAG, CHUNKED, SSE, RAW, MIDDLEWARE, REGEX)
*/

#ifndef REQUESTHANDLERLITE_H
#define REQUESTHANDLERLITE_H

#include <vector>
#include <assert.h>
#include "../HTTP_Method_Lite.h"

class WebServerLite;

class RequestHandlerLite {
public:
  virtual ~RequestHandlerLite() {
  }

  virtual bool canHandle(HTTPMethodLite method, const String &uri) {
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(const String &uri) {
    (void)uri;
    return false;
  }

  virtual bool canHandle(WebServerLite &server, HTTPMethodLite method, const String &uri) {
    (void)server;
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canUpload(WebServerLite &server, const String &uri) {
    (void)server;
    (void)uri;
    return false;
  }
  virtual bool handle(WebServerLite &server, HTTPMethodLite requestMethod, const String &requestUri) {
    (void)server;
    (void)requestMethod;
    (void)requestUri;
    return false;
  }
  virtual void upload(WebServerLite &server, const String &requestUri, HTTPUploadLite &upload) {
    (void)server;
    (void)requestUri;
    (void)upload;
  }

  RequestHandlerLite *next() {
    return _next;
  }
  void next(RequestHandlerLite *r) {
    _next = r;
  }

  bool process(WebServerLite &server, HTTPMethodLite requestMethod, String requestUri);

private:
  RequestHandlerLite *_next = nullptr;

protected:
  std::vector<String> pathArgs;

public:
  const String &pathArg(unsigned int i) {
    assert(i < pathArgs.size());
    return pathArgs[i];
  }
};

#endif  //REQUESTHANDLERLITE_H