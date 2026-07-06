#include "RadarParser.h"

namespace RadarParser {

namespace {

const uint8_t FRAME_HDR[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t FRAME_END[4] = {0xF8, 0xF7, 0xF6, 0xF5};

enum ParserState : uint8_t { SYNC, RD_LEN, RD_DATA, RD_END };

ParserState   pState     = SYNC;
uint8_t       hdrIdx     = 0;
uint8_t       frameBuf[48];
uint16_t      dataLen    = 0;
uint16_t      rxCount    = 0;
unsigned long lastByteMs = 0;

Telemetry current = {};

// frameBuf: [0-3]=HDR [4-5]=length [6]=tipo [7]=head [8]=targetState ...
void onFrame() {
    if (dataLen < 13) return;
    uint8_t type = frameBuf[6];
    if (frameBuf[7] != 0xAA) return;
    if (type != 0x01 && type != 0x02) return;   // 0x01=engineering  0x02=básico

    current.targetState       = frameBuf[8];
    current.movingDistanceCm  = frameBuf[9]  | ((uint16_t)frameBuf[10] << 8);
    current.movingEnergyBasic = frameBuf[11];
    current.staticDistanceCm  = frameBuf[12] | ((uint16_t)frameBuf[13] << 8);
    current.staticEnergyBasic = frameBuf[14];
    current.detectDistanceCm  = frameBuf[15] | ((uint16_t)frameBuf[16] << 8);

    current.engineering = (type == 0x01);
    if (current.engineering && dataLen >= 35) {
        for (uint8_t i = 0; i < GATE_COUNT; i++) current.movingEnergy[i] = frameBuf[19 + i];
        for (uint8_t i = 0; i < GATE_COUNT; i++) current.staticEnergy[i] = frameBuf[28 + i];
        current.lightValue   = frameBuf[37];
        current.outPinStatus = frameBuf[38];
    }
}

} // namespace

void feed(uint8_t b) {
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

void tick() {
    if (pState != SYNC && millis() - lastByteMs > PARSER_TIMEOUT_MS) {
        pState  = SYNC;
        hdrIdx  = 0;
        rxCount = 0;
    }
}

const Telemetry &telemetry() { return current; }

} // namespace RadarParser
