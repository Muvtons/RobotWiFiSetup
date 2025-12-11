#include <RobotController.h>
#include <WiFiSetupPortal.h>

RobotController robot;
WiFiSetupPortal wifiPortal;

// WiFi credentials callback
void onWiFiCredentials(const char* ssid, const char* password) {
  // This runs on Core 1, but we need to handle connection on Core 0
  // Use a queue or flag to pass credentials to main app
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.printf("CREDENTIALS_RECEIVED: SSID=%s\n", ssid);
    xSemaphoreGive(serialMutex);
  }
}

void setup() {
  // Initialize robot on Core 0
  robot.begin();
  robot.enableSerialControl(false); // Disable library's serial processing
  
  // Configure and start WiFi portal on Core 1
  WiFiSetupPortal::Config wifiConfig;
  wifiConfig.apName = "MyRobotSetup";
  wifiConfig.apPassword = "mysecurepass";
  wifiConfig.defaultDashboardURL = "http://192.168.1.100/dashboard";
  wifiConfig.debugMode = true;
  
  wifiPortal.begin(wifiConfig);
  wifiPortal.setCredentialsCallback(onWiFiCredentials);
  wifiPortal.beginTask(1); // Start on Core 1
  
  Serial.println("\n✅ Multi-Core System Ready!");
  Serial.println("Robot commands: move_forward, turn_left 90, turn_right 90, STOP");
  Serial.println("WiFi portal running on Core 1");
}

String inputBuffer = "";

void processRobotCommand(String cmd) {
  if (cmd.equalsIgnoreCase("move_forward")) {
    robot.moveForward(1000);
  } 
  else if (cmd.startsWith("turn_left ")) {
    int angle = cmd.substring(10).toInt();
    if (angle > 0) robot.turnLeft(angle);
  } 
  else if (cmd.startsWith("turn_right ")) {
    int angle = cmd.substring(11).toInt();
    if (angle > 0) robot.turnRight(angle);
  } 
  else if (cmd.equalsIgnoreCase("STOP")) {
    robot.stop();
  }
  else if (cmd.equalsIgnoreCase("status")) {
    Serial.printf("[Core0] Robot status: %s\n", wifiPortal.isConnected() ? "ONLINE" : "OFFLINE");
    Serial.printf("[Core0] Dashboard: %s\n", wifiPortal.getDashboardURL().c_str());
  }
}

void loop() {
  // Read serial commands on Core 0
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        processRobotCommand(inputBuffer);
      }
      inputBuffer = "";
    } else if (inputBuffer.length() < 64) {
      inputBuffer += c;
    }
  }
  
  // Process WiFi portal status updates
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000) {
    if (wifiPortal.isConnected()) {
      Serial.printf("[Core0] ✅ WiFi Connected! Dashboard: %s\n", wifiPortal.getDashboardURL().c_str());
    }
    lastStatus = millis();
  }
  
  // Run robot control on Core 0
  robot.run();
  
  delay(1);
}
