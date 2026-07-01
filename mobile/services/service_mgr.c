/*
 * Service Manager Library - Implementation
 * BSD-style daemon service management
 * Extended: name-based dependency resolution, dependency DFS, cycle detection,
 *           wait-state enforcement, start-all ordering, and argv null termination.
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
            if (services[i].pid == pid) {
                services[i].pid = -1;
                if (services[i].state == SVC_STATE_RUNNING) {
                    services[i].state = SVC_STATE_STOPPED;
                    syslog(LOG_WARNING, "Service %s (pid %d) died unexpectedly",
                           services[i].name, pid);

                    if (services[i].restart_count < services[i].restart_max) {
                        services[i].restart_count++;
                        syslog(LOG_INFO,
                               "Watchdog: restarting service '%s' (attempt %d/%d)",
                               services[i].name,
                               services[i].restart_count,
                               services[i].restart_max);
                        services[i].state = SVC_STATE_STARTING;
                        svc_spawn(&services[i]);
                    } else {
                        services[i].state = SVC_STATE_FAILED;
                        syslog(LOG_ERR,
                               "Service '%s' exceeded restart limit (%d). Marking failed.",
                               services[i].name,
                               services[i].restart_max);
                    }
                } else if (services[i].state == SVC_STATE_STOPPING) {
                    services[i].state = SVC_STATE_STOPPED;
                    syslog(LOG_INFO, "Service %s stopped (pid %d)",
                           services[i].name, pid);
                }
            }
        }
    }
}

static int svc_bind_dependency(service_t *svc, const char *dep_name);
static int svc_cycle_from(int svc_index, int *visited, int *stack);

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

static int
svc_bind_dependency(service_t *svc, const char *dep_name)
{
    if (svc->dep_count >= SVC_DEPENDENCY_MAX)
        return -1;
    service_t *dep = svc_find(dep_name);
    if (!dep) {
        syslog(LOG_WARNING, "Service '%s' dependency '%s' not registered yet (will resolve later)",
               svc->name, dep_name);
        strncpy(svc->dep_names[svc->dep_count], dep_name, sizeof(svc->dep_names[0]) - 1);
        svc->dep_names[svc->dep_count][sizeof(svc->dep_names[0]) - 1] = '\0';
        svc->depends_on[svc->dep_count] = NULL;
        svc->dep_count++;
        return 0;
    }
    svc->depends_on[svc->dep_count++] = dep;
    return 0;
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
    memset(svc, 0, sizeof(*svc));
    strncpy(svc->name, name, sizeof(svc->name) - 1);
    svc->name[sizeof(svc->name) - 1] = '\0';

    strncpy(svc->cmd, cmd, sizeof(svc->cmd) - 1);
    svc->cmd[sizeof(svc->cmd) - 1] = '\0';

    svc->argv[0] = svc->cmd;
    for (int i = 0; i < argc && i < SVC_MAX_CMDARGS - 2; i++) {
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
svc_add_dependency(const char *service, const char *depends_on)
{
    service_t *svc = svc_find(service);
    if (!svc) {
        syslog(LOG_ERR, "Cannot add dependency: service '%s' not found", service);
        return -1;
    }
    return svc_bind_dependency(svc, depends_on);
}

int
svc_resolve_all_dependencies(void)
{
    int resolved = 0;
    for (int i = 0; i < service_count; i++) {
        service_t *svc = &services[i];
        for (int d = 0; d < svc->dep_count; d++) {
            if (!svc->depends_on[d] && svc->dep_names[d][0]) {
                svc->depends_on[d] = svc_find(svc->dep_names[d]);
                if (svc->depends_on[d]) {
                    svc->dep_names[d][0] = '\0';
                    resolved++;
                } else {
                    syslog(LOG_WARNING, "Dependency '%s' for service '%s' unresolved",
                           svc->dep_names[d], svc->name);
                }
            }
        }
    }
    syslog(LOG_INFO, "Resolved %d dependency references", resolved);
    return resolved;
}

static int
svc_cycle_from(int svc_index, int *visited, int *stack)
{
    if (stack[svc_index])
        return 1;
    if (visited[svc_index])
        return 0;

    visited[svc_index] = 1;
    stack[svc_index] = 1;

    for (int d = 0; d < services[svc_index].dep_count; d++) {
        service_t *dep = services[svc_index].depends_on[d];
        if (!dep)
            continue;
        int dep_index = (int)(dep - services);
        if (svc_cycle_from(dep_index, visited, stack))
            return 1;
    }

    stack[svc_index] = 0;
    return 0;
}

int
svc_detect_cycles(void)
{
    int visited[SVC_MAX_SERVICES];
    int stack[SVC_MAX_SERVICES];
    memset(visited, 0, sizeof(visited));
    memset(stack, 0, sizeof(stack));

    for (int i = 0; i < service_count; i++) {
        if (svc_cycle_from(i, visited, stack)) {
            syslog(LOG_ERR, "Dependency cycle detected involving service '%s'",
                   services[i].name);
            return -1;
        }
    }
    syslog(LOG_INFO, "No dependency cycles detected");
    return 0;
}

static int
svc_start(const char *name)
{
    service_t *svc;

    svc = svc_find(name);
    if (!svc) {
        syslog(LOG_ERR, "Service '%s' not found", name);
        return -1;
    }

    if (svc->state == SVC_STATE_STARTING || svc->state == SVC_STATE_RUNNING)
        return 0;

    svc_resolve_all_dependencies();

    /* If deps are missing or still unstarted, enter waiting. */
    int blocked = 0;
    for (int i = 0; i < svc->dep_count; i++) {
        service_t *dep = svc->depends_on[i];
        if (!dep) {
            blocked = 1;
            break;
        }
        if (dep->state != SVC_STATE_RUNNING) {
            if (svc_start(dep->name) < 0) {
                blocked = 1;
                break;
            }
        }
    }

    if (blocked) {
        if (svc->state != SVC_STATE_WAITING) {
            syslog(LOG_INFO, "Service '%s' waiting for dependencies", svc->name);
            svc->state = SVC_STATE_WAITING;
        }
        return -1;
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
    }
}

