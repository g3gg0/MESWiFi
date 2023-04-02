
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

void mqtt_setup()
{
    mqtt.setCallback(callback);

    ha_setup();

    t_ha_entity entity;

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_ota";
    entity.name = "Enable OTA";
    entity.type = ha_button;
    entity.cmd_t = "command/%s/ota";
    entity.received = &mqtt_ota_received;
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_rssi";
    entity.name = "WiFi RSSI";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rssi";
    entity.unit_of_meas = "dBm";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_error";
    entity.name = "Error message";
    entity.type = ha_text;
    entity.stat_t = "feeds/string/%s/error";
    entity.cmd_t = "feeds/string/%s/error";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_burning-minutes";
    entity.name = "Brenndauer";
    entity.state_class = "total_increasing";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/burning-minutes";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_last-error-minutes";
    entity.name = "Letzter Fehlerzeitpunkt";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/last-error-minutes";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_last-error-delta";
    entity.name = "Zeit seit letztem Fehler";
    entity.dev_class = "duration";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/last-error-delta";
    entity.unit_of_meas = "min";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_rx-count";
    entity.name = "Botschaftszähler Rx";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rx-count";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_rx-crc";
    entity.name = "Botschaftszähler Rx CRC-Fehler";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/rx-crc";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_fehlercode";
    entity.name = "Fehlercode";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/fehlercode";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_drehzahl";
    entity.name = "Drehzahl";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/drehzahl";
    entity.unit_of_meas = "rpm";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_betriebsstunden";
    entity.name = "Betriebsstunden";
    entity.dev_class = "duration";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/betriebsstunden";
    entity.unit_of_meas = "h";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_anheiz-count";
    entity.name = "Anheizzähler";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/anheiz-count";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_zustand";
    entity.name = "Zustand";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/zustand";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_display";
    entity.name = "Displayanzeige";
    entity.type = ha_text;
    entity.stat_t = "feeds/string/%s/display";
    entity.cmd_t = "feeds/string/%s/display";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_foerder-soll";
    entity.name = "Fördermenge/h soll";
    entity.dev_class = "weight";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/foerder-soll";
    entity.val_tpl = "{{ (value|float) / 10 }}";
    entity.unit_of_meas = "kg";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_foerder-berech";
    entity.name = "Fördermenge/h berechnet";
    entity.dev_class = "weight";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/integer/%s/foerder-berech";
    entity.val_tpl = "{{ (value|float) / 10 }}";
    entity.unit_of_meas = "kg";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_foerder-integral-kWh";
    entity.name = "Fördermenge integriert";
    entity.dev_class = "energy";
    entity.state_class = "total_increasing";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/foerder-integral-kWh";
    entity.unit_of_meas = "kWh";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_leistung";
    entity.name = "Leistung ist";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/leistung-soll";
    entity.unit_of_meas = "%";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_leistung-soll";
    entity.name = "Leistung soll";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/leistung-soll";
    entity.unit_of_meas = "%";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-pcb";
    entity.name = "Steuereinheit";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-pcb";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-kammer";
    entity.name = "Temperatur Brennkammer";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-kammer";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-abgas";
    entity.name = "Temperatur Abgas";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-abgas";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-kessel-soll";
    entity.name = "Temperatur Kessel soll";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-kessel-soll";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-kessel";
    entity.name = "Temperatur Kessel";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-kessel";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-aussen";
    entity.name = "Temperatur Aussen";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-aussen";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-vorlauf-soll-hk";
    entity.name = "Temperatur Vorlauf Heizkörper soll";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-vorlauf-soll-hk";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-vorlauf-soll-ww";
    entity.name = "Temperatur Vorlauf Warmwasser soll";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-vorlauf-soll-ww";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-soll-ww";
    entity.name = "Temperatur Warmwasser soll";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-soll-ww";
    ha_add(&entity);

    memset(&entity, 0x00, sizeof(entity));
    entity.id = "mes_wifi_temp-speicher";
    entity.name = "Temperatur Speicher";
    entity.dev_class = "temperature";
    entity.unit_of_meas = "°C";
    entity.type = ha_sensor;
    entity.stat_t = "feeds/float/%s/temp-speicher";
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
            mqtt_publish_int("feeds/integer/%s/last-error-minutes", lon_stat.last_error_minutes);

            if (lon_stat.burning_minutes != 0x7FFFFFFF && lon_stat.last_error_minutes != 0x7FFFFFFF)
            {
                mqtt_publish_int("feeds/integer/%s/last-error-delta", lon_stat.burning_minutes - lon_stat.last_error_minutes);
            }
            mqtt_publish_int("feeds/integer/%s/time-hour", timeStruct.tm_hour);
            mqtt_publish_int("feeds/integer/%s/time-minute", timeStruct.tm_min);
            mqtt_publish_int("feeds/integer/%s/ignite-stat-pos", lon_stat.igniteStatPos);

            mqtt_publish_int("feeds/integer/%s/anheiz-stat", lon_stat.ignites_24h);
            mqtt_publish_int("feeds/integer/%s/rx-count", lon_stat.rx_count);
            mqtt_publish_int("feeds/integer/%s/rx-crc", lon_stat.crc_errors);

            mqtt_publish_int("feeds/integer/%s/fehlercode", lon_stat.error);
            mqtt_publish_int("feeds/integer/%s/drehzahl", lon_stat.var_nv_12);
            mqtt_publish_int("feeds/integer/%s/betriebsstunden", lon_stat.var_nv_2A);
            mqtt_publish_int("feeds/integer/%s/anheiz-count", lon_stat.var_nv_2B);
            mqtt_publish_int("feeds/integer/%s/zustand", lon_stat.var_nv_10_state);

            mqtt_publish_string("feeds/string/%s/display", (const char *)lon_stat.var_nv_10);
            mqtt_publish_float("feeds/float/%s/temp-pcb", tempsens_value);

            mqtt_publish_int("feeds/integer/%s/foerder-soll", lon_stat.var_nv_15_pmx);
            mqtt_publish_int("feeds/integer/%s/foerder-berech", lon_stat.var_nv_1B_pmx);

            int expired_ms = (time - mqtt_last_publish_time);
            /* calc energy with
                  kg/h  *  kWh/kg  /  timestep
               where
                  timestep = miliseconds since last read
            */
            foerder_integral += ((float)lon_stat.var_nv_15_pmx / 10.0f * 4.8f * expired_ms) / (60.0f * 60.0f * 1000.0f);
            mqtt_publish_float("feeds/float/%s/foerder-integral-kWh", foerder_integral);

            if (lon_stat.var_nv_23_pmx != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/leistung-soll", lon_stat.var_nv_23_pmx / 2.0f);
            }
            if (lon_stat.var_nv_25_pmx != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/leistung", lon_stat.var_nv_25_pmx / 2.0f);
            }
            if (lon_stat.var_nv_2F != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-kammer", (lon_stat.var_nv_2F / 10.0f) - 273.15f);
            }
            if (lon_stat.var_nv_31 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-abgas", (lon_stat.var_nv_31 / 10.0f) - 273.15f);
            }
            if (lon_stat.var_nv_32 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-kessel-soll", lon_stat.var_nv_32 / 100.0f);
            }
            if (lon_stat.var_sel_110 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-kessel", lon_stat.var_sel_110 / 100.0f);
            }
            if (lon_stat.var_sel_00 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-aussen", lon_stat.var_sel_00 / 100.0f);
            }
            if (lon_stat.var_sel_10 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-vorlauf-soll-hk", lon_stat.var_sel_10 / 100.0f);
            }
            if (lon_stat.var_sel_12 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-vorlauf-soll-ww", lon_stat.var_sel_12 / 100.0f);
            }
            if (lon_stat.var_sel_72 != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-soll-ww", lon_stat.var_sel_72 / 100.0f);
            }
            if (lon_stat.var_nv_1B != 0x7FFFFFFF)
            {
                mqtt_publish_float("feeds/float/%s/temp-speicher", lon_stat.var_nv_1B / 100.0f);
            }

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
