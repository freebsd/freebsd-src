/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Herb Hasler and Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /*not lint*/

#if 0
#ifndef lint
static char sccsid[] = "@(#)mountd.c	8.15 (Berkeley) 5/1/95";
#endif /*not lint*/
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/fnv_hash.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/mount.h>
#include <nfs/nfsproto.h>
#include <nfs/nfssvc.h>
#include <nfsserver/nfs.h>

#include <fs/nfs/nfsport.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <libutil.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"
#include "mntopts.h"

#ifdef DEBUG
#include <stdarg.h>
#endif

/*
 * Structures for keeping the mount list and export list
 */
struct mountlist {
	char	ml_host[MNTNAMLEN+1];
	char	ml_dirp[MNTPATHLEN+1];

	SLIST_ENTRY(mountlist)	next;
};

struct dirlist {
	struct dirlist	*dp_left;
	struct dirlist	*dp_right;
	int		dp_flag;
	struct hostlist	*dp_hosts;	/* List of hosts this dir exported to */
	char		*dp_dirp;
};
/* dp_flag bits */
#define	DP_DEFSET	0x1
#define DP_HOSTSET	0x2

/*
 * maproot/mapall credentials.
 * cr_smallgrps can be used for a group list up to SMALLNGROUPS in size.
 * Larger group lists are malloc'd/free'd.
 */
#define	SMALLNGROUPS	32
struct expcred {
	uid_t		cr_uid;
	int		cr_ngroups;
	gid_t		cr_smallgrps[SMALLNGROUPS];
	gid_t		*cr_groups;
};

struct exportlist {
	struct dirlist	*ex_dirl;
	struct dirlist	*ex_defdir;
	struct grouplist *ex_grphead;
	int		ex_flag;
	fsid_t		ex_fs;
	char		*ex_fsdir;
	char		*ex_indexfile;
	struct expcred	ex_defanon;
	uint64_t	ex_defexflags;
	int		ex_numsecflavors;
	int		ex_secflavors[MAXSECFLAVORS];
	int		ex_defnumsecflavors;
	int		ex_defsecflavors[MAXSECFLAVORS];

	SLIST_ENTRY(exportlist) entries;
};
/* ex_flag bits */
#define	EX_LINKED	0x1
#define	EX_DONE		0x2
#define	EX_DEFSET	0x4
#define	EX_PUBLICFH	0x8

SLIST_HEAD(exportlisthead, exportlist);

struct netmsk {
	struct sockaddr_storage nt_net;
	struct sockaddr_storage nt_mask;
	char		*nt_name;
};

union grouptypes {
	struct addrinfo *gt_addrinfo;
	struct netmsk	gt_net;
};

struct grouplist {
	int gr_type;
	union grouptypes gr_ptr;
	struct grouplist *gr_next;
	struct expcred gr_anon;
	uint64_t gr_exflags;
	int gr_flag;
	int gr_numsecflavors;
	int gr_secflavors[MAXSECFLAVORS];
};
/* Group types */
#define	GT_NULL		0x0
#define	GT_HOST		0x1
#define	GT_NET		0x2
#define	GT_DEFAULT	0x3
#define GT_IGNORE	0x5

/* Group flags */
#define	GR_FND		0x1

struct hostlist {
	int		 ht_flag;	/* Uses DP_xx bits */
	struct grouplist *ht_grp;
	struct hostlist	 *ht_next;
};

struct fhreturn {
	int	fhr_flag;
	int	fhr_vers;
	nfsfh_t	fhr_fh;
	int	fhr_numsecflavors;
	int	*fhr_secflavors;
};

#define	GETPORT_MAXTRY	20	/* Max tries to get a port # */

/*
 * How long to delay a reload of exports when there are RPC request(s)
 * to process, in usec.  Must be less than 1second.
 */
#define	RELOADDELAY	250000

/* Global defs */
static char	*add_expdir(struct dirlist **, char *, int);
static void	add_dlist(struct dirlist **, struct dirlist *,
		    struct grouplist *, int, struct exportlist *,
		    struct expcred *, uint64_t);
static void	add_mlist(char *, char *);
static int	check_path_component(const char *, char **);
static int	check_dirpath(char *, char **);
static int	check_statfs(const char *, struct statfs *, char **);
static int	check_options(struct dirlist *);
static int	checkmask(struct sockaddr *sa);
static int	chk_host(struct dirlist *, struct sockaddr *, int *, int *,
		    int *, int **);
static char	*strsep_quote(char **stringp, const char *delim);
static int	create_service(struct netconfig *nconf);
static void	complete_service(struct netconfig *nconf, char *port_str);
static void	clearout_service(void);
static void	del_mlist(char *hostp, char *dirp);
static struct dirlist	*dirp_search(struct dirlist *, char *);
static int	do_export_mount(struct exportlist *, struct statfs *);
static int	do_mount(struct exportlist *, struct grouplist *, uint64_t,
		    struct expcred *, char *, int, struct statfs *, int, int *);
static int	do_opt(char **, char **, struct exportlist *,
		    struct grouplist *, int *, uint64_t *, struct expcred *);
static struct exportlist	*ex_search(fsid_t *, struct exportlisthead *);
static struct exportlist	*get_exp(void);
static void	free_dir(struct dirlist *);
static void	free_exp(struct exportlist *);
static void	free_grp(struct grouplist *);
static void	free_host(struct hostlist *);
static void	free_v4rootexp(void);
static void	get_exportlist_one(int);
static void	get_exportlist(int);
static void	insert_exports(struct exportlist *, struct exportlisthead *);
static void	free_exports(struct exportlisthead *);
static void	read_exportfile(int);
static int	compare_nmount_exportlist(struct iovec *, int, char *);
static int	compare_export(struct exportlist *, struct exportlist *);
static int	compare_cred(struct expcred *, struct expcred *);
static int	compare_secflavor(int *, int *, int);
static void	delete_export(struct iovec *, int, struct statfs *, char *);
static int	get_host(char *, struct grouplist *, struct grouplist *);
static struct hostlist *get_ht(void);
static int	get_line(void);
static void	get_mountlist(void);
static int	get_net(char *, struct netmsk *, int);
static void	getexp_err(struct exportlist *, struct grouplist *, const char *);
static struct grouplist	*get_grp(void);
static void	hang_dirp(struct dirlist *, struct grouplist *,
		    struct exportlist *, int, struct expcred *, uint64_t);
static void	huphandler(int sig);
static int	makemask(struct sockaddr_storage *ssp, int bitlen);
static void	mntsrv(struct svc_req *, SVCXPRT *);
static void	nextfield(char **, char **);
static void	out_of_mem(void);
static void	parsecred(char *, struct expcred *);
static int	parsesec(char *, struct exportlist *);
static int	put_exlist(struct dirlist *, XDR *, struct dirlist *,
		    int *, int);
static void	*sa_rawaddr(struct sockaddr *sa, int *nbytes);
static int	sacmp(struct sockaddr *sa1, struct sockaddr *sa2,
		    struct sockaddr *samask);
static int	scan_tree(struct dirlist *, struct sockaddr *);
static void	usage(void);
static int	xdr_dir(XDR *, char *);
static int	xdr_explist(XDR *, caddr_t);
static int	xdr_explist_brief(XDR *, caddr_t);
static int	xdr_explist_common(XDR *, caddr_t, int);
static int	xdr_fhs(XDR *, caddr_t);
static int	xdr_mlist(XDR *, caddr_t);
static void	terminate(int);
static void	cp_cred(struct expcred *, struct expcred *);

#define	EXPHASH(f)	(fnv_32_buf((f), sizeof(fsid_t), 0) % exphashsize)
static struct exportlisthead *exphead = NULL;
static struct exportlisthead *oldexphead = NULL;
static int exphashsize = 0;
static SLIST_HEAD(, mountlist) mlhead = SLIST_HEAD_INITIALIZER(&mlhead);
static char *exnames_default[2] = { _PATH_EXPORTS, NULL };
static char **exnames;
static char **hosts = NULL;
static int force_v2 = 0;
static int resvport_only = 1;
static int nhosts = 0;
static int dir_only = 1;
static int dolog = 0;
static int got_sighup = 0;
static int xcreated = 0;

static char *svcport_str = NULL;
static int mallocd_svcport = 0;
static int *sock_fd;
static int sock_fdcnt;
static int sock_fdpos;
static int suspend_nfsd = 0;

static int opt_flags;
static int have_v6 = 1;

static int v4root_phase = 0;
static char v4root_dirpath[PATH_MAX + 1];
static struct exportlist *v4root_ep = NULL;
static int has_publicfh = 0;
static int has_set_publicfh = 0;

static struct pidfh *pfh = NULL;
/* Bits for opt_flags above */
#define	OP_MAPROOT	0x01
#define	OP_MAPALL	0x02
/* 0x4 free */
#define	OP_MASK		0x08
#define	OP_NET		0x10
#define	OP_ALLDIRS	0x40
#define	OP_HAVEMASK	0x80	/* A mask was specified or inferred. */
#define	OP_QUIET	0x100
#define OP_MASKLEN	0x200
#define OP_SEC		0x400

#ifdef DEBUG
static int debug = 1;
static void	SYSLOG(int, const char *, ...) __printflike(2, 3);
#define syslog SYSLOG
#else
static int debug = 0;
#endif

/*
 * The LOGDEBUG() syslog() calls are always compiled into the daemon.
 * To enable them, create a file at _PATH_MOUNTDDEBUG. This file can be empty.
 * To disable the logging, just delete the file at _PATH_MOUNTDDEBUG.
 */
