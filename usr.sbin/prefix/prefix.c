/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/prefix/prefix.c,v 1.2.2.1 2000/07/15 07:36:49 kris Exp $
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800
struct	in6_prefixreq	prereq = {{NULL}, /* interface name */
				      PR_ORIG_STATIC,
				      64, /* default plen */
				      2592000, /* vltime=30days */
				      604800, /* pltime=7days */
				      /* ra onlink=1 autonomous=1 */
				      {{1,1,0}},
				      {NULL}};  /* prefix */
struct	in6_rrenumreq	rrreq = {{NULL}, /* interface name */
					 PR_ORIG_STATIC, /* default origin */
					 64, /* default match len */
					 0, /* default min match len */
					 128, /* default max match len */
					 0, /* default uselen */
					 0, /* default keeplen */
				         {1,1,0}, /* default raflag mask */
					 2592000, /* vltime=30days */
					 604800, /* pltime=7days */
					 /* ra onlink=1 autonomous=1 */
					 {{1,1,0}},
					 {NULL}, /* match prefix */
					 {NULL} /* use prefix */
					 };

#define C(x) ((caddr_t) &x)

struct prefix_cmds {
	const char *errmsg;
	int cmd;
	caddr_t req;
} prcmds[] = {
	{"SIOCSIFPREFIX_IN6 failed", SIOCSIFPREFIX_IN6, C(prereq)},
	{"SIOCDIFPREFIX_IN6 failed", SIOCDIFPREFIX_IN6, C(prereq)},
	{"SIOCAIFPREFIX_IN6 failed", SIOCAIFPREFIX_IN6, C(rrreq)},
	{"SIOCCIFPREFIX_IN6 failed", SIOCCIFPREFIX_IN6, C(rrreq)},
	{"SIOCSGIFPREFIX_IN6 failed", SIOCSGIFPREFIX_IN6, C(rrreq)}
};

#define PREF_CMD_SET		0
#define PREF_CMD_DELETE		1
#define PREF_CMD_ADD		2
#define PREF_CMD_CHANGE		3
#define PREF_CMD_SETGLOBAL	4
#define PREF_CMD_MAX		5

u_int	prcmd = PREF_CMD_SET;	/* default command */

char	name[32];
int	flags;

int	newprefix_setdel, newprefix_match, newprefix_use, newprefix_uselen,
	newprefix_keeplen;

void	Perror __P((const char *cmd));
int	prefix __P((int argc, char *const *argv));
void	usage __P((void));
void	setlifetime __P((const char *atime, u_int32_t *btime));
int	all, explicit_prefix = 0;

typedef	void c_func __P((const char *cmd, int arg));
c_func	set_vltime, set_pltime, set_raf_onlink,
	set_raf_auto, set_rrf_decrvalid, set_rrf_decrprefd,
	get_setdelprefix, get_matchprefix, get_useprefix, set_matchlen,
	set_match_minlen, set_match_maxlen,
	set_use_uselen, set_use_keeplen, set_prefix_cmd;

void getprefixlen __P((const char *, int));
void getprefix __P((const char *, int));

#define	NEXTARG		0xffffff

const
struct	cmd {
	const	char *c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func) __P((const char *, int));
} cmds[] = {
	{ "set",	PREF_CMD_SET, set_prefix_cmd },
	{ "delete",	PREF_CMD_DELETE, set_prefix_cmd },
	{ "prefixlen",	NEXTARG,	getprefixlen },
	{ "add",	PREF_CMD_ADD, set_prefix_cmd },
	{ "change",	PREF_CMD_CHANGE, set_prefix_cmd },
	{ "setglobal",	PREF_CMD_SETGLOBAL, set_prefix_cmd },
	{ "matchpr",	NEXTARG,	get_matchprefix },
	{ "usepr",	NEXTARG,	get_useprefix },
	{ "mp_len",	NEXTARG,	set_matchlen },
	{ "mp_minlen",	NEXTARG,	set_match_minlen },
	{ "mp_maxlen",	NEXTARG,	set_match_maxlen },
	{ "up_uselen",	NEXTARG,	set_use_uselen },
	{ "up_keeplen",	NEXTARG,	set_use_keeplen },
	{ "vltime",	NEXTARG,	set_vltime },
	{ "pltime",	NEXTARG,	set_pltime },
	{ "raf_onlink",		1,	set_raf_onlink },
	{ "-raf_onlink",	0,	set_raf_onlink },
	{ "raf_auto",		1,	set_raf_auto },
	{ "-raf_auto",		0,	set_raf_auto },
	{ "rrf_decrvalid",	1,	set_rrf_decrvalid },
	{ "-rrf_decrvalid",	0,	set_rrf_decrvalid },
	{ "rrf_decrprefd",	1,	set_rrf_decrprefd },
	{ "-rrf_decrprefd",	0,	set_rrf_decrprefd },
	{ 0,			0,	get_setdelprefix },
	{ 0,	0,	0 },
};


