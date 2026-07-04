#ifndef URI_LITE_H
#define URI_LITE_H

#include <Arduino.h>
#include <vector>

class UriLite {

protected:
  const String _uri;

public:
  UriLite(const char *uri) : _uri(uri) {}
  UriLite(const String &uri) : _uri(uri) {}
  UriLite(const __FlashStringHelper *uri) : _uri((const char *)uri) {}
  virtual ~UriLite() {}

  virtual UriLite *clone() const {
    return new UriLite(_uri);
  };

  virtual void initPathArgs(__attribute__((unused)) std::vector<String> &pathArgs) {}

  virtual bool canHandle(const String &requestUri, __attribute__((unused)) std::vector<String> &pathArgs) {
    return _uri == requestUri;
  }

  const String &getUri() const {
    return _uri;
  }
};

#endif