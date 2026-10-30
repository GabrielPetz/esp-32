
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

// ============================================================
// CONFIGURACOES DO ESP32-S3
// ============================================================

// LED WS2812 integrado
#define RGB_PIN 48
#define NUM_LEDS 1

// Credenciais da rede Wi-Fi existente
const char* WIFI_SSID = "Paula e Gabriel 2G";
const char* WIFI_PASSWORD = "Paula492*";

// Rede propria (fallback) do ESP32
const char* AP_SSID = "ESP32-RGB";
const char* AP_PASSWORD = "12345678";

// ============================================================
// OBJETOS E ESTADO
// ============================================================

Adafruit_NeoPixel led(
  NUM_LEDS,
  RGB_PIN,
  NEO_GRB + NEO_KHZ800
);

WebServer server(80);

uint8_t red = 138;
uint8_t green = 92;
uint8_t blue = 246;

uint8_t brightness = 70;
bool ledEnabled = true;

// ============================================================
// CONTROLE DO LED
// ============================================================

void updateLED() {
  if (!ledEnabled) {
    led.clear();
  } else {
    led.setPixelColor(
      0,
      led.Color(red, green, blue)
    );
  }

  led.setBrightness(brightness);
  led.show();
}

String colorToHex() {
  char buffer[8];

  snprintf(
    buffer,
    sizeof(buffer),
    "#%02X%02X%02X",
    red,
    green,
    blue
  );

  return String(buffer);
}

// ============================================================
// INTERFACE WEB
// ============================================================

const char WEB_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="pt-BR">

<head>
<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>ESP32 RGB Control</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  font-family: Arial, sans-serif;
  background: #10121b;
  color: #ffffff;

  display: flex;
  justify-content: center;
  align-items: center;

  min-height: 100vh;
  padding: 20px;
}

.container {
  width: 100%;
  max-width: 430px;

  padding: 28px;
  border-radius: 22px;

  background: #1b1e2b;

  box-shadow:
    0 12px 40px rgba(0,0,0,.35);
}

h1 {
  margin: 0 0 8px;
  text-align: center;
  font-size: 25px;
}

.subtitle {
  text-align: center;
  color: #999eae;
  margin-bottom: 28px;
}

.preview {
  display: flex;
  flex-direction: column;
  align-items: center;

  background: #141723;
  padding: 25px;
  border-radius: 16px;
  margin-bottom: 25px;
}

.led-preview {
  width: 110px;
  height: 110px;
  border-radius: 50%;

  background: #8b5cf6;
  box-shadow: 0 0 45px #8b5cf666;

  margin-bottom: 18px;
}

.hex-value {
  font-size: 21px;
  font-weight: bold;
}

.rgb-value {
  color: #999eae;
  font-size: 13px;
  margin-top: 7px;
}

label {
  display: block;
  margin-bottom: 12px;
  font-weight: bold;
}

.color-input {
  width: 100%;
  height: 58px;
  border: none;
  padding: 0;
  cursor: pointer;
  background: none;
}

.presets {
  display: flex;
  gap: 9px;
  margin: 20px 0 28px;
}

.preset {
  flex: 1;
  height: 40px;
  border: 2px solid #ffffff33;
  border-radius: 9px;
  cursor: pointer;
}

.brightness-header {
  display: flex;
  justify-content: space-between;
}

input[type="range"] {
  width: 100%;
  accent-color: #8b5cf6;
  cursor: pointer;
}

button.power {
  width: 100%;
  padding: 16px;

  margin-top: 28px;
  border: none;
  border-radius: 12px;

  background: #e5484d;
  color: white;

  font-size: 16px;
  font-weight: bold;
  cursor: pointer;
}

button.power.off {
  background: #22a06b;
}

.status {
  text-align: center;
  margin-top: 20px;
  font-size: 13px;
  color: #aaa;
}

</style>
</head>

<body>

<div class="container">

  <h1>ESP32 RGB Control</h1>

  <div class="subtitle">
    Controle seu LED WS2812
  </div>

  <div class="preview">

    <div id="ledPreview"
         class="led-preview">
    </div>

    <div id="hexValue"
         class="hex-value">
      #8A5CF6
    </div>

    <div id="rgbValue"
         class="rgb-value">
      RGB(138, 92, 246)
    </div>

  </div>

  <label>Selecione uma cor</label>

  <input
    type="color"
    id="colorPicker"
    class="color-input"
    value="#8a5cf6"
  >

  <div class="presets">
    <button class="preset"
      style="background:#ff0000"
      onclick="setColor('#ff0000')"></button>

    <button class="preset"
      style="background:#ff9500"
      onclick="setColor('#ff9500')"></button>

    <button class="preset"
      style="background:#ffff00"
      onclick="setColor('#ffff00')"></button>

    <button class="preset"
      style="background:#00ff00"
      onclick="setColor('#00ff00')"></button>

    <button class="preset"
      style="background:#00ffff"
      onclick="setColor('#00ffff')"></button>

    <button class="preset"
      style="background:#0000ff"
      onclick="setColor('#0000ff')"></button>

    <button class="preset"
      style="background:#8b5cf6"
      onclick="setColor('#8b5cf6')"></button>
  </div>

  <div class="brightness-header">
    <label>Brilho</label>

    <span id="brightnessValue">
      70%
    </span>
  </div>

  <input
    type="range"
    id="brightness"
    min="0"
    max="100"
    value="70"
  >

  <button
    id="powerButton"
    class="power"
    onclick="toggleLED()">
    Desligar LED
  </button>

  <div class="status" id="status">
    Conectando...
  </div>

</div>

<script>

let currentColor = "#8a5cf6";
let currentBrightness = 70;
let enabled = true;

