/*-
 * Copyright (c) 2004 Apple Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/projects/trustedbsd/openbsm/libbsm/bsm_wrappers.c#26 $
 */

#ifdef __APPLE__
#define	_SYS_AUDIT_H		/* Prevent include of sys/audit.h. */
#endif

#include <sys/param.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <sys/queue.h>		/* Our bsm/audit.h doesn't include queue.h. */
#endif

#include <sys/sysctl.h>

#include <bsm/libbsm.h>

#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* These are not advertised in libbsm.h */
int audit_set_terminal_port(dev_t *p);
int audit_set_terminal_host(uint32_t *m);

/*
 * General purpose audit submission mechanism for userspace.
 */
int
audit_submit(short au_event, au_id_t auid, char status,
    int reterr, const char *fmt, ...)
{
	char text[MAX_AUDITSTRING_LEN];
	token_t *token;
	long acond;
	va_list ap;
	pid_t pid;
	int error, afd, subj_ex;
	struct auditinfo ai;
	struct auditinfo_addr aia;

	if (auditon(A_GETCOND, &acond, sizeof(acond)) < 0) {
		/*
		 * If auditon(2) returns ENOSYS, then audit has not been
		 * compiled into the kernel, so just return.
		 */
		if (errno == ENOSYS)
			return (0);
		error = errno;
		syslog(LOG_AUTH | LOG_ERR, "audit: auditon failed: %s",
		    strerror(errno));
		errno = error;
		return (-1);
	}
	if (acond == AUC_NOAUDIT)
		return (0);
	/* XXXCSJP we should be doing a pre-select here */
	afd = au_open();
	if (afd < 0) {
		error = errno;
		syslog(LOG_AUTH | LOG_ERR, "audit: au_open failed: %s",
		    strerror(errno));
		errno = error;
		return (-1);
	}
	/*
	 * Some operating systems do not have getaudit_addr(2) implemented
	 * yet.  So we try to use getaudit(2) first, if the subject is
	 * using IPv6, then we will have to try getaudit_addr(2).  Failing
	 * this, we return error.
	 */
	subj_ex = 0;
	error = getaudit(&ai);
	if (error < 0 && errno == E2BIG) {
		error = getaudit_addr(&aia, sizeof(aia));
		if (error == 0)
			subj_ex = 1;
	}
	if (error < 0) {
		error = errno;
		syslog(LOG_AUTH | LOG_ERR, "audit: getaudit failed: %s",
		    strerror(errno));
		errno = error;
		return (-1);
	}
	pid = getpid();
	if (subj_ex == 0)
		token = au_to_subject32(auid, geteuid(), getegid(),
		    getuid(), getgid(), pid, pid, &ai.ai_termid);
	else
		token = au_to_subject_ex(auid, geteuid(), getegid(),
		    getuid(), getgid(), pid, pid, &aia.ai_termid);
	if (token == NULL) {
		syslog(LOG_AUTH | LOG_ERR,
		    "audit: unable to build subject token");
		(void) au_close(afd, AU_TO_NO_WRITE, au_event);
		errno = EPERM;
		return (-1);
	}
	if (au_write(afd, token) < 0) {
		error = errno;
		syslog(LOG_AUTH | LOG_ERR,
		    "audit: au_write failed: %s", strerror(errno));
		(void) au_close(afd, AU_TO_NO_WRITE, au_event);
		errno = error;
		return (-1);
	}
	if (fmt != NULL) {
		va_start(ap, fmt);
		(void) vsnprintf(text, MAX_AUDITSTRING_LEN, fmt, ap);
		va_end(ap);
		token = au_to_text(text);
		if (token == NULL) {
			syslog(LOG_AUTH | LOG_ERR,
			    "audit: failed to generate text token");
			(void) au_close(afd, AU_TO_NO_WRITE, au_event);
			errno = EPERM;
			return (-1);
		}
		if (au_write(afd, token) < 0) {
			error = errno;
			syslog(LOG_AUTH | LOG_ERR,
			    "audit: au_write failed: %s", strerror(errno));
			(void) au_close(afd, AU_TO_NO_WRITE, au_event);
			errno = error;
			return (-1);
		}
	}
	token = au_to_return32(status, reterr);
	if (token == NULL) {
		syslog(LOG_AUTH | LOG_ERR,
		    "audit: enable to build return token");
		(void) au_close(afd, AU_TO_NO_WRITE, au_event);
		errno = EPERM;
		return (-1);
	}
	if (au_write(afd, token) < 0) {
		error = errno;
		syslog(LOG_AUTH | LOG_ERR,
		    "audit: au_write failed: %s", strerror(errno));
		(void) au_close(afd, AU_TO_NO_WRITE, au_event);
		errno = error;
		return (-1);
	}
	if (au_close(afd, AU_TO_WRITE, au_event) < 0) {
		error = errno;
		syslog(LOG_AUTH | LOG_ERR, "audit: record not committed");
		errno = error;
		return (-1);
	}
	return (0);
}

