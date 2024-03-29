
// #define TESTMODE

#include <PubSubClient.h>
#include <ESP32httpUpdate.h>
#include <Config.h>

#include "HA.h"

WiFiClient client;
PubSubClient mqtt(client);

extern int wifi_rssi;
extern uint32_t lon_rx_count;
extern uint32_t lon_crc_errors;
extern t_cfg current_config;

float foerder_integral = 0;

uint32_t mqtt_last_publish_time = 0;
uint32_t mqtt_lastConnect = 0;
uint32_t mqtt_retries = 0;
bool mqtt_fail = false;

char command_topic[64];
char response_topic[64];


extern uint32_t manual_set_temperature;
extern bool manual_set_active;
extern uint32_t manual_set_next;
extern uint32_t manual_set_end;
extern uint32_t manual_set_duration;



void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.print("'");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.print("'");
    Serial.println();

    payload[length] = 0;

    ha_received(topic, (const char *)payload);

    if (!strcmp(topic, command_topic))
    {
        char *command = (char *)payload;
        char buf[1024];

        if (!strncmp(command, "http", 4))
        {
            snprintf(buf, sizeof(buf) - 1, "updating from: '%s'", command);
            Serial.printf("%s\n", buf);

            mqtt.publish(response_topic, buf);
            ESPhttpUpdate.rebootOnUpdate(false);
            t_httpUpdate_return ret = ESPhttpUpdate.update(command);

            switch (ret)
            {
            case HTTP_UPDATE_FAILED:
                snprintf(buf, sizeof(buf) - 1, "HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                mqtt.publish(response_topic, buf);
                Serial.printf("%s\n", buf);
                break;

            case HTTP_UPDATE_NO_UPDATES:
                snprintf(buf, sizeof(buf) - 1, "HTTP_UPDATE_NO_UPDATES");
                mqtt.publish(response_topic, buf);
                Serial.printf("%s\n", buf);
                break;

            case HTTP_UPDATE_OK:
                snprintf(buf, sizeof(buf) - 1, "HTTP_UPDATE_OK");
                mqtt.publish(response_topic, buf);
                Serial.printf("%s\n", buf);
                delay(500);
                ESP.restart();
                break;

            default:
                snprintf(buf, sizeof(buf) - 1, "update failed");
                mqtt.publish(response_topic, buf);
                Serial.printf("%s\n", buf);
                break;
            }
        }
        else if (!strncmp(command, "ow", 2))
        {
            onewire_search();
        }
        else
        {
            snprintf(buf, sizeof(buf) - 1, "unknown command: '%s'", command);
            mqtt.publish(response_topic, buf);
            Serial.printf("%s\n", buf);
        }
    }
}

void mqtt_ota_received(const t_ha_entity *entity, void *ctx, const char *message)
{
    ota_setup();
}

void mqtt_manual_set_temperature_received(const t_ha_entity *entity, void *ctx, const char *message)
{
    int temperature = 0;

    int ret = sscanf(message, "%d", &temperature);
    if (ret != 1)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to parse '%s': %d", message, ret);
        mqtt_publish_string("feeds/string/%s/error", msg);
        return;
    }

    manual_set_temperature = temperature;
}

void mqtt_manual_set_temperature_transmit(const t_ha_entity *entity, void *ctx)
{
    char state_buf[64];

    sprintf(state_buf, "%d", manual_set_temperature);

    ha_transmit(entity, state_buf);
}

void mqtt_manual_set_active_received(const t_ha_entity *entity, void *ctx, const char *message)
{
    bool active = false;

    if (!strcasecmp(message, "ON"))
    {
        active = true;
        manual_set_next = 0;
        manual_set_end = millis() + manual_set_duration * 1000;
    }

    manual_set_active = active;
}

void mqtt_manual_set_active_transmit(const t_ha_entity *entity, void *ctx)
{
    char state_buf[64];

    sprintf(state_buf, "%s", manual_set_active?"ON":"OFF");

    ha_transmit(entity, state_buf);
}