static int logdebug = 0;
#define	LOGDEBUG(format, ...)						\
    (logdebug ? syslog(LOG_DEBUG, format, ## __VA_ARGS__) : 0)

/*
 * Similar to strsep(), but it allows for quoted strings
 * and escaped characters.
 *
 * It returns the string (or NULL, if *stringp is NULL),
 * which is a de-quoted version of the string if necessary.
 *
 * It modifies *stringp in place.
 */
static char *
strsep_quote(char **stringp, const char *delim)
{
	char *srcptr, *dstptr, *retval;
	char quot = 0;
	
	if (stringp == NULL || *stringp == NULL)
		return (NULL);

	srcptr = dstptr = retval = *stringp;

	while (*srcptr) {
		/*
		 * We're looking for several edge cases here.
		 * First:  if we're in quote state (quot != 0),
		 * then we ignore the delim characters, but otherwise
		 * process as normal, unless it is the quote character.
		 * Second:  if the current character is a backslash,
		 * we take the next character as-is, without checking
		 * for delim, quote, or backslash.  Exception:  if the
		 * next character is a NUL, that's the end of the string.
		 * Third:  if the character is a quote character, we toggle
		 * quote state.
		 * Otherwise:  check the current character for NUL, or
		 * being in delim, and end the string if either is true.
		 */
		if (*srcptr == '\\') {
			srcptr++;
			/*
			 * The edge case here is if the next character
			 * is NUL, we want to stop processing.  But if
			 * it's not NUL, then we simply want to copy it.
			 */
			if (*srcptr) {
				*dstptr++ = *srcptr++;
			}
			continue;
		}
		if (quot == 0 && (*srcptr == '\'' || *srcptr == '"')) {
			quot = *srcptr++;
			continue;
		}
		if (quot && *srcptr == quot) {
			/* End of the quoted part */
			quot = 0;
			srcptr++;
			continue;
		}
		if (!quot && strchr(delim, *srcptr))
			break;
		*dstptr++ = *srcptr++;
	}

	*stringp = (*srcptr == '\0') ? NULL : srcptr + 1;
	*dstptr = 0; /* Terminate the string */
	return (retval);
}

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * and "-n" to allow nonroot mount.
 */
int
main(int argc, char **argv)
{
	fd_set readfds;
	struct netconfig *nconf;
	char *endptr, **hosts_bak;
	void *nc_handle;
	pid_t otherpid;
	in_port_t svcport;
	int c, k, s;
	int maxrec = RPC_MAXDATASIZE;
	int attempt_cnt, port_len, port_pos, ret;
	char **port_list;
	uint64_t curtime, nexttime;
	struct timeval tv;
	struct timespec tp;
	sigset_t sig_mask, sighup_mask;
	int enable_rpcbind;

	enable_rpcbind = 1;
	/* Check that another mountd isn't already running. */
	pfh = pidfile_open(_PATH_MOUNTDPID, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST)
			errx(1, "mountd already running, pid: %d.", otherpid);
		warn("cannot open or create pidfile");
	}

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		have_v6 = 0;
	else
		close(s);

	while ((c = getopt(argc, argv, "2deh:lnp:RrS")) != -1)
		switch (c) {
		case '2':
			force_v2 = 1;
			break;
		case 'e':
			/* now a no-op, since this is the default */
			break;
		case 'n':
			resvport_only = 0;
			break;
		case 'R':
			/* Do not support Mount protocol */
			enable_rpcbind = 0;
			break;
		case 'r':
			dir_only = 0;
			break;
		case 'd':
			debug = debug ? 0 : 1;
			break;
		case 'l':
			dolog = 1;
			break;
		case 'p':
			endptr = NULL;
			svcport = (in_port_t)strtoul(optarg, &endptr, 10);
			if (endptr == NULL || *endptr != '\0' ||
			    svcport == 0 || svcport >= IPPORT_MAX)
				usage();
			svcport_str = strdup(optarg);
			break;
		case 'h':
			++nhosts;
			hosts_bak = hosts;
			hosts_bak = realloc(hosts, nhosts * sizeof(char *));
			if (hosts_bak == NULL) {
				if (hosts != NULL) {
					for (k = 0; k < nhosts; k++) 
						free(hosts[k]);
					free(hosts);
					out_of_mem();
				}
			}
			hosts = hosts_bak;
			hosts[nhosts - 1] = strdup(optarg);
			if (hosts[nhosts - 1] == NULL) {
				for (k = 0; k < (nhosts - 1); k++) 
					free(hosts[k]);
				free(hosts);
				out_of_mem();
			}
			break;
		case 'S':
			suspend_nfsd = 1;
			break;
		default:
			usage();
		}
	if (enable_rpcbind == 0) {
		if (svcport_str != NULL) {
			warnx("-p option not compatible with -R, ignored");
			free(svcport_str);
			svcport_str = NULL;
		}
		if (nhosts > 0) {
			warnx("-h option not compatible with -R, ignored");
			for (k = 0; k < nhosts; k++) 
				free(hosts[k]);
			free(hosts);
			hosts = NULL;
			nhosts = 0;
		}
	}

	if (modfind("nfsd") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("nfsd") < 0 || modfind("nfsd") < 0)
			errx(1, "NFS server is not available");
	}

	argc -= optind;
	argv += optind;
	if (argc > 0)
		exnames = argv;
	else
		exnames = exnames_default;
	openlog("mountd", LOG_PID, LOG_DAEMON);
	if (debug)
		warnx("getting export list");
	get_exportlist(0);
	if (debug)
		warnx("getting mount list");
	get_mountlist();
	if (debug)
		warnx("here we go");
	if (debug == 0) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGHUP, huphandler);
	signal(SIGTERM, terminate);
	signal(SIGPIPE, SIG_IGN);

	pidfile_write(pfh);

	if (enable_rpcbind != 0) {
		rpcb_unset(MOUNTPROG, MOUNTVERS, NULL);
		rpcb_unset(MOUNTPROG, MOUNTVERS3, NULL);
		rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrec);

		if (!resvport_only) {
			if (sysctlbyname("vfs.nfsd.nfs_privport", NULL, NULL,
			    &resvport_only, sizeof(resvport_only)) != 0 &&
			    errno != ENOENT) {
				syslog(LOG_ERR, "sysctl: %m");
				exit(1);
			}
		}

		/*
		 * If no hosts were specified, add a wildcard entry to bind to
		 * INADDR_ANY. Otherwise make sure 127.0.0.1 and ::1 are added
		 * to the list.
		 */
		if (nhosts == 0) {
			hosts = malloc(sizeof(char *));
			if (hosts == NULL)
				out_of_mem();
			hosts[0] = "*";
			nhosts = 1;
		} else {
			hosts_bak = hosts;
			if (have_v6) {
				hosts_bak = realloc(hosts, (nhosts + 2) *
				    sizeof(char *));
				if (hosts_bak == NULL) {
					for (k = 0; k < nhosts; k++)
						free(hosts[k]);
			    		free(hosts);
			    		out_of_mem();
				} else
					hosts = hosts_bak;
				nhosts += 2;
				hosts[nhosts - 2] = "::1";
			} else {
				hosts_bak = realloc(hosts, (nhosts + 1) *
				    sizeof(char *));
				if (hosts_bak == NULL) {
					for (k = 0; k < nhosts; k++)
						free(hosts[k]);
					free(hosts);
					out_of_mem();
				} else {
					nhosts += 1;
					hosts = hosts_bak;
				}
			}

			hosts[nhosts - 1] = "127.0.0.1";
		}
	}

	attempt_cnt = 1;
	sock_fdcnt = 0;
	sock_fd = NULL;
	port_list = NULL;
	port_len = 0;
	if (enable_rpcbind != 0) {
		nc_handle = setnetconfig();
		while ((nconf = getnetconfig(nc_handle))) {
			if (nconf->nc_flag & NC_VISIBLE) {
				if (have_v6 == 0 && strcmp(nconf->nc_protofmly,
				    "inet6") == 0) {
					/* DO NOTHING */
				} else {
					ret = create_service(nconf);
					if (ret == 1)
						/* Ignore this call */
						continue;
					if (ret < 0) {
						/*
						 * Failed to bind port, so close
						 * off all sockets created and
						 * try again if the port# was
						 * dynamically assigned via
						 * bind(2).
						 */
						clearout_service();
						if (mallocd_svcport != 0 &&
						    attempt_cnt <
						    GETPORT_MAXTRY) {
							free(svcport_str);
							svcport_str = NULL;
							mallocd_svcport = 0;
						} else {
							errno = EADDRINUSE;
							syslog(LOG_ERR,
							    "bindresvport_sa:"
							    " %m");
							exit(1);
						}

						/*
						 * Start over at the first
						 * service.
						 */
						free(sock_fd);
						sock_fdcnt = 0;
						sock_fd = NULL;
						nc_handle = setnetconfig();
						attempt_cnt++;
					} else if (mallocd_svcport != 0 &&
					    attempt_cnt == GETPORT_MAXTRY) {
						/*
						 * For the last attempt, allow
						 * different port #s for each
						 * nconf by saving the
						 * svcport_str setting it back
						 * to NULL.
						 */
						port_list = realloc(port_list,
						    (port_len + 1) *
						    sizeof(char *));
						if (port_list == NULL)
							out_of_mem();
						port_list[port_len++] =
						    svcport_str;
						svcport_str = NULL;
						mallocd_svcport = 0;
					}
				}
			}
		}

		/*
		 * Successfully bound the ports, so call complete_service() to
		 * do the rest of the setup on the service(s).
		 */
		sock_fdpos = 0;
		port_pos = 0;
		nc_handle = setnetconfig();
		while ((nconf = getnetconfig(nc_handle))) {
			if (nconf->nc_flag & NC_VISIBLE) {
				if (have_v6 == 0 && strcmp(nconf->nc_protofmly,
				    "inet6") == 0) {
					/* DO NOTHING */
				} else if (port_list != NULL) {
					if (port_pos >= port_len) {
						syslog(LOG_ERR, "too many"
						    " port#s");
						exit(1);
					}
					complete_service(nconf,
					    port_list[port_pos++]);
				} else
					complete_service(nconf, svcport_str);
			}
		}
		endnetconfig(nc_handle);
		free(sock_fd);
		if (port_list != NULL) {
			for (port_pos = 0; port_pos < port_len; port_pos++)
				free(port_list[port_pos]);
			free(port_list);
		}

		if (xcreated == 0) {
			syslog(LOG_ERR, "could not create any services");
			exit(1);
		}
	}

	/* Expand svc_run() here so that we can call get_exportlist(). */
	curtime = nexttime = 0;
	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &tp);
		curtime = tp.tv_sec;
		curtime = curtime * 1000000 + tp.tv_nsec / 1000;
		sigprocmask(SIG_BLOCK, &sighup_mask, &sig_mask);
		if (got_sighup && curtime >= nexttime) {
			got_sighup = 0;
			sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
			get_exportlist(1);
			clock_gettime(CLOCK_MONOTONIC, &tp);
			nexttime = tp.tv_sec;
			nexttime = nexttime * 1000000 + tp.tv_nsec / 1000 +
			    RELOADDELAY;
		} else
			sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);

		/*
		 * If a reload is pending, poll for received request(s),
		 * otherwise set a RELOADDELAY timeout, since a SIGHUP
		 * could be processed between the got_sighup test and
		 * the select() system call.
		 */
		tv.tv_sec = 0;
		if (got_sighup)
			tv.tv_usec = 0;
		else
			tv.tv_usec = RELOADDELAY;
		if (enable_rpcbind != 0) {
			readfds = svc_fdset;
			switch (select(svc_maxfd + 1, &readfds, NULL, NULL,
			    &tv)) {
			case -1:
				if (errno == EINTR) {
					/* Allow a reload now. */
					nexttime = 0;
					continue;
				}
				syslog(LOG_ERR, "mountd died: select: %m");
				exit(1);
			case 0:
				/* Allow a reload now. */
				nexttime = 0;
				continue;
			default:
				svc_getreqset(&readfds);
			}
		} else {
			/* Simply wait for a signal. */
			sigsuspend(&sig_mask);
		}
	}
} 

/*
 * This routine creates and binds sockets on the appropriate
 * addresses. It gets called one time for each transport.
 * It returns 0 upon success, 1 for ignore the call and -1 to indicate
 * bind failed with EADDRINUSE.
 * Any file descriptors that have been created are stored in sock_fd and
 * the total count of them is maintained in sock_fdcnt.
 */
static int
create_service(struct netconfig *nconf)
{
	struct addrinfo hints, *res = NULL;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct __rpc_sockinfo si;
	int aicode;
	int fd;
	int nhostsbak;
	int one = 1;
	int r;
	u_int32_t host_addr[4];  /* IPv4 or IPv6 */
	int mallocd_res;

	if ((nconf->nc_semantics != NC_TPI_CLTS) &&
	    (nconf->nc_semantics != NC_TPI_COTS) &&
	    (nconf->nc_semantics != NC_TPI_COTS_ORD))
		return (1);	/* not my type */

	/*
	 * XXX - using RPC library internal functions.
	 */
	if (!__rpc_nconf2sockinfo(nconf, &si)) {
		syslog(LOG_ERR, "cannot get information for %s",
		    nconf->nc_netid);
		return (1);
	}

	/* Get mountd's address on this transport */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = si.si_af;
	hints.ai_socktype = si.si_socktype;
	hints.ai_protocol = si.si_proto;

	/*
	 * Bind to specific IPs if asked to
	 */
	nhostsbak = nhosts;
	while (nhostsbak > 0) {
		--nhostsbak;
		sock_fd = realloc(sock_fd, (sock_fdcnt + 1) * sizeof(int));
		if (sock_fd == NULL)
			out_of_mem();
		sock_fd[sock_fdcnt++] = -1;	/* Set invalid for now. */
		mallocd_res = 0;

		hints.ai_flags = AI_PASSIVE;

		/*	
		 * XXX - using RPC library internal functions.
		 */
		if ((fd = __rpc_nconf2fd(nconf)) < 0) {
			int non_fatal = 0;
	    		if (errno == EAFNOSUPPORT &&
			    nconf->nc_semantics != NC_TPI_CLTS) 
				non_fatal = 1;
				
			syslog(non_fatal ? LOG_DEBUG : LOG_ERR, 
			    "cannot create socket for %s", nconf->nc_netid);
			if (non_fatal != 0)
				continue;
			exit(1);
		}

		switch (hints.ai_family) {
		case AF_INET:
			if (inet_pton(AF_INET, hosts[nhostsbak],
			    host_addr) == 1) {
				hints.ai_flags |= AI_NUMERICHOST;
			} else {
				/*
				 * Skip if we have an AF_INET6 address.
				 */
				if (inet_pton(AF_INET6, hosts[nhostsbak],
				    host_addr) == 1) {
					close(fd);
					continue;
				}
			}
			break;
		case AF_INET6:
			if (inet_pton(AF_INET6, hosts[nhostsbak],
			    host_addr) == 1) {
				hints.ai_flags |= AI_NUMERICHOST;
			} else {
				/*
				 * Skip if we have an AF_INET address.
				 */
				if (inet_pton(AF_INET, hosts[nhostsbak],
				    host_addr) == 1) {
					close(fd);
					continue;
				}
			}

			/*
			 * We're doing host-based access checks here, so don't
			 * allow v4-in-v6 to confuse things. The kernel will
			 * disable it by default on NFS sockets too.
			 */
			if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one,
			    sizeof one) < 0) {
				syslog(LOG_ERR,
				    "can't disable v4-in-v6 on IPv6 socket");
				exit(1);
			}
			break;
		default:
			break;
		}

		/*
		 * If no hosts were specified, just bind to INADDR_ANY
		 */
		if (strcmp("*", hosts[nhostsbak]) == 0) {
			if (svcport_str == NULL) {
				res = malloc(sizeof(struct addrinfo));
				if (res == NULL) 
					out_of_mem();
				mallocd_res = 1;
				res->ai_flags = hints.ai_flags;
				res->ai_family = hints.ai_family;
				res->ai_protocol = hints.ai_protocol;
				switch (res->ai_family) {
				case AF_INET:
					sin = malloc(sizeof(struct sockaddr_in));
					if (sin == NULL) 
						out_of_mem();
					sin->sin_family = AF_INET;
					sin->sin_port = htons(0);
					sin->sin_addr.s_addr = htonl(INADDR_ANY);
					res->ai_addr = (struct sockaddr*) sin;
					res->ai_addrlen = (socklen_t)
					    sizeof(struct sockaddr_in);
					break;
				case AF_INET6:
					sin6 = malloc(sizeof(struct sockaddr_in6));
					if (sin6 == NULL)
						out_of_mem();
					sin6->sin6_family = AF_INET6;
					sin6->sin6_port = htons(0);
					sin6->sin6_addr = in6addr_any;
					res->ai_addr = (struct sockaddr*) sin6;
					res->ai_addrlen = (socklen_t)
					    sizeof(struct sockaddr_in6);
					break;
				default:
					syslog(LOG_ERR, "bad addr fam %d",
					    res->ai_family);
					exit(1);
				}
			} else { 
				if ((aicode = getaddrinfo(NULL, svcport_str,
				    &hints, &res)) != 0) {
					syslog(LOG_ERR,
					    "cannot get local address for %s: %s",
					    nconf->nc_netid,
					    gai_strerror(aicode));
					close(fd);
					continue;
				}
			}
		} else {
			if ((aicode = getaddrinfo(hosts[nhostsbak], svcport_str,
			    &hints, &res)) != 0) {
				syslog(LOG_ERR,
				    "cannot get local address for %s: %s",
				    nconf->nc_netid, gai_strerror(aicode));
				close(fd);
				continue;
			}
		}

		/* Store the fd. */
		sock_fd[sock_fdcnt - 1] = fd;

		/* Now, attempt the bind. */
		r = bindresvport_sa(fd, res->ai_addr);
		if (r != 0) {
			if (errno == EADDRINUSE && mallocd_svcport != 0) {
				if (mallocd_res != 0) {
					free(res->ai_addr);
					free(res);
				} else
					freeaddrinfo(res);
				return (-1);
			}
			syslog(LOG_ERR, "bindresvport_sa: %m");
			exit(1);
		}

		if (svcport_str == NULL) {
			svcport_str = malloc(NI_MAXSERV * sizeof(char));
			if (svcport_str == NULL)
				out_of_mem();
			mallocd_svcport = 1;

			if (getnameinfo(res->ai_addr,
			    res->ai_addr->sa_len, NULL, NI_MAXHOST,
			    svcport_str, NI_MAXSERV * sizeof(char),
			    NI_NUMERICHOST | NI_NUMERICSERV))
				errx(1, "Cannot get port number");
		}
		if (mallocd_res != 0) {
			free(res->ai_addr);
			free(res);
		} else
			freeaddrinfo(res);
		res = NULL;
	}
	return (0);
}

/*
 * Called after all the create_service() calls have succeeded, to complete
 * the setup and registration.
 */
