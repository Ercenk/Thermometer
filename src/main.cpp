#include <Arduino.h>
#include <Wire.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <ArduinoJSON.h>

// Wifi manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>

// OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>

////// Device identification

const String DeviceType = "thermometer";
const String deviceId = "wifitherm1";
#define WIFICONFIG 0 // use for WiFiManager captive portal setup,

///////// Wi-Fi stuff
WiFiManager wifiManager;
WiFiClient client;

String host;
String portString;
int port;

char hostParam[40];
char portParam[40];

WiFiManagerParameter host_parameter("server", "host", hostParam, 40);
WiFiManagerParameter port_parameter("port", "1880", portParam, 40);

//????? DHT 22
#define DHTPIN 12

#define DHTTYPE DHT22
DHT_Unified dhtUnified(DHTPIN, DHTTYPE);
// Bring the other for heat index and temp conversion
DHT dht(DHTPIN, DHTTYPE);

float temp = -100;
float humidity = 0;
float heatIndex = 0;

float oldTemp = -100;
float oldHumidity = 0;
float oldHeatIndex = 0;

uint32_t delayMS;

// Display
#define OLED_pin_scl_sck 14
#define OLED_pin_sda_mosi 16
#define OLED_pin_cs_ss 4
#define OLED_pin_res_rst 5
#define OLED_pin_dc_rs 13

const uint16_t OLED_Color_Black = 0x0000;
const uint16_t OLED_Color_Blue = 0x001F;
const uint16_t OLED_Color_Red = 0xF800;
const uint16_t OLED_Color_Green = 0x07E0;
const uint16_t OLED_Color_Cyan = 0x07FF;
const uint16_t OLED_Color_Magenta = 0xF81F;
const uint16_t OLED_Color_Yellow = 0xFFE0;
const uint16_t OLED_Color_White = 0xFFFF;

uint16_t OLED_Text_Color = OLED_Color_Blue;
uint16_t OLED_Backround_Color = OLED_Color_Black;

Adafruit_SSD1331 oled =
    Adafruit_SSD1331(
        OLED_pin_cs_ss,
        OLED_pin_dc_rs,
        OLED_pin_sda_mosi,
        OLED_pin_scl_sck,
        OLED_pin_res_rst);

// assume the display is off until configured in setup()
bool isDisplayVisible = false;

// declare size of working string buffers. Basic strlen("d hh:mm:ss") = 10
const size_t MaxString = 16;

// the string being displayed on the SSD1331 (initially empty)
String oldDisplayString;

unsigned long sentTime = 0;
// Settings file

