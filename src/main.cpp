#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <arduinoFFT.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

const char *ssid = "CABECA";
const char *password = "240619yj";

#define SAMPLING_FREQUENCY 1000
const uint16_t FFT_SAMPLES = 256;
const double sampling_period_us = round(1000000.0 / SAMPLING_FREQUENCY);
double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
ArduinoFFT<double> FFTobj(vReal, vImag, FFT_SAMPLES, SAMPLING_FREQUENCY);
const int FFT_BINS_TO_SEND = 64;

Preferences preferences;
unsigned long totalRuntimeSeconds = 0;
float vibrationThreshold = 3.0;
float noiseFloor = 0.0;
bool motorIsOn = false;
const int CALIBRATION_S = 5;
const int ON_DEBOUNCE_S = 3;
const int OFF_DEBOUNCE_S = 5;
const float THRESHOLD_MULTIPLIER = 2.0;
const float THRESHOLD_ADDER = 0.5;
unsigned long motorOnTimestamp = 0;
unsigned long motorOffTimestamp = 0;
unsigned long lastRuntimeSave = 0;

unsigned long lastSensorRead = 0;
unsigned long lastRuntimeCheck = 0;
unsigned long lastFFTTime = 0;
unsigned long lastRuntimeWsPush = 0;
unsigned long lastLogTime = 0;

SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t wsMutex;

DynamicJsonDocument docCore1(1024);
DynamicJsonDocument docCore0(2048);

Adafruit_MPU6050 mpu;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

volatile float last_accX = 0, last_accY = 0, last_accZ = 0, last_magnitude = 0, last_temp = 0;

void saveRuntime() {
  preferences.putULong("runtime", totalRuntimeSeconds);
  Serial.printf("[Core 0] Salvando runtime: %lu s\n", totalRuntimeSeconds);
}

void updateRuntime() {
  unsigned long currentMillis = millis();

  if(last_magnitude > vibrationThreshold) {
    motorOffTimestamp = 0;

    if(motorOnTimestamp == 0) {
      motorOnTimestamp = currentMillis;
    }
    if(!motorIsOn && (currentMillis - motorOnTimestamp) >= (ON_DEBOUNCE_S * 1000)) {
      motorIsOn = true;
      Serial.printf("[Core 0] --- Motor LIGADO (Vib: %.2f > Lim: %.2f) ---\n", last_magnitude, vibrationThreshold);
    }
  }
  else {
    motorOnTimestamp = 0;
    
    if (motorOffTimestamp == 0) {
      motorOffTimestamp = currentMillis;
    }
    if (motorIsOn && (currentMillis - motorOffTimestamp) >= (OFF_DEBOUNCE_S * 1000)) {
      motorIsOn = false;
      saveRuntime();
      Serial.printf("[Core 0] --- Motor DESLIGADO (Vib: %.2f < Lim: %.2f) ---\n", last_magnitude, vibrationThreshold);
    }
  }

  if (motorIsOn) {
    totalRuntimeSeconds++;
  }

  if (currentMillis - lastRuntimeSave >= 300000) {
    lastRuntimeSave = currentMillis;
    if (motorIsOn) {
      saveRuntime();
    }
  }
}

bool readSensor_safe() {
  bool readSuccess = false;

  if (xSemaphoreTake(i2cMutex, (TickType_t)10) == pdTRUE) {
    sensors_event_t a, g, temp;

    if (mpu.getEvent(&a, &g, &temp)) {
      last_accX = a.acceleration.x;
      last_accY = a.acceleration.y;
      last_accZ = a.acceleration.z;
      last_temp = temp.temperature;
      last_magnitude = sqrt(pow(last_accX, 2) + pow(last_accY, 2) + pow(last_accZ - 9.8, 2));
      readSuccess = true;
    }
    xSemaphoreGive(i2cMutex);
  }
  return readSuccess;
}

