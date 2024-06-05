#include <Arduino.h>
#include <SIM76xx.h>
#include <GSMClient.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 27                   // Define the pin where the DHT22 is connected
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define TOPIC "/v1.6/devices/esp32_gprs"
#define MQTT_USERNAME "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"
#define USE_WIFI true               // Set to true to use Wi-Fi, false to use GSM
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

#if USE_WIFI
WiFiClient wifi_client;
PubSubClient client(wifi_client);
#else
GSMClient gsm_client;
PubSubClient client(gsm_client);
#endif

void callback(char* topic, byte* payload, unsigned int length) {
                    // Your existing callback code...
}

void connectWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected to Wi-Fi");
}

void connectGSM() {
  Serial.println("Connecting to GSM...");
  while (!GSM.begin()) {
    Serial.println("GSM setup fail");
    delay(1000);
  }
  Serial.println("Connected to GSM");
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client", MQTT_USERNAME, MQTT_PASSWORD)) {
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
  Serial.println("Hello!");

#if USE_WIFI
  connectWiFi();
#else
  connectGSM();
#endif

  client.setServer("broker.emqx.io", 1883);
  client.setCallback(callback);

  dht.begin();  // Initialize DHT sensor
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read DHT22 data
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (!isnan(humidity) && !isnan(temperature)) {
    // Convert float values to string
    String temperatureString = String(temperature);
    String humidityString = String(humidity);

    // Publish temperature and humidity to MQTT topics
    client.publish("temperatureTopic", temperatureString.c_str());
    client.publish("humidityTopic", humidityString.c_str());

    Serial.print("Published Temperature: ");
    Serial.println(temperatureString);
    Serial.print("Published Humidity: ");
    Serial.println(humidityString);
  } else {
    Serial.println("Failed to read from DHT sensor");
  }

  delay(5000); // Delay for 5 seconds before sending the next update
}
