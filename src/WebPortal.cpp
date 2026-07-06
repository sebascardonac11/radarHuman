#include "WebPortal.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "Config.h"
#include "RadarParser.h"
#include "RadarProtocol.h"
#include "PresenceRelay.h"

namespace WebPortal {

namespace {

ESP8266WebServer httpServer(80);
bool mdnsActive = false;

// Se conecta como estación a la red de la finca (además del AP propio), para
// poder entrar al portal desde cualquier equipo ya conectado a esa red.
void connectToStation() {
    Serial.print("[WIFI] Conectando a '");
    Serial.print(WIFI_STA_SSID);
    Serial.print("'");
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_STA_TIMEOUT_MS) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] No se pudo conectar a la red de la finca — sigue disponible por el AP");
        return;
    }

    Serial.print("[WIFI] Conectado — IP en la red de la finca: ");
    Serial.println(WiFi.localIP());

    mdnsActive = MDNS.begin(MDNS_HOSTNAME);
    if (mdnsActive) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("[WIFI] También disponible en http://");
        Serial.print(MDNS_HOSTNAME);
        Serial.println(".local/");
    }
}

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Radar Humano — Configuración</title>
<style>
  body { font-family: -apple-system, Arial, sans-serif; background:#111; color:#eee; margin:0; padding:12px; }
  h1 { font-size:1.2em; margin:0 0 4px; }
  .sub { color:#888; font-size:0.85em; margin-bottom:14px; }
  .panel { background:#1c1c1c; border-radius:8px; padding:12px; margin-bottom:14px; }
  .row { display:flex; justify-content:space-between; margin:4px 0; font-size:0.95em; }
  .row b { color:#fff; }
  #relay.on { color:#4caf50; font-weight:bold; }
  #relay.off { color:#888; }
  table { width:100%; border-collapse:collapse; font-size:0.85em; }
  th, td { padding:6px 4px; text-align:center; border-bottom:1px solid #333; }
  th { color:#999; font-weight:normal; }
  input[type=number] { width:52px; background:#222; color:#eee; border:1px solid #444; border-radius:4px; padding:4px; }
  input:disabled { opacity:0.3; }
  button { background:#2d6cdf; color:#fff; border:none; border-radius:4px; padding:6px 10px; font-size:0.85em; }
  button:disabled { opacity:0.6; }
  .barbg { background:#333; height:8px; width:70px; border-radius:4px; margin:0 auto 2px; overflow:hidden; }
  .bar { height:100%; width:0%; background:#4caf50; }
  .bar.trig { background:#e53935; }
  #cfgStatus { color:#888; font-size:0.8em; margin-top:8px; }
  .reload { margin-bottom:10px; }
</style>
</head>
<body>
  <h1>Radar Humano — LD2410B</h1>
  <div class="sub">Estado en vivo por gate y sensibilidad (0.75 m por gate aprox.)</div>

  <div class="panel">
    <div class="row"><span>Estado</span><b id="state">—</b></div>
    <div class="row"><span>Distancia movimiento</span><b id="movDist">— cm</b></div>
    <div class="row"><span>Distancia estática</span><b id="statDist">— cm</b></div>
    <div class="row"><span>Relé</span><b id="relay">—</b></div>
  </div>

  <div class="panel">
    <div class="row"><span>Uptime</span><b id="uptime">—</b></div>
    <div class="row"><span>Heap libre</span><b id="heap">—</b></div>
    <div class="row"><span>Último reinicio</span><b id="resetReason">—</b></div>
    <div class="row"><span>WiFi finca</span><b id="staInfo">—</b></div>
  </div>

  <div class="panel">
    <button class="reload" onclick="loadConfig()">Recargar configuración del sensor</button>
    <div id="cfgStatus"></div>
    <table>
      <thead>
        <tr><th>Gate</th><th>Energía mov.</th><th>Sens. mov.</th><th>Energía est.</th><th>Sens. est.</th><th></th></tr>
      </thead>
      <tbody id="gatesBody"></tbody>
    </table>
  </div>

<script>
const GATES = 9;

function buildTable() {
  const tbody = document.getElementById('gatesBody');
  let html = '';
  for (let i = 0; i < GATES; i++) {
    const staticDisabled = (i < 2) ? 'disabled' : '';
    html += '<tr>' +
      '<td>' + i + '</td>' +
      '<td><div class="barbg"><div class="bar" id="mbar' + i + '"></div></div><span id="mval' + i + '">0</span></td>' +
      '<td><input type="number" id="msens' + i + '" min="0" max="100" value="20"></td>' +
      '<td><div class="barbg"><div class="bar" id="sbar' + i + '"></div></div><span id="sval' + i + '">0</span></td>' +
      '<td><input type="number" id="ssens' + i + '" min="0" max="100" value="20" ' + staticDisabled + '></td>' +
      '<td><button id="savebtn' + i + '" onclick="saveGate(' + i + ')">Guardar</button></td>' +
    '</tr>';
  }
  tbody.innerHTML = html;
}

function stateLabel(s) {
  switch (s) {
    case 0: return 'Sin objetivo';
    case 1: return 'Movimiento';
    case 2: return 'Estático';
    case 3: return 'Movimiento + estático';
    default: return '—';
  }
}

function setBar(kind, gate, energy) {
  const bar = document.getElementById(kind + 'bar' + gate);
  const val = document.getElementById(kind + 'val' + gate);
  const sensInput = document.getElementById((kind === 'm' ? 'm' : 's') + 'sens' + gate);
  if (!bar) return;
  bar.style.width = Math.min(energy, 100) + '%';
  val.textContent = energy;
  const sens = sensInput ? parseInt(sensInput.value || '0', 10) : 0;
  bar.classList.toggle('trig', energy >= sens);
}

function formatUptime(totalSeconds) {
  const h = Math.floor(totalSeconds / 3600);
  const m = Math.floor((totalSeconds % 3600) / 60);
  const s = totalSeconds % 60;
  return h + 'h ' + m + 'm ' + s + 's';
}

async function refreshStatus() {
  try {
    const r = await fetch('/status.json');
    const d = await r.json();
    document.getElementById('state').textContent = stateLabel(d.state);
    const relayEl = document.getElementById('relay');
    relayEl.textContent = d.relay ? 'ENCENDIDO' : 'apagado';
    relayEl.className = d.relay ? 'on' : 'off';
    document.getElementById('movDist').textContent = d.movDist + ' cm';
    document.getElementById('statDist').textContent = d.statDist + ' cm';
    if (d.eng) {
      for (let i = 0; i < GATES; i++) {
        setBar('m', i, d.mov[i]);
        setBar('s', i, d.stat[i]);
      }
    }
    document.getElementById('uptime').textContent = formatUptime(d.uptimeS);
    document.getElementById('heap').textContent = d.heap + ' B (' + d.heapFrag + '% frag.)';
    document.getElementById('resetReason').textContent = d.resetReason;
    document.getElementById('staInfo').textContent = d.staConnected
      ? d.staIp + ' (' + d.staRssi + ' dBm)'
      : 'no conectado';
  } catch (e) { /* red inestable, se reintenta en el próximo ciclo */ }
}

async function loadConfig() {
  document.getElementById('cfgStatus').textContent = 'Leyendo configuración del sensor...';
  try {
    const r = await fetch('/api/config');
    const d = await r.json();
    if (!d.ok) throw new Error('el sensor no respondió');
    for (let i = 0; i < GATES; i++) {
      document.getElementById('msens' + i).value = d.motion[i];
      document.getElementById('ssens' + i).value = d.static[i];
    }
    document.getElementById('cfgStatus').textContent = 'Configuración cargada del sensor.';
  } catch (e) {
    document.getElementById('cfgStatus').textContent = 'Error leyendo configuración: ' + e;
  }
}

async function saveGate(gate) {
  const motion = document.getElementById('msens' + gate).value;
  const staticS = document.getElementById('ssens' + gate).value;
  const btn = document.getElementById('savebtn' + gate);
  btn.disabled = true; btn.textContent = '...';
  try {
    const r = await fetch('/api/sensitivity', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'gate=' + gate + '&motion=' + motion + '&static=' + staticS
    });
    const d = await r.json();
    btn.textContent = d.ok ? 'Guardado' : 'Error';
  } catch (e) {
    btn.textContent = 'Error';
  }
  setTimeout(() => { btn.disabled = false; btn.textContent = 'Guardar'; }, 1500);
}

setInterval(refreshStatus, 700);
window.onload = () => { buildTable(); refreshStatus(); loadConfig(); };
</script>
</body>
</html>
)HTML";

void handleRoot() {
    httpServer.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    const RadarParser::Telemetry &t = RadarParser::telemetry();
    bool staConnected = WiFi.status() == WL_CONNECTED;
    char buf[500];
    int n = snprintf(buf, sizeof(buf),
        "{\"state\":%u,\"relay\":%s,\"movDist\":%u,\"statDist\":%u,\"detectDist\":%u,\"eng\":%s,"
        "\"mov\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
        "\"stat\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
        "\"light\":%u,"
        "\"uptimeS\":%lu,\"heap\":%u,\"heapFrag\":%u,\"resetReason\":\"%s\","
        "\"staConnected\":%s,\"staIp\":\"%s\",\"staRssi\":%d}",
        t.targetState, PresenceRelay::isOn() ? "true" : "false", t.movingDistanceCm, t.staticDistanceCm, t.detectDistanceCm,
        t.engineering ? "true" : "false",
        t.movingEnergy[0], t.movingEnergy[1], t.movingEnergy[2], t.movingEnergy[3], t.movingEnergy[4],
        t.movingEnergy[5], t.movingEnergy[6], t.movingEnergy[7], t.movingEnergy[8],
        t.staticEnergy[0], t.staticEnergy[1], t.staticEnergy[2], t.staticEnergy[3], t.staticEnergy[4],
        t.staticEnergy[5], t.staticEnergy[6], t.staticEnergy[7], t.staticEnergy[8],
        t.lightValue,
        millis() / 1000, ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getResetReason().c_str(),
        staConnected ? "true" : "false",
        staConnected ? WiFi.localIP().toString().c_str() : "",
        staConnected ? WiFi.RSSI() : 0);
    httpServer.send(200, "application/json", n > 0 ? buf : "{}");
}

void handleGetConfig() {
    RadarProtocol::GateConfig cfg = RadarProtocol::readConfig();
    if (!cfg.ok) {
        httpServer.send(200, "application/json", "{\"ok\":false}");
        return;
    }
    char buf[300];
    int n = snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"maxMovingGate\":%u,\"maxStaticGate\":%u,\"noOneDuration\":%u,"
        "\"motion\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],"
        "\"static\":[%u,%u,%u,%u,%u,%u,%u,%u,%u]}",
        cfg.maxMovingGate, cfg.maxStaticGate, cfg.noOneDuration,
        cfg.motionSens[0], cfg.motionSens[1], cfg.motionSens[2], cfg.motionSens[3], cfg.motionSens[4],
        cfg.motionSens[5], cfg.motionSens[6], cfg.motionSens[7], cfg.motionSens[8],
        cfg.staticSens[0], cfg.staticSens[1], cfg.staticSens[2], cfg.staticSens[3], cfg.staticSens[4],
        cfg.staticSens[5], cfg.staticSens[6], cfg.staticSens[7], cfg.staticSens[8]);
    httpServer.send(200, "application/json", n > 0 ? buf : "{\"ok\":false}");
}

void handleSetSensitivity() {
    if (!httpServer.hasArg("gate") || !httpServer.hasArg("motion") || !httpServer.hasArg("static")) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"faltan parametros\"}");
        return;
    }
    int gate    = httpServer.arg("gate").toInt();
    int motion  = httpServer.arg("motion").toInt();
    int staticS = httpServer.arg("static").toInt();
    if (gate < 0 || gate >= GATE_COUNT || motion < 0 || motion > 100 || staticS < 0 || staticS > 100) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"fuera de rango\"}");
        return;
    }
    bool ok = RadarProtocol::applyGateSensitivity((uint8_t)gate, (uint8_t)motion, (uint8_t)staticS);
    httpServer.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

} // namespace

void begin() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    Serial.print("[WIFI] AP activo — SSID: ");
    Serial.print(WIFI_AP_SSID);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());

    connectToStation();

    httpServer.on("/", HTTP_GET, handleRoot);
    httpServer.on("/status.json", HTTP_GET, handleStatus);
    httpServer.on("/api/config", HTTP_GET, handleGetConfig);
    httpServer.on("/api/sensitivity", HTTP_POST, handleSetSensitivity);
    httpServer.begin();
    Serial.println("[HTTP] Servidor iniciado");
}

void handleClient() {
    httpServer.handleClient();
    if (mdnsActive) MDNS.update();
}

} // namespace WebPortal
