#pragma once

#include <Arduino.h>
#include "Config.h"

// Decodifica los frames de reporte del LD2410B (protocolo "F4 F3 F2 F1 ...
// F8 F7 F6 F5", distinto del protocolo de comandos que usa RadarProtocol).
namespace RadarParser {

struct Telemetry {
    uint8_t  targetState;        // 0=sin objetivo 1=movimiento 2=estático 3=ambos
    uint16_t movingDistanceCm;
    uint8_t  movingEnergyBasic;
    uint16_t staticDistanceCm;
    uint8_t  staticEnergyBasic;
    uint16_t detectDistanceCm;
    bool     engineering;         // true si el frame trae energía por gate
    uint8_t  movingEnergy[GATE_COUNT];
    uint8_t  staticEnergy[GATE_COUNT];
    uint8_t  lightValue;
    uint8_t  outPinStatus;
};

// Alimenta un byte crudo leído de radarSerial. Llamar por cada byte disponible.
void feed(uint8_t b);

// Detecta y resetea el parser si quedó atascado a mitad de frame (byte drop
// de SoftwareSerial). Llamar una vez por iteración de loop().
void tick();

// Último frame válido decodificado.
const Telemetry &telemetry();

} // namespace RadarParser
