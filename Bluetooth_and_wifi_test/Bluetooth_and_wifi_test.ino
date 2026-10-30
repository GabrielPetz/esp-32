#include "BluetoothSerial.h"

#define DEBUG_ENABLE 1

#if DEBUG_ENABLE
  #define DEBUG_PRINT(x)       Serial.print(x)
  #define DEBUG_PRINTLN(x)     Serial.println(x)
  #define DEBUG_LOG(fmt, ...)  Serial.printf("[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_LOG(fmt, ...)
#endif

BluetoothSerial SerialBT;

const int LED_PIN = 2; // Pino do LED onboard na maioria dos ESP32 DevKit

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Começa desligado
  
  DEBUG_LOG("Iniciando a configuracao do Bluetooth...");
  
  if (!SerialBT.begin("ESP32_Bluetooth")) {
    DEBUG_LOG("ERRO: Falha ao inicializar o Bluetooth!");
  } else {
    DEBUG_LOG("Bluetooth pronto para pareamento.");
  }
}

void loop() {
  if (SerialBT.available()) {
    String mensagem = SerialBT.readString();
    mensagem.trim();
    
    if (mensagem.length() > 0) {
      DEBUG_LOG("Comando recebido: %s", mensagem.c_str());
      
      // Converte para maiúsculas para aceitar "ligar", "Ligar" ou "LIGAR"
      mensagem.toUpperCase();

      if (mensagem == "LIGAR") {
        digitalWrite(LED_PIN, HIGH);
        
        // Retorno enviado para o aplicativo do celular
        SerialBT.println("OK: LED aceso!");
        DEBUG_LOG("Resposta enviada via BT: OK: LED aceso!");
      } 
      else if (mensagem == "DESLIGAR") {
        digitalWrite(LED_PIN, LOW);
        
        // Retorno enviado para o aplicativo do celular
        SerialBT.println("OK: LED apagado!");
        DEBUG_LOG("Resposta enviada via BT: OK: LED apagado!");
      } 
      else if (mensagem == "STATUS") {
        int estado = digitalRead(LED_PIN);
        if (estado == HIGH) {
          SerialBT.println("STATUS: O LED esta LIGADO.");
        } else {
          SerialBT.println("STATUS: O LED esta DESLIGADO.");
        }
      } 
      else {
        // Retorno de erro caso o comando não exista
        SerialBT.printf("ERRO: Comando '%s' nao reconhecido.\n", mensagem.c_str());
        DEBUG_LOG("Comando desconhecido recebido.");
      }
    }
  }
}