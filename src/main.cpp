#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <arduinoFFT.h>

const char* ssid = "***";
const char* password = "**";

#define SAMPLING_FREQUENCY 1000 
const uint16_t FFT_SAMPLES = 256;

const double sampling_period_us = round(1000000.0 / SAMPLING_FREQUENCY);

double vReal[FFT_SAMPLES];
double vImag[FFT_SAMPLES];
// --------------------------

Adafruit_MPU6050 mpu;
WebServer server(80);
ArduinoFFT<double> FFTobj(vReal, vImag, FFT_SAMPLES, SAMPLING_FREQUENCY);

float last_accX = 0, last_accY = 0, last_accZ = 0, last_magnitude = 0;
unsigned long lastSensorRead = 0;

void readSensor() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorRead >= 100) {
    lastSensorRead = currentMillis;
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      last_accX = a.acceleration.x;
      last_accY = a.acceleration.y;
      last_accZ = a.acceleration.z;
      last_magnitude = sqrt(pow(last_accX, 2) + pow(last_accY, 2) + pow(last_accZ - 9.8, 2));
    } else {
        Serial.println("Falha na leitura do MPU6050 em readSensor");
    }
  }
}

void handleData() {
  readSensor();

  float az_comp = last_accZ - 9.8;

  String json = "{";
  json += "\"magnitude\":" + String(last_magnitude, 2) + ",";
  json += "\"ax\":" + String(last_accX, 2) + ",";
  json += "\"ay\":" + String(last_accY, 2) + ",";
  json += "\"az_comp\":" + String(az_comp, 2);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleFFTData() {
  Serial.println("Coletando amostras para FFT...");

  unsigned long startMicros;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    startMicros = micros();
    sensors_event_t a, g, temp;
    if (!mpu.getEvent(&a, &g, &temp)) {
        Serial.print("!");
         vReal[i] = 0;
    } else {
         vReal[i] = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z - 9.8, 2));
    }
    vImag[i] = 0;

    while (micros() - startMicros < sampling_period_us) {
    }
  }
  Serial.println("Amostragem concluída.");

  FFTobj.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFTobj.compute(FFT_FORWARD);
  FFTobj.complexToMagnitude();
  double peakFreq = FFTobj.majorPeak();

  Serial.printf("Frequencia de Pico: %.2f Hz\n", peakFreq);

  String json = "{";
  json += "\"peakFreq\":" + String(peakFreq, 2);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200);
}

void handleRoot() {
  server.send(200, "text/plain", "Servidor ESP32 de Vibracao OK. Use /data ou /fftdata");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  Serial.println("\n--- INICIO SETUP ---");

  if (!mpu.begin()) {
    Serial.println("!!! FALHA MPU !!!");
    while (1) { delay(10); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("--- MPU OK ---");

  Serial.print("Conectando a '"); Serial.print(ssid); Serial.println("' ...");
  WiFi.begin(ssid, password);
  int wifi_timeout_counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    wifi_timeout_counter++;
    if (wifi_timeout_counter > 40) {
        Serial.println("\n!!! TIMEOUT WIFI !!! Verifique SSID/Senha.");
        while(1) { delay(100); }
    }
  }
  Serial.println("\n--- WIFI OK ---");
  Serial.print("Endereco IP: "); Serial.println(WiFi.localIP());


  server.on("/", handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.on("/fftdata", HTTP_GET, handleFFTData);
  server.on("/fftdata", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
  Serial.println("Endpoints: /data (bruto), /fftdata (frequencia de pico)");
  Serial.println("--- FIM SETUP ---");
}

void loop() {
  server.handleClient();
}