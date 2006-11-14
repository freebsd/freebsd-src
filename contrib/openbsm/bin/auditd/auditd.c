/*
 * Copyright (c) 2004 Apple Computer, Inc.
 * All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
 * @APPLE_BSD_LICENSE_HEADER_END@
 *
 * $P4: //depot/projects/trustedbsd/openbsm/bin/auditd/auditd.c#23 $
 */

#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>

#include "auditd.h"

#define	NA_EVENT_STR_SIZE	25
#define	POL_STR_SIZE		128

static int	 ret, minval;
static char	*lastfile = NULL;
static int	 allhardcount = 0;
static int	 triggerfd = 0;
static int	 sigchlds, sigchlds_handled;
static int	 sighups, sighups_handled;
static int	 sigterms, sigterms_handled;

static TAILQ_HEAD(, dir_ent)	dir_q;

static int	config_audit_controls(void);

/*
 * Error starting auditd
 */
static void
fail_exit(void)
{

	audit_warn_nostart();
	exit(1);
}

/*
 * Free our local list of directory names.
 */
static void
free_dir_q(void)
{
	struct dir_ent *dirent;

	while ((dirent = TAILQ_FIRST(&dir_q))) {
		TAILQ_REMOVE(&dir_q, dirent, dirs);
		free(dirent->dirname);
		free(dirent);
	}
}

/*
 * Generate the timestamp string.
 */
static int
getTSstr(char *buf, int len)
{
	struct timeval ts;
	struct timezone tzp;
	time_t tt;

	if (gettimeofday(&ts, &tzp) != 0)
		return (-1);
	tt = (time_t)ts.tv_sec;
	if (!strftime(buf, len, "%Y%m%d%H%M%S", gmtime(&tt)))
		return (-1);
	return (0);
}

/*
 * Concat the directory name to the given file name.
 * XXX We should affix the hostname also
 */
static char *
affixdir(char *name, struct dir_ent *dirent)
{
	char *fn;
	char *curdir;
	const char *sep = "/";

	curdir = dirent->dirname;
	syslog(LOG_DEBUG, "dir = %s", dirent->dirname);

	fn = malloc(strlen(curdir) + strlen(sep) + (2 * POSTFIX_LEN) + 1);
	if (fn == NULL)
		return (NULL);
	strcpy(fn, curdir);
	strcat(fn, sep);
	strcat(fn, name);
	return (fn);
}

/*
 * Close the previous audit trail file.
 */
static int
close_lastfile(char *TS)
{
	char *ptr;
	char *oldname;

	if (lastfile != NULL) {
		oldname = (char *)malloc(strlen(lastfile) + 1);
		if (oldname == NULL)
			return (-1);
		strcpy(oldname, lastfile);

		/* Rename the last file -- append timestamp. */
		if ((ptr = strstr(lastfile, NOT_TERMINATED)) != NULL) {
			*ptr = '.';
			strcpy(ptr+1, TS);
			if (rename(oldname, lastfile) != 0)
				syslog(LOG_ERR,
				    "Could not rename %s to %s: %m", oldname,
				    lastfile);
			else {
				syslog(LOG_INFO, "renamed %s to %s",
				    oldname, lastfile);
				audit_warn_closefile(lastfile);
			}
		}
		free(lastfile);
		free(oldname);
		lastfile = NULL;
	}
	return (0);
}

/*
 * Create the new audit file with appropriate permissions and ownership.  Try
 * to clean up if something goes wrong.
 */
static int
#ifdef AUDIT_REVIEW_GROUP
open_trail(const char *fname, uid_t uid, gid_t gid)
#else
open_trail(const char *fname)
#endif
{
	int error, fd;

	fd = open(fname, O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP);
	if (fd < 0)
		return (-1);
#ifdef AUDIT_REVIEW_GROUP
	if (fchown(fd, uid, gid) < 0) {
		error = errno;
		close(fd);
		(void)unlink(fname);
		errno = error;
		return (-1);
	}
#endif
	return (fd);
}

/*
 * Create the new file name, swap with existing audit file.
 */
