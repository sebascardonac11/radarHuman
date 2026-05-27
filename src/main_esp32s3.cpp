#include <Arduino.h>

#define LED_PIN 2

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10); // espera a que el host abra el puerto CDC
    pinMode(LED_PIN, OUTPUT);
}

void loop() {
    Serial.println("Hola Mundo");
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
}