static void
complete_service(struct netconfig *nconf, char *port_str)
{
	struct addrinfo hints, *res = NULL;
	struct __rpc_sockinfo si;
	struct netbuf servaddr;
	SVCXPRT	*transp = NULL;
	int aicode, fd, nhostsbak;
	int registered = 0;

	if ((nconf->nc_semantics != NC_TPI_CLTS) &&
	    (nconf->nc_semantics != NC_TPI_COTS) &&
	    (nconf->nc_semantics != NC_TPI_COTS_ORD))
		return;	/* not my type */

	/*
	 * XXX - using RPC library internal functions.
	 */
	if (!__rpc_nconf2sockinfo(nconf, &si)) {
		syslog(LOG_ERR, "cannot get information for %s",
		    nconf->nc_netid);
		return;
	}

	nhostsbak = nhosts;
	while (nhostsbak > 0) {
		--nhostsbak;
		if (sock_fdpos >= sock_fdcnt) {
			/* Should never happen. */
			syslog(LOG_ERR, "Ran out of socket fd's");
			return;
		}
		fd = sock_fd[sock_fdpos++];
		if (fd < 0)
			continue;

		/*
		 * Using -1 tells listen(2) to use
		 * kern.ipc.soacceptqueue for the backlog.
		 */
		if (nconf->nc_semantics != NC_TPI_CLTS)
			listen(fd, -1);

		if (nconf->nc_semantics == NC_TPI_CLTS )
			transp = svc_dg_create(fd, 0, 0);
		else 
			transp = svc_vc_create(fd, RPC_MAXDATASIZE,
			    RPC_MAXDATASIZE);

		if (transp != (SVCXPRT *) NULL) {
			if (!svc_reg(transp, MOUNTPROG, MOUNTVERS, mntsrv,
			    NULL)) 
				syslog(LOG_ERR,
				    "can't register %s MOUNTVERS service",
				    nconf->nc_netid);
			if (!force_v2) {
				if (!svc_reg(transp, MOUNTPROG, MOUNTVERS3,
				    mntsrv, NULL)) 
					syslog(LOG_ERR,
					    "can't register %s MOUNTVERS3 service",
					    nconf->nc_netid);
			}
		} else 
			syslog(LOG_WARNING, "can't create %s services",
			    nconf->nc_netid);

		if (registered == 0) {
			registered = 1;
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = si.si_af;
			hints.ai_socktype = si.si_socktype;
			hints.ai_protocol = si.si_proto;

			if ((aicode = getaddrinfo(NULL, port_str, &hints,
			    &res)) != 0) {
				syslog(LOG_ERR, "cannot get local address: %s",
				    gai_strerror(aicode));
				exit(1);
			}

			servaddr.buf = malloc(res->ai_addrlen);
			memcpy(servaddr.buf, res->ai_addr, res->ai_addrlen);
			servaddr.len = res->ai_addrlen;

			rpcb_set(MOUNTPROG, MOUNTVERS, nconf, &servaddr);
			rpcb_set(MOUNTPROG, MOUNTVERS3, nconf, &servaddr);

			xcreated++;
			freeaddrinfo(res);
		}
	} /* end while */
}

/*
 * Clear out sockets after a failure to bind one of them, so that the
 * cycle of socket creation/binding can start anew.
 */
static void
clearout_service(void)
{
	int i;

	for (i = 0; i < sock_fdcnt; i++) {
		if (sock_fd[i] >= 0) {
			shutdown(sock_fd[i], SHUT_RDWR);
			close(sock_fd[i]);
		}
	}
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: mountd [-2] [-d] [-e] [-l] [-n] [-p <port>] [-r] "
		"[-S] [-h <bindip>] [export_file ...]\n");
	exit(1);
}

/*
 * The mount rpc service
 */
void
mntsrv(struct svc_req *rqstp, SVCXPRT *transp)
{
	struct exportlist *ep;
	struct dirlist *dp;
	struct fhreturn fhr;
	struct stat stb;
	struct statfs fsb;
	char host[NI_MAXHOST], numerichost[NI_MAXHOST];
	int lookup_failed = 1;
	struct sockaddr *saddr;
	u_short sport;
	char rpcpath[MNTPATHLEN + 1], dirpath[MAXPATHLEN];
	int defset, hostset;
	long bad = 0;
	sigset_t sighup_mask;
	int numsecflavors, *secflavorsp;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	saddr = svc_getrpccaller(transp)->buf;
	switch (saddr->sa_family) {
	case AF_INET6:
		sport = ntohs(((struct sockaddr_in6 *)saddr)->sin6_port);
		break;
	case AF_INET:
		sport = ntohs(((struct sockaddr_in *)saddr)->sin_port);
		break;
	default:
		syslog(LOG_ERR, "request from unknown address family");
		return;
	}
	switch (rqstp->rq_proc) {
	case MOUNTPROC_MNT:
	case MOUNTPROC_UMNT:
	case MOUNTPROC_UMNTALL:
		lookup_failed = getnameinfo(saddr, saddr->sa_len, host,
		    sizeof host, NULL, 0, 0);
	}
	getnameinfo(saddr, saddr->sa_len, numerichost,
	    sizeof numerichost, NULL, 0, NI_NUMERICHOST);
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, NULL))
			syslog(LOG_ERR, "can't send reply");
		return;
	case MOUNTPROC_MNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "mount request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, (xdrproc_t)xdr_dir, rpcpath)) {
			syslog(LOG_NOTICE, "undecodable mount request from %s",
			    numerichost);
			svcerr_decode(transp);
			return;
		}

		/*
		 * Get the real pathname and make sure it is a directory
		 * or a regular file if the -r option was specified
		 * and it exists.
		 */
		if (realpath(rpcpath, dirpath) == NULL ||
		    stat(dirpath, &stb) < 0 ||
		    statfs(dirpath, &fsb) < 0) {
			chdir("/");	/* Just in case realpath doesn't */
			syslog(LOG_NOTICE,
			    "mount request from %s for non existent path %s",
			    numerichost, dirpath);
			if (debug)
				warnx("stat failed on %s", dirpath);
			bad = ENOENT;	/* We will send error reply later */
		}
		if (!bad &&
		    !S_ISDIR(stb.st_mode) &&
		    (dir_only || !S_ISREG(stb.st_mode))) {
			syslog(LOG_NOTICE,
			    "mount request from %s for non-directory path %s",
			    numerichost, dirpath);
			if (debug)
				warnx("mounting non-directory %s", dirpath);
			bad = ENOTDIR;	/* We will send error reply later */
		}

		/* Check in the exports list */
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		if (bad)
			ep = NULL;
		else
			ep = ex_search(&fsb.f_fsid, exphead);
		hostset = defset = 0;
		if (ep && (chk_host(ep->ex_defdir, saddr, &defset, &hostset,
		    &numsecflavors, &secflavorsp) ||
		    ((dp = dirp_search(ep->ex_dirl, dirpath)) &&
		      chk_host(dp, saddr, &defset, &hostset, &numsecflavors,
		       &secflavorsp)) ||
		    (defset && scan_tree(ep->ex_defdir, saddr) == 0 &&
		     scan_tree(ep->ex_dirl, saddr) == 0))) {
			if (bad) {
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (hostset & DP_HOSTSET) {
				fhr.fhr_flag = hostset;
				fhr.fhr_numsecflavors = numsecflavors;
				fhr.fhr_secflavors = secflavorsp;
			} else {
				fhr.fhr_flag = defset;
				fhr.fhr_numsecflavors = ep->ex_defnumsecflavors;
				fhr.fhr_secflavors = ep->ex_defsecflavors;
			}
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(nfsfh_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				bad = errno;
				syslog(LOG_ERR, "can't get fh for %s", dirpath);
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (!svc_sendreply(transp, (xdrproc_t)xdr_fhs,
			    (caddr_t)&fhr))
				syslog(LOG_ERR, "can't send reply");
			if (!lookup_failed)
				add_mlist(host, dirpath);
			else
				add_mlist(numerichost, dirpath);
			if (debug)
				warnx("mount successful");
			if (dolog)
				syslog(LOG_NOTICE,
				    "mount request succeeded from %s for %s",
				    numerichost, dirpath);
		} else {
			if (!bad)
				bad = EACCES;
			syslog(LOG_NOTICE,
			    "mount request denied from %s for %s",
			    numerichost, dirpath);
		}

		if (bad && !svc_sendreply(transp, (xdrproc_t)xdr_long,
		    (caddr_t)&bad))
			syslog(LOG_ERR, "can't send reply");
		sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case MOUNTPROC_DUMP:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_mlist, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		else if (dolog)
			syslog(LOG_NOTICE,
			    "dump request succeeded from %s",
			    numerichost);
		return;
	case MOUNTPROC_UMNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "umount request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, (xdrproc_t)xdr_dir, rpcpath)) {
			syslog(LOG_NOTICE, "undecodable umount request from %s",
			    numerichost);
			svcerr_decode(transp);
			return;
		}
		if (realpath(rpcpath, dirpath) == NULL) {
			syslog(LOG_NOTICE, "umount request from %s "
			    "for non existent path %s",
			    numerichost, dirpath);
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		if (!lookup_failed)
			del_mlist(host, dirpath);
		del_mlist(numerichost, dirpath);
		if (dolog)
			syslog(LOG_NOTICE,
			    "umount request succeeded from %s for %s",
			    numerichost, dirpath);
		return;
	case MOUNTPROC_UMNTALL:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "umountall request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		if (!lookup_failed)
			del_mlist(host, NULL);
		del_mlist(numerichost, NULL);
		if (dolog)
			syslog(LOG_NOTICE,
			    "umountall request succeeded from %s",
			    numerichost);
		return;
	case MOUNTPROC_EXPORT:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_explist, (caddr_t)NULL))
			if (!svc_sendreply(transp, (xdrproc_t)xdr_explist_brief,
			    (caddr_t)NULL))
				syslog(LOG_ERR, "can't send reply");
		if (dolog)
			syslog(LOG_NOTICE,
			    "export request succeeded from %s",
			    numerichost);
		return;
	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dirpath string
 */
static int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, MNTPATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
static int
xdr_fhs(XDR *xdrsp, caddr_t cp)
{
	struct fhreturn *fhrp = (struct fhreturn *)cp;
	u_long ok = 0, len, auth;
	int i;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, NFSX_V2FH));
	case 3:
		len = NFSX_V3FH;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, len))
			return (0);
		if (fhrp->fhr_numsecflavors) {
			if (!xdr_int(xdrsp, &fhrp->fhr_numsecflavors))
				return (0);
			for (i = 0; i < fhrp->fhr_numsecflavors; i++)
				if (!xdr_int(xdrsp, &fhrp->fhr_secflavors[i]))
					return (0);
			return (1);
		} else {
			auth = AUTH_SYS;
			len = 1;
			if (!xdr_long(xdrsp, &len))
				return (0);
			return (xdr_long(xdrsp, &auth));
		}
	}
	return (0);
}

static int
xdr_mlist(XDR *xdrsp, caddr_t cp __unused)
{
	struct mountlist *mlp;
	int true = 1;
	int false = 0;
	char *strp;

	SLIST_FOREACH(mlp, &mlhead, next) {
		if (!xdr_bool(xdrsp, &true))
			return (0);
		strp = &mlp->ml_host[0];
		if (!xdr_string(xdrsp, &strp, MNTNAMLEN))
			return (0);
		strp = &mlp->ml_dirp[0];
		if (!xdr_string(xdrsp, &strp, MNTPATHLEN))
			return (0);
	}
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
static int
xdr_explist_common(XDR *xdrsp, caddr_t cp __unused, int brief)
{
	struct exportlist *ep;
	int false = 0;
	int putdef;
	sigset_t sighup_mask;
	int i;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &sighup_mask, NULL);

	for (i = 0; i < exphashsize; i++)
		SLIST_FOREACH(ep, &exphead[i], entries) {
			putdef = 0;
			if (put_exlist(ep->ex_dirl, xdrsp, ep->ex_defdir,
				       &putdef, brief))
				goto errout;
			if (ep->ex_defdir && putdef == 0 &&
				put_exlist(ep->ex_defdir, xdrsp, NULL,
				&putdef, brief))
				goto errout;
		}
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
errout:
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to traverse the tree and export the
 * directory paths.
 */
static int
put_exlist(struct dirlist *dp, XDR *xdrsp, struct dirlist *adp, int *putdefp,
	int brief)
{
	struct grouplist *grp;
	struct hostlist *hp;
	int true = 1;
	int false = 0;
	int gotalldir = 0;
	char *strp;

	if (dp) {
		if (put_exlist(dp->dp_left, xdrsp, adp, putdefp, brief))
			return (1);
		if (!xdr_bool(xdrsp, &true))
			return (1);
		strp = dp->dp_dirp;
		if (!xdr_string(xdrsp, &strp, MNTPATHLEN))
			return (1);
		if (adp && !strcmp(dp->dp_dirp, adp->dp_dirp)) {
			gotalldir = 1;
			*putdefp = 1;
		}
		if (brief) {
			if (!xdr_bool(xdrsp, &true))
				return (1);
			strp = "(...)";
			if (!xdr_string(xdrsp, &strp, MNTPATHLEN))
				return (1);
		} else if ((dp->dp_flag & DP_DEFSET) == 0 &&
		    (gotalldir == 0 || (adp->dp_flag & DP_DEFSET) == 0)) {
			hp = dp->dp_hosts;
			while (hp) {
				grp = hp->ht_grp;
				if (grp->gr_type == GT_HOST) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_addrinfo->ai_canonname;
					if (!xdr_string(xdrsp, &strp,
					    MNTNAMLEN))
						return (1);
				} else if (grp->gr_type == GT_NET) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_net.nt_name;
					if (!xdr_string(xdrsp, &strp,
					    MNTNAMLEN))
						return (1);
				}
				hp = hp->ht_next;
				if (gotalldir && hp == (struct hostlist *)NULL) {
					hp = adp->dp_hosts;
					gotalldir = 0;
				}
			}
		}
		if (!xdr_bool(xdrsp, &false))
			return (1);
		if (put_exlist(dp->dp_right, xdrsp, adp, putdefp, brief))
			return (1);
	}
	return (0);
}

static int
xdr_explist(XDR *xdrsp, caddr_t cp)
{

	return xdr_explist_common(xdrsp, cp, 0);
}

static int
xdr_explist_brief(XDR *xdrsp, caddr_t cp)
{

	return xdr_explist_common(xdrsp, cp, 1);
}

static char *line;
static size_t linesize;
static FILE *exp_file;

/*
 * Get the export list from one, currently open file
 */
