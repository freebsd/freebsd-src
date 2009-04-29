/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2008 Bjoern A. Zeeb.
 * Copyright (c) 2009 James Gritton.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/osd.h>
#include <sys/sx.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/vimage.h>
#include <net/if.h>
#include <netinet/in.h>
#ifdef DDB
#include <ddb/ddb.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#endif /* INET6 */
#endif /* DDB */

#include <security/mac/mac_framework.h>

MALLOC_DEFINE(M_PRISON, "prison", "Prison structures");

SYSCTL_NODE(_security, OID_AUTO, jail, CTLFLAG_RW, 0,
    "Jail rules");

int	jail_set_hostname_allowed = 1;
SYSCTL_INT(_security_jail, OID_AUTO, set_hostname_allowed, CTLFLAG_RW,
    &jail_set_hostname_allowed, 0,
    "Processes in jail can set their hostnames");

int	jail_socket_unixiproute_only = 1;
SYSCTL_INT(_security_jail, OID_AUTO, socket_unixiproute_only, CTLFLAG_RW,
    &jail_socket_unixiproute_only, 0,
    "Processes in jail are limited to creating UNIX/IP/route sockets only");

int	jail_sysvipc_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, sysvipc_allowed, CTLFLAG_RW,
    &jail_sysvipc_allowed, 0,
    "Processes in jail can use System V IPC primitives");

static int jail_enforce_statfs = 2;
SYSCTL_INT(_security_jail, OID_AUTO, enforce_statfs, CTLFLAG_RW,
    &jail_enforce_statfs, 0,
    "Processes in jail cannot see all mounted file systems");

int	jail_allow_raw_sockets = 0;
SYSCTL_INT(_security_jail, OID_AUTO, allow_raw_sockets, CTLFLAG_RW,
    &jail_allow_raw_sockets, 0,
    "Prison root can create raw sockets");

int	jail_chflags_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, chflags_allowed, CTLFLAG_RW,
    &jail_chflags_allowed, 0,
    "Processes in jail can alter system file flags");

int	jail_mount_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, mount_allowed, CTLFLAG_RW,
    &jail_mount_allowed, 0,
    "Processes in jail can mount/unmount jail-friendly file systems");

int	jail_max_af_ips = 255;
SYSCTL_INT(_security_jail, OID_AUTO, jail_max_af_ips, CTLFLAG_RW,
    &jail_max_af_ips, 0,
    "Number of IP addresses a jail may have at most per address family");

/* allprison, lastprid, and prisoncount are protected by allprison_lock. */
struct	sx allprison_lock;
SX_SYSINIT(allprison_lock, &allprison_lock, "allprison");
struct	prisonlist allprison = TAILQ_HEAD_INITIALIZER(allprison);
int	lastprid = 0;
int	prisoncount = 0;

static int do_jail_attach(struct thread *td, struct prison *pr);
static void prison_complete(void *context, int pending);
static void prison_deref(struct prison *pr, int flags);
#ifdef INET
static int _prison_check_ip4(struct prison *pr, struct in_addr *ia);
#endif
#ifdef INET6
static int _prison_check_ip6(struct prison *pr, struct in6_addr *ia6);
#endif
static int sysctl_jail_list(SYSCTL_HANDLER_ARGS);

/* Flags for prison_deref */
#define	PD_DEREF	0x01
#define	PD_DEUREF	0x02
#define	PD_LOCKED	0x04
#define	PD_LIST_SLOCKED	0x08
#define	PD_LIST_XLOCKED	0x10

#ifdef INET
static int
qcmp_v4(const void *ip1, const void *ip2)
{
	in_addr_t iaa, iab;

	/*
	 * We need to compare in HBO here to get the list sorted as expected
	 * by the result of the code.  Sorting NBO addresses gives you
	 * interesting results.  If you do not understand, do not try.
	 */
	iaa = ntohl(((const struct in_addr *)ip1)->s_addr);
	iab = ntohl(((const struct in_addr *)ip2)->s_addr);

	/*
	 * Do not simply return the difference of the two numbers, the int is
	 * not wide enough.
	 */
	if (iaa > iab)
		return (1);
	else if (iaa < iab)
		return (-1);
	else
		return (0);
}
#endif

#ifdef INET6
static int
qcmp_v6(const void *ip1, const void *ip2)
{
	const struct in6_addr *ia6a, *ia6b;
	int i, rc;

	ia6a = (const struct in6_addr *)ip1;
	ia6b = (const struct in6_addr *)ip2;

	rc = 0;
	for (i = 0; rc == 0 && i < sizeof(struct in6_addr); i++) {
		if (ia6a->s6_addr[i] > ia6b->s6_addr[i])
			rc = 1;
		else if (ia6a->s6_addr[i] < ia6b->s6_addr[i])
			rc = -1;
	}
	return (rc);
}
#endif

/*
 * struct jail_args {
 *	struct jail *jail;
 * };
 */
