#include "RadarProtocol.h"
#include <string.h>

SoftwareSerial radarSerial(RADAR_RX_PIN, RADAR_TX_PIN);

namespace RadarProtocol {

namespace {

const uint8_t CMD_HDR[4] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t CMD_END[4] = {0x04, 0x03, 0x02, 0x01};

void sendCommand(uint16_t cmdWord, const uint8_t *payload, uint8_t payloadLen) {
    uint16_t len = 2 + payloadLen;
    radarSerial.write(CMD_HDR, 4);
    radarSerial.write((uint8_t)(len & 0xFF));
    radarSerial.write((uint8_t)(len >> 8));
    radarSerial.write((uint8_t)(cmdWord & 0xFF));
    radarSerial.write((uint8_t)(cmdWord >> 8));
    if (payloadLen) radarSerial.write(payload, payloadLen);
    radarSerial.write(CMD_END, 4);
}

// Bytes crudos vistos en el último readAck(), sin importar si formaron un ACK
// válido. Sirve para diagnosticar: 0 bytes = el sensor no está respondiendo
// en absoluto (sospechar del cableado TX del ESP hacia el RX del sensor);
// >0 bytes pero sin ACK = algo llega pero no encaja con el protocolo.
uint16_t g_bytesSeenLastAck = 0;
uint8_t  g_rawBytesLastAck[40];

uint8_t readByteTraced() {
    uint8_t b = radarSerial.read();
    if (g_bytesSeenLastAck < sizeof(g_rawBytesLastAck)) g_rawBytesLastAck[g_bytesSeenLastAck] = b;
    g_bytesSeenLastAck++;
    return b;
}

// Consume bytes hasta encontrar la cabecera FD FC FB FA (o hasta agotar el
// timeout). No consume ningún byte extra más allá de la cabecera: el próximo
// read() del llamador cae exactamente en el primer byte de longitud.
bool syncHeader(uint16_t timeoutMs) {
    uint8_t hIdx = 0;
    while (hIdx < 4) {
        unsigned long t = millis();
        while (!radarSerial.available()) { if (millis() - t > timeoutMs) return false; yield(); }
        uint8_t b = readByteTraced();
        hIdx = (b == CMD_HDR[hIdx]) ? hIdx + 1 : (b == CMD_HDR[0] ? 1 : 0);
    }
    return true;
}

// Lee el próximo frame ACK (FD FC FB FA ... 04 03 02 01), ignorando cualquier
// frame de reporte (F4 F3 F2 F1 ...) que llegue mientras tanto.
bool readAck(uint8_t *ackPayload, uint8_t &ackLen, uint16_t timeoutMs) {
    g_bytesSeenLastAck = 0;

    // Hasta 3 intentos de sincronización: si la longitud o el marcador final
    // no encajan, puede que la cabecera encontrada no fuera un ACK real
    // (coincidencia con datos), así que se vuelve a buscar desde ahí.
    for (uint8_t resync = 0; resync < 3; resync++) {
        if (!syncHeader(timeoutMs)) return false;

        uint8_t lenBytes[2];
        for (uint8_t i = 0; i < 2; i++) {
            unsigned long t = millis();
            while (!radarSerial.available()) { if (millis() - t > timeoutMs) return false; yield(); }
            lenBytes[i] = readByteTraced();
        }
        uint16_t dlen = lenBytes[0] | ((uint16_t)lenBytes[1] << 8);
        if (dlen == 0 || dlen > 40) continue;

        uint8_t buf[40];
        for (uint16_t i = 0; i < dlen; i++) {
            unsigned long t = millis();
            while (!radarSerial.available()) { if (millis() - t > timeoutMs) return false; yield(); }
            buf[i] = readByteTraced();
        }

        uint8_t end[4];
        for (uint8_t i = 0; i < 4; i++) {
            unsigned long t = millis();
            while (!radarSerial.available()) { if (millis() - t > timeoutMs) return false; yield(); }
            end[i] = readByteTraced();
        }
        if (memcmp(end, CMD_END, 4) != 0) continue;

        ackLen = (uint8_t)dlen;
        memcpy(ackPayload, buf, dlen);
        return true;
    }
    return false;
}

void printHexDump(const char *label) {
    Serial.print(label);
    uint16_t n = g_bytesSeenLastAck < sizeof(g_rawBytesLastAck) ? g_bytesSeenLastAck : sizeof(g_rawBytesLastAck);
    for (uint16_t i = 0; i < n; i++) {
        if (g_rawBytesLastAck[i] < 0x10) Serial.print('0');
        Serial.print(g_rawBytesLastAck[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

// Un ACK válido no solo trae status=0: el comando que devuelve debe coincidir
// con el que se envió (word | 0x0100), si no se corre el riesgo de aceptar un
// ACK viejo que haya quedado sin leer de un comando anterior.
bool ackMatches(const uint8_t *ack, uint8_t ackLen, uint16_t cmdWord) {
    if (ackLen < 4) return false;
    uint16_t echoed = ack[0] | ((uint16_t)ack[1] << 8);
    return echoed == (uint16_t)(cmdWord | 0x0100) && ack[2] == 0x00 && ack[3] == 0x00;
}

bool enableEngineeringMode() {
    sendCommand(0x0062, nullptr, 0);
    uint8_t ack[40]; uint8_t ackLen;
    return readAck(ack, ackLen, RADAR_ACK_TIMEOUT_MS) && ackMatches(ack, ackLen, 0x0062);
}

} // namespace

uint16_t lastAckBytesSeen() { return g_bytesSeenLastAck; }

bool enterConfig() {
    uint8_t val[2] = {0x01, 0x00};
    sendCommand(0x00FF, val, 2);
    uint8_t ack[40]; uint8_t ackLen;
    return readAck(ack, ackLen, RADAR_ACK_TIMEOUT_MS) && ackMatches(ack, ackLen, 0x00FF);
}

bool exitConfig() {
    sendCommand(0x00FE, nullptr, 0);
    uint8_t ack[40]; uint8_t ackLen;
    return readAck(ack, ackLen, RADAR_ACK_TIMEOUT_MS) && ackMatches(ack, ackLen, 0x00FE);
}

bool setGateSensitivity(uint8_t gate, uint8_t motionSens, uint8_t staticSens) {
    uint16_t gateWord = gate;
    uint8_t payload[18] = {
        0x00, 0x00, (uint8_t)(gateWord & 0xFF), (uint8_t)(gateWord >> 8), 0x00, 0x00,
        0x01, 0x00, motionSens, 0x00, 0x00, 0x00,
        0x02, 0x00, staticSens, 0x00, 0x00, 0x00,
    };
    sendCommand(0x0064, payload, sizeof(payload));
    uint8_t ack[40]; uint8_t ackLen;
    return readAck(ack, ackLen, RADAR_ACK_TIMEOUT_MS) && ackMatches(ack, ackLen, 0x0064);
}

bool applyGateSensitivity(uint8_t gate, uint8_t motionSens, uint8_t staticSens) {
    if (!enterConfig()) return false;
    bool ok = setGateSensitivity(gate, motionSens, staticSens);
    exitConfig();
    return ok;
}

GateConfig readConfig() {
    GateConfig cfg = {};
    cfg.ok = false;
    if (!enterConfig()) return cfg;

    sendCommand(0x0061, nullptr, 0);
    uint8_t ack[40]; uint8_t ackLen;
    if (readAck(ack, ackLen, RADAR_ACK_TIMEOUT_MS) && ackLen >= 28 &&
        ackMatches(ack, ackLen, 0x0061) && ack[4] == 0xAA) {
        cfg.maxMovingGate = ack[6];
        cfg.maxStaticGate = ack[7];
        for (uint8_t i = 0; i < GATE_COUNT; i++) cfg.motionSens[i] = ack[8 + i];
        for (uint8_t i = 0; i < GATE_COUNT; i++) cfg.staticSens[i] = ack[17 + i];
        cfg.noOneDuration = ack[26] | ((uint16_t)ack[27] << 8);
        cfg.ok = true;
    }

    exitConfig();
    return cfg;
}

void begin() {
    // Comandos UART del LD2410B enviados en crudo: el sensor puede estar a
    // 256000 baud (default de fábrica) o ya en 115200, así que no hay un ACK
    // fiable que esperar hasta fijar el baud rate correcto.
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

    // Descarta bytes residuales acumulados mientras el puerto estaba a 256000
    // baud (si el sensor ya venía operando a 115200 de un arranque previo, lo
    // que recibimos en esa ventana es ruido mal muestreado).
    while (radarSerial.available()) radarSerial.read();

    Serial.println("[RADAR] Sensor configurado a 115200 baud");

    // El sensor puede tardar un poco en responder justo después del cambio de
    // baud/reinicio — reintenta un par de veces antes de rendirse.
    bool engOk = false;
    for (uint8_t attempt = 0; attempt < 3 && !engOk; attempt++) {
        if (attempt > 0) delay(200);
        if (!enterConfig()) {
            Serial.print("[RADAR] intento "); Serial.print(attempt + 1);
            Serial.print("/3: no entró en modo config, bytes recibidos=");
            Serial.println(lastAckBytesSeen());
            printHexDump("[RADAR]   raw: ");
            continue;
        }
        engOk = enableEngineeringMode();
        if (!engOk) {
            Serial.print("[RADAR] intento "); Serial.print(attempt + 1);
            Serial.print("/3: engineering mode rechazado, bytes recibidos=");
            Serial.println(lastAckBytesSeen());
            printHexDump("[RADAR]   raw: ");
        }
        exitConfig();
    }
    Serial.println(engOk ? "[RADAR] Modo ingeniería habilitado" : "[RADAR] No se pudo habilitar modo ingeniería (reintentos agotados)");
}

} // namespace RadarProtocol