void mqtt_manual_set_duration_received(const t_ha_entity *entity, void *ctx, const char *message)
{
    int duration = 0;

    int ret = sscanf(message, "%d", &duration);
    if (ret != 1)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Failed to parse '%s': %d", message, ret);
        mqtt_publish_string("feeds/string/%s/error", msg);
        return;
    }

    manual_set_duration = duration;
}

void mqtt_manual_set_duration_transmit(const t_ha_entity *entity, void *ctx)
{
    char state_buf[64];

    sprintf(state_buf, "%d", manual_set_duration);

    ha_transmit(entity, state_buf);
}



void mqtt_setup()
{
    mqtt.setCallback(callback);

    t_ha_entity entity;

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "ota";
    entity.name = "Enable OTA";
    entity.type = ha_button;
    entity.cmd_t = "command/%s/ota";
    entity.received = &mqtt_ota_received;
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "rssi";
    entity.name = "WiFi RSSI";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rssi";
    entity.unit_of_meas = "dBm";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "error";
    entity.name = "Error message";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/string/%s/error";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "burning-minutes";
    entity.name = "Brenndauer";
    entity.state_class = "total_increasing";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/burning-minutes";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "last-error-minutes";
    entity.name = "Letzter Fehlerzeitpunkt";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/last-error-minutes";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "last-error-delta";
    entity.name = "Zeit seit letztem Fehler";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/last-error-delta";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "rx-count";
    entity.name = "Botschaftszähler Rx";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rx-count";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "rx-crc";
    entity.name = "Botschaftszähler Rx CRC-Fehler";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rx-crc";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "fehlercode";
    entity.name = "Fehlercode";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/fehlercode";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "display";
    entity.name = "Displayanzeige";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/string/%s/display";
    entity.cmd_t = "feeds/string/%s/display";
    ha_add(&entity);


    memset(&entity, 0x00, sizeof(entity));
    entity.id = "foerder-integral-kWh";
    entity.name = "Fördermenge integriert";
    entity.dev_class = "energy";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/foerder-integral-kWh";
    entity.unit_of_meas = "kWh";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "temp-pcb";
    entity.name = "Steuereinheit";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-pcb";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "temp-soll-set";
    entity.name = "Anforderung: Temperatur Vorlauf";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_number;
    entity.min = 0;
    entity.max = 80;
    entity.mode = "slider";
    entity.cmd_t = "command/%s/temp-soll-set";
    entity.stat_t = "state/%s/temp-soll-set";
    entity.received = &mqtt_manual_set_temperature_received;
    entity.transmit = &mqtt_manual_set_temperature_transmit;
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "temp-soll-duration";
    entity.name = "Anforderung: Dauer";
    entity.unit_of_meas = "s";
    entity.type = ha_number;
    entity.min = 0;
    entity.max = 3600;
    entity.mode = "slider";
    entity.cmd_t = "command/%s/temp-soll-duration";
    entity.stat_t = "state/%s/temp-soll-duration";
    entity.received = &mqtt_manual_set_duration_received;
    entity.transmit = &mqtt_manual_set_duration_transmit;
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "temp-soll-active";
    entity.name = "Anforderung: Aktiv";
    entity.type = ha_switch;
    entity.cmd_t = "command/%s/temp-soll-active";
    entity.stat_t = "state/%s/temp-soll-active";
    entity.received = &mqtt_manual_set_active_received;
    entity.transmit = &mqtt_manual_set_active_transmit;
    ha_add(&entity);
}

void mqtt_publish_string(const char *name, const char *value)
{
    char path_buffer[128];

    sprintf(path_buffer, name, current_config.mqtt_client);

    if (!mqtt.publish(path_buffer, value))
    {
        mqtt_fail = true;
    }
    Serial.printf("Published %s : %s\n", path_buffer, value);
}

void mqtt_publish_float(const char *name, float value)
{
    char path_buffer[128];
    char buffer[32];

    sprintf(path_buffer, name, current_config.mqtt_client);
    sprintf(buffer, "%0.2f", value);

    if (!mqtt.publish(path_buffer, buffer))
    {
        mqtt_fail = true;
    }
    Serial.printf("Published %s : %s\n", path_buffer, buffer);
}

