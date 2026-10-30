
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
// ESP32-S3 - CONTROLADOR WEB DE 4 SAIDAS
// ============================================================

// Wi-Fi principal
const char* WIFI_SSID = "Paula e Gabriel 2G";
const char* WIFI_PASSWORD = "Paula492*";

// Access Point de emergencia
const char* AP_SSID = "ESP32-RELES";
const char* AP_PASSWORD = "12345678";

// GPIOs utilizados
const uint8_t RELAY_PINS[4] = {
  4, 5, 6, 7
};

// HIGH = sinal de 3.3V
// LOW  = sinal de 0V
const uint8_t RELAY_ON = HIGH;
const uint8_t RELAY_OFF = LOW;

bool relayState[4] = {
  false, false, false, false
};

WebServer server(80);

// ============================================================
// CONTROLE DOS GPIOs
// ============================================================

void setRelay(uint8_t index, bool enabled) {

  if (index >= 4) return;

  relayState[index] = enabled;

  digitalWrite(
    RELAY_PINS[index],
    enabled ? RELAY_ON : RELAY_OFF
  );

  Serial.printf(
    "[GPIO%d] CANAL %d -> %s (%s)\n",
    RELAY_PINS[index],
    index + 1,
    enabled ? "LIGADO" : "DESLIGADO",
    enabled ? "HIGH - 3.3V" : "LOW - 0V"
  );
}

void setAllRelays(bool enabled) {

  for (uint8_t i = 0; i < 4; i++) {
    setRelay(i, enabled);
  }
}

// ============================================================
// JSON DO ESTADO
// ============================================================

String getStateJSON() {

  String json = "{\"relays\":[";

  for (uint8_t i = 0; i < 4; i++) {

    if (i > 0) json += ",";

    json += relayState[i] ? "true" : "false";
  }

  json += "]}";

  return json;
}

// ============================================================
// PAGINA WEB
// ============================================================

const char WEB_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>ESP32-S3 - Controle GPIO</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  min-height: 100vh;
  padding: 20px;
  background: #10131d;
  color: white;
  font-family: Arial, sans-serif;
  display: flex;
  align-items: center;
  justify-content: center;
}

.container {
  width: 100%;
  max-width: 460px;
  background: #1c2130;
  padding: 25px;
  border-radius: 20px;
}

h1 {
  margin: 0;
  text-align: center;
  font-size: 24px;
}

.subtitle {
  text-align: center;
  color: #a1a8b8;
  margin: 12px 0 25px;
}

.grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

.channel {
  padding: 18px;
  background: #121723;
  border-radius: 12px;
}

.channel h3 {
  margin: 0 0 10px;
}

.pin {
  color: #a1a8b8;
  font-size: 12px;
  margin-bottom: 20px;
}

.state {
  margin-bottom: 12px;
  color: #a1a8b8;
  font-size: 13px;
}

.state.on {
  color: #4ade80;
}

button {
  cursor: pointer;
  border: none;
  color: white;
  font-weight: bold;
  padding: 13px;
  border-radius: 9px;
}

.toggle {
  width: 100%;
  background: #384153;
}

.toggle.on {
  background: #16a34a;
}

.actions {
  display: flex;
  gap: 10px;
  margin-top: 20px;
}

.actions button {
  flex: 1;
}

.all-on {
  background: #16a34a;
}

.all-off {
  background: #dc2626;
}

.status {
  text-align: center;
  margin-top: 20px;
  font-size: 13px;
  color: #a1a8b8;
}
</style>
</head>

<body>
<div class="container">

<h1>ESP32-S3 GPIO Control</h1>

<p class="subtitle">
Controle de 4 canais por Wi-Fi
</p>

<div class="grid">

<div class="channel">
  <h3>Canal 1</h3>
  <div class="pin">GPIO4</div>
  <div id="state0" class="state">Desligado</div>
  <button id="btn0" class="toggle"
          onclick="toggleRelay(0)">
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 2</h3>
  <div class="pin">GPIO5</div>
  <div id="state1" class="state">Desligado</div>
  <button id="btn1" class="toggle"
          onclick="toggleRelay(1)">
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 3</h3>
  <div class="pin">GPIO6</div>
  <div id="state2" class="state">Desligado</div>
  <button id="btn2" class="toggle"
          onclick="toggleRelay(2)">
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 4</h3>
  <div class="pin">GPIO7</div>
  <div id="state3" class="state">Desligado</div>
  <button id="btn3" class="toggle"
          onclick="toggleRelay(3)">
    Ligar
  </button>
