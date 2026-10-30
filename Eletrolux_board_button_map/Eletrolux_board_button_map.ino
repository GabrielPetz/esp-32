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

// Função para ler o estado de todos os botões
uint32_t readButtons() {
  uint32_t keyData = 0;
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, 0x42); // Comando 0x42: Ler Teclado
  
  pinMode(DIO, INPUT);
  delayMicroseconds(2); // Pausa para estabilizar a linha
  
  // O CI envia 4 bytes de estado dos botões
  for (int i = 0; i < 4; i++) {
    uint8_t byteLido = shiftIn(DIO, CLK, LSBFIRST);
    keyData |= ((uint32_t)byteLido << (i * 8));
  }
  
  pinMode(DIO, OUTPUT);
  digitalWrite(STB, HIGH);
  return keyData;
}

// Função para descobrir qual endereço liga qual LED
void mapearLEDs() {
  Serial.println("Iniciando varredura de LEDs...");
  for(uint8_t addr = 0; addr <= 0x0D; addr += 2) { // Endereços de dados (pares)
    for(uint8_t bit = 0; bit < 8; bit++) {
      sendData(0xC0 + addr, 1 << bit);
      
      Serial.print("Anotar -> Endereço: 0x");
      Serial.print(0xC0 + addr, HEX);
      Serial.print(" | Bit/Valor: 0x");
      Serial.println(1 << bit, HEX);
      
      delay(1500); // Fica 1.5s aceso para você olhar e anotar
      sendData(0xC0 + addr, 0x00);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(STB, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  
  sendCommand(0x40); // Modo escrita
  sendCommand(0x8F); // Display ON (brilho máximo)
  
  // Apaga toda a memória residual
  for(int i = 0; i < 16; i++) {
    sendData(0xC0 + i, 0x00);
  }
  
  // DESCOMENTE A LINHA ABAIXO APENAS UMA VEZ PARA MAPEAR OS LEDS
  // mapearLEDs(); 
  
  Serial.println("Aguardando pressionamento dos botões...");
}

void loop() {
  uint32_t botoes = readButtons();
  
  if (botoes > 0) {
    Serial.print("Botão Pressionado! Código RAW: 0x");
    Serial.println(botoes, HEX);
    delay(250); // Anti-bounce para não inundar o terminal
  }
}