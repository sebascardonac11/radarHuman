#pragma once

// ── Pines ──────────────────────────────────────────────────────────────────
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

// ── Timing ─────────────────────────────────────────────────────────────────
#define RADAR_BAUD            115200UL
#define CONFIRM_MS            2000UL   // presencia continua antes de activar el relé
#define HOLD_MS                2000UL   // ausencia sostenida antes de apagar el relé
#define PARSER_TIMEOUT_MS      150UL    // reset del parser si un frame queda a medias
#define RADAR_ACK_TIMEOUT_MS   500UL    // espera máxima por un ACK de configuración

// ── Portal WiFi (modo Access Point) ───────────────────────────────────────
// El ESP crea su propia red — conéctate desde el celular/laptop y abre
// http://192.168.4.1/ para ver el estado del radar por gate y ajustar la
// sensibilidad (para filtrar falsos positivos por ramas, etc). Se mantiene
// activa siempre, como respaldo si el router de la finca no está disponible.
#define WIFI_AP_SSID  "RadarFinca"
#define WIFI_AP_PASS  "radar1234"   // mínimo 8 caracteres

// ── WiFi de la finca (modo estación) ──────────────────────────────────────
// Edita estos dos valores con el SSID/contraseña reales antes de flashear.
// El ESP se conecta también a esta red (además del AP de arriba), así que
// desde cualquier equipo ya conectado al WiFi de la finca podés entrar al
// portal con la IP que le asigne el router (se imprime por Serial al
// conectar) o directamente en http://radarhuman.local/ vía mDNS.
#define WIFI_STA_SSID  "Cardonas"
#define WIFI_STA_PASS  "Cardonas1234567890"
#define WIFI_STA_TIMEOUT_MS  15000UL   // tiempo máximo de espera al conectar
#define MDNS_HOSTNAME  "radarhuman"    // http://radarhuman.local/

#define GATE_COUNT  9
