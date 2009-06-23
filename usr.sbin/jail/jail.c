/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2009 James Gritton
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

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <login_cap.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SJPARAM		"security.jail.param"
#define	ERRMSG_SIZE	256

struct param {
	struct iovec name;
	struct iovec value;
};

static struct param *params;
static char **param_values;
static int nparams;

static char *ip4_addr;
#ifdef INET6
static char *ip6_addr;
#endif

static void add_ip_addr(char **addrp, char *newaddr);
#ifdef INET6
static void add_ip_addr46(char *newaddr);
#endif
static void add_ip_addrinfo(int ai_flags, char *value);
static void quoted_print(FILE *fp, char *str);
static void set_param(const char *name, char *value);
static void usage(void);

static const char *perm_sysctl[][3] = {
	{ "security.jail.set_hostname_allowed",
	  "allow.noset_hostname", "allow.set_hostname" },
	{ "security.jail.sysvipc_allowed",
	  "allow.nosysvipc", "allow.sysvipc" },
	{ "security.jail.allow_raw_sockets",
	  "allow.noraw_sockets", "allow.raw_sockets" },
	{ "security.jail.chflags_allowed",
	  "allow.nochflags", "allow.chflags" },
	{ "security.jail.mount_allowed",
	  "allow.nomount", "allow.mount" },
	{ "security.jail.socket_unixiproute_only",
	  "allow.socket_af", "allow.nosocket_af" },
};

extern char **environ;

#define GET_USER_INFO do {						\
	pwd = getpwnam(username);					\
	if (pwd == NULL) {						\
		if (errno)						\
			err(1, "getpwnam: %s", username);		\
		else							\
			errx(1, "%s: no such user", username);		\
	}								\
	lcap = login_getpwclass(pwd);					\
	if (lcap == NULL)						\
		err(1, "getpwclass: %s", username);			\
	ngroups = ngroups_max;						\
	if (getgrouplist(username, pwd->pw_gid, groups, &ngroups) != 0)	\
		err(1, "getgrouplist: %s", username);			\
} while (0)

