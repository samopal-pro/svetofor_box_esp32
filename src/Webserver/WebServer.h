/*
  Fork samopal.pro 03.07.2026
  WebServer.h - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Original Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  
  Changes:
  - Added conditional compilation for unused features
  - Uri-based on() always available for backward compatibility
  - removeRoute() only with WEBSERVER_INCLUDE_REGEX
  - Removed middleware/Middleware.h include (not used)
  - Removed MiddlewareChain member
  - Removed String-based on() to avoid ambiguity with Uri
*/

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <functional>
#include <memory>
#include "FS.h"
#include "Network.h"
#include "HTTP_Method.h"
#include "Uri.h"

// ===== FLAGS FOR EXCLUDING UNUSED FUNCTIONALITY =====
#ifndef WEBSERVER_NO_AUTH
  #define WEBSERVER_INCLUDE_AUTH 1
#endif

#ifndef WEBSERVER_NO_CORS
  #define WEBSERVER_INCLUDE_CORS 1
#endif

#ifndef WEBSERVER_NO_ETAG
  #define WEBSERVER_INCLUDE_ETAG 1
#endif

#ifndef WEBSERVER_NO_CHUNKED
  #define WEBSERVER_INCLUDE_CHUNKED 1
#endif

#ifndef WEBSERVER_NO_SSE
  #define WEBSERVER_INCLUDE_SSE 1
#endif

#ifndef WEBSERVER_NO_RAW
  #define WEBSERVER_INCLUDE_RAW 1
#endif

#ifndef WEBSERVER_NO_MIDDLEWARE
  #define WEBSERVER_INCLUDE_MIDDLEWARE 1
#endif

#ifndef WEBSERVER_NO_REGEX
  #define WEBSERVER_INCLUDE_REGEX 1
#endif

enum HTTPUploadStatus {
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED
};

#ifdef WEBSERVER_INCLUDE_RAW
enum HTTPRawStatus {
  RAW_START,
  RAW_WRITE,
  RAW_END,
  RAW_ABORTED
};
#endif

enum HTTPClientStatus {
  HC_NONE,
  HC_WAIT_READ,
  HC_WAIT_CLOSE
};

#ifdef WEBSERVER_INCLUDE_AUTH
enum HTTPAuthMethod {
  BASIC_AUTH,
  DIGEST_AUTH,
  OTHER_AUTH
};
#endif

#define HTTP_DOWNLOAD_UNIT_SIZE 1436

#ifndef HTTP_UPLOAD_BUFLEN
#define HTTP_UPLOAD_BUFLEN 1436
#endif

#ifdef WEBSERVER_INCLUDE_RAW
#ifndef HTTP_RAW_BUFLEN
#define HTTP_RAW_BUFLEN 1436
#endif
#endif

#define HTTP_MAX_DATA_WAIT      5000
#define HTTP_MAX_POST_WAIT      5000
#define HTTP_MAX_SEND_WAIT      5000
#define HTTP_MAX_CLOSE_WAIT     5000

#ifdef WEBSERVER_INCLUDE_AUTH
#define HTTP_MAX_BASIC_AUTH_LEN 256
#endif

#define CONTENT_LENGTH_UNKNOWN ((size_t) - 1)
#define CONTENT_LENGTH_NOT_SET ((size_t) - 2)

class WebServer;

typedef struct {
  HTTPUploadStatus status;
  String filename;
  String name;
  String type;
  size_t totalSize;
  size_t currentSize;
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
} HTTPUpload;

#ifdef WEBSERVER_INCLUDE_RAW
typedef struct {
  HTTPRawStatus status;
  size_t totalSize;
  size_t currentSize;
  uint8_t buf[HTTP_RAW_BUFLEN];
  void *data;
} HTTPRaw;
#endif

// Middleware support removed in this fork
// #include "middleware/Middleware.h"

#include "detail/RequestHandler.h"

namespace fs {
class FS;
}

class WebServer {
public:
  WebServer(IPAddress addr, int port = 80);
  WebServer(int port = 80);
  virtual ~WebServer();

  virtual void begin();
  virtual void begin(uint16_t port);
  virtual void handleClient();

  virtual void close();
  void stop();

  // ===== AUTHENTICATION (conditional) =====
  #ifdef WEBSERVER_INCLUDE_AUTH
    const String AuthTypeDigest = F("Digest");
    const String AuthTypeBasic = F("Basic");
    typedef std::function<String *(HTTPAuthMethod mode, String enteredUsernameOrReq, String extraParams[])> THandlerFunctionAuthCheck;
    bool authenticate(THandlerFunctionAuthCheck fn);
    bool authenticate(const char *username, const char *password);
    bool authenticateBasicSHA1(const char *_username, const char *_sha1AsBase64orHex);
    void requestAuthentication(HTTPAuthMethod mode = BASIC_AUTH, const char *realm = NULL, const String &authFailMsg = String(""));
  #endif