int
jail(struct thread *td, struct jail_args *uap)
{
	struct iovec optiov[10];
	struct uio opt;
	char *u_path, *u_hostname, *u_name;
#ifdef INET
	struct in_addr *u_ip4;
#endif
#ifdef INET6
	struct in6_addr *u_ip6;
#endif
	uint32_t version;
	int error;

	error = copyin(uap->jail, &version, sizeof(uint32_t));
	if (error)
		return (error);

	switch (version) {
	case 0:
	{
		/* FreeBSD single IPv4 jails. */
		struct jail_v0 j0;

		error = copyin(uap->jail, &j0, sizeof(struct jail_v0));
		if (error)
			return (error);
		u_path = malloc(MAXPATHLEN + MAXHOSTNAMELEN, M_TEMP, M_WAITOK);
		u_hostname = u_path + MAXPATHLEN;
		opt.uio_iov = optiov;
		opt.uio_iovcnt = 4;
		opt.uio_offset = -1;
		opt.uio_resid = -1;
		opt.uio_segflg = UIO_SYSSPACE;
		opt.uio_rw = UIO_READ;
		opt.uio_td = td;
		optiov[0].iov_base = "path";
		optiov[0].iov_len = sizeof("path");
		optiov[1].iov_base = u_path;
		error =
		    copyinstr(j0.path, u_path, MAXPATHLEN, &optiov[1].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
		optiov[2].iov_base = "host.hostname";
		optiov[2].iov_len = sizeof("host.hostname");
		optiov[3].iov_base = u_hostname;
		error = copyinstr(j0.hostname, u_hostname, MAXHOSTNAMELEN,
		    &optiov[3].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
#ifdef INET
		optiov[opt.uio_iovcnt].iov_base = "ip4.addr";
		optiov[opt.uio_iovcnt].iov_len = sizeof("ip4.addr");
		opt.uio_iovcnt++;
		optiov[opt.uio_iovcnt].iov_base = &j0.ip_number;
		j0.ip_number = htonl(j0.ip_number);
		optiov[opt.uio_iovcnt].iov_len = sizeof(j0.ip_number);
		opt.uio_iovcnt++;
#endif
		break;
	}

	case 1:
		/*
		 * Version 1 was used by multi-IPv4 jail implementations
		 * that never made it into the official kernel.
		 */
		return (EINVAL);

	case 2:	/* JAIL_API_VERSION */
	{
		/* FreeBSD multi-IPv4/IPv6,noIP jails. */
		struct jail j;
		size_t tmplen;

		error = copyin(uap->jail, &j, sizeof(struct jail));
		if (error)
			return (error);
		tmplen = MAXPATHLEN + MAXHOSTNAMELEN + MAXHOSTNAMELEN;
#ifdef INET
		if (j.ip4s > jail_max_af_ips)
			return (EINVAL);
		tmplen += j.ip4s * sizeof(struct in_addr);
#else
		if (j.ip4s > 0)
			return (EINVAL);
#endif
#ifdef INET6
		if (j.ip6s > jail_max_af_ips)
			return (EINVAL);
		tmplen += j.ip6s * sizeof(struct in6_addr);
#else
		if (j.ip6s > 0)
			return (EINVAL);
#endif
		u_path = malloc(tmplen, M_TEMP, M_WAITOK);
		u_hostname = u_path + MAXPATHLEN;
		u_name = u_hostname + MAXHOSTNAMELEN;
#ifdef INET
		u_ip4 = (struct in_addr *)(u_name + MAXHOSTNAMELEN);
#endif
#ifdef INET6
#ifdef INET
		u_ip6 = (struct in6_addr *)(u_ip4 + j.ip4s);
#else
		u_ip6 = (struct in6_addr *)(u_name + MAXHOSTNAMELEN);
#endif
#endif
		opt.uio_iov = optiov;
		opt.uio_iovcnt = 4;
		opt.uio_offset = -1;
		opt.uio_resid = -1;
		opt.uio_segflg = UIO_SYSSPACE;
		opt.uio_rw = UIO_READ;
		opt.uio_td = td;
		optiov[0].iov_base = "path";
		optiov[0].iov_len = sizeof("path");
		optiov[1].iov_base = u_path;
		error =
		    copyinstr(j.path, u_path, MAXPATHLEN, &optiov[1].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
		optiov[2].iov_base = "host.hostname";
		optiov[2].iov_len = sizeof("host.hostname");
		optiov[3].iov_base = u_hostname;
		error = copyinstr(j.hostname, u_hostname, MAXHOSTNAMELEN,
		    &optiov[3].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
		if (j.jailname != NULL) {
			optiov[opt.uio_iovcnt].iov_base = "name";
			optiov[opt.uio_iovcnt].iov_len = sizeof("name");
			opt.uio_iovcnt++;
			optiov[opt.uio_iovcnt].iov_base = u_name;
			error = copyinstr(j.jailname, u_name, MAXHOSTNAMELEN,
			    &optiov[opt.uio_iovcnt].iov_len);
			if (error) {
				free(u_path, M_TEMP);
				return (error);
			}
			opt.uio_iovcnt++;
		}
#ifdef INET
		optiov[opt.uio_iovcnt].iov_base = "ip4.addr";
		optiov[opt.uio_iovcnt].iov_len = sizeof("ip4.addr");
		opt.uio_iovcnt++;
		optiov[opt.uio_iovcnt].iov_base = u_ip4;
		optiov[opt.uio_iovcnt].iov_len =
		    j.ip4s * sizeof(struct in_addr);
		error = copyin(j.ip4, u_ip4, optiov[opt.uio_iovcnt].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
		opt.uio_iovcnt++;
#endif
#ifdef INET6
		optiov[opt.uio_iovcnt].iov_base = "ip6.addr";
		optiov[opt.uio_iovcnt].iov_len = sizeof("ip6.addr");
		opt.uio_iovcnt++;
		optiov[opt.uio_iovcnt].iov_base = u_ip6;
		optiov[opt.uio_iovcnt].iov_len =
		    j.ip6s * sizeof(struct in6_addr);
		error = copyin(j.ip6, u_ip6, optiov[opt.uio_iovcnt].iov_len);
		if (error) {
			free(u_path, M_TEMP);
			return (error);
		}
		opt.uio_iovcnt++;
#endif
		break;
	}

	default:
		/* Sci-Fi jails are not supported, sorry. */
		return (EINVAL);
	}
	error = kern_jail_set(td, &opt, JAIL_CREATE | JAIL_ATTACH);
	free(u_path, M_TEMP);
	return (error);
}

/*
 * struct jail_set_args {
 *	struct iovec *iovp;
 *	unsigned int iovcnt;
 *	int flags;
 * };
 */
int
jail_set(struct thread *td, struct jail_set_args *uap)
{
	struct uio *auio;
	int error;

	/* Check that we have an even number of iovecs. */
	if (uap->iovcnt & 1)
		return (EINVAL);

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_jail_set(td, auio, uap->flags);
	free(auio, M_IOV);
	return (error);
}

int
kern_jail_set(struct thread *td, struct uio *optuio, int flags)
{
	struct nameidata nd;
#ifdef INET
	struct in_addr *ip4;
#endif
#ifdef INET6
	struct in6_addr *ip6;
#endif
	struct vfsopt *opt;
	struct vfsoptlist *opts;
	struct prison *pr, *deadpr, *tpr;
	struct vnode *root;
	char *errmsg, *host, *name, *p, *path;
	void *op;
	int created, cuflags, error, errmsg_len, errmsg_pos;
	int gotslevel, jid, len;
	int slevel, vfslocked;
#if defined(INET) || defined(INET6)
	int ii;
#endif
#ifdef INET
	int ip4s;
#endif
#ifdef INET6
	int ip6s;
#endif
	unsigned pr_flags, ch_flags;
	char numbuf[12];

	error = priv_check(td, PRIV_JAIL_SET);
	if (!error && (flags & JAIL_ATTACH))
		error = priv_check(td, PRIV_JAIL_ATTACH);
	if (error)
		return (error);
	if (flags & ~JAIL_SET_MASK)
		return (EINVAL);

	/*
	 * Check all the parameters before committing to anything.  Not all
	 * errors can be caught early, but we may as well try.  Also, this
	 * takes care of some expensive stuff (path lookup) before getting
	 * the allprison lock.
	 *
	 * XXX Jails are not filesystems, and jail parameters are not mount
	 *     options.  But it makes more sense to re-use the vfsopt code
	 *     than duplicate it under a different name.
	 */
	error = vfs_buildopts(optuio, &opts);
	if (error)
		return (error);
#ifdef INET
	ip4 = NULL;
#endif
#ifdef INET6
	ip6 = NULL;
#endif

	error = vfs_copyopt(opts, "jid", &jid, sizeof(jid));
	if (error == ENOENT)
		jid = 0;
	else if (error != 0)
		goto done_free;

	error = vfs_copyopt(opts, "securelevel", &slevel, sizeof(slevel));
	if (error == ENOENT)
		gotslevel = 0;
	else if (error != 0)
		goto done_free;
	else
		gotslevel = 1;

	pr_flags = ch_flags = 0;
	vfs_flagopt(opts, "persist", &pr_flags, PR_PERSIST);
	vfs_flagopt(opts, "nopersist", &ch_flags, PR_PERSIST);
	ch_flags |= pr_flags;
	if ((flags & (JAIL_CREATE | JAIL_UPDATE | JAIL_ATTACH)) == JAIL_CREATE
	    && !(pr_flags & PR_PERSIST)) {
		error = EINVAL;
		vfs_opterror(opts, "new jail must persist or attach");
		goto done_errmsg;
	}

	error = vfs_getopt(opts, "name", (void **)&name, &len);
	if (error == ENOENT)
		name = NULL;
	else if (error != 0)
		goto done_free;
	else {
		if (len == 0 || name[len - 1] != '\0') {
			error = EINVAL;
			goto done_free;
		}
		if (len > MAXHOSTNAMELEN) {
			error = ENAMETOOLONG;
			goto done_free;
		}
	}

	error = vfs_getopt(opts, "host.hostname", (void **)&host, &len);
	if (error == ENOENT)
		host = NULL;
	else if (error != 0)
		goto done_free;
	else {
		if (len == 0 || host[len - 1] != '\0') {
			error = EINVAL;
			goto done_free;
		}
		if (len > MAXHOSTNAMELEN) {
			error = ENAMETOOLONG;
			goto done_free;
		}
	}

#ifdef INET
	error = vfs_getopt(opts, "ip4.addr", &op, &ip4s);
	if (error == ENOENT)
		ip4s = -1;
	else if (error != 0)
		goto done_free;
	else if (ip4s & (sizeof(*ip4) - 1)) {
		error = EINVAL;
		goto done_free;
	} else if (ip4s > 0) {
		ip4s /= sizeof(*ip4);
		if (ip4s > jail_max_af_ips) {
			error = EINVAL;
			vfs_opterror(opts, "too many IPv4 addresses");
			goto done_errmsg;
		}
		ip4 = malloc(ip4s * sizeof(*ip4), M_PRISON, M_WAITOK);
		bcopy(op, ip4, ip4s * sizeof(*ip4));
		/*
		 * IP addresses are all sorted but ip[0] to preserve the
		 * primary IP address as given from userland.  This special IP
		 * is used for unbound outgoing connections as well for
		 * "loopback" traffic.
		 */
		if (ip4s > 1)
			qsort(ip4 + 1, ip4s - 1, sizeof(*ip4), qcmp_v4);
		/*
		 * Check for duplicate addresses and do some simple zero and
		 * broadcast checks. If users give other bogus addresses it is
		 * their problem.
		 *
		 * We do not have to care about byte order for these checks so
		 * we will do them in NBO.
		 */
		for (ii = 0; ii < ip4s; ii++) {
			if (ip4[ii].s_addr == INADDR_ANY ||
			    ip4[ii].s_addr == INADDR_BROADCAST) {
				error = EINVAL;
				goto done_free;
			}
			if ((ii+1) < ip4s &&
			    (ip4[0].s_addr == ip4[ii+1].s_addr ||
			     ip4[ii].s_addr == ip4[ii+1].s_addr)) {
				error = EINVAL;
				goto done_free;
			}
		}
	}
#endif

#ifdef INET6
	error = vfs_getopt(opts, "ip6.addr", &op, &ip6s);
	if (error == ENOENT)
		ip6s = -1;
	else if (error != 0)
		goto done_free;
	else if (ip6s & (sizeof(*ip6) - 1)) {
		error = EINVAL;
		goto done_free;
	} else if (ip6s > 0) {
		ip6s /= sizeof(*ip6);
		if (ip6s > jail_max_af_ips) {
			error = EINVAL;
			vfs_opterror(opts, "too many IPv6 addresses");
			goto done_errmsg;
		}
		ip6 = malloc(ip6s * sizeof(*ip6), M_PRISON, M_WAITOK);
		bcopy(op, ip6, ip6s * sizeof(*ip6));
		if (ip6s > 1)
			qsort(ip6 + 1, ip6s - 1, sizeof(*ip6), qcmp_v6);
		for (ii = 0; ii < ip6s; ii++) {
			if (IN6_IS_ADDR_UNSPECIFIED(&ip6[0])) {
				error = EINVAL;
				goto done_free;
			}
			if ((ii+1) < ip6s &&
			    (IN6_ARE_ADDR_EQUAL(&ip6[0], &ip6[ii+1]) ||
			     IN6_ARE_ADDR_EQUAL(&ip6[ii], &ip6[ii+1])))
			{
				error = EINVAL;
				goto done_free;
			}
		}
	}
#endif

	root = NULL;
	error = vfs_getopt(opts, "path", (void **)&path, &len);
	if (error == ENOENT)
		path = NULL;
	else if (error != 0)
		goto done_free;
	else {
		if (flags & JAIL_UPDATE) {
			error = EINVAL;
			vfs_opterror(opts,
			    "path cannot be changed after creation");
			goto done_errmsg;
		}
		if (len == 0 || path[len - 1] != '\0') {
			error = EINVAL;
			goto done_free;
		}
		if (len > MAXPATHLEN) {
			error = ENAMETOOLONG;
			goto done_free;
		}
		if (len < 2 || (len == 2 && path[0] == '/'))
			path = NULL;
		else {
			NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW, UIO_SYSSPACE,
			    path, td);
			error = namei(&nd);
			if (error)
				goto done_free;
			vfslocked = NDHASGIANT(&nd);
			root = nd.ni_vp;
			NDFREE(&nd, NDF_ONLY_PNBUF);
			if (root->v_type != VDIR) {
				error = ENOTDIR;
				vrele(root);
				VFS_UNLOCK_GIANT(vfslocked);
				goto done_free;
			}
			VFS_UNLOCK_GIANT(vfslocked);
		}
	}

	/*
	 * Grab the allprison lock before letting modules check their
	 * parameters.  Once we have it, do not let go so we'll have a
	 * consistent view of the OSD list.
	 */
	sx_xlock(&allprison_lock);
	error = osd_jail_call(NULL, PR_METHOD_CHECK, opts);
	if (error)
		goto done_unlock_list;

	/* By now, all parameters should have been noted. */
	TAILQ_FOREACH(opt, opts, link) {
		if (!opt->seen && strcmp(opt->name, "errmsg")) {
			error = EINVAL;
			vfs_opterror(opts, "unknown parameter: %s", opt->name);
			goto done_unlock_list;
		}
	}

	/*
	 * See if we are creating a new record or updating an existing one.
	 * This abuses the file error codes ENOENT and EEXIST.
	 */
	cuflags = flags & (JAIL_CREATE | JAIL_UPDATE);
	if (!cuflags) {
		error = EINVAL;
		vfs_opterror(opts, "no valid operation (create or update)");
		goto done_unlock_list;
	}
	pr = NULL;
	if (jid != 0) {
		/* See if a requested jid already exists. */
		if (jid < 0) {
			error = EINVAL;
			vfs_opterror(opts, "negative jid");
			goto done_unlock_list;
		}
		pr = prison_find(jid);
		if (pr != NULL) {
			/* Create: jid must not exist. */
			if (cuflags == JAIL_CREATE) {
				mtx_unlock(&pr->pr_mtx);
				error = EEXIST;
				vfs_opterror(opts, "jail %d already exists",
				    jid);
				goto done_unlock_list;
			}
			if (pr->pr_uref == 0) {
				if (!(flags & JAIL_DYING)) {
					mtx_unlock(&pr->pr_mtx);
					error = ENOENT;
					vfs_opterror(opts, "jail %d is dying",
					    jid);
					goto done_unlock_list;
				} else if ((flags & JAIL_ATTACH) ||
				    (pr_flags & PR_PERSIST)) {
					/*
					 * A dying jail might be resurrected
					 * (via attach or persist), but first
					 * it must determine if another jail
					 * has claimed its name.  Accomplish
					 * this by implicitly re-setting the
					 * name.
					 */
					if (name == NULL)
						name = pr->pr_name;
				}
			}
		}
		if (pr == NULL) {
			/* Update: jid must exist. */
			if (cuflags == JAIL_UPDATE) {
				error = ENOENT;
				vfs_opterror(opts, "jail %d not found", jid);
				goto done_unlock_list;
			}
		}
	}
	/*
	 * If the caller provided a name, look for a jail by that name.
	 * This has different semantics for creates and updates keyed by jid
	 * (where the name must not already exist in a different jail),
	 * and updates keyed by the name itself (where the name must exist
	 * because that is the jail being updated).
	 */
	if (name != NULL) {
		if (name[0] != '\0') {
			deadpr = NULL;
 name_again:
			TAILQ_FOREACH(tpr, &allprison, pr_list) {
				if (tpr != pr && tpr->pr_ref > 0 &&
				    !strcmp(tpr->pr_name, name)) {
					if (pr == NULL &&
					    cuflags != JAIL_CREATE) {
						mtx_lock(&tpr->pr_mtx);
						if (tpr->pr_ref > 0) {
							/*
							 * Use this jail
							 * for updates.
							 */
							if (tpr->pr_uref > 0) {
								pr = tpr;
								break;
							}
							deadpr = tpr;
						}
						mtx_unlock(&tpr->pr_mtx);
					} else if (tpr->pr_uref > 0) {
						/*
						 * Create, or update(jid):
						 * name must not exist in an
						 * active jail.
						 */
						error = EEXIST;
						if (pr != NULL)
							mtx_unlock(&pr->pr_mtx);
						vfs_opterror(opts,
						   "jail \"%s\" already exists",
						   name);
						goto done_unlock_list;
					}
				}
			}
			/* If no active jail is found, use a dying one. */
			if (deadpr != NULL && pr == NULL) {
				if (flags & JAIL_DYING) {
					mtx_lock(&deadpr->pr_mtx);
					if (deadpr->pr_ref == 0) {
						mtx_unlock(&deadpr->pr_mtx);
						goto name_again;
					}
					pr = deadpr;
				} else if (cuflags == JAIL_UPDATE) {
					error = ENOENT;
					vfs_opterror(opts,
					    "jail \"%s\" is dying", name);
					goto done_unlock_list;
				}
			}
			/* Update: name must exist if no jid. */
			else if (cuflags == JAIL_UPDATE && pr == NULL) {
				error = ENOENT;
				vfs_opterror(opts, "jail \"%s\" not found",
				    name);
				goto done_unlock_list;
			}
		}
	}
	/* Update: must provide a jid or name. */
	else if (cuflags == JAIL_UPDATE && pr == NULL) {
		error = ENOENT;
		vfs_opterror(opts, "update specified no jail");
		goto done_unlock_list;
	}

	/* If there's no prison to update, create a new one and link it in. */
	if (pr == NULL) {
		created = 1;
		pr = malloc(sizeof(*pr), M_PRISON, M_WAITOK | M_ZERO);
		if (jid == 0) {
			/* Find the next free jid. */
			jid = lastprid + 1;
 findnext:
			if (jid == JAIL_MAX)
				jid = 1;
			TAILQ_FOREACH(tpr, &allprison, pr_list) {
				if (tpr->pr_id < jid)
					continue;
				if (tpr->pr_id > jid || tpr->pr_ref == 0) {
					TAILQ_INSERT_BEFORE(tpr, pr, pr_list);
					break;
				}
				if (jid == lastprid) {
					error = EAGAIN;
					vfs_opterror(opts,
					    "no available jail IDs");
					free(pr, M_PRISON);
					goto done_unlock_list;
				}
				jid++;
				goto findnext;
			}
			lastprid = jid;
		} else {
			/*
			 * The jail already has a jid (that did not yet exist),
			 * so just find where to insert it.
			 */
			TAILQ_FOREACH(tpr, &allprison, pr_list)
				if (tpr->pr_id >= jid) {
					TAILQ_INSERT_BEFORE(tpr, pr, pr_list);
					break;
				}
		}
		if (tpr == NULL)
			TAILQ_INSERT_TAIL(&allprison, pr, pr_list);
		prisoncount++;

		pr->pr_id = jid;
		if (name == NULL)
			name = "";
		if (path == NULL) {
			path = "/";
			root = rootvnode;
			vref(root);
		}

		mtx_init(&pr->pr_mtx, "jail mutex", NULL, MTX_DEF);

		/*
		 * Allocate a dedicated cpuset for each jail.
		 * Unlike other initial settings, this may return an erorr.
		 */
		error = cpuset_create_root(td, &pr->pr_cpuset);
		if (error) {
			prison_deref(pr, PD_LIST_XLOCKED);
			goto done_releroot;
		}

		mtx_lock(&pr->pr_mtx);
		/*
		 * New prisons do not yet have a reference, because we do not
		 * want other to see the incomplete prison once the
		 * allprison_lock is downgraded.
		 */
	} else {
		created = 0;
		/*
		 * Grab a reference for existing prisons, to ensure they
		 * continue to exist for the duration of the call.
		 */
		pr->pr_ref++;
	}

	/* Do final error checking before setting anything. */
	error = 0;
#if defined(INET) || defined(INET6)
	if (
#ifdef INET
	    ip4s > 0
#ifdef INET6
	    ||
#endif
#endif
#ifdef INET6
	    ip6s > 0
#endif
	    )
		/*
		 * Check for conflicting IP addresses.  We permit them if there
		 * is no more than 1 IP on each jail.  If there is a duplicate
		 * on a jail with more than one IP stop checking and return
		 * error.
		 */
		TAILQ_FOREACH(tpr, &allprison, pr_list) {
			if (tpr == pr || tpr->pr_uref == 0)
				continue;
#ifdef INET
			if ((ip4s > 0 && tpr->pr_ip4s > 1) ||
			    (ip4s > 1 && tpr->pr_ip4s > 0))
				for (ii = 0; ii < ip4s; ii++)
					if (_prison_check_ip4(tpr,
					    &ip4[ii]) == 0) {
						error = EINVAL;
						vfs_opterror(opts,
						    "IPv4 addresses clash");
						goto done_deref_locked;
					}
#endif
#ifdef INET6
			if ((ip6s > 0 && tpr->pr_ip6s > 1) ||
			    (ip6s > 1 && tpr->pr_ip6s > 0))
				for (ii = 0; ii < ip6s; ii++)
					if (_prison_check_ip6(tpr,
					    &ip6[ii]) == 0) {
						error = EINVAL;
						vfs_opterror(opts,
						    "IPv6 addresses clash");
						goto done_deref_locked;
					}
#endif
		}
#endif
	if (error == 0 && name != NULL) {
		/* Give a default name of the jid. */
		if (name[0] == '\0')
			snprintf(name = numbuf, sizeof(numbuf), "%d", jid);
		else if (strtoul(name, &p, 10) != jid && *p == '\0') {
			error = EINVAL;
			vfs_opterror(opts, "name cannot be numeric");
		}
	}
	if (error) {
 done_deref_locked:
		/*
		 * Some parameter had an error so do not set anything.
		 * If this is a new jail, it will go away without ever
		 * having been seen.
		 */
		prison_deref(pr, created
		    ? PD_LOCKED | PD_LIST_XLOCKED
		    : PD_DEREF | PD_LOCKED | PD_LIST_XLOCKED);
		goto done_releroot;
	}

	/* Set the parameters of the prison. */
#ifdef INET
	if (ip4s >= 0) {
		pr->pr_ip4s = ip4s;
		free(pr->pr_ip4, M_PRISON);
		pr->pr_ip4 = ip4;
		ip4 = NULL;
	}
#endif
#ifdef INET6
	if (ip6s >= 0) {
		pr->pr_ip6s = ip6s;
		free(pr->pr_ip6, M_PRISON);
		pr->pr_ip6 = ip6;
		ip6 = NULL;
	}
#endif
	if (gotslevel)
		pr->pr_securelevel = slevel;
	if (name != NULL)
		strlcpy(pr->pr_name, name, sizeof(pr->pr_name));
	if (path != NULL) {
		strlcpy(pr->pr_path, path, sizeof(pr->pr_path));
		pr->pr_root = root;
	}
	if (host != NULL)
		strlcpy(pr->pr_host, host, sizeof(pr->pr_host));
	/*
	 * Persistent prisons get an extra reference, and prisons losing their
	 * persist flag lose that reference.  Only do this for existing prisons
	 * for now, so new ones will remain unseen until after the module
	 * handlers have completed.
	 */
	if (!created && (ch_flags & PR_PERSIST & (pr_flags ^ pr->pr_flags))) {
		if (pr_flags & PR_PERSIST) {
			pr->pr_ref++;
			pr->pr_uref++;
		} else {
			pr->pr_ref--;
			pr->pr_uref--;
		}
	}
	pr->pr_flags = (pr->pr_flags & ~ch_flags) | pr_flags;
	mtx_unlock(&pr->pr_mtx);

	/* Let the modules do their work. */
	sx_downgrade(&allprison_lock);
	if (created) {
		error = osd_jail_call(pr, PR_METHOD_CREATE, opts);
		if (error) {
			prison_deref(pr, PD_LIST_SLOCKED);
			goto done_errmsg;
		}
	}
	error = osd_jail_call(pr, PR_METHOD_SET, opts);
	if (error) {
		prison_deref(pr, created
		    ? PD_LIST_SLOCKED
		    : PD_DEREF | PD_LIST_SLOCKED);
		goto done_errmsg;
	}

	/* Attach this process to the prison if requested. */
	if (flags & JAIL_ATTACH) {
		mtx_lock(&pr->pr_mtx);
		error = do_jail_attach(td, pr);
		if (error) {
			vfs_opterror(opts, "attach failed");
			if (!created)
				prison_deref(pr, PD_DEREF);
			goto done_errmsg;
		}
	}

	/*
	 * Now that it is all there, drop the temporary reference from existing
	 * prisons.  Or add a reference to newly created persistent prisons
	 * (which was not done earlier so that the prison would not be publicly
	 * visible).
	 */
	if (!created) {
		prison_deref(pr, (flags & JAIL_ATTACH)
		    ? PD_DEREF
		    : PD_DEREF | PD_LIST_SLOCKED);
	} else {
		if (pr_flags & PR_PERSIST) {
			mtx_lock(&pr->pr_mtx);
			pr->pr_ref++;
			pr->pr_uref++;
			mtx_unlock(&pr->pr_mtx);
		}
		if (!(flags & JAIL_ATTACH))
			sx_sunlock(&allprison_lock);
	}
	td->td_retval[0] = pr->pr_id;
	goto done_errmsg;

 done_unlock_list:
	sx_xunlock(&allprison_lock);
 done_releroot:
	if (root != NULL) {
		vfslocked = VFS_LOCK_GIANT(root->v_mount);
		vrele(root);
		VFS_UNLOCK_GIANT(vfslocked);
	}
 done_errmsg:
	if (error) {
		vfs_getopt(opts, "errmsg", (void **)&errmsg, &errmsg_len);
		if (errmsg_len > 0) {
			errmsg_pos = 2 * vfs_getopt_pos(opts, "errmsg") + 1;
			if (errmsg_pos > 0) {
				if (optuio->uio_segflg == UIO_SYSSPACE)
					bcopy(errmsg,
					   optuio->uio_iov[errmsg_pos].iov_base,
					   errmsg_len);
				else
					copyout(errmsg,
					   optuio->uio_iov[errmsg_pos].iov_base,
					   errmsg_len);
			}
		}
	}
 done_free:
#ifdef INET
	free(ip4, M_PRISON);
#endif
#ifdef INET6
	free(ip6, M_PRISON);
#endif
	vfs_freeopts(opts);
	return (error);
}

/*
 * Sysctl nodes to describe jail parameters.  Maximum length of string
 * parameters is returned in the string itself, and the other parameters
 * exist merely to make themselves and their types known.
 */
SYSCTL_NODE(_security_jail, OID_AUTO, param, CTLFLAG_RW, 0,
    "Jail parameters");

int
sysctl_jail_param(SYSCTL_HANDLER_ARGS)
{
	int i;
	long l;
	size_t s;
	char numbuf[12];

	switch (oidp->oid_kind & CTLTYPE)
	{
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
		l = 0;
#ifdef SCTL_MASK32
		if (!(req->flags & SCTL_MASK32))
#endif
			return (SYSCTL_OUT(req, &l, sizeof(l)));
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
		i = 0;
		return (SYSCTL_OUT(req, &i, sizeof(i)));
	case CTLTYPE_STRING:
		snprintf(numbuf, sizeof(numbuf), "%d", arg2);
		return
		    (sysctl_handle_string(oidp, numbuf, sizeof(numbuf), req));
	case CTLTYPE_STRUCT:
		s = (size_t)arg2;
		return (SYSCTL_OUT(req, &s, sizeof(s)));
	}
	return (0);
}

SYSCTL_JAIL_PARAM(, jid, CTLTYPE_INT | CTLFLAG_RD, "I", "Jail ID");
SYSCTL_JAIL_PARAM_STRING(, name, CTLFLAG_RW, MAXHOSTNAMELEN, "Jail name");
SYSCTL_JAIL_PARAM(, cpuset, CTLTYPE_INT | CTLFLAG_RD, "I", "Jail cpuset ID");
SYSCTL_JAIL_PARAM_STRING(, path, CTLFLAG_RD, MAXPATHLEN, "Jail root path");
SYSCTL_JAIL_PARAM(, securelevel, CTLTYPE_INT | CTLFLAG_RW,
    "I", "Jail secure level");
SYSCTL_JAIL_PARAM(, persist, CTLTYPE_INT | CTLFLAG_RW,
    "B", "Jail persistence");
SYSCTL_JAIL_PARAM(, dying, CTLTYPE_INT | CTLFLAG_RD,
    "B", "Jail is in the process of shutting down");

SYSCTL_JAIL_PARAM_NODE(host, "Jail host info");
SYSCTL_JAIL_PARAM_STRING(_host, hostname, CTLFLAG_RW, MAXHOSTNAMELEN,
    "Jail hostname");

#ifdef INET
SYSCTL_JAIL_PARAM_NODE(ip4, "Jail IPv4 address virtualization");
SYSCTL_JAIL_PARAM_STRUCT(_ip4, addr, CTLFLAG_RW, sizeof(struct in_addr),
    "S,in_addr,a", "Jail IPv4 addresses");
#endif
#ifdef INET6
SYSCTL_JAIL_PARAM_NODE(ip6, "Jail IPv6 address virtualization");
SYSCTL_JAIL_PARAM_STRUCT(_ip6, addr, CTLFLAG_RW, sizeof(struct in6_addr),
    "S,in6_addr,a", "Jail IPv6 addresses");
#endif


/*
 * struct jail_get_args {
 *	struct iovec *iovp;
 *	unsigned int iovcnt;
 *	int flags;
 * };
 */
int
jail_get(struct thread *td, struct jail_get_args *uap)
{
	struct uio *auio;
	int error;

	/* Check that we have an even number of iovecs. */
	if (uap->iovcnt & 1)
		return (EINVAL);

	error = copyinuio(uap->iovp, uap->iovcnt, &auio);
	if (error)
		return (error);
	error = kern_jail_get(td, auio, uap->flags);
	if (error == 0)
		error = copyout(auio->uio_iov, uap->iovp,
		    uap->iovcnt * sizeof (struct iovec));
	free(auio, M_IOV);
	return (error);
}

int
kern_jail_get(struct thread *td, struct uio *optuio, int flags)
{
	struct prison *pr;
	struct vfsopt *opt;
	struct vfsoptlist *opts;
	char *errmsg, *name;
	int error, errmsg_len, errmsg_pos, i, jid, len, locked, pos;

	if (flags & ~JAIL_GET_MASK)
		return (EINVAL);
	if (jailed(td->td_ucred)) {
		/*
		 * Don't allow a jailed process to see any jails,
		 * not even its own.
		 */
		vfs_opterror(opts, "jail not found");
		return (ENOENT);
	}

	/* Get the parameter list. */
	error = vfs_buildopts(optuio, &opts);
	if (error)
		return (error);
	errmsg_pos = vfs_getopt_pos(opts, "errmsg");

	/*
	 * Find the prison specified by one of: lastjid, jid, name.
	 */
	sx_slock(&allprison_lock);
	error = vfs_copyopt(opts, "lastjid", &jid, sizeof(jid));
	if (error == 0) {
		TAILQ_FOREACH(pr, &allprison, pr_list) {
			if (pr->pr_id > jid) {
				mtx_lock(&pr->pr_mtx);
				if (pr->pr_ref > 0 &&
				    (pr->pr_uref > 0 || (flags & JAIL_DYING)))
					break;
				mtx_unlock(&pr->pr_mtx);
			}
		}
		if (pr != NULL)
			goto found_prison;
		error = ENOENT;
		vfs_opterror(opts, "no jail after %d", jid);
		goto done_unlock_list;
	} else if (error != ENOENT)
		goto done_unlock_list;

	error = vfs_copyopt(opts, "jid", &jid, sizeof(jid));
	if (error == 0) {
		if (jid != 0) {
			pr = prison_find(jid);
			if (pr != NULL) {
				if (pr->pr_uref == 0 && !(flags & JAIL_DYING)) {
					mtx_unlock(&pr->pr_mtx);
					error = ENOENT;
					vfs_opterror(opts, "jail %d is dying",
					    jid);
					goto done_unlock_list;
				}
				goto found_prison;
			}
			error = ENOENT;
			vfs_opterror(opts, "jail %d not found", jid);
			goto done_unlock_list;
		}
	} else if (error != ENOENT)
		goto done_unlock_list;

	error = vfs_getopt(opts, "name", (void **)&name, &len);
	if (error == 0) {
		if (len == 0 || name[len - 1] != '\0') {
			error = EINVAL;
			goto done_unlock_list;
		}
		pr = prison_find_name(name);
		if (pr != NULL) {
			if (pr->pr_uref == 0 && !(flags & JAIL_DYING)) {
				mtx_unlock(&pr->pr_mtx);
				error = ENOENT;
				vfs_opterror(opts, "jail \"%s\" is dying",
				    name);
				goto done_unlock_list;
			}
			goto found_prison;
		}
		error = ENOENT;
		vfs_opterror(opts, "jail \"%s\" not found", name);
		goto done_unlock_list;
	} else if (error != ENOENT)
		goto done_unlock_list;

	vfs_opterror(opts, "no jail specified");
	error = ENOENT;
	goto done_unlock_list;

 found_prison:
	/* Get the parameters of the prison. */
	pr->pr_ref++;
	locked = PD_LOCKED;
	td->td_retval[0] = pr->pr_id;
	error = vfs_setopt(opts, "jid", &pr->pr_id, sizeof(pr->pr_id));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	error = vfs_setopts(opts, "name", pr->pr_name);
	if (error != 0 && error != ENOENT)
		goto done_deref;
	error = vfs_setopt(opts, "cpuset", &pr->pr_cpuset->cs_id,
	    sizeof(pr->pr_cpuset->cs_id));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	error = vfs_setopts(opts, "path", pr->pr_path);
	if (error != 0 && error != ENOENT)
		goto done_deref;
#ifdef INET
	error = vfs_setopt_part(opts, "ip4.addr", pr->pr_ip4,
	    pr->pr_ip4s * sizeof(*pr->pr_ip4));
	if (error != 0 && error != ENOENT)
		goto done_deref;
#endif
#ifdef INET6
	error = vfs_setopt_part(opts, "ip6.addr", pr->pr_ip6,
	    pr->pr_ip6s * sizeof(*pr->pr_ip6));
	if (error != 0 && error != ENOENT)
		goto done_deref;
#endif
	error = vfs_setopt(opts, "securelevel", &pr->pr_securelevel,
	    sizeof(pr->pr_securelevel));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	error = vfs_setopts(opts, "host.hostname", pr->pr_host);
	if (error != 0 && error != ENOENT)
		goto done_deref;
	i = pr->pr_flags & PR_PERSIST ? 1 : 0;
	error = vfs_setopt(opts, "persist", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	i = !i;
	error = vfs_setopt(opts, "nopersist", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	i = (pr->pr_uref == 0);
	error = vfs_setopt(opts, "dying", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done_deref;
	i = !i;
	error = vfs_setopt(opts, "nodying", &i, sizeof(i));
	if (error != 0 && error != ENOENT)
		goto done_deref;

	/* Get the module parameters. */
	mtx_unlock(&pr->pr_mtx);
	locked = 0;
	error = osd_jail_call(pr, PR_METHOD_GET, opts);
	if (error)
		goto done_deref;
	prison_deref(pr, PD_DEREF | PD_LIST_SLOCKED);

	/* By now, all parameters should have been noted. */
	TAILQ_FOREACH(opt, opts, link) {
		if (!opt->seen && strcmp(opt->name, "errmsg")) {
			error = EINVAL;
			vfs_opterror(opts, "unknown parameter: %s", opt->name);
			goto done_errmsg;
		}
	}

	/* Write the fetched parameters back to userspace. */
	error = 0;
	TAILQ_FOREACH(opt, opts, link) {
		if (opt->pos >= 0 && opt->pos != errmsg_pos) {
			pos = 2 * opt->pos + 1;
			optuio->uio_iov[pos].iov_len = opt->len;
			if (opt->value != NULL) {
				if (optuio->uio_segflg == UIO_SYSSPACE) {
					bcopy(opt->value,
					    optuio->uio_iov[pos].iov_base,
					    opt->len);
				} else {
					error = copyout(opt->value,
					    optuio->uio_iov[pos].iov_base,
					    opt->len);
					if (error)
						break;
				}
			}
		}
	}
	goto done_errmsg;

 done_deref:
	prison_deref(pr, locked | PD_DEREF | PD_LIST_SLOCKED);
	goto done_errmsg;

 done_unlock_list:
	sx_sunlock(&allprison_lock);
 done_errmsg:
	if (error && errmsg_pos >= 0) {
		vfs_getopt(opts, "errmsg", (void **)&errmsg, &errmsg_len);
		errmsg_pos = 2 * errmsg_pos + 1;
		if (errmsg_len > 0) {
			if (optuio->uio_segflg == UIO_SYSSPACE)
				bcopy(errmsg,
				    optuio->uio_iov[errmsg_pos].iov_base,
				    errmsg_len);
			else
				copyout(errmsg,
				    optuio->uio_iov[errmsg_pos].iov_base,
				    errmsg_len);
		}
	}
	vfs_freeopts(opts);
	return (error);
}

/*
 * struct jail_remove_args {
 *	int jid;
 * };
 */
int
jail_remove(struct thread *td, struct jail_remove_args *uap)
{
	struct prison *pr;
	struct proc *p;
	int deuref, error;

	error = priv_check(td, PRIV_JAIL_REMOVE);
	if (error)
		return (error);

	sx_xlock(&allprison_lock);
	pr = prison_find(uap->jid);
	if (pr == NULL) {
		sx_xunlock(&allprison_lock);
		return (EINVAL);
	}

	/* If the prison was persistent, it is not anymore. */
	deuref = 0;
	if (pr->pr_flags & PR_PERSIST) {
		pr->pr_ref--;
		deuref = PD_DEUREF;
		pr->pr_flags &= ~PR_PERSIST;
	}

	/* If there are no references left, remove the prison now. */
	if (pr->pr_ref == 0) {
		prison_deref(pr,
		    deuref | PD_DEREF | PD_LOCKED | PD_LIST_XLOCKED);
		return (0);
	}

	/*
	 * Keep a temporary reference to make sure this prison sticks around.
	 */
	pr->pr_ref++;
	mtx_unlock(&pr->pr_mtx);
	sx_xunlock(&allprison_lock);
	/*
	 * Kill all processes unfortunate enough to be attached to this prison.
	 */
	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		PROC_LOCK(p);
		if (p->p_state != PRS_NEW && p->p_ucred &&
		    p->p_ucred->cr_prison == pr)
			psignal(p, SIGKILL);
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);
	/* Remove the temporary reference. */
	prison_deref(pr, deuref | PD_DEREF);
	return (0);
}


/*
 * struct jail_attach_args {
 *	int jid;
 * };
 */
int
jail_attach(struct thread *td, struct jail_attach_args *uap)
{
	struct prison *pr;
	int error;

	error = priv_check(td, PRIV_JAIL_ATTACH);
	if (error)
		return (error);

	sx_slock(&allprison_lock);
	pr = prison_find(uap->jid);
	if (pr == NULL) {
		sx_sunlock(&allprison_lock);
		return (EINVAL);
	}

	/*
	 * Do not allow a process to attach to a prison that is not
	 * considered to be "alive".
	 */
	if (pr->pr_uref == 0) {
		mtx_unlock(&pr->pr_mtx);
		sx_sunlock(&allprison_lock);
		return (EINVAL);
	}

	return (do_jail_attach(td, pr));
}

static int
do_jail_attach(struct thread *td, struct prison *pr)
{
	struct proc *p;
	struct ucred *newcred, *oldcred;
	int vfslocked, error;

	/*
	 * XXX: Note that there is a slight race here if two threads
	 * in the same privileged process attempt to attach to two
	 * different jails at the same time.  It is important for
	 * user processes not to do this, or they might end up with
	 * a process root from one prison, but attached to the jail
	 * of another.
	 */
	pr->pr_ref++;
	pr->pr_uref++;
	mtx_unlock(&pr->pr_mtx);

	/* Let modules do whatever they need to prepare for attaching. */
	error = osd_jail_call(pr, PR_METHOD_ATTACH, td);
	if (error) {
		prison_deref(pr, PD_DEREF | PD_DEUREF | PD_LIST_SLOCKED);
		return (error);
	}
	sx_sunlock(&allprison_lock);

	/*
	 * Reparent the newly attached process to this jail.
	 */
	p = td->td_proc;
	error = cpuset_setproc_update_set(p, pr->pr_cpuset);
	if (error)
		goto e_revert_osd;

	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vn_lock(pr->pr_root, LK_EXCLUSIVE | LK_RETRY);
	if ((error = change_dir(pr->pr_root, td)) != 0)
		goto e_unlock;
#ifdef MAC
	if ((error = mac_vnode_check_chroot(td->td_ucred, pr->pr_root)))
		goto e_unlock;
#endif
	VOP_UNLOCK(pr->pr_root, 0);
	if ((error = change_root(pr->pr_root, td)))
		goto e_unlock_giant;
	VFS_UNLOCK_GIANT(vfslocked);

	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	setsugid(p);
	crcopy(newcred, oldcred);
	newcred->cr_prison = pr;
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
 e_unlock:
	VOP_UNLOCK(pr->pr_root, 0);
 e_unlock_giant:
	VFS_UNLOCK_GIANT(vfslocked);
 e_revert_osd:
	/* Tell modules this thread is still in its old jail after all. */
	(void)osd_jail_call(td->td_ucred->cr_prison, PR_METHOD_ATTACH, td);
	prison_deref(pr, PD_DEREF | PD_DEUREF);
	return (error);
}

/*
 * Returns a locked prison instance, or NULL on failure.
 */
struct prison *
prison_find(int prid)
{
	struct prison *pr;

	sx_assert(&allprison_lock, SX_LOCKED);
	TAILQ_FOREACH(pr, &allprison, pr_list) {
		if (pr->pr_id == prid) {
			mtx_lock(&pr->pr_mtx);
			if (pr->pr_ref > 0)
				return (pr);
			mtx_unlock(&pr->pr_mtx);
		}
	}
	return (NULL);
}

/*
 * Look for the named prison.  Returns a locked prison or NULL.
 */
struct prison *
prison_find_name(const char *name)
{
	struct prison *pr, *deadpr;

	sx_assert(&allprison_lock, SX_LOCKED);
 again:
	deadpr = NULL;
	TAILQ_FOREACH(pr, &allprison, pr_list) {
		if (!strcmp(pr->pr_name, name)) {
			mtx_lock(&pr->pr_mtx);
			if (pr->pr_ref > 0) {
				if (pr->pr_uref > 0)
					return (pr);
				deadpr = pr;
			}
			mtx_unlock(&pr->pr_mtx);
		}
	}
	/* There was no valid prison - perhaps there was a dying one */
	if (deadpr != NULL) {
		mtx_lock(&deadpr->pr_mtx);
		if (deadpr->pr_ref == 0) {
			mtx_unlock(&deadpr->pr_mtx);
			goto again;
		}
	}
	return (deadpr);
}

/*
 * Remove a prison reference.  If that was the last reference, remove the
 * prison itself - but not in this context in case there are locks held.
 */
void
prison_free_locked(struct prison *pr)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	pr->pr_ref--;
	if (pr->pr_ref == 0) {
		mtx_unlock(&pr->pr_mtx);
		TASK_INIT(&pr->pr_task, 0, prison_complete, pr);
		taskqueue_enqueue(taskqueue_thread, &pr->pr_task);
		return;
	}
	mtx_unlock(&pr->pr_mtx);
}

void
prison_free(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	prison_free_locked(pr);
}

static void
prison_complete(void *context, int pending)
{

	prison_deref((struct prison *)context, 0);
}

/*
 * Remove a prison reference (usually).  This internal version assumes no
 * mutexes are held, except perhaps the prison itself.  If there are no more
 * references, release and delist the prison.  On completion, the prison lock
 * and the allprison lock are both unlocked.
 */
static void
prison_deref(struct prison *pr, int flags)
{
	int vfslocked;

	if (!(flags & PD_LOCKED))
		mtx_lock(&pr->pr_mtx);
	if (flags & PD_DEUREF) {
		pr->pr_uref--;
		/* Done if there were only user references to remove. */
		if (!(flags & PD_DEREF)) {
			mtx_unlock(&pr->pr_mtx);
			if (flags & PD_LIST_SLOCKED)
				sx_sunlock(&allprison_lock);
			else if (flags & PD_LIST_XLOCKED)
				sx_xunlock(&allprison_lock);
			return;
		}
	}
	if (flags & PD_DEREF)
		pr->pr_ref--;
	/* If the prison still has references, nothing else to do. */
	if (pr->pr_ref > 0) {
		mtx_unlock(&pr->pr_mtx);
		if (flags & PD_LIST_SLOCKED)
			sx_sunlock(&allprison_lock);
		else if (flags & PD_LIST_XLOCKED)
			sx_xunlock(&allprison_lock);
		return;
	}

	KASSERT(pr->pr_uref == 0,
	    ("%s: Trying to remove an active prison (jid=%d).", __func__,
	    pr->pr_id));
	mtx_unlock(&pr->pr_mtx);
	if (flags & PD_LIST_SLOCKED) {
		if (!sx_try_upgrade(&allprison_lock)) {
			sx_sunlock(&allprison_lock);
			sx_xlock(&allprison_lock);
		}
	} else if (!(flags & PD_LIST_XLOCKED))
		sx_xlock(&allprison_lock);

	TAILQ_REMOVE(&allprison, pr, pr_list);
	prisoncount--;
	sx_xunlock(&allprison_lock);

	if (pr->pr_root != NULL) {
		vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
		vrele(pr->pr_root);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	mtx_destroy(&pr->pr_mtx);
	free(pr->pr_linux, M_PRISON);
#ifdef INET
	free(pr->pr_ip4, M_PRISON);
#endif
#ifdef INET6
	free(pr->pr_ip6, M_PRISON);
#endif
	if (pr->pr_cpuset != NULL)
		cpuset_rel(pr->pr_cpuset);
	osd_jail_exit(pr);
	free(pr, M_PRISON);
}

void
prison_hold_locked(struct prison *pr)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	KASSERT(pr->pr_ref > 0,
	    ("Trying to hold dead prison (jid=%d).", pr->pr_id));
	pr->pr_ref++;
}

void
prison_hold(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	prison_hold_locked(pr);
	mtx_unlock(&pr->pr_mtx);
}

void
prison_proc_hold(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	KASSERT(pr->pr_uref > 0,
	    ("Cannot add a process to a non-alive prison (jid=%d)", pr->pr_id));
	pr->pr_uref++;
	mtx_unlock(&pr->pr_mtx);
}

void
prison_proc_free(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	KASSERT(pr->pr_uref > 0,
	    ("Trying to kill a process in a dead prison (jid=%d)", pr->pr_id));
	prison_deref(pr, PD_DEUREF | PD_LOCKED);
}


#ifdef INET
/*
 * Pass back primary IPv4 address of this jail.
 *
 * If not jailed return success but do not alter the address.  Caller has to
 * make sure to initialize it correctly (e.g. INADDR_ANY).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address returned in NBO.
 */
int
prison_get_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip4 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	ia->s_addr = pr->pr_ip4[0].s_addr;
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

/*
 * Make sure our (source) address is set to something meaningful to this
 * jail.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO and returned in NBO.
 */
int
prison_local_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;
	struct in_addr ia0;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip4 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	ia0.s_addr = ntohl(ia->s_addr);
	if (ia0.s_addr == INADDR_LOOPBACK) {
		ia->s_addr = pr->pr_ip4[0].s_addr;
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	if (ia0.s_addr == INADDR_ANY) {
		/*
		 * In case there is only 1 IPv4 address, bind directly.
		 */
		if (pr->pr_ip4s == 1)
			ia->s_addr = pr->pr_ip4[0].s_addr;
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	error = _prison_check_ip4(pr, ia);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

/*
 * Rewrite destination address in case we will connect to loopback address.
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO and returned in NBO.
 */
int
prison_remote_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip4 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (ntohl(ia->s_addr) == INADDR_LOOPBACK) {
		ia->s_addr = pr->pr_ip4[0].s_addr;
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	/*
	 * Return success because nothing had to be changed.
	 */
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

/*
 * Check if given address belongs to the jail referenced by cred/prison.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO.
 */
static int
_prison_check_ip4(struct prison *pr, struct in_addr *ia)
{
	int i, a, z, d;

	/*
	 * Check the primary IP.
	 */
	if (pr->pr_ip4[0].s_addr == ia->s_addr)
		return (0);

	/*
	 * All the other IPs are sorted so we can do a binary search.
	 */
	a = 0;
	z = pr->pr_ip4s - 2;
	while (a <= z) {
		i = (a + z) / 2;
		d = qcmp_v4(&pr->pr_ip4[i+1], ia);
		if (d > 0)
			z = i - 1;
		else if (d < 0)
			a = i + 1;
		else
			return (0);
	}

	return (EADDRNOTAVAIL);
}

int
prison_check_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip4 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	error = _prison_check_ip4(pr, ia);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}
#endif

#ifdef INET6
/*
 * Pass back primary IPv6 address for this jail.
 *
 * If not jailed return success but do not alter the address.  Caller has to
 * make sure to initialize it correctly (e.g. IN6ADDR_ANY_INIT).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_get_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip6 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	bcopy(&pr->pr_ip6[0], ia6, sizeof(struct in6_addr));
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

/*
 * Make sure our (source) address is set to something meaningful to this jail.
 *
 * v6only should be set based on (inp->inp_flags & IN6P_IPV6_V6ONLY != 0)
 * when needed while binding.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_local_ip6(struct ucred *cred, struct in6_addr *ia6, int v6only)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip6 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (IN6_IS_ADDR_LOOPBACK(ia6)) {
		bcopy(&pr->pr_ip6[0], ia6, sizeof(struct in6_addr));
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	if (IN6_IS_ADDR_UNSPECIFIED(ia6)) {
		/*
		 * In case there is only 1 IPv6 address, and v6only is true,
		 * then bind directly.
		 */
		if (v6only != 0 && pr->pr_ip6s == 1)
			bcopy(&pr->pr_ip6[0], ia6, sizeof(struct in6_addr));
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	error = _prison_check_ip6(pr, ia6);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

/*
 * Rewrite destination address in case we will connect to loopback address.
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_remote_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip6 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (IN6_IS_ADDR_LOOPBACK(ia6)) {
		bcopy(&pr->pr_ip6[0], ia6, sizeof(struct in6_addr));
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	/*
	 * Return success because nothing had to be changed.
	 */
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

/*
 * Check if given address belongs to the jail referenced by cred/prison.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
static int
_prison_check_ip6(struct prison *pr, struct in6_addr *ia6)
{
	int i, a, z, d;

	/*
	 * Check the primary IP.
	 */
	if (IN6_ARE_ADDR_EQUAL(&pr->pr_ip6[0], ia6))
		return (0);

	/*
	 * All the other IPs are sorted so we can do a binary search.
	 */
	a = 0;
	z = pr->pr_ip6s - 2;
	while (a <= z) {
		i = (a + z) / 2;
		d = qcmp_v6(&pr->pr_ip6[i+1], ia6);
		if (d > 0)
			z = i - 1;
		else if (d < 0)
			a = i + 1;
		else
			return (0);
	}

	return (EADDRNOTAVAIL);
}

int
prison_check_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	pr = cred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	if (pr->pr_ip6 == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	error = _prison_check_ip6(pr, ia6);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}
#endif

/*
 * Check if a jail supports the given address family.
 *
 * Returns 0 if not jailed or the address family is supported, EAFNOSUPPORT
 * if not.
 */
int
prison_check_af(struct ucred *cred, int af)
{
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));


	if (!jailed(cred))
		return (0);

	error = 0;
	switch (af)
	{
#ifdef INET
	case AF_INET:
		if (cred->cr_prison->pr_ip4 == NULL)
			error = EAFNOSUPPORT;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (cred->cr_prison->pr_ip6 == NULL)
			error = EAFNOSUPPORT;
		break;
#endif
	case AF_LOCAL:
	case AF_ROUTE:
		break;
	default:
		if (jail_socket_unixiproute_only)
			error = EAFNOSUPPORT;
	}
	return (error);
}

/*
 * Check if given address belongs to the jail referenced by cred (wrapper to
 * prison_check_ip[46]).
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow
 * the address family.  IPv4 Address passed in in NBO.
 */
int
prison_if(struct ucred *cred, struct sockaddr *sa)
{
#ifdef INET
	struct sockaddr_in *sai;
#endif
#ifdef INET6
	struct sockaddr_in6 *sai6;
#endif
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(sa != NULL, ("%s: sa is NULL", __func__));

	error = 0;
	switch (sa->sa_family)
	{
#ifdef INET
	case AF_INET:
		sai = (struct sockaddr_in *)sa;
		error = prison_check_ip4(cred, &sai->sin_addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sai6 = (struct sockaddr_in6 *)sa;
		error = prison_check_ip6(cred, &sai6->sin6_addr);
		break;
#endif
	default:
		if (jailed(cred) && jail_socket_unixiproute_only)
			error = EAFNOSUPPORT;
	}
	return (error);
}

/*
 * Return 0 if jails permit p1 to frob p2, otherwise ESRCH.
 */
int
prison_check(struct ucred *cred1, struct ucred *cred2)
{

	if (jailed(cred1)) {
		if (!jailed(cred2))
			return (ESRCH);
		if (cred2->cr_prison != cred1->cr_prison)
			return (ESRCH);
	}

	return (0);
}

/*
 * Return 1 if the passed credential is in a jail, otherwise 0.
 */
int
jailed(struct ucred *cred)
{

	return (cred->cr_prison != NULL);
}

/*
 * Return the correct hostname for the passed credential.
 */
void
getcredhostname(struct ucred *cred, char *buf, size_t size)
{
	INIT_VPROCG(cred->cr_vimage->v_procg);

	if (jailed(cred)) {
		mtx_lock(&cred->cr_prison->pr_mtx);
		strlcpy(buf, cred->cr_prison->pr_host, size);
		mtx_unlock(&cred->cr_prison->pr_mtx);
	} else {
		mtx_lock(&hostname_mtx);
		strlcpy(buf, V_hostname, size);
		mtx_unlock(&hostname_mtx);
	}
}

/*
 * Determine whether the subject represented by cred can "see"
 * status of a mount point.
 * Returns: 0 for permitted, ENOENT otherwise.
 * XXX: This function should be called cr_canseemount() and should be
 *      placed in kern_prot.c.
 */
int
prison_canseemount(struct ucred *cred, struct mount *mp)
{
	struct prison *pr;
	struct statfs *sp;
	size_t len;

	if (!jailed(cred) || jail_enforce_statfs == 0)
		return (0);
	pr = cred->cr_prison;
	if (pr->pr_root->v_mount == mp)
		return (0);
	if (jail_enforce_statfs == 2)
		return (ENOENT);
	/*
	 * If jail's chroot directory is set to "/" we should be able to see
	 * all mount-points from inside a jail.
	 * This is ugly check, but this is the only situation when jail's
	 * directory ends with '/'.
	 */
	if (strcmp(pr->pr_path, "/") == 0)
		return (0);
	len = strlen(pr->pr_path);
	sp = &mp->mnt_stat;
	if (strncmp(pr->pr_path, sp->f_mntonname, len) != 0)
		return (ENOENT);
	/*
	 * Be sure that we don't have situation where jail's root directory
	 * is "/some/path" and mount point is "/some/pathpath".
	 */
	if (sp->f_mntonname[len] != '\0' && sp->f_mntonname[len] != '/')
		return (ENOENT);
	return (0);
}

void
prison_enforce_statfs(struct ucred *cred, struct mount *mp, struct statfs *sp)
{
	char jpath[MAXPATHLEN];
	struct prison *pr;
	size_t len;

	if (!jailed(cred) || jail_enforce_statfs == 0)
		return;
	pr = cred->cr_prison;
	if (prison_canseemount(cred, mp) != 0) {
		bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
		strlcpy(sp->f_mntonname, "[restricted]",
		    sizeof(sp->f_mntonname));
		return;
	}
	if (pr->pr_root->v_mount == mp) {
		/*
		 * Clear current buffer data, so we are sure nothing from
		 * the valid path left there.
		 */
		bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
		*sp->f_mntonname = '/';
		return;
	}
	/*
	 * If jail's chroot directory is set to "/" we should be able to see
	 * all mount-points from inside a jail.
	 */
	if (strcmp(pr->pr_path, "/") == 0)
		return;
	len = strlen(pr->pr_path);
	strlcpy(jpath, sp->f_mntonname + len, sizeof(jpath));
	/*
	 * Clear current buffer data, so we are sure nothing from
	 * the valid path left there.
	 */
	bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
	if (*jpath == '\0') {
		/* Should never happen. */
		*sp->f_mntonname = '/';
	} else {
		strlcpy(sp->f_mntonname, jpath, sizeof(sp->f_mntonname));
	}
}

/*
 * Check with permission for a specific privilege is granted within jail.  We
 * have a specific list of accepted privileges; the rest are denied.
 */
int
prison_priv_check(struct ucred *cred, int priv)
{

	if (!jailed(cred))
		return (0);

	switch (priv) {

		/*
		 * Allow ktrace privileges for root in jail.
		 */
	case PRIV_KTRACE:

#if 0
		/*
		 * Allow jailed processes to configure audit identity and
		 * submit audit records (login, etc).  In the future we may
		 * want to further refine the relationship between audit and
		 * jail.
		 */
	case PRIV_AUDIT_GETAUDIT:
	case PRIV_AUDIT_SETAUDIT:
	case PRIV_AUDIT_SUBMIT:
#endif

		/*
		 * Allow jailed processes to manipulate process UNIX
		 * credentials in any way they see fit.
		 */
	case PRIV_CRED_SETUID:
	case PRIV_CRED_SETEUID:
	case PRIV_CRED_SETGID:
	case PRIV_CRED_SETEGID:
	case PRIV_CRED_SETGROUPS:
	case PRIV_CRED_SETREUID:
	case PRIV_CRED_SETREGID:
	case PRIV_CRED_SETRESUID:
	case PRIV_CRED_SETRESGID:

		/*
		 * Jail implements visibility constraints already, so allow
		 * jailed root to override uid/gid-based constraints.
		 */
	case PRIV_SEEOTHERGIDS:
	case PRIV_SEEOTHERUIDS:

		/*
		 * Jail implements inter-process debugging limits already, so
		 * allow jailed root various debugging privileges.
		 */
	case PRIV_DEBUG_DIFFCRED:
	case PRIV_DEBUG_SUGID:
	case PRIV_DEBUG_UNPRIV:

		/*
		 * Allow jail to set various resource limits and login
		 * properties, and for now, exceed process resource limits.
		 */
	case PRIV_PROC_LIMIT:
	case PRIV_PROC_SETLOGIN:
	case PRIV_PROC_SETRLIMIT:

		/*
		 * System V and POSIX IPC privileges are granted in jail.
		 */
	case PRIV_IPC_READ:
	case PRIV_IPC_WRITE:
	case PRIV_IPC_ADMIN:
	case PRIV_IPC_MSGSIZE:
	case PRIV_MQ_ADMIN:

		/*
		 * Jail implements its own inter-process limits, so allow
		 * root processes in jail to change scheduling on other
		 * processes in the same jail.  Likewise for signalling.
		 */
	case PRIV_SCHED_DIFFCRED:
	case PRIV_SCHED_CPUSET:
	case PRIV_SIGNAL_DIFFCRED:
	case PRIV_SIGNAL_SUGID:

		/*
		 * Allow jailed processes to write to sysctls marked as jail
		 * writable.
		 */
	case PRIV_SYSCTL_WRITEJAIL:

		/*
		 * Allow root in jail to manage a variety of quota
		 * properties.  These should likely be conditional on a
		 * configuration option.
		 */
	case PRIV_VFS_GETQUOTA:
	case PRIV_VFS_SETQUOTA:

		/*
		 * Since Jail relies on chroot() to implement file system
		 * protections, grant many VFS privileges to root in jail.
		 * Be careful to exclude mount-related and NFS-related
		 * privileges.
		 */
	case PRIV_VFS_READ:
	case PRIV_VFS_WRITE:
	case PRIV_VFS_ADMIN:
	case PRIV_VFS_EXEC:
	case PRIV_VFS_LOOKUP:
	case PRIV_VFS_BLOCKRESERVE:	/* XXXRW: Slightly surprising. */
	case PRIV_VFS_CHFLAGS_DEV:
	case PRIV_VFS_CHOWN:
	case PRIV_VFS_CHROOT:
	case PRIV_VFS_RETAINSUGID:
	case PRIV_VFS_FCHROOT:
	case PRIV_VFS_LINK:
	case PRIV_VFS_SETGID:
	case PRIV_VFS_STAT:
	case PRIV_VFS_STICKYFILE:
		return (0);

		/*
		 * Depending on the global setting, allow privilege of
		 * setting system flags.
		 */
	case PRIV_VFS_SYSFLAGS:
		if (jail_chflags_allowed)
			return (0);
		else
			return (EPERM);

		/*
		 * Depending on the global setting, allow privilege of
		 * mounting/unmounting file systems.
		 */
	case PRIV_VFS_MOUNT:
	case PRIV_VFS_UNMOUNT:
	case PRIV_VFS_MOUNT_NONUSER:
	case PRIV_VFS_MOUNT_OWNER:
		if (jail_mount_allowed)
			return (0);
		else
			return (EPERM);

		/*
		 * Allow jailed root to bind reserved ports and reuse in-use
		 * ports.
		 */
	case PRIV_NETINET_RESERVEDPORT:
	case PRIV_NETINET_REUSEPORT:
		return (0);

		/*
		 * Allow jailed root to set certian IPv4/6 (option) headers.
		 */
	case PRIV_NETINET_SETHDROPTS:
		return (0);

		/*
		 * Conditionally allow creating raw sockets in jail.
		 */
	case PRIV_NETINET_RAW:
		if (jail_allow_raw_sockets)
			return (0);
		else
			return (EPERM);

		/*
		 * Since jail implements its own visibility limits on netstat
		 * sysctls, allow getcred.  This allows identd to work in
		 * jail.
		 */
	case PRIV_NETINET_GETCRED:
		return (0);

	default:
		/*
		 * In all remaining cases, deny the privilege request.  This
		 * includes almost all network privileges, many system
		 * configuration privileges.
		 */
		return (EPERM);
	}
}

static int
sysctl_jail_list(SYSCTL_HANDLER_ARGS)
{
	struct xprison *xp;
	struct prison *pr;
#ifdef INET
	struct in_addr *ip4 = NULL;
	int ip4s = 0;
#endif
#ifdef INET6
	struct in_addr *ip6 = NULL;
	int ip6s = 0;
#endif
	int error;

	if (jailed(req->td->td_ucred))
		return (0);

	xp = malloc(sizeof(*xp), M_TEMP, M_WAITOK);
	error = 0;
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list) {
 again:
		mtx_lock(&pr->pr_mtx);
#ifdef INET
		if (pr->pr_ip4s > 0) {
			if (ip4s < pr->pr_ip4s) {
				ip4s = pr->pr_ip4s;
				mtx_unlock(&pr->pr_mtx);
				ip4 = realloc(ip4, ip4s *
				    sizeof(struct in_addr), M_TEMP, M_WAITOK);
				goto again;
			}
			bcopy(pr->pr_ip4, ip4,
			    pr->pr_ip4s * sizeof(struct in_addr));
		}
#endif
#ifdef INET6
		if (pr->pr_ip6s > 0) {
			if (ip6s < pr->pr_ip6s) {
				ip6s = pr->pr_ip6s;
				mtx_unlock(&pr->pr_mtx);
				ip6 = realloc(ip6, ip6s *
				    sizeof(struct in6_addr), M_TEMP, M_WAITOK);
				goto again;
			}
			bcopy(pr->pr_ip6, ip6,
			    pr->pr_ip6s * sizeof(struct in6_addr));
		}
#endif
		if (pr->pr_ref == 0) {
			mtx_unlock(&pr->pr_mtx);
			continue;
		}
		bzero(xp, sizeof(*xp));
		xp->pr_version = XPRISON_VERSION;
		xp->pr_id = pr->pr_id;
		xp->pr_state = pr->pr_uref > 0
		    ? PRISON_STATE_ALIVE : PRISON_STATE_DYING;
		strlcpy(xp->pr_path, pr->pr_path, sizeof(xp->pr_path));
		strlcpy(xp->pr_host, pr->pr_host, sizeof(xp->pr_host));
		strlcpy(xp->pr_name, pr->pr_name, sizeof(xp->pr_name));
#ifdef INET
		xp->pr_ip4s = pr->pr_ip4s;
#endif
#ifdef INET6
		xp->pr_ip6s = pr->pr_ip6s;
#endif
		mtx_unlock(&pr->pr_mtx);
		error = SYSCTL_OUT(req, xp, sizeof(*xp));
		if (error)
			break;
#ifdef INET
		if (xp->pr_ip4s > 0) {
			error = SYSCTL_OUT(req, ip4,
			    xp->pr_ip4s * sizeof(struct in_addr));
			if (error)
				break;
		}
#endif
#ifdef INET6
		if (xp->pr_ip6s > 0) {
			error = SYSCTL_OUT(req, ip6,
			    xp->pr_ip6s * sizeof(struct in6_addr));
			if (error)
				break;
		}
#endif
	}
	sx_sunlock(&allprison_lock);
	free(xp, M_TEMP);
#ifdef INET
	free(ip4, M_TEMP);
#endif
#ifdef INET6
	free(ip6, M_TEMP);
#endif
	return (error);
}

SYSCTL_OID(_security_jail, OID_AUTO, list,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_jail_list, "S", "List of active jails");

static int
sysctl_jail_jailed(SYSCTL_HANDLER_ARGS)
{
	int error, injail;

	injail = jailed(req->td->td_ucred);
	error = SYSCTL_OUT(req, &injail, sizeof(injail));

	return (error);
}
SYSCTL_PROC(_security_jail, OID_AUTO, jailed,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_jail_jailed, "I", "Process in jail?");

#ifdef DDB

static void
db_show_prison(struct prison *pr)
{
#if defined(INET) || defined(INET6)
	int ii;
#endif
#ifdef INET6
	char ip6buf[INET6_ADDRSTRLEN];
#endif

	db_printf("prison %p:\n", pr);
	db_printf(" jid             = %d\n", pr->pr_id);
	db_printf(" name            = %s\n", pr->pr_name);
	db_printf(" ref             = %d\n", pr->pr_ref);
	db_printf(" uref            = %d\n", pr->pr_uref);
	db_printf(" path            = %s\n", pr->pr_path);
	db_printf(" cpuset          = %d\n", pr->pr_cpuset
	    ? pr->pr_cpuset->cs_id : -1);
	db_printf(" root            = %p\n", pr->pr_root);
	db_printf(" securelevel     = %d\n", pr->pr_securelevel);
	db_printf(" flags           = %x", pr->pr_flags);
	if (pr->pr_flags & PR_PERSIST)
		db_printf(" persist");
	db_printf("\n");
	db_printf(" host.hostname   = %s\n", pr->pr_host);
#ifdef INET
	db_printf(" ip4s            = %d\n", pr->pr_ip4s);
	for (ii = 0; ii < pr->pr_ip4s; ii++)
		db_printf(" %s %s\n",
		    ii == 0 ? "ip4             =" : "                 ",
		    inet_ntoa(pr->pr_ip4[ii]));
#endif
#ifdef INET6
	db_printf(" ip6s            = %d\n", pr->pr_ip6s);
	for (ii = 0; ii < pr->pr_ip6s; ii++)
		db_printf(" %s %s\n",
		    ii == 0 ? "ip6             =" : "                 ",
		    ip6_sprintf(ip6buf, &pr->pr_ip6[ii]));
#endif
}

DB_SHOW_COMMAND(prison, db_show_prison_command)
{
	struct prison *pr;

	if (!have_addr) {
		/* Show all prisons in the list. */
		TAILQ_FOREACH(pr, &allprison, pr_list) {
			db_show_prison(pr);
			if (db_pager_quit)
				break;
		}
		return;
	}

	/* Look for a prison with the ID and with references. */
	TAILQ_FOREACH(pr, &allprison, pr_list)
		if (pr->pr_id == addr && pr->pr_ref > 0)
			break;
	if (pr == NULL)
		/* Look again, without requiring a reference. */
		TAILQ_FOREACH(pr, &allprison, pr_list)
			if (pr->pr_id == addr)
				break;
	if (pr == NULL)
		/* Assume address points to a valid prison. */
		pr = (struct prison *)addr;
	db_show_prison(pr);
}

#endif /* DDB */
