#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_ctl.c,v 8.39 2000/12/19 23:31:38 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1997-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Extern. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

/* Defs. */

#define CONTROL_FOUND	0x0001	/* for mark and sweep. */
#define	MAX_STR_LEN	500

struct control {
	LINK(struct control) link;
	enum { t_dead, t_inet, t_unix } type;
	struct ctl_sctx *sctx;
	u_int flags;
	union {
		struct {
			struct sockaddr_in in;
			ip_match_list allow;
		} v_inet;
#ifndef NO_SOCKADDR_UN
		struct {
			struct sockaddr_un un;
			mode_t mode;
			uid_t owner;
			gid_t group;
		} v_unix;
#endif
	} var;
};

/* Forward. */

static struct ctl_sctx *mksrvr(control, const struct sockaddr *, size_t);
static control		new_control(void);
static void		free_control(controls *, control);
static void		free_controls(controls *);
static int		match_control(control, control);
static control		find_control(controls, control);
static void		propagate_changes(const control, control);
static void		install(control);
static void		install_inet(control);
static void		install_unix(control);
static void		logger(enum ctl_severity, const char *fmt, ...);
static void		verb_connect(struct ctl_sctx *, struct ctl_sess *,
				     const struct ctl_verb *,
				     const char *, u_int, void *, void *);
static void		verb_getpid(struct ctl_sctx *, struct ctl_sess *,
				    const struct ctl_verb *,
				    const char *, u_int, void *, void *);
static void		getpid_closure(struct ctl_sctx *, struct ctl_sess *,
				       void *);
static void		verb_status(struct ctl_sctx *, struct ctl_sess *,
				    const struct ctl_verb *,
				    const char *, u_int, void *, void *);
static void		status_closure(struct ctl_sctx *, struct ctl_sess *,
				       void *);
static void		verb_stop(struct ctl_sctx *, struct ctl_sess *,
				  const struct ctl_verb *,
				  const char *, u_int, void *, void *);
static void		verb_exec(struct ctl_sctx *, struct ctl_sess *,
				  const struct ctl_verb *,
				  const char *, u_int, void *, void *);
static void		verb_reload(struct ctl_sctx *, struct ctl_sess *,
				    const struct ctl_verb *,
				    const char *, u_int, void *, void *);
static void		verb_reconfig(struct ctl_sctx *, struct ctl_sess *,
				      const struct ctl_verb *,
				      const char *, u_int, void *, void *);
static void		verb_dumpdb(struct ctl_sctx *, struct ctl_sess *,
				    const struct ctl_verb *,
				    const char *, u_int, void *, void *);
static void		verb_stats(struct ctl_sctx *, struct ctl_sess *,
				   const struct ctl_verb *,
				   const char *, u_int, void *, void *);
static void		verb_trace(struct ctl_sctx *, struct ctl_sess *,
				   const struct ctl_verb *,
				   const char *, u_int, void *, void *);
static void		trace_closure(struct ctl_sctx *, struct ctl_sess *,
				       void *);
static void		verb_notrace(struct ctl_sctx *, struct ctl_sess *,
				     const struct ctl_verb *,
				     const char *, u_int, void *, void *);
static void		verb_querylog(struct ctl_sctx *, struct ctl_sess *,
				      const struct ctl_verb *,
				      const char *, u_int, void *, void *);
static void		verb_help(struct ctl_sctx *, struct ctl_sess *,
				  const struct ctl_verb *,
				  const char *, u_int, void *, void *);
static void		verb_quit(struct ctl_sctx *, struct ctl_sess *,
				  const struct ctl_verb *,
				  const char *, u_int, void *, void *);

/* Private data. */

static controls server_controls;

