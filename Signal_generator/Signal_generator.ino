
// ============================================================
// ESP32 CLÁSSICO - SIGNAL GENERATOR
// ============================================================
//
// 6 saídas independentes:
//
// GPIO 25 -> SENOIDE       -> DAC
// GPIO 26 -> TRIANGULAR    -> DAC
// GPIO 27 -> QUADRADA      -> PWM
// GPIO 14 -> DENTE DE SERRA -> PWM + variação de duty
// GPIO 32 -> PULSO         -> PWM
// GPIO 33 -> PWM           -> PWM
//
// Compatível com ESP32 clássico / ESP32-WROOM-32
// Arduino IDE + ESP32 Arduino Core
//
// ============================================================


// ============================================================
// CONFIGURAÇÃO DOS PINOS
// ============================================================

const int PIN_SINE     = 25;
const int PIN_TRIANGLE = 26;
const int PIN_SQUARE   = 27;
const int PIN_SAW      = 14;
const int PIN_PULSE    = 32;
const int PIN_PWM      = 33;


// ============================================================
// CONFIGURAÇÃO DOS SINAIS
// ============================================================

// -------------------------
// SENOIDE
// GPIO 25 - DAC
// -------------------------

const float SINE_FREQUENCY = 1000.0;


// -------------------------
// TRIANGULAR
// GPIO 26 - DAC
// -------------------------

const float TRIANGLE_FREQUENCY = 500.0;


// -------------------------
// QUADRADA
// GPIO 27 - PWM
// -------------------------

const int SQUARE_FREQUENCY = 2000;
const int SQUARE_DUTY = 50;


// -------------------------
// DENTE DE SERRA
// GPIO 14
//
// A frequência da portadora PWM precisa ser
// muito maior que a frequência da serra.
//
// O filtro RC transforma a variação do duty
// em uma tensão aproximadamente analógica.
// -------------------------

const int SAW_FREQUENCY = 100;
const int SAW_PWM_FREQUENCY = 20000;


// -------------------------
// PULSO
// GPIO 32
// -------------------------

const int PULSE_FREQUENCY = 10000;
const int PULSE_DUTY = 10;


// -------------------------
// PWM
// GPIO 33
// -------------------------

const int PWM_FREQUENCY = 5000;
const int PWM_DUTY = 75;


// ============================================================
// CONFIGURAÇÃO DA TABELA DA SENOIDE
// ============================================================

#define WAVE_TABLE_SIZE 256

uint8_t sineTable[WAVE_TABLE_SIZE];


// ============================================================
// CONTROLE DA SENOIDE
// ============================================================

volatile uint32_t sineIndex = 0;

uint32_t sineSampleInterval;


// ============================================================
// CONTROLE DO TRIÂNGULO
// ============================================================

volatile uint32_t triangleIndex = 0;

uint32_t triangleSampleInterval;


// ============================================================
// CONTROLE DO DENTE DE SERRA
// ============================================================

uint32_t sawLastUpdate = 0;

const int SAW_STEPS = 256;

int sawStep = 0;

uint32_t sawStepInterval;


// ============================================================
// TIMER DE CONTROLE
// ============================================================

unsigned long lastSineUpdate = 0;
unsigned long lastTriangleUpdate = 0;


// ============================================================
// CRIA TABELA DE SENOIDE
// ============================================================

void createSineTable()
{
    for (int i = 0; i < WAVE_TABLE_SIZE; i++)
    {
        float angle = (2.0 * PI * i) / WAVE_TABLE_SIZE;

        float value = sin(angle);

        // Converte de -1..+1 para 0..255
        int dacValue = (int)((value + 1.0) * 127.5);

        if (dacValue < 0)
            dacValue = 0;

        if (dacValue > 255)
            dacValue = 255;

        sineTable[i] = dacValue;
    }
}


// ============================================================
// ATUALIZA SENOIDE
// ============================================================

void updateSine()
{
    unsigned long now = micros();

    if ((now - lastSineUpdate) >= sineSampleInterval)
    {
        lastSineUpdate = now;

        dacWrite(PIN_SINE, sineTable[sineIndex]);

        sineIndex++;

        if (sineIndex >= WAVE_TABLE_SIZE)
        {
            sineIndex = 0;
        }
    }
}


// ============================================================
// ATUALIZA TRIÂNGULO
// ============================================================

void updateTriangle()
{
    unsigned long now = micros();

    if ((now - lastTriangleUpdate) >= triangleSampleInterval)
    {
        lastTriangleUpdate = now;

        uint32_t index = triangleIndex;

        uint8_t value;

        // Primeira metade:
        // 0 -> 255
        if (index < (WAVE_TABLE_SIZE / 2))
        {
            value = map(
                index,
                0,
                (WAVE_TABLE_SIZE / 2) - 1,
                0,
                255
            );
        }

        // Segunda metade:
        // 255 -> 0
        else
        {
            value = map(
                index,
                WAVE_TABLE_SIZE / 2,
                WAVE_TABLE_SIZE - 1,
                255,
                0
            );
        }

        dacWrite(PIN_TRIANGLE, value);

        triangleIndex++;

        if (triangleIndex >= WAVE_TABLE_SIZE)
        {
            triangleIndex = 0;
        }
    }
}


// ============================================================
// ATUALIZA DENTE DE SERRA
// ============================================================
//
// O GPIO 14 gera PWM de alta frequência.
// O duty cycle é alterado progressivamente:
//
// 0% -> 100%
//
// Com um filtro RC:
//
// PWM -> R -> saída
//            |
//            C
//            |
//           GND
//
// a saída se aproxima de uma rampa.
// ============================================================

