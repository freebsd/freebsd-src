/*	$NetBSD: lockd_lock.c,v 1.5 2000/11/21 03:47:41 enami Exp $	*/
/*	$FreeBSD$ */

/*
 * Copyright (c) 2000 Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <rpcsvc/sm_inter.h>
#include <rpcsvc/nlm_prot.h>
#include "lockd_lock.h"
#include "lockd.h"

/* A set of utilities for managing file locking */
LIST_HEAD(lcklst_head, file_lock);
struct lcklst_head lcklst_head = LIST_HEAD_INITIALIZER(lcklst_head);

/* struct describing a lock */
struct file_lock {
	LIST_ENTRY(file_lock) lcklst;
	fhandle_t filehandle; /* NFS filehandle */
	struct sockaddr *addr;
	struct nlm4_holder client; /* lock holder */
	netobj client_cookie; /* cookie sent by the client */
	char client_name[128];
	int nsm_status; /* status from the remote lock manager */
	int status; /* lock status, see below */
	int flags; /* lock flags, see lockd_lock.h */
	pid_t locker; /* pid of the child process trying to get the lock */
	int fd;	/* file descriptor for this lock */
};

/* lock status */
#define LKST_LOCKED	1 /* lock is locked */
#define LKST_WAITING	2 /* file is already locked by another host */
#define LKST_PROCESSING	3 /* child is trying to aquire the lock */
#define LKST_DYING	4 /* must dies when we get news from the child */

void lfree __P((struct file_lock *));
enum nlm_stats do_lock __P((struct file_lock *, int));
enum nlm_stats do_unlock __P((struct file_lock *));
void send_granted __P((struct file_lock *, int));
void siglock __P((void));
void sigunlock __P((void));

/* list of hosts we monitor */
LIST_HEAD(hostlst_head, host);
struct hostlst_head hostlst_head = LIST_HEAD_INITIALIZER(hostlst_head);

/* struct describing a lock */
struct host {
	LIST_ENTRY(host) hostlst;
	char name[SM_MAXSTRLEN];
	int refcnt;
};

void do_mon __P((char *));

/*
 * testlock(): inform the caller if the requested lock would be granted or not
 * returns NULL if lock would granted, or pointer to the current nlm4_holder
 * otherwise.
 */

struct nlm4_holder *
testlock(lock, flags)
	struct nlm4_lock *lock;
	int flags;
{
	struct file_lock *fl;
	fhandle_t filehandle;

	/* convert lock to a local filehandle */
	memcpy(&filehandle, lock->fh.n_bytes, sizeof(filehandle));

	siglock();
	/* search through the list for lock holder */
	for (fl = LIST_FIRST(&lcklst_head); fl != NULL;
	    fl = LIST_NEXT(fl, lcklst)) {
		if (fl->status != LKST_LOCKED)
			continue;
		if (memcmp(&fl->filehandle, &filehandle, sizeof(filehandle)))
			continue;
		/* got it ! */
		syslog(LOG_DEBUG, "test for %s: found lock held by %s",
		    lock->caller_name, fl->client_name);
		sigunlock();
		return (&fl->client);
	}
	/* not found */
	sigunlock();
	syslog(LOG_DEBUG, "test for %s: no lock found", lock->caller_name);
	return NULL;
}

/*
 * getlock: try to aquire the lock. 
 * If file is already locked and we can sleep, put the lock in the list with
 * status LKST_WAITING; it'll be processed later.
 * Otherwise try to lock. If we're allowed to block, fork a child which
 * will do the blocking lock.
 */
