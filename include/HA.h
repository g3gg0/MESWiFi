#ifndef __HA_H__
#define __HA_H__

#define MAX_LEN 32
#define MAX_ENTITIES 32

typedef enum
{
    ha_unused = 0,
    /* https://www.home-assistant.io/integrations/text.mqtt/ */
    ha_text,
    /* https://www.home-assistant.io/integrations/sensor.mqtt/ */
    ha_sensor,
    /* https://www.home-assistant.io/integrations/number.mqtt/ */
    ha_number,
    /* https://www.home-assistant.io/integrations/button.mqtt/ */
    ha_button,
    /* https://www.home-assistant.io/integrations/select.mqtt/ */
    ha_select,
    /* https://www.home-assistant.io/integrations/binary_sensor.mqtt/ */
    ha_binary_sensor
} t_ha_device_type;

typedef struct s_ha_entity t_ha_entity;

struct s_ha_entity
{
    t_ha_device_type type;

    const char *name;
    const char *id;

    /* used by: sensor */
    const char *unit_of_meas;
    /* used by: sensor */
    const char *val_tpl;
    /* used by: sensor */
    const char *dev_class;
    /* used by: sensor */
    const char *state_class;
    /* used by: button, number, text */
    const char *cmd_t;
    /* used by: sensor, binary_sensor, number, text */
    const char *stat_t;
    /* used by: switch, comma separated */
    const char *options;
    /* used by: number */
    float min;
    /* used by: number */
    float max;
    /* icon */
    const char *ic;
    /* entity_category */
    const char *ent_cat;

    void (*received)(const t_ha_entity *, void *, const char *);
    void *received_ctx;
    void (*transmit)(const t_ha_entity *, void *);
    void *transmit_ctx;
};

typedef struct
{
    char name[MAX_LEN];
    char id[MAX_LEN];
    char cu[MAX_LEN];
    char mf[MAX_LEN];
    char mdl[MAX_LEN];
    char sw[MAX_LEN];
    t_ha_entity entities[MAX_ENTITIES];
    int entitiy_count;
} t_ha_info;

void ha_setup();
void ha_connected();
bool ha_loop();
void ha_transmit_all();
void ha_publish();
void ha_add(t_ha_entity *entity);
void ha_received(char *topic, const char *payload);
void ha_transmit(const t_ha_entity *entity, const char *value);
int ha_parse_index(const char *options, const char *message);
void ha_get_index(const char *options, int index, char *text);

#endif
