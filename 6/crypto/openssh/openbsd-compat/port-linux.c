/* $Id: port-linux.c,v 1.3 2006/09/01 05:38:41 djm Exp $ */

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

#include <errno.h>
#include <stdarg.h>
#include <string.h>

#ifdef WITH_SELINUX
#include "log.h"
#include "port-linux.h"

#include <selinux/selinux.h>
#include <selinux/flask.h>
#include <selinux/get_context_list.h>

/* Wrapper around is_selinux_enabled() to log its return value once only */
static int
ssh_selinux_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		enabled = is_selinux_enabled();
		debug("SELinux support %s", enabled ? "enabled" : "disabled");
	}

	return (enabled);
}

/* Return the default security context for the given username */
static security_context_t
ssh_selinux_getctxbyname(char *pwname)
{
	security_context_t sc;
	char *sename = NULL, *lvl = NULL;
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
		default:
			fatal("%s: Failed to get default SELinux security "
			    "context for %s (in enforcing mode)",
			    __func__, pwname);
		}
	}

#ifdef HAVE_GETSEUSERBYNAME
	if (sename != NULL)
		xfree(sename);
	if (lvl != NULL)
		xfree(lvl);
#endif

	return (sc);
}

/* Set the execution context to the default for the specified user */
void
ssh_selinux_setup_exec_context(char *pwname)
{
	security_context_t user_ctx = NULL;

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
	security_context_t new_tty_ctx = NULL;
	security_context_t user_ctx = NULL;
	security_context_t old_tty_ctx = NULL;

	if (!ssh_selinux_enabled())
		return;

	debug3("%s: setting TTY context on %s", __func__, tty);

	user_ctx = ssh_selinux_getctxbyname(pwname);

	/* XXX: should these calls fatal() upon failure in enforcing mode? */

	if (getfilecon(tty, &old_tty_ctx) == -1) {
		error("%s: getfilecon: %s", __func__, strerror(errno));
		goto out;
	}

	if (security_compute_relabel(user_ctx, old_tty_ctx,
	    SECCLASS_CHR_FILE, &new_tty_ctx) != 0) {
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
#endif /* WITH_SELINUX */