void
usage()
{
	fprintf(stderr, "%s",
	"usage: prefix interface prefix_value [parameters] [set|delete]\n"
	"       prefix interface\n"
	"                matchpr matchpr_value mp_len mp_len_value\n"
	"                usepr usepr_value up_uselen up_uselen_value\n"
	"                [parameters] [add|change|setglobal]\n"
	"       prefix -a [-d] [-u]\n"
	"                matchpr matchpr_value mp_len mp_len_value\n"
	"                usepr usepr_value up_uselen up_uselen_value\n"
	"                [parameters] [add|change|setglobal]\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *const *argv;
{
	int c;
	int downonly, uponly;
	int foundit = 0;
	int addrcount;
	struct	if_msghdr *ifm, *nextifm;
	struct	ifa_msghdr *ifam;
	struct	sockaddr_dl *sdl;
	char	*buf, *lim, *next;


	size_t needed;
	int mib[6];

	/* Parse leading line options */
	all = downonly = uponly = 0;
	while ((c = getopt(argc, argv, "adu")) != -1) {
		switch (c) {
		case 'a':	/* scan all interfaces */
			all++;
			break;
		case 'd':	/* restrict scan to "down" interfaces */
			downonly++;
			break;
		case 'u':	/* restrict scan to "up" interfaces */
			uponly++;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* nonsense.. */
	if (uponly && downonly)
		usage();

	if (!all) {
		/* not listing, need an argument */
		if (argc < 1)
			usage();

		strncpy(name, *argv, sizeof(name));
		argc--, argv++;
	}

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;	/* address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		errx(1, "iflist-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		errx(1, "actual retrieval of interface table");
	lim = buf + needed;

	next = buf;
	while (next < lim) {

		ifm = (struct if_msghdr *)next;
		
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
		} else {
			fprintf(stderr, "out of sync parsing NET_RT_IFLIST\n");
			fprintf(stderr, "expected %d, got %d\n", RTM_IFINFO,
				ifm->ifm_type);
			fprintf(stderr, "msglen = %d\n", ifm->ifm_msglen);
			fprintf(stderr, "buf:%p, next:%p, lim:%p\n", buf, next,
				lim);
			exit (1);
		}

		next += ifm->ifm_msglen;
		ifam = NULL;
		addrcount = 0;
		while (next < lim) {

			nextifm = (struct if_msghdr *)next;

			if (nextifm->ifm_type != RTM_NEWADDR)
				break;

			if (ifam == NULL)
				ifam = (struct ifa_msghdr *)nextifm;

			addrcount++;
			next += nextifm->ifm_msglen;
		}

		if (all) {
			if (uponly)
				if ((flags & IFF_UP) == 0)
					continue; /* not up */
			if (downonly)
				if (flags & IFF_UP)
					continue; /* not down */
			strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
			name[sdl->sdl_nlen] = '\0';
		} else {
			if (strlen(name) != sdl->sdl_nlen)
				continue; /* not same len */
			if (strncmp(name, sdl->sdl_data, sdl->sdl_nlen) != 0)
				continue; /* not same name */
		}

		if (argc > 0)
			prefix(argc, argv);
#if 0
		else {
			/* TODO: print prefix status by sysctl */
		}
#endif

		if (all == 0) {
			foundit++; /* flag it as 'done' */
			break;
		}
	}
	free(buf);

	if (all == 0 && foundit == 0)
		errx(1, "interface %s does not exist", name);


	exit (0);
}


int
prefix(int argc, char *const *argv)
{
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	while (argc > 0) {
		register const struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_func) {
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0);
				argc--, argv++;
			} else
				(*p->c_func)(*argv, p->c_parameter);
		}
		argc--, argv++;
	}
	if (prcmd > PREF_CMD_MAX) {
		Perror("ioctl: unknown prefix cmd");
		goto end;
	}
	if (prcmd == PREF_CMD_SET || prcmd == PREF_CMD_DELETE) {
		if (!newprefix_setdel)
			usage();
	} else { /* ADD|CHANGE|SETGLOBAL */
		if (!newprefix_match)
			usage();
	}
	if (!newprefix_use)
		rrreq.irr_u_uselen = 0; /* make clear that no use_prefix */
	else if (newprefix_keeplen == NULL && rrreq.irr_u_uselen < 64)
		/* init keeplen to make uselen + keeplen equal 64 */
		rrreq.irr_u_keeplen = 64 - rrreq.irr_u_uselen;
	if (explicit_prefix == 0) {
		/* Aggregatable address architecture defines all prefixes
		   are 64. So, it is convenient to set prefixlen to 64 if
		   it is not specified. */
		getprefixlen("64", 0);
	}
	strncpy(prcmds[prcmd].req, name, IFNAMSIZ);
	if (ioctl(s, prcmds[prcmd].cmd, prcmds[prcmd].req) < 0) {
		if (all && errno == EADDRNOTAVAIL)
			goto end;
		Perror(prcmds[prcmd].errmsg);
	}
    end:
	close(s);
	return(0);
}
#define PREFIX	0
#define MPREFIX	1
#define UPREFIX	2

void
Perror(cmd)
	const char *cmd;
{
	switch (errno) {

	case ENXIO:
		errx(1, "%s: no such interface", cmd);
		break;

	case EPERM:
		errx(1, "%s: permission denied", cmd);
		break;

	default:
		err(1, "%s", cmd);
	}
}

