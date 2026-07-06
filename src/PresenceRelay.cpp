#include "PresenceRelay.h"
#include "Config.h"

namespace PresenceRelay {

namespace {
unsigned long firstDetectedMs = 0;
unsigned long lastDetectedMs  = 0;
bool          relayOn         = false;
} // namespace

void begin() {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
}

void update(uint8_t targetState) {
    unsigned long now = millis();

    if (targetState != 0) {
        lastDetectedMs = now;
        if (firstDetectedMs == 0) firstDetectedMs = now;
    }

    if (firstDetectedMs > 0) {
        if (now - lastDetectedMs >= HOLD_MS) {
            firstDetectedMs = 0;
            if (relayOn) {
                relayOn = false;
                digitalWrite(RELAY_PIN, LOW);
            }
        } else if (!relayOn && now - firstDetectedMs >= CONFIRM_MS) {
            relayOn = true;
            digitalWrite(RELAY_PIN, HIGH);
        }
    }
}

bool isOn() { return relayOn; }

} // namespace PresenceRelay
