/**
 * ESP8266 + LD2410B Human Presence Radar Sensor
 *
 * Pin connections:
 * ESP8266              ESP-01 (pins)
 * GPIO16 (TX) ─────►  Pin 4 (RX)
 * GPIO17 (RX) ◄─────  Pin 5 (TX)
 * GPIO14 ────────────►  Pin 3 (GPIO0)
 * GPIO15 ────────────►  Pin 7 (RST)
 * GND ─────────────────►  Pin 1 (GND)
 */

// ── Pin configuration ──────────────────────────────────────────────────────────────
#define SENSOR_PIN   0   // GPIO0
#define LED_PIN      2   // GPIO2 

// ── Setup / Loop ─────────────────────────────────────────────────────────────
void setup() {
    // No iniciamos Serial inmediatamente para no meter ruido
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Forzamos el apagado inmediato
    
    delay(2000); // ESPERAR 2 SEGUNDOS: El sensor LD2410 necesita tiempo para estabilizarse
    
    Serial.begin(115200);
    Serial.println("[Sistema] Iniciado con pines fijos");

    pinMode(SENSOR_PIN, INPUT);
}

void loop() {
    // Read sensor input and control LED
    bool sensorDetected = digitalRead(SENSOR_PIN);

    if (sensorDetected) {
        digitalWrite(LED_PIN,HIGH );
        Serial.println("[Sensor] PRESENCE DETECTED - LED ON");
    } else {
        digitalWrite(LED_PIN, LOW);
        Serial.println("[Sensor] No presence - LED OFF");
    }

    delay(500);  // Update every 500ms
}