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

// --- Configurações (Wi-Fi, FFT, Runtime) ---
// ... (Copie todas as suas configurações de Wi-Fi, FFT e Runtime aqui) ...
const char* ssid = "CABECA";
const char* password = "240619yj";
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
// ------------------------------------------------

// --- A "CHAVE" (MUTEX) PARA O SENSOR I2C ---
SemaphoreHandle_t i2cMutex;
// ------------------------------------------

// --- Objetos Principais ---
Adafruit_MPU6050 mpu;
AsyncWebServer server(80);    
AsyncWebSocket ws("/ws");    
DynamicJsonDocument doc(2048); 

// --- Variáveis Globais de Dados ---
volatile float last_accX = 0, last_accY = 0, last_accZ = 0, last_magnitude = 0, last_temp = 0;

// --- Funções Helper (Core 0) ---

void saveRuntime() { /* ... (igual ao anterior) ... */ }
void updateRuntime() { /* ... (igual ao anterior) ... */ }

// Função helper segura para ler o sensor (com Mutex)
bool readSensor_safe() {
  bool readSuccess = false;
  // Tenta pegar a "chave" (espera até 10ms)
  if (xSemaphoreTake(i2cMutex, (TickType_t) 10) == pdTRUE) {
    // ESTAMOS SEGUROS, pegamos a chave
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      last_accX = a.acceleration.x;
      last_accY = a.acceleration.y;
      last_accZ = a.acceleration.z;
      last_temp = temp.temperature;
      last_magnitude = sqrt(pow(last_accX, 2) + pow(last_accY, 2) + pow(last_accZ - 9.8, 2));
      readSuccess = true;
    }
    xSemaphoreGive(i2cMutex); // Devolve a chave
  } else {
    // Não conseguimos a chave, o outro núcleo está demorando muito
    Serial.println("[!] Falha ao obter Mutex I2C!");
  }
  return readSuccess;
}

// =================================================================
// --- TAREFA DO NÚCLEO 0 (O TRABALHADOR PESADO) ---
// =================================================================
void heavyTaskLoop(void * parameter) {
  Serial.println("Heavy Task (Core 0) iniciada.");
  
  int loopCounter = 0;

  for (;;) {
    // 1. Atualiza o Runtime (1x por segundo)
    updateRuntime();
    
    // 2. Envia FFT (a cada 2 segundos)
    if (loopCounter % 2 == 0) {
      
      // Roda a FFT (agora com Mutex)
      for (int i = 0; i < FFT_SAMPLES; i++) {
        // Tenta pegar a chave
        if (xSemaphoreTake(i2cMutex, (TickType_t) 10) == pdTRUE) {
          sensors_event_t a, g, temp;
          if (!mpu.getEvent(&a, &g, &temp)) { vReal[i] = 0; }
          else { vReal[i] = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z - 9.8, 2)); }
          xSemaphoreGive(i2cMutex); // Devolve a chave
        } else {
          vReal[i] = 0; // Falha ao pegar o mutex
        }
        delayMicroseconds(sampling_period_us);
      }
      
      // (O resto da lógica da FFT é igual)
      FFTobj.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      // ... (código da FFT e envio via WebSocket igual ao anterior) ...
    }

    // 3. Envia Runtime (a cada 10 segundos)
    if (loopCounter % 10 == 0) {
      // ... (código de envio do runtime igual ao anterior) ...
    }
    
    loopCounter++;
    if (loopCounter > 600) { loopCounter = 0; }

    vTaskDelay(1000 / portTICK_PERIOD_MS); // Dorme por 1 segundo
  }
}
// =================================================================


// Event handler do WebSocket (Roda no Core 1)
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) { /* ... (igual ao anterior) ... */ }

// =================================================================
// --- SETUP (Roda 1x em qualquer núcleo, geralmente Core 1) ---
// =================================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("\n\n--- INICIO SETUP (Dual Core, Com Mutex) ---");

  // --- CRIA O MUTEX (A CHAVE) ---
  i2cMutex = xSemaphoreCreateMutex();
  // -----------------------------

  preferences.begin("motor-monitor", false); 
  totalRuntimeSeconds = preferences.getULong("runtime", 0); 
  Serial.printf("Tempo de uso carregado: %lu s\n", totalRuntimeSeconds);

  if (!mpu.begin()) {
    Serial.println("!!! FALHA MPU !!!");
    while (1) { delay(10); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("--- MPU OK ---");

  // Calibração (agora com Mutex)
  Serial.printf("Iniciando calibracao do ruido por %d s...\n", CALIBRATION_S);
  float noiseSum = 0; int noiseSamples = 0; unsigned long calibrationStart = millis();
  while (millis() - calibrationStart < (CALIBRATION_S * 1000)) {
    // Protegemos a leitura com a "chave"
    if (xSemaphoreTake(i2cMutex, (TickType_t) 10) == pdTRUE) {
      sensors_event_t a, g, temp; 
      if (mpu.getEvent(&a, &g, &temp)) {
        float mag = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z - 9.8, 2));
        if (millis() - calibrationStart > 500) { noiseSum += mag; noiseSamples++; }
      }
      xSemaphoreGive(i2cMutex); // Devolve a chave
    }
    delay(100); 
  }
  if (noiseSamples > 0) { noiseFloor = noiseSum / noiseSamples; }
  vibrationThreshold = (noiseFloor * THRESHOLD_MULTIPLIER) + THRESHOLD_ADDER; 
  Serial.printf("Calibracao concluida. Limite: %.2f m/s^2\n", vibrationThreshold);

  // Conexão Wi-Fi (igual)
  Serial.print("Conectando a '"); Serial.print(ssid); Serial.println("' ...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n--- WIFI OK ---");
  Serial.print("Endereco IP: "); Serial.println(WiFi.localIP());

  // Configura o WebSocket (igual)
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Servidor de Vibracao (WebSocket, Dual-Core) OK");
  });
  server.begin();
  Serial.println("Servidor HTTP e WebSocket iniciados (no Core 1).");

  // Inicia a tarefa no Núcleo 0 (igual)
  xTaskCreatePinnedToCore(
      heavyTaskLoop, "HeavyTask", 10000, NULL, 1, NULL, 0);                 
  
  Serial.println("--- FIM SETUP ---");
}
// =================================================================


// =================================================================
// --- LOOP PRINCIPAL (Roda no Núcleo 1) ---
// =================================================================
void loop() {
  unsigned long currentMillis = millis();
  
  // Roda 4x por segundo (250ms)
  if (currentMillis - lastSensorRead >= 250) { 
    lastSensorRead = currentMillis;
    
    // 1. Lê o sensor (agora usando a função segura)
    readSensor_safe(); 
    
    // 2. Envia os dados pelo WebSocket (igual)
    doc.clear();
    doc["type"] = "axes";
    doc["ax"] = String(last_accX, 2).toFloat();
    doc["ay"] = String(last_accY, 2).toFloat();
    doc["az"] = String(last_accZ, 2).toFloat(); // O 'az' já está compensado na função de leitura
    doc["temp"] = String(last_temp, 1).toFloat();
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
  }

  // 3. Limpa clientes
  ws.cleanupClients();
  
  vTaskDelay(10 / portTICK_PERIOD_MS); // Roda a cada 10ms
}