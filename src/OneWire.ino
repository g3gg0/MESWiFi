
#include <OneWire.h>

#define APP_PLUS 33
#define APP_MINUS 32

#define TEMP_INT 13

OneWire ds_int(TEMP_INT);
OneWire ow(APP_PLUS);

void onewire_setup()
{
    digitalWrite(APP_PLUS, LOW);
    digitalWrite(APP_MINUS, LOW);
    pinMode(APP_PLUS, OUTPUT);
    pinMode(APP_MINUS, INPUT);
}

void onewire_loop()
{
}


void onewire_read()
{
}

void onewire_search()
{
    char msg[64];
    uint8_t address[8];
    uint8_t count = 0;

    mqtt_publish_string("tele/%s/response", "Searching...");

    ow.reset_search();

    while (ow.search(address))
    {
        count++;
        sprintf(msg, " found: ");
        for (uint8_t i = 0; i < 8; i++)
        {
            char tmp[64];
            sprintf(tmp, "%02X", address[i]);
            strcat(msg, tmp);
        }

        mqtt_publish_string("tele/%s/response", msg);
    }
    sprintf(msg, "Done, %d devices found", count);
    mqtt_publish_string("tele/%s/response", msg);
}