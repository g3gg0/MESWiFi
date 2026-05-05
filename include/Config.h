#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_SOFTAPNAME "esp32-config"
#define CONFIG_OTANAME "MES-WiFi"

#define CONFIG_MAGIC 0xE1AAFF1B

#define VERBOSE_WIFI  (1 << 0)
#define VERBOSE_HA    (1 << 2)

typedef struct
{
    uint32_t magic;

    char hostname[32];
    char wifi_ssid[32];
    char wifi_password[32];

    char mqtt_server[32];
    uint32_t mqtt_port;
    char mqtt_user[32];
    char mqtt_password[32];
    char mqtt_client[32];

    uint32_t version;

    uint32_t verbose;
    uint32_t mqtt_publish;
} t_cfg;

extern t_cfg current_config;

#endif