static int
swap_audit_file(void)
{
	char timestr[2 * POSTFIX_LEN];
	char *fn;
	char TS[POSTFIX_LEN];
	struct dir_ent *dirent;
#ifdef AUDIT_REVIEW_GROUP
	struct group *grp;
	gid_t gid;
	uid_t uid;
#endif
	int error, fd;

	if (getTSstr(TS, POSTFIX_LEN) != 0)
		return (-1);

	strcpy(timestr, TS);
	strcat(timestr, NOT_TERMINATED);

#ifdef AUDIT_REVIEW_GROUP
	/*
	 * XXXRW: Currently, this code falls back to the daemon gid, which is
	 * likely the wheel group.  Is there a better way to deal with this?
	 */
	grp = getgrnam(AUDIT_REVIEW_GROUP);
	if (grp == NULL) {
		syslog(LOG_INFO,
		    "Audit review group '%s' not available, using daemon gid",
		    AUDIT_REVIEW_GROUP);
		gid = -1;
	} else
		gid = grp->gr_gid;
	uid = getuid();
#endif

	/* Try until we succeed. */
	while ((dirent = TAILQ_FIRST(&dir_q))) {
		if ((fn = affixdir(timestr, dirent)) == NULL) {
			syslog(LOG_INFO, "Failed to swap log at time %s",
				timestr);
			return (-1);
		}

		/*
		 * Create and open the file; then close and pass to the
		 * kernel if all went well.
		 */
		syslog(LOG_INFO, "New audit file is %s", fn);
#ifdef AUDIT_REVIEW_GROUP
		fd = open_trail(fn, uid, gid);
#else
		fd = open_trail(fn);
#endif
		if (fd < 0)
			warn("open(%s)", fn);
		if (fd >= 0) {
			error = auditctl(fn);
			if (error) {
				syslog(LOG_ERR,
				    "auditctl failed setting log file! : %s",
				    strerror(errno));
				close(fd);
			} else {
				/* Success. */
				close_lastfile(TS);
				lastfile = fn;
				close(fd);
				return (0);
			}
		}

		/*
		 * Tell the administrator about lack of permissions for dir.
		 */
		audit_warn_getacdir(dirent->dirname);

		/* Try again with a different directory. */
		TAILQ_REMOVE(&dir_q, dirent, dirs);
		free(dirent->dirname);
		free(dirent);
	}
	syslog(LOG_ERR, "Log directories exhausted");
	return (-1);
}

/*
 * Read the audit_control file contents.
 */
static int
read_control_file(void)
{
	char cur_dir[MAXNAMLEN];
	struct dir_ent *dirent;
	au_qctrl_t qctrl;

	/*
	 * Clear old values.  Force a re-read of the file the next time.
	 */
	free_dir_q();
	endac();

	/*
	 * Read the list of directories into a local linked list.
	 *
	 * XXX We should use the reentrant interfaces once they are
	 * available.
	 */
	while (getacdir(cur_dir, MAXNAMLEN) >= 0) {
		dirent = (struct dir_ent *) malloc(sizeof(struct dir_ent));
		if (dirent == NULL)
			return (-1);
		dirent->softlim = 0;
		dirent->dirname = (char *) malloc(MAXNAMLEN);
		if (dirent->dirname == NULL) {
			free(dirent);
			return (-1);
		}
		strcpy(dirent->dirname, cur_dir);
		TAILQ_INSERT_TAIL(&dir_q, dirent, dirs);
	}

	allhardcount = 0;
	if (swap_audit_file() == -1) {
		syslog(LOG_ERR, "Could not swap audit file");
		/*
		 * XXX Faulty directory listing? - user should be given
		 * XXX an opportunity to change the audit_control file
		 * XXX switch to a reduced mode of auditing?
		 */
		return (-1);
	}

	/*
	 * XXX There are synchronization problems here
 	 * XXX what should we do if a trigger for the earlier limit
	 * XXX is generated here?
	 */
	if (0 == (ret = getacmin(&minval))) {
		syslog(LOG_DEBUG, "min free = %d", minval);
		if (auditon(A_GETQCTRL, &qctrl, sizeof(qctrl)) != 0) {
			syslog(LOG_ERR,
			    "could not get audit queue settings");
				return (-1);
		}
		qctrl.aq_minfree = minval;
		if (auditon(A_SETQCTRL, &qctrl, sizeof(qctrl)) != 0) {
			syslog(LOG_ERR,
			    "could not set audit queue settings");
			return (-1);
		}
	}

	return (0);
}

/*
 * Close all log files, control files, and tell the audit system.
 */
