#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

void setup() {

    mpu.getEvent(&a, &temp);

    std::cout << "x " a.acceleration.x << endl;
    std::cout << "y " a.acceleration.y << endl;
    std::cout << "temperature: " << temp << endl;
}

void loop() {
    setup();
}
