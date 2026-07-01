/*
 * Mobile OS Init Process - PID 1
 * BSD-style init implementation
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "init.h"
#include "service_mgr.h"

static struct service_entry *service_list;
static int service_count;
static volatile sig_atomic_t init_sigchld_received = 0;

static void
sigchld_handler(int sig __unused)
{
    init_sigchld_received = 1;
}

static void
sigterm_handler(int sig __unused)
{
    syslog(LOG_INFO, "Received SIGTERM, shutting down services");
}

static void
sigint_handler(int sig __unused)
{
    syslog(LOG_INFO, "Received SIGINT, shutting down services");
}

static void
setup_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

void
init_setup_mounts(void)
{
    int ret;

    openlog("init", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Setting up early filesystems");

    ret = mount("procfs", "/proc", MNT_RDONLY, NULL);
    if (ret == -1 && errno != EBUSY)
        syslog(LOG_WARNING, "Failed to mount /proc: %s", strerror(errno));

    ret = mount("devfs", "/dev", MNT_RDONLY, NULL);
    if (ret == -1 && errno != EBUSY)
        syslog(LOG_WARNING, "Failed to mount /dev: %s", strerror(errno));

    ret = mount("tmpfs", "/run", MNT_RDWR, NULL);
    if (ret == -1 && errno != EBUSY)
        syslog(LOG_WARNING, "Failed to mount /run: %s", strerror(errno));
}

int
init_parse_rc_conf(const char *path, struct service_entry **services, int *count)
{
    int fd;
    char line[512];
    int svc_count = 0;
    struct service_entry *svc_list = NULL;
    char tmp_cmd[256];
    char tmp_name[64];

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        syslog(LOG_WARNING, "Cannot read %s: %s", path, strerror(errno));
        *services = NULL;
        *count = 0;
        return 0;
    }

    while (read(fd, line, sizeof(line)) > 0) {
        int n = sscanf(line, "service_%63s=\"%255[^\"]\"", tmp_name, tmp_cmd);
        if (n == 2) {
            svc_list = realloc(svc_list, sizeof(struct service_entry) * (svc_count + 1));
            if (svc_list) {
                strncpy(svc_list[svc_count].name, tmp_name, sizeof(svc_list[svc_count].name) - 1);
                svc_list[svc_count].name[sizeof(svc_list[svc_count].name) - 1] = '\0';
                strncpy(svc_list[svc_count].cmd, tmp_cmd, sizeof(svc_list[svc_count].cmd) - 1);
                svc_list[svc_count].cmd[sizeof(svc_list[svc_count].cmd) - 1] = '\0';
                svc_list[svc_count].enabled = 1;
                svc_list[svc_count].pid = -1;
                svc_count++;
            }
        }
    }
    close(fd);

    *services = svc_list;
    *count = svc_count;
    return svc_count;
}

void
init_spawn_service(const char *cmd)
{
    pid_t pid;
    char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };

    pid = fork();
    if (pid == 0) {
        /* Child process */
        setsid();
        execv(argv[0], argv);
        syslog(LOG_ERR, "Failed to exec service '%s': %s", cmd, strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        syslog(LOG_INFO, "Started service '%s' (pid %d)", cmd, pid);
    } else {
        syslog(LOG_ERR, "Failed to fork service '%s': %s", cmd, strerror(errno));
    }
}

void
init_spawn_getty(const char *tty)
{
    pid_t pid;
    char tty_path[32];
    char *argv[] = { INIT_GETTY_CMD, tty, NULL };

    snprintf(tty_path, sizeof(tty_path), "/dev/%s", tty);
    
    pid = fork();
    if (pid == 0) {
        /* Child process */
        setsid();
        ioctl(open(tty_path, O_RDWR), TIOCSCTTY, 0);
        execv(INIT_GETTY_CMD, argv);
        syslog(LOG_ERR, "Failed to exec getty: %s", strerror(errno));
        _exit(127);
    } else if (pid > 0) {
        syslog(LOG_INFO, "Started getty on %s (pid %d)", tty_path, pid);
    } else {
        syslog(LOG_ERR, "Failed to fork getty: %s", strerror(errno));
    }
}

void
init_reboot(unsigned int flags)
{
    syslog(LOG_INFO, "Rebooting system (flags=0x%x)", flags);
    /* Send SIGTERM to all children */
    kill(-1, SIGTERM);
    sleep(2);
    kill(-1, SIGKILL);
    /* Issue reboot syscall */
    reboot(RB_AUTOBOOT | flags);
}

void
init_halt(void)
{
    syslog(LOG_INFO, "Halting system");
    /* Send SIGTERM to all children */
    kill(-1, SIGTERM);
    sleep(2);
    kill(-1, SIGKILL);
    /* Issue halt syscall */
    reboot(RB_HALT_SYSTEM);
}

static void
init_import_rc_to_service_mgr(void)
{
    char line[512];
    char name[64];
    char cmd[256];
    int fd;

    fd = open(INIT_RC_CONF_PATH, O_RDONLY);
    if (fd == -1) {
        syslog(LOG_WARNING, "Cannot read %s: %s", INIT_RC_CONF_PATH, strerror(errno));
        return;
    }

    while (read(fd, line, sizeof(line)) > 0) {
        int n = sscanf(line, "service_%63s=\"%255[^\"]\"", name, cmd);
        if (n == 2) {
            char *argv[] = { cmd };
            svc_register(name, cmd, 1, argv);
        }
    }
    close(fd);
}

int
init_main(int argc __unused, char **argv __unused)
{
    pid_t child_pid;
    int status;

    init_setup_mounts();
    setup_signal_handlers();

    openlog("init", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Mobile OS init starting");

    init_import_rc_to_service_mgr();
    svc_resolve_all_dependencies();
    svc_detect_cycles();

    syslog(LOG_INFO, "Starting all services in dependency order");
    svc_start_all();

    /* Start getty on ttyv0 */
    init_spawn_getty(INIT_GETTY_TTY);

    /* Main loop: wait for children */
    while (1) {
        child_pid = waitpid(-1, &status, WUNTRACED);
        if (child_pid > 0) {
            if (WIFEXITED(status)) {
                syslog(LOG_INFO, "Child %d exited with status %d", child_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                syslog(LOG_WARNING, "Child %d killed by signal %d", child_pid, WTERMSIG(status));
            }
        }

        if (init_sigchld_received) {
            init_sigchld_received = 0;
            while (waitpid(-1, NULL, WNOHANG) > 0)
                ;
        }

        svc_watchdog_check();
        pause();
    }

    return 0;
}