static int
close_all(void)
{
	struct auditinfo ai;
	int err_ret = 0;
	char TS[POSTFIX_LEN];
	int aufd;
	token_t *tok;
	long cond;

	/* Generate an audit record. */
	if ((aufd = au_open()) == -1)
		syslog(LOG_ERR, "Could not create audit shutdown event.");
	else {
		if ((tok = au_to_text("auditd::Audit shutdown")) != NULL)
			au_write(aufd, tok);
		/*
		 * XXX we need to implement extended subject tokens so we can
		 * effectively represent terminal lines with this token type.
		 */
		bzero(&ai, sizeof(ai));
		if ((tok = au_to_subject32(getuid(), geteuid(), getegid(),
		    getuid(), getgid(), getpid(), getpid(), &ai.ai_termid))
		    != NULL)
			au_write(aufd, tok);
		if ((tok = au_to_return32(0, 0)) != NULL)
			au_write(aufd, tok);
		if (au_close(aufd, 1, AUE_audit_shutdown) == -1)
			syslog(LOG_ERR,
			    "Could not close audit shutdown event.");
	}

	/* Flush contents. */
	cond = AUC_DISABLED;
	err_ret = auditon(A_SETCOND, &cond, sizeof(cond));
	if (err_ret != 0) {
		syslog(LOG_ERR, "Disabling audit failed! : %s",
		    strerror(errno));
		err_ret = 1;
	}
	if (getTSstr(TS, POSTFIX_LEN) == 0)
		close_lastfile(TS);
	if (lastfile != NULL)
		free(lastfile);

	free_dir_q();
	if ((remove(AUDITD_PIDFILE) == -1) || err_ret) {
		syslog(LOG_ERR, "Could not unregister");
		audit_warn_postsigterm();
		return (1);
	}
	endac();

	if (close(triggerfd) != 0)
		syslog(LOG_ERR, "Error closing control file");
	syslog(LOG_INFO, "Finished");
	return (0);
}

/*
 * When we get a signal, we are often not at a clean point.  So, little can
 * be done in the signal handler itself.  Instead,  we send a message to the
 * main servicing loop to do proper handling from a non-signal-handler
 * context.
 */
static void
relay_signal(int signal)
{

	if (signal == SIGHUP)
		sighups++;
	if (signal == SIGTERM)
		sigterms++;
	if (signal == SIGCHLD)
		sigchlds++;
}

/*
 * Registering the daemon.
 */
static int
register_daemon(void)
{
	FILE * pidfile;
	int fd;
	pid_t pid;

	/* Set up the signal hander. */
	if (signal(SIGTERM, relay_signal) == SIG_ERR) {
		syslog(LOG_ERR,
		    "Could not set signal handler for SIGTERM");
		fail_exit();
	}
	if (signal(SIGCHLD, relay_signal) == SIG_ERR) {
		syslog(LOG_ERR,
		    "Could not set signal handler for SIGCHLD");
		fail_exit();
	}
	if (signal(SIGHUP, relay_signal) == SIG_ERR) {
		syslog(LOG_ERR,
		    "Could not set signal handler for SIGHUP");
		fail_exit();
	}

	if ((pidfile = fopen(AUDITD_PIDFILE, "a")) == NULL) {
		syslog(LOG_ERR, "Could not open PID file");
		audit_warn_tmpfile();
		return (-1);
	}

	/* Attempt to lock the pid file; if a lock is present, exit. */
	fd = fileno(pidfile);
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		syslog(LOG_ERR,
		    "PID file is locked (is another auditd running?).");
		audit_warn_ebusy();
		return (-1);
	}

	pid = getpid();
	ftruncate(fd, 0);
	if (fprintf(pidfile, "%u\n", pid) < 0) {
		/* Should not start the daemon. */
		fail_exit();
	}

	fflush(pidfile);
	return (0);
}

/*
 * Handle the audit trigger event.
 *
 * We suppress (ignore) duplicated triggers in close succession in order to
 * try to avoid thrashing-like behavior.  However, not all triggers can be
 * ignored, as triggers generally represent edge triggers, not level
 * triggers, and won't be retransmitted if the condition persists.  Of
 * specific concern is the rotate trigger -- if one is dropped, then it will
 * not be retransmitted, and the log file will grow in an unbounded fashion.
 */
