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
 * Linux-specific portability code
 */

#include "includes.h"

#if defined(LINUX_OOM_ADJUST) || defined(SYSTEMD_NOTIFY)
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

#ifdef LINUX_MEMLOCK_ONFAULT
#include <sys/mman.h>

void
memlock_onfault_setup(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE | MCL_ONFAULT) < 0)
		verbose("unable to lock memory: %s", strerror(errno));
	else
		debug("memory locked");
}
#endif /* LINUX_MEMLOCK_ONFAULT */

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
