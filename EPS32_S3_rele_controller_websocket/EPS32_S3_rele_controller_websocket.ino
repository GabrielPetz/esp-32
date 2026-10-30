
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ============================================================
// ESP32-S3 - CONTROLADOR DE 4 GPIOs VIA WEBSOCKET
// ============================================================

// ------------------------------------------------------------
// CONFIGURACOES DE REDE
// ------------------------------------------------------------

const char* WIFI_SSID = "Paula e Gabriel 2G";
const char* WIFI_PASSWORD = "Paula492*";

const char* AP_SSID = "ESP32-RELES";
const char* AP_PASSWORD = "12345678";

// ------------------------------------------------------------
// CONFIGURACOES DE PORTAS
// ------------------------------------------------------------

#define HTTP_PORT 80
#define WEBSOCKET_PORT 81

const uint8_t GPIO_PINS[4] = {
  4, 5, 6, 7
};

const uint8_t NUM_CHANNELS = 4;

// HIGH = Saida ativa (3.3V)
// LOW = Saida inativa (0V)
//
// Para S9014 em configuracao de chave NPN:
// HIGH -> transistor conduz
// LOW  -> transistor desliga

const uint8_t OUTPUT_ON = HIGH;
const uint8_t OUTPUT_OFF = LOW;

// ------------------------------------------------------------
// ESTADO
// ------------------------------------------------------------

bool channelState[NUM_CHANNELS] = {
  false, false, false, false
};

WebServer httpServer(HTTP_PORT);

WebSocketsServer webSocket(WEBSOCKET_PORT);

// ============================================================
// CONTROLE DOS GPIOs
// ============================================================

void setChannel(uint8_t index, bool enabled) {

  if (index >= NUM_CHANNELS) {
    return;
  }

  channelState[index] = enabled;

  digitalWrite(
    GPIO_PINS[index],
    enabled ? OUTPUT_ON : OUTPUT_OFF
  );

  Serial.printf(
    "[GPIO%d] Canal %d: %s (%s)\n",
    GPIO_PINS[index],
    index + 1,
    enabled ? "LIGADO" : "DESLIGADO",
    enabled ? "HIGH - 3.3V" : "LOW - 0V"
  );
}

void setAllChannels(bool enabled) {

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    setChannel(i, enabled);
  }
}

// ============================================================
// JSON DE ESTADO
// ============================================================

String buildStateJSON() {

  String json = "{\"type\":\"state\",\"channels\":[";

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {

    if (i > 0) {
      json += ",";
    }

    json += channelState[i] ? "true" : "false";
  }

  json += "]}";

  return json;
}

// Envia estado para apenas um navegador
void sendStateToClient(uint8_t clientId) {

  String json = buildStateJSON();

  webSocket.sendTXT(clientId, json);
}

// Envia estado para todos os navegadores
void broadcastState() {

  String json = buildStateJSON();

  webSocket.broadcastTXT(json);

  Serial.print("[WS] Estado transmitido: ");
  Serial.println(json);
}

// ============================================================
// EVENTOS WEBSOCKET
// ============================================================

void handleWebSocketEvent(
  uint8_t clientId,
  WStype_t type,
  uint8_t* payload,
  size_t length
) {

  switch (type) {

    case WStype_CONNECTED: {

      IPAddress clientIP =
        webSocket.remoteIP(clientId);

      Serial.printf(
        "[WS] Cliente %u conectado: %s\n",
        clientId,
        clientIP.toString().c_str()
      );

      // Envia o estado atual imediatamente
      sendStateToClient(clientId);

      break;
    }

    case WStype_DISCONNECTED: {

      Serial.printf(
        "[WS] Cliente %u desconectado\n",
        clientId
      );

      break;
    }

    case WStype_TEXT: {

      // Limite para comandos curtos de controle
      if (length == 0 || length > 32) {
        return;
      }

      // Converte usando o comprimento recebido
      String message;

      message.reserve(length + 1);

      for (size_t i = 0; i < length; i++) {
        message += (char)payload[i];
      }

      Serial.printf(
        "[WS] Cliente %u enviou: %s\n",
        clientId,
        message.c_str()
      );

      // ------------------------------------------
      // CONSULTAR ESTADO
      // GET
      // ------------------------------------------

      if (message == "GET") {
        sendStateToClient(clientId);
        return;
      }

      // ------------------------------------------
      // ALTERAR CANAL INDIVIDUAL
      // SET:0:1 -> Liga GPIO4
      // SET:0:0 -> Desliga GPIO4
      // ------------------------------------------

      if (
        message.length() == 7 &&
        message.startsWith("SET:") &&
        message[5] == ':' &&
        message[4] >= '0' &&
        message[4] <= '3' &&
        (message[6] == '0' || message[6] == '1')
      ) {

        uint8_t index = message[4] - '0';

        bool enabled = message[6] == '1';

        setChannel(index, enabled);

        // Sincroniza todos os navegadores
        broadcastState();

        return;
      }

      // ------------------------------------------
      // ALTERAR TODOS OS CANAIS
      // ALL:1 -> Liga todos
      // ALL:0 -> Desliga todos
      // ------------------------------------------

      if (
        message == "ALL:1" ||
        message == "ALL:0"
      ) {

        bool enabled = message == "ALL:1";

        setAllChannels(enabled);

        broadcastState();

        return;
      }

      Serial.println("[WS] Comando invalido");

      webSocket.sendTXT(
        clientId,
        "{\"type\":\"error\",\"message\":\"Comando invalido\"}"
      );

      break;
    }

    case WStype_ERROR: {
      Serial.printf(
        "[WS] Erro no cliente %u\n",
        clientId
      );
      break;
    }

    default:
      break;
  }
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

<title>ESP32-S3 WebSocket Control</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  min-height: 100vh;
  padding: 20px;

  background: #0f121b;
  color: #ffffff;

  font-family: Arial, sans-serif;

  display: flex;
  justify-content: center;
  align-items: center;
}

