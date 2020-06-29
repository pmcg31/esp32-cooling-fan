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
#include <PID_v1.h>
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

HardwareSerial &Debug = Serial2;

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

bool manualMode = false;

bool reportTemp = true;

bool fanOn = true;
unsigned int fanPWMPct = 100;
unsigned int fanSpeedRPM = 0;
double tempF = 0.0;
double humidity = 0.0;

double tempC = 0.0;
double setPoint = 32.5;
double kp = 5.0;
double ki = 5.0;
double kd = 1.0;
double pidOutput = 30.0;

PID pid(&tempC, &pidOutput, &setPoint, kp, ki, kd, REVERSE);

int fanPctToPwm[101];

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Adafruit_Si7021 sensor = Adafruit_Si7021();

char loggerBuf[2048];

int espLogger(const char *str, va_list vl)
{
  vsprintf(loggerBuf, str, vl);
  Debug.println(loggerBuf);
}

void setup()
{
  Debug.begin(115200);
  Debug.println("Booting");
  Debug.print("Compile date: ");
  Debug.println(compileDate);
  Debug.print("Compile time: ");
  Debug.println(compileTime);

  for (int i = 0; i <= 100; i++)
  {
    fanPctToPwm[i] = (int)(((double)i / 100.0) * 255.0);
  }

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

  Debug.println("Fan On!");
  Debug.print("fan pwm value: ");
  Debug.println(fanPctToPwm[fanPWMPct]);
  syncValues();

  lastTachResetMillis = millis();
  attachInts();

  pid.SetOutputLimits(0, 100);
  if (manualMode)
  {
    pid.SetMode(MANUAL);
  }
  else
  {
    pid.SetMode(AUTOMATIC);
  }
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

        if (!manualMode)
        {
          double tmp = pidOutput;
          pid.Compute();

          if (tmp != pidOutput)
          {
            if (pidOutput > 99.9)
            {
              fanPWMPct = 100;
            }
            else if (pidOutput < 0.1)
            {
              fanPWMPct = 0;
            }
            else
            {
              fanPWMPct = (int)pidOutput;
            }

            if (fanPWMPct < 10)
            {
              if (fanOn)
              {
                fanOn = false;
              }
            }
            else
            {
              if (!fanOn)
              {
                fanOn = true;
              }
            }

            syncValues();
          }
        }
      }

      ws.textAll(JSON.stringify(doc));

      if (restartTimer != 0)
      {
        restartTimer--;
        if (restartTimer == 0)
        {
          Debug.println("Restarting....");
          ESP.restart();
        }
        else
        {
          Debug.printf("Restart in %d sec\r\n", restartTimer);
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
    Debug.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  char contents[1024];
  int cp = 0;
  if (!configFile)
  {
    Debug.println("Failed to open config.json for reading");
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
      Debug.println("Error parsing config.json");
    }

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

  ledcWrite(0, fanPctToPwm[fanPWMPct]);

  if (manualMode)
  {
    pid.SetMode(MANUAL);
  }
  else
  {
    pid.SetMode(AUTOMATIC);
  }

  JSONVar doc;
  doc["fanOn"] = fanOn;
  doc["fanPwmPct"] = (int)fanPWMPct;
  doc["manualMode"] = manualMode;
  doc["setPoint"] = setPoint;

  ws.textAll(JSON.stringify(doc));
}

void IRAM_ATTR countTach()
{
  tachCount++;
}

void setupNetwork()
{
  WiFi.mode(WIFI_STA);
  Debug.printf("Connecting to SSID \"%s\"\r\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Debug.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  if (!MDNS.begin(host))
  {
    Debug.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
  }
  Debug.printf("mDNS responder started @ %s.local\r\n", host);

  Debug.println("Ready");
  Debug.print("IP address: ");
  Debug.println(WiFi.localIP());
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
        Debug.println("Start updating " + type);
      })
      .onEnd([]() {
        attachInts();
        Debug.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Debug.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Debug.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Debug.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Debug.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Debug.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Debug.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Debug.println("End Failed");
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
        Debug.println("Did not find Si7021 sensor! Giving up on it, no temp reporting this run");
        reportTemp = false;
        return;
      }
      else
      {
        Debug.print("Did not find Si7021 sensor! Will retry ");
        Debug.print(retryCount);
        Debug.print(" time");
        if (retryCount == 1)
        {
          Debug.println();
        }
        else
        {
          Debug.println("s");
        }
      }

      delay(1000);
    }
  }

  Debug.print("Found model ");
  switch (sensor.getModel())
  {
  case SI_Engineering_Samples:
    Debug.print("SI engineering samples");
    break;
  case SI_7013:
    Debug.print("Si7013");
    break;
  case SI_7020:
    Debug.print("Si7020");
    break;
  case SI_7021:
    Debug.print("Si7021");
    break;
  case SI_UNKNOWN:
  default:
    Debug.print("Unknown");
  }
  Debug.print(" Rev(");
  Debug.print(sensor.getRevision());
  Debug.print(")");
  Debug.print(" Serial #");
  Debug.print(sensor.sernum_a, HEX);
  Debug.println(sensor.sernum_b, HEX);
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
    Debug.printf("FW Update: %s\r\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    { //start with max available size
      Update.printError(Debug);
    }
  }

  /* flashing firmware to ESP*/
  if (Update.write(data, len) != len)
  {
    Update.printError(Debug);
  }

  if (final)
  {
    if (Update.end(true))
    { //true to set the size to the current progress
      Debug.printf("FW Update Success: %s %u\r\n", filename.c_str(), index + len);
    }
    else
    {
      Update.printError(Debug);
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
    Debug.printf("FS Update: %s\r\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
    { //start with max available size
      Update.printError(Debug);
    }
  }

  /* flashing firmware to ESP*/
  if (Update.write(data, len) != len)
  {
    Update.printError(Debug);
  }

  if (final)
  {
    if (Update.end(true))
    { //true to set the size to the current progress
      Debug.printf("FS Update Success: %s %u\r\n", filename.c_str(), index + len);
    }
    else
    {
      Update.printError(Debug);
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
    Debug.println("WS client connect");

    JSONVar doc;
    doc["fanOn"] = fanOn;
    doc["fanPwmPct"] = (int)fanPWMPct;
    doc["fanSpeed"] = (int)fanSpeedRPM;
    doc["tempC"] = tempC;
    doc["tempF"] = tempF;
    doc["humidity"] = humidity;
    doc["manualMode"] = manualMode;
    doc["setPoint"] = setPoint;

    client->text(JSON.stringify(doc));
  }
  break;
  case WS_EVT_DISCONNECT:
    Debug.println("WS client disconnect");
    break;
  case WS_EVT_DATA:
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && (info->index == 0) && (info->len == length))
    {
      if (info->opcode == WS_TEXT)
      {
        data[length] = 0;
        Debug.print("data is ");
        Debug.println((char *)data);

        JSONVar doc = JSON.parse((char *)data);
        bool update = false;

        if (doc.hasOwnProperty("fanOn"))
        {
          fanOn = doc["fanOn"];
          update = true;
        }

        if (doc.hasOwnProperty("fanPwmPct"))
        {
          fanPWMPct = (int)doc["fanPwmPct"];
          update = true;
        }

        if (doc.hasOwnProperty("manualMode"))
        {
          manualMode = doc["manualMode"];
          update = true;
        }

        if (doc.hasOwnProperty("setPoint"))
        {
          setPoint = doc[setPoint];
        }

        if (update)
        {
          syncValues();
        }
      }
      else
      {
        Debug.println("Received a ws message, but it isn't text");
      }
    }
    else
    {
      Debug.println("Received a ws message, but it didn't fit into one frame");
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