# radarHuman

Detector de presencia humana con ESP8266-12F y sensor de radar HLK-LD2410B (24 GHz mmWave). Lee los frames UART del sensor y activa un relé cuando detecta presencia continua durante 500 ms. Instalación prevista en pared a 2,5 m de altura en exterior.

---

## Hardware

| Componente | Descripción |
|---|---|
| MCU | ESP8266-12F (160 MHz, 4 MB flash) |
| Sensor | HLK-LD2410B — radar 24 GHz mmWave, detección de movimiento y presencia estática |
| Actuador | Módulo relé 5 V con aislamiento óptico (entrada activa-baja) |
| Driver relé | Transistor NPN BC547 / 2N2222 + resistencia 1 kΩ |

## Conexiones

### LD2410B ↔ ESP8266-12F (SoftwareSerial)

| Pin sensor | GPIO ESP8266 | Pin WeMos D1 Mini | Función |
|---|---|---|---|
| TX | GPIO5 | D1 | Datos sensor → MCU (RX) |
| RX | GPIO14 | D5 | Comandos MCU → sensor (TX) |
| VCC | 3,3 V o 5 V | — | Alimentación |
| GND | GND | — | Tierra común |

### Relé (GPIO4 / D2)

```
GPIO4 ──[1 kΩ]── Base (BC547 / 2N2222)
                 Collector ── IN (módulo relé, activo-bajo)
                 Emitter  ── GND

GPIO4 HIGH  →  transistor ON  →  IN = LOW  →  relé energizado
GPIO4 LOW   →  transistor OFF →  IN = HIGH →  relé desenergizado
```

### Debug / monitor serie

UART0 (GPIO1 TX / GPIO3 RX) se usa para los logs de debug a 115 200 baud, accesible vía USB-serial.

---

## Funcionamiento

1. En `setup()`, `configureSensor()` abre SoftwareSerial a 256 000 baud (default de fábrica) y envía comandos al LD2410B para bajar el baud a 115 200 y reiniciarlo. Si el sensor ya está a 115 200, los comandos llegan corruptos y son ignorados — en ambos casos el sistema arranca en 115 200.
2. El parser de frames (`parseByte`) implementa una máquina de estados de 4 fases sobre el protocolo binario del LD2410B.
3. Cada frame (≈10 Hz) reporta el estado del objetivo en `frameBuf[8]`: `0x00` = ninguno, `0x01` = movimiento, `0x02` = estático, `0x03` = ambos.
4. Al detectar cualquier presencia (`state ≠ 0x00`), se registra `firstDetectedMs`. Tras 500 ms continuos de detección, el relé se activa.
5. `lastDetectedMs` se actualiza en cada frame con presencia. Si pasan 10 s sin detección, el relé se desactiva.

### Parámetros de tiempo

| Constante | Valor | Efecto |
|---|---|---|
| `CONFIRM_MS` | 500 ms | Presencia continua mínima para activar el relé |
| `HOLD_MS` | 10 000 ms | Ausencia sostenida para desactivar el relé |

---

## Protocolo UART LD2410B

Frame de reporte (23 bytes, ≈10 Hz):

```
F4 F3 F2 F1  |  0D 00  |  02  AA  [estado]  [dist_mov 2B]  [energía_mov]
[dist_est 2B]  [energía_est]  [dist_det 2B]  55  00  |  F8 F7 F6 F5
```

| Offset | Campo | Valor relevante |
|---|---|---|
| 0–3 | Header | `F4 F3 F2 F1` |
| 4–5 | Data length | `0D 00` (13 bytes) |
| 6 | Data type | `0x02` = reporte periódico |
| 7 | Head | `0xAA` |
| **8** | **Target state** | `0x00` sin objetivo / `≠0x00` presencia |
| 19–22 | End | `F8 F7 F6 F5` |

---

## Build y flash (PlatformIO)

```bash
# Compilar
pio run

# Compilar y flashear
pio run --target upload

# Monitor serie (si el botón de VSCode no responde, usar terminal)
~/.platformio/penv/bin/pio device monitor
```

Plataforma: `espressif8266`, board: `esp12e`, framework: `arduino`, CPU: 160 MHz. Solo se compila `src/main.cpp` (`build_src_filter = +<main.cpp>`).

---

## Documentación oficial

- Google Drive (datasheet, protocolo, herramienta de configuración BLE):
  https://drive.google.com/drive/folders/16zI-fium_BZeP08EyQke0rWp0BJTMvw3
- Protocolo serie detallado: `Doc/HLK-LD2410B human presence detection with BLE/LD2410B Serial communication protocol V1.07.pdf`
- Esquemático y PCB KiCad: `Doc/radarHuman/`