#define	DUPLICATE_INTERVAL	30
static void
handle_audit_trigger(int trigger)
{
	static int last_trigger, last_warning;
	static time_t last_time;
	struct dir_ent *dirent;
	struct timeval ts;
	struct timezone tzp;
	time_t tt;

	/*
	 * Suppress duplicate messages from the kernel within the specified
	 * interval.
	 */
	if (gettimeofday(&ts, &tzp) == 0) {
		tt = (time_t)ts.tv_sec;
		switch (trigger) {
		case AUDIT_TRIGGER_LOW_SPACE:
		case AUDIT_TRIGGER_NO_SPACE:
			/*
			 * Triggers we can suppress.  Of course, we also need
			 * to rate limit the warnings, so apply the same
			 * interval limit on syslog messages.
			 */
			if ((trigger == last_trigger) &&
			    (tt < (last_time + DUPLICATE_INTERVAL))) {
				if (tt >= (last_warning + DUPLICATE_INTERVAL))
					syslog(LOG_INFO,
					    "Suppressing duplicate trigger %d",
					    trigger);
				return;
			}
			last_warning = tt;
			break;

		case AUDIT_TRIGGER_ROTATE_KERNEL:
		case AUDIT_TRIGGER_ROTATE_USER:
		case AUDIT_TRIGGER_READ_FILE:
			/*
			 * Triggers that we cannot suppress.
			 */
			break;
		}

		/*
		 * Only update last_trigger after aborting due to a duplicate
		 * trigger, not before, or we will never allow that trigger
		 * again.
		 */
		last_trigger = trigger;
		last_time = tt;
	}

	/*
	 * Message processing is done here.
 	 */
	dirent = TAILQ_FIRST(&dir_q);
	switch(trigger) {
	case AUDIT_TRIGGER_LOW_SPACE:
		syslog(LOG_INFO, "Got low space trigger");
		if (dirent && (dirent->softlim != 1)) {
			TAILQ_REMOVE(&dir_q, dirent, dirs);
				/* Add this node to the end of the list. */
				TAILQ_INSERT_TAIL(&dir_q, dirent, dirs);
				audit_warn_soft(dirent->dirname);
				dirent->softlim = 1;

			if (TAILQ_NEXT(TAILQ_FIRST(&dir_q), dirs) != NULL &&
			    swap_audit_file() == -1)
				syslog(LOG_ERR, "Error swapping audit file");

			/*
			 * Check if the next dir has already reached its soft
			 * limit.
			 */
			dirent = TAILQ_FIRST(&dir_q);
			if (dirent->softlim == 1)  {
				/* All dirs have reached their soft limit. */
				audit_warn_allsoft();
			}
		} else {
			/*
			 * Continue auditing to the current file.  Also
			 * generate an allsoft warning.
			 *
			 * XXX do we want to do this ?
			 */
			audit_warn_allsoft();
		}
		break;

	case AUDIT_TRIGGER_NO_SPACE:
		syslog(LOG_INFO, "Got no space trigger");

		/* Delete current dir, go on to next. */
		TAILQ_REMOVE(&dir_q, dirent, dirs);
		audit_warn_hard(dirent->dirname);
		free(dirent->dirname);
		free(dirent);

		if (swap_audit_file() == -1)
			syslog(LOG_ERR, "Error swapping audit file");

		/* We are out of log directories. */
		audit_warn_allhard(++allhardcount);
		break;

	case AUDIT_TRIGGER_ROTATE_KERNEL:
	case AUDIT_TRIGGER_ROTATE_USER:
		/*
		 * Create a new file and swap with the one being used in
		 * kernel
		 */
		syslog(LOG_INFO, "Got open new trigger from %s", trigger ==
		    AUDIT_TRIGGER_ROTATE_KERNEL ? "kernel" : "user");
		if (swap_audit_file() == -1)
			syslog(LOG_ERR, "Error swapping audit file");
		break;

	case AUDIT_TRIGGER_READ_FILE:
		syslog(LOG_INFO, "Got read file trigger");
		if (read_control_file() == -1)
			syslog(LOG_ERR, "Error in audit control file");
		if (config_audit_controls() == -1)
			syslog(LOG_ERR, "Error setting audit controls");
		break;

	default:
		syslog(LOG_ERR, "Got unknown trigger %d", trigger);
		break;
	}
}

static void
handle_sighup(void)
{

	sighups_handled = sighups;
	config_audit_controls();
}

/*
 * Reap our children.
 */
static void
reap_children(void)
{
	pid_t child;
	int wstatus;

	while ((child = waitpid(-1, &wstatus, WNOHANG)) > 0) {
		if (!wstatus)
			continue;
		syslog(LOG_INFO, "warn process [pid=%d] %s %d.", child,
		    ((WIFEXITED(wstatus)) ? "exited with non-zero status" :
		    "exited as a result of signal"),
		    ((WIFEXITED(wstatus)) ? WEXITSTATUS(wstatus) :
		    WTERMSIG(wstatus)));
	}
}

