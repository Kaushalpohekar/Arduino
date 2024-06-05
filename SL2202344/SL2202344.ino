#define TINY_GSM_MODEM_SIM800
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <TinyGsmClient.h>
#include <WiFiManager.h> // Include the WiFiManager library
#include <FS.h>

#define MAX485_DE D3
#define MAX485_RE_NEG D0

SoftwareSerial modbusSerial(D4, D2);
SoftwareSerial SerialGSM(D1, D7); // RX, TX
int mqttConnectAttempts = 0;


String savedDeviceID = "SL02202344";
int savedDelay = 120000;  // Default delay value
char deviceID[32] = "SL02202344"; // Default device ID
// Initialize WiFiManager
  WiFiManager wifiManager;
  int dataSendingDelay = 120000;
  // Create a custom input field for the device ID
WiFiManagerParameter deviceIDParam("deviceid", "Device ID", deviceID, 32);
WiFiManagerParameter delayParam("delay", "Data Sending Delay (ms)", "120000", 6);



ModbusMaster node;

union u_tag {
  uint16_t bdata[2];
  float floatValue;
} unionFloat;

int tab[] = {0,2,4,6,8,10,12,14,16};
const char* registerNames[] = {
  "voltage_1n","voltage_2n","voltage_3n","voltage_N","voltage_12","voltage_23","voltage_31","voltage_L","current_1"
};

int tab2[] = {18,20,22,24,26,28,30,32,34,36,38}; 
const char* registerNames2[] = {"current_2","current_3","current","kw_1","kw_2","kw_3","kvar_1","kvar_2","kvar_3","kva_1","kva_2"};

int tab3[] = {40,42,44,46,48,50,52,54,56,58};
const char* registerNames3[] = {"kva_3","pf_1","pf_2","pf_3","pf","freq","kw","kvar","kva","max_kw"};

int tab4[] = {60,62,64,66,68,70,72,74,76,78};
const char* registerNames4[] = {"min_kw","max_kvar","min_kvar","max_kva","max_int_v1n","max_int_v2n","max_int_v3n","max_int_v12","max_int_v23","max_int_v31"};

int tab5[] = {80,82,84,86,88,90,92,94,96,98};
const char* registerNames5[] = {"max_int_i1","max_int_i2","max_int_i3","imp_kwh","exp_kwh","kwh","imp_kvarh","exp_kvarh","kvarh","kvah"};

int tab6[] = {100,102,124,126,128,130,132,134,136,138,140};
const char* registerNames6[] = {"run_h","on_h","thd_v1n","thd_v2n","thd_v3n","thd_v12","thd_v23","thd_v31","thd_i1","thd_i2","thd_i3"};

const int numRegisters = sizeof(tab) / sizeof(tab[0]);
const int numRegisters2 = sizeof(tab2) / sizeof(tab2[0]);
const int numRegisters3 = sizeof(tab3) / sizeof(tab3[0]);
const int numRegisters4 = sizeof(tab4) / sizeof(tab4[0]);
const int numRegisters5 = sizeof(tab5) / sizeof(tab5[0]);
const int numRegisters6= sizeof(tab6) / sizeof(tab6[0]);

float values[numRegisters];
float values2[numRegisters2];
float values3[numRegisters3];
float values4[numRegisters4];
float values5[numRegisters5];
float values6[numRegisters6];

TinyGsm modemGSM(SerialGSM);
TinyGsmClient gsmClient(modemGSM);

PubSubClient client("broker.emqx.io", 1883, gsmClient);

int GREEN_LED = D5; // Example: GPIO 5 for the indicator LED
int BLUE_LED = D6;
uint32_t lastTime = 0;

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW);
  digitalWrite(MAX485_DE, LOW);
}

void setupGSM() {
  Serial.println("\n Setup GSM...");
  SerialGSM.begin(9600);
  delay(3000);
  Serial.println(modemGSM.getModemInfo());
  if (!modemGSM.restart()) {
    Serial.println("Restarting GSM Modem failed");
    delay(1000);
    ESP.restart();
    return;
  }
  if (!modemGSM.waitForNetwork()) {
    Serial.println("Failed to connect to the network");
    delay(1000);
    ESP.restart();
    return;
  }
  if (!modemGSM.gprsConnect("", "", "")) {
    Serial.println("GPRS Connection Failed");
    delay(1000);
    ESP.restart();
    return;
  }
  Serial.println("Setup GSM Success");
   blinkLED(BLUE_LED, 200, 3);

   
  //digitalWrite(BLUE_LED, HIGH);
}

