/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2009-2010 James Gritton
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jailp.h"

#define JP_RDTUN(jp)	(((jp)->jp_ctltype & CTLFLAG_RDTUN) == CTLFLAG_RDTUN)

struct permspec {
	const char	*name;
	enum intparam	ipnum;
	int		rev;
};

const char *cfname;
int verbose;

static int create_jail(struct cfjail *j);
static void clear_persist(struct cfjail *j);
static int update_jail(struct cfjail *j);
static int rdtun_params(struct cfjail *j, int dofail);
static void running_jid(struct cfjail *j, int dflag);
static int jailparam_set_note(const struct cfjail *j, struct jailparam *jp,
    unsigned njp, int flags);
static void print_jail(FILE *fp, struct cfjail *j, int oldcl);
static void print_param(FILE *fp, const struct cfparam *p, int sep, int doname);
static void quoted_print(FILE *fp, char *str);
static void usage(void);

static struct permspec perm_sysctl[] = {
	{ "security.jail.set_hostname_allowed", KP_ALLOW_SET_HOSTNAME, 0 },
	{ "security.jail.sysvipc_allowed", KP_ALLOW_SYSVIPC, 0 },
	{ "security.jail.allow_raw_sockets", KP_ALLOW_RAW_SOCKETS, 0 },
	{ "security.jail.chflags_allowed", KP_ALLOW_CHFLAGS, 0 },
	{ "security.jail.mount_allowed", KP_ALLOW_MOUNT, 0 },
	{ "security.jail.socket_unixiproute_only", KP_ALLOW_SOCKET_AF, 1 },
};

