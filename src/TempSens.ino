#include <OneWire.h>
#include <DallasTemperature.h>
#include "HA.h"

extern OneWire ds_int;
extern OneWire ow;
DallasTemperature sensors_int(&ds_int);
DallasTemperature sensors_ext(&ow);
DeviceAddress insideThermometer;

bool tempsens_found = false;
float tempsens_value = 0;

void tempsens_setup_single(DallasTemperature *sensors)
{
    char msg[64];

    sensors->begin();

    if (sensors->getDeviceCount() == 0)
    {
        mqtt_publish_string("tele/%s/response", "Tempsens: No devices found");
        return;
    }
    sprintf(msg, "Tempsens: %d devices found", sensors->getDeviceCount());
    mqtt_publish_string("tele/%s/response", msg);
    mqtt_publish_string("tele/%s/response", sensors->isParasitePowerMode() ? "Tempsens: parasite on" : "Tempsens: parasite off");
}

void tempsens_setup()
{
    tempsens_setup_single(&sensors_ext);
    tempsens_setup_single(&sensors_int);
    
    if (!sensors_int.getAddress(insideThermometer, 0))
    {
        Serial.println("Unable to find address for Device 0");
        return;
    }

    tempsens_found = true;
    Serial.printf("[Sensors] Found 1-Wire sensor: %02X%02X%02X%02X%02X%02X%02X%02X\n", insideThermometer[0], insideThermometer[1], insideThermometer[2], insideThermometer[3], insideThermometer[4], insideThermometer[5], insideThermometer[6], insideThermometer[7]);
}

bool tempsens_loop()
{
    int curTime = millis();
    static int nextTime = 0;

    if (nextTime > curTime)
    {
        return false;
    }
    nextTime = curTime + 10000;

    if (!tempsens_found)
    {
        tempsens_setup();
        return false;
    }

    sensors_int.requestTemperatures();
    tempsens_value = sensors_int.getTempC(insideThermometer);

    if(!sensors_ext.getDeviceCount())
    {
        mqtt_publish_string("tele/%s/response", "No sensors found, rescan");
        tempsens_setup_single(&sensors_ext);
    }
    sensors_ext.requestTemperatures();

    for(int pos = 0; pos < sensors_ext.getDeviceCount(); pos++)
    {
        char topic[48];
        char name[32];
        char id[32];
        char tmp[3];
        uint8_t address[8];

        if(!sensors_ext.getAddress(address, pos))
        {
            break;
        }
        float temp = sensors_ext.getTempC(address);
        

        strcpy(topic, "feeds/float/%s/tmp-");
        strcpy(id, "tmp-");
        strcpy(name, "Sensor ");
        for (uint8_t i = 0; i < 8; i++)
        {
            sprintf(tmp, "%02X", address[i]);
            strcat(topic, tmp);
            strcat(id, tmp);
            strcat(name, tmp);
        }
        
        if(!ha_exists(id))
        {
            t_ha_entity entity;

            memset(&entity, 0x00, sizeof(entity));
            entity.id = strdup(id);
            entity.name = strdup(name);
            entity.dev_class = "temperature";
            entity.unit_of_meas = "°C";
            entity.type = ha_sensor;
            entity.stat_t = strdup(topic);

            if(!ha_add(&entity))
            {
                free((void*)entity.id);
                free((void*)entity.name);
                free((void*)entity.stat_t);
            }
        }

        if(temp > -30.0f && temp < 120.0f)
        {
            mqtt_publish_float(topic, temp);
        }
    }

    return false;
}