void connectMQTTServer() {
   // Attempt to connect to MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("SL02202344")) {
      Serial.println("connected");
     digitalWrite(BLUE_LED, HIGH);
 

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
       digitalWrite(BLUE_LED, LOW);
      delay(2000); // Wait for 2 seconds before the next attempt
      mqttConnectAttempts++;

      if (mqttConnectAttempts >= 5) {
        Serial.println("Exceeded maximum MQTT connection attempts. Restarting ESP...");
        ESP.restart(); // Restart the ESP
      }
    }
  }
}


void saveConfigCallback() {
  // Save the custom parameters to deviceID and delay
  strcpy(deviceID, deviceIDParam.getValue());
  //savedDeviceID = deviceIDParam.getValue();
  savedDelay = atoi(delayParam.getValue());

  // Save the updated configuration to SPIFFS
  DynamicJsonDocument jsonDoc(1024);  // Adjust the size as needed
  jsonDoc["deviceID"] = deviceID;
  jsonDoc["delay"] = savedDelay;

  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(jsonDoc, configFile);
    configFile.close();
  }
}


void read() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (configFile) {
    size_t size = configFile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    configFile.close();

    DynamicJsonDocument jsonDoc(1024);  // Adjust the size as needed
    deserializeJson(jsonDoc, buf.get());

    //savedDeviceID = jsonDoc["deviceID"].as<String>();
   // savedDelay = jsonDoc["delay"].as<int>();
  }
}