static struct ctl_verb verbs[] = {
	{ "",		verb_connect,	""},
	{ "getpid",	verb_getpid,	"getpid"},
	{ "status",	verb_status,	"status"},
	{ "stop",	verb_stop,	"stop"},
	{ "exec",	verb_exec,	"exec"},
	{ "reload",	verb_reload,	"reload [zone] ..."},
	{ "reconfig",	verb_reconfig,	"reconfig [-noexpired] (just sees new/gone zones)"},
	{ "dumpdb",	verb_dumpdb,	"dumpdb"},
	{ "stats",	verb_stats,	"stats [clear]"},
	{ "trace",	verb_trace,	"trace [level]"},
	{ "notrace",	verb_notrace,	"notrace"},
	{ "querylog",	verb_querylog,	"querylog"},
	{ "qrylog",	verb_querylog,	"qrylog"},
	{ "help",	verb_help,	"help"},
	{ "quit",	verb_quit,	"quit"},
	{ NULL,		NULL,		NULL}
};

/* Public functions. */

void
ns_ctl_initialize(void) {
	INIT_LIST(server_controls);
}

void
ns_ctl_shutdown(void) {
	if (!EMPTY(server_controls))
		free_controls(&server_controls);
}

void
ns_ctl_defaults(controls *list) {
#ifdef NO_SOCKADDR_UN
	struct in_addr saddr;
	ip_match_list iml;
	ip_match_element ime;

	/*
	 * If the operating system does not support local domain sockets, 
	 * connect with ndc on 127.0.0.1, port 101, and only allow
	 * connections from 127.0.0.1.
	 */
	saddr.s_addr = htonl (INADDR_LOOPBACK);
	iml = new_ip_match_list();
	ime = new_ip_match_pattern(saddr, 32);
	add_to_ip_match_list(iml, ime);

	ns_ctl_add(list, ns_ctl_new_inet(saddr, htons (101), iml));
#else
#ifdef NEED_SECURE_DIRECTORY
	ns_ctl_add(list, ns_ctl_new_unix(_PATH_NDCSOCK, 0700, 0, 0));
#else
	ns_ctl_add(list, ns_ctl_new_unix(_PATH_NDCSOCK, 0600, 0, 0));
#endif
#endif /*NO_SOCKADDR_UN*/
}

void
ns_ctl_add(controls *list, control new) {
	if (!find_control(*list, new))
		APPEND(*list, new, link);
}

control
ns_ctl_new_inet(struct in_addr saddr, u_int sport, ip_match_list allow) {
	control new = new_control();

	INIT_LINK(new, link);
	new->type = t_inet;
	memset(&new->var.v_inet.in, 0, sizeof new->var.v_inet.in);
	new->var.v_inet.in.sin_family = AF_INET;
	new->var.v_inet.in.sin_addr = saddr;
	new->var.v_inet.in.sin_port = sport;
	new->var.v_inet.allow = allow;
	return (new);
}

#ifndef NO_SOCKADDR_UN
control
ns_ctl_new_unix(char *path, mode_t mode, uid_t owner, gid_t group) {
	control new = new_control();

	INIT_LINK(new, link);
	new->type = t_unix;
	memset(&new->var.v_unix.un, 0, sizeof new->var.v_unix.un);
	new->var.v_unix.un.sun_family = AF_UNIX;
	strncpy(new->var.v_unix.un.sun_path, path,
		sizeof new->var.v_unix.un.sun_path - 1);
	new->var.v_unix.mode = mode;
	new->var.v_unix.owner = owner;
	new->var.v_unix.group = group;
	return (new);
}
#endif

void
ns_ctl_install(controls *new) {
	control ctl, old, next;

	/* Find all the controls which aren't new or deleted. */
	for (ctl = HEAD(server_controls); ctl != NULL; ctl = NEXT(ctl, link))
		ctl->flags &= ~CONTROL_FOUND;
	for (ctl = HEAD(*new); ctl != NULL; ctl = next) {
		next = NEXT(ctl, link);
		old = find_control(server_controls, ctl);
		if (old != NULL) {
			old->flags |= CONTROL_FOUND;
			propagate_changes(ctl, old);
			if (old->sctx == NULL)
				free_control(&server_controls, old);
			free_control(new, ctl);
		}
	}

	/* Destroy any old controls which weren't found. */
	for (ctl = HEAD(server_controls); ctl != NULL; ctl = next) {
		next = NEXT(ctl, link);
		if ((ctl->flags & CONTROL_FOUND) == 0)
			free_control(&server_controls, ctl);
	}

	/* Add any new controls which were found. */
	for (ctl = HEAD(*new); ctl != NULL; ctl = next) {
		next = NEXT(ctl, link);
		UNLINK(*new, ctl, link);
		APPEND(server_controls, ctl, link);
		install(ctl);
		if (ctl->sctx == NULL)
			free_control(&server_controls, ctl);
	}
}