enum nlm_stats
getlock(lckarg, rqstp, flags)
	nlm4_lockargs * lckarg;
	struct svc_req *rqstp;
	int flags;
{
	struct file_lock *fl, *newfl;
	enum nlm_stats retval;

	if (grace_expired == 0 && lckarg->reclaim == 0)
		return (flags & LOCK_V4) ?
		    nlm4_denied_grace_period : nlm_denied_grace_period;
			
	/* allocate new file_lock for this request */
	newfl = malloc(sizeof(struct file_lock));
	if (newfl == NULL) {
		syslog(LOG_NOTICE, "malloc failed: %s", strerror(errno));
		/* failed */
		return (flags & LOCK_V4) ?
		    nlm4_denied_nolocks : nlm_denied_nolocks;
	}
	if (lckarg->alock.fh.n_len != sizeof(fhandle_t)) {
		syslog(LOG_DEBUG, "recieved fhandle size %d, local size %d",
		    lckarg->alock.fh.n_len, (int)sizeof(fhandle_t));
	}
	memcpy(&newfl->filehandle, lckarg->alock.fh.n_bytes, sizeof(fhandle_t));
	newfl->addr = (struct sockaddr *)svc_getrpccaller(rqstp->rq_xprt)->buf;
	newfl->client.exclusive = lckarg->exclusive;
	newfl->client.svid = lckarg->alock.svid;
	newfl->client.oh.n_bytes = malloc(lckarg->alock.oh.n_len);
	if (newfl->client.oh.n_bytes == NULL) {
		syslog(LOG_NOTICE, "malloc failed: %s", strerror(errno));
		free(newfl);
		return (flags & LOCK_V4) ?
		    nlm4_denied_nolocks : nlm_denied_nolocks;
	}
	newfl->client.oh.n_len = lckarg->alock.oh.n_len;
	memcpy(newfl->client.oh.n_bytes, lckarg->alock.oh.n_bytes,
	    lckarg->alock.oh.n_len);
	newfl->client.l_offset = lckarg->alock.l_offset;
	newfl->client.l_len = lckarg->alock.l_len;
	newfl->client_cookie.n_len = lckarg->cookie.n_len;
	newfl->client_cookie.n_bytes = malloc(lckarg->cookie.n_len);
	if (newfl->client_cookie.n_bytes == NULL) {
		syslog(LOG_NOTICE, "malloc failed: %s", strerror(errno));
		free(newfl->client.oh.n_bytes);
		free(newfl);
		return (flags & LOCK_V4) ? 
		    nlm4_denied_nolocks : nlm_denied_nolocks;
	}
	memcpy(newfl->client_cookie.n_bytes, lckarg->cookie.n_bytes,
	    lckarg->cookie.n_len);
	strncpy(newfl->client_name, lckarg->alock.caller_name, 128);
	newfl->nsm_status = lckarg->state;
	newfl->status = 0;
	newfl->flags = flags;
	siglock();
	/* look for a lock rq from this host for this fh */
	for (fl = LIST_FIRST(&lcklst_head); fl != NULL;
	    fl = LIST_NEXT(fl, lcklst)) {
		if (memcmp(&newfl->filehandle, &fl->filehandle,
		    sizeof(fhandle_t)) == 0) {
			if (strcmp(newfl->client_name, fl->client_name) == 0 &&
			    newfl->client.svid == fl->client.svid) {
				/* already locked by this host ??? */
				sigunlock();
				syslog(LOG_NOTICE, "duplicate lock from %s",
				    newfl->client_name);
				lfree(newfl);
				switch(fl->status) {
				case LKST_LOCKED:
					return (flags & LOCK_V4) ?
					    nlm4_granted : nlm_granted;
				case LKST_WAITING:
				case LKST_PROCESSING:
					return (flags & LOCK_V4) ?
					    nlm4_blocked : nlm_blocked;
				case LKST_DYING:
					return (flags & LOCK_V4) ?
					    nlm4_denied : nlm_denied;
				default:
					syslog(LOG_NOTICE, "bad status %d",
					    fl->status);
					return (flags & LOCK_V4) ?
					    nlm4_failed : nlm_denied;
				}
			}
			/*
			 * We already have a lock for this file. Put this one
			 * in waiting state if allowed to block
			 */
			if (lckarg->block) {
				syslog(LOG_DEBUG, "lock from %s: already "
				    "locked, waiting",
				    lckarg->alock.caller_name);
				newfl->status = LKST_WAITING;
				LIST_INSERT_HEAD(&lcklst_head, newfl, lcklst);
				do_mon(lckarg->alock.caller_name);
				sigunlock();
				return (flags & LOCK_V4) ?
				    nlm4_blocked : nlm_blocked;
			} else {
				sigunlock();
				syslog(LOG_DEBUG, "lock from %s: already "
				    "locked, failed",
				    lckarg->alock.caller_name);
				lfree(newfl);
				return (flags & LOCK_V4) ?
				    nlm4_denied : nlm_denied;
			}
		}
	}
	/* no entry for this file yet; add to list */
	LIST_INSERT_HEAD(&lcklst_head, newfl, lcklst);
	/* do the lock */
	retval = do_lock(newfl, lckarg->block);
	switch (retval) {
	case nlm4_granted:
	/* case nlm_granted: is the same as nlm4_granted */
	case nlm4_blocked:
	/* case nlm_blocked: is the same as nlm4_blocked */
		do_mon(lckarg->alock.caller_name);
		break;
	default:
		lfree(newfl);
		break;
	}
	sigunlock();
	return retval;
}