.container {
  width: 100%;
  max-width: 470px;

  background: #1c2130;
  border-radius: 20px;
  padding: 25px;

  box-shadow:
    0 12px 40px rgba(0,0,0,.3);
}

h1 {
  text-align: center;
  font-size: 23px;
  margin: 0 0 8px;
}

.subtitle {
  text-align: center;
  color: #9ca3b5;
  margin-bottom: 20px;
}

.connection {
  padding: 12px;
  border-radius: 10px;
  background: #303b50;

  text-align: center;
  font-size: 13px;
  margin-bottom: 24px;
}

.connection.online {
  background: #14532d;
  color: #86efac;
}

.connection.offline {
  background: #7f1d1d;
  color: #fca5a5;
}

.grid {
  display: grid;
  grid-template-columns: repeat(2,1fr);
  gap: 13px;
}

.channel {
  background: #121723;
  border-radius: 13px;
  padding: 17px;
  border: 1px solid #ffffff10;
}

.channel h3 {
  margin: 0 0 7px;
  font-size: 17px;
}

.pin {
  font-size: 12px;
  color: #9ca3b5;
  margin-bottom: 18px;
}

.state {
  font-size: 13px;
  color: #9ca3b5;
  margin-bottom: 12px;
}

.state.on {
  color: #4ade80;
}

button {
  padding: 13px;
  border: none;
  border-radius: 9px;

  font-weight: bold;
  font-size: 14px;

  color: white;
  cursor: pointer;
}

