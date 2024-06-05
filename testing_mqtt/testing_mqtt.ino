#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "GHRTBIF";
const char* password = "Global@2023";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "sense/test/mqtt";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  
  // Generate random JSON data
  String json_data = "{\"temperature\":" + String(random(0, 100)) + ",\"humidity\":" + String(random(0, 100)) + ",\"pressure\":" + String(random(0, 100)) + ",\"faheritnite\":" + String(random(0, 100)) + ",\"humidity\":" + String(random(0, 100)) + ",\"pressure\":" + String(random(0, 100)) + ",\"faheritnite\":" + String(random(0, 100)) + "}";
  
  // Publish the JSON data to the MQTT topic
  client.publish(mqtt_topic, json_data.c_str());
  Serial.println("Published: " + json_data);
}