/* unlock a filehandle */
enum nlm_stats
unlock(lck, flags)
	nlm4_lock *lck;
	int flags;
{
	struct file_lock *fl;
	fhandle_t filehandle;
	int err = (flags & LOCK_V4) ? nlm4_granted : nlm_granted;

	memcpy(&filehandle, lck->fh.n_bytes, sizeof(fhandle_t));
	siglock();
	for (fl = LIST_FIRST(&lcklst_head); fl != NULL;
	    fl = LIST_NEXT(fl, lcklst)) {
		if (strcmp(fl->client_name, lck->caller_name) ||
		    memcmp(&filehandle, &fl->filehandle, sizeof(fhandle_t)) ||
		    fl->client.oh.n_len != lck->oh.n_len ||
		    memcmp(fl->client.oh.n_bytes, lck->oh.n_bytes,
			fl->client.oh.n_len) != 0 ||
		    fl->client.svid != lck->svid)
			continue;
		/* Got it, unlock and remove from the queue */
		syslog(LOG_DEBUG, "unlock from %s: found struct, status %d",
		    lck->caller_name, fl->status);
		switch (fl->status) {
		case LKST_LOCKED:
			err = do_unlock(fl);
			break;
		case LKST_WAITING:
			/* remove from the list */
			LIST_REMOVE(fl, lcklst);
			lfree(fl);
			break;
		case LKST_PROCESSING:
			/*
			 * being handled by a child; will clean up
			 * when the child exits
			 */
			fl->status = LKST_DYING;
			break;
		case LKST_DYING:
			/* nothing to do */
			break;
		default:
			syslog(LOG_NOTICE, "unknow status %d for %s",
			    fl->status, fl->client_name);
		}
		sigunlock();
		return err;
	}
	sigunlock();
	/* didn't find a matching entry; log anyway */
	syslog(LOG_NOTICE, "no matching entry for %s",
	    lck->caller_name);
	return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
}

void
lfree(fl)
	struct file_lock *fl;
{
	free(fl->client.oh.n_bytes);
	free(fl->client_cookie.n_bytes);
	free(fl);
}

void
sigchild_handler(sig)
	int sig;
{
	int status;
	pid_t pid;
	struct file_lock *fl;

	while (1) {
		pid = wait4(-1, &status, WNOHANG, NULL);
		if (pid == -1) {
			if (errno != ECHILD)
				syslog(LOG_NOTICE, "wait failed: %s",
				    strerror(errno));
			else
				syslog(LOG_DEBUG, "wait failed: %s",
				    strerror(errno));
			return;
		}
		if (pid == 0) {
			/* no more child to handle yet */
			return;
		}
		/*
		 * if we're here we have a child that exited
		 * Find the associated file_lock.
		 */
		for (fl = LIST_FIRST(&lcklst_head); fl != NULL;
		    fl = LIST_NEXT(fl, lcklst)) {
			if (pid == fl->locker)
				break;
		}
		if (pid != fl->locker) {
			syslog(LOG_NOTICE, "unknow child %d", pid);
		} else {
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				syslog(LOG_NOTICE, "child %d failed", pid);
				/*
				 * can't do much here; we can't reply
				 * anything but OK for blocked locks
				 * Eventually the client will time out
				 * and retry.
				 */
				do_unlock(fl);
				return;
			}
			    
			/* check lock status */
			syslog(LOG_DEBUG, "processing child %d, status %d",
			    pid, fl->status);
			switch(fl->status) {
			case LKST_PROCESSING:
				fl->status = LKST_LOCKED;
				send_granted(fl, (fl->flags & LOCK_V4) ?
				    nlm4_granted : nlm_granted);
				break;
			case LKST_DYING:
				do_unlock(fl);
				break;
			default:
				syslog(LOG_NOTICE, "bad lock status (%d) for"
				   " child %d", fl->status, pid);
			}
		}
	}
}

