/*
 * Sandbox/Restrict Framework
 * Uses FreeBSD jail(2) for filesystem isolation
 * Uses pledge(2)/unveil(2) for capability restrictions
 */

#ifndef _SANDBOX_H_
#define _SANDBOX_H_

#include <sys/types.h>
#include <sys/jail.h>
#include <stdlib.h>

#define SB_PATH_MAX    256
#define SB_SYSCALL_MAX 512
#define SB_APP_DIR     "/mobile/apps"
#define SB_ROOT_DIR    "/mobile/jail"
#define SB_CONFIG_PATH "/etc/sandbox.conf"

typedef enum {
    SB_READ     = 1,
    SB_WRITE    = 2,
    SB_EXECUTE  = 4,
    SB_RW       = SB_READ | SB_WRITE,
    SB_RWX      = SB_READ | SB_WRITE | SB_EXECUTE,
} sb_access_t;

typedef enum {
    SB_SOCK_STREAM = SOCK_STREAM,
    SB_SOCK_DGRAM  = SOCK_DGRAM,
    SB_SOCK_RAW    = SOCK_RAW,
    SB_SOCK_SEQPACKET = SOCK_SEQPACKET,
} sb_socket_type_t;

/* Initialize sandbox framework */
int sb_init(const char *app_id, uid_t uid);

/* Shutdown */
void sb_shutdown(void);

/* Allow path access for app */
int sb_allow_path(const char *app_id, const char *path, sb_access_t mode);

/* Deny previously allowed path */
int sb_deny_path(const char *app_id, const char *path);

/* Allow network socket access */
int sb_allow_socket(const char *app_id, int domain, sb_socket_type_t type, int protocol);

/* Allow specific syscall */
int sb_allow_syscall(const char *app_id, int syscall_nr);

/* Apply deny-all-default policy */
int sb_deny_all_by_default(const char *app_id);

/* Apply sandbox to current process */
int sb_apply(const char *app_id);

/* Apply pledge restrictions */
int sb_pledge(const char *app_id, const char *promises, const char *execpromises);

/* Apply unveil restrictions */
int sb_unveil(const char *app_id, const char *path, const char *permissions);

/* Jail-based isolation */
int sb_jail_create(const char *app_id, uid_t uid, const char *path);

/* Check if path is allowed */
int sb_path_allowed(const char *app_id, const char *path, sb_access_t mode);

#endif /* _SANDBOX_H_ */
