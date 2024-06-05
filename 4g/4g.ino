#include <Arduino.h>
#include <SIM76xx.h>
#include <GSMClient.h>
#include <PubSubClient.h>

#define TOPIC "/v1.6/devices/esp32_gprs"
#define MQTT_USERNAME "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"             

GSMClient gsm_client;
PubSubClient client(gsm_client);

void callback(char* topic, byte* payload, unsigned int length) {
                    // Your existing callback code...
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

  connectGSM();


  client.setServer("broker.emqx.io", 1883);
  client.setCallback(callback);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(5000); // Delay for 5 seconds before sending the next update
}
