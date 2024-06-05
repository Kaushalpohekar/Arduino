#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <ArduinoJson.h>

#define CONNECTION_TIMEOUT 20 // Increase the connection timeout to 20 seconds
#define MAX485_DE 2
#define MAX485_RE_NEG 4

ModbusMaster node;
AsyncWebServer server(80);
WiFiClient wifiClient;
PubSubClient client(wifiClient);
SoftwareSerial modbusSerial(16, 17);

union u_tag {
  uint16_t bdata[2];
  float floatValue;
} unionFloat;

char mqttBroker[100];
int mqttPort;
char mqttTopic[100];
char mqttUsername[100];
char mqttPassword[100];
String configType;
String dns;
String staticIP;
String subnetMask;
String gateway;
int modbusSlaveId;
int modbusBaudRate;
int modbusWordCount;
char modbusParity[100];
char modbusStopBit[100];
int modbusLength;
int modbusAddress;
int modbusPointType;
String modbusParityConfig = "";

int tab[] = {0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,102,124,126,128,130,132,134,136,138,140};
const char* registerNames[] = {
  "voltage_1n","voltage_2n","voltage_3n","voltage_N","voltage_12","voltage_23","voltage_31","voltage_L","current_1","current_2","current_3","current","kw_1",
  "kw_2","kw_3","kvar_1","kvar_2","kvar_3","kva_1","kva_2","kva_3","pf_1","pf_2","pf_3","pf","freq","kw","kvar","kva","max_kw","min_kw","max_kvar","min_kvar","max_kva",
  "max_int_v1n","max_int_v2n","max_int_v3n","max_int_v12","max_int_v23","max_int_v31","max_int_i1","max_int_i2","max_int_i3","imp_kwh","exp_kwh","kwh","imp_kvarh",
  "exp_kvarh","kvarh","kvah","run_h","on_h","thd_v1n","thd_v2n","thd_v3n","thd_v12","thd_v23","thd_v31","thd_i1","thd_i2","thd_i3"
};

const int numRegisters = sizeof(tab) / sizeof(tab[0]);
float values[numRegisters];

unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 1000; // 10 seconds
bool mqttConnected = false;
bool printMqttErrors = true;

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW);
  digitalWrite(MAX485_DE, LOW);
}

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
      int mqttState = client.state();
      Serial.print("failed, rc=");
      Serial.print(mqttState);

      if (printMqttErrors) {
        switch (mqttState) {
          case -4:
            Serial.println(" (MQTT_CONNECTION_TIMEOUT)");
            break;
          case -3:
            Serial.println(" (MQTT_CONNECTION_LOST)");
            break;
          case -2:
            Serial.println(" (MQTT_CONNECT_FAILED)");
            break;
          case -1:
            Serial.println(" (MQTT_DISCONNECTED)");
            break;
          case 1:
            Serial.println(" (MQTT_CONNECT_BAD_PROTOCOL)");
            break;
          case 2:
            Serial.println(" (MQTT_CONNECT_BAD_CLIENT_ID)");
            break;
          case 3:
            Serial.println(" (MQTT_CONNECT_UNAVAILABLE)");
            break;
          case 4:
            Serial.println(" (MQTT_CONNECT_BAD_CREDENTIALS)");
            break;
          case 5:
            Serial.println(" (MQTT_CONNECT_UNAUTHORIZED)");
            break;
          default:
            Serial.println(" (Unknown error)");
            break;
        }
      } else {
        Serial.println(" try again in 5 seconds");
      }

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
  String  ethernetConfig = readFile("/ethernet.txt");
  if (ethernetConfig.startsWith("dhcp")) {
    configType = "dhcp";
  } else {
    int separatorIndex = ethernetConfig.indexOf(':');
    if (separatorIndex != -1) {
      configType = ethernetConfig.substring(0, separatorIndex);
      if (configType.equals("static")) {
        // Configuration type is "static," split the remaining values
        int nextSeparatorIndex = ethernetConfig.indexOf(':', separatorIndex + 1);
        if (nextSeparatorIndex != -1) {
          dns = ethernetConfig.substring(separatorIndex + 1, nextSeparatorIndex);
          // Move to the next parameter (staticIP)
          separatorIndex = nextSeparatorIndex;
          
          // Split for staticIP
          nextSeparatorIndex = ethernetConfig.indexOf(':', separatorIndex + 1);
          if (nextSeparatorIndex != -1) {
            staticIP = ethernetConfig.substring(separatorIndex + 1, nextSeparatorIndex);
            // Move to the next parameter (subnetMask)
            separatorIndex = nextSeparatorIndex;
            
            // Split for subnetMask
            nextSeparatorIndex = ethernetConfig.indexOf(':', separatorIndex + 1);
            if (nextSeparatorIndex != -1) {
              subnetMask = ethernetConfig.substring(separatorIndex + 1, nextSeparatorIndex);
              // Move to the next parameter (gateway)
              separatorIndex = nextSeparatorIndex;
              
              // Extract the gateway
              gateway = ethernetConfig.substring(separatorIndex + 1);
            }
          }
        }
      }
    }
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

// Split the Modbus configuration using ':' as a separator
String modbusConfig = readFile("/modbus.txt");

// Split the Modbus configuration using ':' as a separator
int modbusSeparatorIndex = 0;
for (int i = 0; i < 8; i++) {
    int modbusNextSeparatorIndex = modbusConfig.indexOf(':', modbusSeparatorIndex);
    if (modbusNextSeparatorIndex == -1) {
        // Handle the last value outside of the loop
        if (i == 7) {
            modbusPointType = modbusConfig.substring(modbusSeparatorIndex).toInt();
        }
        break;
    }

    switch (i) {
        case 0:
            modbusSlaveId = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
        case 1:
            modbusBaudRate = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
        case 2:
            modbusWordCount = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
        case 3:
            modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toCharArray(modbusParity, 100);
            break;
        case 4:
            modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toCharArray(modbusStopBit, 100);
            break;
        case 5:
            modbusLength = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
        case 6:
            modbusAddress = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
        case 7:
            modbusPointType = modbusConfig.substring(modbusSeparatorIndex, modbusNextSeparatorIndex).toInt();
            break;
    }
    
    modbusSeparatorIndex = modbusNextSeparatorIndex + 1;
}


// Check Modbus parameters and set Modbus parity and configuration
if ((modbusWordCount == 7 || modbusWordCount == 8) && (strcmp(modbusStopBit, "1") == 0 || strcmp(modbusStopBit, "2") == 0)) {
    if (strcmp(modbusParity, "even") == 0) {
        if (modbusWordCount == 7) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_7E1
                modbusParityConfig = "Serial_7E1";
                Serial.println("Modbus parameters match Serial_7E1");
                // Serial.begin(modbusBaudRate, SERIAL_7E1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_7E2
                modbusParityConfig = "Serial_7E2";
                Serial.println("Modbus parameters match Serial_7E2");
                // Serial.begin(modbusBaudRate, SERIAL_7E2);
            }
        } else if (modbusWordCount == 8) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_8E1
                modbusParityConfig = "Serial_8E1";
                Serial.println("Modbus parameters match Serial_8E1");
                // Serial.begin(modbusBaudRate, SERIAL_8E1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_8E2
                modbusParityConfig = "Serial_8E2";
                Serial.println("Modbus parameters match Serial_8E2");
                // Serial.begin(modbusBaudRate, SERIAL_8E2);
            }
        }
    } else if (strcmp(modbusParity, "odd") == 0) {
        if (modbusWordCount == 7) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_7O1
                modbusParityConfig = "Serial_7O1";
                Serial.println("Modbus parameters match Serial_7O1");
                // Serial.begin(modbusBaudRate, SERIAL_7O1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_7O2
                modbusParityConfig = "Serial_7O2";
                Serial.println("Modbus parameters match Serial_7O2");
                // Serial.begin(modbusBaudRate, SERIAL_7O2);
            }
        } else if (modbusWordCount == 8) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_8O1
                modbusParityConfig = "Serial_8O1";
                Serial.println("Modbus parameters match Serial_8O1");
                // Serial.begin(modbusBaudRate, SERIAL_8O1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_8O2
                modbusParityConfig = "Serial_8O2";
                Serial.println("Modbus parameters match Serial_8O2");
                // Serial.begin(modbusBaudRate, SERIAL_8O2);
            }
        }
    } else if (strcmp(modbusParity, "none") == 0) {
        if (modbusWordCount == 7) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_7N1
                modbusParityConfig = "Serial_7N1";
                Serial.println("Modbus parameters match Serial_7N1");
                // Serial.begin(modbusBaudRate, SERIAL_7N1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_7N2
                modbusParityConfig = "Serial_7N2";
                Serial.println("Modbus parameters match Serial_7N2");
                // Serial.begin(modbusBaudRate, SERIAL_7N2);
            }
        } else if (modbusWordCount == 8) {
            if (strcmp(modbusStopBit, "1") == 0) {
                // Set Modbus parity to Serial_8N1
                modbusParityConfig = "Serial_8N1";
                Serial.println("Modbus parameters match Serial_8N1");
                // Serial.begin(modbusBaudRate, SERIAL_8N1);
            } else if (strcmp(modbusStopBit, "2") == 0) {
                // Set Modbus parity to Serial_8N2
                modbusParityConfig = "Serial_8N2";
                Serial.println("Modbus parameters match Serial_8N2");
                // Serial.begin(modbusBaudRate, SERIAL_8N2);
            }
        }
    } else {
        // Handle unsupported Modbus parity
        Serial.println("Unsupported Modbus parity");
        // You can set a default Modbus configuration here
    }
} else {
    // Handle unsupported Modbus configurations
    Serial.println("Unsupported Modbus parameters");
    // You can set a default Modbus configuration here
}

