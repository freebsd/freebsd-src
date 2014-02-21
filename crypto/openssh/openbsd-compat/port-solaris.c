/* $Id: port-solaris.c,v 1.4 2010/11/05 01:03:05 dtucker Exp $ */

/*
 * Copyright (c) 2006 Chad Mynhier.
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

#include "config.h"
#include "includes.h"

#ifdef USE_SOLARIS_PROCESS_CONTRACTS

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <libcontract.h>
#include <sys/contract/process.h>
#include <sys/ctfs.h>

#include "log.h"

#define CT_TEMPLATE	CTFS_ROOT "/process/template"
#define CT_LATEST	CTFS_ROOT "/process/latest"

static int tmpl_fd = -1;

/* Lookup the latest process contract */
static ctid_t
get_active_process_contract_id(void)
{
	int stat_fd;
	ctid_t ctid = -1;
	ct_stathdl_t stathdl;

	if ((stat_fd = open64(CT_LATEST, O_RDONLY)) == -1) {
		error("%s: Error opening 'latest' process "
		    "contract: %s", __func__, strerror(errno));
		return -1;
	}
	if (ct_status_read(stat_fd, CTD_COMMON, &stathdl) != 0) {
		error("%s: Error reading process contract "
		    "status: %s", __func__, strerror(errno));
		goto out;
	}
	if ((ctid = ct_status_get_id(stathdl)) < 0) {
		error("%s: Error getting process contract id: %s",
		    __func__, strerror(errno));
		goto out;
	}

	ct_status_free(stathdl);
 out:
	close(stat_fd);
	return ctid;
}

void
solaris_contract_pre_fork(void)
{
	if ((tmpl_fd = open64(CT_TEMPLATE, O_RDWR)) == -1) {
		error("%s: open %s: %s", __func__,
		    CT_TEMPLATE, strerror(errno));
		return;
	}

	debug2("%s: setting up process contract template on fd %d",
	    __func__, tmpl_fd);

	/* First we set the template parameters and event sets. */
	if (ct_pr_tmpl_set_param(tmpl_fd, CT_PR_PGRPONLY) != 0) {
		error("%s: Error setting process contract parameter set "
		    "(pgrponly): %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_pr_tmpl_set_fatal(tmpl_fd, CT_PR_EV_HWERR) != 0) {
		error("%s: Error setting process contract template "
		    "fatal events: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_tmpl_set_critical(tmpl_fd, 0) != 0) {
		error("%s: Error setting process contract template "
		    "critical events: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_tmpl_set_informative(tmpl_fd, CT_PR_EV_HWERR) != 0) {
		error("%s: Error setting process contract template "
		    "informative events: %s", __func__, strerror(errno));
		goto fail;
	}

	/* Now make this the active template for this process. */
	if (ct_tmpl_activate(tmpl_fd) != 0) {
		error("%s: Error activating process contract "
		    "template: %s", __func__, strerror(errno));
		goto fail;
	}
	return;

 fail:
	if (tmpl_fd != -1) {
		close(tmpl_fd);
		tmpl_fd = -1;
	}
}

void
solaris_contract_post_fork_child()
{
	debug2("%s: clearing process contract template on fd %d",
	    __func__, tmpl_fd);

	/* Clear the active template. */
	if (ct_tmpl_clear(tmpl_fd) != 0)
		error("%s: Error clearing active process contract "
		    "template: %s", __func__, strerror(errno));

	close(tmpl_fd);
	tmpl_fd = -1;
}

void
solaris_contract_post_fork_parent(pid_t pid)
{
	ctid_t ctid;
	char ctl_path[256];
	int r, ctl_fd = -1, stat_fd = -1;

	debug2("%s: clearing template (fd %d)", __func__, tmpl_fd);

	if (tmpl_fd == -1)
		return;

	/* First clear the active template. */
	if ((r = ct_tmpl_clear(tmpl_fd)) != 0)
		error("%s: Error clearing active process contract "
		    "template: %s", __func__, strerror(errno));

	close(tmpl_fd);
	tmpl_fd = -1;

	/*
	 * If either the fork didn't succeed (pid < 0), or clearing
	 * th active contract failed (r != 0), then we have nothing
	 * more do.
	 */
	if (r != 0 || pid <= 0)
		return;

	/* Now lookup and abandon the contract we've created. */
	ctid = get_active_process_contract_id();

	debug2("%s: abandoning contract id %ld", __func__, ctid);

	snprintf(ctl_path, sizeof(ctl_path),
	    CTFS_ROOT "/process/%ld/ctl", ctid);
	if ((ctl_fd = open64(ctl_path, O_WRONLY)) < 0) {
		error("%s: Error opening process contract "
		    "ctl file: %s", __func__, strerror(errno));
		goto fail;
	}
	if (ct_ctl_abandon(ctl_fd) < 0) {
		error("%s: Error abandoning process contract: %s",
		    __func__, strerror(errno));
		goto fail;
	}
	close(ctl_fd);
	return;

 fail:
	if (tmpl_fd != -1) {
		close(tmpl_fd);
		tmpl_fd = -1;
	}
	if (stat_fd != -1)
		close(stat_fd);
	if (ctl_fd != -1)
		close(ctl_fd);
}
#endif

#ifdef USE_SOLARIS_PROJECTS
#include <sys/task.h>
#include <project.h>

/*
 * Get/set solaris default project.
 * If we fail, just run along gracefully.
 */
void
solaris_set_default_project(struct passwd *pw)
{
	struct project  *defaultproject;
	struct project   tempproject;
	char buf[1024];

	/* get default project, if we fail just return gracefully  */
	if ((defaultproject = getdefaultproj(pw->pw_name, &tempproject, &buf,
	    sizeof(buf))) > 0) {
		/* set default project */
		if (setproject(defaultproject->pj_name, pw->pw_name,
		    TASK_NORMAL) != 0)
			debug("setproject(%s): %s", defaultproject->pj_name,
			    strerror(errno));
	} else {
		/* debug on getdefaultproj() error */
		debug("getdefaultproj(%s): %s", pw->pw_name, strerror(errno));
	}
}
#endif /* USE_SOLARIS_PROJECTS */
