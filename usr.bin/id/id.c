/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)id.c	8.2 (Berkeley) 2/16/94";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mac.h>

#ifdef USE_BSM_AUDIT
#include <bsm/audit.h>
#endif

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void	id_print(struct passwd *, int, int, int);
void	pline(struct passwd *);
void	pretty(struct passwd *);
void	auditid(void);
void	group(struct passwd *, int);
void	maclabel(void);
void	usage(void);
struct passwd *who(char *);

int isgroups, iswhoami;

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct passwd *pw;
	int Gflag, Mflag, Pflag, ch, gflag, id, nflag, pflag, rflag, uflag;
	int Aflag;
	const char *myname;

	Gflag = Mflag = Pflag = gflag = nflag = pflag = rflag = uflag = 0;
	Aflag = 0;

	myname = strrchr(argv[0], '/');
	myname = (myname != NULL) ? myname + 1 : argv[0];
	if (strcmp(myname, "groups") == 0) {
		isgroups = 1;
		Gflag = nflag = 1;
	}
	else if (strcmp(myname, "whoami") == 0) {
		iswhoami = 1;
		uflag = nflag = 1;
	}

	while ((ch = getopt(argc, argv,
	    (isgroups || iswhoami) ? "" : "APGMagnpru")) != -1)
		switch(ch) {
#ifdef USE_BSM_AUDIT
		case 'A':
			Aflag = 1;
			break;
#endif
		case 'G':
			Gflag = 1;
			break;
		case 'M':
			Mflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'a':
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (iswhoami && argc > 0)
		usage();

	switch(Aflag + Gflag + Mflag + Pflag + gflag + pflag + uflag) {
	case 1:
		break;
	case 0:
		if (!nflag && !rflag)
			break;
		/* FALLTHROUGH */
	default:
		usage();
	}

	pw = *argv ? who(*argv) : NULL;

	if (Mflag && pw != NULL)
		usage();

#ifdef USE_BSM_AUDIT
	if (Aflag) {
		auditid();
		exit(0);
	}
#endif

	if (gflag) {
		id = pw ? pw->pw_gid : rflag ? getgid() : getegid();
		if (nflag && (gr = getgrgid(id)))
			(void)printf("%s\n", gr->gr_name);
		else
			(void)printf("%u\n", id);
		exit(0);
	}

	if (uflag) {
		id = pw ? pw->pw_uid : rflag ? getuid() : geteuid();
		if (nflag && (pw = getpwuid(id)))
			(void)printf("%s\n", pw->pw_name);
		else
			(void)printf("%u\n", id);
		exit(0);
	}

	if (Gflag) {
		group(pw, nflag);
		exit(0);
	}

	if (Mflag) {
		maclabel();
		exit(0);
	}

	if (Pflag) {
		pline(pw);
		exit(0);
	}

	if (pflag) {
		pretty(pw);
		exit(0);
	}

	if (pw) {
		id_print(pw, 1, 0, 0);
	}
	else {
		id = getuid();
		pw = getpwuid(id);
		id_print(pw, 0, 1, 1);
	}
	exit(0);
}

void
pretty(struct passwd *pw)
{
	struct group *gr;
	u_int eid, rid;
	char *login;

	if (pw) {
		(void)printf("uid\t%s\n", pw->pw_name);
		(void)printf("groups\t");
		group(pw, 1);
	} else {
		if ((login = getlogin()) == NULL)
			err(1, "getlogin");

		pw = getpwuid(rid = getuid());
		if (pw == NULL || strcmp(login, pw->pw_name))
			(void)printf("login\t%s\n", login);
		if (pw)
			(void)printf("uid\t%s\n", pw->pw_name);
		else
			(void)printf("uid\t%u\n", rid);

		if ((eid = geteuid()) != rid) {
			if ((pw = getpwuid(eid)))
				(void)printf("euid\t%s\n", pw->pw_name);
			else
				(void)printf("euid\t%u\n", eid);
		}
		if ((rid = getgid()) != (eid = getegid())) {
			if ((gr = getgrgid(rid)))
				(void)printf("rgid\t%s\n", gr->gr_name);
			else
				(void)printf("rgid\t%u\n", rid);
		}
		(void)printf("groups\t");
		group(NULL, 1);
	}
}