int
audit_set_terminal_port(dev_t *p)
{
	struct stat st;

	if (p == NULL)
		return (kAUBadParamErr);

#ifdef NODEV
	*p = NODEV;
#else
	*p = -1;
#endif

	/* for /usr/bin/login, try fstat() first */
	if (fstat(STDIN_FILENO, &st) != 0) {
		if (errno != EBADF) {
			syslog(LOG_ERR, "fstat() failed (%s)",
			    strerror(errno));
			return (kAUStatErr);
		}
		if (stat("/dev/console", &st) != 0) {
			syslog(LOG_ERR, "stat() failed (%s)",
			    strerror(errno));
			return (kAUStatErr);
		}
	}
	*p = st.st_rdev;
	return (kAUNoErr);
}

int
audit_set_terminal_host(uint32_t *m)
{

#ifdef KERN_HOSTID
	int name[2] = { CTL_KERN, KERN_HOSTID };
	size_t len;

	if (m == NULL)
		return (kAUBadParamErr);
	*m = 0;
	len = sizeof(*m);
	if (sysctl(name, 2, m, &len, NULL, 0) != 0) {
		syslog(LOG_ERR, "sysctl() failed (%s)", strerror(errno));
		return (kAUSysctlErr);
	}
	return (kAUNoErr);
#else
	*m = -1;
	return (kAUNoErr);
#endif
}

int
audit_set_terminal_id(au_tid_t *tid)
{
	int ret;

	if (tid == NULL)
		return (kAUBadParamErr);
	if ((ret = audit_set_terminal_port(&tid->port)) != kAUNoErr)
		return (ret);
	return (audit_set_terminal_host(&tid->machine));
}

/*
 * This is OK for those callers who have only one token to write.  If you have
 * multiple tokens that logically form part of the same audit record, you need
 * to use the existing au_open()/au_write()/au_close() API:
 *
 * aufd = au_open();
 * tok = au_to_random_token_1(...);
 * au_write(aufd, tok);
 * tok = au_to_random_token_2(...);
 * au_write(aufd, tok);
 * ...
 * au_close(aufd, AU_TO_WRITE, AUE_your_event_type);
 *
 * Assumes, like all wrapper calls, that the caller has previously checked
 * that auditing is enabled via the audit_get_state() call.
 *
 * XXX: Should be more robust against bad arguments.
 */
