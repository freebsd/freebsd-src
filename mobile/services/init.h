/*
 * Mobile OS Init Process
 * PID 1 - System initialization and service management
 */

#ifndef _MOBILE_INIT_H_
#define _MOBILE_INIT_H_

#include <sys/types.h>

#define INIT_RC_CONF_PATH   "/etc/rc.conf"
#define INIT_GETTY_CMD     "/usr/sbin/getty"
#define INIT_GETTY_TTY     "ttyv0"

struct service_entry {
    char name[64];
    char cmd[256];
    pid_t pid;
    int enabled;
};

extern volatile sig_atomic_t init_sigchld_received;

int init_main(int argc, char **argv);
void init_reboot(unsigned int flags);
void init_halt(void);
int init_parse_rc_conf(const char *path, struct service_entry **services, int *count);
void init_spawn_service(const char *cmd);
void init_spawn_getty(const char *tty);
void init_setup_mounts(void);

#endif /* _MOBILE_INIT_H_ */