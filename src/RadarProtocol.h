#pragma once

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "Config.h"

// Instancia compartida del puerto serie hacia el LD2410B — también la drena
// RadarParser::feed() en el loop principal para los frames de reporte.
extern SoftwareSerial radarSerial;

// Comandos de configuración del LD2410B (protocolo "FD FC FB FA ... 04 03 02
// 01", distinto del protocolo de frames de reporte que usa RadarParser). Ver
// Doc/HLK-LD2410B.../LD2410B Serial communication protocol V1.07.pdf
namespace RadarProtocol {

struct GateConfig {
    bool     ok;
    uint8_t  maxMovingGate;
    uint8_t  maxStaticGate;
    uint8_t  motionSens[GATE_COUNT];
    uint8_t  staticSens[GATE_COUNT];
    uint16_t noOneDuration;
};

// Deja el sensor operando a 115200 baud y con el modo ingeniería habilitado
// (necesario para reportar la energía por gate). Llamar una vez en setup().
void begin();

bool enterConfig();
bool exitConfig();

// Bytes crudos vistos durante el último intento de leer un ACK, sin importar
// si formaron un ACK válido. Diagnóstico: 0 = el sensor no respondió nada.
uint16_t lastAckBytesSeen();

// Sensibilidad de un gate puntual (0-8). Persiste en la memoria no volátil
// del propio LD2410B, sobrevive un reinicio del ESP o del sensor.
bool setGateSensitivity(uint8_t gate, uint8_t motionSens, uint8_t staticSens);
bool applyGateSensitivity(uint8_t gate, uint8_t motionSens, uint8_t staticSens); // enter+set+exit

// Lee la configuración de sensibilidad vigente directo del sensor.
GateConfig readConfig();

} // namespace RadarProtocol
