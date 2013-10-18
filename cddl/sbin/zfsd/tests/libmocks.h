#ifndef _LIBMOCKS_H_
#define _LIBMOCKS_H_

#ifdef __cplusplus
extern "C" {
#endif

struct libzfs_handle;
typedef struct libzfs_handle libzfs_handle_t;
struct zpool_handle;
typedef struct zpool_handle zpool_handle_t;
typedef int (*zpool_iter_f)(zpool_handle_t *, void *);

void syslog(int priority, const char* message, ...);
int zpool_iter(libzfs_handle_t*, zpool_iter_f, void*);

extern int syslog_last_priority;
extern char syslog_last_message[4096];

#ifdef __cplusplus
}
#endif

#endif