static void
get_exportlist_one(int passno)
{
	struct exportlist *ep;
	struct grouplist *grp, *tgrp, *savgrp;
	struct dirlist *dirhead;
	struct statfs fsb;
	struct expcred anon;
	char *cp, *endcp, *dirp, *hst, *usr, *dom, savedc;
	char *err_msg = NULL;
	int len, has_host, got_nondir, dirplen, netgrp;
	uint64_t exflags;

	v4root_phase = 0;
	anon.cr_groups = NULL;
	dirhead = (struct dirlist *)NULL;
	while (get_line()) {
		if (debug)
			warnx("got line %s", line);
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;

		/*
		 * Set defaults.
		 */
		has_host = FALSE;
		anon.cr_groups = anon.cr_smallgrps;
		anon.cr_uid = UID_NOBODY;
		anon.cr_ngroups = 1;
		anon.cr_groups[0] = GID_NOGROUP;
		exflags = MNT_EXPORTED;
		got_nondir = 0;
		opt_flags = 0;
		ep = (struct exportlist *)NULL;
		dirp = NULL;

		/*
		 * Handle the V4 root dir.
		 */
		if (*cp == 'V' && *(cp + 1) == '4' && *(cp + 2) == ':') {
			/*
			 * V4: just indicates that it is the v4 root point,
			 * so skip over that and set v4root_phase.
			 */
			if (v4root_phase > 0) {
				syslog(LOG_ERR, "V4:duplicate line, ignored");
				goto nextline;
			}
			v4root_phase = 1;
			cp += 3;
			nextfield(&cp, &endcp);
		}

		/*
		 * Create new exports list entry
		 */
		len = endcp-cp;
		tgrp = grp = get_grp();
		while (len > 0) {
			if (len > MNTNAMLEN) {
			    getexp_err(ep, tgrp, "mountpoint too long");
			    goto nextline;
			}
			if (*cp == '-') {
			    if (ep == (struct exportlist *)NULL) {
				getexp_err(ep, tgrp,
				    "flag before export path definition");
				goto nextline;
			    }
			    if (debug)
				warnx("doing opt %s", cp);
			    got_nondir = 1;
			    if (do_opt(&cp, &endcp, ep, grp, &has_host,
				&exflags, &anon)) {
				getexp_err(ep, tgrp, NULL);
				goto nextline;
			    }
			} else if (*cp == '/') {
			    savedc = *endcp;
			    *endcp = '\0';
			    if (v4root_phase > 1) {
				    if (dirp != NULL) {
					getexp_err(ep, tgrp, "Multiple V4 dirs");
					goto nextline;
				    }
			    }
			    if (check_dirpath(cp, &err_msg) &&
				check_statfs(cp, &fsb, &err_msg)) {
				if ((fsb.f_flags & MNT_AUTOMOUNTED) != 0)
				    syslog(LOG_ERR, "Warning: exporting of "
					"automounted fs %s not supported", cp);
				if (got_nondir) {
				    getexp_err(ep, tgrp, "dirs must be first");
				    goto nextline;
				}
				if (v4root_phase == 1) {
				    if (dirp != NULL) {
					getexp_err(ep, tgrp, "Multiple V4 dirs");
					goto nextline;
				    }
				    if (strlen(v4root_dirpath) == 0) {
					strlcpy(v4root_dirpath, cp,
					    sizeof (v4root_dirpath));
				    } else if (strcmp(v4root_dirpath, cp)
					!= 0) {
					syslog(LOG_ERR,
					    "different V4 dirpath %s", cp);
					getexp_err(ep, tgrp, NULL);
					goto nextline;
				    }
				    dirp = cp;
				    v4root_phase = 2;
				    got_nondir = 1;
				    ep = get_exp();
				} else {
				    if (ep) {
					if (fsidcmp(&ep->ex_fs, &fsb.f_fsid)
					    != 0) {
						getexp_err(ep, tgrp,
						    "fsid mismatch");
						goto nextline;
					}
				    } else {
					/*
					 * See if this directory is already
					 * in the list.
					 */
					ep = ex_search(&fsb.f_fsid, exphead);
					if (ep == (struct exportlist *)NULL) {
					    ep = get_exp();
					    ep->ex_fs = fsb.f_fsid;
					    ep->ex_fsdir = strdup(fsb.f_mntonname);
					    if (ep->ex_fsdir == NULL)
						out_of_mem();
					    if (debug)
						warnx(
						  "making new ep fs=0x%x,0x%x",
						  fsb.f_fsid.val[0],
						  fsb.f_fsid.val[1]);
					} else if (debug)
					    warnx("found ep fs=0x%x,0x%x",
						fsb.f_fsid.val[0],
						fsb.f_fsid.val[1]);
				    }

				    /*
				     * Add dirpath to export mount point.
				     */
				    dirp = add_expdir(&dirhead, cp, len);
				    dirplen = len;
				}
			    } else {
				if (err_msg != NULL) {
					getexp_err(ep, tgrp, err_msg);
					free(err_msg);
					err_msg = NULL;
				} else {
					getexp_err(ep, tgrp,
					    "symbolic link in export path or "
					    "statfs failed");
				}
				goto nextline;
			    }
			    *endcp = savedc;
			} else {
			    savedc = *endcp;
			    *endcp = '\0';
			    got_nondir = 1;
			    if (ep == (struct exportlist *)NULL) {
				getexp_err(ep, tgrp,
				    "host(s) before export path definition");
				goto nextline;
			    }

			    /*
			     * Get the host or netgroup.
			     */
			    setnetgrent(cp);
			    netgrp = getnetgrent(&hst, &usr, &dom);
			    do {
				if (has_host) {
				    grp->gr_next = get_grp();
				    grp = grp->gr_next;
				}
				if (netgrp) {
				    if (hst == 0) {
					syslog(LOG_ERR,
				"null hostname in netgroup %s, skipping", cp);
					grp->gr_type = GT_IGNORE;
				    } else if (get_host(hst, grp, tgrp)) {
					syslog(LOG_ERR,
			"bad host %s in netgroup %s, skipping", hst, cp);
					grp->gr_type = GT_IGNORE;
				    }
				} else if (get_host(cp, grp, tgrp)) {
				    syslog(LOG_ERR, "bad host %s, skipping", cp);
				    grp->gr_type = GT_IGNORE;
				}
				has_host = TRUE;
			    } while (netgrp && getnetgrent(&hst, &usr, &dom));
			    endnetgrent();
			    *endcp = savedc;
			}
			cp = endcp;
			nextfield(&cp, &endcp);
			len = endcp - cp;
		}
		if (check_options(dirhead)) {
			getexp_err(ep, tgrp, NULL);
			goto nextline;
		}
		if (!has_host) {
			grp->gr_type = GT_DEFAULT;
			if (debug)
				warnx("adding a default entry");

		/*
		 * Don't allow a network export coincide with a list of
		 * host(s) on the same line.
		 */
		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			getexp_err(ep, tgrp, "network/host conflict");
			goto nextline;

		/*
		 * If an export list was specified on this line, make sure
		 * that we have at least one valid entry, otherwise skip it.
		 */
		} else {
			grp = tgrp;
			while (grp && grp->gr_type == GT_IGNORE)
				grp = grp->gr_next;
			if (! grp) {
			    getexp_err(ep, tgrp, "no valid entries");
			    goto nextline;
			}
		}

		if (v4root_phase == 1) {
			getexp_err(ep, tgrp, "V4:root, no dirp, ignored");
			goto nextline;
		}

		/*
		 * Loop through hosts, pushing the exports into the kernel.
		 * After loop, tgrp points to the start of the list and
		 * grp points to the last entry in the list.
		 * Do not do the do_mount() for passno == 1, since the
		 * second pass will do it, as required.
		 */
		grp = tgrp;
		do {
			grp->gr_exflags = exflags;
			cp_cred(&grp->gr_anon, &anon);
			if (v4root_phase == 2 && passno == 0)
				LOGDEBUG("do_mount v4root");
			if (passno == 0 && do_mount(ep, grp, exflags, &anon,
			    dirp, dirplen, &fsb, ep->ex_numsecflavors,
			    ep->ex_secflavors)) {
				getexp_err(ep, tgrp, NULL);
				goto nextline;
			}
		} while (grp->gr_next && (grp = grp->gr_next));

		/*
		 * For V4: don't enter in mount lists.
		 */
		if (v4root_phase > 0 && v4root_phase <= 2) {
			/*
			 * These structures are used for the reload,
			 * so save them for that case.  Otherwise, just
			 * free them up now.
			 */
			if (passno == 1 && ep != NULL) {
				savgrp = tgrp;
				while (tgrp != NULL) {
					/*
					 * Save the security flavors and exflags
					 * for this host set in the groups.
					 */
					tgrp->gr_numsecflavors =
					    ep->ex_numsecflavors;
					if (ep->ex_numsecflavors > 0)
						memcpy(tgrp->gr_secflavors,
						    ep->ex_secflavors,
						    sizeof(ep->ex_secflavors));
					tgrp = tgrp->gr_next;
				}
				if (v4root_ep == NULL) {
					v4root_ep = ep;
					ep = NULL;	/* Don't free below. */
				}
				grp->gr_next = v4root_ep->ex_grphead;
				v4root_ep->ex_grphead = savgrp;
			}
			if (ep != NULL)
				free_exp(ep);
			while (tgrp != NULL) {
				grp = tgrp;
				tgrp = tgrp->gr_next;
				free_grp(grp);
			}
			goto nextline;
		}

		/*
		 * Success. Update the data structures.
		 */
		if (has_host) {
			hang_dirp(dirhead, tgrp, ep, opt_flags, &anon, exflags);
			grp->gr_next = ep->ex_grphead;
			ep->ex_grphead = tgrp;
		} else {
			hang_dirp(dirhead, (struct grouplist *)NULL, ep,
				opt_flags, &anon, exflags);
			free_grp(grp);
		}
		dirhead = (struct dirlist *)NULL;
		if ((ep->ex_flag & EX_LINKED) == 0) {
			insert_exports(ep, exphead);

			ep->ex_flag |= EX_LINKED;
		}
nextline:
		v4root_phase = 0;
		if (dirhead) {
			free_dir(dirhead);
			dirhead = (struct dirlist *)NULL;
		}
		if (anon.cr_groups != anon.cr_smallgrps) {
			free(anon.cr_groups);
			anon.cr_groups = NULL;
		}
	}
}

/*
 * Get the export list from all specified files
 */
static void
get_exportlist(int passno)
{
	struct export_args export;
	struct iovec *iov;
	struct statfs *mntbufp;
	char errmsg[255];
	int error, i, nfs_maxvers, num;
	int iovlen;
	struct nfsex_args eargs;
	FILE *debug_file;
	size_t nfs_maxvers_size;

	if ((debug_file = fopen(_PATH_MOUNTDDEBUG, "r")) != NULL) {
		fclose(debug_file);
		logdebug = 1;
	} else
		logdebug = 0;
	LOGDEBUG("passno=%d", passno);
	v4root_dirpath[0] = '\0';
	free_v4rootexp();
	if (passno == 1) {
		/*
		 * Save the current lists as old ones, so that the new lists
		 * can be compared with the old ones in the 2nd pass.
		 */
		for (i = 0; i < exphashsize; i++) {
			SLIST_FIRST(&oldexphead[i]) = SLIST_FIRST(&exphead[i]);
			SLIST_INIT(&exphead[i]);
		}

		/* Note that the public fh has not yet been set. */
		has_set_publicfh = 0;

		/* Read the export file(s) and process them */
		read_exportfile(passno);
	} else {
		/*
		 * Just make the old lists empty.
		 * exphashsize == 0 for the first call, before oldexphead
		 * has been initialized-->loop won't be executed.
		 */
		for (i = 0; i < exphashsize; i++)
			SLIST_INIT(&oldexphead[i]);
	}

	bzero(&export, sizeof(export));
	export.ex_flags = MNT_DELEXPORT;
	iov = NULL;
	iovlen = 0;
	bzero(errmsg, sizeof(errmsg));

	if (suspend_nfsd != 0)
		(void)nfssvc(NFSSVC_SUSPENDNFSD, NULL);
	/*
	 * Delete the old V4 root dir.
	 */
	bzero(&eargs, sizeof (eargs));
	eargs.export.ex_flags = MNT_DELEXPORT;
	if (nfssvc(NFSSVC_V4ROOTEXPORT | NFSSVC_NEWSTRUCT, (caddr_t)&eargs) < 0 &&
	    errno != ENOENT)
		syslog(LOG_ERR, "Can't delete exports for V4:");

	build_iovec(&iov, &iovlen, "fstype", NULL, 0);
	build_iovec(&iov, &iovlen, "fspath", NULL, 0);
	build_iovec(&iov, &iovlen, "from", NULL, 0);
	build_iovec(&iov, &iovlen, "update", NULL, 0);
	build_iovec(&iov, &iovlen, "export", &export,
	    sizeof(export));
	build_iovec(&iov, &iovlen, "errmsg", errmsg,
	    sizeof(errmsg));

	/*
	 * For passno == 1, compare the old and new lists updating the kernel
	 * exports for any cases that have changed.
	 * This call is doing the second pass through the lists.
	 * If it fails, fall back on the bulk reload.
	 */
	if (passno == 1 && compare_nmount_exportlist(iov, iovlen, errmsg) ==
	    0) {
		LOGDEBUG("compareok");
		/* Free up the old lists. */
		free_exports(oldexphead);
	} else {
		LOGDEBUG("doing passno=0");
		/*
		 * Clear flag that notes if a public fh has been exported.
		 * It is set by do_mount() if MNT_EXPUBLIC is set for the entry.
		 */
		has_publicfh = 0;

		/* exphead == NULL if not yet allocated (first call). */
		if (exphead != NULL) {
			/*
			 * First, get rid of the old lists.
			 */
			free_exports(exphead);
			free_exports(oldexphead);
		}

		/*
		 * And delete exports that are in the kernel for all local
		 * filesystems.
		 * XXX: Should know how to handle all local exportable
		 * filesystems.
		 */
		num = getmntinfo(&mntbufp, MNT_NOWAIT);

		/* Allocate hash tables, for first call. */
		if (exphead == NULL) {
			/* Target an average linked list length of 10. */
			exphashsize = num / 10;
			if (exphashsize < 1)
				exphashsize = 1;
			else if (exphashsize > 100000)
				exphashsize = 100000;
			exphead = malloc(exphashsize * sizeof(*exphead));
			oldexphead = malloc(exphashsize * sizeof(*oldexphead));
			if (exphead == NULL || oldexphead == NULL)
				errx(1, "Can't malloc hash tables");

			for (i = 0; i < exphashsize; i++) {
				SLIST_INIT(&exphead[i]);
				SLIST_INIT(&oldexphead[i]);
			}
		}
	
		for (i = 0; i < num; i++)
			delete_export(iov, iovlen, &mntbufp[i], errmsg);


		/* Read the export file(s) and process them */
		read_exportfile(0);
	}

	if (strlen(v4root_dirpath) == 0) {
		/* Check to see if a V4: line is needed. */
		nfs_maxvers_size = sizeof(nfs_maxvers);
		error = sysctlbyname("vfs.nfsd.server_max_nfsvers",
		    &nfs_maxvers, &nfs_maxvers_size, NULL, 0);
		if (error != 0 || nfs_maxvers < NFS_VER2 || nfs_maxvers >
		    NFS_VER4) {
			syslog(LOG_ERR, "sysctlbyname(vfs.nfsd."
			    "server_max_nfsvers) failed, defaulting to NFSv3");
			nfs_maxvers = NFS_VER3;
		}
		if (nfs_maxvers == NFS_VER4)
			syslog(LOG_ERR, "NFSv4 requires at least one V4: line");
	}

	if (iov != NULL) {
		/* Free strings allocated by strdup() in getmntopts.c */
		free(iov[0].iov_base); /* fstype */
		free(iov[2].iov_base); /* fspath */
		free(iov[4].iov_base); /* from */
		free(iov[6].iov_base); /* update */
		free(iov[8].iov_base); /* export */
		free(iov[10].iov_base); /* errmsg */

		/* free iov, allocated by realloc() */
		free(iov);
		iovlen = 0;
	}

	/*
	 * If there was no public fh, clear any previous one set.
	 */
	if (has_publicfh == 0) {
		LOGDEBUG("clear public fh");
		(void) nfssvc(NFSSVC_NOPUBLICFH, NULL);
	}

	/* Resume the nfsd. If they weren't suspended, this is harmless. */
	(void)nfssvc(NFSSVC_RESUMENFSD, NULL);
	LOGDEBUG("eo get_exportlist");
}

