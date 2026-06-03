/*
 * Thermal Management
 * Monitors temperature and triggers thermal throttling/shutdown
 */

#include "thermal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_THERMAL_ZONES 4
#define MAX_COOLING_DEVICES 8

static struct thermal_zone thermal_zones[MAX_THERMAL_ZONES];
static struct cooling_device cooling_devs[MAX_COOLING_DEVICES];
static trip_callback_t trip_callbacks[MAX_THERMAL_ZONES][TRIP_TYPE_COUNT];
static int tz_count = 0;
static int cd_count = 0;
static int thermal_monitor_running = 0;

int th_init(void) {
    tz_count = 0;
    cd_count = 0;
    memset(thermal_zones, 0, sizeof(thermal_zones));
    memset(cooling_devs, 0, sizeof(cooling_devs));
    memset(trip_callbacks, 0, sizeof(trip_callbacks));

    thermal_zones[0].id = 0;
    strlcpy(thermal_zones[0].type, "CPU", sizeof(thermal_zones[0].type));
    thermal_zones[0].current_temp = 35000;
    thermal_zones[0].passive = 85000;
    thermal_zones[0].critical = 95000;
    thermal_zones[0].passive_enabled = 1;
    tz_count++;

    thermal_zones[1].id = 1;
    strlcpy(thermal_zones[1].type, "GPU", sizeof(thermal_zones[1].type));
    thermal_zones[1].current_temp = 30000;
    thermal_zones[1].passive = 90000;
    thermal_zones[1].critical = 100000;
    thermal_zones[1].passive_enabled = 1;
    tz_count++;

    thermal_zones[2].id = 2;
    strlcpy(thermal_zones[2].type, "Battery", sizeof(thermal_zones[2].type));
    thermal_zones[2].current_temp = 25000;
    thermal_zones[2].passive = 60000;
    thermal_zones[2].critical = 70000;
    tz_count++;

    cooling_devs[0].id = 0;
    strlcpy(cooling_devs[0].type, "cpu-thermal", sizeof(cooling_devs[0].type));
    cooling_devs[0].max_state = 100;
    cooling_devs[0].cur_state = 0;
    cd_count++;

    cooling_devs[1].id = 1;
    strlcpy(cooling_devs[1].type, "gpu-fan", sizeof(cooling_devs[1].type));
    cooling_devs[1].max_state = 100;
    cooling_devs[1].cur_state = 20;
    cd_count++;

    return 0;
}

struct thermal_zone *th_get_zones(void) {
    return thermal_zones;
}

int th_get_zone_count(void) {
    return tz_count;
}

struct thermal_zone *th_get_zone_by_id(int id) {
    for (int i = 0; i < tz_count; i++) {
        if (thermal_zones[i].id == id)
            return &thermal_zones[i];
    }
    return NULL;
}

struct thermal_zone *th_get_zone_by_type(const char *type) {
    for (int i = 0; i < tz_count; i++) {
        if (strcmp(thermal_zones[i].type, type) == 0)
            return &thermal_zones[i];
    }
    return NULL;
}

struct cooling_device *th_get_cooling_devices(void) {
    return cooling_devs;
}

int th_get_cooling_count(void) {
    return cd_count;
}

int th_set_temperature(int zone_id, int temp_mc) {
    struct thermal_zone *tz = th_get_zone_by_id(zone_id);
    if (!tz)
        return -1;
    tz->current_temp = temp_mc;
    return 0;
}

int th_cooling_set_state(int cd_id, int state) {
    if (cd_id < 0 || cd_id >= cd_count)
        return -1;
    if (state < 0 || state > cooling_devs[cd_id].max_state)
        return -1;
    cooling_devs[cd_id].cur_state = state;
    return 0;
}

int th_register_trip(int zone_id, trip_type_t type, trip_callback_t cb) {
    if (zone_id < 0 || zone_id >= tz_count)
        return -1;
    if (type < 0 || type >= TRIP_TYPE_COUNT)
        return -1;
    trip_callbacks[zone_id][type] = cb;
    return 0;
}

void th_check_trips(void) {
    for (int i = 0; i < tz_count; i++) {
        struct thermal_zone *tz = &thermal_zones[i];
        if (tz->passive_enabled && tz->current_temp >= tz->passive) {
            trip_callbacks[i][TRIP_PASSIVE](tz);
        }
        if (tz->current_temp >= tz->critical) {
            trip_callbacks[i][TRIP_CRITICAL](tz);
        }
    }
}

void th_monitor_loop(void) {
    thermal_monitor_running = 1;
    while (thermal_monitor_running) {
        th_check_trips();
        sleep(1);
    }
}

void th_stop_monitor(void) {
    thermal_monitor_running = 0;
}

/* Default trip handlers */
void th_default_passive_handler(struct thermal_zone *tz) {
    (void)tz;
}

void th_default_critical_handler(struct thermal_zone *tz) {
    (void)tz;
}