/* Private functions. */

static struct ctl_sctx *
mksrvr(control ctl, const struct sockaddr *sa, size_t salen) {
	return (ctl_server(ev, sa, salen, verbs, 500, 222,
			   600, 5, 10, logger, ctl));
}

static control
new_control(void) {
	control new = memget(sizeof *new);

	if (new == NULL)
		panic("memget failed in new_control()", NULL);
	new->type = t_dead;
	new->sctx = NULL;
	return (new);
}

static void
free_control(controls *list, control this) {
	int was_live = 0;
	struct stat sb;

	if (this->sctx != NULL) {
		ctl_endserver(this->sctx);
		this->sctx = NULL;
		was_live = 1;
	}
	switch (this->type) {
	case t_inet:
		if (this->var.v_inet.allow != NULL) {
			free_ip_match_list(this->var.v_inet.allow);
			this->var.v_inet.allow = NULL;
		}
		break;
#ifndef NO_SOCKADDR_UN
	case t_unix:
		/* XXX Race condition. */
		if (was_live &&
		    stat(this->var.v_unix.un.sun_path, &sb) == 0 &&
		    (S_ISSOCK(sb.st_mode) || S_ISFIFO(sb.st_mode))) {
			/* XXX Race condition. */
			unlink(this->var.v_unix.un.sun_path);
		}
		break;
#endif
	default:
		panic("impossible type in free_control", NULL);
		/* NOTREACHED */
	}
	UNLINK(*list, this, link);
	memput(this, sizeof *this);
}

static void
free_controls(controls *list) {
	control ctl, next;

	for (ctl = HEAD(*list); ctl != NULL; ctl = next) {
		next = NEXT(ctl, link);
		free_control(list, ctl);
	}
	INIT_LIST(*list);
}

static int
match_control(control l, control r) {
	int match = 1;

	if (l->type != r->type)
		match = 0;
	else
		switch (l->type) {
		case t_inet:
			if (l->var.v_inet.in.sin_family !=
			    r->var.v_inet.in.sin_family ||
			    l->var.v_inet.in.sin_port !=
			    r->var.v_inet.in.sin_port ||
			    l->var.v_inet.in.sin_addr.s_addr !=
			    r->var.v_inet.in.sin_addr.s_addr)
				match = 0;
			break;
#ifndef NO_SOCKADDR_UN
		case t_unix:
			if (l->var.v_unix.un.sun_family !=
			    r->var.v_unix.un.sun_family ||
			    strcmp(l->var.v_unix.un.sun_path,
				   r->var.v_unix.un.sun_path) != 0)
				match = 0;
			break;
#endif
		default:
			panic("impossible type in match_control", NULL);
			/* NOTREACHED */
		}
	ns_debug(ns_log_config, 20, "match_control(): %d", match);
	return (match);
}

static control
find_control(controls list, control new) {
	control ctl;

	for (ctl = HEAD(list); ctl != NULL; ctl = NEXT(ctl, link))
		if (match_control(ctl, new))
			return (ctl);
	return (NULL);
}

static void
propagate_changes(const control diff, control base) {
	int need_install = 0;

	switch (base->type) {
	case t_inet:
		if (base->var.v_inet.allow != NULL)
			free_ip_match_list(base->var.v_inet.allow);
		base->var.v_inet.allow = diff->var.v_inet.allow;
		diff->var.v_inet.allow = NULL;
		need_install++;
		break;
#ifndef NO_SOCKADDR_UN
	case t_unix:
		if (base->var.v_unix.mode != diff->var.v_unix.mode) {
			base->var.v_unix.mode = diff->var.v_unix.mode;
			need_install++;
		}
		if (base->var.v_unix.owner != diff->var.v_unix.owner) {
			base->var.v_unix.owner = diff->var.v_unix.owner;
			need_install++;
		}
		if (base->var.v_unix.group != diff->var.v_unix.group) {
			base->var.v_unix.group = diff->var.v_unix.group;
			need_install++;
		}
		break;
#endif
	default:
		panic("impossible type in ns_ctl::propagate_changes", NULL);
		/* NOTREACHED */
	}
	if (need_install)
		install(base);
}

