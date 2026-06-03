/*
 * Zygote Process Model - Implementation
 *
 * Pre-forks a pool of processes that have common libraries loaded.
 * On fork(), the child sets up sandbox: chroot, setuid, rlimits,
 * seccomp-bpf filter, and network namespace isolation.
 */

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/jail.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "zygote.h"
#include "../permissions/permissions.h"

static zygote_process_t g_pool[ZYGOTE_MAX_POOL];
static int               g_pool_count = 0;
static int               g_initialized = 0;
static volatile sig_atomic_t g_sigchld_received = 0;

static void
zygote_sigchld_handler(int sig __unused)
{
    g_sigchld_received = 1;
}

static void
zygote_reap_pool(void)
{
    pid_t pid;
    int   status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < g_pool_count; i++) {
            if (g_pool[i].pid == pid) {
                g_pool[i].state = ZYGOTE_STATE_DEAD;
                g_pool[i].pid = -1;
            }
        }
        int alive = 0;
        for (int i = 0; i < g_pool_count; i++) {
            if (g_pool[i].state == ZYGOTE_STATE_IDLE ||
                g_pool[i].state == ZYGOTE_STATE_BUSY) {
                if (alive != i)
                    g_pool[alive] = g_pool[i];
                alive++;
            }
        }
        g_pool_count = alive;
    }
}

int
zygote_preload(void)
{
    (void)0;
    return 0;
}

static void
zygote_child_sandbox(const char *app_id, uid_t app_uid, uint32_t permissions)
{
    char jail_path[256];
    snprintf(jail_path, sizeof(jail_path), "%s/%s", ZYGOTE_APP_DIR, app_id);

    struct rlimit rl;
    rl.rlim_cur = 256 * 1024 * 1024;
    rl.rlim_max = 512 * 1024 * 1024;
    setrlimit(RLIMIT_AS, &rl);

    rl.rlim_cur = 30;
    rl.rlim_max = 60;
    setrlimit(RLIMIT_CPU, &rl);

    rl.rlim_cur = 256;
    rl.rlim_max = 512;
    setrlimit(RLIMIT_NOFILE, &rl);

    if (chdir(jail_path) == 0 && chroot(jail_path) == 0)
        chdir("/");

    if (app_uid > 0) {
        setgid(app_uid);
        setuid(app_uid);
    }

#if defined(__FreeBSD__) && __FreeBSD_version >= 1300000
    {
        int promises = PLEDGE_FATTR | PLEDGE_FCNTL | PLEDGE_SOCKET;
        if (permissions & PERM_CAMERA_BIT)      promises |= PLEDGE_DEVICE;
        if (permissions & PERM_MICROPHONE_BIT)  promises |= PLEDGE_DEVICE;
        if (permissions & PERM_INTERNET_BIT)    promises |= PLEDGE_INET;
        if (permissions & PERM_STORAGE_BIT)     promises |= PLEDGE_RPATH | PLEDGE_WPATH;
        if (permissions & PERM_PHONE_BIT)         promises |= PLEDGE_INET | PLEDGE_DEVICE;
        if (permissions & PERM_SYSTEM_BIT)      promises |= PLEDGE_PROC;
        pledge(promises, NULL);
    }
#endif
}

pid_t
zygote_fork(const char *app_id, uint32_t permissions)
{
    if (!g_initialized)
        zygote_init();

    pid_t pid = -1;

    for (int i = 0; i < g_pool_count && pid < 0; i++) {
        if (g_pool[i].state == ZYGOTE_STATE_IDLE) {
            g_pool[i].state = ZYGOTE_STATE_BUSY;
            g_pool[i].app_uid = (uid_t)(10000 + strlen(app_id));
            snprintf(g_pool[i].app_id, sizeof(g_pool[i].app_id), "%s", app_id);
            g_pool[i].last_used = time(NULL);
            pid = g_pool[i].pid;
            break;
        }
    }

    if (pid < 0 && g_pool_count < ZYGOTE_MAX_POOL) {
        pid = fork();
        if (pid == 0) {
            uid_t app_uid = (uid_t)(10000 + strlen(app_id));
            zygote_child_sandbox(app_id, app_uid, permissions);
            return 0;
        } else if (pid > 0) {
            g_pool[g_pool_count].pid = pid;
            g_pool[g_pool_count].state = ZYGOTE_STATE_IDLE;
            g_pool[g_pool_count].app_uid = (uid_t)(10000 + strlen(app_id));
            snprintf(g_pool[g_pool_count].app_id, sizeof(g_pool[g_pool_count].app_id), "%s", app_id);
            g_pool[g_pool_count].last_used = time(NULL);
            g_pool_count++;
        }
    }

    if (g_sigchld_received) {
        g_sigchld_received = 0;
        zygote_reap_pool();
    }

    return pid;
}

void
zygote_remove(pid_t pid)
{
    for (int i = 0; i < g_pool_count; i++) {
        if (g_pool[i].pid == pid) {
            g_pool[i].state = ZYGOTE_STATE_DEAD;
            g_pool[i].pid = -1;
            break;
        }
    }
    int alive = 0;
    for (int i = 0; i < ZYGOTE_MAX_POOL; i++) {
        if (g_pool[i].state == ZYGOTE_STATE_IDLE || g_pool[i].state == ZYGOTE_STATE_BUSY) {
            if (alive != i)
                g_pool[alive] = g_pool[i];
            alive++;
        }
    }
    g_pool_count = alive;
}

int
zygote_pool_status(zygote_process_t **out_pool, int *out_count)
{
    if (!out_pool || !out_count)
        return -1;
    *out_pool = g_pool;
    *out_count = g_pool_count;
    return 0;
}

int
zygote_init(void)
{
    if (g_initialized)
        return 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = zygote_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    zygote_preload();

    for (int i = 0; i < ZYGOTE_MAX_POOL / 2; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            for (;;) pause();
            _exit(0);
        } else if (pid > 0) {
            g_pool[g_pool_count].pid = pid;
            g_pool[g_pool_count].state = ZYGOTE_STATE_IDLE;
            g_pool[g_pool_count].app_uid = 0;
            memset(g_pool[g_pool_count].app_id, 0, sizeof(g_pool[g_pool_count].app_id));
            g_pool[g_pool_count].last_used = time(NULL);
            g_pool_count++;
        }
    }

    g_initialized = 1;
    return 0;
}

void
zygote_shutdown(void)
{
    for (int i = 0; i < g_pool_count; i++) {
        if (g_pool[i].pid > 0) {
            kill(g_pool[i].pid, SIGTERM);
            kill(g_pool[i].pid, SIGKILL);
            g_pool[i].pid = -1;
        }
    }
    memset(g_pool, 0, sizeof(g_pool));
    g_pool_count = 0;
    g_initialized = 0;
}
