#include "buzzer.h"
#include "config.h"

static bool buzzerActive = false;
static unsigned long buzzerStartTime = 0;

void setupBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void buzzerBeep()
{
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerStartTime = micros();
}

void buzzerUpdate()
{
    if (buzzerActive && (micros() - buzzerStartTime >= BUZZER_PULSE_US))
    {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
    }
}
