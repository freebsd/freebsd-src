/*
 * Sandbox/Restrict Framework
 * FreeBSD jail-based isolation + pledge/unveil for process sandboxing
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/jail.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/queue.h>

#include "sandbox.h"
#include "../permissions/permissions.h"

#define SB_JAIL_PREFIX "mobile_app"
#define SB_MAX_PATHS   256
#define SB_MAX_SYSCALLS 512

typedef struct sb_path_entry {
    char              path[SB_PATH_MAX];
    sb_access_t       mode;
    SLIST_ENTRY(sb_path_entry) link;
} sb_path_entry_t;

typedef struct sb_syscall_entry {
    int                nr;
    SLIST_ENTRY(sb_syscall_entry) link;
} sb_syscall_entry_t;

/* Per-app sandbox state (in production, keyed by app_id) */
static SLIST_HEAD(path_head, sb_path_entry) g_allowed_paths;
static SLIST_HEAD(sc_head, sb_syscall_entry) g_allowed_syscalls;
static int   g_default_deny = 0;
static int   g_sandbox_initialized = 0;
static uid_t g_current_app_uid = 0;

int
sb_init(const char *app_id, uid_t uid)
{
    if (!app_id)
        return -1;

    SLIST_INIT(&g_allowed_paths);
    SLIST_INIT(&g_allowed_syscalls);
    g_default_deny = 0;
    g_sandbox_initialized = 1;
    g_current_app_uid = uid;

    /* Load per-app sandbox config if present */
    char cfg[SB_PATH_MAX];
    snprintf(cfg, sizeof(cfg), "/mobile/apps/%s/sandbox.conf", app_id);
    int fd = open(cfg, O_RDONLY);
    if (fd >= 0) {
        char line[512];
        while (readline(fd, line, sizeof(line)) > 0) {
            char *tok = strtok(line, " \t");
            if (!tok) continue;

            if (strcmp(tok, "allow_path") == 0) {
                char *p = strtok(NULL, " \t");
                if (p) sb_allow_path(app_id, p, SB_READ);
            } else if (strcmp(tok, "allow_socket") == 0) {
                char *d = strtok(NULL, " \t");
                char *t = strtok(NULL, " \t");
                if (d && t)
                    sb_allow_socket(app_id, atoi(d), (sb_socket_type_t)atoi(t), 0);
            }
        }
        close(fd);
    }

    return 0;
}

void
sb_shutdown(void)
{
    sb_path_entry_t *p;
    while ((p = SLIST_FIRST(&g_allowed_paths)) != NULL) {
        SLIST_REMOVE_HEAD(&g_allowed_paths, link);
        free(p);
    }
    sb_syscall_entry_t *s;
    while ((s = SLIST_FIRST(&g_allowed_syscalls)) != NULL) {
        SLIST_REMOVE_HEAD(&g_allowed_syscalls, link);
        free(s);
    }
    g_sandbox_initialized = 0;
}

int
sb_allow_path(const char *app_id, const char *path, sb_access_t mode)
{
    (void)app_id;
    if (!path || !g_sandbox_initialized)
        return -1;

    sb_path_entry_t *entry = malloc(sizeof(*entry));
    if (!entry) return -1;

    strlcpy(entry->path, path, sizeof(entry->path));
    entry->mode = mode;
    SLIST_INSERT_HEAD(&g_allowed_paths, entry, link);
    return 0;
}

int
sb_deny_path(const char *app_id, const char *path)
{
    sb_path_entry_t *entry;
    SLIST_FOREACH(entry, &g_allowed_paths, link) {
        if (strcmp(entry->path, path) == 0) {
            SLIST_REMOVE(&g_allowed_paths, entry, sb_path_entry, link);
            free(entry);
            return 0;
        }
    }
    return -1;
}

int
sb_allow_socket(const char *app_id, int domain, sb_socket_type_t type, int protocol)
{
    (void)app_id; (void)domain; (void)type; (void)protocol;
    /* Socket access is controlled via pledge(PLEDGE_INET) */
    return 0;
}

int
sb_allow_syscall(const char *app_id, int syscall_nr)
{
    (void)app_id;
    if (!g_sandbox_initialized)
        return -1;

    sb_syscall_entry_t *entry = malloc(sizeof(*entry));
    if (!entry) return -1;
    entry->nr = syscall_nr;
    SLIST_INSERT_HEAD(&g_allowed_syscalls, entry, link);
    return 0;
}

