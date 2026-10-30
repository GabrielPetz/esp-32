// ============================================================
// ESP32 - SIMULADOR REALISTA DE SONDA LAMBDA NARROWBAND
// ============================================================
//
// ESP32 DevKit clássico
//
// DAC:
// GPIO 25
//
// Sonda simulada:
// Zirconia / Narrowband / Switching sensor
//
// Saída pretendida após o estágio de interface:
// aproximadamente 0.10 V ~ 0.90 V
//
// Características da simulação:
//
// - Platô pobre
// - Transição pobre -> rica curva
// - Região de comutação rápida
// - Platô rico
// - Transição rica -> pobre curva
// - Tempos assimétricos
// - Pequena variação de período
// - Pequena variação de amplitude
// - Ruído controlado
//
// ============================================================

#include <Arduino.h>
#include <math.h>

// ============================================================
// PINO
// ============================================================

const int LAMBDA_DAC_PIN = 25;

// ============================================================
// CONFIGURAÇÃO ELÉTRICA
// ============================================================
//
// O DAC do ESP32 produz aproximadamente 0 ~ 3.3 V.
//
// Exemplo de divisor:
// R_TOP    = 22k
// R_BOTTOM = 10k
//
// Vout = Vdac * 10 / 32
//
// Portanto, para aproximadamente 0.90 V na saída,
// precisamos de aproximadamente 2.88 V no DAC.
//
// ============================================================

const float DAC_SUPPLY = 3.30;

const float R_TOP    = 22000.0;
const float R_BOTTOM = 10000.0;

// ============================================================
// PARÂMETROS DA SONDA
// ============================================================

// Tensão mínima da sonda
float lambdaLow = 0.10;

// Tensão máxima da sonda
float lambdaHigh = 0.90;

// ============================================================
// TEMPOS BASE
// ============================================================
//
// Valores escolhidos para produzir uma forma semelhante
// a uma sonda narrowband em funcionamento.
//
// O tempo de transição é propositalmente diferente
// entre subida e descida.
// ============================================================

// Tempo pobre antes de enriquecer
float leanHoldMs = 420.0;

// Transição pobre -> rica
float leanToRichMs = 300.0;

// Tempo rico
float richHoldMs = 430.0;

// Transição rica -> pobre
float richToLeanMs = 230.0;

// ============================================================
// RUÍDO
// ============================================================
//
// Pequena perturbação na tensão.
// Valores maiores deixam o sinal menos "perfeito".
//
// 0.005 = ±5 mV aproximadamente
// ============================================================

float noiseAmplitude = 0.004;

// ============================================================
// VARIAÇÃO DE CICLO
// ============================================================
//
// Introduz pequenas diferenças entre os ciclos,
// tornando o sinal menos artificial.
//
// 0.05 = ±5%
// ============================================================

float cycleVariation = 0.05;

// ============================================================
// TAXA DE ATUALIZAÇÃO
// ============================================================
//
// 1000 Hz = atualização a cada 1 ms.
//
// Isso é mais que suficiente para uma sonda narrowband,
// cuja dinâmica ocorre na escala de dezenas/centenas de ms.
// ============================================================

const unsigned long UPDATE_INTERVAL_US = 1000;

// ============================================================
// ESTADOS
// ============================================================

enum LambdaState
{
    LEAN_HOLD,
    LEAN_TO_RICH,
    RICH_HOLD,
    RICH_TO_LEAN
};

LambdaState state = LEAN_HOLD;

// ============================================================
// CONTROLE DO TEMPO
// ============================================================

unsigned long lastUpdate = 0;
unsigned long stateStart = 0;

// ============================================================
// PARÂMETROS DO CICLO ATUAL
// ============================================================

float currentLeanHold;
float currentLeanToRich;
float currentRichHold;
float currentRichToLean;

// ============================================================
// GERADOR DE RUÍDO
// ============================================================

float randomNoise()
{
    float r = (float)random(-1000, 1001) / 1000.0;

    return r * noiseAmplitude;
}

// ============================================================
// FUNÇÃO SIGMOIDE
// ============================================================
//
// Produz uma transição curva:
//
// 0 ................. 1
//      ╭──────────
//    ╭─
//  ──╯
//
// O parâmetro shape controla quão rápida é a região
// central da mudança.
// ============================================================

float sigmoid(float x, float shape)
{
    return 1.0 / (1.0 + expf(-shape * (x - 0.5)));
}

// ============================================================
// NORMALIZA SIGMOIDE
// ============================================================
//
// Faz a curva começar exatamente em 0
// e terminar exatamente em 1.
// ============================================================

float normalizedSigmoid(float x)
{
    const float shape = 12.0;

    float y = sigmoid(x, shape);

    float y0 = sigmoid(0.0, shape);
    float y1 = sigmoid(1.0, shape);

    y = (y - y0) / (y1 - y0);

    if (y < 0.0)
        y = 0.0;

    if (y > 1.0)
        y = 1.0;

    return y;
}

// ============================================================
// INTERPOLAÇÃO
// ============================================================

float transitionCurve(
    float startVoltage,
    float endVoltage,
    float elapsed,
    float duration
)
{
    if (duration <= 0.0)
        return endVoltage;

    float x = elapsed / duration;

    if (x <= 0.0)
        return startVoltage;

    if (x >= 1.0)
        return endVoltage;

    float curve = normalizedSigmoid(x);

    return startVoltage +
           ((endVoltage - startVoltage) * curve);
}

