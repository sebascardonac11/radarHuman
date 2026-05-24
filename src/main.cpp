#include <Arduino.h>
#include <SoftwareSerial.h>

// ── Pin config ────────────────────────────────────────────────────────────────
#define RELAY_PIN     4   // GPIO4 — D2: NPN base via 1kΩ → relé activo-bajo
#define RADAR_RX_PIN  5   // GPIO5 — D1: ← LD2410B TX
#define RADAR_TX_PIN  14  // GPIO14 — D5: → LD2410B RX (necesario para configurar)

/*
  GPIO4 ──[1kΩ]── Base (BC547 / 2N2222)
                  Collector ── IN (módulo relé 5V, activo-bajo)
                  Emitter  ── GND
  GPIO4 HIGH → transistor ON → IN=LOW → relé ON
  GPIO4 LOW  → transistor OFF → IN=HIGH → relé OFF
*/

// ── Timing ────────────────────────────────────────────────────────────────────
#define RADAR_BAUD   115200   // baud rate operativo tras configureSensor()
#define CONFIRM_MS   2000UL   // presencia continua antes de activar el relé
#define HOLD_MS      2000UL   // ausencia sostenida antes de apagar el relé

SoftwareSerial radarSerial(RADAR_RX_PIN, RADAR_TX_PIN);

// ── LD2410B frame markers ─────────────────────────────────────────────────────
static const uint8_t FRAME_HDR[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t FRAME_END[4] = {0xF8, 0xF7, 0xF6, 0xF5};

// ── Parser state machine ──────────────────────────────────────────────────────
enum ParserState : uint8_t { SYNC, RD_LEN, RD_DATA, RD_END };
static ParserState   pState      = SYNC;
static uint8_t       hdrIdx      = 0;
static uint8_t       frameBuf[48];
static uint16_t      dataLen     = 0;
static uint16_t      rxCount     = 0;
static unsigned long lastByteMs  = 0;   // for parser stuck-detection
#define PARSER_TIMEOUT_MS  150UL        // reset parser if no byte arrives mid-frame

// ── Presencia / relé ──────────────────────────────────────────────────────────
static unsigned long firstDetectedMs  = 0;
static unsigned long lastDetectedMs   = 0;
static bool          relayOn          = false;
static int8_t        pendingState     = -1;
static uint8_t       lastPrintedState = 0xFF;  // throttle Serial: only print on change

// frameBuf: [0-3]=HDR [4-5]=length [6]=type [7]=head [8]=targetState
void onFrame() {
    if (dataLen < 3) return;
    if (frameBuf[6] != 0x02 || frameBuf[7] != 0xAA) return;

    uint8_t state = frameBuf[8];
    pendingState = state;

    if (state != 0x00) {
        lastDetectedMs = millis();
        if (firstDetectedMs == 0) firstDetectedMs = lastDetectedMs;
    }
}

void parseByte(uint8_t b) {
    lastByteMs = millis();
    switch (pState) {
    case SYNC:
        if (b == FRAME_HDR[hdrIdx]) {
            frameBuf[hdrIdx] = b;
            if (++hdrIdx == 4) { pState = RD_LEN; rxCount = 0; }
        } else {
            hdrIdx = (b == FRAME_HDR[0]) ? (frameBuf[0] = b, 1) : 0;
        }
        break;
    case RD_LEN:
        frameBuf[4 + rxCount] = b;
        if (++rxCount == 2) {
            dataLen = frameBuf[4] | ((uint16_t)frameBuf[5] << 8);
            if (dataLen == 0 || dataLen > 40) { pState = SYNC; hdrIdx = 0; break; }
            pState = RD_DATA; rxCount = 0;
        }
        break;
    case RD_DATA:
        frameBuf[6 + rxCount] = b;
        if (++rxCount == dataLen) { pState = RD_END; rxCount = 0; }
        break;
    case RD_END:
        if (b != FRAME_END[rxCount]) { pState = SYNC; hdrIdx = 0; break; }
        if (++rxCount == 4) { onFrame(); pState = SYNC; hdrIdx = 0; }
        break;
    }
}

// ── Configuración inicial del sensor ─────────────────────────────────────────
// Envía comandos al sensor a 256000 baud (default de fábrica) para bajarlo a
// 115200 y reiniciarlo. Si ya está en 115200, los bytes serán ignorados por el
// sensor (baud rate incorrecto) y se procede normalmente.
void configureSensor() {
    // Comandos UART del LD2410B (header FD FC FB FA / end 04 03 02 01)
    static const uint8_t CMD_ENABLE[]  = {0xFD,0xFC,0xFB,0xFA, 0x04,0x00, 0xFF,0x00, 0x01,0x00, 0x04,0x03,0x02,0x01};
    static const uint8_t CMD_BAUD[]    = {0xFD,0xFC,0xFB,0xFA, 0x04,0x00, 0xA1,0x00, 0x05,0x00, 0x04,0x03,0x02,0x01};
    static const uint8_t CMD_RESTART[] = {0xFD,0xFC,0xFB,0xFA, 0x02,0x00, 0xA3,0x00, 0x04,0x03,0x02,0x01};

    radarSerial.begin(256000);
    delay(300);
    radarSerial.write(CMD_ENABLE,  sizeof(CMD_ENABLE));  delay(100);
    radarSerial.write(CMD_BAUD,    sizeof(CMD_BAUD));    delay(100);
    radarSerial.write(CMD_RESTART, sizeof(CMD_RESTART)); delay(2000);

    radarSerial.begin(RADAR_BAUD);  // 115200 — operativo desde aquí
    delay(200);
    Serial.println("[RADAR] Sensor configurado a 115200 baud");
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("=== BOOT OK ===");
    configureSensor();
    Serial.println("[RADAR] Esperando frames del LD2410B...");
}

void loop() {
    while (radarSerial.available()) {
        parseByte(radarSerial.read());
        yield();   // feed ESP8266 watchdog and background tasks while draining buffer
    }

    // Reset parser if it got stuck mid-frame (byte drop from SoftwareSerial)
    if (pState != SYNC && millis() - lastByteMs > PARSER_TIMEOUT_MS) {
        pState = SYNC;
        hdrIdx = 0;
        rxCount = 0;
        // Serial.println("[PARSER] timeout — reset");
    }

    if (pendingState >= 0 && (uint8_t)pendingState != lastPrintedState) {
        lastPrintedState = (uint8_t)pendingState;
        // switch (pendingState) {
        //     case 0x00: Serial.println("[STATE] sin objetivo");          break;
        //     case 0x01: Serial.println("[STATE] movimiento");            break;
        //     case 0x02: Serial.println("[STATE] estatico");              break;
        //     case 0x03: Serial.println("[STATE] movimiento + estatico"); break;
        //     default:   Serial.print("[STATE] 0x"); Serial.println(pendingState, HEX); break;
        // }
    }
    pendingState = -1;

    unsigned long now = millis();

    if (firstDetectedMs > 0) {
        if (now - lastDetectedMs >= HOLD_MS) {
            firstDetectedMs = 0;
            if (relayOn) {
                relayOn = false;
                digitalWrite(RELAY_PIN, LOW);
                // Serial.println("[RELAY] OFF");
            }
        } else if (!relayOn && now - firstDetectedMs >= CONFIRM_MS) {
            relayOn = true;
            digitalWrite(RELAY_PIN, HIGH);
            // Serial.println("[RELAY] ON");
        }
    }
}
