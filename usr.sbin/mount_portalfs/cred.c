/*-
 * Copyright (C) 2005 Diomidis Spinellis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>

#include "portald.h"

/*
 * Set the process's credentials to those specified in user,
 * saveing the existing ones in save.
 * Return 0 on success, -1 (with errno set) on error.
 */
int
set_user_credentials(struct portal_cred *user, struct portal_cred *save)
{
	save->pcr_uid = geteuid();
	if ((save->pcr_ngroups = getgroups(NGROUPS_MAX, save->pcr_groups)) < 0)
		return (-1);
	if (setgroups(user->pcr_ngroups, user->pcr_groups) < 0)
		return (-1);
	if (seteuid(user->pcr_uid) < 0)
		return (-1);
	return (0);
}

/*
 * Restore the process's credentials to the ones specified in save.
 * Log failures using LOG_ERR.
 * Return 0 on success, -1 (with errno set) on error.
 */
int
restore_credentials(struct portal_cred *save)
{
	if (seteuid(save->pcr_uid) < 0) {
		syslog(LOG_ERR, "seteuid: %m");
		return (-1);
	}
	if (setgroups(save->pcr_ngroups, save->pcr_groups) < 0) {
		syslog(LOG_ERR, "setgroups: %m");
		return (-1);
	}
	return (0);
}