static void
install(control ctl) {
	switch (ctl->type) {
	case t_inet:
		install_inet(ctl);
		break;
#ifndef NO_SOCKADDR_UN
	case t_unix:
		install_unix(ctl);
		break;
#endif
	default:
		panic("impossible type in ns_ctl::install", NULL);
		/* NOTREACHED */
	}
}

static void
install_inet(control ctl) {
	if (ctl->sctx == NULL) {
		ctl->sctx = mksrvr(ctl,
				   (struct sockaddr *)&ctl->var.v_inet.in,
				   sizeof ctl->var.v_inet.in);
	}
}

#ifndef NO_SOCKADDR_UN
/*
 * Unattach an old unix domain socket if it exists.
 */
static void
unattach(control ctl) {
	int s;
	struct stat sb;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		ns_warning(ns_log_config,
			   "unix control \"%s\" socket failed: %s",
			   ctl->var.v_unix.un.sun_path,
			   strerror(errno));
		return;
	}

	if (stat(ctl->var.v_unix.un.sun_path, &sb) < 0) {
		switch (errno) {
		case ENOENT:	/* We exited cleanly last time */
			break;
		default:
			ns_warning(ns_log_config,
				   "unix control \"%s\" stat failed: %s",
				   ctl->var.v_unix.un.sun_path,
				   strerror(errno));
			break;
		}
		goto cleanup;
	}

	if (!(S_ISSOCK(sb.st_mode) || S_ISFIFO(sb.st_mode))) {
		ns_warning(ns_log_config, "unix control \"%s\" not socket",
			   ctl->var.v_unix.un.sun_path);
		goto cleanup;
	}

	if (connect(s, (struct sockaddr *)&ctl->var.v_unix.un,
		    sizeof ctl->var.v_unix.un) < 0) {
		switch (errno) {
		case ECONNREFUSED:
		case ECONNRESET:
			if (unlink(ctl->var.v_unix.un.sun_path) < 0)
				ns_warning(ns_log_config,
			      "unix control \"%s\" unlink failed: %s",
					   ctl->var.v_unix.un.sun_path,
					   strerror(errno));
			break;
		default:
			ns_warning(ns_log_config,
				   "unix control \"%s\" connect failed: %s",
				   ctl->var.v_unix.un.sun_path,
				   strerror(errno));
			break;
		}
	}
  cleanup:
	close(s);
}

static void
install_unix(control ctl) {
	char *path;
#ifdef NEED_SECURE_DIRECTORY
	char *slash;

	path = savestr(ctl->var.v_unix.un.sun_path, 1);

	slash = strrchr(path, '/');
	if (slash != NULL) {
		if (slash != path)
			*slash = '\0';
		else {
			freestr(path);
			path = savestr("/", 1);
		}
	} else {
		freestr(path);
		path = savestr(".", 1);
	}
	if (mkdir(path, ctl->var.v_unix.mode) < 0) {
		if (errno != EEXIST) {
			ns_warning(ns_log_config,
				   "unix control \"%s\" mkdir failed: %s",
				   path, strerror(errno));
		}
	}
#else
	path = ctl->var.v_unix.un.sun_path;
#endif

	if (ctl->sctx == NULL) {
		unattach(ctl);
		ctl->sctx = mksrvr(ctl,
				   (struct sockaddr *)&ctl->var.v_unix.un,
				   sizeof ctl->var.v_unix.un);
	}
	if (ctl->sctx != NULL) {
		/* XXX Race condition. */
		if (chmod(path, ctl->var.v_unix.mode) < 0) {
			ns_warning(ns_log_config, "chmod(\"%s\", 0%03o): %s",
				   ctl->var.v_unix.un.sun_path,
				   ctl->var.v_unix.mode,
				   strerror(errno));
		}
		if (chown(path, ctl->var.v_unix.owner,
			  ctl->var.v_unix.group) < 0) {
			ns_warning(ns_log_config, "chown(\"%s\", %d, %d): %s",
				   ctl->var.v_unix.un.sun_path,
				   ctl->var.v_unix.owner,
				   ctl->var.v_unix.group,
				   strerror(errno));
		}
	}
#ifdef NEED_SECURE_DIRECTORY
	freestr(path);
#endif
}
#endif

