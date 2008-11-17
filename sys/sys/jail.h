/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#ifndef _SYS_JAIL_H_
#define _SYS_JAIL_H_

struct jail {
	u_int32_t	version;
	char		*path;
	char		*hostname;
	u_int32_t	ip_number;
};

struct xprison {
	int		 pr_version;
	int		 pr_id;
	char		 pr_path[MAXPATHLEN];
	char 		 pr_host[MAXHOSTNAMELEN];
	u_int32_t	 pr_ip;
};
#define	XPRISON_VERSION	1

#ifndef _KERNEL

int jail(struct jail *);
int jail_attach(int);

#else /* _KERNEL */

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/_task.h>

#define JAIL_MAX	999999

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_PRISON);
#endif
#endif /* _KERNEL */

#if defined(_KERNEL) || defined(_WANT_PRISON)

#include <sys/osd.h>

/*
 * This structure describes a prison.  It is pointed to by all struct
 * ucreds's of the inmates.  pr_ref keeps track of them and is used to
 * delete the struture when the last inmate is dead.
 *
 * Lock key:
 *   (a) allprison_lock
 *   (p) locked by pr_mtx
 *   (c) set only during creation before the structure is shared, no mutex
 *       required to read
 *   (d) set only during destruction of jail, no mutex needed
 */
struct prison {
	LIST_ENTRY(prison) pr_list;			/* (a) all prisons */
	int		 pr_id;				/* (c) prison id */
	int		 pr_ref;			/* (p) refcount */
	char		 pr_path[MAXPATHLEN];		/* (c) chroot path */
	struct vnode	*pr_root;			/* (c) vnode to rdir */
	char 		 pr_host[MAXHOSTNAMELEN];	/* (p) jail hostname */
	u_int32_t	 pr_ip;				/* (c) ip addr host */
	void		*pr_linux;			/* (p) linux abi */
	int		 pr_securelevel;		/* (p) securelevel */
	struct task	 pr_task;			/* (d) destroy task */
	struct mtx	 pr_mtx;
	struct osd	 pr_osd;			/* (p) additional data */
};
#endif /* _KERNEL || _WANT_PRISON */

#ifdef _KERNEL
/*
 * Sysctl-set variables that determine global jail policy
 *
 * XXX MIB entries will need to be protected by a mutex.
 */
extern int	jail_set_hostname_allowed;
extern int	jail_socket_unixiproute_only;
extern int	jail_sysvipc_allowed;
extern int	jail_getfsstat_jailrootonly;
extern int	jail_allow_raw_sockets;
extern int	jail_chflags_allowed;

LIST_HEAD(prisonlist, prison);
extern struct	prisonlist allprison;
extern struct	sx allprison_lock;

/*
 * Kernel support functions for jail().
 */
struct ucred;
struct mount;
struct sockaddr;
struct statfs;
int jailed(struct ucred *cred);
void getcredhostname(struct ucred *cred, char *, size_t);
int prison_check(struct ucred *cred1, struct ucred *cred2);
int prison_canseemount(struct ucred *cred, struct mount *mp);
void prison_enforce_statfs(struct ucred *cred, struct mount *mp,
    struct statfs *sp);
struct prison *prison_find(int prid);
void prison_free(struct prison *pr);
void prison_free_locked(struct prison *pr);
u_int32_t prison_getip(struct ucred *cred);
void prison_hold(struct prison *pr);
void prison_hold_locked(struct prison *pr);
int prison_if(struct ucred *cred, struct sockaddr *sa);
int prison_ip(struct ucred *cred, int flag, u_int32_t *ip);
int prison_priv_check(struct ucred *cred, int priv);
void prison_remote_ip(struct ucred *cred, int flags, u_int32_t *ip);

/*
 * Kernel jail services.
 */
struct prison_service;
typedef int (*prison_create_t)(struct prison_service *psrv, struct prison *pr);
typedef int (*prison_destroy_t)(struct prison_service *psrv, struct prison *pr);

struct prison_service *prison_service_register(const char *name,
    prison_create_t create, prison_destroy_t destroy);
void prison_service_deregister(struct prison_service *psrv);

void prison_service_data_set(struct prison_service *psrv, struct prison *pr,
    void *data);
void *prison_service_data_get(struct prison_service *psrv, struct prison *pr);
void *prison_service_data_del(struct prison_service *psrv, struct prison *pr);

#endif /* _KERNEL */
#endif /* !_SYS_JAIL_H_ */