void heavyTaskLoop(void *parameter) {
  Serial.println("Heavy Task (Core 0) iniciada.");
  int loopCounter = 0;

  for (;;) {
    updateRuntime();

    if (loopCounter % 2 == 0) {

      for (int i = 0; i < FFT_SAMPLES; i++) {
        if (xSemaphoreTake(i2cMutex, (TickType_t)10) == pdTRUE) {
          sensors_event_t a, g, temp;

            if (!mpu.getEvent(&a, &g, &temp)) {
              vReal[i] = 0;
            } else {
              vReal[i] = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z - 9.8, 2));
            }
            xSemaphoreGive(i2cMutex);

        } else {
          vImag[i] = 0;
        }
        vImag[i] = 0;
        delayMicroseconds(sampling_period_us);
      }

      FFTobj.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFTobj.compute(FFT_FORWARD);
      FFTobj.complexToMagnitude();

      if (xSemaphoreTake(wsMutex, (TickType_t) 100) == pdTRUE) {
        docCore0.clear();
        docCore0["type"] = "fft";

        JsonArray freqs = docCore0.createNestedArray("freqs");
        JsonArray mags = docCore0.createNestedArray("mags");
        double freqResolution = (double)SAMPLING_FREQUENCY / FFT_SAMPLES;

        for(int i = 1; i < FFT_BINS_TO_SEND; i++) {
          freqs.add(String(i * freqResolution, 1).toFloat());
          mags.add(String(vReal[i], 2).toFloat());
        }

        String output;
        serializeJson(docCore0, output);
        ws.textAll(output);
        xSemaphoreGive(wsMutex);
      }
    }

    if (loopCounter % 10 == 0) {
      if (xSemaphoreTake(wsMutex, (TickType_t) 100) == pdTRUE) {
        docCore0.clear();
        docCore0["type"] = "runtime";
        docCore0["seconds"] = totalRuntimeSeconds;
        String output;
        serializeJson(docCore0, output);
        ws.textAll(output);
        xSemaphoreGive(wsMutex);
      }
    }

    loopCounter++;
    if (loopCounter > 600){
      loopCounter = 0;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[Core 1] Cliente WebSocket #%u conectado\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[Core 1] Cliente WebSocket #%u desconectado\n", client->id());
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("\n\n--- INICIO SETUP (Dual Core, Sem Log) ---");

  i2cMutex = xSemaphoreCreateMutex();
  wsMutex = xSemaphoreCreateMutex();

  preferences.begin("motor-monitor", false);
  totalRuntimeSeconds = preferences.getULong("runtime", 0);
  Serial.printf("Tempo de uso carregado: %lu s\n", totalRuntimeSeconds);

  if (!mpu.begin()) {
    Serial.println("!!! FALHA MPU !!!");
    while (1) {
      delay(10);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("--- MPU OK ---");

  Serial.printf("Iniciando calibracao do ruido por %d s...\n", CALIBRATION_S);
  float noiseSum = 0;
  int noiseSamples = 0;
  unsigned long calibrationStart = millis();

  while (millis() - calibrationStart < (CALIBRATION_S * 1000)) {
    if (xSemaphoreTake(i2cMutex, (TickType_t)10) == pdTRUE) {
      sensors_event_t a, g, temp;
      if (mpu.getEvent(&a, &g, &temp)) {
        float mag = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z - 9.8, 2));
        if (millis() - calibrationStart > 500) {
          noiseSum += mag;
          noiseSamples++;
        }
      }
      xSemaphoreGive(i2cMutex);
    }
    delay(100);
  }

  if (noiseSamples > 0) {
    noiseFloor = noiseSum / noiseSamples;
  }
  vibrationThreshold = (noiseFloor * THRESHOLD_MULTIPLIER) + THRESHOLD_ADDER;
  Serial.printf("Calibracao concluida. Limite: %.2f m/s^2\n", vibrationThreshold);

  Serial.print("Conectando a '");
  Serial.print(ssid);
  Serial.println("' ...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n--- WIFI OK ---");
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) { 
    request->send(200, "text/plain", "Servidor de Vibracao (WebSocket, Dual-Core) OK"); 
  });

  server.begin();
  Serial.println("Servidor HTTP e WebSocket iniciados (no Core 1).");

  xTaskCreatePinnedToCore(
      heavyTaskLoop,
      "HeavyTask",
      10000,
      NULL,
      1,
      NULL,
      0);

  Serial.println("--- FIM SETUP ---");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorRead >= 250){

    lastSensorRead = currentMillis;
    readSensor_safe();

    if(xSemaphoreTake(wsMutex, (TickType_t) 10) == pdTRUE){
      docCore1.clear();
      docCore1["type"] = "axes";
      docCore1["ax"] = String(last_accX, 2).toFloat();
      docCore1["ay"] = String(last_accY, 2).toFloat();
      docCore1["az"] = String(last_accZ - 9.8, 2).toFloat();
      docCore1["temp"] = String(last_temp, 1).toFloat();
      String output;
      serializeJson(docCore1, output);
      ws.textAll(output);
      xSemaphoreGive(wsMutex);
    }
  }

  ws.cleanupClients();
  vTaskDelay(10 / portTICK_PERIOD_MS);
}