#include <Arduino.h> // Library for Arduino compatibility
#include <AzureIoTHub.h> // Library for Azure IoT Hub
#include "WiFi101.h" // Library for WiFi module
#include "credentials.h"  // WiFi Router credentials
#include <time.h>
#include <sys/time.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h> // Graphics core library
#include <Adafruit_SSD1306.h> // Library for Monochrome OLEDs display
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h> // JSON library for Arduino and IoT
#include <AzureIoTProtocol_HTTP.h>
#include <NTPClient.h>
#include "config.h"

// Display instance
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// Oled FeatherWings buttons map
#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

// WiFi credentials
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PASS = SECRET_PASS;

// WiFi parameter
int status = WL_IDLE_STATUS;
char server[] = "www.google.com";    // name address for Google (using DNS)
long rssi;

// WiFi Instance
WiFiSSLClient client; // WiFi connection with SSL

// Configuring network parameters
IPAddress ip(192,168,0,5);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);
IPAddress dns(8,8,8,8);

// Azure parameters
static bool messagePending = false;
const char* connectionString = CONN_STRING;

// Sensor parameters
#define DHTPIN 6     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT11     // DHT 11

// Sensor instance
DHT_Unified dht(DHTPIN, DHTTYPE);

// Configuring battery measuring pin
#define VBATPIN A7

/////////////// Sub Routine to measure battery voltage ///////////////
void batteryMeasure() {
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.clearDisplay();
    display.print("VBat: "); display.println(measuredvbat);
    display.display();
}

/////////////// Sub Routine to print WiFi data ///////////////
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  // Display values
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);
  display.print("WiFi connected ");
  display.println(WIFI_SSID);
  display.print("Ip: ");
  ip = WiFi.localIP();
  display.println(ip);
  display.print("Gateway: ");
  gateway = WiFi.gatewayIP();
  display.println(gateway);
  rssi = WiFi.RSSI();
  display.print("Signal: ");
  display.print(rssi);
  display.println(" dBm");
  display.display();
}

/////////////// Sub Routine for the batteryMeasure and printWiFiStatus interruption ///////////////
void interruption() {
  if (!digitalRead(BUTTON_B)) printWiFiStatus();
  if (!digitalRead(BUTTON_C)) batteryMeasure();
  delay(2000);
}

/////////////// Sub Routine for initialization of DTH 11 sensor ///////////////
void initSensor() {
  dht.begin();
}

/////////////// Sub Routine for temperature reading ///////////////
void readTemperature()
{
  interruption();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.display();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    }
    else {
      display.print("Temperature: ");
      display.print(event.temperature);
      display.println(" C");
      display.display();
      delay(2000);
    }
}

/////////////// Sub Routine for humidity reading ///////////////
void readHumidity()
{
  interruption();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.display();
  sensor_t sensor;
  dht.humidity().getSensor(&sensor);
  sensors_event_t event;
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity"));
    }
    else {
      display.print("Humidity: ");
      display.print(event.relative_humidity);
      display.println(" %");
      display.display();
      delay(2000);
    }
}


// the setup function runs once when you press reset or power the board
void setup() {

    //Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8,7,4,2);

    // Serial Port initialization
    Serial.begin(115200); // Init Serial Connection

    // Initialization for DHT_11 sensor
    initSensor();
    
    // Display initialization
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
    display.display();
    delay(2000);

    // Clear Display buffer
    display.clearDisplay();
    display.display();

    // Board's buttons pinout
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);

    // Init WiFi connection
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("Connecting to ");
    display.println(WIFI_SSID);
    display.display();

    // WiFi Init
    while (status != WL_CONNECTED) {
        display.println("Connecting...");
        display.display();
        WiFi.config(ip, dns,gateway, subnet);
        status = WiFi.begin(WIFI_SSID, WIFI_PASS);
        delay(2000);
    }

    // Show WiFi Status
    printWiFiStatus();
    delay(2000);


}


// the loop function runs over and over again until power down or reset
void loop() {
    
  readTemperature(); // Calls temperature reading sub routine
  readHumidity(); // Calls humidity reading sub routine

}