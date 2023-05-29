/*-
 * Copyright (c) 2009 Rick Macklem, University of Guelph
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <nfs/nfssvc.h>

#include <rpc/rpc.h>

#include <fs/nfs/rpcv2.h>
#include <fs/nfs/nfsproto.h>
#include <fs/nfs/nfskpiport.h>
#include <fs/nfs/nfs.h>

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/*
 * This program loads the password and group databases into the kernel
 * for NFS V4.
 */

static void	cleanup_term(int);
static void	usage(void);
static void	nfsuserdsrv(struct svc_req *, SVCXPRT *);
static bool_t	xdr_getid(XDR *, caddr_t);
static bool_t	xdr_getname(XDR *, caddr_t);
static bool_t	xdr_retval(XDR *, caddr_t);
static int	nfsbind_localhost(void);

#define	MAXNAME		1024
#define	MAXNFSUSERD	20
#define	DEFNFSUSERD	4
#define	MAXUSERMAX	100000
#define	MINUSERMAX	10
#define	DEFUSERMAX	200
#define	DEFUSERTIMEOUT	(1 * 60)
struct info {
	long	id;
	long	retval;
	char	name[MAXNAME + 1];
};

u_char *dnsname = "default.domain";
u_char *defaultuser = "nobody";
uid_t defaultuid = 65534;
u_char *defaultgroup = "nogroup";
gid_t defaultgid = 65533;
int verbose = 0, im_a_server = 0, nfsuserdcnt = -1, forcestart = 0;
int defusertimeout = DEFUSERTIMEOUT, manage_gids = 0;
pid_t servers[MAXNFSUSERD];
static struct sockaddr_storage fromip;
#ifdef INET6
static struct in6_addr in6loopback = IN6ADDR_LOOPBACK_INIT;
#endif

int
main(int argc, char *argv[])
{
	int i, j;
	int error, fnd_dup, len, mustfreeai = 0, start_uidpos;
	struct nfsd_idargs nid;
	struct passwd *pwd;
	struct group *grp;
	int sock, one = 1;
	SVCXPRT *udptransp;
	struct nfsuserd_args nargs;
	sigset_t signew;
	char hostname[MAXHOSTNAMELEN + 1], *cp;
	struct addrinfo *aip, hints;
	static uid_t check_dups[MAXUSERMAX];
	gid_t grps[NGROUPS];
	int ngroup;
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	int jailed, s;
	size_t jailed_size;

	if (modfind("nfscommon") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("nfscommon") < 0 ||
		    modfind("nfscommon") < 0)
			errx(1, "Experimental nfs subsystem is not available");
	}

	/*
	 * First, figure out what our domain name and Kerberos Realm
	 * seem to be. Command line args may override these later.
	 */
	if (gethostname(hostname, MAXHOSTNAMELEN) == 0) {
		if ((cp = strchr(hostname, '.')) != NULL &&
		    *(cp + 1) != '\0') {
			dnsname = cp + 1;
		} else {
			memset((void *)&hints, 0, sizeof (hints));
			hints.ai_flags = AI_CANONNAME;
			error = getaddrinfo(hostname, NULL, &hints, &aip);
			if (error == 0) {
			    if (aip->ai_canonname != NULL &&
				(cp = strchr(aip->ai_canonname, '.')) != NULL
				&& *(cp + 1) != '\0') {
					dnsname = cp + 1;
					mustfreeai = 1;
				} else {
					freeaddrinfo(aip);
				}
			}
		}
	}

	/*
	 * See if this server handles IPv4 or IPv6 and set up the default
	 * localhost address.
	 */
	s = -1;
#ifdef INET6
	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s >= 0) {
		fromip.ss_family = AF_INET6;
		fromip.ss_len = sizeof(struct sockaddr_in6);
		sin6 = (struct sockaddr_in6 *)&fromip;
		sin6->sin6_addr = in6loopback;
		close(s);
	}
#endif	/* INET6 */
#ifdef INET
	if (s < 0) {
		s = socket(PF_INET, SOCK_DGRAM, 0);
		if (s >= 0) {
			fromip.ss_family = AF_INET;
			fromip.ss_len = sizeof(struct sockaddr_in);
			sin = (struct sockaddr_in *)&fromip;
			sin->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			close(s);
		}
	}
