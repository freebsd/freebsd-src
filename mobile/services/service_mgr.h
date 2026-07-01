/*
 * Service Manager Library
 * BSD-style daemon service management
 */

#ifndef _SERVICE_MGR_H_
#define _SERVICE_MGR_H_

#include <sys/types.h>

#define SVC_PATH_MAX      256
#define SVC_NAME_MAX      64
#define SVC_MAX_CMDARGS   32
#define SVC_SOCKET_PATH   "/var/run/service.sock"
#define SVC_MAX_SERVICES  128
#define SVC_DEPENDENCY_MAX 16

#define SVC_STATE_STOPPED     0
#define SVC_STATE_STARTING    1
#define SVC_STATE_RUNNING     2
#define SVC_STATE_STOPPING    3
#define SVC_STATE_FAILED      4
#define SVC_STATE_WAITING     5

#define SVC_CMD_START    'S'
#define SVC_CMD_STOP     'T'
#define SVC_CMD_RESTART  'R'
#define SVC_CMD_STATUS   's'
#define SVC_CMD_LIST     'L'

typedef struct service {
    char name[SVC_NAME_MAX];
    pid_t pid;
    int state;
    int restart_count;
    int restart_max;
    int restart_delay; /* seconds */
    char cmd[SVC_PATH_MAX];
    char *argv[SVC_MAX_CMDARGS];
    struct service *depends_on[SVC_DEPENDENCY_MAX];
    char dep_names[SVC_DEPENDENCY_MAX][SVC_NAME_MAX];
    int dep_count;
    int enabled;
    int svc_visit_mark;
} service_t;

/* Register a service with the manager */
int svc_register(const char *name, const char *cmd, int argc, char **argv);

/* Start a service */
int svc_start(const char *name);

/* Stop a service */
int svc_stop(const char *name);

/* Restart a service */
int svc_restart(const char *name);

/* Get service status */
int svc_status(const char *name, int *state, pid_t *pid);

/* List all services */
int svc_list(int (*callback)(const char *name, int state, pid_t pid, void *arg), void *arg);

/* Initialize service manager */
int svc_init(const char *socket_path);

/* Shutdown service manager */
void svc_shutdown(void);

/* Watchdog check - restart crashed services */
void svc_watchdog_check(void);

/* Dependency management */
int svc_add_dependency(const char *service, const char *depends_on);
int svc_resolve_all_dependencies(void);
int svc_detect_cycles(void);

/* Bootstrap: start/stop all enabled services in dependency order */
int svc_start_all(void);
int svc_stop_all(void);

#endif /* _SERVICE_MGR_H_ */