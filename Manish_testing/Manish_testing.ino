#define TINY_GSM_MODEM_SIM800
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <TinyGsmClient.h>

#define MAX485_DE D3
#define MAX485_RE_NEG D0

SoftwareSerial modbusSerial(D4, D2);
SoftwareSerial SerialGSM(D1, D7); // RX, TX
int mqttConnectAttempts = 0;


ModbusMaster node;

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

float values[numRegisters];

TinyGsm modemGSM(SerialGSM);
TinyGsmClient gsmClient(modemGSM);

PubSubClient client("broker.emqx.io", 1883, gsmClient);

int GREEN_LED = 5; // Example: GPIO 5 for the indicator LED
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
}

void connectMQTTServer() {
   // Attempt to connect to MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("SL2023testing")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000); // Wait for 2 seconds before the next attempt
      mqttConnectAttempts++;

      if (mqttConnectAttempts >= 5) {
        Serial.println("Exceeded maximum MQTT connection attempts. Restarting ESP...");
        ESP.restart(); // Restart the ESP
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  modbusSerial.begin(19200, SWSERIAL_8N1, D4, D2);
  pinMode(MAX485_DE, OUTPUT);
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_DE, LOW);
  digitalWrite(MAX485_RE_NEG, LOW);

  node.begin(1, modbusSerial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  setupGSM();
  connectMQTTServer();
}

void loop() {
  // If disconnected from the MQTT server, reconnect
  if (!client.connected()) {
    connectMQTTServer();
  }

  // Create a JSON object
  DynamicJsonDocument jsonDoc(3072); // Adjust the size as needed

  // Add the extra variables
  jsonDoc["api_key"] = "SL-2024";
  jsonDoc["device_uid"] = "SLTEST"; 


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
    }
    delay(50);
  }

  // Serialize the JSON objects to strings
  String jsonString1;
  serializeJson(jsonDoc, jsonString1);


  // Publish the JSON strings via MQTT to different topics
  publishMQTT("MQTT/MANISH/1", jsonString1);
 
  delay(5000); // Delay between readings
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
}