static int
svc_visit_clear(void)
{
    for (int i = 0; i < service_count; i++)
        services[i].svc_visit_mark = 0;
    return 0;
}

static int
svc_topological_from(int svc_index, int *order, int *order_count)
{
    service_t *svc = &services[svc_index];

    if (svc->svc_visit_mark == 1) {
        syslog(LOG_ERR, "Dependency cycle detected (topo sort) involving '%s'",
               svc->name);
        return -1;
    }
    if (svc->svc_visit_mark == 2)
        return 0;

    svc->svc_visit_mark = 1;
    for (int d = 0; d < svc->dep_count; d++) {
        service_t *dep = svc->depends_on[d];
        if (!dep)
            continue;
        int dep_index = (int)(dep - services);
        if (svc_topological_from(dep_index, order, order_count) < 0)
            return -1;
    }
    svc->svc_visit_mark = 2;
    order[(*order_count)++] = svc_index;
    return 0;
}

int
svc_build_start_order(int *order_out, int *order_count)
{
    int order[SVC_MAX_SERVICES];
    int count = 0;

    for (int i = 0; i < service_count; i++) {
        services[i].svc_visit_mark = 0;
    }

    for (int i = 0; i < service_count; i++) {
        if (services[i].svc_visit_mark == 0) {
            if (svc_topological_from(i, order, &count) < 0) {
                svc_visit_clear();
                return -1;
            }
        }
    }

    for (int i = 0; i < count; i++) {
        order_out[i] = order[i];
    }
    *order_count = count;
    svc_visit_clear();
    return 0;
}

int
svc_start_all(void)
{
    int order[SVC_MAX_SERVICES];
    int count = 0;

    if (svc_build_start_order(order, &count) < 0) {
        syslog(LOG_ERR, "Cannot start services: dependency cycle detected");
        return -1;
    }

    syslog(LOG_INFO, "Starting %d services in dependency order", count);
    for (int i = 0; i < count; i++) {
        service_t *svc = &services[order[i]];
        if (!svc->enabled)
            continue;
        if (svc->state == SVC_STATE_RUNNING)
            continue;
        char *argv[] = { svc->cmd };
        svc_spawn(svc);
    }
    return 0;
}

int
svc_stop_all(void)
{
    int order[SVC_MAX_SERVICES];
    int count = 0;

    if (svc_build_start_order(order, &count) < 0) {
        syslog(LOG_WARNING, "Dependency cycle detected; stopping in reverse registration order");
        for (int i = service_count - 1; i >= 0; i--)
            order[count++] = i;
    }

    for (int i = count - 1; i >= 0; i--) {
        service_t *svc = &services[order[i]];
        if (svc->state == SVC_STATE_RUNNING)
            svc_stop(svc->name);
    }
    return 0;
}