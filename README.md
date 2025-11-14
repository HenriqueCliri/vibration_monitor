### VIBRATION AND TEMPERATURE MONITOR

Este projeto tem como objetivo monitorar sistemas que tenham constância em vibração (podendo ser motores) em tempo real para detectar anomalias e prever falhas.

---

### to do: 
- [x] fazer conexão com ESP32
- [] analize de dados para criar curva padrão para dispositivo
- [] implementar matriz da confusão
- [x] criar interface para visualizar em tempo real
- [x] fazer conexão via websocket
- [x] colocar termometro
- [x] mostrar picos de amplitudes
- [] estimar a quantidade de horas que o sistema trabalha
- [] transformar em uma arquitetura distribuída

## 🛠️ Para Começar

### Requisitos

- **Hardware:**
    - ESP32
    - Sensor de Vibração (ex: MPU6050)
    - Motor Elétrico (a ser monitorado)

- **Software:**
    - VS code
    - PlatformIO

---

## ⚙️ Funcionalidades

### 1. Coleta e Análise de Dados
- **Conexão:** Estabelecer a comunicação entre o ESP32 e os sensores de vibração e temperatura.
- **Análise de Dados:** Coletar e analisar os dados para criar uma **curva de referência** para o motor elétrico. Essa curva, que combina padrões de vibração e temperatura, servirá como base para detectar anomalias.

### 2. Avaliação e Visualização
- **Matriz de Confusão:** Implementar cálculos para avaliar a precisão do modelo de detecção de anomalias.
- **Interface Gráfica:** Desenvolver uma interface para visualizar a análise em tempo real, exibindo gráficos de vibração e temperatura.

---

## 🔗 Referências
- [**Documentação da nuvem Arduino para ESP32:** Guia para integrar o ESP32 com a plataforma Arduino Cloud. ](https://docs.arduino.cc/arduino-cloud/guides/esp32/)
- [**Código de teste MPU6050:** Exemplo de código para testar o sensor MPU6050 no fórum do Arduino. ](https://forum.arduino.cc/t/simplest-test-code-for-mpu6050/1250345)
