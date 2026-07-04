/*
  Fork samopal.pro 03.07.2026
  WebServerLite.h - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Original Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  
  Changes:
  - Added "lite" suffix to avoid conflicts with stock library
  - Removed all conditional features (AUTH, CORS, ETAG, CHUNKED, SSE, RAW, MIDDLEWARE, REGEX)
  - Removed log_v debug output
  - Added convenience overloads for const char* routes
*/

#ifndef WEBSERVERLITE_H
#define WEBSERVERLITE_H

#include <functional>
#include <memory>
#include "FS.h"
#include "Network.h"
#include "HTTP_Method_Lite.h"
#include "UriLite.h"

enum HTTPUploadStatusLite {
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED
};

enum HTTPClientStatusLite {
  HC_NONE,
  HC_WAIT_READ,
  HC_WAIT_CLOSE
};

#define HTTP_DOWNLOAD_UNIT_SIZE 1436

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

#define HTTP_MAX_DATA_WAIT      5000
#define HTTP_MAX_POST_WAIT      5000
#define HTTP_MAX_SEND_WAIT      5000
#define HTTP_MAX_CLOSE_WAIT     5000

#define CONTENT_LENGTH_UNKNOWN ((size_t) - 1)
#define CONTENT_LENGTH_NOT_SET ((size_t) - 2)

class WebServerLite;

