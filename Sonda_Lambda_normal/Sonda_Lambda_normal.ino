// ============================================================
// ESP32 - Simulador de Sonda Lambda Narrowband
// Arduino IDE
//
// ESP32 DevKit clássico
//
// Saída DAC:
// GPIO 25
//
// Faixa simulada da sonda:
// 0.10 V = mistura pobre
// 0.90 V = mistura rica
//
// IMPORTANTE:
// O DAC do ESP32 trabalha aproximadamente entre 0 e 3.3 V.
// Use divisor resistivo para obter aproximadamente 0-1 V.
// ============================================================

#include <Arduino.h>

// ============================================================
// CONFIGURAÇÃO
// ============================================================

const int LAMBDA_DAC_PIN = 25;

// Tensão simulada da sonda
const float LAMBDA_LOW_VOLTAGE  = 0.10;  // Pobre
const float LAMBDA_HIGH_VOLTAGE = 0.90;  // Rica

// Tempo em cada estado
const unsigned long LOW_TIME_MS  = 700;
const unsigned long HIGH_TIME_MS = 700;

// Tempo da transição entre estados
const unsigned long TRANSITION_MS = 100;

// Tensão do DAC do ESP32
const float DAC_VOLTAGE = 3.30;

// ============================================================
// DIVISOR RESISTIVO
//
// 22k no resistor superior
// 10k no resistor inferior
//
// Vout = Vin * 10 / 32
// ============================================================

const float R_TOP    = 22000.0;
const float R_BOTTOM = 10000.0;

// ============================================================
// ESTADOS
// ============================================================

enum LambdaState
{
    LAMBDA_LOW,
    LAMBDA_TRANSITION_UP,
    LAMBDA_HIGH,
    LAMBDA_TRANSITION_DOWN
};

LambdaState state = LAMBDA_LOW;

unsigned long stateStartTime = 0;

// ============================================================
// CONVERTE TENSÃO DA SONDA PARA VALOR DO DAC
// ============================================================

uint8_t lambdaVoltageToDAC(float lambdaVoltage)
{
    // Descobre qual tensão precisa sair do DAC
    //
    // Vlambda = Vdac * Rbottom / (Rtop + Rbottom)
    //
    // Portanto:
    //
    // Vdac = Vlambda * (Rtop + Rbottom) / Rbottom

    float dacVoltage =
        lambdaVoltage *
        (R_TOP + R_BOTTOM) /
        R_BOTTOM;

    // Limita à faixa do DAC
    if (dacVoltage < 0.0)
        dacVoltage = 0.0;

    if (dacVoltage > DAC_VOLTAGE)
        dacVoltage = DAC_VOLTAGE;

    // DAC ESP32 = 8 bits
    int dacValue =
        (int)((dacVoltage / DAC_VOLTAGE) * 255.0);

    if (dacValue < 0)
        dacValue = 0;

    if (dacValue > 255)
        dacValue = 255;

    return (uint8_t)dacValue;
}

// ============================================================
// ESCREVE UMA TENSÃO NA SAÍDA
// ============================================================

void setLambdaVoltage(float voltage)
{
    uint8_t dacValue = lambdaVoltageToDAC(voltage);

    dacWrite(LAMBDA_DAC_PIN, dacValue);
}

// ============================================================
// INTERPOLAÇÃO LINEAR
// ============================================================

float interpolate(
    float start,
    float end,
    unsigned long elapsed,
    unsigned long duration
)
{
    if (duration == 0)
        return end;

    if (elapsed >= duration)
        return end;

    float progress =
        (float)elapsed / (float)duration;

    return start +
           ((end - start) * progress);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(500);

    // Inicialização do DAC
    dacWrite(LAMBDA_DAC_PIN, 0);

    state = LAMBDA_LOW;
    stateStartTime = millis();

    setLambdaVoltage(LAMBDA_LOW_VOLTAGE);

    Serial.println();
    Serial.println("==============================================");
    Serial.println(" ESP32 - Simulador de Sonda Lambda");
    Serial.println("==============================================");
    Serial.println("DAC: GPIO 25");
    Serial.println();
    Serial.print("Lambda LOW : ");
    Serial.print(LAMBDA_LOW_VOLTAGE);
    Serial.println(" V");

    Serial.print("Lambda HIGH: ");
    Serial.print(LAMBDA_HIGH_VOLTAGE);
    Serial.println(" V");

    Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    unsigned long now = millis();
    unsigned long elapsed = now - stateStartTime;

    float voltage = 0.0;

    switch (state)
    {
        // ----------------------------------------------------
        // MISTURA POBRE
        // ----------------------------------------------------

        case LAMBDA_LOW:

            voltage = LAMBDA_LOW_VOLTAGE;

            setLambdaVoltage(voltage);

            if (elapsed >= LOW_TIME_MS)
            {
                state = LAMBDA_TRANSITION_UP;
                stateStartTime = now;
            }

            break;

        // ----------------------------------------------------
        // TRANSIÇÃO POBRE -> RICA
        // ----------------------------------------------------

        case LAMBDA_TRANSITION_UP:

            voltage = interpolate(
                LAMBDA_LOW_VOLTAGE,
                LAMBDA_HIGH_VOLTAGE,
                elapsed,
                TRANSITION_MS
            );

            setLambdaVoltage(voltage);

            if (elapsed >= TRANSITION_MS)
            {
                state = LAMBDA_HIGH;
                stateStartTime = now;
            }

            break;

        // ----------------------------------------------------
        // MISTURA RICA
        // ----------------------------------------------------

        case LAMBDA_HIGH:

            voltage = LAMBDA_HIGH_VOLTAGE;

            setLambdaVoltage(voltage);

            if (elapsed >= HIGH_TIME_MS)
            {
                state = LAMBDA_TRANSITION_DOWN;
                stateStartTime = now;
            }

            break;

        // ----------------------------------------------------
        // TRANSIÇÃO RICA -> POBRE
        // ----------------------------------------------------

        case LAMBDA_TRANSITION_DOWN:

            voltage = interpolate(
                LAMBDA_HIGH_VOLTAGE,
                LAMBDA_LOW_VOLTAGE,
                elapsed,
                TRANSITION_MS
            );

            setLambdaVoltage(voltage);

            if (elapsed >= TRANSITION_MS)
            {
                state = LAMBDA_LOW;
                stateStartTime = now;
            }

            break;
    }

    // ========================================================
    // MONITOR SERIAL
    //
    // Imprime aproximadamente a cada 1 segundo
    // ========================================================

    static unsigned long lastPrint = 0;

    if (now - lastPrint >= 1000)
    {
        lastPrint = now;

        Serial.print("Estado: ");

        switch (state)
        {
            case LAMBDA_LOW:
                Serial.print("POBRE");
                break;

            case LAMBDA_TRANSITION_UP:
                Serial.print("TRANSICAO -> RICA");
                break;

            case LAMBDA_HIGH:
                Serial.print("RICA");
                break;

            case LAMBDA_TRANSITION_DOWN:
                Serial.print("TRANSICAO -> POBRE");
                break;
        }

        Serial.print(" | Tensao aproximada: ");
        Serial.print(voltage, 3);
        Serial.println(" V");
    }
}