void updateSaw()
{
    unsigned long now = micros();

    if ((now - sawLastUpdate) >= sawStepInterval)
    {
        sawLastUpdate = now;

        int duty = map(
            sawStep,
            0,
            SAW_STEPS - 1,
            0,
            255
        );

        analogWrite(PIN_SAW, duty);

        sawStep++;

        if (sawStep >= SAW_STEPS)
        {
            sawStep = 0;
        }
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("       ESP32 SIGNAL GENERATOR");
    Serial.println("==============================================");
    Serial.println();


    // ========================================================
    // CRIA TABELA DA SENOIDE
    // ========================================================

    createSineTable();


    // ========================================================
    // CALCULA INTERVALO DA SENOIDE
    // ========================================================
    //
    // Exemplo:
    //
    // 1000 Hz / 256 amostras
    //
    // = 256000 amostras/s
    //
    // intervalo = 3,906 us
    //
    // ========================================================

    sineSampleInterval =
        (uint32_t)(1000000.0 /
                   (SINE_FREQUENCY * WAVE_TABLE_SIZE));


    // ========================================================
    // CALCULA INTERVALO DO TRIÂNGULO
    // ========================================================

    triangleSampleInterval =
        (uint32_t)(1000000.0 /
                   (TRIANGLE_FREQUENCY * WAVE_TABLE_SIZE));


    // ========================================================
    // CALCULA INTERVALO DO DENTE DE SERRA
    // ========================================================

    sawStepInterval =
        (uint32_t)(1000000.0 /
                   (SAW_FREQUENCY * SAW_STEPS));


    // ========================================================
    // CONFIGURA GPIO 25 - SENOIDE
    // ========================================================

    pinMode(PIN_SINE, OUTPUT);

    dacWrite(PIN_SINE, 128);


    // ========================================================
    // CONFIGURA GPIO 26 - TRIÂNGULO
    // ========================================================

    pinMode(PIN_TRIANGLE, OUTPUT);

    dacWrite(PIN_TRIANGLE, 0);


    // ========================================================
    // CONFIGURA GPIO 27 - QUADRADA
    // ========================================================

    pinMode(PIN_SQUARE, OUTPUT);

    analogWriteFrequency(
        PIN_SQUARE,
        SQUARE_FREQUENCY
    );

    analogWriteResolution(
        PIN_SQUARE,
        8
    );

    analogWrite(
        PIN_SQUARE,
        (SQUARE_DUTY * 255) / 100
    );


    // ========================================================
    // CONFIGURA GPIO 14 - DENTE DE SERRA
    // ========================================================

    pinMode(PIN_SAW, OUTPUT);

    analogWriteFrequency(
        PIN_SAW,
        SAW_PWM_FREQUENCY
    );

    analogWriteResolution(
        PIN_SAW,
        8
    );

    analogWrite(
        PIN_SAW,
        0
    );


    // ========================================================
    // CONFIGURA GPIO 32 - PULSO
    // ========================================================

    pinMode(PIN_PULSE, OUTPUT);

    analogWriteFrequency(
        PIN_PULSE,
        PULSE_FREQUENCY
    );

    analogWriteResolution(
        PIN_PULSE,
        8
    );

    analogWrite(
        PIN_PULSE,
        (PULSE_DUTY * 255) / 100
    );


    // ========================================================
    // CONFIGURA GPIO 33 - PWM
    // ========================================================

    pinMode(PIN_PWM, OUTPUT);

    analogWriteFrequency(
        PIN_PWM,
        PWM_FREQUENCY
    );

    analogWriteResolution(
        PIN_PWM,
        8
    );

    analogWrite(
        PIN_PWM,
        (PWM_DUTY * 255) / 100
    );


    // ========================================================
    // INFORMAÇÕES
    // ========================================================

    Serial.println("Canais configurados:");
    Serial.println();

    Serial.println("----------------------------------------------");

    Serial.print("GPIO ");
    Serial.print(PIN_SINE);
    Serial.print(" -> SENOIDE       ");
    Serial.print(SINE_FREQUENCY);
    Serial.println(" Hz");

    Serial.print("GPIO ");
    Serial.print(PIN_TRIANGLE);
    Serial.print(" -> TRIANGULAR    ");
    Serial.print(TRIANGLE_FREQUENCY);
    Serial.println(" Hz");

    Serial.print("GPIO ");
    Serial.print(PIN_SQUARE);
    Serial.print(" -> QUADRADA      ");
    Serial.print(SQUARE_FREQUENCY);
    Serial.print(" Hz / ");
    Serial.print(SQUARE_DUTY);
    Serial.println("%");

    Serial.print("GPIO ");
    Serial.print(PIN_SAW);
    Serial.print(" -> DENTE SERRA   ");
    Serial.print(SAW_FREQUENCY);
    Serial.println(" Hz");

    Serial.print("GPIO ");
    Serial.print(PIN_PULSE);
    Serial.print(" -> PULSO         ");
    Serial.print(PULSE_FREQUENCY);
    Serial.print(" Hz / ");
    Serial.print(PULSE_DUTY);
    Serial.println("%");

    Serial.print("GPIO ");
    Serial.print(PIN_PWM);
    Serial.print(" -> PWM           ");
    Serial.print(PWM_FREQUENCY);
    Serial.print(" Hz / ");
    Serial.print(PWM_DUTY);
    Serial.println("%");

    Serial.println("----------------------------------------------");

    Serial.println();
    Serial.println("Gerador iniciado.");
    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Atualiza os sinais analógicos
    updateSine();
    updateTriangle();

    // Atualiza o dente de serra
    updateSaw();

    // Os sinais PWM dos GPIOs 27, 32 e 33
    // continuam sendo gerados pelo hardware.
}