int
main(int argc, char **argv)
{
#ifdef INET6
	struct in6_addr addr6;
#endif
	struct stat st;
	FILE *jfp;
	struct cfjail *j;
	char *cs, *ncs, *JidFile;
	size_t sysvallen;
	unsigned op, pi;
	int ch, docf, error, i, oldcl, sysval;
	int dflag, iflag, Rflag;
	char enforce_statfs[4];

	op = 0;
	dflag = iflag = Rflag = 0;
	docf = 1;
	cfname = CONF_FILE;
	JidFile = NULL;

	while ((ch = getopt(argc, argv, "cdf:hiJ:lmn:p:qrRs:U:v")) != -1) {
		switch (ch) {
		case 'c':
			op |= JF_START;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			cfname = optarg;
			break;
		case 'h':
			add_param(NULL, NULL, IP_IP_HOSTNAME, NULL);
			docf = 0;
			break;
		case 'i':
			iflag = 1;
			verbose = -1;
			break;
		case 'J':
			JidFile = optarg;
			break;
		case 'l':
			add_param(NULL, NULL, IP_EXEC_CLEAN, NULL);
			docf = 0;
			break;
		case 'm':
			op |= JF_SET;
			break;
		case 'n':
			add_param(NULL, NULL, KP_NAME, optarg);
			docf = 0;
			break;
		case 'p':
			paralimit = strtol(optarg, NULL, 10);
			if (paralimit == 0)
				paralimit = -1;
			break;
		case 'q':
			verbose = -1;
			break;
		case 'r':
			op |= JF_STOP;
			break;
		case 'R':
			op |= JF_STOP;
			Rflag = 1;
			break;
		case 's':
			add_param(NULL, NULL, KP_SECURELEVEL, optarg);
			docf = 0;
			break;
		case 'u':
			add_param(NULL, NULL, IP_EXEC_JAIL_USER, optarg);
			add_param(NULL, NULL, IP_EXEC_SYSTEM_JAIL_USER, NULL);
			docf = 0;
			break;
		case 'U':
			add_param(NULL, NULL, IP_EXEC_JAIL_USER, optarg);
			add_param(NULL, NULL, IP_EXEC_SYSTEM_JAIL_USER,
			    "false");
			docf = 0;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Find out which of the four command line styles this is. */
	oldcl = 0;
	if (!op) {
		/* Old-style command line with four fixed parameters */
		if (argc < 4 || argv[0][0] != '/')
			usage();
		op = JF_START;
		docf = 0;
		oldcl = 1;
		add_param(NULL, NULL, KP_PATH, argv[0]);
		add_param(NULL, NULL, KP_HOST_HOSTNAME, argv[1]);
		if (argv[2][0] != '\0') {
			for (cs = argv[2];; cs = ncs + 1) {
				ncs = strchr(cs, ',');
				if (ncs)
					*ncs = '\0';
				add_param(NULL, NULL,
#ifdef INET6
				    inet_pton(AF_INET6, cs, &addr6) == 1
				    ? KP_IP6_ADDR :
#endif
				    KP_IP4_ADDR, cs);
				if (!ncs)
					break;
			}
		}
		for (i = 3; i < argc; i++)
			add_param(NULL, NULL, IP_COMMAND, argv[i]);
		/* Emulate the defaults from security.jail.* sysctls. */
		sysvallen = sizeof(sysval);
		if (sysctlbyname("security.jail.jailed", &sysval, &sysvallen,
		    NULL, 0) == 0 && sysval == 0) {
			for (pi = 0; pi < sizeof(perm_sysctl) /
			     sizeof(perm_sysctl[0]); pi++) {
				sysvallen = sizeof(sysval);
				if (sysctlbyname(perm_sysctl[pi].name,
				    &sysval, &sysvallen, NULL, 0) == 0)
					add_param(NULL, NULL,
					    perm_sysctl[pi].ipnum,
					    (sysval ? 1 : 0) ^ 
					    perm_sysctl[pi].rev
					    ? NULL : "false");
			}
			sysvallen = sizeof(sysval);
			if (sysctlbyname("security.jail.enforce_statfs",
			    &sysval, &sysvallen, NULL, 0) == 0) {
				snprintf(enforce_statfs,
				    sizeof(enforce_statfs), "%d", sysval);
				add_param(NULL, NULL, KP_ENFORCE_STATFS,
				    enforce_statfs);
			}
		}
	} else if (op == JF_STOP) {
		/* Jail remove, perhaps using the config file */
		if (!docf || argc == 0)
			usage();
		if (!Rflag)
			for (i = 0; i < argc; i++)
				if (strchr(argv[i], '='))
					usage();
		if ((docf = !Rflag &&
		     (!strcmp(cfname, "-") || stat(cfname, &st) == 0)))
			load_config();
	} else if (argc > 1 || (argc == 1 && strchr(argv[0], '='))) {
		/* Single jail specified on the command line */
		if (Rflag)
			usage();
		docf = 0;
		for (i = 0; i < argc; i++) {
			if (!strncmp(argv[i], "command", 7) &&
			    (argv[i][7] == '\0' || argv[i][7] == '=')) {
				if (argv[i][7]  == '=')
					add_param(NULL, NULL, IP_COMMAND,
					    argv[i] + 8);
				for (i++; i < argc; i++)
					add_param(NULL, NULL, IP_COMMAND,
					    argv[i]);
				break;
			}
			add_param(NULL, NULL, 0, argv[i]);
		}
	} else {
		/* From the config file, perhaps with a specified jail */
		if (Rflag || !docf)
			usage();
		load_config();
	}

	/* Find out which jails will be run. */
	dep_setup(docf);
	error = 0;
	if (op == JF_STOP) {
		for (i = 0; i < argc; i++)
			if (start_state(argv[i], op, Rflag) < 0)
				error = 1;
	} else {
		if (start_state(docf ? argv[0] : NULL, op, 0) < 0)
			exit(1);
	}

	jfp = NULL;
	if (JidFile != NULL) {
		jfp = fopen(JidFile, "w");
		if (jfp == NULL)
			err(1, "open %s", JidFile);
		setlinebuf(jfp);
	}
	setlinebuf(stdout);

	/*
	 * The main loop: Get an available jail and perform the required
	 * operation on it.  When that is done, the jail may be finished,
	 * or it may go back for the next step.
	 */
	while ((j = next_jail()))
	{
		if (j->flags & JF_FAILED) {
			clear_persist(j);
			if (j->flags & JF_MOUNTED) {
				(void)run_command(j, IP_MOUNT_DEVFS);
				if (run_command(j, IP__MOUNT_FROM_FSTAB))
					while (run_command(j, 0)) ;
				if (run_command(j, IP_MOUNT))
					while (run_command(j, 0)) ;
			}
			if (j->flags & JF_IFUP) {
				if (run_command(j, IP__IP4_IFADDR))
					while (run_command(j, 0)) ;
#ifdef INET6
				if (run_command(j, IP__IP6_IFADDR))
					while (run_command(j, 0)) ;
#endif
			}
			error = 1;
			dep_done(j, 0);
			continue;
		}
		if (!(j->flags & JF_PARAMS))
		{
			j->flags |= JF_PARAMS;
			if (dflag)
				add_param(j, NULL, IP_ALLOW_DYING, NULL);
			if (check_intparams(j) < 0)
				continue;
			if ((j->flags & (JF_START | JF_SET)) &&
			    import_params(j) < 0)
				continue;
		}
		if (!j->jid)
			running_jid(j,
			    (j->flags & (JF_SET | JF_DEPEND)) == JF_SET
			    ? dflag || bool_param(j->intparams[IP_ALLOW_DYING])
			    : 0);
		if (finish_command(j) || run_command(j, 0))
			continue;

		switch (j->flags & JF_OP_MASK) {
			/*
			 * These operations just turn into a different op
			 * depending on the jail's current status.
			 */
		case JF_START_SET:
			j->flags = j->jid < 0 ? JF_START : JF_SET;
			break;
		case JF_SET_RESTART:
			if (j->jid < 0) {
				warnx("\"%s\" not found", j->name);
				failed(j);
				continue;
			}
			j->flags = rdtun_params(j, 0) ? JF_RESTART : JF_SET;
			if (j->flags == JF_RESTART)
				dep_reset(j);
			break;
		case JF_START_SET_RESTART:
			j->flags = j->jid < 0 ? JF_START
			    : rdtun_params(j, 0) ? JF_RESTART : JF_SET;
			if (j->flags == JF_RESTART)
				dep_reset(j);
		}

		switch (j->flags & JF_OP_MASK) {
		case JF_START:
			/*
			 * 1: check existence and dependencies
			 * 2: configure IP addresses
			 * 3: run any exec.prestart commands
			 * 4: create the jail
			 * 5: configure vnet interfaces
			 * 6: run any exec.start or "command" commands
			 * 7: run any exec.poststart commands
			 */
			switch (j->comparam) {
			default:
				if (j->jid > 0 &&
				    !(j->flags & (JF_DEPEND | JF_WILD))) {
					warnx("\"%s\" already exists", j->name);
					failed(j);
					continue;
				}
				if (dep_check(j))
					continue;
				if (j->jid > 0)
					goto jail_create_done;
				if (run_command(j, IP__IP4_IFADDR))
					continue;
				/* FALLTHROUGH */
			case IP__IP4_IFADDR:
#ifdef INET6
				if (run_command(j, IP__IP6_IFADDR))
					continue;
				/* FALLTHROUGH */
			case IP__IP6_IFADDR:
#endif
				if (run_command(j, IP_MOUNT))
					continue;
				/* FALLTHROUGH */
			case IP_MOUNT:
				if (run_command(j,
				    IP__MOUNT_FROM_FSTAB))
					continue;
				/* FALLTHROUGH */
			case IP__MOUNT_FROM_FSTAB:
				if (run_command(j, IP_MOUNT_DEVFS))
					continue;
				/* FALLTHROUGH */
			case IP_MOUNT_DEVFS:
				if (run_command(j, IP_EXEC_PRESTART))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_PRESTART:
				if (create_jail(j) < 0)
					continue;
				if (iflag)
					printf("%d\n", j->jid);
				if (jfp != NULL)
					print_jail(jfp, j, oldcl);
				if (verbose >= 0 && (j->name || verbose > 0))
					jail_note(j, "created\n");
				dep_done(j, DF_LIGHT);
				if (bool_param(j->intparams[KP_VNET]) &&
				    run_command(j, IP_VNET_INTERFACE))
					continue;
				/* FALLTHROUGH */
			case IP_VNET_INTERFACE:
				if (run_command(j, IP_EXEC_START))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_START:
				if (run_command(j, IP_COMMAND))
					continue;
				/* FALLTHROUGH */
			case IP_COMMAND:
				if (run_command(j, IP_EXEC_POSTSTART))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_POSTSTART:
			jail_create_done:
				clear_persist(j);
				dep_done(j, 0);
			}
			break;

		case JF_SET:
			/*
			 * 1: check existence and dependencies
			 * 2: update the jail
			 */
			if (j->jid < 0 && !(j->flags & JF_DEPEND)) {
				warnx("\"%s\" not found", j->name);
				failed(j);
				continue;;
			}
			if (dep_check(j))
				continue;
			if (!(j->flags & JF_DEPEND)) {
				if (rdtun_params(j, 1) < 0 ||
				    update_jail(j) < 0)
					continue;
				if (verbose >= 0 && (j->name || verbose > 0))
					jail_note(j, "updated\n");
			}
			dep_done(j, 0);
			break;

		case JF_STOP:
		case JF_RESTART:
			/*
			 * 1: check dependencies and existence (note order)
			 * 2: run any exec.prestop commands
			 * 3: run any exec.stop commands
			 * 4: send SIGTERM to all jail processes
			 * 5: remove the jail
			 * 6: run any exec.poststop commands
			 * 7: take down IP addresses
			 */
			switch (j->comparam) {
			default:
				if (dep_check(j))
					continue;
				if (j->jid < 0) {
					if (!(j->flags & (JF_DEPEND | JF_WILD))
					    && verbose >= 0)
						warnx("\"%s\" not found",
						    j->name);
					goto jail_remove_done;
				}
				if (run_command(j, IP_EXEC_PRESTOP))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_PRESTOP:
				if (run_command(j, IP_EXEC_STOP))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_STOP:
				if (run_command(j, IP_STOP_TIMEOUT))
					continue;
				/* FALLTHROUGH */
			case IP_STOP_TIMEOUT:
				(void)jail_remove(j->jid);
				j->jid = -1;
				if (verbose >= 0 &&
				    (docf || argc > 1 ||
				     wild_jail_name(argv[0]) || verbose > 0))
					jail_note(j, "removed\n");
				dep_done(j, DF_LIGHT);
				if (run_command(j, IP_EXEC_POSTSTOP))
					continue;
				/* FALLTHROUGH */
			case IP_EXEC_POSTSTOP:
				if (run_command(j, IP_MOUNT_DEVFS))
					continue;
				/* FALLTHROUGH */
			case IP_MOUNT_DEVFS:
				if (run_command(j, IP__MOUNT_FROM_FSTAB))
					continue;
				/* FALLTHROUGH */
			case IP__MOUNT_FROM_FSTAB:
				if (run_command(j, IP_MOUNT))
					continue;
				/* FALLTHROUGH */
			case IP_MOUNT:
				if (run_command(j, IP__IP4_IFADDR))
					continue;
				/* FALLTHROUGH */
			case IP__IP4_IFADDR:
#ifdef INET6
				if (run_command(j, IP__IP6_IFADDR))
					continue;
				/* FALLTHROUGH */
			case IP__IP6_IFADDR:
#endif
			jail_remove_done:
				dep_done(j, 0);
				if (j->flags & JF_START) {
					j->comparam = 0;
					j->flags &= ~JF_STOP;
					dep_reset(j);
					requeue(j,
					    j->ndeps ? &depend : &ready);
				}
			}
			break;
		}
	}

	if (jfp != NULL)
		fclose(jfp);
	exit(error);
}

/*
 * Mark a jail's failure for future handling.
 */
void
failed(struct cfjail *j)
{
	j->flags |= JF_FAILED;
	TAILQ_REMOVE(j->queue, j, tq);
	TAILQ_INSERT_HEAD(&ready, j, tq);
	j->queue = &ready;
}

/*
 * Exit slightly more gracefully when out of memory.
 */
void *
emalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (!p)
		err(1, "malloc");
	return p;
}

void *
erealloc(void *ptr, size_t size)
{
	void *p;

	p = realloc(ptr, size);
	if (!p)
		err(1, "malloc");
	return p;
}

char *
estrdup(const char *str)
{
	char *ns;

	ns = strdup(str);
	if (!ns)
		err(1, "malloc");
	return ns;
}

/*
 * Print a message including an optional jail name.
 */
void
jail_note(const struct cfjail *j, const char *fmt, ...)
{
	va_list ap, tap;
	char *cs;
	size_t len;

	va_start(ap, fmt);
	va_copy(tap, ap);
	len = vsnprintf(NULL, 0, fmt, tap);
	va_end(tap);
	cs = alloca(len + 1);
	(void)vsnprintf(cs, len + 1, fmt, ap);
	va_end(ap);
	if (j->name)
		printf("%s: %s", j->name, cs);
	else
		printf("%s", cs);
}

/*
 * Print a warning message including an optional jail name.
 */
void
jail_warnx(const struct cfjail *j, const char *fmt, ...)
{
	va_list ap, tap;
	char *cs;
	size_t len;

	va_start(ap, fmt);
	va_copy(tap, ap);
	len = vsnprintf(NULL, 0, fmt, tap);
	va_end(tap);
	cs = alloca(len + 1);
	(void)vsnprintf(cs, len + 1, fmt, ap);
	va_end(ap);
	if (j->name)
		warnx("%s: %s", j->name, cs);
	else
		warnx("%s", cs);
}

/*
 * Create a new jail.
 */
static int
create_jail(struct cfjail *j)
{
	struct iovec jiov[4];
	struct stat st;
	struct jailparam *jp, *setparams, *setparams2, *sjp;
	const char *path;
	int dopersist, ns, jid, dying, didfail;

	/*
	 * Check the jail's path, with a better error message than jail_set
	 * gives.
	 */
	if ((path = string_param(j->intparams[KP_PATH]))) {
		if (path[0] != '/') {
			jail_warnx(j, "path %s: not an absolute pathname",
			    path);
			failed(j);
			return -1;
		}
		if (stat(path, &st) < 0) {
			jail_warnx(j, "path %s: %s", path, strerror(errno));
			failed(j);
			return -1;
		}
		if (!S_ISDIR(st.st_mode)) {
			jail_warnx(j, "path %s: %s", path, strerror(ENOTDIR));
			failed(j);
			return -1;
		}
	}

	/*
	 * Copy all the parameters, except that "persist" is always set when
	 * there are commands to run later.
	 */
	dopersist = !bool_param(j->intparams[KP_PERSIST]) &&
	    (j->intparams[IP_EXEC_START] || j->intparams[IP_COMMAND] ||
	     j->intparams[IP_EXEC_POSTSTART]);
	sjp = setparams =
	    alloca((j->njp + dopersist) * sizeof(struct jailparam));
	if (dopersist && jailparam_init(sjp++, "persist") < 0) {
		jail_warnx(j, "%s", jail_errmsg);
		failed(j);
		return -1;
	}
	for (jp = j->jp; jp < j->jp + j->njp; jp++)
		if (!dopersist || !equalopts(jp->jp_name, "persist"))
			*sjp++ = *jp;
	ns = sjp - setparams;

	didfail = 0;
	j->jid = jailparam_set_note(j, setparams, ns, JAIL_CREATE);
	if (j->jid < 0 && errno == EEXIST &&
	    bool_param(j->intparams[IP_ALLOW_DYING]) &&
	    int_param(j->intparams[KP_JID], &jid) && jid != 0) {
		/*
		 * The jail already exists, but may be dying.
		 * Make sure it is, in which case an update is appropriate.
		 */
		*(const void **)&jiov[0].iov_base = "jid";
		jiov[0].iov_len = sizeof("jid");
		jiov[1].iov_base = &jid;
		jiov[1].iov_len = sizeof(jid);
		*(const void **)&jiov[2].iov_base = "dying";
		jiov[2].iov_len = sizeof("dying");
		jiov[3].iov_base = &dying;
		jiov[3].iov_len = sizeof(dying);
		if (jail_get(jiov, 4, JAIL_DYING) < 0) {
			/*
			 * It could be that the jail just barely finished
			 * dying, or it could be that the jid never existed
			 * but the name does.  In either case, another try
			 * at creating the jail should do the right thing.
			 */
			if (errno == ENOENT)
				j->jid = jailparam_set_note(j, setparams, ns,
				    JAIL_CREATE);
		} else if (dying) {
			j->jid = jid;
			if (rdtun_params(j, 1) < 0) {
				j->jid = -1;
				didfail = 1;
			} else {
				sjp = setparams2 = alloca((j->njp + dopersist) *
				    sizeof(struct jailparam));
				for (jp = setparams; jp < setparams + ns; jp++)
					if (!JP_RDTUN(jp) ||
					    !strcmp(jp->jp_name, "jid"))
						*sjp++ = *jp;
				j->jid = jailparam_set_note(j, setparams2,
				    sjp - setparams2, JAIL_UPDATE | JAIL_DYING);
				/*
				 * Again, perhaps the jail just finished dying.
				 */
				if (j->jid < 0 && errno == ENOENT)
					j->jid = jailparam_set_note(j,
					    setparams, ns, JAIL_CREATE);
			}
		}
	}
	if (j->jid < 0 && !didfail) {
		jail_warnx(j, "%s", jail_errmsg);
		failed(j);
	}
	if (dopersist) {
		jailparam_free(setparams, 1);
		if (j->jid > 0)
			j->flags |= JF_PERSIST;
	}
	return j->jid;
}

/*
 * Remove a temporarily set "persist" parameter.
 */
static void
clear_persist(struct cfjail *j)
{
	struct iovec jiov[4];
	int jid;

	if (!(j->flags & JF_PERSIST))
		return;
	j->flags &= ~JF_PERSIST;
	*(const void **)&jiov[0].iov_base = "jid";
	jiov[0].iov_len = sizeof("jid");
	jiov[1].iov_base = &j->jid;
	jiov[1].iov_len = sizeof(j->jid);
	*(const void **)&jiov[2].iov_base = "nopersist";
	jiov[2].iov_len = sizeof("nopersist");
	jiov[3].iov_base = NULL;
	jiov[3].iov_len = 0;
	jid = jail_set(jiov, 4, JAIL_UPDATE);
	if (verbose > 0)
		jail_note(j, "jail_set(JAIL_UPDATE) jid=%d nopersist%s%s\n",
		    j->jid, jid < 0 ? ": " : "",
		    jid < 0 ? strerror(errno) : "");
}

/*
 * Set a jail's parameters.
 */
static int
update_jail(struct cfjail *j)
{
	struct jailparam *jp, *setparams, *sjp;
	int ns, jid;

	ns = 0;
	for (jp = j->jp; jp < j->jp + j->njp; jp++)
		if (!JP_RDTUN(jp))
			ns++;
	if (ns == 0)
		return 0;
	sjp = setparams = alloca(++ns * sizeof(struct jailparam));
	if (jailparam_init(sjp, "jid") < 0 ||
	    jailparam_import_raw(sjp, &j->jid, sizeof j->jid) < 0) {
		jail_warnx(j, "%s", jail_errmsg);
		failed(j);
		return -1;
	}
	for (jp = j->jp; jp < j->jp + j->njp; jp++)
		if (!JP_RDTUN(jp))
			*++sjp = *jp;

	jid = jailparam_set_note(j, setparams, ns,
	    bool_param(j->intparams[IP_ALLOW_DYING])
	    ? JAIL_UPDATE | JAIL_DYING : JAIL_UPDATE);
	if (jid < 0) {
		jail_warnx(j, "%s", jail_errmsg);
		failed(j);
	}
	jailparam_free(setparams, 1);
	return jid;
}

/*
 * Return if a jail set would change any create-only parameters.
 */
static int
rdtun_params(struct cfjail *j, int dofail)
{
	struct jailparam *jp, *rtparams, *rtjp;
	int nrt, rval;

	if (j->flags & JF_RDTUN)
		return 0;
	j->flags |= JF_RDTUN;
	nrt = 0;
	for (jp = j->jp; jp < j->jp + j->njp; jp++)
		if (JP_RDTUN(jp) && strcmp(jp->jp_name, "jid"))
			nrt++;
	if (nrt == 0)
		return 0;
	rtjp = rtparams = alloca(++nrt * sizeof(struct jailparam));
	if (jailparam_init(rtjp, "jid") < 0 ||
	    jailparam_import_raw(rtjp, &j->jid, sizeof j->jid) < 0) {
		jail_warnx(j, "%s", jail_errmsg);
		exit(1);
	}
	for (jp = j->jp; jp < j->jp + j->njp; jp++)
		if (JP_RDTUN(jp) && strcmp(jp->jp_name, "jid"))
			*++rtjp = *jp;
	rval = 0;
	if (jailparam_get(rtparams, nrt,
	    bool_param(j->intparams[IP_ALLOW_DYING]) ? JAIL_DYING : 0) > 0) {
		rtjp = rtparams + 1;
		for (jp = j->jp, rtjp = rtparams + 1; rtjp < rtparams + nrt;
		     jp++) {
			if (JP_RDTUN(jp) && strcmp(jp->jp_name, "jid")) {
				if (!((jp->jp_flags & (JP_BOOL | JP_NOBOOL)) &&
				    jp->jp_valuelen == 0 &&
				    *(int *)jp->jp_value) &&
				    !(rtjp->jp_valuelen == jp->jp_valuelen &&
				    !memcmp(rtjp->jp_value, jp->jp_value,
				    jp->jp_valuelen))) {
					if (dofail) {
						jail_warnx(j, "%s cannot be "
						    "changed after creation",
						    jp->jp_name);
						failed(j);
						rval = -1;
					} else
						rval = 1;
					break;
				}
				rtjp++;
			}
		}
	}
	for (rtjp = rtparams + 1; rtjp < rtparams + nrt; rtjp++)
		rtjp->jp_name = NULL;
	jailparam_free(rtparams, nrt);
	return rval;
}

/*
 * Get the jail's jid if it is running.
 */
static void
running_jid(struct cfjail *j, int dflag)
{
	struct iovec jiov[2];
	const char *pval;
	char *ep;
	int jid;

	if ((pval = string_param(j->intparams[KP_JID]))) {
		if (!(jid = strtol(pval, &ep, 10)) || *ep) {
			j->jid = -1;
			return;
		}
		*(const void **)&jiov[0].iov_base = "jid";
		jiov[0].iov_len = sizeof("jid");
		jiov[1].iov_base = &jid;
		jiov[1].iov_len = sizeof(jid);
	} else if ((pval = string_param(j->intparams[KP_NAME]))) {
		*(const void **)&jiov[0].iov_base = "name";
		jiov[0].iov_len = sizeof("name");
		jiov[1].iov_len = strlen(pval) + 1;
		jiov[1].iov_base = alloca(jiov[1].iov_len);
		strcpy(jiov[1].iov_base, pval);
	} else {
		j->jid = -1;
		return;
	}
	j->jid = jail_get(jiov, 2, dflag ? JAIL_DYING : 0);
}

/*
 * Set jail parameters and possible print them out.
 */
static int
jailparam_set_note(const struct cfjail *j, struct jailparam *jp, unsigned njp,
    int flags)
{
	char *value;
	int jid;
	unsigned i;

	jid = jailparam_set(jp, njp, flags);
	if (verbose > 0) {
		jail_note(j, "jail_set(%s%s)",
		    (flags & (JAIL_CREATE | JAIL_UPDATE)) == JAIL_CREATE
		    ? "JAIL_CREATE" : "JAIL_UPDATE",
		    (flags & JAIL_DYING) ? " | JAIL_DYING" : "");
		for (i = 0; i < njp; i++) {
			printf(" %s", jp[i].jp_name);
			if (jp[i].jp_value == NULL)
				continue;
			putchar('=');
			value = jailparam_export(jp + i);
			if (value == NULL)
				err(1, "jailparam_export");
			quoted_print(stdout, value);
			free(value);
		}
		if (jid < 0)
			printf(": %s", strerror(errno));
		printf("\n");
	}
	return jid;
}

/*
 * Print a jail record.
 */
static void
print_jail(FILE *fp, struct cfjail *j, int oldcl)
{
	struct cfparam *p;

	if (oldcl) {
		fprintf(fp, "%d\t", j->jid);
		print_param(fp, j->intparams[KP_PATH], ',', 0);
		putc('\t', fp);
		print_param(fp, j->intparams[KP_HOST_HOSTNAME], ',', 0);
		putc('\t', fp);
		print_param(fp, j->intparams[KP_IP4_ADDR], ',', 0);
#ifdef INET6
		if (j->intparams[KP_IP6_ADDR] &&
		    !STAILQ_EMPTY(&j->intparams[KP_IP6_ADDR]->val)) {
			if (j->intparams[KP_IP4_ADDR] &&
			    !STAILQ_EMPTY(&j->intparams[KP_IP4_ADDR]->val))
				putc(',', fp);
			print_param(fp, j->intparams[KP_IP6_ADDR], ',', 0);
		}
#endif
		putc('\t', fp);
		print_param(fp, j->intparams[IP_COMMAND], ' ', 0);
	} else {
		fprintf(fp, "jid=%d", j->jid);
		TAILQ_FOREACH(p, &j->params, tq)
			if (strcmp(p->name, "jid")) {
				putc(' ', fp);
				print_param(fp, p, ',', 1);
			}
	}
	putc('\n', fp);
}

/*
 * Print a parameter value, or a name=value pair.
 */
static void
print_param(FILE *fp, const struct cfparam *p, int sep, int doname)
{
	const struct cfstring *s, *ts;

	if (doname)
		fputs(p->name, fp);
	if (p == NULL || STAILQ_EMPTY(&p->val))
		return;
	if (doname)
		putc('=', fp);
	STAILQ_FOREACH_SAFE(s, &p->val, tq, ts) {
		quoted_print(fp, s->s);
		if (ts != NULL)
			putc(sep, fp);
	}
}

/*
 * Print a string with quotes around spaces.
 */
static void
quoted_print(FILE *fp, char *str)
{
	int c, qc;
	char *p = str;

	qc = !*p ? '"'
	    : strchr(p, '\'') ? '"'
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
usage(void)
{

	(void)fprintf(stderr,
	    "usage: jail [-dhilqv] [-J jid_file] [-u username] [-U username]\n"
	    "            -[cmr] param=value ... [command=command ...]\n"
	    "       jail [-dqv] [-f file] -[cmr] [jail]\n"
	    "       jail [-qv] [-f file] -[rR] ['*' | jail ...]\n"
	    "       jail [-dhilqv] [-J jid_file] [-u username] [-U username]\n"
	    "            [-n jailname] [-s securelevel]\n"
	    "            path hostname [ip[,...]] command ...\n");
	exit(1);
}
