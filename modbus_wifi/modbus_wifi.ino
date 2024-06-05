#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <ArduinoJson.h>
#include <WiFi.h> // Include the WiFi library
#include <PubSubClient.h>

#define MAX485_DE 2
#define MAX485_RE_NEG 4

SoftwareSerial modbusSerial(16, 17);
ModbusMaster node;

String ssid = "Airtel_SenseLive";       // Change to your WiFi SSID
String password = "SL123456"; // Change to your WiFi password
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char* mqttUsername = "kaushal";
const char* mqttPassword = "kaushal";
const char* mqttTopic = "sense/modbus/SLTEST";

WiFiClient wifiClient; // Use WiFiClient instead of GSMClient
PubSubClient client(wifiClient);

int status = 0; // 0: Not published, 1: Published successfully, -1: Failed to publish

union u_tag {
  uint16_t bdata[2];
  float floatValue;
} unionFloat;

int tab[] = {0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,102,124,126,128,130,132,134,136,138,140};
const char* registerNames[] = {
  "voltage_1n","voltage_2n","voltage_3n","voltage_N","voltage_12","voltage_23","voltage_31","voltage_L","current_1","current_2","current_3","current","kw_1",
  "kw_2","kw_3","kvar_1","kvar_2","kvar_3","kva_1","kva_2","kva_3","pf_1","pf_2","pf_3","pf","freq","kw","kvar","kva","max_kw","min_kw","max_kvar","min_kvar","max_kva",
  "max_int_v1n","max_int_v2n","max_int_v3n","max_int_v12","max_int_v23","max_int_v31","max_int_i1","max_int_i2","max_int_i3","imp_kwh","exp_kwh","kwh","imp_kvarh",
  "exp_kvarh","kvarh","kvah","run_h","on_h","thd_v1n","thd_v2n","thd_v3n","thd_v12","thd_v23","thd_v31","thd_i1","thd_i2","thd_i3"
};

const int numRegisters = sizeof(tab) / sizeof(tab[0]);

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH);
  digitalWrite(MAX485_DE, HIGH);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW);
  digitalWrite(MAX485_DE, LOW);
}

void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println("Connected to WiFi");

}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqttUsername, mqttPassword)) {
      Serial.println("connected");
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
  modbusSerial.begin(19200, SWSERIAL_8N1, 16, 17);
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);

  node.begin(1, modbusSerial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  connectWiFi();
  client.setServer(mqttServer, mqttPort);
}

void loop() {
  DynamicJsonDocument jsonDoc(10240);  // Adjust the size as needed

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
    } else {
      Serial.print("Failed to read register ");
      Serial.println(tab[i]);
    }
    delay(100);
  }

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  Serial.println("JSON data:");
  Serial.println(jsonString);

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (client.connected()) {
    if (client.publish(mqttTopic, jsonString.c_str())) {
      Serial.println("Published to MQTT");
      status = 1; // Published successfully
    } else {
      Serial.println("Failed to publish to MQTT");
      status = -1; // Failed to publish
    }
  } else {
    status = 0; // Not published, MQTT not connected
  }

  // Print the status
  if (status == 1) {
    Serial.println("Status: Published successfully");
  } else if (status == -1) {
    Serial.println("Status: Failed to publish");
  } else {
    Serial.println("Status: Not published, MQTT not connected");
  }

  delay(5000); // Delay between readings
}