void mqtt_publish_int(const char *name, uint32_t value)
{
    char path_buffer[128];
    char buffer[32];

    if (value == 0x7FFFFFFF)
    {
        return;
    }
    sprintf(path_buffer, name, current_config.mqtt_client);
    sprintf(buffer, "%d", value);

    if (!mqtt.publish(path_buffer, buffer))
    {
        mqtt_fail = true;
    }
    Serial.printf("Published %s : %s\n", path_buffer, buffer);
}

bool mqtt_loop()
{
    uint32_t time = millis();
    static uint32_t nextTime = 0;

#ifdef TESTMODE
    return false;
#endif
    if (mqtt_fail)
    {
        mqtt_fail = false;
        mqtt.disconnect();
    }

    MQTT_connect();

    if (!mqtt.connected())
    {
        return false;
    }

    mqtt.loop();

    ha_loop();

    if (time >= nextTime)
    {
        bool do_publish = false;

        if ((time - mqtt_last_publish_time) > 5000)
        {
            do_publish = true;
        }

        if (do_publish)
        {
            /* debug */
            mqtt_publish_int("feeds/integer/%s/rssi", wifi_rssi);
            mqtt_publish_int("feeds/integer/%s/burning-minutes", lon_stat.burning_minutes);

            if (lon_stat.last_error_minutes < 0x40000000)
            {
                mqtt_publish_int("feeds/integer/%s/last-error-minutes", lon_stat.last_error_minutes);
                mqtt_publish_int("feeds/integer/%s/last-error-delta", lon_stat.burning_minutes - lon_stat.last_error_minutes);
            }
            mqtt_publish_int("feeds/integer/%s/time-hour", timeStruct.tm_hour);
            mqtt_publish_int("feeds/integer/%s/time-minute", timeStruct.tm_min);
            mqtt_publish_int("feeds/integer/%s/ignite-stat-pos", lon_stat.igniteStatPos);
            mqtt_publish_int("feeds/integer/%s/anheiz-stat", lon_stat.ignites_24h);
            mqtt_publish_int("feeds/integer/%s/rx-count", lon_stat.rx_count);
            mqtt_publish_int("feeds/integer/%s/rx-crc", lon_stat.crc_errors);
            mqtt_publish_float("feeds/float/%s/temp-pcb", tempsens_value);

            int expired_ms = (time - mqtt_last_publish_time);
            /* calc energy with
                  kg/h  *  kWh/kg  /  timestep
               where
                  timestep = miliseconds since last read
            */
            foerder_integral += (lon_stat.foerder_soll * 4.8f * expired_ms) / (60.0f * 60.0f * 1000.0f);
            mqtt_publish_float("feeds/float/%s/foerder-integral-kWh", foerder_integral);

            mqtt_last_publish_time = time;
        }
        nextTime = time + 1000;
    }

    return false;
}

void MQTT_connect()
{
    uint32_t curTime = millis();
    int8_t ret;

    if (strlen(current_config.mqtt_server) == 0)
    {
        return;
    }

    mqtt.setServer(current_config.mqtt_server, current_config.mqtt_port);

    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (mqtt.connected())
    {
        return;
    }

    if ((mqtt_lastConnect != 0) && (curTime - mqtt_lastConnect < (1000 << mqtt_retries)))
    {
        return;
    }

    mqtt_lastConnect = curTime;

    Serial.println("MQTT: Connecting to MQTT... ");

    sprintf(command_topic, "tele/%s/command", current_config.mqtt_client);
    sprintf(response_topic, "tele/%s/response", current_config.mqtt_client);

    ret = mqtt.connect(current_config.mqtt_client, current_config.mqtt_user, current_config.mqtt_password);

    if (ret == 0)
    {
        mqtt_retries++;
        if (mqtt_retries > 8)
        {
            mqtt_retries = 8;
        }
        Serial.printf("MQTT: (%d) ", mqtt.state());
        Serial.println("MQTT: Retrying MQTT connection");
        mqtt.disconnect();
    }
    else
    {
        Serial.println("MQTT Connected!");
        mqtt.subscribe(command_topic);
        ha_connected();
        mqtt_publish_string((char *)"feeds/string/%s/error", "");
    }
}
