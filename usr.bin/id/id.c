/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/param.h>
#include <sys/mac.h>

#ifdef USE_BSM_AUDIT
#include <bsm/audit.h>
#endif

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	id_print(struct passwd *);
static void	pline(struct passwd *);
static void	pretty(struct passwd *);
#ifdef USE_BSM_AUDIT
static void	auditid(void);
#endif
static void	group(struct passwd *, bool);
static void	maclabel(void);
static void	dir(struct passwd *);
static void	shell(struct passwd *);
static void	usage(void);
static struct passwd *who(char *);

static bool isgroups, iswhoami;

int
main(int argc, char *argv[])
{
	struct group *gr;
	struct passwd *pw;
	bool Aflag, Gflag, Mflag, Pflag;
	bool cflag, dflag, gflag, nflag, pflag, rflag, sflag, uflag;
	int ch, combo, error, id;
	const char *myname, *optstr;
	char loginclass[MAXLOGNAME];

	Aflag = Gflag = Mflag = Pflag = false;
	cflag = dflag = gflag = nflag = pflag = rflag = sflag = uflag = false;

	myname = getprogname();
	optstr = "AGMPacdgnprsu";
	if (strcmp(myname, "groups") == 0) {
		isgroups = true;
		optstr = "";
		Gflag = nflag = true;
	}
	else if (strcmp(myname, "whoami") == 0) {
		iswhoami = true;
		optstr = "";
		uflag = nflag = true;
	}

	while ((ch = getopt(argc, argv, optstr)) != -1) {
		switch(ch) {
#ifdef USE_BSM_AUDIT
		case 'A':
			Aflag = true;
			break;
#endif
		case 'G':
			Gflag = true;
			break;
		case 'M':
			Mflag = true;
			break;
		case 'P':
			Pflag = true;
			break;
		case 'a':
			break;
		case 'c':
			cflag = true;
			break;
		case 'd':
			dflag = true;
			break;
		case 'g':
			gflag = true;
			break;
		case 'n':
			nflag = true;
			break;
		case 'p':
			pflag = true;
			break;
		case 'r':
			rflag = true;
			break;
		case 's':
			sflag = true;
			break;
		case 'u':
			uflag = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (iswhoami && argc > 0)
		usage();
	if ((cflag || Aflag || Mflag) && argc > 0)
		usage();

	combo = Aflag + Gflag + Mflag + Pflag + gflag + pflag + uflag;
	if (combo + dflag + sflag > 1)
		usage();
	if (combo > 1)
		usage();
	if (combo == 0 && (nflag || rflag))
		usage();

	pw = *argv ? who(*argv) : NULL;

	if (Mflag && pw != NULL)
		usage();

#ifdef USE_BSM_AUDIT
	if (Aflag) {
		auditid();
		exit(0);
	}
#endif

	if (cflag) {
		error = getloginclass(loginclass, sizeof(loginclass));
		if (error != 0)
			err(1, "loginclass");
		(void)printf("%s\n", loginclass);
		exit(0);
	}

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

	if (dflag) {
		dir(pw);
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

	if (sflag) {
		shell(pw);
		exit(0);
	}

	id_print(pw);
	exit(0);
}

static void
pretty(struct passwd *pw)
{
	struct group *gr;
	u_int eid, rid;
	char *login;

	if (pw) {
		(void)printf("uid\t%s\n", pw->pw_name);
		(void)printf("groups\t");
		group(pw, true);
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
		group(NULL, true);
	}
}

static void
id_print(struct passwd *pw)
{
	struct group *gr;
	gid_t gid, egid, lastgid;
	uid_t uid, euid;
	int cnt, ngroups;
	long ngroups_max;
	gid_t *groups;
	const char *fmt;
	bool print_dbinfo;

	print_dbinfo = pw != NULL;
	if (print_dbinfo) {
		uid = pw->pw_uid;
		gid = pw->pw_gid;
	}
	else {
		uid = getuid();
		gid = getgid();
		pw = getpwuid(uid);
	}

	ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
	if ((groups = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
		err(1, "malloc");

	if (print_dbinfo) {
		ngroups = ngroups_max;
		getgrouplist(pw->pw_name, gid, groups, &ngroups);
	}
	else {
		ngroups = getgroups(ngroups_max, groups);
	}

	/*
	 * We always resolve uids and gids where we can to a name, even if we
	 * are printing the running process credentials, to be nice.
	 */
	if (pw != NULL)
		printf("uid=%u(%s)", uid, pw->pw_name);
	else
		printf("uid=%u", uid);
	printf(" gid=%u", gid);
	if ((gr = getgrgid(gid)))
		(void)printf("(%s)", gr->gr_name);
	if (!print_dbinfo && (euid = geteuid()) != uid) {
		(void)printf(" euid=%u", euid);
		if ((pw = getpwuid(euid)))
			(void)printf("(%s)", pw->pw_name);
	}
	if (!print_dbinfo && (egid = getegid()) != gid) {
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
static void
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
		    "termid_addr.port=0x%08jx\n"
		    "termid_addr.addr[0]=0x%08x\n"
		    "termid_addr.addr[1]=0x%08x\n"
		    "termid_addr.addr[2]=0x%08x\n"
		    "termid_addr.addr[3]=0x%08x\n",
			ainfo_addr.ai_auid, ainfo_addr.ai_mask.am_success,
			ainfo_addr.ai_mask.am_failure, ainfo_addr.ai_asid,
			(uintmax_t)ainfo_addr.ai_termid.at_port,
			ainfo_addr.ai_termid.at_addr[0],
			ainfo_addr.ai_termid.at_addr[1],
			ainfo_addr.ai_termid.at_addr[2],
			ainfo_addr.ai_termid.at_addr[3]);
	} else {
		(void) printf("auid=%d\n"
		    "mask.success=0x%08x\n"
		    "mask.failure=0x%08x\n"
		    "asid=%d\n"
		    "termid.port=0x%08jx\n"
		    "termid.machine=0x%08x\n",
			auditinfo.ai_auid, auditinfo.ai_mask.am_success,
			auditinfo.ai_mask.am_failure,
			auditinfo.ai_asid, (uintmax_t)auditinfo.ai_termid.port,
			auditinfo.ai_termid.machine);
	}
}
#endif

static void
group(struct passwd *pw, bool nflag)
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

static void
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

static struct passwd *
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

static void
pline(struct passwd *pw)
{
	if (pw == NULL) {
		if ((pw = getpwuid(getuid())) == NULL)
			err(1, "getpwuid");
	}
	(void)printf("%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n", pw->pw_name,
	    pw->pw_passwd, pw->pw_uid, pw->pw_gid, pw->pw_class,
	    (long)pw->pw_change, (long)pw->pw_expire, pw->pw_gecos,
	    pw->pw_dir, pw->pw_shell);
}

static void
dir(struct passwd *pw)
{
	if (pw == NULL) {
		if ((pw = getpwuid(getuid())) == NULL)
			err(1, "getpwuid");
	}
	printf("%s\n", pw->pw_dir);
}

static void
shell(struct passwd *pw)
{
	if (pw == NULL) {
		if ((pw = getpwuid(getuid())) == NULL)
			err(1, "getpwuid");
	}
	printf("%s\n", pw->pw_shell);
}

static void
usage(void)
{
	if (isgroups)
		(void)fprintf(stderr, "usage: groups [user]\n");
	else if (iswhoami)
		(void)fprintf(stderr, "usage: whoami\n");
	else
		(void)fprintf(stderr,
		    "usage: id [user]\n"
#ifdef USE_BSM_AUDIT
		    "       id -A\n"
#endif
		    "       id -G [-n] [user]\n"
		    "       id -M\n"
		    "       id -P [user]\n"
		    "       id -c\n"
		    "       id -d [user]\n"
		    "       id -g [-nr] [user]\n"
		    "       id -p [user]\n"
		    "       id -s [user]\n"
		    "       id -u [-nr] [user]\n");
	exit(1);
}
