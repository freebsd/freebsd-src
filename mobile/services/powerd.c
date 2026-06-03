/*
 * Power Daemon - Implementation
 * BSD-style power management service
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include "powerd.h"

static int powerd_running = 0;
static int powerd_socket_fd = -1;
static struct battery_info powerd_battery;
static struct cpu_freq_info powerd_cpu;
static int power_state = POWER_STATE_NORMAL;

static void
powerd_sigterm_handler(int sig __unused)
{
    powerd_running = 0;
}

static int
read_sysfs_int(const char *path, int *value)
{
    int fd;
    char buf[32];
    int len;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;

    len = read(fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        *value = atoi(buf);
    }
    close(fd);
    return len;
}

static int
write_sysfs_int(const char *path, int value)
{
    int fd;
    char buf[32];
    int len;

    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -1;

    len = snprintf(buf, sizeof(buf), "%d", value);
    if (write(fd, buf, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int
write_sysfs_string(const char *path, const char *value)
{
    int fd;
    int len;

    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -1;

    len = strlen(value);
    if (write(fd, value, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int
powerd_get_battery(struct battery_info *info)
{
    int cap;

    memset(info, 0, sizeof(*info));

    if (read_sysfs_int("/sys/class/power_supply/battery/capacity", &cap) < 0) {
        info->capacity = 0;
        info->status = BATTERY_STATUS_UNKNOWN;
        return -1;
    }
    info->capacity = cap;

    /* Check charging status */
    {
        char status_buf[16];
        int fd = open("/sys/class/power_supply/battery/status", O_RDONLY);
        if (fd >= 0) {
            int len = read(fd, status_buf, sizeof(status_buf) - 1);
            close(fd);
            if (len > 0) {
                status_buf[len] = '\0';
                if (strncmp(status_buf, "Charging", 8) == 0)
                    info->status = BATTERY_STATUS_CHARGING;
                else if (strncmp(status_buf, "Full", 4) == 0)
                    info->status = BATTERY_STATUS_FULL;
                else
                    info->status = BATTERY_STATUS_DISCHARGING;
            }
        }
    }

    if (read_sysfs_int("/sys/class/power_supply/battery/temp", &info->temperature) < 0)
        info->temperature = 0;

    powerd_battery = *info;
    return 0;
}

int
powerd_get_cpu_freq(struct cpu_freq_info *info)
{
    memset(info, 0, sizeof(*info));

    if (read_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
                       &info->current_freq_khz) < 0)
        info->current_freq_khz = 0;

    if (read_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
                       &info->min_freq_khz) < 0)
        info->min_freq_khz = 0;

    if (read_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                       &info->max_freq_khz) < 0)
        info->max_freq_khz = 0;

    {
        char gov_buf[32];
        int fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", O_RDONLY);
        if (fd >= 0) {
            int len = read(fd, gov_buf, sizeof(gov_buf) - 1);
            close(fd);
            if (len > 0) {
                gov_buf[len] = '\0';
                strncpy(info->governor, gov_buf, sizeof(info->governor) - 1);
            }
        }
    }

    powerd_cpu = *info;
    return 0;
}

int
powerd_set_cpu_freq(const char *governor, int freq_khz)
{
    if (governor) {
        write_sysfs_string("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", governor);
    }
    if (freq_khz > 0) {
        write_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", freq_khz);
    }
    return 0;
}

void
powerd_check_battery(void)
{
    struct battery_info info;
    int new_power_state = POWER_STATE_NORMAL;

    if (powerd_get_battery(&info) < 0)
        return;

    if (info.capacity < 10) {
        new_power_state = POWER_STATE_CRITICAL;
    } else if (info.capacity < 20) {
        new_power_state = POWER_STATE_LOW;
    }

    if (new_power_state != power_state) {
        syslog(LOG_WARNING, "Power state changed: %d -> %d (battery %d%%)",
               power_state, new_power_state, info.capacity);
        power_state = new_power_state;

        switch (power_state) {
        case POWER_STATE_CRITICAL:
            powerd_suspend();
            break;
        case POWER_STATE_LOW:
            powerd_set_cpu_freq(CPU_GOVERNOR_POWERSAVE, 0);
            break;
        }
    }
}

void
powerd_thermal_throttle(int temp_limit)
{
    struct cpu_freq_info info;
    int max_freq;

    if (powerd_battery.temperature >= temp_limit) {
        powerd_get_cpu_freq(&info);
        max_freq = info.current_freq_khz * 70 / 100; /* Reduce by 30% */
        syslog(LOG_WARNING, "Thermal throttling: temp %d, reducing max freq to %d kHz",
               powerd_battery.temperature, max_freq);
        powerd_set_cpu_freq(info.governor, max_freq);
    }
}

void
powerd_suspend(void)
{
    syslog(LOG_INFO, "Entering suspend state");

    /* Notify display daemon */
    /* In real implementation, would use IPC to displayd */

    /* Enter suspend via sysctl or platform-specific call */
    /* For now, just log */
}

void
powerd_wakeup(void)
{
    syslog(LOG_INFO, "Waking up from suspend");

    /* Resume display */
    /* In real implementation, would use IPC to displayd */
}

int
powerd_init(void)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("powerd", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = powerd_sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Initialize default battery state */
    memset(&powerd_battery, 0, sizeof(powerd_battery));
    powerd_battery.capacity = 50;
    powerd_battery.status = BATTERY_STATUS_DISCHARGING;

    /* Create control socket */
    powerd_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (powerd_socket_fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, POWERD_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(POWERD_SOCKET_PATH);
        if (bind(powerd_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(powerd_socket_fd);
            powerd_socket_fd = -1;
        }
    }

    powerd_running = 1;
    syslog(LOG_INFO, "Power daemon initialized");
    return 0;
}

void
powerd_shutdown(void)
{
    powerd_running = 0;

    if (powerd_socket_fd >= 0) {
        close(powerd_socket_fd);
        unlink(POWERD_SOCKET_PATH);
    }

    closelog();
}

int
powerd_main(int argc __unused, char **argv __unused)
{
    struct pollfd pfd;
    int listen_fd;

    if (powerd_init() < 0)
        return 1;

    listen_fd = powerd_socket_fd;
    if (listen_fd < 0) {
        syslog(LOG_ERR, "No control socket available");
        powerd_shutdown();
        return 1;
    }

    pfd.fd = listen_fd;
    pfd.events = POLLIN;

    while (powerd_running) {
        poll(&pfd, 1, 5000); /* 5 second intervals */

        powerd_check_battery();

        /* Check thermal every 30 seconds */
        static int thermal_counter = 0;
        thermal_counter++;
        if (thermal_counter >= 6) {
            powerd_thermal_throttle(75000); /* 75C limit */
            thermal_counter = 0;
        }
    }

    powerd_shutdown();
    return 0;
}