static void
logger(enum ctl_severity ctlsev, const char *format, ...) {
	va_list args;
	int logsev;

	switch (ctlsev) {
	case ctl_debug:		logsev = log_debug(5);	break;
	case ctl_warning:	logsev = log_warning;	break;
	case ctl_error:		logsev = log_error;	break;
	default:		panic("invalid ctlsev in logger", NULL);
	}
	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, ns_log_control, logsev, format, args);
	va_end(args);
}

static void
verb_connect(struct ctl_sctx *ctl, struct ctl_sess *sess,
	     const struct ctl_verb *verb, const char *rest,
	     u_int respflags, void *respctx, void *uctx)
{
	const struct sockaddr *sa = (struct sockaddr *)respctx;
	control nsctl = (control)uctx;

	if (sa->sa_family == AF_INET) {
		const struct sockaddr_in *in = (struct sockaddr_in *)sa;
		const ip_match_list acl = nsctl->var.v_inet.allow;

		if (!ip_address_allowed(acl, in->sin_addr)) {
			ctl_response(sess, 502, "Permission denied.",
				     CTL_EXIT, NULL, NULL, NULL, NULL, 0);
			return;
		}
	}
	ctl_response(sess, 220, server_options->version, 0, NULL, NULL, NULL,
		     NULL, 0);
}

static void
verb_getpid(struct ctl_sctx *ctl, struct ctl_sess *sess,
	    const struct ctl_verb *verb, const char *rest,
	    u_int respflags, void *respctx, void *uctx)
{
	char *msg = memget(MAX_STR_LEN);

	if (msg == NULL) {
		ctl_response(sess, 503, "(out of memory)", 0,
			     NULL, NULL, NULL, NULL, 0);
		return;
	}
	sprintf(msg, "my pid is <%ld>", (long)getpid());
	ctl_response(sess, 250, msg, 0, NULL, getpid_closure, msg, NULL, 0);
}

static void
getpid_closure(struct ctl_sctx *sctx, struct ctl_sess *sess, void *uap) {
	char *msg = uap;

	memput(msg, MAX_STR_LEN);
}

enum state {
	e_version = 0,
	e_config,
	e_nzones,
	e_debug,
	e_xfersrun,
	e_xfersdfr,
	e_qserials,
	e_qrylog,
	e_priming,
	e_finito
};

struct pvt_status {
	enum state	state;
	char		text[MAX_STR_LEN];
};