#define SIN6(x) ((struct sockaddr_in6 *) &(x))
struct sockaddr_in6 *sin6tab[] = {
SIN6(prereq.ipr_prefix), SIN6(rrreq.irr_matchprefix),
SIN6(rrreq.irr_useprefix)};

void
getprefixlen(const char *plen, int unused)
{
	int len = atoi(plen);

	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);

	/* set plen for prereq */
	prereq.ipr_plen = len;
	explicit_prefix = 1;
}

void
getprefix(const char *prefix, int which)
{
	register struct sockaddr_in6 *sin = sin6tab[which];

	/*
	 * Delay the ioctl to set the interface prefix until flags are all set.
	 * The prefix interpretation may depend on the flags,
	 * and the flags may change when the prefix is set.
	 */

	sin->sin6_len = sizeof(*sin);
	sin->sin6_family = AF_INET6;

        if (inet_pton(AF_INET6, prefix, &sin->sin6_addr) != 1)
		errx(1, "%s: bad value", prefix);
}

void
get_setdelprefix(const char *prefix, int unused)
{
	newprefix_setdel++;
	getprefix(prefix, PREFIX);
}

void
get_matchprefix(const char *prefix, int unused)
{
	newprefix_match++;
	prcmd = (prcmd == PREF_CMD_SET) ? PREF_CMD_ADD : prcmd;
	getprefix(prefix, MPREFIX);
}

void
get_useprefix(const char *prefix, int unused)
{
	newprefix_use++;
	if (newprefix_uselen == 0)
		rrreq.irr_u_uselen = 64;
	getprefix(prefix, UPREFIX);
}

static int
get_plen(const char *plen)
{
	int len;

	len = atoi(plen);
	if ((len < 0) || (len > 128))
		errx(1, "%s: bad value", plen);
	return len;
}

void
set_matchlen(const char *plen, int unused)
{
	rrreq.irr_m_len = get_plen(plen);
}

void
set_match_minlen(const char *plen, int unused)
{
	rrreq.irr_m_minlen = get_plen(plen);
}

void
set_match_maxlen(const char *plen, int unused)
{
	rrreq.irr_m_maxlen = get_plen(plen);
}

void
set_use_uselen(const char *plen, int unused)
{
	newprefix_uselen++;
	rrreq.irr_u_uselen = get_plen(plen);
}

void
set_use_keeplen(const char *plen, int unused)
{
	newprefix_keeplen++;
	rrreq.irr_u_keeplen = get_plen(plen);
}

void
set_vltime(const char *ltime, int unused)
{
	setlifetime(ltime, &prereq.ipr_vltime);
	rrreq.irr_vltime = prereq.ipr_vltime;
}

void
set_pltime(const char *ltime, int unused)
{
	setlifetime(ltime, &prereq.ipr_pltime);
	rrreq.irr_pltime = prereq.ipr_pltime;
}

void
set_raf_onlink(const char *unused, int value)
{
	/* raflagmask is only meaningful when newprefix_rrenum */
	rrreq.irr_raflagmask.onlink = 1;
	prereq.ipr_flags.prf_ra.onlink =
	rrreq.irr_flags.prf_ra.onlink = value ? 1 : 0;
}

void
set_raf_auto(const char *unused, int value)
{
	/* only meaningful when newprefix_rrenum */
	rrreq.irr_raflagmask.autonomous = 1;
	prereq.ipr_flags.prf_ra.autonomous =
	rrreq.irr_flags.prf_ra.autonomous = value ? 1 : 0;
}

void
set_rrf_decrvalid(const char *unused, int value)
{
	prereq.ipr_flags.prf_rr.decrvalid =
	rrreq.irr_flags.prf_rr.decrvalid = value ? 1 : 0;
}

void
set_rrf_decrprefd(const char *unused, int value)
{
	prereq.ipr_flags.prf_rr.decrprefd =
	rrreq.irr_flags.prf_rr.decrprefd = value ? 1 : 0;
}

void
set_prefix_cmd(const char *unused, int cmd)
{
	prcmd = cmd;
}

void
setlifetime(const char *atime, u_int32_t *btime)
{
	int days = 0, hours = 0, minutes = 0, seconds = 0;
	u_int32_t ttime;
	char *check;

	if (strcmp(atime, "infinity") == 0) {
		*btime = 0xffffffff;
		return;
	}
	ttime = strtoul(atime, &check ,10) & 0xffffffff;
	if (*check == '\0') {
		*btime = ttime;
		return;
	}
	if (sscanf(atime, "d%2dh%2dm%2ds%2d", &days, &hours, &minutes,
		   &seconds) < 0) {
		Perror("wrong time format: valid is d00h00m00s00, \n"
		       "where 00 can be any octal number, \n"
		       "\'d\' is for days, \'h\' is for hours, \n"
		       "\'m\' is for minutes, and \'s\' is for seconds \n");
		return;
	}
	*btime = 0;
	*btime += seconds;
	*btime += minutes * 60;
	*btime += hours * 3600;
	*btime += days * 86400;
	return;
}