static void
handle_sigchld(void)
{

	sigchlds_handled = sigchlds;
	reap_children();
}

/*
 * Read the control file for triggers/signals and handle appropriately.
 */
static int
wait_for_events(void)
{
	int num;
	unsigned int trigger;

	for (;;) {
		num = read(triggerfd, &trigger, sizeof(trigger));
		if ((num == -1) && (errno != EINTR)) {
			syslog(LOG_ERR, "%s: error %d", __FUNCTION__, errno);
			return (-1);
		}
		if (sigterms != sigterms_handled) {
			syslog(LOG_DEBUG, "%s: SIGTERM", __FUNCTION__);
			break;
		}
		if (sigchlds != sigchlds_handled)
			handle_sigchld();
		if (sighups != sighups_handled) {
			syslog(LOG_DEBUG, "%s: SIGHUP", __FUNCTION__);
			handle_sighup();
		}
		if ((num == -1) && (errno == EINTR))
			continue;
		if (num == 0) {
			syslog(LOG_ERR, "%s: read EOF", __FUNCTION__);
			return (-1);
		}
		if (trigger == AUDIT_TRIGGER_CLOSE_AND_DIE)
			break;
		else
			handle_audit_trigger(trigger);
	}
	return (close_all());
}

/*
 * Configure the audit controls in the kernel: the event to class mapping,
 * kernel preselection mask, etc.
 */
static int
config_audit_controls(void)
{
	au_event_ent_t ev, *evp;
	au_evclass_map_t evc_map;
	au_mask_t aumask;
	int ctr = 0;
	char naeventstr[NA_EVENT_STR_SIZE];
	char polstr[POL_STR_SIZE];
	long policy;
	au_fstat_t au_fstat;
	size_t filesz;

	/*
	 * Process the audit event file, obtaining a class mapping for each
	 * event, and send that mapping into the kernel.
	 *
	 * XXX There's a risk here that the BSM library will return NULL
	 * for an event when it can't properly map it to a class. In that
	 * case, we will not process any events beyond the one that failed,
	 * but should. We need a way to get a count of the events.
	*/
	ev.ae_name = (char *)malloc(AU_EVENT_NAME_MAX);
	ev.ae_desc = (char *)malloc(AU_EVENT_DESC_MAX);
	if ((ev.ae_name == NULL) || (ev.ae_desc == NULL)) {
		if (ev.ae_name != NULL)
			free(ev.ae_name);
		syslog(LOG_ERR,
		    "Memory allocation error when configuring audit controls.");
		return (-1);
	}

	/*
	 * XXXRW: Currently we have no way to remove mappings from the kernel
	 * when they are removed from the file-based mappings.
	 */
	evp = &ev;
	setauevent();
	while ((evp = getauevent_r(evp)) != NULL) {
		evc_map.ec_number = evp->ae_number;
		evc_map.ec_class = evp->ae_class;
		if (auditon(A_SETCLASS, &evc_map, sizeof(au_evclass_map_t))
		    != 0)
			syslog(LOG_ERR,
				"Failed to register class mapping for event %s",
				 evp->ae_name);
		else
			ctr++;
	}
	endauevent();
	free(ev.ae_name);
	free(ev.ae_desc);
	if (ctr == 0)
		syslog(LOG_ERR, "No events to class mappings registered.");
	else
		syslog(LOG_DEBUG, "Registered %d event to class mappings.",
		    ctr);

	/*
	 * Get the non-attributable event string and set the kernel mask from
	 * that.
	 */
	if ((getacna(naeventstr, NA_EVENT_STR_SIZE) == 0) &&
	    (getauditflagsbin(naeventstr, &aumask) == 0)) {
		if (auditon(A_SETKMASK, &aumask, sizeof(au_mask_t)))
			syslog(LOG_ERR,
			    "Failed to register non-attributable event mask.");
		else
			syslog(LOG_DEBUG,
			    "Registered non-attributable event mask.");
	} else
		syslog(LOG_ERR,
		    "Failed to obtain non-attributable event mask.");

	/*
	 * If a policy is configured in audit_control(5), implement the
	 * policy.  However, if one isn't defined, set AUDIT_CNT to avoid
	 * leaving the system in a fragile state.
	 */
	if ((getacpol(polstr, POL_STR_SIZE) == 0) &&
	    (au_strtopol(polstr, &policy) == 0)) {
		if (auditon(A_SETPOLICY, &policy, sizeof(policy)))
			syslog(LOG_ERR, "Failed to set audit policy: %m");
	} else {
		syslog(LOG_ERR, "Failed to obtain policy flags: %m");
		policy = AUDIT_CNT;
		if (auditon(A_SETPOLICY, &policy, sizeof(policy)))
			syslog(LOG_ERR,
			    "Failed to set default audit policy: %m");
	}

	/*
	 * Set trail rotation size.
	 */
	if (getacfilesz(&filesz) == 0) {
		bzero(&au_fstat, sizeof(au_fstat));
		au_fstat.af_filesz = filesz;
		if (auditon(A_SETFSIZE, &au_fstat, sizeof(au_fstat)) < 0)
			syslog(LOG_ERR, "Failed to set filesz: %m");
	} else
		syslog(LOG_ERR, "Failed to obtain filesz: %m");

	return (0);
}

