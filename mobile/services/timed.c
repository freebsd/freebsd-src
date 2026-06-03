/*
 * Time Daemon - Implementation
 * BSD-style time management service
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
#include <time.h>
#include "timed.h"

static int timed_running = 0;
static int timed_socket_fd = -1;
static int ntp_state = NTP_STATE_DISCONNECTED;
static struct timezone_info current_tz;
static struct ntp_server ntp_servers[4];
static int ntp_server_count = 0;

static void
timed_sigterm_handler(int sig __unused)
{
    timed_running = 0;
}

static int
timed_read_ntp_servers(void)
{
    int fd;
    char line[128];
    int count = 0;

    fd = open("/etc/ntp.conf", O_RDONLY);
    if (fd == -1) {
        /* Default servers if no config */
        strncpy(ntp_servers[0].hostname, "pool.ntp.org", sizeof(ntp_servers[0].hostname) - 1);
        ntp_servers[0].port = 123;
        ntp_servers[0].reachable = 1;
        ntp_server_count = 1;
        return 1;
    }

    while (read(fd, line, sizeof(line)) > 0 && count < 4) {
        char *server;
        int port;

        server = strtok(line, " \t\n");
        if (server && strncmp(server, "server", 6) == 0) {
            server = strtok(NULL, " \t\n");
            if (server) {
                strncpy(ntp_servers[count].hostname, server, sizeof(ntp_servers[count].hostname) - 1);
                ntp_servers[count].port = 123;
                ntp_servers[count].reachable = 1;
                ntp_servers[count].stratum = 0;
                count++;
            }
        }
    }
    close(fd);

    ntp_server_count = count;
    return count;
}

int
timed_ntp_sync(const char *server)
{
    pid_t pid;
    char *argv[] = { "/usr/sbin/ntpdate", "-s", (char *)server, NULL };

    ntp_state = NTP_STATE_SYNCING;
    syslog(LOG_INFO, "Syncing time with NTP server %s", server);

    pid = fork();
    if (pid == 0) {
        execv(argv[0], argv);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            ntp_state = NTP_STATE_SYNCED;
            syslog(LOG_INFO, "Time synchronized successfully");
            return 0;
        }
    }

    ntp_state = NTP_STATE_ERROR;
    return -1;
}

int
timed_set_timezone(const char *tz_name)
{
    int fd;
    char buf[256];

    strncpy(current_tz.name, tz_name, sizeof(current_tz.name) - 1);
    current_tz.offset_minutes = 0; /* Would parse TZ string */
    current_tz.dst_active = 0;

    snprintf(buf, sizeof(buf), "TZ=%s", tz_name);
    setenv("TZ", tz_name, 1);
    tzset();

    syslog(LOG_INFO, "Timezone set to %s", tz_name);
    return 0;
}

int
timed_get_timezone(struct timezone_info *tz)
{
    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = localtime(&now);

    if (tz) {
        tz->offset_minutes = tm_info->tm_gmtoff / 60;
        strncpy(tz->name, getenv("TZ") ? getenv("TZ") : "UTC", sizeof(tz->name) - 1);
    }
    return 0;
}

int
timed_set_rtc_alarm(const struct tm *alarm_time)
{
    int fd;
    char buf[64];

    fd = open("/dev/rtc0", O_WRONLY);
    if (fd == -1) {
        /* Try sysfs interface */
        fd = open("/sys/class/rtc/rtc0/alarm", O_WRONLY);
    }

    if (fd >= 0) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", alarm_time);
        write(fd, buf, strlen(buf));
        close(fd);
        syslog(LOG_INFO, "RTC alarm set for %s", buf);
        return 0;
    }

    syslog(LOG_WARNING, "Cannot set RTC alarm");
    return -1;
}

int
timed_cancel_rtc_alarm(void)
{
    int fd;

    fd = open("/sys/class/rtc/rtc0/alarm", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
        syslog(LOG_INFO, "RTC alarm cancelled");
        return 0;
    }
    return -1;
}

int
timed_init(void)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("timed", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timed_sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Read NTP servers */
    timed_read_ntp_servers();

    /* Initialize timezone */
    tzset();
    strncpy(current_tz.name, getenv("TZ") ? getenv("TZ") : "UTC", sizeof(current_tz.name) - 1);

    /* Create control socket */
    timed_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (timed_socket_fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TIMED_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(TIMED_SOCKET_PATH);
        if (bind(timed_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(timed_socket_fd);
            timed_socket_fd = -1;
        }
    }

    /* Perform initial time sync */
    if (ntp_server_count > 0) {
        timed_ntp_sync(ntp_servers[0].hostname);
    }

    timed_running = 1;
    syslog(LOG_INFO, "Time daemon initialized");
    return 0;
}

void
timed_shutdown(void)
{
    timed_running = 0;

    if (timed_socket_fd >= 0) {
        close(timed_socket_fd);
        unlink(TIMED_SOCKET_PATH);
    }

    closelog();
}

int
timed_main(int argc __unused, char **argv __unused)
{
    time_t last_sync = 0;
    struct pollfd pfd;

    if (timed_init() < 0)
        return 1;

    pfd.fd = timed_socket_fd;
    pfd.events = POLLIN;

    while (timed_running) {
        poll(&pfd, 1, 60000); /* 1 minute intervals */

        /* Periodic NTP sync (every hour) */
        time_t now;
        time(&now);
        if (now - last_sync > 3600) {
            if (ntp_server_count > 0) {
                timed_ntp_sync(ntp_servers[0].hostname);
            }
            last_sync = now;
        }
    }

    timed_shutdown();
    return 0;
}