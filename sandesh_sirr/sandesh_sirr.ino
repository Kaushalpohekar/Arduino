#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <WiFiManager.h>

#define LED_IND D5 // opto D6=DATA ENTRY INDICATOR HTTP, new gsm device D5
#define LED_BLUE D6 // network HTTP INDICATOR, new gsm device D6

#define rxPin D1
#define txPin D7
SoftwareSerial sim800L(rxPin, txPin);

const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
String apiKeyValue = "SL-2024";

const int sensorCount = 3; // Number of DS18B20 sensors
DeviceAddress sensorAddresses[sensorCount] = {
    {0x28, 0xB8, 0x56, 0x57, 0x04, 0xE1, 0x3C, 0xDD}, // Sensor 1 address
    {0x28, 0x62, 0xA7, 0x57, 0x04, 0xE1, 0x3C, 0xBD}, // Sensor 2 address
    {0x28, 0x7E, 0x60, 0x57, 0x04, 0xE1, 0x3C, 0xE8}  // Sensor 3 address
};

void setup()
{
    WiFi.mode(WIFI_STA);
    Serial.begin(115200);
    pinMode(LED_IND, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    sim800L.begin(9600);

    Serial.println("Initializing...");
    sendATcommand("AT", "OK", 2000);
    sendATcommand("AT+CMGF=1", "OK", 2000);
}

void loop()
{
    for (int i = 0; i < sensorCount; i++)
    {
        float temperatureC = readTemperature(i);
        Serial.print("Sensor ");
        Serial.print(i + 1);
        Serial.print(" Temperature: ");
        Serial.print(temperatureC);
        Serial.println(" °C");

        // Send temperature data or perform any desired actions here
        // Example: sendDataToServer(temperatureC);

        delay(5000); // Wait for 5 seconds before reading the next sensor
    }
}

float readTemperature(int sensorIndex)
{
    sensors.requestTemperaturesByAddress(sensorAddresses[sensorIndex]);
    return sensors.getTempCByIndex(0);
}

int8_t sendATcommand(char *ATcommand, char *expected_answer, unsigned int timeout)
{
    uint8_t x = 0, answer = 0;
    char response[100];
    unsigned long previous;

    memset(response, '\0', 100);
    delay(100);

    while (sim800L.available() > 0)
        sim800L.read();

    if (ATcommand[0] != '\0')
    {
        sim800L.println(ATcommand);
    }

    x = 0;
    previous = millis();

    do
    {
        if (sim800L.available() != 0)
        {
            response[x] = sim800L.read();
            Serial.print(response[x]);
            x++;

            if (strstr(response, expected_answer) != NULL)
            {
                answer = 1;
            }
        }
    } while ((answer == 0) && ((millis() - previous) < timeout));

    return answer;
}