/*
 * Insert an export entry in the appropriate list.
 */
static void
insert_exports(struct exportlist *ep, struct exportlisthead *exhp)
{
	uint32_t i;

	i = EXPHASH(&ep->ex_fs);
	LOGDEBUG("fs=%s hash=%i", ep->ex_fsdir, i);
	SLIST_INSERT_HEAD(&exhp[i], ep, entries);
}

/*
 * Free up the exports lists passed in as arguments.
 */
static void
free_exports(struct exportlisthead *exhp)
{
	struct exportlist *ep, *ep2;
	int i;

	for (i = 0; i < exphashsize; i++) {
		SLIST_FOREACH_SAFE(ep, &exhp[i], entries, ep2) {
			SLIST_REMOVE(&exhp[i], ep, exportlist, entries);
			free_exp(ep);
		}
		SLIST_INIT(&exhp[i]);
	}
}

/*
 * Read the exports file(s) and call get_exportlist_one() for each line.
 */
static void
read_exportfile(int passno)
{
	int done, i;

	/*
	 * Read in the exports file and build the list, calling
	 * nmount() as we go along to push the export rules into the kernel.
	 */
	done = 0;
	for (i = 0; exnames[i] != NULL; i++) {
		if (debug)
			warnx("reading exports from %s", exnames[i]);
		if ((exp_file = fopen(exnames[i], "r")) == NULL) {
			syslog(LOG_WARNING, "can't open %s", exnames[i]);
			continue;
		}
		get_exportlist_one(passno);
		fclose(exp_file);
		done++;
	}
	if (done == 0) {
		syslog(LOG_ERR, "can't open any exports file");
		exit(2);
	}
}

/*
 * Compare the export lists against the old ones and do nmount() operations
 * for any cases that have changed.  This avoids doing nmount() for entries
 * that have not changed.
 * Return 0 upon success, 1 otherwise.
 */
static int
compare_nmount_exportlist(struct iovec *iov, int iovlen, char *errmsg)
{
	struct exportlist *ep, *oep;
	struct grouplist *grp;
	struct statfs fs, ofs;
	int i, ret;

	/*
	 * Loop through the current list and look for an entry in the old
	 * list.
	 * If found, check to see if it the same.
	 *        If it is not the same, delete and re-export.
	 *        Then mark it done on the old list.
	 * else (not found)
	 *        export it.
	 * Any entries left in the old list after processing must have their
	 * exports deleted.
	 */
	for (i = 0; i < exphashsize; i++)
		SLIST_FOREACH(ep, &exphead[i], entries) {
			LOGDEBUG("foreach ep=%s", ep->ex_fsdir);
			oep = ex_search(&ep->ex_fs, oldexphead);
			if (oep != NULL) {
				/*
				 * Check the mount paths are the same.
				 * If not, return 1 so that the reload of the
				 * exports will be done in bulk, the
				 * passno == 0 way.
				 */
				LOGDEBUG("found old exp");
				if (strcmp(ep->ex_fsdir, oep->ex_fsdir) != 0)
					return (1);
				LOGDEBUG("same fsdir");
				/*
				 * Test to see if the entry is the same.
				 * If not the same delete exports and
				 * re-export.
				 */
				if (compare_export(ep, oep) != 0) {
					/*
					 * Clear has_publicfh if if was set
					 * in the old exports, but only if it
					 * has not been set during processing of
					 * the exports for this pass, as
					 * indicated by has_set_publicfh.
					 */
					if (has_set_publicfh == 0 &&
					    (oep->ex_flag & EX_PUBLICFH) != 0)
						has_publicfh = 0;

					/* Delete and re-export. */
					if (statfs(ep->ex_fsdir, &fs) < 0)
						return (1);
					delete_export(iov, iovlen, &fs, errmsg);
					ret = do_export_mount(ep, &fs);
					if (ret != 0)
						return (ret);
				}
				oep->ex_flag |= EX_DONE;
				LOGDEBUG("exdone");
			} else {
				LOGDEBUG("not found so export");
				/* Not found, so do export. */
				if (statfs(ep->ex_fsdir, &fs) < 0)
					return (1);
				ret = do_export_mount(ep, &fs);
				if (ret != 0)
					return (ret);
			}
		}

	/* Delete exports not done. */
	for (i = 0; i < exphashsize; i++)
		SLIST_FOREACH(oep, &oldexphead[i], entries) {
			if ((oep->ex_flag & EX_DONE) == 0) {
				LOGDEBUG("not done delete=%s", oep->ex_fsdir);
				if (statfs(oep->ex_fsdir, &ofs) >= 0 &&
				    fsidcmp(&oep->ex_fs, &ofs.f_fsid) == 0) {
					LOGDEBUG("do delete");
					/*
					 * Clear has_publicfh if if was set
					 * in the old exports, but only if it
					 * has not been set during processing of
					 * the exports for this pass, as
					 * indicated by has_set_publicfh.
					 */
					if (has_set_publicfh == 0 &&
					    (oep->ex_flag & EX_PUBLICFH) != 0)
						has_publicfh = 0;

					delete_export(iov, iovlen, &ofs,
					    errmsg);
				}
			}
		}

	/* Do the V4 root exports, as required. */
	grp = NULL;
	if (v4root_ep != NULL)
		grp = v4root_ep->ex_grphead;
	v4root_phase = 2;
	while (v4root_ep != NULL && grp != NULL) {
		LOGDEBUG("v4root expath=%s", v4root_dirpath);
		ret = do_mount(v4root_ep, grp, grp->gr_exflags, &grp->gr_anon,
		    v4root_dirpath, strlen(v4root_dirpath), &fs,
		    grp->gr_numsecflavors, grp->gr_secflavors);
		if (ret != 0) {
			v4root_phase = 0;
			return (ret);
		}
		grp = grp->gr_next;
	}
	v4root_phase = 0;
	free_v4rootexp();
	return (0);
}

/*
 * Compare old and current exportlist entries for the fsid and return 0
 * if they are the same, 1 otherwise.
 */
static int
compare_export(struct exportlist *ep, struct exportlist *oep)
{
	struct grouplist *grp, *ogrp;

	if (strcmp(ep->ex_fsdir, oep->ex_fsdir) != 0)
		return (1);
	if ((ep->ex_flag & EX_DEFSET) != (oep->ex_flag & EX_DEFSET))
		return (1);
	if ((ep->ex_defdir != NULL && oep->ex_defdir == NULL) ||
	    (ep->ex_defdir == NULL && oep->ex_defdir != NULL))
		return (1);
	if (ep->ex_defdir != NULL && (ep->ex_defdir->dp_flag & DP_DEFSET) !=
	    (oep->ex_defdir->dp_flag & DP_DEFSET))
		return (1);
	if ((ep->ex_flag & EX_DEFSET) != 0 && (ep->ex_defnumsecflavors !=
	    oep->ex_defnumsecflavors || ep->ex_defexflags !=
	    oep->ex_defexflags || compare_cred(&ep->ex_defanon,
	    &oep->ex_defanon) != 0 || compare_secflavor(ep->ex_defsecflavors,
	    oep->ex_defsecflavors, ep->ex_defnumsecflavors) != 0))
		return (1);

	/* Now, check all the groups. */
	for (ogrp = oep->ex_grphead; ogrp != NULL; ogrp = ogrp->gr_next)
		ogrp->gr_flag = 0;
	for (grp = ep->ex_grphead; grp != NULL; grp = grp->gr_next) {
		for (ogrp = oep->ex_grphead; ogrp != NULL; ogrp =
		    ogrp->gr_next)
			if ((ogrp->gr_flag & GR_FND) == 0 &&
			    grp->gr_numsecflavors == ogrp->gr_numsecflavors &&
			    grp->gr_exflags == ogrp->gr_exflags &&
			    compare_cred(&grp->gr_anon, &ogrp->gr_anon) == 0 &&
			    compare_secflavor(grp->gr_secflavors,
			    ogrp->gr_secflavors, grp->gr_numsecflavors) == 0)
				break;
		if (ogrp != NULL)
			ogrp->gr_flag |= GR_FND;
		else
			return (1);
	}
	for (ogrp = oep->ex_grphead; ogrp != NULL; ogrp = ogrp->gr_next)
		if ((ogrp->gr_flag & GR_FND) == 0)
			return (1);
	return (0);
}

/*
 * This algorithm compares two arrays of "n" items. It returns 0 if they are
 * the "same" and 1 otherwise.  Although suboptimal, it is always safe to
 * return 1, which makes compare_nmount_export() reload the exports entry.
 * "same" refers to having the same set of values in the two arrays.
 * The arrays are in no particular order and duplicates (multiple entries
 * in an array with the same value) is allowed.
 * The algorithm is inefficient, but the common case of indentical arrays is
 * handled first and "n" is normally fairly small.
 * Since the two functions need the same algorithm but for arrays of
 * different types (gid_t vs int), this is done as a macro.
 */
#define	COMPARE_ARRAYS(a1, a2, n)					\
	do {								\
		int fnd, fndarray[(n)], i, j;				\
		/* Handle common case of identical arrays. */		\
		for (i = 0; i < (n); i++)				\
			if ((a1)[i] != (a2)[i])				\
				break;					\
		if (i == (n))						\
			return (0);					\
		for (i = 0; i < (n); i++)				\
			fndarray[i] = 0;				\
		for (i = 0; i < (n); i++) {				\
			fnd = 0;					\
			for (j = 0; j < (n); j++) {			\
				if ((a1)[i] == (a2)[j]) {		\
					fndarray[j] = 1;		\
					fnd = 1;			\
				}					\
			}						\
			if (fnd == 0)					\
				return (1);				\
		}							\
		for (i = 0; i < (n); i++)				\
			if (fndarray[i] == 0)				\
				return (1);				\
		return (0);						\
	} while (0)

/*
 * Compare two struct expcred's.  Return 0 if the same and 1 otherwise.
 */
static int
compare_cred(struct expcred *cr0, struct expcred *cr1)
{

	if (cr0->cr_uid != cr1->cr_uid || cr0->cr_ngroups != cr1->cr_ngroups)
		return (1);

	COMPARE_ARRAYS(cr0->cr_groups, cr1->cr_groups, cr0->cr_ngroups);
}

/*
 * Compare two lists of security flavors.  Return 0 if the same and 1 otherwise.
 */
static int
compare_secflavor(int *sec1, int *sec2, int nsec)
{

	COMPARE_ARRAYS(sec1, sec2, nsec);
}

/*
 * Delete an exports entry.
 */
static void
delete_export(struct iovec *iov, int iovlen, struct statfs *fsp, char *errmsg)
{
	struct xvfsconf vfc;

	if (getvfsbyname(fsp->f_fstypename, &vfc) != 0) {
		syslog(LOG_ERR, "getvfsbyname() failed for %s",
		    fsp->f_fstypename);
		return;
	}
	
	/*
	 * We do not need to delete "export" flag from
	 * filesystems that do not have it set.
	 */
	if (!(fsp->f_flags & MNT_EXPORTED))
		return;
	/*
	 * Do not delete export for network filesystem by
	 * passing "export" arg to nmount().
	 * It only makes sense to do this for local filesystems.
	 */
	if (vfc.vfc_flags & VFCF_NETWORK)
		return;
	
	iov[1].iov_base = fsp->f_fstypename;
	iov[1].iov_len = strlen(fsp->f_fstypename) + 1;
	iov[3].iov_base = fsp->f_mntonname;
	iov[3].iov_len = strlen(fsp->f_mntonname) + 1;
	iov[5].iov_base = fsp->f_mntfromname;
	iov[5].iov_len = strlen(fsp->f_mntfromname) + 1;
	errmsg[0] = '\0';
	
	/*
	 * EXDEV is returned when path exists but is not a
	 * mount point.  May happens if raced with unmount.
	 */
	if (nmount(iov, iovlen, fsp->f_flags) < 0 && errno != ENOENT &&
	    errno != ENOTSUP && errno != EXDEV) {
		syslog(LOG_ERR,
		    "can't delete exports for %s: %m %s",
		    fsp->f_mntonname, errmsg);
	}
}

/*
 * Allocate an export list element
 */
static struct exportlist *
get_exp(void)
{
	struct exportlist *ep;

	ep = (struct exportlist *)calloc(1, sizeof (struct exportlist));
	if (ep == (struct exportlist *)NULL)
		out_of_mem();
	return (ep);
}

/*
 * Allocate a group list element
 */
