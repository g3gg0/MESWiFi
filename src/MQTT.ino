
//#define TESTMODE

#include <PubSubClient.h>
#include <ESP32httpUpdate.h>
#include <Config.h>


WiFiClient client;
PubSubClient mqtt(client);

extern uint32_t lon_rx_count;
extern uint32_t lon_crc_errors;
extern t_cfg current_config;

float foerder_integral = 0;

int mqtt_last_publish_time = 0;
int mqtt_lastConnect = 0;
int mqtt_retries = 0;
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

    if (!strcmp(topic, command_topic))
    {
        char *command = (char *)payload;
        char buf[1024];

        if (!strncmp(command, "http", 4))
        {
            snprintf(buf, sizeof(buf)-1, "updating from: '%s'", command);
            Serial.printf("%s\n", buf);

            mqtt.publish(response_topic, buf);
            ESPhttpUpdate.rebootOnUpdate(false);
            t_httpUpdate_return ret = ESPhttpUpdate.update(command);

            switch (ret)
            {
                case HTTP_UPDATE_FAILED:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    mqtt.publish(response_topic, buf);
                    Serial.printf("%s\n", buf);
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_NO_UPDATES");
                    mqtt.publish(response_topic, buf);
                    Serial.printf("%s\n", buf);
                    break;

                case HTTP_UPDATE_OK:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_OK");
                    mqtt.publish(response_topic, buf);
                    Serial.printf("%s\n", buf);
                    delay(500);
                    ESP.restart();
                    break;

                default:
                    snprintf(buf, sizeof(buf)-1, "update failed");
                    mqtt.publish(response_topic, buf);
                    Serial.printf("%s\n", buf);
                    break;
            }
        }
        else
        {
            snprintf(buf, sizeof(buf)-1, "unknown command: '%s'", command);
            mqtt.publish(response_topic, buf);
            Serial.printf("%s\n", buf);
        }
    }
}

void mqtt_setup()
{
    mqtt.setCallback(callback);
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
    static int nextTime = 0;

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
            foerder_integral += ((float)lon_stat.var_nv_15_pmx * 4.8f * expired_ms) / (60.0f * 60.0f * 1000.0f);
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
    int curTime = millis();
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
        mqtt_publish_string((char *)"feeds/string/%s/error", "");
    }
}
