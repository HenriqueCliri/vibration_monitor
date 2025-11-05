### VIBRATION AND TEMPERATURE MONITOR

Este projeto tem como objetivo monitorar a vibração e a temperatura de um motor elétrico em tempo real para detectar anomalias e prever falhas.

---

<<<<<<< HEAD
to do: 
[x] make connection with ESP32
[] data analyze to make default curve for device (eletrical engine)
[] calculors for confusion matrix
[] create interface to show graphic in real time analyze
=======
## 🛠️ Para Começar

### Requisitos

- **Hardware:**
    - ESP32
    - Sensor de Vibração (ex: MPU6050)
    - Sensor de Temperatura (ex: DS18B20 ou DHT22)
    - Motor Elétrico (a ser monitorado)

- **Software:**
    - IDE Arduino

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
>>>>>>> f7a3aa298d8860d36ccbf6a34391978cb8f94d3e