let updateTimer = null;

const picker =
  document.getElementById("colorPicker");

const slider =
  document.getElementById("brightness");

const preview =
  document.getElementById("ledPreview");

const statusElement =
  document.getElementById("status");

function setStatus(message) {
  statusElement.textContent = message;
}

function hexToRGB(hex) {
  return {
    r: parseInt(hex.substring(1, 3), 16),
    g: parseInt(hex.substring(3, 5), 16),
    b: parseInt(hex.substring(5, 7), 16)
  };
}

function updatePreview() {

  const rgb = hexToRGB(currentColor);

  const factor =
    enabled ? currentBrightness / 100 : 0;

  const r = Math.round(rgb.r * factor);
  const g = Math.round(rgb.g * factor);
  const b = Math.round(rgb.b * factor);

  const displayColor =
    `rgb(${r},${g},${b})`;

  preview.style.background = displayColor;

  preview.style.boxShadow =
    enabled
    ? `0 0 45px ${displayColor}`
    : "none";

  document.getElementById("hexValue")
    .textContent =
      enabled ? currentColor.toUpperCase() : "OFF";

  document.getElementById("rgbValue")
    .textContent =
      `RGB(${r}, ${g}, ${b})`;

  document.getElementById("brightnessValue")
    .textContent =
      currentBrightness + "%";

  const button =
    document.getElementById("powerButton");

  button.textContent =
    enabled ? "Desligar LED" : "Ligar LED";

  button.classList.toggle("off", !enabled);
}

async function sendState() {

  const rgb = hexToRGB(currentColor);

  const params = new URLSearchParams({
    r: rgb.r,
    g: rgb.g,
    b: rgb.b,
    brightness: currentBrightness,
    enabled: enabled ? 1 : 0
  });

  try {

    const response = await fetch(
      "/api/set?" + params.toString(),
      { cache: "no-store" }
    );

    if (!response.ok) {
      throw new Error("Erro HTTP");
    }

    setStatus("Conectado ao ESP32");

  } catch (error) {
    setStatus("Erro de comunicacao");
  }
}

function scheduleUpdate() {
  clearTimeout(updateTimer);

  updateTimer = setTimeout(
    sendState,
    50
  );
}

function setColor(color) {
  currentColor = color;
  enabled = true;

  picker.value = color;

  updatePreview();
  scheduleUpdate();
}

function toggleLED() {
  enabled = !enabled;

  updatePreview();
  scheduleUpdate();
}

picker.addEventListener("input", function() {
  currentColor = picker.value;
  enabled = true;

  updatePreview();
  scheduleUpdate();
});

slider.addEventListener("input", function() {
  currentBrightness = Number(slider.value);

  updatePreview();
  scheduleUpdate();
});

async function loadState() {

  try {
    const response = await fetch(
      "/api/state",
      { cache: "no-store" }
    );

    if (!response.ok) {
      throw new Error("Erro HTTP");
    }

    const state = await response.json();

    currentColor = state.color;
    currentBrightness = state.brightness;
    enabled = state.enabled;

    picker.value = currentColor;
    slider.value = currentBrightness;

    updatePreview();

    setStatus("Conectado ao ESP32");

  } catch (error) {
    setStatus("ESP32 indisponivel");
  }
}

updatePreview();
loadState();

</script>

</body>
</html>
)HTML";

// ============================================================
// API WEB
// ============================================================

void handleRoot() {
  server.send_P(
    200,
    "text/html; charset=utf-8",
    WEB_PAGE
  );
}

void handleSet() {

  if (server.hasArg("r")) {
    red = constrain(
      server.arg("r").toInt(),
      0,
      255
    );
  }

  if (server.hasArg("g")) {
    green = constrain(
      server.arg("g").toInt(),
      0,
      255
    );
  }

  if (server.hasArg("b")) {
    blue = constrain(
      server.arg("b").toInt(),
      0,
      255
    );
  }

  if (server.hasArg("brightness")) {
    int percent = constrain(
      server.arg("brightness").toInt(),
      0,
      100
    );

    brightness = map(
      percent,
      0,
      100,
      0,
      255
    );
  }

  if (server.hasArg("enabled")) {
    ledEnabled =
      server.arg("enabled") == "1";
  }

  updateLED();

  server.send(
    200,
    "application/json",
    "{\"success\":true}"
  );
}

void handleState() {

  int percent = map(
    brightness,
    0,
    255,
    0,
    100
  );

  String json = "{";

  json += "\"color\":\"";
  json += colorToHex();
  json += "\",";

  json += "\"brightness\":";
  json += String(percent);
  json += ",";

  json += "\"enabled\":";
  json += ledEnabled ? "true" : "false";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// CONFIGURACAO WI-FI
// ============================================================

void setupWiFi() {

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("Conectando ao Wi-Fi");

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 15000
  ) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("Wi-Fi conectado!");
    Serial.print("Endereco IP: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println();
    Serial.println("Iniciando Access Point");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );

    Serial.print("Rede: ");
    Serial.println(AP_SSID);

    Serial.print("Senha: ");
    Serial.println(AP_PASSWORD);

    Serial.print("Endereco IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==========================");
  Serial.println("ESP32-S3 RGB WEB CONTROL");
  Serial.println("==========================");

  led.begin();
  led.clear();

  updateLED();

  setupWiFi();

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/api/set",
    HTTP_GET,
    handleSet
  );

  server.on(
    "/api/state",
    HTTP_GET,
    handleState
  );

  server.onNotFound([]() {
    server.send(
      404,
      "text/plain",
      "Pagina nao encontrada"
    );
  });

  server.begin();

  Serial.println("Servidor HTTP iniciado");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  server.handleClient();
}