#endif	/* INET */
	if (s < 0)
		err(1, "Can't create a inet/inet6 socket");

	nid.nid_usermax = DEFUSERMAX;
	nid.nid_usertimeout = defusertimeout;

	argc--;
	argv++;
	while (argc >= 1) {
		if (!strcmp(*argv, "-domain")) {
			if (argc == 1)
				usage();
			argc--;
			argv++;
			strncpy(hostname, *argv, MAXHOSTNAMELEN);
			hostname[MAXHOSTNAMELEN] = '\0';
			dnsname = hostname;
		} else if (!strcmp(*argv, "-verbose")) {
			verbose = 1;
		} else if (!strcmp(*argv, "-force")) {
			forcestart = 1;
		} else if (!strcmp(*argv, "-manage-gids")) {
			manage_gids = 1;
		} else if (!strcmp(*argv, "-usermax")) {
			if (argc == 1)
				usage();
			argc--;
			argv++;
			i = atoi(*argv);
			if (i < MINUSERMAX || i > MAXUSERMAX) {
				fprintf(stderr,
				    "usermax %d out of range %d<->%d\n", i,
				    MINUSERMAX, MAXUSERMAX);
				usage();
			}
			nid.nid_usermax = i;
		} else if (!strcmp(*argv, "-usertimeout")) {
			if (argc == 1)
				usage();
			argc--;
			argv++;
			i = atoi(*argv);
			if (i < 0 || i > 100000) {
				fprintf(stderr,
				    "usertimeout %d out of range 0<->100000\n",
				    i);
				usage();
			}
			nid.nid_usertimeout = defusertimeout = i * 60;
		} else if (nfsuserdcnt == -1) {
			nfsuserdcnt = atoi(*argv);
			if (nfsuserdcnt < 1)
				usage();
			if (nfsuserdcnt > MAXNFSUSERD) {
				warnx("nfsuserd count %d; reset to %d",
				    nfsuserdcnt, DEFNFSUSERD);
				nfsuserdcnt = DEFNFSUSERD;
			}
		} else {
			usage();
		}
		argc--;
		argv++;
	}
	if (nfsuserdcnt < 1)
		nfsuserdcnt = DEFNFSUSERD;

	/*
	 * Strip off leading and trailing '.'s in domain name and map
	 * alphabetics to lower case.
	 */
	while (*dnsname == '.')
		dnsname++;
	if (*dnsname == '\0')
		errx(1, "Domain name all '.'");
	len = strlen(dnsname);
	cp = dnsname + len - 1;
	while (*cp == '.') {
		*cp = '\0';
		len--;
		cp--;
	}
	for (i = 0; i < len; i++) {
		if (!isascii(dnsname[i]))
			errx(1, "Domain name has non-ascii char");
		if (isupper(dnsname[i]))
			dnsname[i] = tolower(dnsname[i]);
	}

	/*
	 * If the nfsuserd died off ungracefully, this is necessary to
	 * get them to start again.
	 */
	if (forcestart && nfssvc(NFSSVC_NFSUSERDDELPORT, NULL) < 0)
		errx(1, "Can't do nfssvc() to delete the port");

	if (verbose)
		fprintf(stderr,
		    "nfsuserd: domain=%s usermax=%d usertimeout=%d\n",
		    dnsname, nid.nid_usermax, nid.nid_usertimeout);

	for (i = 0; i < nfsuserdcnt; i++)
		servers[i] = (pid_t)-1;

	nargs.nuserd_family = fromip.ss_family;
	/*
	 * Set up the service port to accept requests via UDP from
	 * localhost (INADDR_LOOPBACK or IN6ADDR_LOOPBACK_INIT).
	 */
	if ((sock = socket(nargs.nuserd_family, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err(1, "cannot create udp socket");

	/*
	 * Not sure what this does, so I'll leave it here for now.
	 */
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	
	if ((udptransp = svcudp_create(sock)) == NULL)
		err(1, "Can't set up socket");

	/*
	 * By not specifying a protocol, it is linked into the
	 * dispatch queue, but not registered with portmapper,
	 * which is just what I want.
	 */
	if (!svc_register(udptransp, RPCPROG_NFSUSERD, RPCNFSUSERD_VERS,
	    nfsuserdsrv, 0))
		err(1, "Can't register nfsuserd");

	/*
	 * Tell the kernel what my port# is.
	 */
	nargs.nuserd_port = htons(udptransp->xp_port);
#ifdef DEBUG
	printf("portnum=0x%x\n", nargs.nuserd_port);
#else
	if (nfssvc(NFSSVC_NFSUSERDPORT | NFSSVC_NEWSTRUCT, &nargs) < 0) {
		if (errno == EPERM) {
			jailed = 0;
			jailed_size = sizeof(jailed);
			sysctlbyname("security.jail.jailed", &jailed,
			    &jailed_size, NULL, 0);
			if (jailed != 0) {
				fprintf(stderr, "Cannot start nfsuserd. "
				    "allow.nfsd might not be configured\n");
			} else {
				fprintf(stderr, "Cannot start nfsuserd "
				    "when already running.");
				fprintf(stderr, " If not running, "
				    "use the -force option.\n");
			}
		} else {
			fprintf(stderr, "Can't do nfssvc() to add port\n");
		}
		exit(1);
	}
#endif

	pwd = getpwnam(defaultuser);
	if (pwd)
		nid.nid_uid = pwd->pw_uid;
	else
		nid.nid_uid = defaultuid;
	grp = getgrnam(defaultgroup);
	if (grp)
		nid.nid_gid = grp->gr_gid;
	else
		nid.nid_gid = defaultgid;
	nid.nid_name = dnsname;
	nid.nid_namelen = strlen(nid.nid_name);
	nid.nid_ngroup = 0;
	nid.nid_grps = NULL;
	nid.nid_flag = NFSID_INITIALIZE;
#ifdef DEBUG
	printf("Initialize uid=%d gid=%d dns=%s\n", nid.nid_uid, nid.nid_gid, 
	    nid.nid_name);
#else
	error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
	if (error)
		errx(1, "Can't initialize nfs user/groups");
#endif

	i = 0;
	/*
	 * Loop around adding all groups.
	 */
	setgrent();
	while (i < nid.nid_usermax && (grp = getgrent())) {
		nid.nid_gid = grp->gr_gid;
		nid.nid_name = grp->gr_name;
		nid.nid_namelen = strlen(grp->gr_name);
		nid.nid_ngroup = 0;
		nid.nid_grps = NULL;
		nid.nid_flag = NFSID_ADDGID;
#ifdef DEBUG
		printf("add gid=%d name=%s\n", nid.nid_gid, nid.nid_name);
#else
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error)
			errx(1, "Can't add group %s", grp->gr_name);
#endif
		i++;
	}
	endgrent();

	/*
	 * Loop around adding all users.
	 */
	start_uidpos = i;
	setpwent();
	while (i < nid.nid_usermax && (pwd = getpwent())) {
		fnd_dup = 0;
		/*
		 * Yes, this is inefficient, but it is only done once when
		 * the daemon is started and will run in a fraction of a second
		 * for nid_usermax at 10000. If nid_usermax is cranked up to
		 * 100000, it will take several seconds, depending on the CPU.
		 */
		for (j = 0; j < (i - start_uidpos); j++)
			if (check_dups[j] == pwd->pw_uid) {
				/* Found another entry for uid, so skip it */
				fnd_dup = 1;
				break;
			}
		if (fnd_dup != 0)
			continue;
		check_dups[i - start_uidpos] = pwd->pw_uid;
		nid.nid_uid = pwd->pw_uid;
		nid.nid_name = pwd->pw_name;
		nid.nid_namelen = strlen(pwd->pw_name);
		if (manage_gids != 0) {
			/* Get the group list for this user. */
			ngroup = NGROUPS;
			if (getgrouplist(pwd->pw_name, pwd->pw_gid, grps,
			    &ngroup) < 0)
				syslog(LOG_ERR, "Group list too small");
			nid.nid_ngroup = ngroup;
			nid.nid_grps = grps;
		} else {
			nid.nid_ngroup = 0;
			nid.nid_grps = NULL;
		}
		nid.nid_flag = NFSID_ADDUID;
#ifdef DEBUG
		printf("add uid=%d name=%s\n", nid.nid_uid, nid.nid_name);
#else
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error)
			errx(1, "Can't add user %s", pwd->pw_name);
#endif
		i++;
	}
	endpwent();

	/*
	 * I should feel guilty for not calling this for all the above exit()
	 * upon error cases, but I don't.
	 */
	if (mustfreeai)
		freeaddrinfo(aip);

#ifdef DEBUG
	exit(0);
#endif
	/*
	 * Temporarily block SIGUSR1 and SIGCHLD, so servers[] can't
	 * end up bogus.
	 */
	sigemptyset(&signew);
	sigaddset(&signew, SIGUSR1);
	sigaddset(&signew, SIGCHLD);
	sigprocmask(SIG_BLOCK, &signew, NULL);

	daemon(0, 0);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGUSR1, cleanup_term);
	(void)signal(SIGCHLD, cleanup_term);

	openlog("nfsuserd:", LOG_PID, LOG_DAEMON);

	/*
	 * Fork off the server daemons that do the work. All the master
	 * does is terminate them and cleanup.
	 */
	for (i = 0; i < nfsuserdcnt; i++) {
		servers[i] = fork();
		if (servers[i] == 0) {
			im_a_server = 1;
			setproctitle("server");
			sigemptyset(&signew);
			sigaddset(&signew, SIGUSR1);
			sigprocmask(SIG_UNBLOCK, &signew, NULL);

			/*
			 * and away we go.
			 */
			svc_run();
			syslog(LOG_ERR, "nfsuserd died: %m");
			exit(1);
		} else if (servers[i] < 0) {
			syslog(LOG_ERR, "fork: %m");
		}
	}

	/*
	 * Just wait for SIGUSR1 or a child to die and then...
	 * As the Governor of California would say, "Terminate them".
	 */
	setproctitle("master");
	sigemptyset(&signew);
	while (1)
		sigsuspend(&signew);
}

