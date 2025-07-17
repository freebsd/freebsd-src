/*
 * Copyright (c) 2005 Daniel Walsh <dwalsh@redhat.com>
 * Copyright (c) 2006 Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Linux-specific portability code - just SELinux support at present
 */

#include "includes.h"

#if defined(WITH_SELINUX) || defined(LINUX_OOM_ADJUST) || \
    defined(SYSTEMD_NOTIFY)
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "xmalloc.h"
#include "port-linux.h"
#include "misc.h"

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/label.h>
#include <selinux/get_context_list.h>

#ifndef SSH_SELINUX_UNCONFINED_TYPE
# define SSH_SELINUX_UNCONFINED_TYPE ":unconfined_t:"
#endif

/* Wrapper around is_selinux_enabled() to log its return value once only */
int
ssh_selinux_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		enabled = (is_selinux_enabled() == 1);
		debug("SELinux support %s", enabled ? "enabled" : "disabled");
	}

	return (enabled);
}

/* Return the default security context for the given username */
static char *
ssh_selinux_getctxbyname(char *pwname)
{
	char *sc = NULL, *sename = NULL, *lvl = NULL;
	int r;

#ifdef HAVE_GETSEUSERBYNAME
	if (getseuserbyname(pwname, &sename, &lvl) != 0)
		return NULL;
#else
	sename = pwname;
	lvl = NULL;
#endif

#ifdef HAVE_GET_DEFAULT_CONTEXT_WITH_LEVEL
	r = get_default_context_with_level(sename, lvl, NULL, &sc);
#else
	r = get_default_context(sename, NULL, &sc);
#endif

	if (r != 0) {
		switch (security_getenforce()) {
		case -1:
			fatal("%s: ssh_selinux_getctxbyname: "
			    "security_getenforce() failed", __func__);
		case 0:
			error("%s: Failed to get default SELinux security "
			    "context for %s", __func__, pwname);
			sc = NULL;
			break;
		default:
			fatal("%s: Failed to get default SELinux security "
			    "context for %s (in enforcing mode)",
			    __func__, pwname);
		}
	}

#ifdef HAVE_GETSEUSERBYNAME
	free(sename);
	free(lvl);
#endif

	return sc;
}

/* Set the execution context to the default for the specified user */
void
ssh_selinux_setup_exec_context(char *pwname)
{
	char *user_ctx = NULL;

	if (!ssh_selinux_enabled())
		return;

	debug3("%s: setting execution context", __func__);

	user_ctx = ssh_selinux_getctxbyname(pwname);
	if (setexeccon(user_ctx) != 0) {
		switch (security_getenforce()) {
		case -1:
			fatal("%s: security_getenforce() failed", __func__);
		case 0:
			error("%s: Failed to set SELinux execution "
			    "context for %s", __func__, pwname);
			break;
		default:
			fatal("%s: Failed to set SELinux execution context "
			    "for %s (in enforcing mode)", __func__, pwname);
		}
	}
	if (user_ctx != NULL)
		freecon(user_ctx);

	debug3("%s: done", __func__);
}

/* Set the TTY context for the specified user */
void
ssh_selinux_setup_pty(char *pwname, const char *tty)
{
	char *new_tty_ctx = NULL, *user_ctx = NULL, *old_tty_ctx = NULL;
	security_class_t chrclass;

	if (!ssh_selinux_enabled())
		return;

	debug3("%s: setting TTY context on %s", __func__, tty);

	user_ctx = ssh_selinux_getctxbyname(pwname);

	/* XXX: should these calls fatal() upon failure in enforcing mode? */

	if (getfilecon(tty, &old_tty_ctx) == -1) {
		error("%s: getfilecon: %s", __func__, strerror(errno));
		goto out;
	}
	if ((chrclass = string_to_security_class("chr_file")) == 0) {
		error("%s: couldn't get security class for chr_file", __func__);
		goto out;
	}
	if (security_compute_relabel(user_ctx, old_tty_ctx,
	    chrclass, &new_tty_ctx) != 0) {
		error("%s: security_compute_relabel: %s",
		    __func__, strerror(errno));
		goto out;
	}

	if (setfilecon(tty, new_tty_ctx) != 0)
		error("%s: setfilecon: %s", __func__, strerror(errno));
 out:
	if (new_tty_ctx != NULL)
		freecon(new_tty_ctx);
	if (old_tty_ctx != NULL)
		freecon(old_tty_ctx);
	if (user_ctx != NULL)
		freecon(user_ctx);
	debug3("%s: done", __func__);
}

void
ssh_selinux_change_context(const char *newname)
{
	char *oldctx, *newctx, *cx, *cx2;
	LogLevel log_level = SYSLOG_LEVEL_INFO;

	if (!ssh_selinux_enabled())
		return;

	if (getcon(&oldctx) < 0) {
		logit_f("getcon failed with %s", strerror(errno));
		return;
	}
	if ((cx = strchr(oldctx, ':')) == NULL ||
	    (cx = strchr(cx + 1, ':')) == NULL ||
	    (cx - oldctx) >= INT_MAX) {
		logit_f("unparsable context %s", oldctx);
		return;
	}

	/*
	 * Check whether we are attempting to switch away from an unconfined
	 * security context.
	 */
	if (strncmp(cx, SSH_SELINUX_UNCONFINED_TYPE,
	    sizeof(SSH_SELINUX_UNCONFINED_TYPE) - 1) == 0)
		log_level = SYSLOG_LEVEL_DEBUG3;

	cx2 = strchr(cx + 1, ':');
	xasprintf(&newctx, "%.*s%s%s", (int)(cx - oldctx + 1), oldctx,
	    newname, cx2 == NULL ? "" : cx2);

	debug3_f("setting context from '%s' to '%s'", oldctx, newctx);
	if (setcon(newctx) < 0)
		do_log2_f(log_level, "setcon %s from %s failed with %s",
		    newctx, oldctx, strerror(errno));
	free(oldctx);
	free(newctx);
}

