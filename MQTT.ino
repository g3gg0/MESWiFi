#define MQTT_DEBUG

#include <PubSubClient.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#define ARB_SERVER      "..."
#define ARB_CLIENT      "MES-WiFi"
#define ARB_SERVERPORT  11883
#define ARB_USERNAME    "..."
#define ARB_PW          "..."

WiFiClient client;
PubSubClient mqtt(client);

extern uint32_t lon_rx_count;
extern uint32_t lon_crc_errors;

int mqtt_last_publish_time = 0;
int mqtt_lastConnect = 0;
int mqtt_retries = 0;
bool mqtt_fail = false;

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqtt_setup()
{
  mqtt.setServer(ARB_SERVER, ARB_SERVERPORT);
  mqtt.setCallback(callback);
}

void mqtt_publish_float(char *name, float value)
{
  char buffer[32];
  
  sprintf(buffer, "%0.2f", value);
  if(!mqtt.publish(name, buffer))
  {
    mqtt_fail = true;
  }
  Serial.printf("Published %s : %s\n", name, buffer);
}

void mqtt_publish_int(char *name, uint32_t value)
{
  char buffer[32];

  if(value == 0x7FFFFFFF)
  {
    return;
  }
  sprintf(buffer, "%d", value);
  mqtt.publish(name, buffer);
  Serial.printf("Published %s : %s\n", name, buffer);
}

bool mqtt_loop()
{
  uint32_t time = millis();
  static int nextTime = 0;

  if(mqtt_fail)
  {
    mqtt_fail = false;
    mqtt.disconnect();
  }
  
  MQTT_connect();
  
  if(!mqtt.connected())
  {
    return false;
  }
  
  mqtt.loop();
  
  if(time >= nextTime)
  {
    bool do_publish = false;
    
    if((time - mqtt_last_publish_time) > 5000)
    {
      do_publish = true;
    }
  
    if(do_publish)
    {
      mqtt_last_publish_time = time;

      /* debug */
      mqtt_publish_int("feeds/integer/heizung/burning-minutes", lon_stat.burning_minutes);
      mqtt_publish_int("feeds/integer/heizung/last-error-minutes", lon_stat.last_error_minutes);
      
      if(lon_stat.burning_minutes != 0x7FFFFFFF && lon_stat.last_error_minutes != 0x7FFFFFFF)
      {
        mqtt_publish_int("feeds/integer/heizung/last-error-delta", lon_stat.burning_minutes - lon_stat.last_error_minutes);
      }
      mqtt_publish_int("feeds/integer/heizung/time-hour", timeStruct.tm_hour);
      mqtt_publish_int("feeds/integer/heizung/time-minute", timeStruct.tm_min);
      mqtt_publish_int("feeds/integer/heizung/ignite-stat-pos", lon_stat.igniteStatPos);
      /* debug */
      
      mqtt_publish_int("feeds/integer/heizung/anheiz-stat", lon_stat.ignites_24h);
      mqtt_publish_int("feeds/integer/heizung/rx-count", lon_stat.rx_count);
      mqtt_publish_int("feeds/integer/heizung/rx-crc", lon_stat.crc_errors);
      
      mqtt_publish_int("feeds/integer/heizung/fehlercode", lon_stat.error);
      mqtt_publish_int("feeds/integer/heizung/drehzahl", lon_stat.var_nv_12);
      mqtt_publish_int("feeds/integer/heizung/betriebsstunden", lon_stat.var_nv_2A);
      mqtt_publish_int("feeds/integer/heizung/anheiz-count", lon_stat.var_nv_2B);
      mqtt_publish_int("feeds/integer/heizung/zustand", lon_stat.var_nv_10_state);
      
      mqtt.publish("feeds/string/heizung/display", (const char*)lon_stat.var_nv_10);
      mqtt_publish_float("feeds/float/heizung/temp-pcb", tempsens_value);

      if(lon_stat.var_nv_2F != 0x7FFFFFFF)
      {
        mqtt_publish_float("feeds/float/heizung/temp-kammer", (lon_stat.var_nv_2F / 10.0f) - 273.15f);
      }
      if(lon_stat.var_nv_31 != 0x7FFFFFFF)
      {
        mqtt_publish_float("feeds/float/heizung/temp-abgas", (lon_stat.var_nv_31 / 10.0f) - 273.15f);
      }
      if(lon_stat.var_sel_110 != 0x7FFFFFFF)
      {
        mqtt_publish_float("feeds/float/heizung/temp-kessel", lon_stat.var_sel_110 / 100.0f);
      }
      if(lon_stat.var_sel_00 != 0x7FFFFFFF)
      {
        mqtt_publish_float("feeds/float/heizung/temp-aussen", lon_stat.var_sel_00 / 100.0f);
      }
      if(lon_stat.var_nv_1B != 0x7FFFFFFF)
      {
        mqtt_publish_float("feeds/float/heizung/temp-speicher", lon_stat.var_nv_1B / 100.0f);
      }
    }
    nextTime = time + 1000;
  }

  return false;
}

void MQTT_connect()
{
  int curTime = millis();
  int8_t ret;

  if(WiFi.status() != WL_CONNECTED)
  {
    return;
  }
  
  if(mqtt.connected())
  {
    return;
  }

  if((mqtt_lastConnect != 0) && (curTime - mqtt_lastConnect < (1000 << mqtt_retries)))
  {
    return;
  }

  mqtt_lastConnect = curTime;

  Serial.print("MQTT: Connecting to MQTT... ");
  ret = mqtt.connect(ARB_CLIENT, ARB_USERNAME, ARB_PW);
  
  if(ret == 0)
  {
    mqtt_retries++;
    if(mqtt_retries > 8)
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
    mqtt.subscribe("feeds/integer/heizung/relay1");
    mqtt.subscribe("feeds/integer/heizung/relay2");
  }
}

