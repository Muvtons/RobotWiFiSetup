#ifndef WiFiSetupPortal_h
#define WiFiSetupPortal_h

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t serialMutex; // From RobotController library

class WiFiSetupPortal {
public:
  typedef std::function<void(const char* ssid, const char* password)> CredentialsCallback;
  
  struct Config {
    const char* apName;
    const char* apPassword;
    const char* defaultDashboardURL;
    uint16_t apChannel;
    bool apHidden;
    bool debugMode;
    
    Config() : 
      apName("RobotSetupAP"),
      apPassword("securepass123"),
      defaultDashboardURL("http://192.168.1.100/dashboard"),
      apChannel(1),
      apHidden(false),
      debugMode(false) {}
  };

  WiFiSetupPortal();
  ~WiFiSetupPortal();
  
  void begin(const Config& config = Config());
  void beginTask(int coreID = 1, int stackSize = 4096, int priority = 1);
  void stop();
  
  bool isConnected();
  String getDashboardURL();
  String getStatus();
  
  void setCredentialsCallback(CredentialsCallback callback);
  
  // For manual loop control (if not using task)
  void loop();
  
private:
  Config _config;
  WebServer* _server;
  DNSServer* _dnsServer;
  bool _isConnected;
  String _dashboardURL;
  String _currentStatus;
  CredentialsCallback _credentialsCallback;
  TaskHandle_t _taskHandle;
  
  void _initWiFiAP();
  void _setupRoutes();
  void _handleRoot();
  void _handleConnect();
  void _handleScan();
  void _handleStatus();
  void _handleNotFound();
  void _processSerialCommands();
  
  static void _taskFunction(void* param);
  void _runTask();
  
  // Thread-safe serial functions
  void _safePrint(const char* str);
  void _safePrintln(const char* str);
  void _safePrintf(const char* format, ...);
};

#endif
