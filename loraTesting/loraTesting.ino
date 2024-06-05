#include <SoftwareSerial.h>

SoftwareSerial e32Serial(2, -1);  // RX pin (2) only

void setup() {
  Serial.begin(9600);
  e32Serial.begin(9600);
}

void loop() {
  // Send a message to the E32-LoRa module
  sendMessage("Hello, E32!");

  // Wait for a moment before sending the next message
  delay(1000);
}

void sendMessage(const char* message) {
  e32Serial.print(message);
  e32Serial.write(0xFF);  // End of message character, adjust as needed
}