/*
 *
 * try to aquire the lock described by fl. Eventually fock a child to do a
 * blocking lock if allowed and required.
 */

enum nlm_stats
do_lock(fl, block)
	struct file_lock *fl;
	int block;
{
	int lflags, error;
	struct stat st;

	fl->fd = fhopen(&fl->filehandle, O_RDWR);
	if (fl->fd < 0) {
		switch (errno) {
		case ESTALE:
			error = nlm4_stale_fh;
			break;
		case EROFS:
			error = nlm4_rofs;
			break;
		default:
			error = nlm4_failed;
		}
		if ((fl->flags & LOCK_V4) == 0)
			error = nlm_denied;
		syslog(LOG_NOTICE, "fhopen failed (from %s): %s",
		    fl->client_name, strerror(errno));
		LIST_REMOVE(fl, lcklst);
		return error;;
	}
	if (fstat(fl->fd, &st) < 0) {
		syslog(LOG_NOTICE, "fstat failed (from %s): %s",
		    fl->client_name, strerror(errno));
	}
	syslog(LOG_DEBUG, "lock from %s for file%s%s: dev %d ino %d (uid %d), "
	    "flags %d",
	    fl->client_name, fl->client.exclusive ? " (exclusive)":"",
	    block ? " (block)":"",
	    st.st_dev, st.st_ino, st.st_uid, fl->flags);
	lflags = LOCK_NB;
	if (fl->client.exclusive == 0)
		lflags |= LOCK_SH;
	else
		lflags |= LOCK_EX;
	error = flock(fl->fd, lflags);
	if (error != 0 && errno == EAGAIN && block) {
		switch (fl->locker = fork()) {
		case -1: /* fork failed */
			syslog(LOG_NOTICE, "fork failed: %s", strerror(errno));
			LIST_REMOVE(fl, lcklst);
			close(fl->fd);
			return (fl->flags & LOCK_V4) ?
			    nlm4_denied_nolocks : nlm_denied_nolocks;
		case 0:
			/*
			 * Attempt a blocking lock. Will have to call
			 * NLM_GRANTED later.
			 */
			setproctitle("%s", fl->client_name);
			lflags &= ~LOCK_NB;
			if(flock(fl->fd, lflags) != 0) {
				syslog(LOG_NOTICE, "flock failed: %s",
				    strerror(errno));
				exit(-1);
			}
			/* lock granted */	
			exit(0);
		default:
			syslog(LOG_DEBUG, "lock request from %s: forked %d",
			    fl->client_name, fl->locker);
			fl->status = LKST_PROCESSING;
			return (fl->flags & LOCK_V4) ?
			    nlm4_blocked : nlm_blocked;
		}
	}
	/* non block case */
	if (error != 0) {
		switch (errno) {
		case EAGAIN:
			error = nlm4_denied;
			break;
		case ESTALE:
			error = nlm4_stale_fh;
			break;
		case EROFS:
			error = nlm4_rofs;
			break;
		default:
			error = nlm4_failed;
		}
		if ((fl->flags & LOCK_V4) == 0)
			error = nlm_denied;
		if (errno != EAGAIN)
			syslog(LOG_NOTICE, "flock for %s failed: %s",
			    fl->client_name, strerror(errno));
		else syslog(LOG_DEBUG, "flock for %s failed: %s",
			    fl->client_name, strerror(errno));
		LIST_REMOVE(fl, lcklst);
		close(fl->fd);
		return error;
	}
	fl->status = LKST_LOCKED;
	return (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
}

void
send_granted(fl, opcode)
	struct file_lock *fl;
	int opcode;
{
	CLIENT *cli;
	static char dummy;
	struct timeval timeo;
	int success;
	static struct nlm_res retval;
	static struct nlm4_res retval4;

	cli = get_client(fl->addr,
	    (fl->flags & LOCK_V4) ? NLM_VERS4 : NLM_VERS);
	if (cli == NULL) {
		syslog(LOG_NOTICE, "failed to get CLIENT for %s",
		    fl->client_name);
		/*
		 * We fail to notify remote that the lock has been granted.
		 * The client will timeout and retry, the lock will be
		 * granted at this time.
		 */
		return;
	}
	timeo.tv_sec = 0;
	timeo.tv_usec = (fl->flags & LOCK_ASYNC) ? 0 : 500000; /* 0.5s */

	if (fl->flags & LOCK_V4) {
		static nlm4_testargs res;
		res.cookie = fl->client_cookie;
		res.exclusive = fl->client.exclusive;
		res.alock.caller_name = fl->client_name;
		res.alock.fh.n_len = sizeof(fhandle_t);
		res.alock.fh.n_bytes = (char*)&fl->filehandle;
		res.alock.oh = fl->client.oh;
		res.alock.svid = fl->client.svid;
		res.alock.l_offset = fl->client.l_offset;
		res.alock.l_len = fl->client.l_len;
		syslog(LOG_DEBUG, "sending v4 reply%s",
		    (fl->flags & LOCK_ASYNC) ? " (async)":"");
		if (fl->flags & LOCK_ASYNC) {
			success = clnt_call(cli, NLM4_GRANTED_MSG,
			    xdr_nlm4_testargs, &res, xdr_void, &dummy, timeo);
		} else {
			success = clnt_call(cli, NLM4_GRANTED,
			    xdr_nlm4_testargs, &res, xdr_nlm4_res,
			    &retval4, timeo);
		}
	} else {
		static nlm_testargs res;

		res.cookie = fl->client_cookie;
		res.exclusive = fl->client.exclusive;
		res.alock.caller_name = fl->client_name;
		res.alock.fh.n_len = sizeof(fhandle_t);
		res.alock.fh.n_bytes = (char*)&fl->filehandle;
		res.alock.oh = fl->client.oh;
		res.alock.svid = fl->client.svid;
		res.alock.l_offset = fl->client.l_offset;
		res.alock.l_len = fl->client.l_len;
		syslog(LOG_DEBUG, "sending v1 reply%s",
		    (fl->flags & LOCK_ASYNC) ? " (async)":"");
		if (fl->flags & LOCK_ASYNC) {
			success = clnt_call(cli, NLM_GRANTED_MSG,
			    xdr_nlm_testargs, &res, xdr_void, &dummy, timeo);
		} else {
			success = clnt_call(cli, NLM_GRANTED,
			    xdr_nlm_testargs, &res, xdr_nlm_res,
			    &retval, timeo);
		}
	}
	if (debug_level > 2)
		syslog(LOG_DEBUG, "clnt_call returns %d(%s) for granted",
		    success, clnt_sperrno(success));

}

enum nlm_stats
do_unlock(rfl)
	struct file_lock *rfl;
{
	struct file_lock *fl;
	int error;
	int lockst;

	/* unlock the file: closing is enouth ! */
	if (close(rfl->fd) < 0) {
		if (errno == ESTALE)
			error = nlm4_stale_fh;
		else
			error = nlm4_failed;
		if ((fl->flags & LOCK_V4) == 0)
			error = nlm_denied;
		syslog(LOG_NOTICE,
		    "close failed (from %s): %s",
		    rfl->client_name, strerror(errno));
	} else {
		error = (fl->flags & LOCK_V4) ?
		    nlm4_granted : nlm_granted;
	}
	LIST_REMOVE(rfl, lcklst);

	/* process the next LKST_WAITING lock request for this fh */
	for (fl = LIST_FIRST(&lcklst_head); fl != NULL;
	     fl = LIST_NEXT(fl, lcklst)) {
		if (fl->status != LKST_WAITING ||
		    memcmp(&rfl->filehandle, &fl->filehandle,
		    sizeof(fhandle_t)) != 0)
			continue;

		lockst = do_lock(fl, 1); /* If it's LKST_WAITING we can block */
		switch (lockst) {
		case nlm4_granted:
		/* case nlm_granted: same as nlm4_granted */
			send_granted(fl, (fl->flags & LOCK_V4) ?
			    nlm4_granted : nlm_granted);
			break;
		case nlm4_blocked:
		/* case nlm_blocked: same as nlm4_blocked */
			break;
		default:
			lfree(fl);
			break;
		}
		break;
	}
	return error;
}

void
siglock()
{
	sigset_t block;
	
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);

	if (sigprocmask(SIG_BLOCK, &block, NULL) < 0) {
		syslog(LOG_WARNING, "siglock failed: %s", strerror(errno));
	}
}

