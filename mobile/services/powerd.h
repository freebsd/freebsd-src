/*
 * Power Daemon - Power management service
 * Handles battery, CPU frequency, thermal throttling, suspend/resume
 */

#ifndef _MOBILE_POWERD_H_
#define _MOBILE_POWERD_H_

#include <sys/types.h>

#define POWERD_SOCKET_PATH "/var/run/powerd.sock"

#define BATTERY_STATUS_UNKNOWN  0
#define BATTERY_STATUS_DISCHARGING  1
#define BATTERY_STATUS_CHARGING  2
#define BATTERY_STATUS_FULL  3

#define POWER_STATE_NORMAL    0
#define POWER_STATE_LOW       1
#define POWER_STATE_CRITICAL  2

#define CPU_GOVERNOR_PERFORMANCE "performance"
#define CPU_GOVERNOR_ONDEMAND    "ondemand"
#define CPU_GOVERNOR_POWERSAVE   "powersave"

struct battery_info {
    int capacity;       /* 0-100 percent */
    int status;         /* charging/discharging/full */
    int temperature;    /* in millidegrees Celsius */
};

struct cpu_freq_info {
    int current_freq_khz;
    int min_freq_khz;
    int max_freq_khz;
    char governor[32];
};

int powerd_main(int argc, char **argv);
int powerd_init(void);
void powerd_shutdown(void);
int powerd_get_battery(struct battery_info *info);
int powerd_get_cpu_freq(struct cpu_freq_info *info);
int powerd_set_cpu_freq(const char *governor, int freq_khz);
void powerd_check_battery(void);
void powerd_thermal_throttle(int temp_limit);
void powerd_suspend(void);
void powerd_wakeup(void);

#endif /* _MOBILE_POWERD_H_ */