static struct grouplist *
get_grp(void)
{
	struct grouplist *gp;

	gp = (struct grouplist *)calloc(1, sizeof (struct grouplist));
	if (gp == (struct grouplist *)NULL)
		out_of_mem();
	return (gp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
static void
getexp_err(struct exportlist *ep, struct grouplist *grp, const char *reason)
{
	struct grouplist *tgrp;

	if (!(opt_flags & OP_QUIET)) {
		if (reason != NULL)
			syslog(LOG_ERR, "bad exports list line '%s': %s", line,
			    reason);
		else
			syslog(LOG_ERR, "bad exports list line '%s'", line);
	}
	if (ep && (ep->ex_flag & EX_LINKED) == 0)
		free_exp(ep);
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
}

/*
 * Search the export list for a matching fs.
 */
static struct exportlist *
ex_search(fsid_t *fsid, struct exportlisthead *exhp)
{
	struct exportlist *ep;
	uint32_t i;

	i = EXPHASH(fsid);
	SLIST_FOREACH(ep, &exhp[i], entries) {
		if (fsidcmp(&ep->ex_fs, fsid) == 0)
			return (ep);
	}

	return (ep);
}

/*
 * Add a directory path to the list.
 */
static char *
add_expdir(struct dirlist **dpp, char *cp, int len)
{
	struct dirlist *dp;

	dp = malloc(sizeof (struct dirlist));
	if (dp == (struct dirlist *)NULL)
		out_of_mem();
	dp->dp_left = *dpp;
	dp->dp_right = (struct dirlist *)NULL;
	dp->dp_flag = 0;
	dp->dp_hosts = (struct hostlist *)NULL;
	dp->dp_dirp = strndup(cp, len);
	if (dp->dp_dirp == NULL)
		out_of_mem();
	*dpp = dp;
	return (dp->dp_dirp);
}

/*
 * Hang the dir list element off the dirpath binary tree as required
 * and update the entry for host.
 */
static void
hang_dirp(struct dirlist *dp, struct grouplist *grp, struct exportlist *ep,
	int flags, struct expcred *anoncrp, uint64_t exflags)
{
	struct hostlist *hp;
	struct dirlist *dp2;

	if (flags & OP_ALLDIRS) {
		if (ep->ex_defdir)
			free((caddr_t)dp);
		else
			ep->ex_defdir = dp;
		if (grp == (struct grouplist *)NULL) {
			ep->ex_flag |= EX_DEFSET;
			ep->ex_defdir->dp_flag |= DP_DEFSET;
			/* Save the default security flavors list. */
			ep->ex_defnumsecflavors = ep->ex_numsecflavors;
			if (ep->ex_numsecflavors > 0)
				memcpy(ep->ex_defsecflavors, ep->ex_secflavors,
				    sizeof(ep->ex_secflavors));
			cp_cred(&ep->ex_defanon, anoncrp);
			ep->ex_defexflags = exflags;
		} else while (grp) {
			hp = get_ht();
			hp->ht_grp = grp;
			hp->ht_next = ep->ex_defdir->dp_hosts;
			ep->ex_defdir->dp_hosts = hp;
			/* Save the security flavors list for this host set. */
			grp->gr_numsecflavors = ep->ex_numsecflavors;
			if (ep->ex_numsecflavors > 0)
				memcpy(grp->gr_secflavors, ep->ex_secflavors,
				    sizeof(ep->ex_secflavors));
			grp = grp->gr_next;
		}
	} else {

		/*
		 * Loop through the directories adding them to the tree.
		 */
		while (dp) {
			dp2 = dp->dp_left;
			add_dlist(&ep->ex_dirl, dp, grp, flags, ep, anoncrp,
			    exflags);
			dp = dp2;
		}
	}
}

/*
 * Traverse the binary tree either updating a node that is already there
 * for the new directory or adding the new node.
 */
static void
add_dlist(struct dirlist **dpp, struct dirlist *newdp, struct grouplist *grp,
	int flags, struct exportlist *ep, struct expcred *anoncrp,
	uint64_t exflags)
{
	struct dirlist *dp;
	struct hostlist *hp;
	int cmp;

	dp = *dpp;
	if (dp) {
		cmp = strcmp(dp->dp_dirp, newdp->dp_dirp);
		if (cmp > 0) {
			add_dlist(&dp->dp_left, newdp, grp, flags, ep, anoncrp,
			    exflags);
			return;
		} else if (cmp < 0) {
			add_dlist(&dp->dp_right, newdp, grp, flags, ep, anoncrp,
			    exflags);
			return;
		} else
			free((caddr_t)newdp);
	} else {
		dp = newdp;
		dp->dp_left = (struct dirlist *)NULL;
		*dpp = dp;
	}
	if (grp) {

		/*
		 * Hang all of the host(s) off of the directory point.
		 */
		do {
			hp = get_ht();
			hp->ht_grp = grp;
			hp->ht_next = dp->dp_hosts;
			dp->dp_hosts = hp;
			/* Save the security flavors list for this host set. */
			grp->gr_numsecflavors = ep->ex_numsecflavors;
			if (ep->ex_numsecflavors > 0)
				memcpy(grp->gr_secflavors, ep->ex_secflavors,
				    sizeof(ep->ex_secflavors));
			grp = grp->gr_next;
		} while (grp);
	} else {
		ep->ex_flag |= EX_DEFSET;
		dp->dp_flag |= DP_DEFSET;
		/* Save the default security flavors list. */
		ep->ex_defnumsecflavors = ep->ex_numsecflavors;
		if (ep->ex_numsecflavors > 0)
			memcpy(ep->ex_defsecflavors, ep->ex_secflavors,
			    sizeof(ep->ex_secflavors));
		cp_cred(&ep->ex_defanon, anoncrp);
		ep->ex_defexflags = exflags;
	}
}

/*
 * Search for a dirpath on the export point.
 */
static struct dirlist *
dirp_search(struct dirlist *dp, char *dirp)
{
	int cmp;

	if (dp) {
		cmp = strcmp(dp->dp_dirp, dirp);
		if (cmp > 0)
			return (dirp_search(dp->dp_left, dirp));
		else if (cmp < 0)
			return (dirp_search(dp->dp_right, dirp));
		else
			return (dp);
	}
	return (dp);
}

/*
 * Scan for a host match in a directory tree.
 */
static int
chk_host(struct dirlist *dp, struct sockaddr *saddr, int *defsetp,
	int *hostsetp, int *numsecflavors, int **secflavorsp)
{
	struct hostlist *hp;
	struct grouplist *grp;
	struct addrinfo *ai;

	if (dp) {
		if (dp->dp_flag & DP_DEFSET)
			*defsetp = dp->dp_flag;
		hp = dp->dp_hosts;
		while (hp) {
			grp = hp->ht_grp;
			switch (grp->gr_type) {
			case GT_HOST:
				ai = grp->gr_ptr.gt_addrinfo;
				for (; ai; ai = ai->ai_next) {
					if (!sacmp(ai->ai_addr, saddr, NULL)) {
						*hostsetp =
						    (hp->ht_flag | DP_HOSTSET);
						if (numsecflavors != NULL) {
							*numsecflavors =
							    grp->gr_numsecflavors;
							*secflavorsp =
							    grp->gr_secflavors;
						}
						return (1);
					}
				}
				break;
			case GT_NET:
				if (!sacmp(saddr, (struct sockaddr *)
				    &grp->gr_ptr.gt_net.nt_net,
				    (struct sockaddr *)
				    &grp->gr_ptr.gt_net.nt_mask)) {
					*hostsetp = (hp->ht_flag | DP_HOSTSET);
					if (numsecflavors != NULL) {
						*numsecflavors =
						    grp->gr_numsecflavors;
						*secflavorsp =
						    grp->gr_secflavors;
					}
					return (1);
				}
				break;
			}
			hp = hp->ht_next;
		}
	}
	return (0);
}

/*
 * Scan tree for a host that matches the address.
 */
static int
scan_tree(struct dirlist *dp, struct sockaddr *saddr)
{
	int defset, hostset;

	if (dp) {
		if (scan_tree(dp->dp_left, saddr))
			return (1);
		if (chk_host(dp, saddr, &defset, &hostset, NULL, NULL))
			return (1);
		if (scan_tree(dp->dp_right, saddr))
			return (1);
	}
	return (0);
}

/*
 * Traverse the dirlist tree and free it up.
 */
static void
free_dir(struct dirlist *dp)
{

	if (dp) {
		free_dir(dp->dp_left);
		free_dir(dp->dp_right);
		free_host(dp->dp_hosts);
		free(dp->dp_dirp);
		free(dp);
	}
}

/*
 * Parse a colon separated list of security flavors
 */
static int
parsesec(char *seclist, struct exportlist *ep)
{
	char *cp, savedc;
	int flavor;

	ep->ex_numsecflavors = 0;
	for (;;) {
		cp = strchr(seclist, ':');
		if (cp) {
			savedc = *cp;
			*cp = '\0';
		}

		if (!strcmp(seclist, "sys"))
			flavor = AUTH_SYS;
		else if (!strcmp(seclist, "krb5"))
			flavor = RPCSEC_GSS_KRB5;
		else if (!strcmp(seclist, "krb5i"))
			flavor = RPCSEC_GSS_KRB5I;
		else if (!strcmp(seclist, "krb5p"))
			flavor = RPCSEC_GSS_KRB5P;
		else {
			if (cp)
				*cp = savedc;
			syslog(LOG_ERR, "bad sec flavor: %s", seclist);
			return (1);
		}
		if (ep->ex_numsecflavors == MAXSECFLAVORS) {
			if (cp)
				*cp = savedc;
			syslog(LOG_ERR, "too many sec flavors: %s", seclist);
			return (1);
		}
		ep->ex_secflavors[ep->ex_numsecflavors] = flavor;
		ep->ex_numsecflavors++;
		if (cp) {
			*cp = savedc;
			seclist = cp + 1;
		} else {
			break;
		}
	}
	return (0);
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
static int
do_opt(char **cpp, char **endcpp, struct exportlist *ep, struct grouplist *grp,
	int *has_hostp, uint64_t *exflagsp, struct expcred *cr)
{
	char *cpoptarg, *cpoptend;
	char *cp, *endcp, *cpopt, savedc, savedc2;
	int allflag, usedarg;

	savedc2 = '\0';
	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		if ((cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if ((cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
		} else {
			if ((cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
			else {
				*cp = savedc;
				nextfield(&cp, &endcp);
				**endcpp = '\0';
				if (endcp > cp && *cp != '-') {
					cpoptarg = cp;
					savedc2 = *endcp;
					*endcp = '\0';
					usedarg = 0;
				}
			}
		}
		if (!strcmp(cpopt, "ro") || !strcmp(cpopt, "o")) {
			*exflagsp |= MNT_EXRDONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
		    !(allflag = strcmp(cpopt, "mapall")) ||
		    !strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			parsecred(cpoptarg, cr);
			if (allflag == 0) {
				*exflagsp |= MNT_EXPORTANON;
				opt_flags |= OP_MAPALL;
			} else
				opt_flags |= OP_MAPROOT;
		} else if (cpoptarg && (!strcmp(cpopt, "mask") ||
		    !strcmp(cpopt, "m"))) {
			if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 1)) {
				syslog(LOG_ERR, "bad mask: %s", cpoptarg);
				return (1);
			}
			usedarg++;
			opt_flags |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
			!strcmp(cpopt, "n"))) {
			if (strchr(cpoptarg, '/') != NULL) {
				if (debug)
					fprintf(stderr, "setting OP_MASKLEN\n");
				opt_flags |= OP_MASKLEN;
			}
			if (grp->gr_type != GT_NULL) {
				syslog(LOG_ERR, "network/host conflict");
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 0)) {
				syslog(LOG_ERR, "bad net: %s", cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			opt_flags |= OP_ALLDIRS;
		} else if (!strcmp(cpopt, "public")) {
			*exflagsp |= MNT_EXPUBLIC;
		} else if (!strcmp(cpopt, "webnfs")) {
			*exflagsp |= (MNT_EXPUBLIC|MNT_EXRDONLY|MNT_EXPORTANON);
			opt_flags |= OP_MAPALL;
		} else if (cpoptarg && !strcmp(cpopt, "index")) {
			ep->ex_indexfile = strdup(cpoptarg);
		} else if (!strcmp(cpopt, "quiet")) {
			opt_flags |= OP_QUIET;
		} else if (cpoptarg && !strcmp(cpopt, "sec")) {
			if (parsesec(cpoptarg, ep))
				return (1);
			opt_flags |= OP_SEC;
			usedarg++;
		} else if (!strcmp(cpopt, "tls")) {
			*exflagsp |= MNT_EXTLS;
		} else if (!strcmp(cpopt, "tlscert")) {
			*exflagsp |= (MNT_EXTLS | MNT_EXTLSCERT);
		} else if (!strcmp(cpopt, "tlscertuser")) {
			*exflagsp |= (MNT_EXTLS | MNT_EXTLSCERT |
			    MNT_EXTLSCERTUSER);
		} else {
			syslog(LOG_ERR, "bad opt %s", cpopt);
			return (1);
		}
		if (usedarg >= 0) {
			*endcp = savedc2;
			**endcpp = savedc;
			if (usedarg > 0) {
				*cpp = cp;
				*endcpp = endcp;
			}
			return (0);
		}
		cpopt = cpoptend;
	}
	**endcpp = savedc;
	return (0);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
static int
get_host(char *cp, struct grouplist *grp, struct grouplist *tgrp)
{
	struct grouplist *checkgrp;
	struct addrinfo *ai, *tai, hints;
	int ecode;
	char host[NI_MAXHOST];

	if (grp->gr_type != GT_NULL) {
		syslog(LOG_ERR, "Bad netgroup type for ip host %s", cp);
		return (1);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_CANONNAME;
	hints.ai_protocol = IPPROTO_UDP;
	ecode = getaddrinfo(cp, NULL, &hints, &ai);
	if (ecode != 0) {
		syslog(LOG_ERR,"can't get address info for host %s", cp);
		return 1;
	}
	grp->gr_ptr.gt_addrinfo = ai;
	while (ai != NULL) {
		if (ai->ai_canonname == NULL) {
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen, host,
			    sizeof host, NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(host, "?", sizeof(host));
			ai->ai_canonname = strdup(host);
			ai->ai_flags |= AI_CANONNAME;
		}
		if (debug)
			fprintf(stderr, "got host %s\n", ai->ai_canonname);
		/*
		 * Sanity check: make sure we don't already have an entry
		 * for this host in the grouplist.
		 */
		for (checkgrp = tgrp; checkgrp != NULL;
		    checkgrp = checkgrp->gr_next) {
			if (checkgrp->gr_type != GT_HOST)
				continue;
			for (tai = checkgrp->gr_ptr.gt_addrinfo; tai != NULL;
			    tai = tai->ai_next) {
				if (sacmp(tai->ai_addr, ai->ai_addr, NULL) != 0)
					continue;
				if (debug)
					fprintf(stderr,
					    "ignoring duplicate host %s\n",
					    ai->ai_canonname);
				grp->gr_type = GT_IGNORE;
				return (0);
			}
		}
		ai = ai->ai_next;
	}
	grp->gr_type = GT_HOST;
	return (0);
}

/*
 * Free up an exports list component
 */
static void
free_exp(struct exportlist *ep)
{
	struct grouplist *grp, *tgrp;

	if (ep->ex_defdir) {
		free_host(ep->ex_defdir->dp_hosts);
		free((caddr_t)ep->ex_defdir);
	}
	if (ep->ex_fsdir)
		free(ep->ex_fsdir);
	if (ep->ex_indexfile)
		free(ep->ex_indexfile);
	free_dir(ep->ex_dirl);
	grp = ep->ex_grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	if (ep->ex_defanon.cr_groups != ep->ex_defanon.cr_smallgrps)
		free(ep->ex_defanon.cr_groups);
	free((caddr_t)ep);
}

/*
 * Free up the v4root exports.
 */
static void
free_v4rootexp(void)
{

	if (v4root_ep != NULL) {
		free_exp(v4root_ep);
		v4root_ep = NULL;
	}
}

/*
 * Free hosts.
 */
static void
free_host(struct hostlist *hp)
{
	struct hostlist *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free((caddr_t)hp2);
	}
}

static struct hostlist *
get_ht(void)
{
	struct hostlist *hp;

	hp = (struct hostlist *)malloc(sizeof (struct hostlist));
	if (hp == (struct hostlist *)NULL)
		out_of_mem();
	hp->ht_next = (struct hostlist *)NULL;
	hp->ht_flag = 0;
	return (hp);
}

/*
 * Out of memory, fatal
 */
static void
out_of_mem(void)
{

	syslog(LOG_ERR, "out of memory");
	exit(2);
}

/*
 * Call do_mount() from the struct exportlist, for each case needed.
 */
static int
do_export_mount(struct exportlist *ep, struct statfs *fsp)
{
	struct grouplist *grp, defgrp;
	int ret;
	size_t dirlen;

	LOGDEBUG("do_export_mount=%s", ep->ex_fsdir);
	dirlen = strlen(ep->ex_fsdir);
	if ((ep->ex_flag & EX_DEFSET) != 0) {
		defgrp.gr_type = GT_DEFAULT;
		defgrp.gr_next = NULL;
		/* We have an entry for all other hosts/nets. */
		LOGDEBUG("ex_defexflags=0x%jx", (uintmax_t)ep->ex_defexflags);
		ret = do_mount(ep, &defgrp, ep->ex_defexflags, &ep->ex_defanon,
		    ep->ex_fsdir, dirlen, fsp, ep->ex_defnumsecflavors,
		    ep->ex_defsecflavors);
		if (ret != 0)
			return (ret);
	}

	/* Do a mount for each group. */
	grp = ep->ex_grphead;
	while (grp != NULL) {
		LOGDEBUG("do mount gr_type=0x%x gr_exflags=0x%jx",
		    grp->gr_type, (uintmax_t)grp->gr_exflags);
		ret = do_mount(ep, grp, grp->gr_exflags, &grp->gr_anon,
		    ep->ex_fsdir, dirlen, fsp, grp->gr_numsecflavors,
		    grp->gr_secflavors);
		if (ret != 0)
			return (ret);
		grp = grp->gr_next;
	}
	return (0);
}

/*
 * Do the nmount() syscall with the update flag to push the export info into
 * the kernel.
 */
static int
do_mount(struct exportlist *ep, struct grouplist *grp, uint64_t exflags,
    struct expcred *anoncrp, char *dirp, int dirplen, struct statfs *fsb,
    int numsecflavors, int *secflavors)
{
	struct statfs fsb1;
	struct addrinfo *ai;
	struct export_args *eap;
	char errmsg[255];
	char *cp;
	int done;
	char savedc;
	struct iovec *iov;
	int i, iovlen;
	int ret;
	struct nfsex_args nfsea;

	eap = &nfsea.export;

	cp = NULL;
	savedc = '\0';
	iov = NULL;
	iovlen = 0;
	ret = 0;

	bzero(eap, sizeof (struct export_args));
	bzero(errmsg, sizeof(errmsg));
	eap->ex_flags = exflags;
	eap->ex_uid = anoncrp->cr_uid;
	eap->ex_ngroups = anoncrp->cr_ngroups;
	if (eap->ex_ngroups > 0) {
		eap->ex_groups = malloc(eap->ex_ngroups * sizeof(gid_t));
		memcpy(eap->ex_groups, anoncrp->cr_groups, eap->ex_ngroups *
		    sizeof(gid_t));
	}
	LOGDEBUG("do_mount exflags=0x%jx", (uintmax_t)exflags);
	eap->ex_indexfile = ep->ex_indexfile;
	if (grp->gr_type == GT_HOST)
		ai = grp->gr_ptr.gt_addrinfo;
	else
		ai = NULL;
	eap->ex_numsecflavors = numsecflavors;
	LOGDEBUG("do_mount numsec=%d", numsecflavors);
	for (i = 0; i < eap->ex_numsecflavors; i++)
		eap->ex_secflavors[i] = secflavors[i];
	if (eap->ex_numsecflavors == 0) {
		eap->ex_numsecflavors = 1;
		eap->ex_secflavors[0] = AUTH_SYS;
	}
	done = FALSE;

	if (v4root_phase == 0) {
		build_iovec(&iov, &iovlen, "fstype", NULL, 0);
		build_iovec(&iov, &iovlen, "fspath", NULL, 0);
		build_iovec(&iov, &iovlen, "from", NULL, 0);
		build_iovec(&iov, &iovlen, "update", NULL, 0);
		build_iovec(&iov, &iovlen, "export", eap,
		    sizeof (struct export_args));
		build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));
	}

	while (!done) {
		switch (grp->gr_type) {
		case GT_HOST:
			if (ai->ai_addr->sa_family == AF_INET6 && have_v6 == 0)
				goto skip;
			eap->ex_addr = ai->ai_addr;
			eap->ex_addrlen = ai->ai_addrlen;
			eap->ex_masklen = 0;
			break;
		case GT_NET:
			if (grp->gr_ptr.gt_net.nt_net.ss_family == AF_INET6 &&
			    have_v6 == 0)
				goto skip;
			eap->ex_addr =
			    (struct sockaddr *)&grp->gr_ptr.gt_net.nt_net;
			eap->ex_addrlen =
			    ((struct sockaddr *)&grp->gr_ptr.gt_net.nt_net)->sa_len;
			eap->ex_mask =
			    (struct sockaddr *)&grp->gr_ptr.gt_net.nt_mask;
			eap->ex_masklen = ((struct sockaddr *)&grp->gr_ptr.gt_net.nt_mask)->sa_len;
			break;
		case GT_DEFAULT:
			eap->ex_addr = NULL;
			eap->ex_addrlen = 0;
			eap->ex_mask = NULL;
			eap->ex_masklen = 0;
			break;
		case GT_IGNORE:
			ret = 0;
			goto error_exit;
			break;
		default:
			syslog(LOG_ERR, "bad grouptype");
			if (cp)
				*cp = savedc;
			ret = 1;
			goto error_exit;
		}

		/*
		 * For V4:, use the nfssvc() syscall, instead of mount().
		 */
		if (v4root_phase == 2) {
			nfsea.fspec = v4root_dirpath;
			if (nfssvc(NFSSVC_V4ROOTEXPORT | NFSSVC_NEWSTRUCT,
			    (caddr_t)&nfsea) < 0) {
				syslog(LOG_ERR, "Exporting V4: failed");
				ret = 2;
				goto error_exit;
			}
		} else {
			/*
			 * XXX:
			 * Maybe I should just use the fsb->f_mntonname path
			 * instead of looping back up the dirp to the mount
			 * point??
			 * Also, needs to know how to export all types of local
			 * exportable filesystems and not just "ufs".
			 */
			iov[1].iov_base = fsb->f_fstypename; /* "fstype" */
			iov[1].iov_len = strlen(fsb->f_fstypename) + 1;
			iov[3].iov_base = fsb->f_mntonname; /* "fspath" */
			iov[3].iov_len = strlen(fsb->f_mntonname) + 1;
			iov[5].iov_base = fsb->f_mntfromname; /* "from" */
			iov[5].iov_len = strlen(fsb->f_mntfromname) + 1;
			errmsg[0] = '\0';
	
			while (nmount(iov, iovlen, fsb->f_flags) < 0) {
				if (cp)
					*cp-- = savedc;
				else
					cp = dirp + dirplen - 1;
				if (opt_flags & OP_QUIET) {
					ret = 1;
					goto error_exit;
				}
				if (errno == EPERM) {
					if (debug)
						warnx("can't change attributes for %s: %s",
						    dirp, errmsg);
					syslog(LOG_ERR,
					   "can't change attributes for %s: %s",
					    dirp, errmsg);
					ret = 1;
					goto error_exit;
				}
				if (opt_flags & OP_ALLDIRS) {
					if (errno == EINVAL)
						syslog(LOG_ERR,
		"-alldirs requested but %s is not a filesystem mountpoint",
						    dirp);
					else
						syslog(LOG_ERR,
						    "could not remount %s: %m",
						    dirp);
					ret = 1;
					goto error_exit;
				}
				/* back up over the last component */
				while (cp > dirp && *cp == '/')
					cp--;
				while (cp > dirp && *(cp - 1) != '/')
					cp--;
				if (cp == dirp) {
					if (debug)
						warnx("mnt unsucc");
					syslog(LOG_ERR, "can't export %s %s",
					    dirp, errmsg);
					ret = 1;
					goto error_exit;
				}
				savedc = *cp;
				*cp = '\0';
				/*
				 * Check that we're still on the same
				 * filesystem.
				 */
				if (statfs(dirp, &fsb1) != 0 ||
				    fsidcmp(&fsb1.f_fsid, &fsb->f_fsid) != 0) {
					*cp = savedc;
					syslog(LOG_ERR,
					    "can't export %s %s", dirp,
					    errmsg);
					ret = 1;
					goto error_exit;
				}
			}
		}

		/*
		 * For the experimental server:
		 * If this is the public directory, get the file handle
		 * and load it into the kernel via the nfssvc() syscall.
		 */
		if ((exflags & MNT_EXPUBLIC) != 0) {
			fhandle_t fh;
			char *public_name;

			if (eap->ex_indexfile != NULL)
				public_name = eap->ex_indexfile;
			else
				public_name = dirp;
			if (getfh(public_name, &fh) < 0)
				syslog(LOG_ERR,
				    "Can't get public fh for %s", public_name);
			else if (nfssvc(NFSSVC_PUBLICFH, (caddr_t)&fh) < 0)
				syslog(LOG_ERR,
				    "Can't set public fh for %s", public_name);
			else {
				has_publicfh = 1;
				has_set_publicfh = 1;
				ep->ex_flag |= EX_PUBLICFH;
			}
		}
skip:
		if (ai != NULL)
			ai = ai->ai_next;
		if (ai == NULL)
			done = TRUE;
	}
	if (cp)
		*cp = savedc;
error_exit:
	free(eap->ex_groups);
	/* free strings allocated by strdup() in getmntopts.c */
	if (iov != NULL) {
		free(iov[0].iov_base); /* fstype */
		free(iov[2].iov_base); /* fspath */
		free(iov[4].iov_base); /* from */
		free(iov[6].iov_base); /* update */
		free(iov[8].iov_base); /* export */
		free(iov[10].iov_base); /* errmsg */

		/* free iov, allocated by realloc() */
		free(iov);
	}
	return (ret);
}

/*
 * Translate a net address.
 *
 * If `maskflg' is nonzero, then `cp' is a netmask, not a network address.
 */
static int
get_net(char *cp, struct netmsk *net, int maskflg)
{
	struct netent *np = NULL;
	char *name, *p, *prefp;
	struct sockaddr_in sin;
	struct sockaddr *sa = NULL;
	struct addrinfo hints, *ai = NULL;
	char netname[NI_MAXHOST];
	long preflen;

	p = prefp = NULL;
	if ((opt_flags & OP_MASKLEN) && !maskflg) {
		p = strchr(cp, '/');
		*p = '\0';
		prefp = p + 1;
	}

	/*
	 * Check for a numeric address first. We wish to avoid
	 * possible DNS lookups in getnetbyname().
	 */
	if (isxdigit(*cp) || *cp == ':') {
		memset(&hints, 0, sizeof hints);
		/* Ensure the mask and the network have the same family. */
		if (maskflg && (opt_flags & OP_NET))
			hints.ai_family = net->nt_net.ss_family;
		else if (!maskflg && (opt_flags & OP_HAVEMASK))
			hints.ai_family = net->nt_mask.ss_family;
		else
			hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(cp, NULL, &hints, &ai) == 0)
			sa = ai->ai_addr;
		if (sa != NULL && ai->ai_family == AF_INET) {
			/*
			 * The address in `cp' is really a network address, so
			 * use inet_network() to re-interpret this correctly.
			 * e.g. "127.1" means 127.1.0.0, not 127.0.0.1.
			 */
			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = inet_makeaddr(inet_network(cp), 0);
			if (debug)
				fprintf(stderr, "get_net: v4 addr %s\n",
				    inet_ntoa(sin.sin_addr));
			sa = (struct sockaddr *)&sin;
		}
	}
	if (sa == NULL && (np = getnetbyname(cp)) != NULL) {
		bzero(&sin, sizeof sin);
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof sin;
		sin.sin_addr = inet_makeaddr(np->n_net, 0);
		sa = (struct sockaddr *)&sin;
	}
	if (sa == NULL)
		goto fail;

	if (maskflg) {
		/* The specified sockaddr is a mask. */
		if (checkmask(sa) != 0)
			goto fail;
		bcopy(sa, &net->nt_mask, sa->sa_len);
		opt_flags |= OP_HAVEMASK;
	} else {
		/* The specified sockaddr is a network address. */
		bcopy(sa, &net->nt_net, sa->sa_len);

		/* Get a network name for the export list. */
		if (np) {
			name = np->n_name;
		} else if (getnameinfo(sa, sa->sa_len, netname, sizeof netname,
		   NULL, 0, NI_NUMERICHOST) == 0) {
			name = netname;
		} else {
			goto fail;
		}
		if ((net->nt_name = strdup(name)) == NULL)
			out_of_mem();

		/*
		 * Extract a mask from either a "/<masklen>" suffix, or
		 * from the class of an IPv4 address.
		 */
		if (opt_flags & OP_MASKLEN) {
			preflen = strtol(prefp, NULL, 10);
			if (preflen < 0L || preflen == LONG_MAX)
				goto fail;
			bcopy(sa, &net->nt_mask, sa->sa_len);
			if (makemask(&net->nt_mask, (int)preflen) != 0)
				goto fail;
			opt_flags |= OP_HAVEMASK;
			*p = '/';
		} else if (sa->sa_family == AF_INET &&
		    (opt_flags & OP_MASK) == 0) {
			in_addr_t addr;

			syslog(LOG_WARNING,
			    "WARNING: No mask specified for %s, "
			    "using out-of-date default", name);
			addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			if (IN_CLASSA(addr))
				preflen = 8;
			else if (IN_CLASSB(addr))
				preflen = 16;
			else if (IN_CLASSC(addr))
				preflen = 24;
			else if (IN_CLASSD(addr))	/* XXX Multicast??? */
				preflen = 28;
			else
				preflen = 32;	/* XXX */

			bcopy(sa, &net->nt_mask, sa->sa_len);
			makemask(&net->nt_mask, (int)preflen);
			opt_flags |= OP_HAVEMASK;
		}
	}

	if (ai)
		freeaddrinfo(ai);
	return 0;

fail:
	if (ai)
		freeaddrinfo(ai);
	return 1;
}

/*
 * Parse out the next white space separated field
 */
static void
nextfield(char **cp, char **endcp)
{
	char *p;
	char quot = 0;

	p = *cp;
	while (*p == ' ' || *p == '\t')
		p++;
	*cp = p;
	while (*p != '\0') {
		if (quot) {
			if (*p == quot)
				quot = 0;
		} else {
			if (*p == '\\' && *(p + 1) != '\0')
				p++;
			else if (*p == '\'' || *p == '"')
				quot = *p;
			else if (*p == ' ' || *p == '\t')
				break;
		}
		p++;
	};
	*endcp = p;
}

/*
 * Get an exports file line. Skip over blank lines and handle line
 * continuations.
 */
static int
get_line(void)
{
	char *p, *cp;
	size_t len;
	int totlen, cont_line;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	p = line;
	totlen = 0;
	do {
		if ((p = fgetln(exp_file, &len)) == NULL)
			return (0);
		cp = p + len - 1;
		cont_line = 0;
		while (cp >= p &&
		    (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\\')) {
			if (*cp == '\\')
				cont_line = 1;
			cp--;
			len--;
		}
		if (cont_line) {
			*++cp = ' ';
			len++;
		}
		if (linesize < len + totlen + 1) {
			linesize = len + totlen + 1;
			line = realloc(line, linesize);
			if (line == NULL)
				out_of_mem();
		}
		memcpy(line + totlen, p, len);
		totlen += len;
		line[totlen] = '\0';
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
static void
parsecred(char *namelist, struct expcred *cr)
{
	char *name;
	int inpos;
	char *names;
	struct passwd *pw;
	struct group *gr;
	gid_t groups[NGROUPS_MAX + 1];
	int ngroups;
	unsigned long name_ul;
	char *end = NULL;

	/*
	 * Set up the unprivileged user.
	 */
	cr->cr_groups = cr->cr_smallgrps;
	cr->cr_uid = UID_NOBODY;
	cr->cr_groups[0] = GID_NOGROUP;
	cr->cr_ngroups = 1;
	/*
	 * Get the user's password table entry.
	 */
	names = namelist;
	name = strsep_quote(&names, ":");
	/* Bug?  name could be NULL here */
	name_ul = strtoul(name, &end, 10);
	if (*end != '\0' || end == name)
		pw = getpwnam(name);
	else
		pw = getpwuid((uid_t)name_ul);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			syslog(LOG_ERR, "unknown user: %s", name);
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS_MAX + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups)) {
			syslog(LOG_ERR, "too many groups");
			ngroups = NGROUPS_MAX + 1;
		}

		/*
		 * Compress out duplicate.
		 */
		if (ngroups > 1 && groups[0] == groups[1]) {
			ngroups--;
			inpos = 2;
		} else {
			inpos = 1;
		}
		if (ngroups > NGROUPS_MAX)
			ngroups = NGROUPS_MAX;
		if (ngroups > SMALLNGROUPS)
			cr->cr_groups = malloc(ngroups * sizeof(gid_t));
		cr->cr_ngroups = ngroups;
		cr->cr_groups[0] = groups[0];
		memcpy(&cr->cr_groups[1], &groups[inpos], (ngroups - 1) *
		    sizeof(gid_t));
		return;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL) {
		cr->cr_uid = pw->pw_uid;
	} else if (*end != '\0' || end == name) {
		syslog(LOG_ERR, "unknown user: %s", name);
		return;
	} else {
		cr->cr_uid = name_ul;
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS_MAX) {
		name = strsep_quote(&names, ":");
		name_ul = strtoul(name, &end, 10);
		if (*end != '\0' || end == name) {
			if ((gr = getgrnam(name)) == NULL) {
				syslog(LOG_ERR, "unknown group: %s", name);
				continue;
			}
			groups[cr->cr_ngroups++] = gr->gr_gid;
		} else {
			groups[cr->cr_ngroups++] = name_ul;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS_MAX)
		syslog(LOG_ERR, "too many groups");
	if (cr->cr_ngroups > SMALLNGROUPS)
		cr->cr_groups = malloc(cr->cr_ngroups * sizeof(gid_t));
	memcpy(cr->cr_groups, groups, cr->cr_ngroups * sizeof(gid_t));
}

#define	STRSIZ	(MNTNAMLEN+MNTPATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
static void
get_mountlist(void)
{
	struct mountlist *mlp;
	char *host, *dirp, *cp;
	char str[STRSIZ];
	FILE *mlfile;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		if (errno == ENOENT)
			return;
		else {
			syslog(LOG_ERR, "can't open %s", _PATH_RMOUNTLIST);
			return;
		}
	}
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dirp = strsep(&cp, " \t\n");
		if (host == NULL || dirp == NULL)
			continue;
		mlp = (struct mountlist *)malloc(sizeof (*mlp));
		if (mlp == (struct mountlist *)NULL)
			out_of_mem();
		strncpy(mlp->ml_host, host, MNTNAMLEN);
		mlp->ml_host[MNTNAMLEN] = '\0';
		strncpy(mlp->ml_dirp, dirp, MNTPATHLEN);
		mlp->ml_dirp[MNTPATHLEN] = '\0';

		SLIST_INSERT_HEAD(&mlhead, mlp, next);
	}
	fclose(mlfile);
}

static void
del_mlist(char *hostp, char *dirp)
{
	struct mountlist *mlp, *mlp2;
	FILE *mlfile;
	int fnd = 0;

	SLIST_FOREACH_SAFE(mlp, &mlhead, next, mlp2) {
		if (!strcmp(mlp->ml_host, hostp) &&
		    (!dirp || !strcmp(mlp->ml_dirp, dirp))) {
			fnd = 1;
			SLIST_REMOVE(&mlhead, mlp, mountlist, next);
			free((caddr_t)mlp);
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			syslog(LOG_ERR,"can't update %s", _PATH_RMOUNTLIST);
			return;
		}
		SLIST_FOREACH(mlp, &mlhead, next) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
		}
		fclose(mlfile);
	}
}

static void
add_mlist(char *hostp, char *dirp)
{
	struct mountlist *mlp;
	FILE *mlfile;

	SLIST_FOREACH(mlp, &mlhead, next) {
		if (!strcmp(mlp->ml_host, hostp) && !strcmp(mlp->ml_dirp, dirp))
			return;
	}

	mlp = (struct mountlist *)malloc(sizeof (*mlp));
	if (mlp == (struct mountlist *)NULL)
		out_of_mem();
	strncpy(mlp->ml_host, hostp, MNTNAMLEN);
	mlp->ml_host[MNTNAMLEN] = '\0';
	strncpy(mlp->ml_dirp, dirp, MNTPATHLEN);
	mlp->ml_dirp[MNTPATHLEN] = '\0';
	SLIST_INSERT_HEAD(&mlhead, mlp, next);
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		syslog(LOG_ERR, "can't update %s", _PATH_RMOUNTLIST);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
	fclose(mlfile);
}

/*
 * Free up a group list.
 */
static void
free_grp(struct grouplist *grp)
{
	if (grp->gr_type == GT_HOST) {
		if (grp->gr_ptr.gt_addrinfo != NULL)
			freeaddrinfo(grp->gr_ptr.gt_addrinfo);
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_ptr.gt_net.nt_name)
			free(grp->gr_ptr.gt_net.nt_name);
	}
	if (grp->gr_anon.cr_groups != grp->gr_anon.cr_smallgrps)
		free(grp->gr_anon.cr_groups);
	free((caddr_t)grp);
}

