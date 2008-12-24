/*-
 * Copyright (c) 2005 Apple Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/bin/auditd/auditd.h#11 $
 */

#ifndef _AUDITD_H_
#define	_AUDITD_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <syslog.h>

#define	MAX_DIR_SIZE	255
#define	AUDITD_NAME	"auditd"

/*
 * If defined, then the audit daemon will attempt to chown newly created logs
 * to this group.  Otherwise, they will be the default for the user running
 * auditd, likely the audit group.
 */
#define	AUDIT_REVIEW_GROUP	"audit"

#define	NOT_TERMINATED		"not_terminated"
#define	POSTFIX_LEN	(sizeof("YYYYMMDDhhmmss") - 1)
#define	FILENAME_LEN	((2 * POSTFIX_LEN) + 2)
#define	TIMESTAMP_LEN	(POSTFIX_LEN + 1)	

struct dir_ent {
	char			*dirname;
	char			 softlim;
	TAILQ_ENTRY(dir_ent)	 dirs;
};

#define	HARDLIM_ALL_WARN	"allhard"
#define	SOFTLIM_ALL_WARN	"allsoft"
#define	AUDITOFF_WARN		"auditoff"
#define	CLOSEFILE_WARN		"closefile"
#define	EBUSY_WARN		"ebusy"
#define	GETACDIR_WARN		"getacdir"
#define	HARDLIM_WARN		"hard"
#define	NOSTART_WARN		"nostart"
#define	POSTSIGTERM_WARN	"postsigterm"
#define	SOFTLIM_WARN		"soft"
#define	TMPFILE_WARN		"tmpfile"

#define	AUDITWARN_SCRIPT	"/etc/security/audit_warn"
#define	AUDITD_PIDFILE		"/var/run/auditd.pid"

int	audit_warn_allhard(int count);
int	audit_warn_allsoft(void);
int	audit_warn_auditoff(void);
int	audit_warn_closefile(char *filename);
int	audit_warn_ebusy(void);
int	audit_warn_getacdir(char *filename);
int	audit_warn_hard(char *filename);
int	audit_warn_nostart(void);
int	audit_warn_postsigterm(void);
int	audit_warn_soft(char *filename);
int	audit_warn_tmpfile(void);

#endif /* !_AUDITD_H_ */