int
audit_write(short event_code, token_t *subject, token_t *misctok, char retval,
    int errcode)
{
	int aufd;
	char *func = "audit_write()";
	token_t *rettok;

	if ((aufd = au_open()) == -1) {
		au_free_token(subject);
		au_free_token(misctok);
		syslog(LOG_ERR, "%s: au_open() failed", func);
		return (kAUOpenErr);
	}

	/* Save subject. */
	if (subject && au_write(aufd, subject) == -1) {
		au_free_token(subject);
		au_free_token(misctok);
		(void)au_close(aufd, AU_TO_NO_WRITE, event_code);
		syslog(LOG_ERR, "%s: write of subject failed", func);
		return (kAUWriteSubjectTokErr);
	}

	/* Save the event-specific token. */
	if (misctok && au_write(aufd, misctok) == -1) {
		au_free_token(misctok);
		(void)au_close(aufd, AU_TO_NO_WRITE, event_code);
		syslog(LOG_ERR, "%s: write of caller token failed", func);
		return (kAUWriteCallerTokErr);
	}

	/* Tokenize and save the return value. */
	if ((rettok = au_to_return32(retval, errcode)) == NULL) {
		(void)au_close(aufd, AU_TO_NO_WRITE, event_code);
		syslog(LOG_ERR, "%s: au_to_return32() failed", func);
		return (kAUMakeReturnTokErr);
	}

	if (au_write(aufd, rettok) == -1) {
		au_free_token(rettok);
		(void)au_close(aufd, AU_TO_NO_WRITE, event_code);
		syslog(LOG_ERR, "%s: write of return code failed", func);
		return (kAUWriteReturnTokErr);
	}

	/*
	 * We assume the caller wouldn't have bothered with this
	 * function if it hadn't already decided to keep the record.
	 */
	if (au_close(aufd, AU_TO_WRITE, event_code) < 0) {
		syslog(LOG_ERR, "%s: au_close() failed", func);
		return (kAUCloseErr);
	}

	return (kAUNoErr);
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly
 * assumes success; use audit_write_failure() on error.
 */
int
audit_write_success(short event_code, token_t *tok, au_id_t auid, uid_t euid,
    gid_t egid, uid_t ruid, gid_t rgid, pid_t pid, au_asid_t sid,
    au_tid_t *tid)
{
	char *func = "audit_write_success()";
	token_t *subject = NULL;

	/* Tokenize and save subject. */
	subject = au_to_subject32(auid, euid, egid, ruid, rgid, pid, sid,
	    tid);
	if (subject == NULL) {
		syslog(LOG_ERR, "%s: au_to_subject32() failed", func);
		return kAUMakeSubjectTokErr;
	}

	return (audit_write(event_code, subject, tok, 0, 0));
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly
 * assumes success; use audit_write_failure_self() on error.
 */
int
audit_write_success_self(short event_code, token_t *tok)
{
	token_t *subject;
	char *func = "audit_write_success_self()";

	if ((subject = au_to_me()) == NULL) {
		syslog(LOG_ERR, "%s: au_to_me() failed", func);
		return (kAUMakeSubjectTokErr);
	}

	return (audit_write(event_code, subject, tok, 0, 0));
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly
 * assumes failure; use audit_write_success() otherwise.
 *
 * XXX  This should let the caller pass an error return value rather than
 * hard-coding -1.
 */
int
audit_write_failure(short event_code, char *errmsg, int errcode, au_id_t auid,
    uid_t euid, gid_t egid, uid_t ruid, gid_t rgid, pid_t pid, au_asid_t sid,
    au_tid_t *tid)
{
	char *func = "audit_write_failure()";
	token_t *subject, *errtok;

	subject = au_to_subject32(auid, euid, egid, ruid, rgid, pid, sid, tid);
	if (subject == NULL) {
		syslog(LOG_ERR, "%s: au_to_subject32() failed", func);
		return (kAUMakeSubjectTokErr);
	}

	/* tokenize and save the error message */
	if ((errtok = au_to_text(errmsg)) == NULL) {
		au_free_token(subject);
		syslog(LOG_ERR, "%s: au_to_text() failed", func);
		return (kAUMakeTextTokErr);
	}

	return (audit_write(event_code, subject, errtok, -1, errcode));
}

/*
 * Same caveats as audit_write().  In addition, this function explicitly
 * assumes failure; use audit_write_success_self() otherwise.
 *
 * XXX  This should let the caller pass an error return value rather than
 * hard-coding -1.
 */
int
audit_write_failure_self(short event_code, char *errmsg, int errret)
{
	char *func = "audit_write_failure_self()";
	token_t *subject, *errtok;

	if ((subject = au_to_me()) == NULL) {
		syslog(LOG_ERR, "%s: au_to_me() failed", func);
		return (kAUMakeSubjectTokErr);
	}
	/* tokenize and save the error message */
	if ((errtok = au_to_text(errmsg)) == NULL) {
		au_free_token(subject);
		syslog(LOG_ERR, "%s: au_to_text() failed", func);
		return (kAUMakeTextTokErr);
	}
	return (audit_write(event_code, subject, errtok, -1, errret));
}

/*
 * For auditing errors during login.  Such errors are implicitly
 * non-attributable (i.e., not ascribable to any user).
 *
 * Assumes, like all wrapper calls, that the caller has previously checked
 * that auditing is enabled via the audit_get_state() call.
 */
int
audit_write_failure_na(short event_code, char *errmsg, int errret, uid_t euid,
    uid_t egid, pid_t pid, au_tid_t *tid)
{

	return (audit_write_failure(event_code, errmsg, errret, -1, euid,
	    egid, -1, -1, pid, -1, tid));
}

/* END OF au_write() WRAPPERS */

#ifdef __APPLE__
void
audit_token_to_au32(audit_token_t atoken, uid_t *auidp, uid_t *euidp,
    gid_t *egidp, uid_t *ruidp, gid_t *rgidp, pid_t *pidp, au_asid_t *asidp,
    au_tid_t *tidp)
{

	if (auidp != NULL)
		*auidp = (uid_t)atoken.val[0];
	if (euidp != NULL)
		*euidp = (uid_t)atoken.val[1];
	if (egidp != NULL)
		*egidp = (gid_t)atoken.val[2];
	if (ruidp != NULL)
		*ruidp = (uid_t)atoken.val[3];
	if (rgidp != NULL)
		*rgidp = (gid_t)atoken.val[4];
	if (pidp != NULL)
		*pidp = (pid_t)atoken.val[5];
	if (asidp != NULL)
		*asidp = (au_asid_t)atoken.val[6];
	if (tidp != NULL) {
		audit_set_terminal_host(&tidp->machine);
		tidp->port = (dev_t)atoken.val[7];
	}
}
#endif /* !__APPLE__ */
