#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Paula e Gabriel 2G";
const char* password = "Paula492*";

const int ledPin = 2;
bool ledState = LOW;

WebServer server(80);

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;}";
  html += ".btn{padding:15px 32px;font-size:18px;border:none;border-radius:6px;color:white;cursor:pointer;text-decoration:none;display:inline-block;}";
  html += ".on{background-color:#28a745;} .off{background-color:#dc3545;}</style></head><body>";
  html += "<h2>Painel de Controle ESP32</h2>";
  html += "<p>Estado do LED: <strong>" + String(ledState ? "LIGADO" : "DESLIGADO") + "</strong></p>";
  
  if (ledState) {
    html += "<a href=\"/led/off\" class=\"btn off\">Desligar LED</a>";
  } else {
    html += "<a href=\"/led/on\" class=\"btn on\">Ligar LED</a>";
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLedOn() {
  ledState = HIGH;
  digitalWrite(ledPin, ledState);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLedOff() {
  ledState = LOW;
  digitalWrite(ledPin, ledState);
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Conectado!");
  Serial.print("Endereço de acesso: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/led/on", handleLedOn);
  server.on("/led/off", handleLedOff);
  server.begin();
}

void loop() {
  server.handleClient();
}