static void
setup(void)
{
	struct auditinfo ai;
	auditinfo_t auinfo;
	int aufd;
	token_t *tok;

	if ((triggerfd = open(AUDIT_TRIGGER_FILE, O_RDONLY, 0)) < 0) {
		syslog(LOG_ERR, "Error opening trigger file");
		fail_exit();
	}

	/*
	 * To provide event feedback cycles and avoid auditd becoming
	 * stalled if auditing is suspended, auditd and its children run
	 * without their events being audited.  We allow the uid, tid, and
	 * mask fields to be implicitly set to zero, but do set the pid.  We
	 * run this after opening the trigger device to avoid configuring
	 * audit state without audit present in the system.
	 *
	 * XXXRW: Is there more to it than this?
	 */
	bzero(&auinfo, sizeof(auinfo));
	auinfo.ai_asid = getpid();
	if (setaudit(&auinfo) == -1) {
		syslog(LOG_ERR, "Error setting audit stat");
		fail_exit();
	}

	TAILQ_INIT(&dir_q);
	if (read_control_file() == -1) {
		syslog(LOG_ERR, "Error reading control file");
		fail_exit();
	}

	/* Generate an audit record. */
	if ((aufd = au_open()) == -1)
		syslog(LOG_ERR, "Could not create audit startup event.");
	else {
		/*
		 * XXXCSJP Perhaps we wan't more robust audit records for
		 * audit start up and shutdown. This might include capturing
		 * failures to initialize the audit subsystem?
		 */
		bzero(&ai, sizeof(ai));
		if ((tok = au_to_subject32(getuid(), geteuid(), getegid(),
		    getuid(), getgid(), getpid(), getpid(), &ai.ai_termid))
		    != NULL)
			au_write(aufd, tok);
		if ((tok = au_to_text("auditd::Audit startup")) != NULL)
			au_write(aufd, tok);
		if ((tok = au_to_return32(0, 0)) != NULL)
			au_write(aufd, tok);
		if (au_close(aufd, 1, AUE_audit_startup) == -1)
			syslog(LOG_ERR,
			    "Could not close audit startup event.");
	}

	if (config_audit_controls() == 0)
		syslog(LOG_INFO, "Audit controls init successful");
	else
		syslog(LOG_ERR, "Audit controls init failed");
}

int
main(int argc, char **argv)
{
	int ch;
	int debug = 0;
	int rc;

	while ((ch = getopt(argc, argv, "dhs")) != -1) {
		switch(ch) {
		case 'd':
			/* Debug option. */
			debug = 1;
			break;

		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: auditd [-d] \n");
			exit(1);
		}
	}

#ifdef LOG_SECURITY
	openlog("auditd", LOG_CONS | LOG_PID, LOG_SECURITY);
#else
	openlog("auditd", LOG_CONS | LOG_PID, LOG_AUTH);
#endif
	syslog(LOG_INFO, "starting...");

	if (debug == 0 && daemon(0, 0) == -1) {
		syslog(LOG_ERR, "Failed to daemonize");
		exit(1);
	}

	if (register_daemon() == -1) {
		syslog(LOG_ERR, "Could not register as daemon");
		exit(1);
	}

	setup();

	rc = wait_for_events();
	syslog(LOG_INFO, "auditd exiting.");

	exit(rc);
}
