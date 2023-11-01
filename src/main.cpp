#include <ArduinoOTA.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

WiFiClient espClient;
PubSubClient client(espClient);

// WI-Fi settings
const char *ssid = "my-fi";
const char *password = "my-fipass";
// domoticz
const char *mqtt_server = "192.168.10.100";
#define TOPIC "domoticz/in"
int in_idx = 4;
int clean_idx = 5;
// sensors
const uint8_t INC_FLOW = 12;
const uint8_t OUT_FLOW = 14;

volatile float flowInput = 0.0;  // объем входящего потока
volatile float flowOutput = 0.0; // объем чистой воды на выходе из осмоса
const float IN_RATIO = 1780.0;   // 1780 (кол-во тактов на 1л) * 2 для CHANGE
const float OUT_RATIO = 1715.0;  // 1715 (кол-во тактов на 1л) *2 для CHANGE

const uint32_t sendPeriod = 1 * 60 * 1000; // 1 минута

void wifiConnect();
void initOTA();
void mqttconnect();
void run();
void sendData();
void incFlow();
void outFlow();

// Setup
void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(INC_FLOW, INPUT);
  pinMode(OUT_FLOW, INPUT);
  attachInterrupt(digitalPinToInterrupt(INC_FLOW), incFlow, CHANGE);
  attachInterrupt(digitalPinToInterrupt(OUT_FLOW), outFlow, CHANGE);
  wifiConnect();
  initOTA();
  client.setServer(mqtt_server, 1883);
}
// Loop
void loop()
{
  wifiConnect();
  ArduinoOTA.handle();
  if (!client.connected())
  {
    mqttconnect();
  }
  client.loop();
  run();
}

// Инициализация OTA
void initOTA()
{
  ArduinoOTA.setPassword("100500");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.begin();
}

// Wi-Fi
void wifiConnect()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  WiFi.hostname("ESP Osmos counter");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connection status: %d\n", WiFi.status());
  while (WiFi.status() != WL_CONNECTED)
  {
    uint32_t uptime = millis();
    // restart ESP if could not connect during 5 minutes
    if (WiFi.status() != WL_CONNECTED && (uptime > 1000 * 60 * 5))
    {
      ESP.restart();
    }
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
  }

  Serial.printf("\nConnection status: %d\n", WiFi.status());
  Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("Hostname: %s\n", WiFi.hostname().c_str());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqttconnect()
{
  while (!client.connected())
  {
    if (client.connect("waterCalc"))
    {
      client.subscribe(TOPIC);
    }
    else
    {
      Serial.print("Connection to MQTT server failed, status code = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds...");
      delay(5000);
    }
  }
}

// Main function
void run()
{
  static uint32_t sendDelay;
  if (millis() - sendDelay <= sendPeriod)
  {
    return;
  }
  Serial.print("in: ");
  Serial.print(flowInput);
  Serial.print(" ");
  Serial.print(flowInput / IN_RATIO);
  Serial.print(" out: ");
  Serial.print(flowOutput);
  Serial.print(" ");
  Serial.println(flowOutput / IN_RATIO);
  sendDelay = millis();
  sendData();
}

// Отправка данных
void sendData()
{
  float in = flowInput / IN_RATIO;
  float out = flowOutput / OUT_RATIO;
  String in_str = "{\"idx\":" + String(in_idx) + ",\"svalue\":\"" + String(in) + "\"}";
  if (client.publish("domoticz/in", in_str.c_str()))
  {
    flowInput = 0.0;
  }
  String clean_str = "{\"idx\":" + String(clean_idx) + ",\"svalue\":\"" + String(out) + "\"}";
  if (client.publish("domoticz/in", clean_str.c_str()))
  {
    flowOutput = 0.0;
  }
}

// считывание данных входящего потока
ICACHE_RAM_ATTR void incFlow()
{
  flowInput++;
}

// считывание данных исходящего потока
ICACHE_RAM_ATTR void outFlow()
{
  flowOutput++;
}