  // ===== CHUNKED RESPONSE (conditional) =====
  #ifdef WEBSERVER_INCLUDE_CHUNKED
    void chunkResponseBegin(const char *contentType = "text/plain");
    void chunkWrite(const char *data, size_t length);
    void chunkResponseEnd();
  #endif

  typedef std::function<void(void)> THandlerFunction;

  // ===== ROUTE REGISTRATION =====
  // Uri-based methods - конструктор Uri(const char*) обрабатывает строки
  RequestHandler &on(const Uri &uri, THandlerFunction fn);
  RequestHandler &on(const Uri &uri, HTTPMethod method, THandlerFunction fn);
  RequestHandler &on(const Uri &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn);

  // Extended route management (only with REGEX)
  #ifdef WEBSERVER_INCLUDE_REGEX
    typedef std::function<bool(WebServer &server)> FilterFunction;
    bool removeRoute(const char *uri);
    bool removeRoute(const char *uri, HTTPMethod method);
    bool removeRoute(const String &uri);
    bool removeRoute(const String &uri, HTTPMethod method);
  #endif

  void addHandler(RequestHandler *handler);
  bool removeHandler(RequestHandler *handler);
  void serveStatic(const char *uri, fs::FS &fs, const char *path, const char *cache_header = NULL);
  void onNotFound(THandlerFunction fn);
  void onFileUpload(THandlerFunction ufn);

  // Middleware removed in this fork

  String uri() const { return _currentUri; }
  HTTPMethod method() const { return _currentMethod; }
  virtual NetworkClient &client() { return _currentClient; }
  HTTPUpload &upload() { return *_currentUpload; }

  #ifdef WEBSERVER_INCLUDE_RAW
  HTTPRaw &raw() { return *_currentRaw; }
  #endif

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

  #ifdef WEBSERVER_INCLUDE_CORS
    void enableCORS(boolean value = true);
    void enableCrossOrigin(boolean value = true);
  #endif

  #ifdef WEBSERVER_INCLUDE_ETAG
    typedef std::function<String(FS &fs, const String &fName)> ETagFunction;
    void enableETag(bool enable, ETagFunction fn = nullptr);
    bool _eTagEnabled = false;
    ETagFunction _eTagFunction = nullptr;
  #endif

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
#ifdef IS_SET_ACTIVITY
  uint32_t _lastRequestTime;
  uint32_t getLastRequestTime() const;
  uint32_t getIdleTime() const;
  void _updateActivity();
#endif

protected:
  virtual size_t _currentClientWrite(const char *b, size_t l) {
    return _currentClient.write(b, l);
  }
  virtual size_t _currentClientWrite_P(PGM_P b, size_t l) {
    return _currentClient.write_P(b, l);
  }
  void _addRequestHandler(RequestHandler *handler);
  bool _removeRequestHandler(RequestHandler *handler);
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

  #ifdef WEBSERVER_INCLUDE_AUTH
    String _getRandomHexString();
    String _extractParam(String &authReq, const String &param, const char delimit = '"');
  #endif

  void _clearResponseHeaders();
  void _clearRequestHeaders();

  struct RequestArgument {
    String key;
    String value;
    RequestArgument *next;
  };

  #ifdef WEBSERVER_INCLUDE_CORS
    boolean _corsEnabled = false;
  #endif

  NetworkServer _server;
  NetworkClient _currentClient;
  HTTPMethod _currentMethod = HTTP_ANY;
  String _currentUri;
  uint8_t _currentVersion = 0;
  HTTPClientStatus _currentStatus = HC_NONE;
  unsigned long _statusChange = 0;
  boolean _nullDelay = true;

  RequestHandler *_currentHandler = nullptr;
  RequestHandler *_firstHandler = nullptr;
  RequestHandler *_lastHandler = nullptr;
  THandlerFunction _notFoundHandler = nullptr;
  THandlerFunction _fileUploadHandler = nullptr;

  int _currentArgCount = 0;
  RequestArgument *_currentArgs = nullptr;
  int _postArgsLen = 0;
  RequestArgument *_postArgs = nullptr;

  std::unique_ptr<HTTPUpload> _currentUpload;

  #ifdef WEBSERVER_INCLUDE_RAW
    std::unique_ptr<HTTPRaw> _currentRaw;
  #endif

  int _headerKeysCount = 0;
  RequestArgument *_currentHeaders = nullptr;
  size_t _contentLength = 0;
  int _clientContentLength = 0;
  RequestArgument *_responseHeaders = nullptr;

  String _hostHeader;

  #ifdef WEBSERVER_INCLUDE_CHUNKED
    bool _chunked = false;
  #endif

  #ifdef WEBSERVER_INCLUDE_AUTH
    String _snonce;
    String _sopaque;
    String _srealm;
  #endif

  int _responseHeaderCount = 0;
  int _responseCode = 0;
  bool _collectAllHeaders = false;

  // MiddlewareChain removed in this fork
  // MiddlewareChain *_chain = nullptr;

  #ifdef WEBSERVER_INCLUDE_CHUNKED
    bool _chunkedResponseActive = false;
    NetworkClient _chunkedClient;
  #endif
};

#endif  //WEBSERVER_H