#include <Arduino.h>

const int STB = 25;
const int CLK = 26;
const int DIO = 27;

void sendCommand(uint8_t value) {
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, value);
  digitalWrite(STB, HIGH);
}

void sendData(uint8_t address, uint8_t data) {
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, address);
  shiftOut(DIO, CLK, LSBFIRST, data);
  digitalWrite(STB, HIGH);
}

void setup() {
  pinMode(STB, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  
  // Estado de repouso das linhas
  digitalWrite(STB, HIGH);
  digitalWrite(CLK, HIGH);
  
  // 1. Comando: Modo de escrita de dados
  sendCommand(0x40); 
  
  // 2. Comando: Limpar a memória (apagar tudo)
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, 0xC0); // Endereço inicial
  for(int i = 0; i < 16; i++) {
    shiftOut(DIO, CLK, LSBFIRST, 0x00);
  }
  digitalWrite(STB, HIGH);
  
  // 3. Comando: Ligar o display (Brilho máximo)
  sendCommand(0x8F); 
}

void loop() {
  // Varredura para acender os LEDs mapeados na memória
  for(uint8_t i = 0; i < 16; i++) {
    sendData(0xC0 + i, 0xFF); 
    delay(150);
  }
  
  delay(1000);
  
  // Apaga todos os LEDs
  for(uint8_t i = 0; i < 16; i++) {
    sendData(0xC0 + i, 0x00);
  }
  
  delay(1000);
}