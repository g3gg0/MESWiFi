
#define RELAYS_1_PIN 14
#define RELAYS_2_PIN 12

typedef struct
{
    int pin;
    bool state;
    int next_time;
    bool next_state;
} relays_job_t;

static relays_job_t relays_job[2] = {{.pin = RELAYS_1_PIN}, {.pin = RELAYS_2_PIN}};

void relays_setup()
{
    digitalWrite(RELAYS_1_PIN, LOW);
    digitalWrite(RELAYS_2_PIN, LOW);
    pinMode(RELAYS_1_PIN, OUTPUT);
    pinMode(RELAYS_2_PIN, OUTPUT);

    relays_set(0, false);
    relays_set(1, false);
}

void relays_set(int num, bool enabled)
{
    Serial.printf("[REL] setting %d to %s\n", num, enabled ? "ON" : "OFF");

    relays_job[num].state = enabled;
    digitalWrite(relays_job[num].pin, enabled ? HIGH : LOW);
}

void relays_set_timed(int num, int ms, bool enabled)
{
    relays_job[num].next_state = enabled;
    relays_job[num].next_time = millis() + ms;
    Serial.printf("[REL] setting %d to %s in %d ms\n", num, enabled ? "ON" : "OFF", ms);
}

void relays_job_loop(int num)
{
    if (relays_job[num].next_time == 0)
    {
        return;
    }

    if (millis() >= relays_job[num].next_time)
    {
        relays_job[num].next_time = 0;
        relays_set(num, relays_job[num].next_state);
    }
}

bool relays_loop()
{
    relays_job_loop(0);
    relays_job_loop(1);

    return false;
}
