#include <Arduino.h>

// ============================================================
// PINOS
// ============================================================
const int STB = 25;
const int CLK = 26;
const int DIO = 27;

// ============================================================
// ESTADOS ESPECIAIS DOS BOTÕES
// ============================================================
const uint32_t ESTADO_REPOUSO = 0x00000002;
const uint32_t ESTADO_DESCONECTADO = 0xFFFFFFFF;

// ============================================================
// ENDEREÇOS E BITS DOS LEDs
// ============================================================
// Endereço 0xC8
const uint8_t LED1_C8 = 0x01;
const uint8_t LED2_C8 = 0x02;
const uint8_t LED3_C8 = 0x04;
const uint8_t LED4_C8 = 0x08;
const uint8_t LED5_C8 = 0x10;
const uint8_t LED6_C8 = 0x20;
const uint8_t LED8_C8 = 0x80;

// Endereço 0xCA
const uint8_t LED9_CA  = 0x01;
const uint8_t LED10_CA = 0x02;
const uint8_t LED11_CA = 0x04;
const uint8_t LED12_CA = 0x08;
const uint8_t LED13_CA = 0x10;
const uint8_t LED15_CA = 0x40;
const uint8_t LED16_CA = 0x80;

// ============================================================
// ESTADOS LÓGICOS DOS BOTÕES
// ============================================================
int cicloSW1 = 0;
int cicloSW2 = 0;

// Estados individuais dos switches SW4 até SW6
bool estadoSW4 = false; // LED16
bool estadoSW5 = false; // LED8
bool estadoSW6 = false; // LED15

// ============================================================
// CONTROLE DO LED 11 (SW3) - MODO CONFIGURAÇÃO
// ============================================================
bool led11Piscando = true; // Inicia no modo de configuração (piscando)
bool estadoFisicoLED11 = false; // Alterna entre aceso/apagado durante o piscar
unsigned long ultimoTempoPiscarLED11 = 0;
const unsigned long INTERVALO_PISCAR_LED11 = 250;

// ============================================================
// ESTADOS DOS REGISTRADORES
// ============================================================
uint8_t regC8 = 0x00;
uint8_t regCA = 0x00;

// ============================================================
// CONTROLE DOS BOTÕES
// ============================================================
uint32_t ultimoBotao = ESTADO_REPOUSO;
uint32_t botaoEmPressao = ESTADO_REPOUSO;
unsigned long tempoInicioPressao = 0;
bool longPressTratado = false;

// ============================================================
// CONTROLE DA SEQUÊNCIA DE LEDs
// ============================================================
bool sequenciaAtiva = false;
int indiceSequencia = 0;
unsigned long ultimoTempoSequencia = 0;
const unsigned long TEMPO_SEQUENCIA = 450;

// ============================================================
// CONTROLE DO PISCAR FINAL
// ============================================================
const int QUANTIDADE_PISCADAS_FINAIS = 3;
int etapaPiscarFinal = 0;
int transicoesPiscar = 0;
bool piscandoFinal = false;

// ============================================================
// ENVIA UM COMANDO AO CONTROLADOR
// ============================================================
void sendCommand(uint8_t value) {
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, value);
  digitalWrite(STB, HIGH);
}

// ============================================================
// ENVIA DADOS PARA UM ENDEREÇO
// ============================================================
void sendData(uint8_t address, uint8_t data) {
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, address);
  shiftOut(DIO, CLK, LSBFIRST, data);
  digitalWrite(STB, HIGH);
}

// ============================================================
// LÊ O ESTADO DOS BOTÕES
// ============================================================
uint32_t readButtons() {
  uint32_t keyData = 0;
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK, LSBFIRST, 0x42);
  pinMode(DIO, INPUT);
  delayMicroseconds(2);
  for (int i = 0; i < 4; i++) {
    uint8_t byteLido = shiftIn(DIO, CLK, LSBFIRST);
    keyData |= ((uint32_t)byteLido << (i * 8));
  }
  pinMode(DIO, OUTPUT);
  digitalWrite(STB, HIGH);
  return keyData;
}

// ============================================================
// ENVIA OS REGISTRADORES ATUAIS PARA O PAINEL
// ============================================================
void enviarPainel() {
  sendData(0xC8, regC8);
  sendData(0xCA, regCA);
}

// ============================================================
// ACENDE TODOS OS LEDs MAPEADOS
// ============================================================
void acenderTodosLEDs() {
  regC8 = 0xBF;
  regCA = 0xDF;
  enviarPainel();
}

// ============================================================
// APAGA TODOS OS LEDs
// ============================================================
void apagarTodosLEDs() {
  regC8 = 0x00;
  regCA = 0x00;
  enviarPainel();
}