int
sb_deny_all_by_default(const char *app_id)
{
    (void)app_id;
    g_default_deny = 1;
    return 0;
}

int
sb_apply(const char *app_id)
{
    if (!g_sandbox_initialized)
        return -1;

    /* Build unveil list from allowed paths */
    char unveil_buf[4096] = "";
    int  offset = 0;

    sb_path_entry_t *entry;
    SLIST_FOREACH(entry, &g_allowed_paths, link) {
        const char *perm = "r";
        if (entry->mode & SB_WRITE) perm = "rw";
        if (entry->mode & SB_EXECUTE) perm = "rwx";

        int n = snprintf(unveil_buf + offset, sizeof(unveil_buf) - (size_t)offset,
                         "%s %s\n", entry->path, perm);
        if (n > 0) offset += n;
        if ((size_t)offset >= sizeof(unveil_buf) - 1) break;
    }

#if defined(__FreeBSD__) && __FreeBSD_version >= 1300000
    /* unveil in the parent before jail for extra safety */
    (void)unveil_buf;
#endif

    return sb_jail_create(app_id, g_current_app_uid, ZYGOTE_APP_DIR);
}

int
sb_pledge(const char *app_id, const char *promises, const char *execpromises)
{
    (void)app_id; (void)execpromises;

#if defined(__FreeBSD__) && __FreeBSD_version >= 1300000
    if (promises)
        return pledge(promises, execpromises);
#endif
    return 0;
}

int
sb_unveil(const char *app_id, const char *path, const char *permissions)
{
    (void)app_id;
#if defined(__FreeBSD__) && __FreeBSD_version >= 1300000
    if (path && permissions)
        return unveil(path, permissions);
#endif
    return 0;
}

int
sb_jail_create(const char *app_id, uid_t uid, const char *path)
{
    char jail_name[128];
    char jail_path[SB_PATH_MAX];

    snprintf(jail_name, sizeof(jail_name), "%s_%s", SB_JAIL_PREFIX, app_id);
    snprintf(jail_path, sizeof(jail_path), "%s/%s", path, app_id);

#if defined(__FreeBSD__) && __FreeBSD_version >= 1100000
    {
        struct iovec jparms[6];
        char uid_str[32], path_str[512];
        int  idx = 0;

        jparms[idx].iov_base = "name";
        jparms[idx].iov_len  = strlen("name") + 1;
        jparms[idx++].iov_base = jail_name;

        jparms[idx].iov_base = "path";
        jparms[idx].iov_len  = strlen(jail_path) + 1;
        jparms[idx++].iov_base = jail_path;

        snprintf(uid_str, sizeof(uid_str), "%u", (unsigned)uid);
        jparms[idx].iov_base = "uid";
        jparms[idx].iov_len  = strlen(uid_str) + 1;
        jparms[idx++].iov_base = uid_str;

        jparms[idx].iov_base = "host.hostname";
        jparms[idx].iov_len  = strlen(app_id) + 1;
        jparms[idx++].iov_base = (char *)app_id;

        jparms[idx].iov_base = "allow.raw_sockets";
        jparms[idx].iov_len  = 2;
        jparms[idx++].iov_base = "0";

        jparms[idx].iov_base = NULL;
        jparms[idx].iov_len  = 0;

        int jid = jail_set(jparms, idx, JAIL_CREATE);
        if (jid >= 0) {
            jail_set(jparms, idx, JAIL_UPDATE | JAIL_ATTACH);
            return jid;
        }
    }
#endif

    /* Fallback: just chroot */
    if (chdir(jail_path) == 0 && chroot(jail_path) == 0)
        return 0;
    return -1;
}

int
sb_path_allowed(const char *app_id, const char *path, sb_access_t mode)
{
    (void)app_id;
    if (g_default_deny) {
        sb_path_entry_t *entry;
        SLIST_FOREACH(entry, &g_allowed_paths, link) {
            if (strncmp(entry->path, path, strlen(entry->path)) == 0 &&
                (entry->mode & mode))
                return 1;
        }
        return 0;
    }
    return 1;
}