// Set default Modbus configuration if not configured
if (modbusParityConfig == "") {
    // Set default Modbus parity to Serial_8N1
    modbusParityConfig = "Serial_8N1";
    Serial.println("Default Modbus parameters: Serial_8N1");
    // Serial.begin(modbusBaudRate, SERIAL_8N1);
}
  Serial.println(modbusSlaveId);
  Serial.println(modbusBaudRate);
  Serial.println(modbusWordCount);
  Serial.println(modbusParity);
  Serial.println(modbusStopBit);
  Serial.println(modbusLength);
  Serial.println(modbusAddress);
  Serial.println(modbusPointType);
  Serial.println(modbusParityConfig);

  
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
    html.replace("{modbusSlaveId}", String(modbusSlaveId));
    html.replace("{modbusBaudRate}", String(modbusBaudRate));
    html.replace("{modbusLength}", String(modbusLength));
    html.replace("{modbusAddress}", String(modbusAddress));
    String modbusPointTypeConfig;
    if(modbusPointType == 1){
      modbusPointTypeConfig = "01: Coil Status";
    } else if(modbusPointType == 2){
      modbusPointTypeConfig = "02: Input Status ";
    } else if(modbusPointType == 3){
      modbusPointTypeConfig = "03: Holding Register ";
    } else if(modbusPointType == 4){
      modbusPointTypeConfig = "04: Input Register";
    }
    html.replace("{modbusPointType}", modbusPointTypeConfig);
    html.replace("{modbusParityConfig}", modbusParityConfig);

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

  server.on("/update-modbus", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Modbus Configuration is updating!!");
    if (request->hasArg("slaveId") && request->hasArg("baudRate")
        && request->hasArg("wordCount") && request->hasArg("parity") 
        && request->hasArg("stopBit") && request->hasArg("length") 
        && request->hasArg("address") && request->hasArg("pointType")) {
        String newSlaveId = request->arg("slaveId");
        String newBaudRate = request->arg("baudRate");
        String newWordCount = request->arg("wordCount");
        String newParity = request->arg("parity");
        String newStopBit = request->arg("stopBit");
        String newLength = request->arg("length");
        String newAddress = request->arg("address");
        String newPointType = request->arg("pointType");
        String modbusCredentials = newSlaveId + ":" + newBaudRate + ":" + newWordCount + ":" + newParity + ":" + newStopBit + ":" + newLength + ":" + newAddress + ":" + newPointType;
        Serial.println(modbusCredentials);
        // Save the new MQTT credentials to mqtt.txt
        bool success = writeFile("/modbus.txt", modbusCredentials);

        if (success) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated. ESP will restart.\"}");
            Serial.println("Moodbus has been updated Sucesssffuly");
            delay(2000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration.\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request.\"}");
    }
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

 server.on("/update-ip", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Ethernet Configuration is updating!!");
    
    if (request->hasArg("ipConfig")) {
        String newIpConfig = request->arg("ipConfig");
        String ethernetCredentials;
        Serial.print("Ip config value:-");
        Serial.println(newIpConfig);
        
        
        if (newIpConfig == "dhcp") {
            ethernetCredentials = newIpConfig;
        } else if (newIpConfig == "static") {
            String newDNS = request->arg("dns");
            String newStaticIP = request->arg("staticIP");
            String newSubnetMask = request->arg("subnetMask");
            String newGateway = request->arg("gateway");

            ethernetCredentials = newIpConfig + ":" + newDNS + ":" + newStaticIP + ":" + newSubnetMask + ":" + newGateway;
            Serial.println("Updateing Static values");
        } else {
            // Invalid IP configuration
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid IP configuration.\"}");
            Serial.println("Invalid IP configuration.");
            return;
        }

        bool success = writeFile("/ethernet.txt", ethernetCredentials);

        if (success) {
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated. ESP will restart.\"}");
            Serial.println("Ethernet has been updated successfully");
            delay(2000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration.\"}");
            Serial.println("Failed to save configuration.");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing IP configuration data.\"}");
        Serial.println("Missing IP configuration data.");
    }
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