#ifdef DEBUG
static void
SYSLOG(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif /* DEBUG */

/*
 * Check options for consistency.
 */
static int
check_options(struct dirlist *dp)
{

	if (v4root_phase == 0 && dp == NULL)
	    return (1);
	if ((opt_flags & (OP_MAPROOT | OP_MAPALL)) == (OP_MAPROOT | OP_MAPALL)) {
	    syslog(LOG_ERR, "-mapall and -maproot mutually exclusive");
	    return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		syslog(LOG_ERR, "-mask requires -network");
		return (1);
	}
	if ((opt_flags & OP_NET) && (opt_flags & OP_HAVEMASK) == 0) {
		syslog(LOG_ERR, "-network requires mask specification");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_MASKLEN)) {
		syslog(LOG_ERR, "-mask and /masklen are mutually exclusive");
		return (1);
	}
	if (v4root_phase > 0 &&
	    (opt_flags &
	     ~(OP_SEC | OP_MASK | OP_NET | OP_HAVEMASK | OP_MASKLEN)) != 0) {
	    syslog(LOG_ERR,"only -sec,-net,-mask options allowed on V4:");
	    return (1);
	}
	if ((opt_flags & OP_ALLDIRS) && dp->dp_left) {
	    syslog(LOG_ERR, "-alldirs has multiple directories");
	    return (1);
	}
	return (0);
}

