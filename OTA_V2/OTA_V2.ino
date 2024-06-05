#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <PubSubClient.h>

#define CONNECTION_TIMEOUT 20 // Increase the connection timeout to 20 seconds

AsyncWebServer server(80);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

char mqttBroker[100];
int mqttPort;
char mqttTopic[100];
char mqttUsername[100];
char mqttPassword[100];

unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 10000; // 10 seconds
bool mqttConnected = false;

String readFile(String path) {
  File file = SPIFFS.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("Failed to open file for reading");
    return String();
  }
  String content = file.readString();
  file.close();
  return content;
}

bool writeFile(String path, String content) {
  File file = SPIFFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  file.print(content);
  file.close();
  return true;
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages here
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqttUsername, mqttPassword)) {
      Serial.println("connected");
      mqttConnected = true;
      // Subscribe to the MQTT topic (if needed)
      // client.subscribe(mqttTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.mode(WIFI_STA);

  // Attempt to read Wi-Fi credentials from "wifi.txt"
  String wifiCredentials = readFile("/wifi.txt");
  if (wifiCredentials.length() < 3) {
    Serial.println("No valid Wi-Fi credentials found. Opening Access Point.");
    WiFi.softAP("ESP32-Setup-AP", "password");
    Serial.println("Esp has created its Hostpot u can Connect andd Password is Password. Acccess point is - 192.168.4.1");
  } else {
    // Split the credentials into SSID and Password
    int separatorIndex = wifiCredentials.indexOf(':');
    if (separatorIndex != -1) {
      String ssid = wifiCredentials.substring(0, separatorIndex);
      String password = wifiCredentials.substring(separatorIndex + 1);
      WiFi.begin(ssid.c_str(), password.c_str());

      Serial.println("\nConnecting to Wi-Fi");
      int timeout_counter = 0;

      while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
        timeout_counter++;
        if (timeout_counter >= CONNECTION_TIMEOUT) {
          Serial.println("\nFailed to connect to Wi-Fi. Opening Access Point.");
          WiFi.softAP("ESP32-Setup-AP", "password");
          Serial.println("Esp has createdd itss Hostpot u can Connecct andd Password is Passwword. Acccess point is192.168.4.1");
          break;
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to the Wi-Fi network");
        Serial.print("Local ESP32 IP: ");
        Serial.println(WiFi.localIP());
      }
    } else {
      Serial.println("Invalid Wi-Fi credentials format in wifi.txt");
    }
  }
  
  Serial.println("HTTP server started");
  
  // Read MQTT configuration from file
  String mqttConfig = readFile("/mqtt.txt");

  // Split the MQTT configuration using ':' as a separator
  int separatorIndex = 0;
  for (int i = 0; i < 5; i++) {
    int nextSeparatorIndex = mqttConfig.indexOf(':', separatorIndex);
    if (nextSeparatorIndex == -1) {
      break;
    }
    if (i == 0) {
      mqttConfig.substring(separatorIndex, nextSeparatorIndex).toCharArray(mqttBroker, 100);
    } else if (i == 1) {
      mqttPort = mqttConfig.substring(separatorIndex, nextSeparatorIndex).toInt();
    } else if (i == 2) {
      mqttConfig.substring(separatorIndex, nextSeparatorIndex).toCharArray(mqttTopic, 100);
    } else if (i == 3) {
      mqttConfig.substring(separatorIndex, nextSeparatorIndex).toCharArray(mqttUsername, 100);
    } else if (i == 4) {
      mqttConfig.substring(separatorIndex, nextSeparatorIndex).toCharArray(mqttPassword, 100);
    }
    separatorIndex = nextSeparatorIndex + 1;
  }

  // MQTT setup
  client.setServer(mqttBroker, mqttPort);
  client.setCallback(callback);
  reconnect();
  
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    String css = readFile("/css/style.css");
    request->send(200, "text/css", css);
  });
  
  server.on("/skeleton.css", HTTP_GET, [](AsyncWebServerRequest *request){
    String css = readFile("/css/skeleton.css");
    request->send(200, "text/css", css);
  });

  server.on("/normalize.css", HTTP_GET, [](AsyncWebServerRequest *request){
    String css = readFile("/css/normalize.css");
    request->send(200, "text/css", css);
  });
   
   server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    String js = readFile("/script.js");
    request->send(200, "application/javascript", js);
  });
  
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Login page accessed!");
    String html = readFile("/index.html");
    request->send(200, "text/html", html);
  });

   server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasArg("username") && request->hasArg("password")) {
      String username = request->arg("username");
      String password = request->arg("password");
      String credentials = readFile("/credentials.txt");
  
      // Split stored credentials into username and password
      int separatorIndex = credentials.indexOf(':');
      if (separatorIndex != -1) {
        String storedUsername = credentials.substring(0, separatorIndex);
        String storedPassword = credentials.substring(separatorIndex + 1);
  
        if (username.equals(storedUsername) && password.equals(storedPassword)) {
          Serial.println("Login Success!");
          request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Login successful\"}");
          return;
        }
      }
      
      // If username or password is incorrect, return Unauthorized
      Serial.println("Login failed!");
      request->send(401, "application/json", "{\"status\":\"error\",\"message\":\"Unauthorized Invalid credentials!!\"}");
    } else {
      Serial.println("Login failed!");
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Something Went Wrrong!\"}");

    }
  });



  server.on("/dash", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Dashboard Configuration page has been Displaying!!");
    String html = readFile("/dash.html");
    html.replace("{ssid}", WiFi.SSID());
    html.replace("{rssi}", String(WiFi.RSSI()));
    html.replace("{mac}", WiFi.macAddress());
    html.replace("{channel}", String(WiFi.channel()));
    html.replace("{ip}", WiFi.localIP().toString());
    html.replace("{subnet}", WiFi.subnetMask().toString());
    html.replace("{gateway}", WiFi.gatewayIP().toString());

    // Read MQTT configuration from file
    String mqttConfig = readFile("/mqtt.txt");

    // Split the MQTT configuration using ':' as a separator
    String mqtt_parts[4];
    int separatorIndex = 0;
    for (int i = 0; i < 4; i++) {
        int nextSeparatorIndex = mqttConfig.indexOf(':', separatorIndex);
        if (nextSeparatorIndex == -1) {
            break;
        }
        mqtt_parts[i] = mqttConfig.substring(separatorIndex, nextSeparatorIndex);
        separatorIndex = nextSeparatorIndex + 1;
    }

    if (separatorIndex > 0) {
        // Replace placeholders with MQTT configuration
        html.replace("{mqtt_host}", mqtt_parts[0]);
        html.replace("{mqtt_port}", mqtt_parts[1]);
        html.replace("{mqtt_topic}", mqtt_parts[2]);
        html.replace("{mqtt_username}", mqtt_parts[3]);
    }

    request->send(200, "text/html", html);
});



   server.on("/modbus", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Modbus Configuration page has been Displaying");
    String html = readFile("/modbus.html");
    request->send(200, "text/html", html);
  });
  

  server.on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("MQTT Configuration page has been Displaying");
   String html = readFile("/mqtt.html");
   request->send(200, "text/html", html);
  });

  server.on("/update-mqtt", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("MQTT Configuration is updating!!");
    if (request->hasArg("host") && request->hasArg("port") && request->hasArg("topic") && request->hasArg("username") && request->hasArg("password")) {
        String newHost = request->arg("host");
        String newPort = request->arg("port");
        String newTopic = request->arg("topic");
        String newUsername = request->arg("username");
        String newPassword = request->arg("password");
        String mqttCredentials = newHost + ":" + newPort + ":" + newTopic + ":" + newUsername + ":" + newPassword;

        // Save the new MQTT credentials to mqtt.txt
        bool success = writeFile("/mqtt.txt", mqttCredentials);

        if (success) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated. ESP will restart.\"}");
            Serial.println("MQTT has been updated Sucesssffuly");
            delay(2000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration.\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request.\"}");
    }
});
  
  server.on("/ethernet", HTTP_GET, [](AsyncWebServerRequest *request){
   Serial.println("Ethernet Configuration page has been Displaying");
   String html = readFile("/ethernet.html");
   request->send(200, "text/html", html);
  });
  

 server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("WIFI Configuration page has been Displaying");
    String html = readFile("/wifi.html");
    request->send(200, "text/html", html);
 });

 server.on("/update-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Wifi Configuration is updating!!");
    if (request->hasArg("ssid") && request->hasArg("password")) {
        String newSsid = request->arg("ssid");
        String newPassword = request->arg("password");
        String wifiCredentials = newSsid + ":" + newPassword;

        // Save the new Wi-Fi credentials to wifi.txt
        bool success = writeFile("/wifi.txt", wifiCredentials);

        if (success) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated. ESP will restart.\"}");
            Serial.println("WIFI has been updated Sucesssffuly");
            delay(2000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration.\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request.\"}");
    }
});



  server.begin();
}


void loop() {
  if (!mqttConnected) {
    reconnect();
  }

  // Check if the MQTT client is connected before publishing data
  if (mqttConnected) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastPublishTime >= publishInterval) {
      lastPublishTime = currentMillis;

      String data = readFile("/data.txt");

      if (client.publish(mqttTopic, data.c_str())) {
        Serial.println("Data published to MQTT");
      } else {
        Serial.println("Data publication failed");
      }
    }
  }

  // Other loop code here...
}
