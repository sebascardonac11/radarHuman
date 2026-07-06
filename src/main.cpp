#include <Arduino.h>

#include "Config.h"
#include "RadarProtocol.h"
#include "RadarParser.h"
#include "PresenceRelay.h"
#include "WebPortal.h"

#define HEALTH_LOG_INTERVAL_MS 60000UL   // diagnóstico de heap, para detectar fugas en sesiones largas

namespace {
unsigned long lastHealthLogMs = 0;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("=== BOOT OK ===");
    Serial.print("[SYS] razon de reinicio: ");
    Serial.println(ESP.getResetReason());

    PresenceRelay::begin();
    RadarProtocol::begin();   // baud rate + modo ingeniería del LD2410B
    WebPortal::begin();       // Access Point + servidor HTTP

    Serial.println("[RADAR] Esperando frames del LD2410B...");
}

void loop() {
    WebPortal::handleClient();

    while (radarSerial.available()) {
        RadarParser::feed(radarSerial.read());
        yield();   // feed ESP8266 watchdog and background tasks while draining buffer
    }
    RadarParser::tick();   // reset del parser si quedó atascado a mitad de frame

    PresenceRelay::update(RadarParser::telemetry().targetState);

    unsigned long now = millis();
    if (now - lastHealthLogMs >= HEALTH_LOG_INTERVAL_MS) {
        lastHealthLogMs = now;
        Serial.print("[SYS] uptime=");
        Serial.print(now / 1000);
        Serial.print("s heap=");
        Serial.print(ESP.getFreeHeap());
        Serial.print(" frag=");
        Serial.print(ESP.getHeapFragmentation());
        Serial.println("%");
    }
}