/*
 * The nfsuserd rpc service
 */
static void
nfsuserdsrv(struct svc_req *rqstp, SVCXPRT *transp)
{
	struct passwd *pwd;
	struct group *grp;
	int error;
#if defined(INET) || defined(INET6)
	u_short sport;
	int ret;
#endif
	struct info info;
	struct nfsd_idargs nid;
	gid_t grps[NGROUPS];
	int ngroup;
#ifdef INET
	struct sockaddr_in *fromsin, *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *fromsin6, *sin6;
	char buf[INET6_ADDRSTRLEN];
#endif

	/*
	 * Only handle requests from localhost on a reserved port number.
	 * If the upcall is from a different address, call nfsbind_localhost()
	 * to check for a remapping of localhost, due to jails.
	 * (Since a reserved port # at localhost implies a client with
	 *  local root, there won't be a security breach. This is about
	 *  the only case I can think of where a reserved port # means
	 *  something.)
	 */
	if (rqstp->rq_proc != NULLPROC) {
		switch (fromip.ss_family) {
#ifdef INET
		case AF_INET:
			if (transp->xp_rtaddr.len < sizeof(*sin)) {
				syslog(LOG_ERR, "xp_rtaddr too small");
				svcerr_weakauth(transp);
				return;
			}
			sin = (struct sockaddr_in *)transp->xp_rtaddr.buf;
			fromsin = (struct sockaddr_in *)&fromip;
			sport = ntohs(sin->sin_port);
			if (sport >= IPPORT_RESERVED) {
				syslog(LOG_ERR, "not a reserved port#");
				svcerr_weakauth(transp);
				return;
			}
			ret = 1;
			if (sin->sin_addr.s_addr != fromsin->sin_addr.s_addr)
				ret = nfsbind_localhost();
			if (ret == 0 || sin->sin_addr.s_addr !=
			    fromsin->sin_addr.s_addr) {
				syslog(LOG_ERR, "bad from ip %s",
				    inet_ntoa(sin->sin_addr));
				svcerr_weakauth(transp);
				return;
			}
			break;
#endif	/* INET */
#ifdef INET6
		case AF_INET6:
			if (transp->xp_rtaddr.len < sizeof(*sin6)) {
				syslog(LOG_ERR, "xp_rtaddr too small");
				svcerr_weakauth(transp);
				return;
			}
			sin6 = (struct sockaddr_in6 *)transp->xp_rtaddr.buf;
			fromsin6 = (struct sockaddr_in6 *)&fromip;
			sport = ntohs(sin6->sin6_port);
			if (sport >= IPV6PORT_RESERVED) {
				syslog(LOG_ERR, "not a reserved port#");
				svcerr_weakauth(transp);
				return;
			}
			ret = 1;
			if (!IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
			    &fromsin6->sin6_addr))
				ret = nfsbind_localhost();
			if (ret == 0 || !IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
			    &fromsin6->sin6_addr)) {
				if (inet_ntop(AF_INET6, &sin6->sin6_addr, buf,
				    INET6_ADDRSTRLEN) != NULL)
					syslog(LOG_ERR, "bad from ip %s", buf);
				else
					syslog(LOG_ERR, "bad from ip6 addr");
				svcerr_weakauth(transp);
				return;
			}
			break;
#endif	/* INET6 */
		}
	}
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCNFSUSERD_GETUID:
		if (!svc_getargs(transp, (xdrproc_t)xdr_getid,
		    (caddr_t)&info)) {
			svcerr_decode(transp);
			return;
		}
		pwd = getpwuid((uid_t)info.id);
		info.retval = 0;
		if (pwd != NULL) {
			nid.nid_usertimeout = defusertimeout;
			nid.nid_uid = pwd->pw_uid;
			nid.nid_name = pwd->pw_name;
			if (manage_gids != 0) {
				/* Get the group list for this user. */
				ngroup = NGROUPS;
				if (getgrouplist(pwd->pw_name, pwd->pw_gid,
				    grps, &ngroup) < 0)
					syslog(LOG_ERR, "Group list too small");
				nid.nid_ngroup = ngroup;
				nid.nid_grps = grps;
			} else {
				nid.nid_ngroup = 0;
				nid.nid_grps = NULL;
			}
		} else {
			nid.nid_usertimeout = 5;
			nid.nid_uid = (uid_t)info.id;
			nid.nid_name = defaultuser;
			nid.nid_ngroup = 0;
			nid.nid_grps = NULL;
		}
		nid.nid_namelen = strlen(nid.nid_name);
		nid.nid_flag = NFSID_ADDUID;
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error) {
			info.retval = error;
			syslog(LOG_ERR, "Can't add user %s\n", pwd->pw_name);
		} else if (verbose) {
			syslog(LOG_ERR,"Added uid=%d name=%s\n",
			    nid.nid_uid, nid.nid_name);
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_retval,
		    (caddr_t)&info))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCNFSUSERD_GETGID:
		if (!svc_getargs(transp, (xdrproc_t)xdr_getid,
		    (caddr_t)&info)) {
			svcerr_decode(transp);
			return;
		}
		grp = getgrgid((gid_t)info.id);
		info.retval = 0;
		if (grp != NULL) {
			nid.nid_usertimeout = defusertimeout;
			nid.nid_gid = grp->gr_gid;
			nid.nid_name = grp->gr_name;
		} else {
			nid.nid_usertimeout = 5;
			nid.nid_gid = (gid_t)info.id;
			nid.nid_name = defaultgroup;
		}
		nid.nid_namelen = strlen(nid.nid_name);
		nid.nid_ngroup = 0;
		nid.nid_grps = NULL;
		nid.nid_flag = NFSID_ADDGID;
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error) {
			info.retval = error;
			syslog(LOG_ERR, "Can't add group %s\n",
			    grp->gr_name);
		} else if (verbose) {
			syslog(LOG_ERR,"Added gid=%d name=%s\n",
			    nid.nid_gid, nid.nid_name);
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_retval,
		    (caddr_t)&info))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCNFSUSERD_GETUSER:
		if (!svc_getargs(transp, (xdrproc_t)xdr_getname,
		    (caddr_t)&info)) {
			svcerr_decode(transp);
			return;
		}
		pwd = getpwnam(info.name);
		info.retval = 0;
		if (pwd != NULL) {
			nid.nid_usertimeout = defusertimeout;
			nid.nid_uid = pwd->pw_uid;
			nid.nid_name = pwd->pw_name;
		} else {
			nid.nid_usertimeout = 5;
			nid.nid_uid = defaultuid;
			nid.nid_name = info.name;
		}
		nid.nid_namelen = strlen(nid.nid_name);
		nid.nid_ngroup = 0;
		nid.nid_grps = NULL;
		nid.nid_flag = NFSID_ADDUSERNAME;
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error) {
			info.retval = error;
			syslog(LOG_ERR, "Can't add user %s\n", pwd->pw_name);
		} else if (verbose) {
			syslog(LOG_ERR,"Added uid=%d name=%s\n",
			    nid.nid_uid, nid.nid_name);
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_retval,
		    (caddr_t)&info))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCNFSUSERD_GETGROUP:
		if (!svc_getargs(transp, (xdrproc_t)xdr_getname,
		    (caddr_t)&info)) {
			svcerr_decode(transp);
			return;
		}
		grp = getgrnam(info.name);
		info.retval = 0;
		if (grp != NULL) {
			nid.nid_usertimeout = defusertimeout;
			nid.nid_gid = grp->gr_gid;
			nid.nid_name = grp->gr_name;
		} else {
			nid.nid_usertimeout = 5;
			nid.nid_gid = defaultgid;
			nid.nid_name = info.name;
		}
		nid.nid_namelen = strlen(nid.nid_name);
		nid.nid_ngroup = 0;
		nid.nid_grps = NULL;
		nid.nid_flag = NFSID_ADDGROUPNAME;
		error = nfssvc(NFSSVC_IDNAME | NFSSVC_NEWSTRUCT, &nid);
		if (error) {
			info.retval = error;
			syslog(LOG_ERR, "Can't add group %s\n",
			    grp->gr_name);
		} else if (verbose) {
			syslog(LOG_ERR,"Added gid=%d name=%s\n",
			    nid.nid_gid, nid.nid_name);
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_retval,
		    (caddr_t)&info))
			syslog(LOG_ERR, "Can't send reply");
		return;
	default:
		svcerr_noproc(transp);
		return;
	};
}

