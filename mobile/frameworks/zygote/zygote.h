/*
 * Zygote Process Model
 * Pre-forks pool of app processes for fast startup
 * Sets up sandbox (chroot, setuid, rlimits, seccomp, netns) in child
 */

#ifndef _ZYGOTE_H_
#define _ZYGOTE_H_

#include <sys/types.h>
#include <stdlib.h>

#define ZYGOTE_MAX_POOL      8
#define ZYGOTE_APP_DIR       "/mobile/apps"
#define ZYGOTE_LIB_PRELOAD   "/mobile/lib/zygote_preload.so"
#define ZYGOTE_SOCKET_PATH   "/var/run/zygote.sock"

#define ZYGOTE_STATE_IDLE    0
#define ZYGOTE_STATE_BUSY    1
#define ZYGOTE_STATE_DEAD    2

typedef struct zygote_process {
    pid_t pid;
    int   state;
    uid_t app_uid;
    char  app_id[128];
    time_t last_used;
} zygote_process_t;

/* Initialize zygote: pre-fork pool, preload common libraries */
int zygote_init(void);

/* Shutdown zygote */
void zygote_shutdown(void);

/* Preload common libraries (libc, libm, libui) before forking */
int zygote_preload(void);

/* Fork a new app process from the pool */
pid_t zygote_fork(const char *app_id, uint32_t permissions);

/* Remove a process from the pool (on crash) */
void zygote_remove(pid_t pid);

/* Get pool status */
int zygote_pool_status(zygote_process_t **out_pool, int *out_count);

#endif /* _ZYGOTE_H_ */
