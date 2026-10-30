#include <Arduino.h>

// ============================================================
// ESP32 - Simulador de comunicação digital síncrona
//
// GPIO 25 -> CLOCK
// GPIO 26 -> DATA
//
// Mensagem: "OK"
// Transmissão: ASCII, 8 bits, MSB primeiro
// ============================================================

// Pinos
const uint8_t CLOCK_PIN = 25;
const uint8_t DATA_PIN  = 26;

// Frequência do clock em Hz
const uint32_t CLOCK_FREQUENCY = 32000;

// Intervalo entre transmissões (ms)
const uint32_t MESSAGE_INTERVAL = 10;

// Mensagem transmitida
const char MESSAGE[] = "OK";

// Meio período do clock em microssegundos
const uint32_t HALF_PERIOD_US =
    1000000UL / (2 * CLOCK_FREQUENCY);


// ============================================================
// Transmite um único bit
// ============================================================
void sendBit(bool bitValue)
{
    // CLOCK em nível baixo
    digitalWrite(CLOCK_PIN, LOW);

    // Prepara o bit no canal DATA
    digitalWrite(DATA_PIN, bitValue ? HIGH : LOW);

    delayMicroseconds(HALF_PERIOD_US);

    // Borda de subida do CLOCK
    // O receptor pode ler DATA neste instante
    digitalWrite(CLOCK_PIN, HIGH);

    delayMicroseconds(HALF_PERIOD_US);

    // Finaliza o pulso
    digitalWrite(CLOCK_PIN, LOW);
}


// ============================================================
// Transmite um byte (8 bits)
// ============================================================
void sendByte(uint8_t value)
{
    // MSB primeiro
    for (int8_t bit = 7; bit >= 0; bit--)
    {
        bool bitValue = (value >> bit) & 0x01;

        sendBit(bitValue);
    }
}


// ============================================================
// Transmite a mensagem completa
// ============================================================
void sendMessage(const char* message)
{
    while (*message != '\0')
    {
        sendByte((uint8_t)*message);
        message++;
    }

    // Retorna DATA ao nível baixo
    digitalWrite(DATA_PIN, LOW);
}


// ============================================================
// Inicialização
// ============================================================
void setup()
{
    Serial.begin(115200);

    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);

    digitalWrite(CLOCK_PIN, LOW);
    digitalWrite(DATA_PIN, LOW);

    Serial.println("ESP32 - Comunicacao digital");
    Serial.println("CLOCK: GPIO 25");
    Serial.println("DATA: GPIO 26");
    Serial.println("Mensagem: OK");
}


// ============================================================
// Loop principal
// ============================================================
void loop()
{
    sendMessage(MESSAGE);

    Serial.println("Mensagem enviada: OK");

    // Intervalo entre mensagens
    delay(MESSAGE_INTERVAL);
}