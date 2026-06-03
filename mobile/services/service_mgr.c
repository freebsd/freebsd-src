/*
 * Service Manager Library - Implementation
 * BSD-style daemon service management
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "service_mgr.h"

static service_t services[SVC_MAX_SERVICES];
static int service_count = 0;
static int svc_socket_fd = -1;
static volatile sig_atomic_t svc_sigchld_received = 0;

static void
svc_sigchld_handler(int sig __unused)
{
    svc_sigchld_received = 1;
}

static void
svc_reap_zombies(void)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < service_count; i++) {
            if (services[i].pid == pid && services[i].state == SVC_STATE_RUNNING) {
                services[i].state = SVC_STATE_STOPPED;
                services[i].pid = -1;
                syslog(LOG_WARNING, "Service %s (pid %d) died unexpectedly",
                       services[i].name, pid);
            }
        }
    }
}

static service_t *
svc_find(const char *name)
{
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0)
            return &services[i];
    }
    return NULL;
}

static int
svc_spawn(service_t *svc)
{
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        /* Child process */
        setsid();
        execv(svc->cmd, svc->argv);
        syslog(LOG_ERR, "Failed to exec service '%s': %s", svc->cmd, strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        svc->pid = pid;
        svc->state = SVC_STATE_RUNNING;
        svc->restart_count = 0;
        syslog(LOG_INFO, "Spawned service '%s' with pid %d", svc->name, pid);
        return 0;
    }
    return -1;
}

int
svc_register(const char *name, const char *cmd, int argc, char **argv)
{
    service_t *svc;

    if (service_count >= SVC_MAX_SERVICES) {
        syslog(LOG_ERR, "Maximum services reached");
        return -1;
    }

    svc = &services[service_count];
    strncpy(svc->name, name, sizeof(svc->name) - 1);
    svc->name[sizeof(svc->name) - 1] = '\0';

    strncpy(svc->cmd, cmd, sizeof(svc->cmd) - 1);
    svc->cmd[sizeof(svc->cmd) - 1] = '\0';

    svc->argv[0] = svc->cmd;
    for (int i = 0; i < argc && i < SVC_MAX_CMDARGS - 1; i++) {
        svc->argv[i + 1] = argv[i];
    }
    svc->argv[argc + 1] = NULL;

    svc->pid = -1;
    svc->state = SVC_STATE_STOPPED;
    svc->restart_count = 0;
    svc->restart_max = 3;
    svc->restart_delay = 5;
    svc->dep_count = 0;
    svc->enabled = 1;

    service_count++;
    syslog(LOG_INFO, "Registered service '%s'", name);
    return 0;
}

int
svc_start(const char *name)
{
    service_t *svc;

    svc = svc_find(name);
    if (!svc) {
        syslog(LOG_ERR, "Service '%s' not found", name);
        return -1;
    }

    /* Check dependencies */
    for (int i = 0; i < svc->dep_count; i++) {
        if (svc->depends_on[i]->state != SVC_STATE_RUNNING) {
            if (svc_start(svc->depends_on[i]->name) < 0) {
                syslog(LOG_ERR, "Failed to start dependency '%s' for '%s'",
                       svc->depends_on[i]->name, name);
                return -1;
            }
        }
    }

    if (svc->state == SVC_STATE_RUNNING) {
        return 0;
    }

    svc->state = SVC_STATE_STARTING;
    return svc_spawn(svc);
}

int
svc_stop(const char *name)
{
    service_t *svc;

    svc = svc_find(name);
    if (!svc) {
        syslog(LOG_ERR, "Service '%s' not found", name);
        return -1;
    }

    if (svc->state != SVC_STATE_RUNNING) {
        return 0;
    }

    svc->state = SVC_STATE_STOPPING;
    if (svc->pid > 0) {
        kill(svc->pid, SIGTERM);
        sleep(1);
        kill(svc->pid, SIGKILL);
    }
    svc->state = SVC_STATE_STOPPED;
    svc->pid = -1;
    return 0;
}

int
svc_restart(const char *name)
{
    svc_stop(name);
    return svc_start(name);
}

int
svc_status(const char *name, int *state, pid_t *pid)
{
    service_t *svc;

    svc = svc_find(name);
    if (!svc) {
        return -1;
    }

    if (state)
        *state = svc->state;
    if (pid)
        *pid = svc->pid;
    return 0;
}

int
svc_list(int (*callback)(const char *name, int state, pid_t pid, void *arg), void *arg)
{
    for (int i = 0; i < service_count; i++) {
        if (callback(services[i].name, services[i].state, services[i].pid, arg) < 0)
            return -1;
    }
    return 0;
}

int
svc_init(const char *socket_path)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("service_mgr", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = svc_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    svc_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (svc_socket_fd == -1) {
        syslog(LOG_ERR, "Cannot create service socket: %s", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    unlink(socket_path);
    if (bind(svc_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        syslog(LOG_ERR, "Cannot bind service socket: %s", strerror(errno));
        close(svc_socket_fd);
        return -1;
    }

    if (listen(svc_socket_fd, 16) == -1) {
        syslog(LOG_ERR, "Cannot listen on service socket: %s", strerror(errno));
        close(svc_socket_fd);
        return -1;
    }

    syslog(LOG_INFO, "Service manager initialized");
    return 0;
}

void
svc_shutdown(void)
{
    for (int i = 0; i < service_count; i++) {
        if (services[i].state == SVC_STATE_RUNNING) {
            kill(services[i].pid, SIGTERM);
        }
    }

    if (svc_socket_fd >= 0) {
        close(svc_socket_fd);
        unlink(SVC_SOCKET_PATH);
    }

    closelog();
}

void
svc_watchdog_check(void)
{
    if (svc_sigchld_received) {
        svc_sigchld_received = 0;
        svc_reap_zombies();

        for (int i = 0; i < service_count; i++) {
            if (services[i].state == SVC_STATE_STOPPED && services[i].pid == -1 &&
                services[i].enabled && services[i].restart_count < services[i].restart_max) {
                syslog(LOG_INFO, "Restarting crashed service '%s'", services[i].name);
                sleep(services[i].restart_delay);
                services[i].restart_count++;
                svc_spawn(&services[i]);
            }
        }
    }
}