/*
 * Xdr routine to get an id number
 */
static bool_t
xdr_getid(XDR *xdrsp, caddr_t cp)
{
	struct info *ifp = (struct info *)cp;

	return (xdr_long(xdrsp, &ifp->id));
}

/*
 * Xdr routine to get a user name
 */
static bool_t
xdr_getname(XDR *xdrsp, caddr_t cp)
{
	struct info *ifp = (struct info *)cp;
	long len;

	if (!xdr_long(xdrsp, &len))
		return (0);
	if (len > MAXNAME)
		return (0);
	if (!xdr_opaque(xdrsp, ifp->name, len))
		return (0);
	ifp->name[len] = '\0';
	return (1);
}

/*
 * Xdr routine to return the value.
 */
static bool_t
xdr_retval(XDR *xdrsp, caddr_t cp)
{
	struct info *ifp = (struct info *)cp;
	long val;

	val = ifp->retval;
	return (xdr_long(xdrsp, &val));
}

/*
 * cleanup_term() called via SIGUSR1.
 */
static void
cleanup_term(int signo __unused)
{
	int i, cnt;

	if (im_a_server)
		exit(0);

	/*
	 * Ok, so I'm the master.
	 * As the Governor of California might say, "Terminate them".
	 */
	cnt = 0;
	for (i = 0; i < nfsuserdcnt; i++) {
		if (servers[i] != (pid_t)-1) {
			cnt++;
			kill(servers[i], SIGUSR1);
		}
	}

	/*
	 * and wait for them to die
	 */
	for (i = 0; i < cnt; i++)
		wait3(NULL, 0, NULL);

	/*
	 * Finally, get rid of the socket
	 */
	if (nfssvc(NFSSVC_NFSUSERDDELPORT, NULL) < 0) {
		syslog(LOG_ERR, "Can't do nfssvc() to delete the port\n");
		exit(1);
	}
	exit(0);
}

