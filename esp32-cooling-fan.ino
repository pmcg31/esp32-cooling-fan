#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Arduino_JSON.h>
#include <string.h>
#include "Adafruit_Si7021.h"

void readConfig();
void syncValues();
void setupNetwork();
void setupOTA();
void setupTempSensor();
void setupWebServer();
void handleUpdateFW(AsyncWebServerRequest *request);
void handleUploadFW(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdateFS(AsyncWebServerRequest *request);
void handleUploadFS(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void attachInts();
void detachInts();
void IRAM_ATTR countTach();

#define FAN_ENABLE_PIN 4
#define TACH_PIN 0
#define PWM_PIN 2

int restartTimer = 0;

const char *compileDate = __DATE__;
const char *compileTime = __TIME__;

char host[50];
char ssid[50];
char password[50];

unsigned long lastReportMillis = 0;
unsigned long lastTachResetMillis = 0;

unsigned int ledOn = 1;

unsigned long millisCount = 0;
unsigned int tickCount = 0;

volatile unsigned int tachCount = 0;

bool reportTemp = true;

bool fanOn = true;
unsigned int fanPWM = 255;
unsigned int fanSpeedRPM = 0;
float tempC = 0.0;
float tempF = 0.0;
float humidity = 0.0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Adafruit_Si7021 sensor = Adafruit_Si7021();

char loggerBuf[2048];

int espLogger(const char *str, va_list vl)
{
  vsprintf(loggerBuf, str, vl);
  Serial2.println(loggerBuf);
}

void setup()
{
  Serial2.begin(115200);
  Serial2.println("Booting");
  Serial2.print("Compile date: ");
  Serial2.println(compileDate);
  Serial2.print("Compile time: ");
  Serial2.println(compileTime);

  esp_log_set_vprintf(espLogger);

  readConfig();

  setupNetwork();

  setupOTA();

  setupWebServer();

  setupTempSensor();

  pinMode(TACH_PIN, INPUT_PULLUP);

  pinMode(FAN_ENABLE_PIN, OUTPUT);

  pinMode(PWM_PIN, OUTPUT);
  ledcSetup(0, 25000, 8);
  ledcAttachPin(PWM_PIN, 0);

  Serial2.println("Fan On!");
  Serial2.print("fan duty cycle: ");
  Serial2.println(fanPWM);
  syncValues();

  lastTachResetMillis = millis();
  attachInts();
}

void loop()
{
  unsigned long millisStart = millis();
  int chatty = 0;
  int fan = 0;

  if (millisStart > lastReportMillis + 100)
  {
    chatty = 1;
    lastReportMillis = millisStart;
    millisCount += lastReportMillis - millisStart;
    tickCount++;

    if (tickCount % 10 == 0)
    {
      int lastTachCount = tachCount;
      tachCount = 0;
      unsigned long now = millis();
      unsigned long totalTime = now - lastTachResetMillis;
      lastTachResetMillis = now;
      double revPerMillis = (double)lastTachCount / (double)totalTime;
      revPerMillis /= 2;
      fanSpeedRPM = revPerMillis * 60000;

      // DynamicJsonDocument doc(1024);
      JSONVar doc;
      doc["fanSpeed"] = (int)fanSpeedRPM;

      if (reportTemp)
      {
        tempC = sensor.readTemperature();
        tempF = (tempC * 9 / 5) + 32;
        humidity = sensor.readHumidity();
        doc["tempC"] = tempC;
        doc["tempF"] = tempF;
        doc["humidity"] = humidity;
      }

      // char json[1024];
      // serializeJsonPretty(doc, json);
      // ws.textAll(json);
      ws.textAll(JSON.stringify(doc));

      if (restartTimer != 0)
      {
        restartTimer--;
        if (restartTimer == 0)
        {
          Serial2.println("Restarting....");
          ESP.restart();
        }
        else
        {
          Serial2.printf("Restart in %d sec\r\n", restartTimer);
        }
      }
    }

    ArduinoOTA.handle();
  }
}

void readConfig()
{
  if (!SPIFFS.begin(true))
  {
    Serial2.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  char contents[1024];
  int cp = 0;
  if (!configFile)
  {
    Serial2.println("Failed to open config.json for reading");
    return;
  }
  else
  {
    while (configFile.available())
    {
      contents[cp] = configFile.read();
      cp++;
    }

    JSONVar doc = JSON.parse(contents);
    if (JSON.typeof(doc) == "undefined")
    {
      Serial2.println("Error parsing config.json");
    }
    // DeserializationError error = deserializeJson(doc, configFile);
    // if (error)
    // {
    //   Serial2.print("Error parsing config.json [");
    //   Serial2.print(error.c_str());
    //   Serial2.println("]");
    // }

    strcpy(host, doc["host"]);
    strcpy(ssid, doc["ssid"]);
    strcpy(password, doc["key"]);

    configFile.close();
  }
}

void syncValues()
{
  if (fanOn)
  {
    digitalWrite(FAN_ENABLE_PIN, HIGH);
  }
  else
  {
    digitalWrite(FAN_ENABLE_PIN, LOW);
  }

  ledcWrite(0, fanPWM);

  // DynamicJsonDocument doc(1024);
  JSONVar doc;
  doc["fanOn"] = fanOn;
  doc["fanPwmPct"] = (int)((double)fanPWM * 100.0 / 255.0);
  // char json[1024];
  // serializeJsonPretty(doc, json);
  // Serial2.print("Sending ");
  // Serial2.println(json);
  // ws.textAll(json);
  ws.textAll(JSON.stringify(doc));
}

void IRAM_ATTR countTach()
{
  tachCount++;
}

void setupNetwork()
{
  WiFi.mode(WIFI_STA);
  Serial2.printf("Connecting to SSID \"%s\"\r\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial2.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  if (!MDNS.begin(host))
  {
    Serial2.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Serial2.printf("mDNS responder started @ %s.local\r\n", host);

  Serial2.println("Ready");
  Serial2.print("IP address: ");
  Serial2.println(WiFi.localIP());
}

void setupOTA()
{
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(host);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
      .onStart([]() {
        detachInts();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial2.println("Start updating " + type);
      })
      .onEnd([]() {
        attachInts();
        Serial2.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial2.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial2.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial2.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial2.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial2.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial2.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial2.println("End Failed");
      });

  ArduinoOTA.begin();
}

void setupTempSensor()
{
  int retryCount = 5;
  if (!sensor.begin())
  {
    while (1)
    {
      if (retryCount == 0)
      {
        Serial2.println("Did not find Si7021 sensor! Giving up on it, no temp reporting this run");
        reportTemp = false;
        return;
      }
      else
      {
        Serial2.print("Did not find Si7021 sensor! Will retry ");
        Serial2.print(retryCount);
        Serial2.print(" time");
        if (retryCount == 1)
        {
          Serial2.println();
        }
        else
        {
          Serial2.println("s");
        }
      }

      delay(1000);
    }
  }

  Serial2.print("Found model ");
  switch (sensor.getModel())
  {
  case SI_Engineering_Samples:
    Serial2.print("SI engineering samples");
    break;
  case SI_7013:
    Serial2.print("Si7013");
    break;
  case SI_7020:
    Serial2.print("Si7020");
    break;
  case SI_7021:
    Serial2.print("Si7021");
    break;
  case SI_UNKNOWN:
  default:
    Serial2.print("Unknown");
  }
  Serial2.print(" Rev(");
  Serial2.print(sensor.getRevision());
  Serial2.print(")");
  Serial2.print(" Serial #");
  Serial2.print(sensor.sernum_a, HEX);
  Serial2.println(sensor.sernum_b, HEX);
}

void handleUpdateFW(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  restartTimer = 5;
}

void handleUploadFW(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    detachInts();
    Serial2.printf("FW Update: %s\r\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    { //start with max available size
      Update.printError(Serial2);
    }
  }

  /* flashing firmware to ESP*/
  if (Update.write(data, len) != len)
  {
    Update.printError(Serial2);
  }

  if (final)
  {
    if (Update.end(true))
    { //true to set the size to the current progress
      Serial2.printf("FW Update Success: %s %u\r\n", filename.c_str(), index + len);
    }
    else
    {
      Update.printError(Serial2);
    }

    attachInts();
  }
}

void handleUpdateFS(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  restartTimer = 5;
}

void handleUploadFS(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    detachInts();
    Serial2.printf("FS Update: %s\r\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
    { //start with max available size
      Update.printError(Serial2);
    }
  }

  /* flashing firmware to ESP*/
  if (Update.write(data, len) != len)
  {
    Update.printError(Serial2);
  }

  if (final)
  {
    if (Update.end(true))
    { //true to set the size to the current progress
      Serial2.printf("FS Update Success: %s %u\r\n", filename.c_str(), index + len);
    }
    else
    {
      Update.printError(Serial2);
    }

    attachInts();
  }
}

void onWSEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type,
               void *arg,
               uint8_t *data,
               size_t length)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
  {
    Serial2.println("WS client connect");
    // DynamicJsonDocument doc(1024);
    JSONVar doc;
    doc["fanOn"] = fanOn;
    doc["fanPwmPct"] = (int)((double)fanPWM * 100.0 / 255.0);
    doc["fanSpeed"] = (int)fanSpeedRPM;
    doc["tempC"] = tempC;
    doc["tempF"] = tempF;
    doc["humidity"] = humidity;
    // char json[1024];
    // serializeJsonPretty(doc, json);
    // client->text(json);
    client->text(JSON.stringify(doc));
  }
  break;
  case WS_EVT_DISCONNECT:
    Serial2.println("WS client disconnect");
    break;
  case WS_EVT_DATA:
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && (info->index == 0) && (info->len == length))
    {
      if (info->opcode == WS_TEXT)
      {
        data[length] = 0;
        Serial2.print("data is ");
        Serial2.println((char *)data);
        // DynamicJsonDocument doc(1024);
        // deserializeJson(doc, (char *)data);
        JSONVar doc = JSON.parse((char *)data);
        bool update = false;

        // if (doc.containsKey("fanOn"))
        if (doc.hasOwnProperty("fanOn"))
        {
          fanOn = doc["fanOn"];
          update = true;
        }

        // if (doc.containsKey("fanPwmPct"))
        if (doc.hasOwnProperty("fanPwmPct"))
        {
          int pct = doc["fanPwmPct"];
          fanPWM = (int)((double)pct * 255.0 / 100.0);
          update = true;
        }

        if (update)
        {
          syncValues();
        }
      }
      else
      {
        Serial2.println("Received a ws message, but it isn't text");
      }
    }
    else
    {
      Serial2.println("Received a ws message, but it didn't fit into one frame");
    }
  }
  break;
  }
}

void setupWebServer()
{
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Firmware and filesystem updates
  server.on("/updatefw", HTTP_POST, handleUpdateFW, handleUploadFW);
  server.on("/updatefs", HTTP_POST, handleUpdateFS, handleUploadFS);

  server.begin();

  MDNS.addService("http", "tcp", 80);
}

void attachInts()
{
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), countTach, FALLING);
}

void detachInts()
{
  detachInterrupt(digitalPinToInterrupt(TACH_PIN));
}