void
id_print(struct passwd *pw, int use_ggl, int p_euid, int p_egid)
{
	struct group *gr;
	gid_t gid, egid, lastgid;
	uid_t uid, euid;
	int cnt, ngroups;
	long ngroups_max;
	gid_t *groups;
	const char *fmt;

	if (pw != NULL) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	else {
		uid = getuid();
		gid = getgid();
	}

	ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
	if ((groups = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
		err(1, "malloc");

	if (use_ggl && pw != NULL) {
		ngroups = ngroups_max;
		getgrouplist(pw->pw_name, gid, groups, &ngroups);
	}
	else {
		ngroups = getgroups(ngroups_max, groups);
	}

	if (pw != NULL)
		printf("uid=%u(%s)", uid, pw->pw_name);
	else 
		printf("uid=%u", getuid());
	printf(" gid=%u", gid);
	if ((gr = getgrgid(gid)))
		(void)printf("(%s)", gr->gr_name);
	if (p_euid && (euid = geteuid()) != uid) {
		(void)printf(" euid=%u", euid);
		if ((pw = getpwuid(euid)))
			(void)printf("(%s)", pw->pw_name);
	}
	if (p_egid && (egid = getegid()) != gid) {
		(void)printf(" egid=%u", egid);
		if ((gr = getgrgid(egid)))
			(void)printf("(%s)", gr->gr_name);
	}
	fmt = " groups=%u";
	for (lastgid = -1, cnt = 0; cnt < ngroups; ++cnt) {
		if (lastgid == (gid = groups[cnt]))
			continue;
		printf(fmt, gid);
		fmt = ",%u";
		if ((gr = getgrgid(gid)))
			printf("(%s)", gr->gr_name);
		lastgid = gid;
	}
	printf("\n");
	free(groups);
}

#ifdef USE_BSM_AUDIT
void
auditid(void)
{
	auditinfo_t auditinfo;
	auditinfo_addr_t ainfo_addr;
	int ret, extended;

	extended = 0;
	ret = getaudit(&auditinfo);
	if (ret < 0 && errno == E2BIG) {
		if (getaudit_addr(&ainfo_addr, sizeof(ainfo_addr)) < 0)
			err(1, "getaudit_addr");
		extended = 1;
	} else if (ret < 0)
		err(1, "getaudit");
	if (extended != 0) {
		(void) printf("auid=%d\n"
		    "mask.success=0x%08x\n"
		    "mask.failure=0x%08x\n"
		    "asid=%d\n"
		    "termid_addr.port=0x%08x\n"
		    "termid_addr.addr[0]=0x%08x\n"
		    "termid_addr.addr[1]=0x%08x\n"
		    "termid_addr.addr[2]=0x%08x\n"
		    "termid_addr.addr[3]=0x%08x\n",
			ainfo_addr.ai_auid, ainfo_addr.ai_mask.am_success,
			ainfo_addr.ai_mask.am_failure, ainfo_addr.ai_asid,
			ainfo_addr.ai_termid.at_port,
			ainfo_addr.ai_termid.at_addr[0],
			ainfo_addr.ai_termid.at_addr[1],
			ainfo_addr.ai_termid.at_addr[2],
			ainfo_addr.ai_termid.at_addr[3]);
	} else {
		(void) printf("auid=%d\n"
		    "mask.success=0x%08x\n"
		    "mask.failure=0x%08x\n"
		    "asid=%d\n"
		    "termid.port=0x%08x\n"
		    "termid.machine=0x%08x\n",
			auditinfo.ai_auid, auditinfo.ai_mask.am_success,
			auditinfo.ai_mask.am_failure,
			auditinfo.ai_asid, auditinfo.ai_termid.port,
			auditinfo.ai_termid.machine);
	}
}
#endif

void
group(struct passwd *pw, int nflag)
{
	struct group *gr;
	int cnt, id, lastid, ngroups;
	long ngroups_max;
	gid_t *groups;
	const char *fmt;

	ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
	if ((groups = malloc(sizeof(gid_t) * (ngroups_max))) == NULL)
		err(1, "malloc");

	if (pw) {
		ngroups = ngroups_max;
		(void) getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	} else {
		ngroups = getgroups(ngroups_max, groups);
	}
	fmt = nflag ? "%s" : "%u";
	for (lastid = -1, cnt = 0; cnt < ngroups; ++cnt) {
		if (lastid == (id = groups[cnt]))
			continue;
		if (nflag) {
			if ((gr = getgrgid(id)))
				(void)printf(fmt, gr->gr_name);
			else
				(void)printf(*fmt == ' ' ? " %u" : "%u",
				    id);
			fmt = " %s";
		} else {
			(void)printf(fmt, id);
			fmt = " %u";
		}
		lastid = id;
	}
	(void)printf("\n");
	free(groups);
}

void
maclabel(void)
{
	char *string;
	mac_t label;
	int error;

	error = mac_prepare_process_label(&label);
	if (error == -1)
		errx(1, "mac_prepare_type: %s", strerror(errno));

	error = mac_get_proc(label);
	if (error == -1)
		errx(1, "mac_get_proc: %s", strerror(errno));

	error = mac_to_text(label, &string);
	if (error == -1)
		errx(1, "mac_to_text: %s", strerror(errno));

	(void)printf("%s\n", string);
	mac_free(label);
	free(string);
}

struct passwd *
who(char *u)
{
	struct passwd *pw;
	long id;
	char *ep;

	/*
	 * Translate user argument into a pw pointer.  First, try to
	 * get it as specified.  If that fails, try it as a number.
	 */
	if ((pw = getpwnam(u)))
		return(pw);
	id = strtol(u, &ep, 10);
	if (*u && !*ep && (pw = getpwuid(id)))
		return(pw);
	errx(1, "%s: no such user", u);
	/* NOTREACHED */
}

void
pline(struct passwd *pw)
{

	if (!pw) {
		if ((pw = getpwuid(getuid())) == NULL)
			err(1, "getpwuid");
	}

	(void)printf("%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n", pw->pw_name,
			pw->pw_passwd, pw->pw_uid, pw->pw_gid, pw->pw_class,
			(long)pw->pw_change, (long)pw->pw_expire, pw->pw_gecos,
			pw->pw_dir, pw->pw_shell);
}


void
usage(void)
{

	if (isgroups)
		(void)fprintf(stderr, "usage: groups [user]\n");
	else if (iswhoami)
		(void)fprintf(stderr, "usage: whoami\n");
	else
		(void)fprintf(stderr, "%s\n%s%s\n%s\n%s\n%s\n%s\n%s\n",
		    "usage: id [user]",
#ifdef USE_BSM_AUDIT
		    "       id -A\n",
#else
		    "",
#endif
		    "       id -G [-n] [user]",
		    "       id -M",
		    "       id -P [user]",
		    "       id -g [-nr] [user]",
		    "       id -p [user]",
		    "       id -u [-nr] [user]");
	exit(1);
}