/*
 * Get the IP address that the localhost address maps to.
 * This is needed when jails map localhost to another IP address.
 */
static int
nfsbind_localhost(void)
{
#ifdef INET
	struct sockaddr_in sin;
#endif
#ifdef INET6
	struct sockaddr_in6 sin6;
#endif
	socklen_t slen;
	int ret, s;

	switch (fromip.ss_family) {
#ifdef INET6
	case AF_INET6:
		s = socket(PF_INET6, SOCK_DGRAM, 0);
		if (s < 0)
			return (0);
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = in6loopback;
		sin6.sin6_port = 0;
		ret = bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
		if (ret < 0) {
			close(s);
			return (0);
		}
		break;
#endif	/* INET6 */
#ifdef INET
	case AF_INET:
		s = socket(PF_INET, SOCK_DGRAM, 0);
		if (s < 0)
			return (0);
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sin.sin_port = 0;
		ret = bind(s, (struct sockaddr *)&sin, sizeof(sin));
		if (ret < 0) {
			close(s);
			return (0);
		}
		break;
#endif	/* INET */
	}
	memset(&fromip, 0, sizeof(fromip));
	slen = sizeof(fromip);
	ret = getsockname(s, (struct sockaddr *)&fromip, &slen);
	close(s);
	if (ret < 0)
		return (0);
	return (1);
}

static void
usage(void)
{

	errx(1,
	    "usage: nfsuserd [-usermax cache_size] [-usertimeout minutes] [-verbose] [-manage-gids] [-domain domain_name] [n]");
}