static void
verb_status(struct ctl_sctx *ctl, struct ctl_sess *sess,
	    const struct ctl_verb *verb, const char *rest,
	    u_int respflags, void *respctx, void *uctx)
{
	struct pvt_status *pvt = ctl_getcsctx(sess);

	if (pvt == NULL) {
		pvt = memget(sizeof *pvt);
		if (pvt == NULL) {
			ctl_response(sess, 505, "(out of memory)",
				     0, NULL, NULL, NULL, NULL, 0);
			return;
		}
		pvt->state = (enum state)0;
		(void)ctl_setcsctx(sess, pvt);
	}
	switch (pvt->state++) {
	case e_version:
		strncpy(pvt->text, Version, sizeof pvt->text);
		pvt->text[sizeof pvt->text - 1] = '\0';
		break;
	case e_config:
		sprintf(pvt->text, "config (%s) last loaded at age: %24s",
			conffile, ctime(&confmtime));
		break;
	case e_nzones:
		sprintf(pvt->text, "number of zones allocated: %d", nzones);
		break;
	case e_debug:
		sprintf(pvt->text, "debug level: %d", debug);
		break;
	case e_xfersrun:
		sprintf(pvt->text, "xfers running: %d", xfers_running);
		break;
	case e_xfersdfr:
		sprintf(pvt->text, "xfers deferred: %d", xfers_deferred);
		break;
	case e_qserials:
		sprintf(pvt->text, "soa queries in progress: %d",
			qserials_running);
		break;
	case e_qrylog:
		sprintf(pvt->text, "query logging is %s",
			qrylog ? "ON" : "OFF");
		break;
	case e_priming:
		if (priming)
			sprintf(pvt->text, "server is initialising itself");
		else
			sprintf(pvt->text, "server is up and running");
		break;
	case e_finito:
		return;
	}
	ctl_response(sess, 250, pvt->text,
		     (pvt->state == e_finito) ? 0 : CTL_MORE,
		     NULL, status_closure, NULL, NULL, 0);
}

static void
status_closure(struct ctl_sctx *sctx, struct ctl_sess *sess, void *uap) {
	struct pvt_status *pvt = ctl_getcsctx(sess);

	memput(pvt, sizeof *pvt);
	ctl_setcsctx(sess, NULL);
}

static void
verb_stop(struct ctl_sctx *ctl, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	ns_need(main_need_exit);
	ctl_response(sess, 250, "Shutdown initiated.", 0, NULL, NULL, NULL,
		     NULL, 0);
}

static void
verb_exec(struct ctl_sctx *ctl, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	struct stat sb;

	if (rest != NULL && *rest != '\0') {
		if (stat(rest, &sb) < 0) {
			ctl_response(sess, 503, strerror(errno),
				     0, NULL, NULL, NULL, NULL, 0);
			return;
		}
		saved_argv[0] = savestr(rest, 1);	/* Never strfreed. */
	}

	if (stat(saved_argv[0], &sb) < 0) {
		const char *save = strerror(errno);

		ns_warning(ns_log_default, "can't exec, %s: %s",
			   saved_argv[0], save);
		ctl_response(sess, 502, save, 0, NULL, NULL, NULL,
			     NULL, 0);
	} else {
		ns_need(main_need_restart);
		ctl_response(sess, 250, "Restart initiated.", 0, NULL,
			     NULL, NULL, NULL, 0);
	}
}

static void
verb_reload(struct ctl_sctx *ctl, struct ctl_sess *sess,
	    const struct ctl_verb *verb, const char *rest,
	    u_int respflags, void *respctx, void *uctx)
{
	static const char spaces[] = " \t";
	struct zoneinfo *zp;
	char *tmp = NULL, *x;
	const char *msg;
	int class, code, success;

	/* If there are no args, this is a classic reload of the config. */
	if (rest == NULL || *rest == '\0') {
		ns_need(main_need_reload);
		code = 250;
		msg = "Reload initiated.";
		goto respond;
	}

	/* Look for optional zclass argument.  Default is "in". */
	tmp = savestr(rest, 1);
	x = tmp + strcspn(tmp, spaces);
	if (*x != '\0') {
		*x++ = '\0';
		x += strspn(x, spaces);
	}
	if (x == NULL || *x == '\0')
		x = "in";
	class = sym_ston(__p_class_syms, x, &success);
	if (!success) {
		code = 507;
		msg = "unrecognized class";
		goto respond;
	}

	/* Look for the zone, and do the right thing to it. */
	zp = find_zone(tmp, class);
	if (zp == NULL) {
		code = 506;
		msg = "Zone not found.";
		goto respond;
	}
	switch (zp->z_type) {
	case z_master:
		ns_stopxfrs(zp);
		/*FALLTHROUGH*/
	case z_hint:
		block_signals();
		code = 251;
		msg = deferred_reload_unsafe(zp);
		unblock_signals();
		break;
	case z_slave:
	case z_stub:
		ns_stopxfrs(zp);
		if (zonefile_changed_p(zp))
			zp->z_serial = 0;	/* force xfer */
		addxfer(zp);
		code = 251;
		msg = "Slave transfer queued.";
		goto respond;
	case z_forward:
	case z_cache:
	default:
		msg = "Non reloadable zone.";
		code = 507;
		break;
	}

 respond:
	ctl_response(sess, code, msg, 0, NULL, NULL, NULL, NULL, 0);
	if (tmp != NULL)
		freestr(tmp);
}

