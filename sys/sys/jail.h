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

#ifdef _KERNEL
struct jail_v0 {
	u_int32_t	version;
	char		*path;
	char		*hostname;
	u_int32_t	ip_number;
};
#endif

struct jail {
	uint32_t	version;
	char		*path;
	char		*hostname;
	char		*jailname;
	uint32_t	ip4s;
	uint32_t	ip6s;
	struct in_addr	*ip4;
	struct in6_addr	*ip6;
};
#define	JAIL_API_VERSION 2

/*
 * For all xprison structs, always keep the pr_version an int and
 * the first variable so userspace can easily distinguish them.
 */
#ifndef _KERNEL
struct xprison_v1 {
	int		 pr_version;
	int		 pr_id;
	char		 pr_path[MAXPATHLEN];
	char		 pr_host[MAXHOSTNAMELEN];
	u_int32_t	 pr_ip;
};
#endif

struct xprison {
	int		 pr_version;
	int		 pr_id;
	int		 pr_state;
	cpusetid_t	 pr_cpusetid;
	char		 pr_path[MAXPATHLEN];
	char 		 pr_host[MAXHOSTNAMELEN];
	char 		 pr_name[MAXHOSTNAMELEN];
	uint32_t	 pr_ip4s;
	uint32_t	 pr_ip6s;
#if 0
	/*
	 * sizeof(xprison) will be malloced + size needed for all
	 * IPv4 and IPv6 addesses. Offsets are based numbers of addresses.
	 */
	struct in_addr	 pr_ip4[];
	struct in6_addr	 pr_ip6[];
#endif
};
#define	XPRISON_VERSION	3

static const struct prison_state {
	int		pr_state;
	const char *	state_name;
} prison_states[] = {
#define	PRISON_STATE_INVALID		0
	{ PRISON_STATE_INVALID,		"INVALID" },
#define	PRISON_STATE_ALIVE		1
	{ PRISON_STATE_ALIVE,		"ALIVE" },
#define	PRISON_STATE_DYING		2
	{ PRISON_STATE_DYING,		"DYING" },
};


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

struct cpuset;

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
	int		 pr_state;			/* (p) prison state */
	int		 pr_nprocs;			/* (p) process count */
	char		 pr_path[MAXPATHLEN];		/* (c) chroot path */
	struct cpuset	*pr_cpuset;			/* (p) cpuset */
	struct vnode	*pr_root;			/* (c) vnode to rdir */
	char 		 pr_host[MAXHOSTNAMELEN];	/* (p) jail hostname */
	char 		 pr_name[MAXHOSTNAMELEN];	/* (c) admin jail name */
	void		*pr_linux;			/* (p) linux abi */
	int		 pr_securelevel;		/* (p) securelevel */
	struct task	 pr_task;			/* (d) destroy task */
	struct mtx	 pr_mtx;
	struct osd	 pr_osd;			/* (p) additional data */
	int		 pr_ip4s;			/* (c) number of v4 IPs */
	struct in_addr	*pr_ip4;			/* (c) v4 IPs of jail */
	int		 pr_ip6s;			/* (c) number of v6 IPs */
	struct in6_addr	*pr_ip6;			/* (c) v6 IPs of jail */
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
struct thread;
int kern_jail(struct thread *, struct jail *);
int jailed(struct ucred *cred);
void getcredhostname(struct ucred *cred, char *, size_t);
int prison_check(struct ucred *cred1, struct ucred *cred2);
int prison_canseemount(struct ucred *cred, struct mount *mp);
void prison_enforce_statfs(struct ucred *cred, struct mount *mp,
    struct statfs *sp);
struct prison *prison_find(int prid);
void prison_free(struct prison *pr);
void prison_free_locked(struct prison *pr);
void prison_hold(struct prison *pr);
void prison_hold_locked(struct prison *pr);
void prison_proc_hold(struct prison *);
void prison_proc_free(struct prison *);
int prison_getip4(struct ucred *cred, struct in_addr *ia);
int prison_local_ip4(struct ucred *cred, struct in_addr *ia);
int prison_remote_ip4(struct ucred *cred, struct in_addr *ia);
int prison_check_ip4(struct ucred *cred, struct in_addr *ia);
#ifdef INET6
int prison_getip6(struct ucred *, struct in6_addr *);
int prison_local_ip6(struct ucred *, struct in6_addr *, int);
int prison_remote_ip6(struct ucred *, struct in6_addr *);
int prison_check_ip6(struct ucred *, struct in6_addr *);
#endif
int prison_if(struct ucred *cred, struct sockaddr *sa);
int prison_priv_check(struct ucred *cred, int priv);

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