void
sigunlock()
{
	sigset_t block;
	
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);

	if (sigprocmask(SIG_UNBLOCK, &block, NULL) < 0) {
		syslog(LOG_WARNING, "sigunlock failed: %s", strerror(errno));
	}
}

/* monitor a host through rpc.statd, and keep a ref count */
void
do_mon(hostname)
	char *hostname;
{
	struct host *hp;
	struct mon my_mon;
	struct sm_stat_res res;
	int retval;

	for (hp = LIST_FIRST(&hostlst_head); hp != NULL;
	    hp = LIST_NEXT(hp, hostlst)) {
		if (strcmp(hostname, hp->name) == 0) {
			/* already monitored, just bump refcnt */
			hp->refcnt++;
			return;
		}
	}
	/* not found, have to create an entry for it */
	hp = malloc(sizeof(struct host));
	strncpy(hp->name, hostname, SM_MAXSTRLEN);
	hp->refcnt = 1;
	syslog(LOG_DEBUG, "monitoring host %s",
	    hostname);
	memset(&my_mon, 0, sizeof(my_mon));
	my_mon.mon_id.mon_name = hp->name;
	my_mon.mon_id.my_id.my_name = "localhost";
	my_mon.mon_id.my_id.my_prog = NLM_PROG;
	my_mon.mon_id.my_id.my_vers = NLM_SM;
	my_mon.mon_id.my_id.my_proc = NLM_SM_NOTIFY;
	if ((retval =
	    callrpc("localhost", SM_PROG, SM_VERS, SM_MON, xdr_mon,
	    (char*)&my_mon, xdr_sm_stat_res, (char*)&res)) != 0) {
		syslog(LOG_WARNING, "rpc to statd failed: %s",
		    clnt_sperrno((enum clnt_stat)retval));
		free(hp);
		return;
	}
	if (res.res_stat == stat_fail) {
		syslog(LOG_WARNING, "statd failed");
		free(hp);
		return;
	}
	LIST_INSERT_HEAD(&hostlst_head, hp, hostlst);
}