static int
check_path_component(const char *path, char **err)
{
	struct stat sb;

	if (lstat(path, &sb)) {
		asprintf(err, "%s: lstat() failed: %s.\n",
		    path, strerror(errno));
		return (0);
	}

	switch (sb.st_mode & S_IFMT) {
	case S_IFDIR:
		return (1);
	case S_IFLNK:
		asprintf(err, "%s: path is a symbolic link.\n", path);
		break;
	case S_IFREG:
		asprintf(err, "%s: path is a file rather than a directory.\n",
		    path);
		break;
	default:
		asprintf(err, "%s: path is not a directory.\n", path);
	}

	return (0);
}

/*
 * Check each path component for the presence of symbolic links. Return true
 */
static int
check_dirpath(char *dirp, char **err)
{
	char *cp;

	cp = dirp + 1;
	while (*cp) {
		if (*cp == '/') {
			*cp = '\0';

			if (!check_path_component(dirp, err)) {
				*cp = '/';
				return (0);
			}

			*cp = '/';
		}
		cp++;
	}

	if (!check_path_component(dirp, err))
		return (0);

	return (1);
}

/*
 * Populate statfs information. Return true on success.
 */
static int
check_statfs(const char *dirp, struct statfs *fsb, char **err)
{
	if (statfs(dirp, fsb)) {
		asprintf(err, "%s: statfs() failed: %s\n", dirp,
		    strerror(errno));
		return (0);
	}

	return (1);
}

/*
 * Make a netmask according to the specified prefix length. The ss_family
 * and other non-address fields must be initialised before calling this.
 */
static int
makemask(struct sockaddr_storage *ssp, int bitlen)
{
	u_char *p;
	int bits, i, len;

	if ((p = sa_rawaddr((struct sockaddr *)ssp, &len)) == NULL)
		return (-1);
	if (bitlen > len * CHAR_BIT)
		return (-1);

	for (i = 0; i < len; i++) {
		bits = MIN(CHAR_BIT, bitlen);
		*p++ = (u_char)~0 << (CHAR_BIT - bits);
		bitlen -= bits;
	}
	return 0;
}

/*
 * Check that the sockaddr is a valid netmask. Returns 0 if the mask
 * is acceptable (i.e. of the form 1...10....0).
 */
static int
checkmask(struct sockaddr *sa)
{
	u_char *mask;
	int i, len;

	if ((mask = sa_rawaddr(sa, &len)) == NULL)
		return (-1);

	for (i = 0; i < len; i++)
		if (mask[i] != 0xff)
			break;
	if (i < len) {
		if (~mask[i] & (u_char)(~mask[i] + 1))
			return (-1);
		i++;
	}
	for (; i < len; i++)
		if (mask[i] != 0)
			return (-1);
	return (0);
}

/*
 * Compare two sockaddrs according to a specified mask. Return zero if
 * `sa1' matches `sa2' when filtered by the netmask in `samask'.
 * If samask is NULL, perform a full comparison.
 */
static int
sacmp(struct sockaddr *sa1, struct sockaddr *sa2, struct sockaddr *samask)
{
	unsigned char *p1, *p2, *mask;
	int len, i;

	if (sa1->sa_family != sa2->sa_family ||
	    (p1 = sa_rawaddr(sa1, &len)) == NULL ||
	    (p2 = sa_rawaddr(sa2, NULL)) == NULL)
		return (1);

	switch (sa1->sa_family) {
	case AF_INET6:
		if (((struct sockaddr_in6 *)sa1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)sa2)->sin6_scope_id)
			return (1);
		break;
	}

	/* Simple binary comparison if no mask specified. */
	if (samask == NULL)
		return (memcmp(p1, p2, len));

	/* Set up the mask, and do a mask-based comparison. */
	if (sa1->sa_family != samask->sa_family ||
	    (mask = sa_rawaddr(samask, NULL)) == NULL)
		return (1);

	for (i = 0; i < len; i++)
		if ((p1[i] & mask[i]) != (p2[i] & mask[i]))
			return (1);
	return (0);
}

/*
 * Return a pointer to the part of the sockaddr that contains the
 * raw address, and set *nbytes to its length in bytes. Returns
 * NULL if the address family is unknown.
 */
static void *
sa_rawaddr(struct sockaddr *sa, int *nbytes) {
	void *p;
	int len;

	switch (sa->sa_family) {
	case AF_INET:
		len = sizeof(((struct sockaddr_in *)sa)->sin_addr);
		p = &((struct sockaddr_in *)sa)->sin_addr;
		break;
	case AF_INET6:
		len = sizeof(((struct sockaddr_in6 *)sa)->sin6_addr);
		p = &((struct sockaddr_in6 *)sa)->sin6_addr;
		break;
	default:
		p = NULL;
		len = 0;
	}

	if (nbytes != NULL)
		*nbytes = len;
	return (p);
}

static void
huphandler(int sig __unused)
{

	got_sighup = 1;
}

static void
terminate(int sig __unused)
{
	pidfile_remove(pfh);
	rpcb_unset(MOUNTPROG, MOUNTVERS, NULL);
	rpcb_unset(MOUNTPROG, MOUNTVERS3, NULL);
	exit (0);
}

static void
cp_cred(struct expcred *outcr, struct expcred *incr)
{

	outcr->cr_uid = incr->cr_uid;
	outcr->cr_ngroups = incr->cr_ngroups;
	if (outcr->cr_ngroups > SMALLNGROUPS)
		outcr->cr_groups = malloc(outcr->cr_ngroups * sizeof(gid_t));
	else
		outcr->cr_groups = outcr->cr_smallgrps;
	memcpy(outcr->cr_groups, incr->cr_groups, incr->cr_ngroups *
	    sizeof(gid_t));
}
