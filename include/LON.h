#pragma once

#include <stdint.h>

#define LON_RX 35
#define LON_TX 25
#define LON_TX_EN 26

#define LONSTATE_VERSION 0x0200
#define LON_PACKET_COUNT 8


#define LON_BUFFER_LEN 512

#define COUNT(x) (sizeof(x) / sizeof((x)[0]))


typedef struct
{
    uint8_t filled;
    uint8_t errorCode;
    uint32_t bitCount;
    uint32_t bitDuration;
    uint8_t payload[LON_BUFFER_LEN];
} lonPacket_t;

typedef enum
{
    SNVT_NONE,
    SNVT_DISP,
    /* https://www.lonmark.org/nvs/?id=SNVT_temp_p */
    SNVT_temp_p,
    /* https://www.lonmark.org/nvs/?id=SNVT_temp */
    SNVT_temp,
    /* https://www.lonmark.org/nvs/?id=SNVT_rpm */
    SNVT_rpm,
    /* https://www.lonmark.org/nvs/?id=SNVT_mass_kilo */
    SNVT_mass_kilo,
    /* https://www.lonmark.org/nvs/?id=SNVT_lev_cont */
    SNVT_lev_cont,
    /* https://www.lonmark.org/nvs/?id=SNVT_time_hour */
    SNVT_time_hour,
    /* https://www.lonmark.org/nvs/?id=SNVT_count */
    SNVT_count,
    /* https://www.lonmark.org/nvs/?id=SNVT_lev_percent */
    SNVT_lev_percent
} snvt_type_t;

typedef struct
{
    const snvt_type_t type;
    volatile float *const destination;
    const char *name;
    const char *description;
} nv_handle_item_t;

typedef struct
{
    const uint8_t dest;
    const uint8_t nvar;
    const nv_handle_item_t handle;
} nv_req_item_t;

typedef struct
{
    const uint16_t sel;
    const nv_handle_item_t handle;
} nv_sel_item_t;

typedef struct
{
    uint64_t magic1;
    uint32_t version;

    /* must not be used from stored state */
    uint32_t stat_start_valid;
    struct tm stat_start;

    /* restored from SPIFFS */
    uint32_t error;
    uint32_t rx_count;
    uint32_t crc_errors;
    uint32_t last_error_minutes;
    uint32_t burning_minutes;
    uint32_t avg_len_0;
    uint32_t avg_len_1;
    uint32_t min_len_0;
    uint32_t min_len_1;
    uint32_t max_len_0;
    uint32_t max_len_1;
    uint32_t igniteStatPos;
    uint8_t igniteStats[24 * 12];
    uint32_t ignites_24h;
    float foerder_soll;
    float pmx_state;

    /* footer */
    uint64_t magic2;
} lonStat_t;

static void handle_req_item(const nv_handle_item_t *item, const uint8_t *payload, uint8_t length);