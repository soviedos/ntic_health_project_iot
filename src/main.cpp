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

/////////////// Sub Routine for time syncronization ///////////////
void initTime()
{
    WiFiUDP _udp;

    time_t epochTime = (time_t)-1;

    NTPClient ntpClient;
    ntpClient.begin();

    while (true)
    {
        epochTime = ntpClient.getEpochTime("pool.ntp.org");

        if (epochTime == (time_t)-1)
        {
            LogInfo("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            LogInfo("Fetched NTP epoch time is: %lu", epochTime);
            break;
        }
    }

    ntpClient.end();

    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);
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

bool readMessage(int messageId, char *payload)
{
    float temperature = readTemperature();
    float humidity = readHumidity();
    StaticJsonBuffer<MESSAGE_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["deviceId"] = DEVICE_ID;
    root["messageId"] = messageId;
    bool result = false;

    // NAN is not the valid json, change it to NULL
    if (std::isnan(temperature))
    {
        root["temperature"] = NULL;
    }
    else
    {
        root["temperature"] = temperature;
        if(temperature > TEMPERATURE_ALERT)
        {
            result = true;
        }
    }

    if (std::isnan(humidity))
    {
        root["humidity"] = NULL;
    }
    else
    {
        root["humidity"] = humidity;
    }
    //root.printTo(payload, MESSAGE_MAX_LEN);
    return result;
}

////////////////////////////// Connection with Azure IoT //////////////////////////////
static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    if (IOTHUB_CLIENT_CONFIRMATION_OK == result)
    {
        LogInfo("Message sent to Azure IoT Hub");
    }
    else
    {
        LogInfo("Failed to send message to Azure IoT Hub");
    }
    messagePending = false;
}

static void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert)
{
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)buffer, strlen(buffer));
    if (messageHandle == NULL)
    {
        LogInfo("unable to create a new IoTHubMessage");
    }
    else
    {
        MAP_HANDLE properties = IoTHubMessage_Properties(messageHandle);
        Map_Add(properties, "temperatureAlert", temperatureAlert ? "true" : "false");
        LogInfo("Sending message: %s", buffer);
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, NULL) != IOTHUB_CLIENT_OK)
        {
            LogInfo("Failed to hand over the message to IoTHubClient");
        }
        else
        {
            messagePending = true;
            LogInfo("IoTHubClient accepted the message for delivery");
        }

        IoTHubMessage_Destroy(messageHandle);
    }
}

IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char *buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        LogInfo("unable to IoTHubMessage_GetByteArray\r\n");
        result = IOTHUBMESSAGE_REJECTED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char temp[size + 1];
        memcpy(temp, buffer, size);
        temp[size] = '\0';
        LogInfo("Receiving message: %s", temp);
        result = IOTHUBMESSAGE_ACCEPTED;
    }

    return result;
}
////////////////////////////// End of Azure IoT connection Sub Routine//////////////////////////////

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

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

    initTime();

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, HTTP_Protocol);
    if (iotHubClientHandle == NULL)
    {
        LogInfo("Failed on IoTHubClient_CreateFromConnectionString");
        while (1);
    }

    // Because it can poll "after 2 seconds" polls will happen
    // effectively at ~3 seconds.
    // Note that for scalabilty, the default value of minimumPollingTime
    // is 25 minutes. For more information, see:
    // https://azure.microsoft.com/documentation/articles/iot-hub-devguide/#messaging
    int minimumPollingTime = 2;
    if (IoTHubClient_LL_SetOption(iotHubClientHandle, "MinimumPollingTime", &minimumPollingTime) != IOTHUB_CLIENT_OK)
    {
        LogInfo("failure to set option \"MinimumPollingTime\"\r\n");
    }

    IoTHubClient_LL_SetOption(iotHubClientHandle, "product_info", "HappyPath_FeatherM0WiFi-C");
    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, NULL);
}

int messageCount = 1;

// the loop function runs over and over again until power down or reset
void loop() {
  readTemperature(); // Calls temperature reading sub routine
  readHumidity(); // Calls humidity reading sub routine

  if (!messagePending)
    {
        char messagePayload[MESSAGE_MAX_LEN];
        bool temperatureAlert = readMessage(messageCount, messagePayload);
        sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
        messageCount++;
        delay(INTERVAL);
    }
    IoTHubClient_LL_DoWork(iotHubClientHandle);
}