// ============================================================
// ATUALIZA TODOS OS LEDs DO PAINEL
// ============================================================
void atualizarEstadosPainel() {
  regC8 = 0x00;
  regCA = 0x00;

  // Lógica do SW1
  switch (cicloSW1) {
    case 0:
      regC8 |= LED1_C8; regCA |= LED9_CA; regC8 |= LED2_C8; regCA |= LED10_CA; regC8 |= LED3_C8;
      break;
    case 1:
      regCA |= LED9_CA; regC8 |= LED2_C8; regCA |= LED10_CA; regC8 |= LED3_C8;
      break;
    case 2:
      regC8 |= LED2_C8; regCA |= LED10_CA; regC8 |= LED3_C8;
      break;
    case 3:
      regCA |= LED10_CA; regC8 |= LED3_C8;
      break;
    case 4:
      regC8 |= LED3_C8;
      break;
    case 5:
      break;
  }

  // Lógica do SW2
  switch (cicloSW2) {
    case 0: break;
    case 1: regC8 |= LED6_C8; break;
    case 2: regCA |= LED13_CA; break;
    case 3: regC8 |= LED5_C8; break;
    case 4: regCA |= LED12_CA; break;
    case 5: regC8 |= LED4_C8; break;
  }

  // ==========================================================
  // LÓGICA DO SW3 (LED 11) - Piscar ou Salvo
  // ==========================================================
  if (led11Piscando) {
    // Se está no modo configuração, acompanha o estado físico do timer
    if (estadoFisicoLED11) {
      regCA |= LED11_CA;
    }
  } else {
    // Se está salvo/travado, o LED fica aceso estático
    regCA |= LED11_CA;
  }

  // SW4 -> LED16
  if (estadoSW4) regCA |= LED16_CA;

  // SW5 -> LED8
  if (estadoSW5) regC8 |= LED8_C8;

  // SW6 -> LED15
  if (estadoSW6) regCA |= LED15_CA;

  enviarPainel();
}

// ============================================================
// EXECUTA A AÇÃO DE UM BOTÃO
// ============================================================
void executarAcaoBotao(uint32_t botaoApertado) {
  switch (botaoApertado) {
    case 0x3:
      cicloSW1++;
      if (cicloSW1 > 5) cicloSW1 = 0;
      Serial.print("SW1 - Ciclo atual: ");
      Serial.println(cicloSW1);
      break;

    case 0xA:
      cicloSW2++;
      if (cicloSW2 > 5) cicloSW2 = 0;
      Serial.print("SW2 - Ciclo atual: ");
      Serial.println(cicloSW2);
      break;

    case 0x102:
      // Alterna entre o modo de configuração (piscando) e salvo (estático)
      led11Piscando = !led11Piscando;
      Serial.print("SW3 - LED11: ");
      Serial.println(led11Piscando ? "AGUARDANDO CONFIGURACAO (Piscando)" : "SALVO (Estatico)");
      break;

    case 0x802:
      estadoSW4 = !estadoSW4;
      Serial.print("SW4 - LED16: ");
      Serial.println(estadoSW4 ? "LIGADO" : "DESLIGADO");
      break;

    case 0x10002:
      estadoSW5 = !estadoSW5;
      Serial.print("SW5 - LED8: ");
      Serial.println(estadoSW5 ? "LIGADO" : "DESLIGADO");
      break;

    case 0x80002:
      estadoSW6 = !estadoSW6;
      Serial.print("SW6 - LED15: ");
      Serial.println(estadoSW6 ? "LIGADO" : "DESLIGADO");
      break;

    default:
      Serial.print("Botao nao mapeado: 0x");
      Serial.println(botaoApertado, HEX);
      return;
  }

  atualizarEstadosPainel();
  Serial.flush();
}

// ============================================================
// INICIA A SEQUÊNCIA DE LEDs
// ============================================================
void iniciarSequenciaLEDs() {
  Serial.println("\n======================================");
  Serial.println("INICIANDO CHECK DOS LEDs");
  Serial.println("======================================");
  sequenciaAtiva = true;
  piscandoFinal = false;
  indiceSequencia = 0;
  etapaPiscarFinal = 0;
  transicoesPiscar = 0;
  ultimoTempoSequencia = millis() - TEMPO_SEQUENCIA;
}

