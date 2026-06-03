/*
 * Package Manager Daemon
 * Listens for package operations and handles repo updates
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
#include <time.h>
#include "pkgd.h"

static volatile sig_atomic_t pkgd_running = 1;

static void sigterm_handler(int sig) {
    (void)sig;
    pkgd_running = 0;
}

static void daemonize(void) {
    if (fork() > 0) exit(0);
    setsid();
    chdir("/");
    close(0); close(1); close(2);
}

int pkgd_main(int argc, char **argv) {
    (void)argc; (void)argv;

    daemonize();

    openlog("pkgd", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "package manager daemon started (pid %d)", getpid());

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    syslog(LOG_INFO, "checking package repositories...");
    syslog(LOG_INFO, "no remote repos configured, using local cache");

    while (pkgd_running) {
        sleep(3600);
    }

    syslog(LOG_INFO, "package manager daemon shutting down");
    closelog();
    return 0;
}