button:disabled {
  opacity: .4;
  cursor: not-allowed;
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
  gap: 12px;
  margin-top: 23px;
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

.footer {
  margin-top: 22px;
  text-align: center;
  font-size: 12px;
  color: #9ca3b5;
}

@media(max-width:340px) {
  .grid {
    grid-template-columns: 1fr;
  }
}
</style>

</head>

<body>

<div class="container">

<h1>ESP32-S3 GPIO Control</h1>

<p class="subtitle">
  Controle em tempo real via WebSocket
</p>

<div id="connection" class="connection">
  Conectando...
</div>

<div class="grid">

<div class="channel">
  <h3>Canal 1</h3>
  <div class="pin">GPIO4</div>
  <div class="state" id="state0">Aguardando</div>
  <button class="toggle" id="btn0"
          onclick="toggleChannel(0)" disabled>
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 2</h3>
  <div class="pin">GPIO5</div>
  <div class="state" id="state1">Aguardando</div>
  <button class="toggle" id="btn1"
          onclick="toggleChannel(1)" disabled>
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 3</h3>
  <div class="pin">GPIO6</div>
  <div class="state" id="state2">Aguardando</div>
  <button class="toggle" id="btn2"
          onclick="toggleChannel(2)" disabled>
    Ligar
  </button>
</div>

<div class="channel">
  <h3>Canal 4</h3>
  <div class="pin">GPIO7</div>
  <div class="state" id="state3">Aguardando</div>
  <button class="toggle" id="btn3"
          onclick="toggleChannel(3)" disabled>
    Ligar
  </button>
</div>

</div>

<div class="actions">
  <button class="all-on" id="allOn"
          onclick="setAll(true)" disabled>
    Ligar todos
  </button>

  <button class="all-off" id="allOff"
          onclick="setAll(false)" disabled>
    Desligar todos
  </button>
</div>

<div class="footer">
  WebSocket · Porta 81
</div>

</div>

<script>

// ========================================================
// ESTADO
// ========================================================

let socket = null;

let connected = false;
let stateKnown = false;

let channelStates = [
  false, false, false, false
];

let reconnectTimer = null;

// ========================================================
// INTERFACE
// ========================================================

function updateConnection(status, message) {

  const element =
    document.getElementById("connection");

  element.textContent = message;

  element.className =
    "connection " + status;
}

function updateButtons() {

  const allowed = connected && stateKnown;

  for (let i = 0; i < 4; i++) {
    document.getElementById("btn" + i)
      .disabled = !allowed;
  }

  document.getElementById("allOn")
    .disabled = !allowed;

  document.getElementById("allOff")
    .disabled = !allowed;
}

function updateUI(states) {

  channelStates = states.map(Boolean);
  stateKnown = true;

  channelStates.forEach((enabled, index) => {

    const state =
      document.getElementById("state" + index);

    const button =
      document.getElementById("btn" + index);

    state.textContent =
      enabled ? "HIGH - 3.3V" : "LOW - 0V";

    state.classList.toggle("on", enabled);

    button.textContent =
      enabled ? "Desligar" : "Ligar";

    button.classList.toggle("on", enabled);
  });

  updateButtons();
}

// ========================================================
// WEBSOCKET
// ========================================================

function connectWebSocket() {

  if (socket &&
      (socket.readyState === WebSocket.OPEN ||
       socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  updateConnection(
    "",
    "Conectando ao ESP32-S3..."
  );

  const url =
    "ws://" + window.location.hostname + ":81/";

  const ws = new WebSocket(url);
  socket = ws;

  ws.onopen = function() {

    if (socket !== ws) return;

    connected = true;
    stateKnown = false;

    updateConnection(
      "online",
      "WebSocket conectado"
    );

    updateButtons();

    // Solicita sincronizacao inicial
    ws.send("GET");
  };

  ws.onmessage = function(event) {

    if (socket !== ws) return;

    try {

      const data = JSON.parse(event.data);

      if (
        data.type === "state" &&
        Array.isArray(data.channels) &&
        data.channels.length === 4 &&
        data.channels.every(v => typeof v === "boolean")
      ) {

        updateUI(data.channels);
      }

      if (data.type === "error") {
        console.error(
          "Erro ESP32:",
          data.message
        );
      }

    } catch (error) {
      console.error(
        "Mensagem invalida:",
        error
      );
    }
  };

  ws.onerror = function(error) {
    console.error("WebSocket error:", error);
  };

  ws.onclose = function() {

    if (socket !== ws) return;

    connected = false;
    stateKnown = false;

    updateButtons();

    for (let i = 0; i < 4; i++) {
      document.getElementById(
        "state" + i
      ).textContent = "Desconectado";
    }

    updateConnection(
      "offline",
      "Desconectado - reconectando..."
    );

    clearTimeout(reconnectTimer);

    reconnectTimer = setTimeout(
      connectWebSocket,
      2000
    );
  };
}

// ========================================================
// ENVIO DE COMANDOS
// ========================================================

function sendCommand(command) {

  if (
    !connected ||
    !stateKnown ||
    !socket ||
    socket.readyState !== WebSocket.OPEN
  ) {
    return;
  }

  socket.send(command);
}

function toggleChannel(index) {

  const newState = !channelStates[index];

  // SET:0:1
  // SET:0:0

  sendCommand(
    "SET:" + index + ":" + (newState ? 1 : 0)
  );
}

function setAll(enabled) {

  // ALL:1 ou ALL:0

  sendCommand(
    "ALL:" + (enabled ? 1 : 0)
  );
}

// ========================================================
// INICIALIZACAO
// ========================================================

updateButtons();
connectWebSocket();

</script>

</body>
</html>
)HTML";

// ============================================================
// SERVIDOR HTTP - APENAS PAGINA HTML
// ============================================================

void handleRoot() {

  httpServer.send_P(
    200,
    "text/html; charset=utf-8",
    WEB_PAGE
  );
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

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 15000
  ) {

    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("[WIFI] Conectado");

    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println();
    Serial.println("[WIFI] Iniciando Access Point");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );

    Serial.print("[AP] Rede: ");
    Serial.println(AP_SSID);

    Serial.print("[AP] Senha: ");
    Serial.println(AP_PASSWORD);

    Serial.print("[AP] IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  // Configurar todos os GPIOs desligados
  // antes de iniciar o Wi-Fi.

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {

    digitalWrite(GPIO_PINS[i], OUTPUT_OFF);

    pinMode(
      GPIO_PINS[i],
      OUTPUT
    );
  }

  Serial.begin(115200);

  unsigned long start = millis();

  while (!Serial && millis() - start < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 WEBSOCKET CONTROLLER");
  Serial.println("================================");

  setAllChannels(false);

  setupWiFi();

  // HTTP usado somente para servir a interface
  httpServer.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  httpServer.onNotFound([]() {
    httpServer.send(
      404,
      "text/plain",
      "Pagina nao encontrada"
    );
  });

  httpServer.begin();

  Serial.println("[HTTP] Servidor porta 80 iniciado");

  // Servidor WebSocket
  webSocket.begin();

  webSocket.onEvent(
    handleWebSocketEvent
  );

  // Heartbeat para detectar conexoes inativas
  webSocket.enableHeartbeat(
    15000,
    3000,
    2
  );

  Serial.println("[WS] Servidor porta 81 iniciado");
  Serial.println("[SYSTEM] Pronto para receber comandos");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  httpServer.handleClient();

  webSocket.loop();
}