static void
verb_reconfig(struct ctl_sctx *ctl, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	if (strcmp(rest, "-noexpired") != 0)
		ns_need(main_need_reconfig);
	else
		ns_need(main_need_noexpired);
	ctl_response(sess, 250, "Reconfig initiated.",
		     0, NULL, NULL, NULL, NULL, 0);
}

static void
verb_dumpdb(struct ctl_sctx *ctl, struct ctl_sess *sess,
	    const struct ctl_verb *verb, const char *rest,
	    u_int respflags, void *respctx, void *uctx)
{
	ns_need(main_need_dump);
	ctl_response(sess, 250, "Database dump initiated.", 0, NULL,
		     NULL, NULL, NULL, 0);
}

static void
verb_stats(struct ctl_sctx *ctl, struct ctl_sess *sess,
	   const struct ctl_verb *verb, const char *rest,
	   u_int respflags, void *respctx, void *uctx)
{
	if (rest != NULL && strcmp(rest, "clear") == 0) {
		ns_need(main_need_statsdumpandclear);
		ctl_response(sess, 250, "Statistics dump and clear initiated.",
			     0, NULL, NULL, NULL, NULL, 0);
	} else {
		ns_need(main_need_statsdump);
		ctl_response(sess, 250, "Statistics dump initiated.",
			     0, NULL, NULL, NULL, NULL, 0);
	}
}

static void
verb_trace(struct ctl_sctx *ctl, struct ctl_sess *sess,
	   const struct ctl_verb *verb, const char *rest,
	   u_int respflags, void *respctx, void *uctx)
{
	int i = atoi(rest);
	char *msg = memget(MAX_STR_LEN);

	if (msg == NULL) {
		ctl_response(sess, 503, "(out of memory)", 0,
			     NULL, NULL, NULL, NULL, 0);
		return;
	}
	if (i > 0) 
		desired_debug = i;
	else
		desired_debug++;
	ns_need(main_need_debug);
	sprintf(msg, "Debug level: %d", desired_debug);
	ctl_response(sess, 250, msg, 0, NULL, trace_closure, msg, NULL, 0);
}

static void
trace_closure(struct ctl_sctx *sctx, struct ctl_sess *sess, void *uap) {
	char *msg = uap;

	memput(msg, MAX_STR_LEN);
}

static void
verb_notrace(struct ctl_sctx *ctl, struct ctl_sess *sess,
	     const struct ctl_verb *verb, const char *rest,
	     u_int respflags, void *respctx, void *uctx)
{
	desired_debug = 0;
	ns_need(main_need_debug);
	ctl_response(sess, 250, "Debugging turned off.",
		     0, NULL, NULL, NULL, NULL, 0);
}

static void
verb_querylog(struct ctl_sctx *ctl, struct ctl_sess *sess,
	      const struct ctl_verb *verb, const char *rest,
	      u_int respflags, void *respctx, void *uctx)
{
	static const char	on[] = "Query logging is now on.",
				off[] = "Query logging is now off.";

	toggle_qrylog();
	ctl_response(sess, 250, qrylog ? on : off,
		     0, NULL, NULL, NULL, NULL, 0);
}

static void
verb_help(struct ctl_sctx *ctl, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	ctl_sendhelp(sess, 214);
}

static void
verb_quit(struct ctl_sctx *ctl, struct ctl_sess *sess,
	  const struct ctl_verb *verb, const char *rest,
	  u_int respflags, void *respctx, void *uctx)
{
	ctl_response(sess, 221, "End of control session.", CTL_EXIT, NULL,
		     NULL, NULL, NULL, 0);
}