// ============================================================
// CALCULA VALOR DAC
// ============================================================

uint8_t voltageToDAC(float lambdaVoltage)
{
    // Converte tensão desejada na saída da interface
    // para tensão necessária no DAC.

    float dacVoltage =
        lambdaVoltage *
        ((R_TOP + R_BOTTOM) / R_BOTTOM);

    // Limites do DAC
    if (dacVoltage < 0.0)
        dacVoltage = 0.0;

    if (dacVoltage > DAC_SUPPLY)
        dacVoltage = DAC_SUPPLY;

    // Conversão para 8 bits
    float value =
        (dacVoltage / DAC_SUPPLY) * 255.0;

    int dacValue = (int)roundf(value);

    if (dacValue < 0)
        dacValue = 0;

    if (dacValue > 255)
        dacValue = 255;

    return (uint8_t)dacValue;
}

// ============================================================
// ESCREVE TENSÃO
// ============================================================

void setLambdaVoltage(float voltage)
{
    uint8_t dacValue =
        voltageToDAC(voltage);

    dacWrite(LAMBDA_DAC_PIN, dacValue);
}

// ============================================================
// GERA VARIAÇÃO DO CICLO
// ============================================================

float randomVariation(float value)
{
    float variation =
        1.0 +
        ((float)random(-1000, 1001) / 1000.0)
        * cycleVariation;

    return value * variation;
}

// ============================================================
// CONFIGURA NOVO CICLO
// ============================================================

void startNewCycle()
{
    currentLeanHold =
        randomVariation(leanHoldMs);

    currentLeanToRich =
        randomVariation(leanToRichMs);

    currentRichHold =
        randomVariation(richHoldMs);

    currentRichToLean =
        randomVariation(richToLeanMs);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    randomSeed(esp_random());

    // Inicializa DAC
    dacWrite(LAMBDA_DAC_PIN, 0);

    // Primeiro ciclo
    startNewCycle();

    state = LEAN_HOLD;

    stateStart = millis();

    lastUpdate = micros();

    setLambdaVoltage(lambdaLow);

    Serial.println();
    Serial.println("==============================================");
    Serial.println(" SIMULADOR DE SONDA LAMBDA NARROWBAND");
    Serial.println("==============================================");

    Serial.println();

    Serial.println("ESP32 DevKit classico");
    Serial.println("DAC GPIO25");

    Serial.println();

    Serial.print("Lambda LOW : ");
    Serial.print(lambdaLow, 3);
    Serial.println(" V");

    Serial.print("Lambda HIGH: ");
    Serial.print(lambdaHigh, 3);
    Serial.println(" V");

    Serial.println();

    Serial.print("Lean hold  : ");
    Serial.print(leanHoldMs);
    Serial.println(" ms");

    Serial.print("Lean->Rich : ");
    Serial.print(leanToRichMs);
    Serial.println(" ms");

    Serial.print("Rich hold  : ");
    Serial.print(richHoldMs);
    Serial.println(" ms");

    Serial.print("Rich->Lean : ");
    Serial.print(richToLeanMs);
    Serial.println(" ms");

    Serial.println();

    Serial.println("Simulacao iniciada.");
    Serial.println();
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================

void loop()
{
    unsigned long nowMicros = micros();

    // Atualiza a saída aproximadamente a cada 1 ms
    if ((unsigned long)(nowMicros - lastUpdate)
        < UPDATE_INTERVAL_US)
    {
        return;
    }

    lastUpdate += UPDATE_INTERVAL_US;

    unsigned long now = millis();

    float elapsed =
        (float)(now - stateStart);

    float voltage = 0.0;

    // ========================================================
    // ESTADO: POBRE
    // ========================================================

    if (state == LEAN_HOLD)
    {
        voltage = lambdaLow;

        if (elapsed >= currentLeanHold)
        {
            state = LEAN_TO_RICH;

            stateStart = now;
        }
    }

    // ========================================================
    // TRANSIÇÃO: POBRE -> RICA
    // ========================================================

    else if (state == LEAN_TO_RICH)
    {
        voltage =
            transitionCurve(
                lambdaLow,
                lambdaHigh,
                elapsed,
                currentLeanToRich
            );

        if (elapsed >= currentLeanToRich)
        {
            state = RICH_HOLD;

            stateStart = now;
        }
    }

    // ========================================================
    // ESTADO: RICA
    // ========================================================

    else if (state == RICH_HOLD)
    {
        voltage = lambdaHigh;

        if (elapsed >= currentRichHold)
        {
            state = RICH_TO_LEAN;

            stateStart = now;
        }
    }

    // ========================================================
    // TRANSIÇÃO: RICA -> POBRE
    // ========================================================

    else if (state == RICH_TO_LEAN)
    {
        voltage =
            transitionCurve(
                lambdaHigh,
                lambdaLow,
                elapsed,
                currentRichToLean
            );

        if (elapsed >= currentRichToLean)
        {
            startNewCycle();

            state = LEAN_HOLD;

            stateStart = now;
        }
    }

    // ========================================================
    // ADICIONA PEQUENO RUÍDO
    // ========================================================

    voltage += randomNoise();

    // ========================================================
    // LIMITA A SAÍDA
    // ========================================================

    if (voltage < lambdaLow)
        voltage = lambdaLow;

    if (voltage > lambdaHigh)
        voltage = lambdaHigh;

    // ========================================================
    // ENVIA AO DAC
    // ========================================================

    setLambdaVoltage(voltage);
}