void
notify(hostname, state)
	char *hostname;
	int state;
{
	struct file_lock *fl, *next_fl;
	int err;
	syslog(LOG_DEBUG, "notify from %s, new state %d", hostname, state);
	/* search all lock for this host; if status changed, release the lock */
	siglock();
	for (fl = LIST_FIRST(&lcklst_head); fl != NULL; fl = next_fl) {
		next_fl = LIST_NEXT(fl, lcklst);
		if (strcmp(hostname, fl->client_name) == 0 &&
		    fl->nsm_status != state) {
			syslog(LOG_DEBUG, "state %d, nsm_state %d, unlocking",
			    fl->status, fl->nsm_status);
			switch(fl->status) {
			case LKST_LOCKED:
				err = do_unlock(fl);
				if (err != nlm_granted)
					syslog(LOG_DEBUG,
					    "notify: unlock failed for %s (%d)",
			    		    hostname, err);
				break;
			case LKST_WAITING:
				LIST_REMOVE(fl, lcklst);
				lfree(fl);
				break;
			case LKST_PROCESSING:
				fl->status = LKST_DYING;
				break;
			case LKST_DYING:
				break;
			default:
				syslog(LOG_NOTICE, "unknow status %d for %s",
				    fl->status, fl->client_name);
			}
		}
	}
	sigunlock();
}
