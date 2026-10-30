#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

const char* ssid = "Paula e Gabriel 2G";
const char* password = "Paula492*";

const int ledPin = 2;
bool ledState = LOW;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 WebSocket</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background: #f4f4f4; }
    .btn { padding: 15px 30px; font-size: 18px; color: white; border: none; border-radius: 6px; cursor: pointer; }
    .on { background-color: #28a745; }
    .off { background-color: #dc3545; }
  </style>
</head>
<body>
  <h2>Controle do LED via WebSocket</h2>
  <p>Estado Atual: <strong id="state">DESLIGADO</strong></p>
  <button id="btn" class="btn on" onclick="toggleLED()">Ligar LED</button>

  <script>
    var socket = new WebSocket('ws://' + window.location.hostname + ':81/');

    socket.onmessage = function(event) {
      var stateText = document.getElementById('state');
      var btn = document.getElementById('btn');
      
      if (event.data === '1') {
        stateText.innerText = 'LIGADO';
        btn.innerText = 'Desligar LED';
        btn.className = 'btn off';
      } else {
        stateText.innerText = 'DESLIGADO';
        btn.innerText = 'Ligar LED';
        btn.className = 'btn on';
      }
    };

    function toggleLED() {
      socket.send('toggle');
    }
  </script>
</body>
</html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char*)payload);
    
    if (message == "toggle") {
      ledState = !ledState;
      digitalWrite(ledPin, ledState);
      webSocket.broadcastTXT(ledState ? "1" : "0");
    }
  } else if (type == WStype_CONNECTED) {
    webSocket.sendTXT(num, ledState ? "1" : "0");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, ledState);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado ao Wi-Fi!");
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });
  
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();
}