</div>

</div>

<div class="actions">
  <button class="all-on" onclick="setAll(true)">
    Ligar todos
  </button>
  <button class="all-off" onclick="setAll(false)">
    Desligar todos
  </button>
</div>

<div id="status" class="status">
  Conectando...
</div>

</div>

<script>

let relayStates = [false, false, false, false];

function updateUI(states) {

  relayStates = states;

  states.forEach((enabled, index) => {

    const state =
      document.getElementById("state" + index);

    const button =
      document.getElementById("btn" + index);

    state.textContent = enabled
      ? "HIGH - 3.3V"
      : "LOW - 0V";

    state.classList.toggle("on", enabled);

    button.textContent = enabled
      ? "Desligar"
      : "Ligar";

    button.classList.toggle("on", enabled);
  });
}

async function loadStates() {

  try {

    const response = await fetch(
      "/api/state",
      {cache: "no-store"}
    );

    if (!response.ok) {
      throw new Error("HTTP");
    }

    const data = await response.json();

    updateUI(data.relays);

    document.getElementById("status")
      .textContent = "Conectado ao ESP32-S3";

  } catch (error) {

    document.getElementById("status")
      .textContent = "Erro de comunicacao";
  }
}

async function toggleRelay(index) {

  const enabled = !relayStates[index];

  try {

    const response = await fetch(
      "/api/relay?id=" + index +
      "&state=" + (enabled ? 1 : 0),
      {cache: "no-store"}
    );

    if (!response.ok) {
      throw new Error("HTTP");
    }

    await loadStates();

  } catch (error) {
    await loadStates();
  }
}

async function setAll(enabled) {

  try {

    const response = await fetch(
      "/api/all?state=" + (enabled ? 1 : 0),
      {cache: "no-store"}
    );

    if (!response.ok) {
      throw new Error("HTTP");
    }

    await loadStates();

  } catch (error) {
    await loadStates();
  }
}

setInterval(loadStates, 2000);
loadStates();

</script>
</body>
</html>
)HTML";

// ============================================================
// ROTAS HTTP
// ============================================================

void handleRoot() {

  server.send_P(
    200,
    "text/html; charset=utf-8",
    WEB_PAGE
  );
}

void handleState() {

  server.send(
    200,
    "application/json",
    getStateJSON()
  );
}

void handleRelay() {

  if (!server.hasArg("id") ||
      !server.hasArg("state")) {

    server.send(400, "text/plain", "Parametros ausentes");
    return;
  }

  String id = server.arg("id");
  String value = server.arg("state");

  if (id.length() != 1 ||
      id[0] < '0' || id[0] > '3' ||
      (value != "0" && value != "1")) {

    server.send(400, "text/plain", "Parametros invalidos");
    return;
  }

  setRelay(
    id[0] - '0',
    value == "1"
  );

  handleState();
}

void handleAll() {

  if (!server.hasArg("state")) {

    server.send(400, "text/plain", "Parametro ausente");
    return;
  }

  String value = server.arg("state");

  if (value != "0" && value != "1") {

    server.send(400, "text/plain", "Estado invalido");
    return;
  }

  setAllRelays(value == "1");

  handleState();
}

// ============================================================
// WIFI
// ============================================================

void setupWiFi() {

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("[WIFI] Conectando");

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("[WIFI] Conectado");

    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println();
    Serial.println("[WIFI] Criando Access Point");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );

    Serial.print("[AP] SSID: ");
    Serial.println(AP_SSID);

    Serial.print("[AP] IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // Inicializa GPIOs em LOW
  for (uint8_t i = 0; i < 4; i++) {

    digitalWrite(RELAY_PINS[i], LOW);
    pinMode(RELAY_PINS[i], OUTPUT);
  }

  Serial.begin(115200);

  unsigned long start = millis();

  while (!Serial && millis() - start < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("=============================");
  Serial.println("ESP32-S3 - CONTROLE DE GPIO");
  Serial.println("=============================");

  setAllRelays(false);

  setupWiFi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/relay", HTTP_GET, handleRelay);
  server.on("/api/all", HTTP_GET, handleAll);

  server.begin();

  Serial.println("[HTTP] Servidor iniciado");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();
}
