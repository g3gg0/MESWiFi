#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_SOFTAPNAME  "esp32-config"
#define CONFIG_OTANAME     "MES-WiFi"

#define CONFIG_MAGIC 0xE1AAFF09


typedef struct
{
    uint32_t magic;
    uint32_t version;

    uint32_t verbose;
    uint32_t mqtt_publish;
    char hostname[32];
    char wifi_ssid[32];
    char wifi_password[32];
    char mqtt_server[32];
    int mqtt_port;
    char mqtt_user[32];
    char mqtt_password[32];
    char mqtt_client[32];
} t_cfg;

extern t_cfg current_config;


#endif