// ============================================================
// ATUALIZA A SEQUÊNCIA AUTOMÁTICA DE LEDs
// ============================================================
void atualizarSequenciaLEDs() {
  if (!sequenciaAtiva) return;

  unsigned long agora = millis();
  if (agora - ultimoTempoSequencia < TEMPO_SEQUENCIA) return;
  ultimoTempoSequencia = agora;

  if (!piscandoFinal) {
    regC8 = 0x00;
    regCA = 0x00;
    switch (indiceSequencia) {
      case 0: regC8 = LED1_C8; break;
      case 1: regC8 = LED2_C8; break;
      case 2: regC8 = LED3_C8; break;
      case 3: regC8 = LED4_C8; break;
      case 4: regC8 = LED5_C8; break;
      case 5: regC8 = LED6_C8; break;
      case 6: regC8 = LED8_C8; break;
      case 7: regCA = LED9_CA; break;
      case 8: regCA = LED10_CA; break;
      case 9: regCA = LED11_CA; break;
      case 10: regCA = LED12_CA; break;
      case 11: regCA = LED13_CA; break;
      case 12: regCA = LED15_CA; break;
      case 13: regCA = LED16_CA; break;
    }
    enviarPainel();
    indiceSequencia++;

    if (indiceSequencia > 13) {
      Serial.println("CHECK INDIVIDUAL CONCLUIDO");
      Serial.println("PISCANDO TODOS OS LEDs 3 VEZES");
      piscandoFinal = true;
      transicoesPiscar = 0;
      ultimoTempoSequencia = millis() - TEMPO_SEQUENCIA;
    }
    return;
  }

  if (transicoesPiscar % 2 == 0) {
    acenderTodosLEDs();
  } else {
    apagarTodosLEDs();
  }

  transicoesPiscar++;

  if (transicoesPiscar >= QUANTIDADE_PISCADAS_FINAIS * 2) {
    sequenciaAtiva = false;
    piscandoFinal = false;
    Serial.println("======================================");
    Serial.println("CHECK DOS LEDs FINALIZADO");
    Serial.println("======================================");
    atualizarEstadosPainel();
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(STB, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  sendCommand(0x40);
  sendCommand(0x8F);
  for (int i = 0; i < 16; i++) {
    sendData(0xC0 + i, 0x00);
  }

  atualizarEstadosPainel();

  Serial.println("\n======================================");
  Serial.println("SISTEMA INICIADO");
  Serial.println("======================================");
  Serial.println("\nMAPEAMENTO:");
  Serial.println("SW1 -> LED1, LED9, LED2, LED10, LED3");
  Serial.println("SW2 -> LED6, LED13, LED5, LED12, LED4");
  Serial.println("SW3 -> LED11 (Piscar / Salvar)");
  Serial.println("SW4 -> LED16");
  Serial.println("SW5 -> LED8");
  Serial.println("SW6 -> LED15");
  Serial.println("\nSegure qualquer botao por 5 segundos para iniciar o check dos LEDs.");
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  // ==========================================================
  // CONTROLE DO PISCAR DO LED 11 (SW3)
  // Só executa se não estiver na sequência de check de LEDs
  // ==========================================================
  if (led11Piscando && !sequenciaAtiva) {
    unsigned long agoraBlink = millis();
    if (agoraBlink - ultimoTempoPiscarLED11 >= INTERVALO_PISCAR_LED11) {
      ultimoTempoPiscarLED11 = agoraBlink;
      estadoFisicoLED11 = !estadoFisicoLED11;
      atualizarEstadosPainel(); // Envia o frame atualizado para a placa
    }
  }

  // ==========================================================
  // CHECK DOS LEDs
  // ==========================================================
  if (sequenciaAtiva) {
    atualizarSequenciaLEDs();
    return;
  }

  // ==========================================================
  // LEITURA DOS BOTÕES
  // ==========================================================
  uint32_t botoes = readButtons();

  if (botoes == ESTADO_DESCONECTADO) {
    delay(100);
    return;
  }

  // ==========================================================
  // MUDANÇA NO ESTADO DOS BOTÕES
  // ==========================================================
  if (botoes != ultimoBotao) {
    if (botoes != ESTADO_REPOUSO) {
      tempoInicioPressao = millis();
      longPressTratado = false;
      botaoEmPressao = botoes;
    } 
    else {
      if (!longPressTratado && botaoEmPressao != ESTADO_REPOUSO) {
        executarAcaoBotao(botaoEmPressao);
      }
      botaoEmPressao = ESTADO_REPOUSO;
    }
    ultimoBotao = botoes;
    delay(50);
  }
  // ==========================================================
  // BOTÃO CONTINUA PRESSIONADO
  // ==========================================================
  else {
    if (botoes != ESTADO_REPOUSO && !longPressTratado) {
      if (millis() - tempoInicioPressao >= 5000) {
        Serial.println("\n--------------------------------------");
        Serial.println("BOTAO SEGURADO POR 5 SEGUNDOS");
        Serial.println("--------------------------------------");
        iniciarSequenciaLEDs();
        longPressTratado = true;
      }
    }
  }
}