typedef struct {
  HTTPUploadStatusLite status;
  String filename;
  String name;
  String type;
  size_t totalSize;
  size_t currentSize;
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUploadLite;

#include "detail/RequestHandlerLite.h"

namespace fs {
class FS;
}

class WebServerLite {
public:
  WebServerLite(IPAddress addr, int port = 80);
  WebServerLite(int port = 80);
  virtual ~WebServerLite();

  virtual void begin();
  virtual void begin(uint16_t port);
  virtual void handleClient();

  virtual void close();
  void stop();

  typedef std::function<void(void)> THandlerFunction;

  // Uri-based methods
  RequestHandlerLite &on(const UriLite &uri, THandlerFunction handler);
  RequestHandlerLite &on(const UriLite &uri, HTTPMethodLite method, THandlerFunction fn);
  RequestHandlerLite &on(const UriLite &uri, HTTPMethodLite method, THandlerFunction fn, THandlerFunction ufn);

  // Convenience overloads for const char* (creates UriLite internally)
  RequestHandlerLite &on(const char *uri, THandlerFunction handler) {
    return on(UriLite(uri), handler);
  }
  RequestHandlerLite &on(const char *uri, HTTPMethodLite method, THandlerFunction fn) {
    return on(UriLite(uri), method, fn);
  }
  RequestHandlerLite &on(const char *uri, HTTPMethodLite method, THandlerFunction fn, THandlerFunction ufn) {
    return on(UriLite(uri), method, fn, ufn);
  }

  // Convenience overloads for String
  RequestHandlerLite &on(const String &uri, THandlerFunction handler) {
    return on(UriLite(uri), handler);
  }
  RequestHandlerLite &on(const String &uri, HTTPMethodLite method, THandlerFunction fn) {
    return on(UriLite(uri), method, fn);
  }
  RequestHandlerLite &on(const String &uri, HTTPMethodLite method, THandlerFunction fn, THandlerFunction ufn) {
    return on(UriLite(uri), method, fn, ufn);
  }

  void addHandler(RequestHandlerLite *handler);
  bool removeHandler(RequestHandlerLite *handler);
  void serveStatic(const char *uri, fs::FS &fs, const char *path, const char *cache_header = NULL);
  void onNotFound(THandlerFunction fn);
  void onFileUpload(THandlerFunction ufn);

  String uri() const { return _currentUri; }
  HTTPMethodLite method() const { return _currentMethod; }
  virtual NetworkClient &client() { return _currentClient; }
  HTTPUploadLite &upload() { return *_currentUpload; }

  String pathArg(unsigned int i) const;
  String arg(const String &name) const;
  String arg(int i) const;
  String argName(int i) const;
  int args() const;
  bool hasArg(const String &name) const;
  void collectHeaders(const char *headerKeys[], const size_t headerKeysCount);
  void collectAllHeaders();
  String header(const String &name) const;
  String header(int i) const;
  String headerName(int i) const;
  int headers() const;
  bool hasHeader(const String &name) const;

  int clientContentLength() const;
  const String version() const;
  String hostHeader() const;

  int responseCode() const;
  int responseHeaders() const;
  const String &responseHeader(String name) const;
  const String &responseHeader(int i) const;
  const String &responseHeaderName(int i) const;
  bool hasResponseHeader(const String &name) const;

  void send(int code, const char *content_type = NULL, const String &content = String(""));
  void send(int code, char *content_type, const String &content);
  void send(int code, const String &content_type, const String &content);
  void send(int code, const char *content_type, const char *content);
  void send(int code, const char *content_type, Stream &stream, size_t content_length = 0);

  void send_P(int code, PGM_P content_type, PGM_P content);
  void send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength);

  void enableDelay(boolean value);

  void setContentLength(const size_t contentLength);
  void sendHeader(const String &name, const String &value, bool first = false);
  void sendContent(const String &content);
  void sendContent(const char *content, size_t contentLength);
  void sendContent_P(PGM_P content);
  void sendContent_P(PGM_P content, size_t size);

  static String urlDecode(const String &text);

  template<typename T> size_t streamFile(T &file, const String &contentType, const int code = 200) {
    _streamFileCore(file.size(), file.name(), contentType, code);
    return _currentClient.write(file);
  }

  static String responseCodeToString(int code);

  uint32_t _lastRequestTime;
  uint32_t getLastRequestTime() const;
  uint32_t getIdleTime() const;
  void _updateActivity();


protected:
  virtual size_t _currentClientWrite(const char *b, size_t l) {
    return _currentClient.write(b, l);
  }
  virtual size_t _currentClientWrite_P(PGM_P b, size_t l) {
    return _currentClient.write_P(b, l);
  }
  void _addRequestHandler(RequestHandlerLite *handler);
  bool _removeRequestHandler(RequestHandlerLite *handler);
  bool _handleRequest();
  void _finalizeResponse();
  bool _parseRequest(NetworkClient &client);
  void _parseArguments(const String &data);
  bool _parseForm(NetworkClient &client, const String &boundary, uint32_t len);
  bool _parseFormUploadAborted();
  void _uploadWriteByte(uint8_t b);
  int _uploadReadByte(NetworkClient &client);
  void _prepareHeader(String &response, int code, const char *content_type, size_t contentLength);
  bool _collectHeader(const char *headerName, const char *headerValue);

  void _streamFileCore(const size_t fileSize, const String &fileName, const String &contentType, const int code = 200);

  void _clearResponseHeaders();
  void _clearRequestHeaders();

  struct RequestArgumentLite {
    String key;
    String value;
    RequestArgumentLite *next;
  };

  NetworkServer _server;
  NetworkClient _currentClient;
  HTTPMethodLite _currentMethod = HTTP_ANY_LITE;
  String _currentUri;
  uint8_t _currentVersion = 0;
  HTTPClientStatusLite _currentStatus = HC_NONE;
  unsigned long _statusChange = 0;
  boolean _nullDelay = true;

  RequestHandlerLite *_currentHandler = nullptr;
  RequestHandlerLite *_firstHandler = nullptr;
  RequestHandlerLite *_lastHandler = nullptr;
  THandlerFunction _notFoundHandler = nullptr;
  THandlerFunction _fileUploadHandler = nullptr;

  int _currentArgCount = 0;
  RequestArgumentLite *_currentArgs = nullptr;
  int _postArgsLen = 0;
  RequestArgumentLite *_postArgs = nullptr;

  std::unique_ptr<HTTPUploadLite> _currentUpload;

  int _headerKeysCount = 0;
  RequestArgumentLite *_currentHeaders = nullptr;
  size_t _contentLength = 0;
  int _clientContentLength = 0;
  RequestArgumentLite *_responseHeaders = nullptr;

  String _hostHeader;

  int _responseHeaderCount = 0;
  int _responseCode = 0;
  bool _collectAllHeaders = false;
};

#endif  //WEBSERVERLITE_H