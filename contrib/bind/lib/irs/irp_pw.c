/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
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

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: irp_pw.c,v 8.1 1999/01/18 07:46:54 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

/* Extern */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <syslog.h>
#include <sys/param.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <utmp.h>
#include <unistd.h>

#include <irs.h>
#include <irp.h>
#include <isc/memcluster.h>
#include <isc/irpmarshall.h>

#include "port_after.h"

#include "irs_p.h"
#include "irp_p.h"


/* Types */

struct	pvt {
	struct irp_p   *girpdata; /* global IRP data */
	int		warned;
	struct passwd	passwd;		/* password structure */
};

/* Forward */

static void			pw_close(struct irs_pw *);
static struct passwd *		pw_next(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static void			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);

static void			free_passwd(struct passwd *pw);

/* Public */
struct irs_pw *
irs_irp_pw(struct irs_acc *this) {
	struct irs_pw *pw;
	struct pvt *pvt;

	if (!(pw = memget(sizeof *pw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pw, 0, sizeof *pw);

	if (!(pvt = memget(sizeof *pvt))) {
		memput(pw, sizeof *pw);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->girpdata = this->private;

	pw->private = pvt;
	pw->close = pw_close;
	pw->next = pw_next;
	pw->byname = pw_byname;
	pw->byuid = pw_byuid;
	pw->rewind = pw_rewind;
	pw->minimize = pw_minimize;

	return (pw);
}

/* Methods */



/*
 * void pw_close(struct irs_pw *this)
 *
 */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pw_minimize(this);

	free_passwd(&pvt->passwd);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}




/*
 * struct passwd * pw_next(struct irs_pw *this)
 *
 */

static struct passwd *
pw_next(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct passwd *pw = &pvt->passwd;
	char *body;
	size_t bodylen;
	int code;
	char text[256];

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getpwent") != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETUSER_OK) {
		free_passwd(pw);
		if (irp_unmarshall_pw(pw, body) != 0) {
			pw = NULL;
		}
	} else {
		pw = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pw);
}




/*
 * struct passwd * pw_byname(struct irs_pw *this, const char *name)
 *
 */

static struct passwd *
pw_byname(struct irs_pw *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct passwd *pw = &pvt->passwd;
	char *body = NULL;
	char text[256];
	size_t bodylen;
	int code;

	if (pw->pw_name != NULL && strcmp(name, pw->pw_name) == 0) {
		return (pw);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getpwnam %s", name) != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETUSER_OK) {
		free_passwd(pw);
		if (irp_unmarshall_pw(pw, body) != 0) {
			pw = NULL;
		}
	} else {
		pw = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pw);
}




/*
 * struct passwd * pw_byuid(struct irs_pw *this, uid_t uid)
 *
 */

static struct passwd *
pw_byuid(struct irs_pw *this, uid_t uid) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *body;
	char text[256];
	size_t bodylen;
	int code;
	struct passwd *pw = &pvt->passwd;

	if (pw->pw_name != NULL && pw->pw_uid == uid) {
		return (pw);
	}

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return (NULL);
	}

	if (irs_irp_send_command(pvt->girpdata, "getpwuid %d", uid) != 0) {
		return (NULL);
	}

	if (irs_irp_get_full_response(pvt->girpdata, &code,
				      text, sizeof text,
				      &body, &bodylen) != 0) {
		return (NULL);
	}

	if (code == IRPD_GETUSER_OK) {
		free_passwd(pw);
		if (irp_unmarshall_pw(pw, body) != 0) {
			pw = NULL;
		}
	} else {
		pw = NULL;
	}

	if (body != NULL) {
		memput(body, bodylen);
	}

	return (pw);
}




/*
 * void pw_rewind(struct irs_pw *this)
 *
 */

static void
pw_rewind(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char text[256];
	int code;

	if (irs_irp_connection_setup(pvt->girpdata, &pvt->warned) != 0) {
		return;
	}

	if (irs_irp_send_command(pvt->girpdata, "setpwent") != 0) {
		return;
	}

	code = irs_irp_read_response(pvt->girpdata, text, sizeof text);
	if (code != IRPD_GETUSER_SETOK) {
		if (irp_log_errors) {
			syslog(LOG_WARNING, "setpwent failed: %s", text);
		}
	}

	return;
}


/*
 * void pw_minimize(struct irs_pw *this)
 *
 */

static void
pw_minimize(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	irs_irp_disconnect(pvt->girpdata);
}


/* Private. */



/*
 * static void free_passwd(struct passwd *pw);
 *
 *	Deallocate all the memory irp_unmarshall_pw allocated.
 *
 */

static void
free_passwd(struct passwd *pw) {
	if (pw == NULL)
		return;

	if (pw->pw_name != NULL)
		free(pw->pw_name);

	if (pw->pw_passwd != NULL)
		free(pw->pw_passwd);

	if (pw->pw_class != NULL)
		free(pw->pw_class);

	if (pw->pw_gecos != NULL)
		free(pw->pw_gecos);

	if (pw->pw_dir != NULL)
		free(pw->pw_dir);

	if (pw->pw_shell != NULL)
		free(pw->pw_shell);
}

#endif /* WANT_IRS_PW */
