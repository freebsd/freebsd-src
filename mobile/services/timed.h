/*
 * Time Daemon - Time management service
 * Handles NTP, system clock, timezone, RTC alarms
 */

#ifndef _MOBILE_TIMED_H_
#define _MOBILE_TIMED_H_

#include <sys/types.h>
#include <time.h>

#define TIMED_SOCKET_PATH "/var/run/timed.sock"

#define NTP_STATE_DISCONNECTED  0
#define NTP_STATE_SYNCING       1
#define NTP_STATE_SYNCED        2
#define NTP_STATE_ERROR         3

struct timezone_info {
    char name[64];
    int offset_minutes;
    int dst_active;
};

struct ntp_server {
    char hostname[128];
    int port;
    int reachable;
    int stratum;
};

int timed_main(int argc, char **argv);
int timed_init(void);
void timed_shutdown(void);
int timed_ntp_sync(const char *server);
int timed_set_timezone(const char *tz_name);
int timed_get_timezone(struct timezone_info *tz);
int timed_set_rtc_alarm(const struct tm *alarm_time);
int timed_cancel_rtc_alarm(void);

#endif /* _MOBILE_TIMED_H_ */