int
main(int argc, char **argv)
{
	login_cap_t *lcap = NULL;
	struct iovec rparams[2];
	struct passwd *pwd = NULL;
	gid_t *groups;
	size_t sysvallen;
	int ch, cmdarg, i, jail_set_flags, jid, ngroups, sysval;
	int hflag, iflag, Jflag, lflag, rflag, uflag, Uflag;
	long ngroups_max;
	unsigned pi;
	char *ep, *jailname, *securelevel, *username, *JidFile;
	char errmsg[ERRMSG_SIZE], enforce_statfs[4];
	static char *cleanenv;
	const char *shell, *p = NULL;
	FILE *fp;

	hflag = iflag = Jflag = lflag = rflag = uflag = Uflag =
	    jail_set_flags = 0;
	cmdarg = jid = -1;
	jailname = securelevel = username = JidFile = cleanenv = NULL;
	fp = NULL;

	ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;	
	if ((groups = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
		err(1, "malloc");

	while ((ch = getopt(argc, argv, "cdhilmn:r:s:u:U:J:")) != -1) {
		switch (ch) {
		case 'd':
			jail_set_flags |= JAIL_DYING;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'J':
			JidFile = optarg;
			Jflag = 1;
			break;
		case 'n':
			jailname = optarg;
			break;
		case 's':
			securelevel = optarg;
			break;
		case 'u':
			username = optarg;
			uflag = 1;
			break;
		case 'U':
			username = optarg;
			Uflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'c':
			jail_set_flags |= JAIL_CREATE;
			break;
		case 'm':
			jail_set_flags |= JAIL_UPDATE;
			break;
		case 'r':
			jid = strtoul(optarg, &ep, 10);
			if (!*optarg || *ep) {
				*(const void **)&rparams[0].iov_base = "name";
				rparams[0].iov_len = sizeof("name");
				rparams[1].iov_base = optarg;
				rparams[1].iov_len = strlen(optarg) + 1;
				jid = jail_get(rparams, 2, 0);
				if (jid < 0)
					errx(1, "unknown jail: %s", optarg);
			}
			rflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (rflag) {
		if (argc > 0 || iflag || Jflag || lflag || uflag || Uflag)
			usage();
		if (jail_remove(jid) < 0)
			err(1, "jail_remove");
		exit (0);
	}
	if (argc == 0)
		usage();
	if (uflag && Uflag)
		usage();
	if (lflag && username == NULL)
		usage();
	if (uflag)
		GET_USER_INFO;

	if (jailname)
		set_param("name", jailname);
	if (securelevel)
		set_param("securelevel", securelevel);
	if (jail_set_flags) {
		for (i = 0; i < argc; i++) {
			if (!strncmp(argv[i], "command=", 8)) {
				cmdarg = i;
				argv[cmdarg] += 8;
				jail_set_flags |= JAIL_ATTACH;
				break;
			}
			if (hflag) {
				if (!strncmp(argv[i], "ip4.addr=", 9)) {
					add_ip_addr(&ip4_addr, argv[i] + 9);
					break;
				}
#ifdef INET6
				if (!strncmp(argv[i], "ip6.addr=", 9)) {
					add_ip_addr(&ip6_addr, argv[i] + 9);
					break;
				}
#endif
				if (!strncmp(argv[i], "host.hostname=", 14))
					add_ip_addrinfo(0, argv[i] + 14);
			}
			set_param(NULL, argv[i]);
		}
	} else {
		if (argc < 4 || argv[0][0] != '/')
			errx(1, "%s\n%s",
			   "no -c or -m, so this must be an old-style command.",
			   "But it doesn't look like one.");
		set_param("path", argv[0]);
		set_param("host.hostname", argv[1]);
		if (hflag)
			add_ip_addrinfo(0, argv[1]);
#ifdef INET6
		add_ip_addr46(argv[2]);
#else
		add_ip_addr(&ip4_addr, argv[2]);
#endif
		cmdarg = 3;
		/* Emulate the defaults from security.jail.* sysctls */
		sysvallen = sizeof(sysval);
		if (sysctlbyname("security.jail.jailed", &sysval, &sysvallen,
		    NULL, 0) == 0 && sysval == 0) {
			for (pi = 0; pi < sizeof(perm_sysctl) /
			     sizeof(perm_sysctl[0]); pi++) {
				sysvallen = sizeof(sysval);
				if (sysctlbyname(perm_sysctl[pi][0],
				    &sysval, &sysvallen, NULL, 0) == 0)
					set_param(perm_sysctl[pi]
					    [sysval ? 2 : 1], NULL);
			}
			sysvallen = sizeof(sysval);
			if (sysctlbyname("security.jail.enforce_statfs",
			    &sysval, &sysvallen, NULL, 0) == 0) {
				snprintf(enforce_statfs,
				    sizeof(enforce_statfs), "%d", sysval);
				set_param("enforce_statfs", enforce_statfs);
			}
		}
	}
	if (ip4_addr != NULL)
		set_param("ip4.addr", ip4_addr);
#ifdef INET6
	if (ip6_addr != NULL)
		set_param("ip6.addr", ip6_addr);
#endif
	errmsg[0] = 0;
	set_param("errmsg", errmsg);

	if (Jflag) {
		fp = fopen(JidFile, "w");
		if (fp == NULL)
			errx(1, "Could not create JidFile: %s", JidFile);
	}
	jid = jail_set(&params->name, 2 * nparams,
	    jail_set_flags ? jail_set_flags : JAIL_CREATE | JAIL_ATTACH);
	if (jid < 0) {
		if (errmsg[0] != '\0')
			errx(1, "%s", errmsg);
		err(1, "jail_set");
	}
	if (iflag) {
		printf("%d\n", jid);
		fflush(stdout);
	}
	if (Jflag) {
		if (jail_set_flags) {
			fprintf(fp, "jid=%d", jid);
			for (i = 0; i < nparams; i++)
				if (strcmp(params[i].name.iov_base, "jid") &&
				    strcmp(params[i].name.iov_base, "errmsg")) {
					fprintf(fp, " %s",
					    (char *)params[i].name.iov_base);
					if (param_values[i]) {
						putc('=', fp);
						quoted_print(fp,
						    param_values[i]);
					}
				}
			fprintf(fp, "\n");
		} else {
			for (i = 0; i < nparams; i++)
				if (!strcmp(params[i].name.iov_base, "path"))
					break;
#ifdef INET6
			fprintf(fp, "%d\t%s\t%s\t%s%s%s\t%s\n",
			    jid, i < nparams
			    ? (char *)params[i].value.iov_base : argv[0],
			    argv[1], ip4_addr ? ip4_addr : "",
			    ip4_addr && ip4_addr[0] && ip6_addr && ip6_addr[0]
			    ? "," : "", ip6_addr ? ip6_addr : "", argv[3]);
#else
			fprintf(fp, "%d\t%s\t%s\t%s\t%s\n",
			    jid, i < nparams
			    ? (char *)params[i].value.iov_base : argv[0],
			    argv[1], ip4_addr ? ip4_addr : "", argv[3]);
#endif
		}
		(void)fclose(fp);
	}
	if (cmdarg < 0)
		exit(0);
	if (username != NULL) {
		if (Uflag)
			GET_USER_INFO;
		if (lflag) {
			p = getenv("TERM");
			environ = &cleanenv;
		}
		if (setgroups(ngroups, groups) != 0)
			err(1, "setgroups");
		if (setgid(pwd->pw_gid) != 0)
			err(1, "setgid");
		if (setusercontext(lcap, pwd, pwd->pw_uid,
		    LOGIN_SETALL & ~LOGIN_SETGROUP & ~LOGIN_SETLOGIN) != 0)
			err(1, "setusercontext");
		login_close(lcap);
	}
	if (lflag) {
		if (*pwd->pw_shell)
			shell = pwd->pw_shell;
		else
			shell = _PATH_BSHELL;
		if (chdir(pwd->pw_dir) < 0)
			errx(1, "no home directory");
		setenv("HOME", pwd->pw_dir, 1);
		setenv("SHELL", shell, 1);
		setenv("USER", pwd->pw_name, 1);
		if (p)
			setenv("TERM", p, 1);
	}
	execvp(argv[cmdarg], argv + cmdarg);
	err(1, "execvp: %s", argv[cmdarg]);
}

static void
add_ip_addr(char **addrp, char *value)
{
	int addrlen;
	char *addr;

	if (!*addrp) {
		*addrp = strdup(value);
		if (!*addrp)
			err(1, "malloc");
	} else if (value[0]) {
		addrlen = strlen(*addrp) + strlen(value) + 2;
		addr = malloc(addrlen);
		if (!addr)
			err(1, "malloc");
		snprintf(addr, addrlen, "%s,%s", *addrp, value);
		free(*addrp);
		*addrp = addr;
	}
}

#ifdef INET6
static void
add_ip_addr46(char *value)
{
	char *p, *np;

	if (!value[0]) {
		add_ip_addr(&ip4_addr, value);
		add_ip_addr(&ip6_addr, value);
		return;
	}
	for (p = value;; p = np + 1)
	{
		np = strchr(p, ',');
		if (np)
			*np = '\0';
		add_ip_addrinfo(AI_NUMERICHOST, p);
		if (!np)
			break;
	}
}
#endif

static void
add_ip_addrinfo(int ai_flags, char *value)
{
	struct addrinfo hints, *ai0, *ai;
	struct in_addr addr4;
	int error;
	char avalue4[INET_ADDRSTRLEN];
#ifdef INET6
	struct in6_addr addr6;
	char avalue6[INET6_ADDRSTRLEN];
#endif

	/* Look up the hostname (or get the address) */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
#ifdef INET6
	hints.ai_family = PF_UNSPEC;
#else
	hints.ai_family = PF_INET;
#endif
	hints.ai_flags = ai_flags;
	error = getaddrinfo(value, NULL, &hints, &ai0);
	if (error != 0)
		errx(1, "hostname %s: %s", value, gai_strerror(error));
	
	/* Convert the addresses to ASCII so set_param can convert them back. */
	for (ai = ai0; ai; ai = ai->ai_next)
		switch (ai->ai_family) {
		case AF_INET:
			memcpy(&addr4, &((struct sockaddr_in *)
			    (void *)ai->ai_addr)->sin_addr, sizeof(addr4));
			if (inet_ntop(AF_INET, &addr4, avalue4,
			    INET_ADDRSTRLEN) == NULL)
				err(1, "inet_ntop");
			add_ip_addr(&ip4_addr, avalue4);
			break;
#ifdef INET6
		case AF_INET6:
			memcpy(&addr6, &((struct sockaddr_in6 *)
			    (void *)ai->ai_addr)->sin6_addr, sizeof(addr6));
			if (inet_ntop(AF_INET6, &addr6, avalue6,
			    INET6_ADDRSTRLEN) == NULL)
				err(1, "inet_ntop");
			add_ip_addr(&ip6_addr, avalue6);
			break;
#endif
		}
	freeaddrinfo(ai0);
}

static void
quoted_print(FILE *fp, char *str)
{
	int c, qc;
	char *p = str;

	/* An empty string needs quoting. */
	if (!*p) {
		fputs("\"\"", fp);
		return;
	}

	/*
	 * The value will be surrounded by quotes if it contains spaces
	 * or quotes.
	 */
	qc = strchr(p, '\'') ? '"'
	    : strchr(p, '"') ? '\''
	    : strchr(p, ' ') || strchr(p, '\t') ? '"'
	    : 0;
	if (qc)
		putc(qc, fp);
	while ((c = *p++)) {
		if (c == '\\' || c == qc)
			putc('\\', fp);
		putc(c, fp);
	}
	if (qc)
		putc(qc, fp);
}

static void
set_param(const char *name, char *value)
{
	struct param *param;
	char *ep, *p;
	size_t buflen, mlen;
	int i, nval, mib[CTL_MAXNAME];
	struct {
		int i;
		char s[MAXPATHLEN];
	} buf;

	static int paramlistsize;

	/* Separate the name from the value, if not done already. */
	if (name == NULL) {
		name = value;
		if ((value = strchr(value, '=')))
			*value++ = '\0';
	}

	/* Check for repeat parameters */
	for (i = 0; i < nparams; i++)
		if (!strcmp(name, params[i].name.iov_base)) {
			memcpy(params + i, params + i + 1,
			    (--nparams - i) * sizeof(struct param));
			break;
		}

	/* Make sure there is room for the new param record. */
	if (!nparams) {
		paramlistsize = 32;
		params = malloc(paramlistsize * sizeof(*params));
		param_values = malloc(paramlistsize * sizeof(*param_values));
		if (params == NULL || param_values == NULL)
			err(1, "malloc");
	} else if (nparams >= paramlistsize) {
		paramlistsize *= 2;
		params = realloc(params, paramlistsize * sizeof(*params));
		param_values = realloc(param_values,
		    paramlistsize * sizeof(*param_values));
		if (params == NULL)
			err(1, "realloc");
	}

	/* Look up the paramter. */
	param_values[nparams] = value;
	param = params + nparams++;
	*(const void **)&param->name.iov_base = name;
	param->name.iov_len = strlen(name) + 1;
	/* Trivial values - no value or errmsg. */
	if (value == NULL) {
		param->value.iov_base = NULL;
		param->value.iov_len = 0;
		return;
	}
	if (!strcmp(name, "errmsg")) {
		param->value.iov_base = value;
		param->value.iov_len = ERRMSG_SIZE;
		return;
	}
	mib[0] = 0;
	mib[1] = 3;
	snprintf(buf.s, sizeof(buf.s), SJPARAM ".%s", name);
	mlen = sizeof(mib) - 2 * sizeof(int);
	if (sysctl(mib, 2, mib + 2, &mlen, buf.s, strlen(buf.s)) < 0)
		errx(1, "unknown parameter: %s", name);
	mib[1] = 4;
	buflen = sizeof(buf);
	if (sysctl(mib, (mlen / sizeof(int)) + 2, &buf, &buflen, NULL, 0) < 0)
		err(1, "sysctl(0.4.%s)", name);
	/*
	 * See if this is an array type.
	 * Treat non-arrays as an array of one.
	 */
	p = strchr(buf.s, '\0');
	nval = 1;
	if (p - 2 >= buf.s && !strcmp(p - 2, ",a")) {
		if (value[0] == '\0' ||
		    (value[0] == '-' && value[1] == '\0')) {
			param->value.iov_base = value;
			param->value.iov_len = 0;
			return;
		}
		p[-2] = 0;
		for (p = strchr(value, ','); p; p = strchr(p + 1, ',')) {
			*p = '\0';
			nval++;
		}
	}
	
	/* Set the values according to the parameter type. */
	switch (buf.i & CTLTYPE) {
	case CTLTYPE_INT:
	case CTLTYPE_UINT:
		param->value.iov_len = nval * sizeof(int);
		break;
	case CTLTYPE_LONG:
	case CTLTYPE_ULONG:
		param->value.iov_len = nval * sizeof(long);
		break;
	case CTLTYPE_STRUCT:
		if (!strcmp(buf.s, "S,in_addr"))
			param->value.iov_len = nval * sizeof(struct in_addr);
#ifdef INET6
		else if (!strcmp(buf.s, "S,in6_addr"))
			param->value.iov_len = nval * sizeof(struct in6_addr);
#endif
		else
			errx(1, "%s: unknown parameter structure (%s)",
			    name, buf.s);
		break;
	case CTLTYPE_STRING:
		if (!strcmp(name, "path")) {
			param->value.iov_base = malloc(MAXPATHLEN);
			if (param->value.iov_base == NULL)
				err(1, "malloc");
			if (realpath(value, param->value.iov_base) == NULL)
				err(1, "%s: realpath(%s)", name, value);
			if (chdir(param->value.iov_base) != 0)
				err(1, "chdir: %s",
				    (char *)param->value.iov_base);
		} else
			param->value.iov_base = value;
		param->value.iov_len = strlen(param->value.iov_base) + 1;
		return;
	default:
		errx(1, "%s: unknown parameter type %d (%s)",
		    name, buf.i, buf.s);
	}
	param->value.iov_base = malloc(param->value.iov_len);
	for (i = 0; i < nval; i++) {
		switch (buf.i & CTLTYPE) {
		case CTLTYPE_INT:
			((int *)param->value.iov_base)[i] =
			    strtol(value, &ep, 10);
			if (ep[0] != '\0')
				errx(1, "%s: non-integer value \"%s\"",
				    name, value);
			break;
		case CTLTYPE_UINT:
			((unsigned *)param->value.iov_base)[i] =
			    strtoul(value, &ep, 10);
			if (ep[0] != '\0')
				errx(1, "%s: non-integer value \"%s\"",
				    name, value);
			break;
		case CTLTYPE_LONG:
			((long *)param->value.iov_base)[i] =
			    strtol(value, &ep, 10);
			if (ep[0] != '\0')
			    errx(1, "%s: non-integer value \"%s\"",
				name, value);
			break;
		case CTLTYPE_ULONG:
			((unsigned long *)param->value.iov_base)[i] =
			    strtoul(value, &ep, 10);
			if (ep[0] != '\0')
			    errx(1, "%s: non-integer value \"%s\"",
				name, value);
			break;
		case CTLTYPE_STRUCT:
			if (!strcmp(buf.s, "S,in_addr")) {
				if (inet_pton(AF_INET, value,
				    &((struct in_addr *)
				    param->value.iov_base)[i]) != 1)
					errx(1, "%s: not an IPv4 address: %s",
					    name, value);
			}
#ifdef INET6
			else if (!strcmp(buf.s, "S,in6_addr")) {
				if (inet_pton(AF_INET6, value,
				    &((struct in6_addr *)
				    param->value.iov_base)[i]) != 1)
					errx(1, "%s: not an IPv6 address: %s",
					    name, value);
			}
#endif
		}
		if (i > 0)
			value[-1] = ',';
		value = strchr(value, '\0') + 1;
	}
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: jail [-d] [-h] [-i] [-J jid_file] "
			"[-l -u username | -U username]\n"
	    "            [-c | -m] param=value ... [command=command ...]\n"
	    "       jail [-r jail]\n");
	exit(1);
}