server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Resetting configuration...");

    // Clear the content of configuration files
    bool successWifi = writeFile("/wifi.txt", "");
    bool successMqtt = writeFile("/mqtt.txt", "");
    bool successModbus = writeFile("/modbus.txt", "");
    bool successEthernet = writeFile("/ethernet.txt", "");

    if (successWifi && successMqtt && successModbus && successEthernet) {
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Device Is gettingg Reset. ESP will restart.\"}");
        Serial.println("Configuration reset successful.");
        delay(2000);
        ESP.restart();
    } else {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to Resett Device.\"}");
        Serial.println("Failed to reset configuration.");
    }
});




  server.begin();
  if (WiFi.getMode() == WIFI_MODE_STA){
    reconnect();
  }

  if (modbusParityConfig == "Serial_7N1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7N1, 16, 17);
  } else if (modbusParityConfig == "Serial_7N2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7N2, 16, 17);
  } else if (modbusParityConfig == "Serial_7E1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7E1, 16, 17);
  } else if (modbusParityConfig == "Serial_7E2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7E2, 16, 17);
  } else if (modbusParityConfig == "Serial_7O1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7O1, 16, 17);
  } else if (modbusParityConfig == "Serial_7O2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_7O2, 16, 17);
  } else if (modbusParityConfig == "Serial_8N1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8N1, 16, 17);
  } else if (modbusParityConfig == "Serial_8N2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8N2, 16, 17);
  } else if (modbusParityConfig == "Serial_8E1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8E1, 16, 17);
  } else if (modbusParityConfig == "Serial_8E2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8E2, 16, 17);
  } else if (modbusParityConfig == "Serial_8O1") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8O1, 16, 17);
  } else if (modbusParityConfig == "Serial_8O2") {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8O2, 16, 17);
  } else {
    modbusSerial.begin(modbusBaudRate, SWSERIAL_8N1, 16, 17);
  }

  
  // Initialize Modbus communication
  uint8_t modbusSlaveIdConfig = modbusSlaveId;
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);
  node.begin(modbusSlaveIdConfig, modbusSerial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  // Read data based on modbusPointType
}

void modbusReadStore(){
    DynamicJsonDocument jsonDoc(1024);
  
  for (int i = 0; i < numRegisters; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      jsonDoc[registerNames[i]] = floatValue;
//      Serial.print("Register: ");
//      Serial.print(registerNames[i]);
//      Serial.print(", Value: ");
//      Serial.println(floatValue);
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab[i]);
    }
    delay(50);
  }
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  bool success = writeFile("/data.txt", jsonString.c_str());
  if(success){
    Serial.println("Data Saved Successfully!");
  } else {
    Serial.println("Data not saving Getting Issue in the Data Storing!");
  }

  String dataPrint = readFile("/data.txt");
  Serial.println(dataPrint);
}

void publishData(){
  if (!mqttConnected) {
    reconnect();
  }

  if (mqttConnected) {      
      String data = readFile("/data.txt");

      if (client.publish(mqttTopic, data.c_str())) {
        Serial.println("Data published to MQTT");
      } else {
        int mqttState = client.state();
        Serial.print("Data publication failed, rc=");
        Serial.println(mqttState);
        
        // Print a more detailed error message based on the MQTT state
        switch (mqttState) {
          case -4:
            Serial.println("MQTT_CONNECTION_TIMEOUT");
            break;
          case -3:
            Serial.println("MQTT_CONNECTION_LOST");
            break;
          case -2:
            Serial.println("MQTT_CONNECT_FAILED");
            break;
          case -1:
            Serial.println("MQTT_DISCONNECTED");
            break;
          case 1:
            Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
            break;
          case 2:
            Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
            break;
          case 3:
            Serial.println("MQTT_CONNECT_UNAVAILABLE");
            break;
          case 4:
            Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
            break;
          case 5:
            Serial.println("MQTT_CONNECT_UNAUTHORIZED");
            break;
          default:
            Serial.println("Unknown error");
            break;
        }
        
        Serial.println("Retrying...");
        reconnect();
      }
    }
}

void loop() {
  publishData();
}
