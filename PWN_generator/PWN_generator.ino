// ============================================================
// ESP32 - Gerador de 2 sinais PWM
// ============================================================

// ============================================================
// PWM 1
// ============================================================

const int PWM_PIN_1 = 25;

const int PWM_FREQUENCY_1 = 1000;  // Frequência em Hz
const int PWM_RESOLUTION_1 = 8;    // Resolução em bits (0-255)
const int PWM_DUTY_1 = 128;        // 50%


// ============================================================
// PWM 2
// ============================================================

const int PWM_PIN_2 = 26;

const int PWM_FREQUENCY_2 = 1000;  // Frequência em Hz
const int PWM_RESOLUTION_2 = 8;    // Resolução em bits (0-255)
const int PWM_DUTY_2 = 128;        // 50%


void setup()
{
    Serial.begin(115200);

    // ========================================================
    // Configuração do PWM 1
    // ========================================================

    pinMode(PWM_PIN_1, OUTPUT);

    analogWriteFrequency(PWM_PIN_1, PWM_FREQUENCY_1);
    analogWriteResolution(PWM_PIN_1, PWM_RESOLUTION_1);
    analogWrite(PWM_PIN_1, PWM_DUTY_1);


    // ========================================================
    // Configuração do PWM 2
    // ========================================================

    pinMode(PWM_PIN_2, OUTPUT);

    analogWriteFrequency(PWM_PIN_2, PWM_FREQUENCY_2);
    analogWriteResolution(PWM_PIN_2, PWM_RESOLUTION_2);
    analogWrite(PWM_PIN_2, PWM_DUTY_2);


    // ========================================================
    // Informações no Serial Monitor
    // ========================================================

    Serial.println("=================================");
    Serial.println("Gerador PWM - ESP32");
    Serial.println("=================================");

    Serial.println();

    Serial.println("PWM 1:");
    Serial.print("  Pino: ");
    Serial.println(PWM_PIN_1);

    Serial.print("  Frequencia: ");
    Serial.print(PWM_FREQUENCY_1);
    Serial.println(" Hz");

    Serial.print("  Duty cycle: ");
    Serial.print((PWM_DUTY_1 * 100.0) / 255.0);
    Serial.println(" %");

    Serial.println();

    Serial.println("PWM 2:");
    Serial.print("  Pino: ");
    Serial.println(PWM_PIN_2);

    Serial.print("  Frequencia: ");
    Serial.print(PWM_FREQUENCY_2);
    Serial.println(" Hz");

    Serial.print("  Duty cycle: ");
    Serial.print((PWM_DUTY_2 * 100.0) / 255.0);
    Serial.println(" %");

    Serial.println();
    Serial.println("PWM's iniciados.");
}


void loop()
{
    // Os dois sinais PWM continuam sendo gerados
    // pelo hardware do ESP32.
}