void setup() {
  Serial.begin(115200);
  modbusSerial.begin(19200, SWSERIAL_8N1, D4, D2);
  pinMode(MAX485_DE, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);

  node.begin(1, modbusSerial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  
  // Create a custom input field for the device ID
WiFiManagerParameter deviceIDParam("deviceid", "Device ID", deviceID, 32);
  wifiManager.addParameter(&delayParam);
  wifiManager.addParameter(&deviceIDParam);
  // Set a callback to save the custom parameters
  wifiManager.setSaveConfigCallback(saveConfigCallback);
   // Set the WiFi configuration portal timeout to 2 minutes (120 seconds)
  wifiManager.setTimeout(120);  // Set timeout in seconds
  
   blinkLED(BLUE_LED, 200, 3);
   
   blinkLED(GREEN_LED, 200, 3);
  // Connect to WiFi or create a configuration portal if not connected
  if (!wifiManager.autoConnect("SenseLive.in")) {
    Serial.println("Failed to connect or configure WiFi");
    // Handle failed WiFi connection here (e.g., restart or set default values)
    //ESP.restart();
   blinkLED(BLUE_LED, 200, 3);
   blinkLED(GREEN_LED, 200, 3);
  }

  // Get the device ID from the custom parameter
  strcpy(deviceID, deviceIDParam.getValue());
  
  // Read the data sending delay input
  dataSendingDelay = atoi(delayParam.getValue());

  Serial.print("device id=");
  Serial.println(deviceID);
  Serial.print("Data sending Delay=");
  Serial.println(dataSendingDelay);
  
  


  setupGSM();
  connectMQTTServer();
}

void loop() {
  // If disconnected from the MQTT server, reconnect
  if (!client.connected()) {
    connectMQTTServer();
  }
   

  // Create a JSON object
  DynamicJsonDocument jsonDoc(512); // Adjust the size as needed
  DynamicJsonDocument jsonDoc2(512);
  DynamicJsonDocument jsonDoc3(512);
  DynamicJsonDocument jsonDoc4(512); // Adjust the size as needed
  DynamicJsonDocument jsonDoc5(512);
  DynamicJsonDocument jsonDoc6(512);

  // Add the extra variables
  jsonDoc["api_key"] = "2024";
  jsonDoc["device_uid"] =  savedDeviceID;
  jsonDoc2["api_key"] = "2024";
  jsonDoc2["device_uid"] =  savedDeviceID;
  jsonDoc3["api_key"] = "2024";
  jsonDoc3["device_uid"] =  savedDeviceID;
  jsonDoc4["api_key"] = "2024";
  jsonDoc4["device_uid"] =  savedDeviceID;
  jsonDoc5["api_key"] = "2024";
  jsonDoc5["device_uid"] =  savedDeviceID;
  jsonDoc6["api_key"] = "2024";
  jsonDoc6["device_uid"] =  savedDeviceID;
 

   for (int i = 0; i < numRegisters; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc[registerNames[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab[i]);
        digitalWrite(GREEN_LED, HIGH);
    }
     delay(40);
  }
  
   if (!client.connected()) {
    connectMQTTServer();
  }
  // Serialize the JSON objects to strings
  String jsonString1;
  serializeJson(jsonDoc, jsonString1);
 // Publish the JSON strings via MQTT to different topics
  publishMQTT("Energy/SenseLive/SL02202344/1", jsonString1);
  Serial.println(jsonString1);


  for (int i = 0; i < numRegisters2; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab2[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc2[registerNames2[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab2[i]);
        digitalWrite(GREEN_LED, HIGH);
    }
     delay(40);
  }
 if (!client.connected()) {
    connectMQTTServer();
  }
 
  String jsonString2;
  serializeJson(jsonDoc2, jsonString2);
 
  publishMQTT("Energy/SenseLive/SL02202344/2", jsonString2);
  Serial.println(jsonString2);

  for (int i = 0; i < numRegisters3; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab3[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc3[registerNames3[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab3[i]);
        digitalWrite(GREEN_LED, HIGH);
    }
    delay(40);
  }

  if (!client.connected()) {
    connectMQTTServer();
  }
 
  String jsonString3;
  serializeJson(jsonDoc3, jsonString3);

 publishMQTT("Energy/SenseLive/SL02202344/3", jsonString3);
 Serial.println(jsonString3);
 

  for (int i = 0; i < numRegisters4; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab4[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc4[registerNames4[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab4[i]);
        digitalWrite(GREEN_LED, HIGH);
    }
    delay(40);
  }
 if (!client.connected()) {
    connectMQTTServer();
  }
 
String jsonString4;
  serializeJson(jsonDoc4, jsonString4);

  publishMQTT("Energy/SenseLive/SL02202344/4", jsonString4);
 Serial.println(jsonString4);
  
  for (int i = 0; i < numRegisters5; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab5[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc5[registerNames5[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab5[i]);
        digitalWrite(GREEN_LED, HIGH);
    }
    delay(40);
  }

 if (!client.connected()) {
    connectMQTTServer();
  }
 
 String jsonString5;
  serializeJson(jsonDoc5, jsonString5);
  publishMQTT("Energy/SenseLive/SL02202344/5", jsonString5);
 Serial.println(jsonString5);

  for (int i = 0; i < numRegisters6; ++i) {
    int j = i;
    uint16_t result = node.readInputRegisters(tab6[i] - 1, 2);

    if (result == node.ku8MBSuccess) {
      uint16_t highWord = node.getResponseBuffer(0x00);
      uint16_t lowWord = node.getResponseBuffer(0x01);

      float floatValue;
      memcpy(&floatValue, &highWord, sizeof(uint16_t));
      memcpy(((char *)&floatValue) + sizeof(uint16_t), &lowWord, sizeof(uint16_t));

      // Assign the value to a named key in the JSON object
      jsonDoc6[registerNames6[i]] = floatValue;
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab6[i]);
         digitalWrite(GREEN_LED, HIGH);
    }
    delay(40);
  }

 if (!client.connected()) {
    connectMQTTServer();
  }
  String jsonString6;
  serializeJson(jsonDoc6, jsonString6);

  publishMQTT("Energy/SenseLive/SL02202344/6", jsonString6);
 Serial.println(jsonString6);
 
  
  delay(dataSendingDelay); // Delay between readings
}

void publishMQTT(const char* topic, const String& jsonString) {
  // Publish the JSON string via MQTT
  char charBuf[jsonString.length() + 1]; // Create a char array to hold the JSON string
  jsonString.toCharArray(charBuf, jsonString.length() + 1); // Convert the String to char array

  Serial.print("Publishing JSON message to topic: ");
  Serial.println(topic);

  int status = client.publish(topic, charBuf); // Publish the char array to the specified topic
  Serial.print("Publish Status: ");
  Serial.println(status);
  
  // Blink the GREEN_LED when data is published
  blinkLED(GREEN_LED, 100, 3); // Blink once with a 100ms interval

}

void blinkLED(int pin, int interval, int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(pin, HIGH);
    delay(interval);
    digitalWrite(pin, LOW);
    delay(interval);
  }
}