void setupFileSystem()
{
  if (LittleFS.begin())
  {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");

        size_t size = configFile.size();
        if (size > 1024)
        {
          Serial.println("Config file size is too large");
          return;
        }

        Serial.print("Size is: ");
        Serial.println(size);
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        StaticJsonDocument<200> doc;
        auto error = deserializeJson(doc, buf.get());
        if (error)
        {
          Serial.println("Failed to parse config file");
          return;
        }

        const char *hostBuffer = doc["host"];
        host = hostBuffer;

        const char *portBuffer = doc["port"];
        portString = portBuffer;
        port = portString.toInt();
        if (port == 0)
        {
          Serial.println("Cannot parse port number.");
          port = 1880;
        }

        Serial.print("File is: ");
        Serial.println(host);

        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
}

void configureWiFi()
{
  wifiManager.addParameter(&host_parameter);
  wifiManager.addParameter(&port_parameter);

  wifiManager.startConfigPortal(deviceId.c_str());
  Serial.println("connected...yeey :)");

  host = host_parameter.getValue();
  Serial.println(host);

  portString = port_parameter.getValue();
  Serial.println(portString);

  StaticJsonDocument<200> doc;

  doc["host"] = host;
  doc["port"] = portString;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJson(doc, configFile);
}

void displayValues()
{
  String newDisplayString = "";

  newDisplayString = String(temp, 2) + "F\n" + String(humidity, 2) + "\n" + String(heatIndex, 2) + "F";

  try
  {
    if (oldTemp != temp || oldHeatIndex != heatIndex || oldHumidity != humidity)
    {
      oled.setCursor(0, 0);
      oled.setTextColor(OLED_Backround_Color);
      oled.print(oldDisplayString);

      oled.setCursor(0, 0);
      oled.setTextColor(OLED_Text_Color);
      oled.print(newDisplayString);

      oldHumidity = humidity;
      oldTemp = temp;
      oldHeatIndex = heatIndex;

      oldDisplayString = String(temp, 2) + "F\n" + String(humidity, 2) + "\n" + String(heatIndex, 2) + "F";

    }
  }
  catch (const std::exception &e)
  {
    Serial.println(e.what());
  }
}

void setup()
{
  Serial.begin(115200);

  // WiWi Manager
  pinMode(WIFICONFIG, INPUT);

  setupFileSystem();
  Serial.print("Host is: ");
  Serial.println(host);

  // Initialize device.
  dhtUnified.begin();
  sensor_t sensor;
  dhtUnified.temperature().getSensor(&sensor);
  // Set delay between sensor readings based on sensor details.
  delayMS = (sensor.min_delay / 1000);

  // Display
  // initialise the SSD1331
  oled.begin();
  oled.setFont();
  oled.fillScreen(OLED_Backround_Color);
  oled.setTextColor(OLED_Text_Color);
  oled.setTextSize(2);

  // the display is now on
  isDisplayVisible = true;
  oled.enableDisplay(isDisplayVisible);

  sentTime = millis();
}

void loop()
{
  try
  {
    if (digitalRead(WIFICONFIG) == LOW || host == "")
    {
      configureWiFi();
    }
  }
  catch (const std::exception &e)
  {
    Serial.print("Check wifi config: ");
    Serial.println(e.what());
  }

  Serial.println("Before sensors");
  try
  {
    // Get temperature event and print its value.
    sensors_event_t event;
    dhtUnified.temperature().getEvent(&event);
    if (!isnan(event.temperature))
    {
      temp = dht.convertCtoF(event.temperature);
    }
    // Get humidity event and print its value.
    dhtUnified.humidity().getEvent(&event);
    if (!isnan(event.relative_humidity))
    {
      humidity = event.relative_humidity;
    }
  }
  catch (const std::exception &e)
  {
    Serial.print("Get sensor values");
    Serial.println(e.what());
  }

  if (temp > -100 && humidity > 0)
  {
    try
    {
      Serial.print("heat index ");
      Serial.print(temp);
      Serial.print("hum ");
      Serial.println(humidity);
      heatIndex = dht.computeHeatIndex(temp, humidity, true);
      Serial.print("heat index ");
      Serial.println(heatIndex);
      if ((millis() - sentTime) > (1000 * 60))
      {
        Serial.println("Sending");
        const size_t capacity = JSON_OBJECT_SIZE(3);

        DynamicJsonDocument doc(capacity);

        doc["temperature"] = temp;
        doc["humidity"] = humidity;
        doc["relativeTemp"] = heatIndex;

        String sensorValues;
        serializeJson(doc, sensorValues);
        Serial.println(sensorValues);

        if (!client.connect(host, port))
        {
          Serial.println("connection failed");
          return;
        }
        // Now post
        String url = "/thermometer";

        HTTPClient httpClient;

        String postUrl = "http://" + String(host) + ":" + String(port) + url;
        Serial.println(postUrl);

        httpClient.begin(client, postUrl);
        httpClient.addHeader("Content-Type", "application/json");
        auto httpCode = httpClient.POST(sensorValues);

        Serial.print("Received HTTP code: ");
        Serial.println(httpCode);

        httpClient.end();
        sentTime = millis();
      }
    }
    catch (const std::exception &e)
    {
      Serial.print("Exception while sending: ");
      Serial.println(e.what());
    }

    displayValues();
  }

  delay(delayMS);
}