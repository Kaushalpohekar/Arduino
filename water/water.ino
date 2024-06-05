#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LittleFS.h> // ESP8266 uses LittleFS instead of SPIFFS

const char* ssid = "Airtel_SenseLive"; // SSID
const char* password = "SL123456";      // PASSWORD

ESP8266WebServer server(80);

String readFile(String path) {
  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("Failed to open file for reading");
    return String();
  }
  String content = file.readString();
  file.close();
  return content;
}

bool writeFile(String path, String content) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  size_t bytesWritten = file.print(content);
  file.close();
  return bytesWritten == content.length();
}

void handleRoot() {
  String html = readFile("/index.html");
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  if (!LittleFS.begin()) { // Initialize LittleFS
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  int timeout_counter = 0;

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n Connecting .................");
    delay(1000);
    timeout_counter++;
    if (timeout_counter >= 10 * 5) {
      ESP.restart();
    }
  }

  Serial.println("\n Connected to the WiFi network");
  Serial.print("Local ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  

  Serial.println("HTTP server started");

  server.on("/", HTTP_GET, handleRoot);

  server.begin();
}

void loop() {
  server.handleClient();
}