void
ssh_selinux_setfscreatecon(const char *path)
{
	char *context;
	struct selabel_handle *shandle = NULL;

	if (!ssh_selinux_enabled())
		return;
	if (path == NULL) {
		setfscreatecon(NULL);
		return;
	}
	if ((shandle = selabel_open(SELABEL_CTX_FILE, NULL, 0)) == NULL) {
		debug_f("selabel_open failed");
		return;
	}
	if (selabel_lookup(shandle, &context, path, 0700) == 0)
		setfscreatecon(context);
	selabel_close(shandle);
}

#endif /* WITH_SELINUX */

#ifdef LINUX_OOM_ADJUST
/*
 * The magic "don't kill me" values, old and new, as documented in eg:
 * http://lxr.linux.no/#linux+v2.6.32/Documentation/filesystems/proc.txt
 * http://lxr.linux.no/#linux+v2.6.36/Documentation/filesystems/proc.txt
 */

static int oom_adj_save = INT_MIN;
static char *oom_adj_path = NULL;
struct {
	char *path;
	int value;
} oom_adjust[] = {
	{"/proc/self/oom_score_adj", -1000},	/* kernels >= 2.6.36 */
	{"/proc/self/oom_adj", -17},		/* kernels <= 2.6.35 */
	{NULL, 0},
};

/*
 * Tell the kernel's out-of-memory killer to avoid sshd.
 * Returns the previous oom_adj value or zero.
 */
void
oom_adjust_setup(void)
{
	int i, value;
	FILE *fp;

	debug3("%s", __func__);
	 for (i = 0; oom_adjust[i].path != NULL; i++) {
		oom_adj_path = oom_adjust[i].path;
		value = oom_adjust[i].value;
		if ((fp = fopen(oom_adj_path, "r+")) != NULL) {
			if (fscanf(fp, "%d", &oom_adj_save) != 1)
				verbose("error reading %s: %s", oom_adj_path,
				    strerror(errno));
			else {
				rewind(fp);
				if (fprintf(fp, "%d\n", value) <= 0)
					verbose("error writing %s: %s",
					   oom_adj_path, strerror(errno));
				else
					debug("Set %s from %d to %d",
					   oom_adj_path, oom_adj_save, value);
			}
			fclose(fp);
			return;
		}
	}
	oom_adj_path = NULL;
}

/* Restore the saved OOM adjustment */
void
oom_adjust_restore(void)
{
	FILE *fp;

	debug3("%s", __func__);
	if (oom_adj_save == INT_MIN || oom_adj_path == NULL ||
	    (fp = fopen(oom_adj_path, "w")) == NULL)
		return;

	if (fprintf(fp, "%d\n", oom_adj_save) <= 0)
		verbose("error writing %s: %s", oom_adj_path, strerror(errno));
	else
		debug("Set %s to %d", oom_adj_path, oom_adj_save);

	fclose(fp);
	return;
}
#endif /* LINUX_OOM_ADJUST */

#ifdef SYSTEMD_NOTIFY

static void ssh_systemd_notify(const char *, ...)
    __attribute__((__format__ (printf, 1, 2))) __attribute__((__nonnull__ (1)));

static void
ssh_systemd_notify(const char *fmt, ...)
{
	char *s = NULL;
	const char *path;
	struct stat sb;
	struct sockaddr_un addr;
	int fd = -1;
	va_list ap;

	if ((path = getenv("NOTIFY_SOCKET")) == NULL || strlen(path) == 0)
		return;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	/* Only AF_UNIX is supported, with path or abstract sockets */
	if (path[0] != '/' && path[0] != '@') {
		error_f("socket \"%s\" is not compatible with AF_UNIX", path);
		goto out;
	}

	if (path[0] == '/' && stat(path, &sb) != 0) {
		error_f("socket \"%s\" stat: %s", path, strerror(errno));
		goto out;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlcpy(addr.sun_path, path,
	    sizeof(addr.sun_path)) >= sizeof(addr.sun_path)) {
		error_f("socket path \"%s\" too long", path);
		goto out;
	}
	/* Support for abstract socket */
	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = 0;
	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1) {
		error_f("socket \"%s\": %s", path, strerror(errno));
		goto out;
	}
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		error_f("socket \"%s\" connect: %s", path, strerror(errno));
		goto out;
	}
	if (write(fd, s, strlen(s)) != (ssize_t)strlen(s)) {
		error_f("socket \"%s\" write: %s", path, strerror(errno));
		goto out;
	}
	debug_f("socket \"%s\" notified %s", path, s);
 out:
	if (fd != -1)
		close(fd);
	free(s);
}

void
ssh_systemd_notify_ready(void)
{
	ssh_systemd_notify("READY=1");
}

void
ssh_systemd_notify_reload(void)
{
	struct timespec now;

	monotime_ts(&now);
	if (now.tv_sec < 0 || now.tv_nsec < 0) {
		error_f("monotime returned negative value");
		ssh_systemd_notify("RELOADING=1");
	} else {
		ssh_systemd_notify("RELOADING=1\nMONOTONIC_USEC=%llu",
		    ((uint64_t)now.tv_sec * 1000000ULL) +
		    ((uint64_t)now.tv_nsec / 1000ULL));
	}
}
#endif /* SYSTEMD_NOTIFY */

#endif /* WITH_SELINUX || LINUX_OOM_ADJUST || SYSTEMD_NOTIFY */
