/*	$NetBSD: lockd_lock.c,v 1.5 2000/11/21 03:47:41 enami Exp $	*/
/*	$FreeBSD$ */

/*
 * Copyright (c) 2001 Andrew P. Lentvorski, Jr.
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

#define LOCKD_DEBUG

#include <stdio.h>
#ifdef LOCKD_DEBUG
#include <stdarg.h>
#endif
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

#define MAXOBJECTSIZE 64
#define MAXBUFFERSIZE 1024

/*
 * SM_MAXSTRLEN is usually 1024.  This means that lock requests and
 * host name monitoring entries are *MUCH* larger than they should be
 */

/*
 * A set of utilities for managing file locking
 *
 * XXX: All locks are in a linked list, a better structure should be used
 * to improve search/access effeciency.
 */
LIST_HEAD(nfslocklist_head, file_lock);
struct nfslocklist_head nfslocklist_head = LIST_HEAD_INITIALIZER(nfslocklist_head);

/* struct describing a lock */
struct file_lock {
	LIST_ENTRY(file_lock) nfslocklist;
	fhandle_t filehandle; /* NFS filehandle */
	struct sockaddr *addr;
	struct nlm4_holder client; /* lock holder */
	netobj client_cookie; /* cookie sent by the client */
	char client_name[SM_MAXSTRLEN];
	int nsm_status; /* status from the remote lock manager */
	int status; /* lock status, see below */
	int flags; /* lock flags, see lockd_lock.h */
	pid_t locker; /* pid of the child process trying to get the lock */
	int fd;	/* file descriptor for this lock */
};

/* lock status */
#define LKST_LOCKED	1 /* lock is locked */
/* XXX: Is this flag file specific or lock specific? */
#define LKST_WAITING	2 /* file is already locked by another host */
#define LKST_PROCESSING	3 /* child is trying to aquire the lock */
#define LKST_DYING	4 /* must dies when we get news from the child */

/* list of hosts we monitor */
LIST_HEAD(hostlst_head, host);
struct hostlst_head hostlst_head = LIST_HEAD_INITIALIZER(hostlst_head);

/* struct describing a lock */
struct host {
	LIST_ENTRY(host) hostlst;
	char name[SM_MAXSTRLEN];
	int refcnt;
};

static int debugdelay = 0;

enum nfslock_status { NFS_GRANTED = 0, NFS_GRANTED_DUPLICATE, NFS_DENIED,
		      NFS_DENIED_NOLOCK};

enum partialfilelock_status { PFL_GRANTED=0, PFL_GRANTED_DUPLICATE, PFL_DENIED,
			      PFL_NFSDENIED, PFL_NFSDENIED_NOLOCK };

void siglock(void);
void sigunlock(void);

void
debuglog(char const *fmt, ...)
{
#ifdef LOCKD_DEBUG
	va_list ap;

	if (debug_level < 1)
		return;

	sleep(debugdelay);

	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
#endif
}

void
dump_static_object(object, s_object, hbuff, s_hbuff, cbuff, s_cbuff)
	const unsigned char* object;
	const int s_object;
	unsigned char* hbuff;
	const int s_hbuff;
	unsigned char* cbuff;
	const int s_cbuff;
{ 
	int i, objectsize;

	if (debug_level < 2)
		return;

	objectsize = s_object;

	if (objectsize == 0) {
		debuglog("object is size 0\n");
		return;
	}

	if (objectsize > MAXOBJECTSIZE) {
		debuglog("Object of size %d being clamped"
		    "to size %d\n", objectsize, MAXOBJECTSIZE);
		objectsize = MAXOBJECTSIZE;
	}
		
	if (hbuff != NULL) {
		if (s_hbuff < objectsize*2+1) {
			debuglog("Hbuff not large enough. Increase size\n");
		} else {
			for (i = 0; i < objectsize; i++) {
				sprintf(hbuff + i * 2, "%02x", *(object+i));
			}
			*(hbuff+i*2) = '\0';
		}
	}
		
	if (cbuff != NULL) {
		if (s_cbuff < objectsize+1) {
			debuglog("Cbuff not large enough."
			    "  Increase Size\n");
		}
		
		for(i=0;i<objectsize;i++) {
			if (*(object+i) >= 32 && *(object+i) <= 127) {
				*(cbuff+i) = *(object+i);
			} else {
				*(cbuff+i) = '.';
			}
		}
		*(cbuff+i) = '\0';
	}
}

void
dump_netobj(const struct netobj *nobj)
{
	char hbuff[MAXBUFFERSIZE*2];
	char cbuff[MAXBUFFERSIZE];

	if (debug_level < 2)
		return;

	if (nobj == NULL) {
		debuglog("Null netobj pointer\n");
	} else if (nobj->n_len == 0) {
		debuglog("Size zero netobj\n");
	} else {
		dump_static_object(nobj->n_bytes, nobj->n_len,
		    hbuff, sizeof(hbuff), cbuff, sizeof(cbuff));
		debuglog("netobj: len: %d  data: %s :::  %s\n",
		    nobj->n_len,hbuff,cbuff);
	}
}

void
dump_filelock(const struct file_lock *fl)
{
	char hbuff[MAXBUFFERSIZE*2];
	char cbuff[MAXBUFFERSIZE];

	if (debug_level < 2)
		return;

	if (fl == NULL) {
		debuglog("NULL file lock structure\n");
		return;
	}

	debuglog("Dumping file lock structure\n");

	dump_static_object((unsigned char *)&fl->filehandle,
	    sizeof(fl->filehandle), hbuff, sizeof(hbuff),
	    cbuff, sizeof(cbuff));
	debuglog("Filehandle: %8s  :::  %8s\n", hbuff, cbuff);

	debuglog("Dumping nlm4_holder:\n");
	debuglog("exc: %x  svid: %x  offset:len %llx:%llx\n",
	    fl->client.exclusive, fl->client.svid,
	    fl->client.l_offset, fl->client.l_len);

	debuglog("Dumping client identity:\n");
	dump_netobj(&fl->client.oh);

	debuglog("Dumping client cookie:\n");
	dump_netobj(&fl->client_cookie);

	debuglog("nsm: %d  status: %d  flags: %d  locker: %d"
	    "  fd:  %d\n", fl->nsm_status, fl->status,
	    fl->flags, fl->locker, fl->fd);
}

void
copy_nlm4_lock_to_nlm4_holder(src, exclusive, dest)
	const struct nlm4_lock *src;
	const bool_t exclusive;
	struct nlm4_holder *dest;
{

	dest->exclusive = exclusive;
	dest->oh.n_len = src->oh.n_len;
	dest->oh.n_bytes = src->oh.n_bytes;
	dest->svid = src->svid;
	dest->l_offset = src->l_offset;
	dest->l_len = src->l_len;
}

/*
 * deallocate_file_lock: Free all storage associated with a file lock
 */

void
deallocate_file_lock(struct file_lock *fl)
{
  /* XXX: Check to see if this gets *all* the dynamic structures */
  /* XXX: It should be placed closer to the file_lock definition */

	free(fl->client.oh.n_bytes);
	free(fl->client_cookie.n_bytes);
	free(fl);
}

/*
 * regions_overlap(): This function examines the two provided regions for
 * overlap.  It is non-trivial because start+len *CAN* overflow a 64-bit
 * unsigned integer and NFS semantics are unspecified on this account.
 */
int
regions_overlap(start1, len1, start2, len2)
	const u_int64_t start1, len1, start2, len2;
{
	/* XXX: Check to make sure I got *ALL* the cases */
	/* XXX: This DESPERATELY needs a regression test */
	debuglog("Entering region overlap with vals: %llu:%llu--%llu:%llu\n",
		 start1, len1, start2, len2);

	/* XXX: Look for a way to collapse the region checks */

	/* XXX: Need to adjust checks to account for integer overflow */
	if (len1 == 0 && len2 == 0)
		return (1);

	if (len1 == 0) {
		/* Region 2 is completely left of region 1 */
		return (!(start2+len2 <= start1));
	} else if (len2 == 0) {
		/* Region 1 is completely left of region 2 */
		return (!(start1+len1 <= start2));
	} else {
		/*
		 * 1 is completely left of 2 or
		 * 2 is completely left of 1
		 */
		return (!(start1+len1 <= start2 || start2+len2 <= start1));
	}
}

/*
 * same_netobj: Compares the apprpriate bits of a netobj for identity
 */
int
same_netobj(const netobj *n0, const netobj *n1)
{
	int retval;

	retval=0;

	debuglog("Entering netobj identity check\n");

	if (n0->n_len == n1->n_len) {
		debuglog("Preliminary length check passed\n");
		
		if (!bcmp(n0->n_bytes,n1->n_bytes,n0->n_len)) {
			retval = 1;
			debuglog("netobj match\n");
		} else {
			debuglog("netobj mismatch\n");
		}
	}
	
	return retval;
}

/*
 * same_filelock_identity: Compares the appropriate bits of a file_lock
 */
int
same_filelock_identity(const struct file_lock *fl0,
    const struct file_lock *fl1)
{
	int retval;

	retval = 0;

	debuglog("Checking filelock identity\n");

	if (fl0->client.svid == fl1->client.svid) {
		/* Process ids match now check host information */
		retval = same_netobj(&(fl0->client.oh),&(fl1->client.oh));
	}

	debuglog("Exiting checking filelock identity: retval: %d\n",retval);

	return retval;
}

/*
 * Below here are routines associated with manipulating the NFS
 * lock list.
 */

/*
 * get_lock_matching_unlock: Return a lock which matches the given unlock lock
 *                           or NULL otehrwise
 */
struct file_lock*
get_lock_matching_unlock(const struct file_lock *fl)
{
	/*
	 * XXX: It is annoying that this duplicates so much code from
	 * test_nfslock
	 */
	struct file_lock *ifl; /* Iterator */
	struct file_lock *retval;

	debugdelay = 0;

	debuglog("Entering lock_matching_unlock\n");
	debuglog("********Dump of fl*****************\n");
	dump_filelock(fl);

	retval = NULL;

	for (ifl = LIST_FIRST(&nfslocklist_head);
	     ifl != NULL && retval == NULL;
	     ifl = LIST_NEXT(ifl, nfslocklist)) {
		debuglog("Pointer to file lock: %p\n",ifl);

		debuglog("****Dump of ifl****\n");
		dump_filelock(ifl);
		debuglog("*******************\n");

		/*
		 * XXX: It is conceivable that someone could use the NLM RPC
		 * system to directly access filehandles.  This may be a
		 * security hazard as the filehandle code may bypass normal
		 * file access controls
		 */
		if (!bcmp(&fl->filehandle, &ifl->filehandle,
			sizeof(fhandle_t))) {
			debuglog("matching_unlock: Filehandles match."
			    "  Checking regions\n");

			/* Filehandles match, check for region overlap */
			if (regions_overlap(fl->client.l_offset, fl->client.l_len,
				ifl->client.l_offset, ifl->client.l_len)) {
				debuglog("matching_unlock: Region overlap"
				    " found %llu : %llu -- %llu : %llu\n",
				    fl->client.l_offset,fl->client.l_len,
				    ifl->client.l_offset,ifl->client.l_len);

				/* Regions overlap, check the identity */
				if (same_filelock_identity(fl,ifl)) {
					debuglog("matching_unlock: Duplicate"
					    " lock id.  Granting\n");
					retval = ifl;
				}
			}
		}
	}

	debugdelay = 0;

	debuglog("Exiting lock_matching_unlock\n");

	return (retval);
}

/*
 * test_nfslock: check for NFS lock in lock list
 *
 * This routine makes the following assumptions:
 *    1) Nothing will adjust the lock list during a lookup
 *
 * This routine has an intersting quirk which bit me hard.
 * The conflicting_fl is the pointer to the conflicting lock.
 * However, to modify the "*pointer* to the conflicting lock" rather
 * that the "conflicting lock itself" one must pass in a "pointer to
 * the pointer of the conflicting lock".  Gross.
 */

enum nfslock_status
test_nfslock(const struct file_lock *fl, struct file_lock **conflicting_fl)
{
	struct file_lock *ifl; /* Iterator */
	enum nfslock_status retval;


	retval = NFS_GRANTED;
	(*conflicting_fl) = NULL;

	if (debug_level > 0) {
		debuglog("Entering test_nfslock\n");
		debuglog("Entering lock search loop\n");
	
		debuglog("***********************************\n");
		debuglog("Dumping match filelock\n");
		debuglog("***********************************\n");
		dump_filelock(fl);
		debuglog("***********************************\n");
	}

	for (ifl = LIST_FIRST(&nfslocklist_head);
	     ifl != NULL && retval != NFS_DENIED;
	     ifl = LIST_NEXT(ifl, nfslocklist)) {
		debuglog("Top of lock loop\n");
		debuglog("Pointer to file lock: %p\n",ifl);
		
		debuglog("***********************************\n");
		debuglog("Dumping test filelock\n");
		debuglog("***********************************\n");
		dump_filelock(ifl);
		debuglog("***********************************\n");

		/*
		 * XXX: It is conceivable that someone could use the NLM RPC
		 * system to directly access filehandles.  This may be a
		 * security hazard as the filehandle code may bypass normal
		 * file access controls
		 */
		if (!bcmp(&fl->filehandle, &ifl->filehandle,
			sizeof(fhandle_t))) {
			debuglog("test_nfslock: filehandle match found\n");

			/* Filehandles match, check for region overlap */
			if (regions_overlap(fl->client.l_offset, fl->client.l_len,
				ifl->client.l_offset, ifl->client.l_len)) {
				debuglog("test_nfslock: Region overlap found"
				    " %llu : %llu -- %llu : %llu\n",
				    fl->client.l_offset,fl->client.l_len,
				    ifl->client.l_offset,ifl->client.l_len);

				/* Regions overlap, check the exclusivity */
				if (fl->client.exclusive ||
				    ifl->client.exclusive) {
					debuglog("test_nfslock: "
					    "Exclusivity failure: %d %d\n",
					    fl->client.exclusive,
					    ifl->client.exclusive);

					if (same_filelock_identity(fl,ifl)) {
						debuglog("test_nfslock: "
						    "Duplicate lock id.  "
						    "Granting\n");
						(*conflicting_fl) = ifl;
						retval = NFS_GRANTED_DUPLICATE;
					} else {
						/* locking attempt fails */
						debuglog("test_nfslock: "
						    "Lock attempt failed\n");
						debuglog("Desired lock\n");
						dump_filelock(fl);
						debuglog("Conflicting lock\n");
						dump_filelock(ifl);
						(*conflicting_fl) = ifl;
						retval = NFS_DENIED;
					}
				}
			}
		}
	}
	
	debuglog("Dumping file locks\n");
	debuglog("Exiting test_nfslock\n");
	
	return retval;
}

/*
 * lock_nfslock: attempt to create a lock in the NFS lock list
 *
 * This routine tests whether the lock will be granted and then adds
 * the entry to the lock list if so.
 * 
 * Argument fl gets modified as its list housekeeping entries get modified
 * upon insertion into the NFS lock list
 *
 * This routine makes several assumptions:
 *    1) It is perfectly happy to grant a duplicate lock from the same pid.
 *       While this seems to be intuitively wrong, it is required for proper
 *       Posix semantics during unlock.  It is absolutely imperative to not
 *       unlock the main lock before the two child locks are established. Thus,
 *       one has be be able to create duplicate locks over an existing lock
 *    2) It currently accepts duplicate locks from the same id,pid
 */

enum nfslock_status
lock_nfslock(struct file_lock *fl)
{
	enum nfslock_status retval;
	struct file_lock *dummy_fl;

	dummy_fl = NULL;

	debuglog("Entering lock_nfslock...\n");

	retval = test_nfslock(fl,&dummy_fl);

	if (retval == NFS_GRANTED || retval == NFS_GRANTED_DUPLICATE) {
		debuglog("Inserting lock...\n");
		dump_filelock(fl);
		LIST_INSERT_HEAD(&nfslocklist_head, fl, nfslocklist);
	}

	debuglog("Exiting lock_nfslock...\n");

	return (retval);
}

/*
 * delete_nfslock: delete an NFS lock list entry
 *
 * This routine is used to delete a lock out of the NFS lock list
 * without regard to status, underlying locks, regions or anything else
 *
 * EXERCISE CAUTION USING THIS ROUTINE!  It should only be used when
 * you need to flush entries out of the NFS lock list (error conditions,
 * reboot recovery, etc.).  It can create huge memory and resource leaks
 * if used improperly.
 *
 * You really wanted unlock_nfslock, instead, didn't you?
 */

enum nfslock_status
delete_nfslock(const struct file_lock *fl)
{
	debuglog("delete_nfslock not yet implemented.\n");
}


enum split_status {SPL_DISJOINT, SPL_LOCK_CONTAINED, SPL_LOCK_LEFT,
		   SPL_LOCK_RIGHT, SPL_UNLOCK_CONTAINED};

enum split_status
split_nfslock(const struct file_lock *exist_lock,
    const struct file_lock *unlock_lock,
    const struct file_lock *left_lock, const struct file_lock *right_lock)
{
}

enum nfslock_status
unlock_nfslock(const struct file_lock *fl)
{
	struct file_lock *lfl,*rfl; /* Left and right locks if split occurs */
	struct file_lock *mfl; /* Matching file lock */
	enum nfslock_status retval;

	/* Allocate two locks up front or die trying */

	debuglog("Entering unlock_nfslock\n");

	retval = NFS_DENIED_NOLOCK;

	do {
		printf("Attempting to match lock...\n");
		mfl = get_lock_matching_unlock(fl);

		if (mfl == NULL) {
			/* No matching lock for unlock */
		} else {
			debuglog("Unlock matched\n");
			printf("Unlock matched\n");
			/* Unlock the lock if it matches identity */
			LIST_REMOVE(mfl, nfslocklist);
			deallocate_file_lock(mfl);
			retval = NFS_GRANTED;
		}
	} while (mfl != NULL);
	  
#if 0
	split_status = split_nfslock(mfl,fl,lfl,rfl);
	switch (split_status) {
	case SPL_DISJOINT:
		/* Shouldn't happen, throw error */
	case SPL_LOCK_CONTAINED:
		/* Delete entire lock */
	case SPL_LOCK_LEFT:
		/* Create new lock for left lock and delete old one */
	case SPL_LOCK_RIGHT:
		/* Create new lock for right lock and delete old one */
	case SPL_UNLOCK_CONTAINED:
	    /* Create new locks for both and then delete old one */
	}
#endif

	debuglog("Exiting unlock_nfslock\n");

	return retval;
}


/*
 * Below here are routines associated with manipulating all
 * aspects of the partial file locking system (list, hardware, etc.)
 */

/*
 * lock_partialfilelock:
 *
 * Argument fl gets modified as its list housekeeping entries get modified
 * upon insertion into the NFS lock list
 *
 * This routine makes several assumptions:
 *    1) It (will) pass locks through to fcntl to lock the underlying file
 *           fcntl has some absolutely brain damaged unlocking semantics which
 *               may cause trouble with NFS subsystem.  The primary problem
 *               is that fcntl drops *ALL* locks held by a process when it
 *               executes a close.  This means that NFS may indicate a range as
 *               locked but the underlying file on hardware may not have that
 *               lock anymore.
 *           The safe solution may be for any NFS locks to cause an flock
 *               of the entire underlying file by the lockd, parcel out
 *               the partitions manually, and then to only
 *               release the underlying file when all NFS locks are finished.
 *    2) Nothing modifies the lock lists between testing and granting
 *           I have no idea whether this is a useful guarantee of not
 *
 */

enum partialfilelock_status
lock_partialfilelock(struct file_lock *fl)
{
	enum partialfilelock_status retval;
	enum nfslock_status lnlstatus;

	debuglog("Entering lock_partialfilelock\n");

	retval = PFL_DENIED;

	/*
	 * Execute the NFS lock first, if possible, as it is significantly
	 * easier and less expensive to undo than the filesystem lock
	 */

	lnlstatus = lock_nfslock(fl);

	if (lnlstatus == NFS_GRANTED || lnlstatus == NFS_GRANTED_DUPLICATE) {
		/* XXX: Add the underlying filesystem locking code */
		retval = (lnlstatus == NFS_GRANTED ?
		    PFL_GRANTED : PFL_GRANTED_DUPLICATE);
		debuglog("NFS lock granted\n");
#if 0
		if (do_rawlock(fl) == RL_GRANTED) {
			;
		} else {
			/* XXX: Unwind the NFS lock which was just granted */
			do_nfsdeletelock(fl);
			retval = PFL_RAWDENIED;
		}
#endif
	} else {
		retval = PFL_NFSDENIED;
		debuglog("NFS lock denied\n");
		dump_filelock(fl);
	}

	debuglog("Exiting lock_partialfilelock\n");

	return retval;
}

/*
 * unlock_partialfilelock:
 *
 */

enum partialfilelock_status
unlock_partialfilelock(const struct file_lock *fl)
{
	enum partialfilelock_status retval;
	int unlstatus;

	debuglog("Entering unlock_partialfilelock\n");

	retval = PFL_DENIED;

	unlstatus = unlock_nfslock(fl);
	
	if (unlstatus == NFS_GRANTED) {
		retval = PFL_GRANTED;
	} else if (unlstatus == NFS_DENIED_NOLOCK) {
		retval = PFL_NFSDENIED_NOLOCK;
	} else {
		retval = PFL_NFSDENIED;
		debuglog("NFS unlock denied\n");
		dump_filelock(fl);
	}

	debuglog("Exiting unlock_partialfilelock\n");

	return retval;
}

/*
 * test_partialfilelock:
 */
enum partialfilelock_status
test_partialfilelock(const struct file_lock *fl,
    struct file_lock **conflicting_fl)
{
	enum partialfilelock_status retval;
	enum nfslock_status teststatus;

	debuglog("Entering testpartialfilelock...\n");

	retval = PFL_DENIED;

	teststatus = test_nfslock(fl, conflicting_fl);
	debuglog("test_partialfilelock: teststatus %d\n",teststatus);

	if (teststatus == NFS_GRANTED || teststatus == NFS_GRANTED_DUPLICATE) {
		/* XXX: Add the underlying filesystem locking code */
		retval = (teststatus == NFS_GRANTED) ?
		    PFL_GRANTED : PFL_GRANTED_DUPLICATE;
		debuglog("Dumping locks...\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		debuglog("Done dumping locks...\n");
	} else {
		retval = PFL_NFSDENIED;
		debuglog("NFS test denied.\n");
		dump_filelock(fl);
		debuglog("Conflicting.\n");
		dump_filelock(*conflicting_fl);
	}

	debuglog("Exiting testpartialfilelock...\n");

	return retval;
}


/*
 * Below here are routines associated with translating the partial file locking
 * codes into useful codes to send back to the NFS RPC messaging system
 */

enum nlm_stats
do_test(struct file_lock *fl, struct file_lock **conflicting_fl)
{
	enum partialfilelock_status pfsret;
	enum nlm_stats retval;

	debuglog("Entering do_test...\n");

	pfsret = test_partialfilelock(fl,conflicting_fl);

	if (pfsret == PFL_GRANTED) {
		debuglog("PFL test lock granted");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else if (pfsret == PFL_GRANTED_DUPLICATE) {
		debuglog("PFL test lock granted--duplicate id detected");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		debuglog("Clearing conflicting_fl for call semantics\n");
		*conflicting_fl = NULL;
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else if (pfsret == PFL_NFSDENIED) {
		debuglog("PFL_NFS test lock denied");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
	} else {
		debuglog("PFL test lock *FAILED*");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}
	
	debuglog("Exiting do_test...\n");

	return retval;
}

/*
 * do_lock: Try to acquire a lock
 *
 * This routine makes a distinction between NLM versions.  I am pretty
 * convinced that this should be abstracted out and bounced up a level
 */

enum nlm_stats
do_lock(struct file_lock *fl)
{
	enum partialfilelock_status pfsret;
	enum nlm_stats retval;

	debuglog("Entering do_lock...\n");

	pfsret = lock_partialfilelock(fl);
	
	if (pfsret == PFL_GRANTED) {
		debuglog("PFL lock granted");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else if (pfsret == PFL_GRANTED_DUPLICATE) {
		debuglog("PFL lock granted--duplicate id detected");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else if (pfsret == PFL_NFSDENIED) {
		debuglog("PFL_NFS lock denied");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
	} else {
		debuglog("PFL lock *FAILED*");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	debuglog("Exiting do_lock...\n");

	return retval;
}

enum nlm_stats
do_unlock(struct file_lock *fl)
{
	enum partialfilelock_status pfsret;
	enum nlm_stats retval;

	debuglog("Entering do_unlock...\n");
	pfsret = unlock_partialfilelock(fl);

	if (pfsret == PFL_GRANTED) {
		debuglog("PFL unlock granted");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else if (pfsret == PFL_NFSDENIED) {
		debuglog("PFL_NFS unlock denied");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
	} else if (pfsret == PFL_NFSDENIED_NOLOCK) {
		debuglog("PFL_NFS no lock found\n");
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	} else {
		debuglog("PFL unlock *FAILED*");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	/* XXX: Since something has unlocked, recheck the blocked lock queue */
	/* update_blocked_queue(); */

	debuglog("Exiting do_unlock...\n");

	return retval;
}

/*
 * The following routines are all called from the code which the
 * RPC layer invokes
 */

/*
 * testlock(): inform the caller if the requested lock would be granted
 *
 * returns NULL if lock would granted
 * returns pointer to a conflicting nlm4_holder if not
 */

struct nlm4_holder *
testlock(struct nlm4_lock *lock, bool_t exclusive, int flags)
{
	struct file_lock test_fl, *conflicting_fl;

	bzero(&test_fl, sizeof(test_fl));

	bcopy(lock->fh.n_bytes, &(test_fl.filehandle), sizeof(fhandle_t));
	copy_nlm4_lock_to_nlm4_holder(lock, exclusive, &test_fl.client);

	siglock();
	debugdelay = 0;
	do_test(&test_fl, &conflicting_fl);
	debugdelay = 0;
	
	if (conflicting_fl == NULL) {
		debuglog("No conflicting lock found\n");
		sigunlock();
		return NULL;
	} else {
		debuglog("Found conflicting lock\n");
		dump_filelock(conflicting_fl);
		sigunlock();
		return (&conflicting_fl->client);
	}
}

/*
 * getlock: try to aquire the lock. 
 * If file is already locked and we can sleep, put the lock in the list with
 * status LKST_WAITING; it'll be processed later.
 * Otherwise try to lock. If we're allowed to block, fork a child which
 * will do the blocking lock.
 */
enum nlm_stats
getlock(nlm4_lockargs *lckarg, struct svc_req *rqstp, const int flags)
{
	struct file_lock *newfl;
	enum nlm_stats retval;

	debuglog("Entering getlock...\n");

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
		debuglog("recieved fhandle size %d, local size %d",
		    lckarg->alock.fh.n_len, (int)sizeof(fhandle_t));
	}

	bcopy(lckarg->alock.fh.n_bytes,&newfl->filehandle, sizeof(fhandle_t));
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
	bcopy(lckarg->alock.oh.n_bytes, newfl->client.oh.n_bytes,
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

	bcopy(lckarg->cookie.n_bytes, newfl->client_cookie.n_bytes,
	    lckarg->cookie.n_len);
	strncpy(newfl->client_name, lckarg->alock.caller_name, SM_MAXSTRLEN);
	newfl->nsm_status = lckarg->state;
	newfl->status = 0;
	newfl->flags = flags;
	
	/*
	 * newfl is now fully constructed and deallocate_file_lock
	 * can now be used to delete it
	 * The *only* place which should deallocate a file_lock is
	 * either here (error condition) or the unlock code.
	 */
	
	siglock();
	debuglog("Pointer to new lock is %p\n",newfl);
	retval = do_lock(newfl);
	debuglog("Pointer to new lock is %p\n",newfl);
	sigunlock();
	
	switch (retval)
		{
		case nlm4_granted:
			/* case nlm_granted: is the same as nlm4_granted */
			/* do_mon(lckarg->alock.caller_name); */
			break;
		case nlm4_blocked:
			/* case nlm_blocked: is the same as nlm4_blocked */
			/* do_mon(lckarg->alock.caller_name); */
			break;
		default:
			deallocate_file_lock(newfl);
			break;
		}

	debuglog("Exiting getlock...\n");

	return (retval);
}


/* unlock a filehandle */
enum nlm_stats
unlock(nlm4_lock *lock, const int flags)
{
	struct file_lock fl;
	enum nlm_stats err;
	
	siglock();
	debugdelay = 0;
	
	debuglog("Entering unlock...\n");
	
	bzero(&fl,sizeof(struct file_lock));
	bcopy(lock->fh.n_bytes, &fl.filehandle, sizeof(fhandle_t));
	
	copy_nlm4_lock_to_nlm4_holder(lock, 0, &fl.client);
	
	err = do_unlock(&fl);
	
	debugdelay = 0;
	sigunlock();
	
	debuglog("Exiting unlock...\n");
	
	return err;
}

/*
 * XXX: The following monitor/unmonitor routines 
 * HAVE NOT BEEN TESTED!!!  THEY ARE GUARANTEED TO
 * BE WRONG!!! At the very least, there are pointers
 * to characters being thrown around when they should be
 * strings in the statd structures.  Consequently, the
 * statd calling code has been commented out. Please note
 * that this problem was also in the original do_mon routine
 * which this pair replaced (and there was no do_unmon either)
 */

/*
 * monitor_lock_host: monitor lock hosts locally with a ref count and
 * inform statd
 *
 * XXX: Are the strnXXX functions the correct choices?
 */
void
monitor_lock_host(const char *hostname)
{
	struct host *ihp, *nhp;
	struct mon smon;
	struct sm_stat_res sres;
	int rpcret, statflag;
	
	rpcret = 0;
	statflag = 0;

	LIST_FOREACH(ihp, &hostlst_head, hostlst) {
		if (strncmp(hostname, ihp->name, SM_MAXSTRLEN) == 0) {
			/* Host is already monitored, bump refcount */
			++ihp->refcnt;
			/* Host should only be in the monitor list once */
			break;
		}
	}

	if (ihp != NULL)
		return;

	/* Host is not yet monitored, add it */
	nhp = malloc(sizeof(struct host));

	if (nhp == NULL) {
		debuglog("Unable to allocate entry for statd mon\n");
		return;
	}

	/* Allocated new host entry, now fill the fields */
	strncpy(nhp->name, hostname, SM_MAXSTRLEN);
	nhp->refcnt = 1;
	debuglog("Locally Monitoring host %16s\n",hostname);
	debuglog("Attempting to tell statd\n");
	bzero(&smon,sizeof(struct mon));
#if 0
	smon.mon_id.mon_name = nhp->name;
	smon.mon_id.my_id.my_name = "localhost";
	smon.mon_id.my_id.my_prog = NLM_PROG;
	smon.mon_id.my_id.my_vers = NLM_SM;
	smon.mon_id.my_id.my_proc = NLM_SM_NOTIFY;
  
	rpcret = callrpc("localhost", SM_PROG, SM_VERS, SM_MON,
	    xdr_mon, &smon, xdr_sm_stat_res, &sres);
			  
	if (rpcret == 0) {
		if (sres.res_stat == stat_fail) {
			debuglog("Statd call failed\n");
			statflag = 0;
		} else {
			statflag = 1;
		}
	} else {
		debuglog("Rpc call to statd failed with return value: %d\n",
		    rpcret);
		statflag = 0;
	}
#endif 0

	/* XXX: remove this when statd code is fixed */
	statflag = 1;
	if (statflag == 1) {
		LIST_INSERT_HEAD(&hostlst_head, nhp, hostlst);
	} else {
		free(nhp);
	}
}

/*
 * unmonitor_lock_host: clear monitor ref counts and inform statd when gone
 */
void
unmonitor_lock_host(const char *hostname)
{
	struct host *ihp, *nhp;
	struct mon_id smon_id;
	struct sm_stat smstat;
	int rpcret;
	
	rpcret = 0;

	LIST_FOREACH(ihp, &hostlst_head, hostlst) {
		if (strncmp(hostname, ihp->name, SM_MAXSTRLEN) == 0) {
			/* Host is monitored, bump refcount */
			--ihp->refcnt;
			/* Host should only be in the monitor list once */
			break;
		}
	}

	if (ihp == NULL) {
		debuglog("Could not find host %16s in mon list\n", hostname);
		return;
	}

	if (ihp->refcnt < 0)
		debuglog("Negative refcount!: %d\n", ihp->refcnt);

	if (ihp->refcnt > 0)
		return;

	debuglog("Attempting to unmonitor host %16s\n", hostname);
	bzero(&smon_id,sizeof(smon_id));
#if 0
	smon_id.mon_name = hostname;
	smon_id.my_id.my_name = "localhost";
	smon_id.my_id.my_prog = NLM_PROG;
	smon_id.my_id.my_vers = NLM_SM;
	smon_id.my_id.my_proc = NLM_SM_NOTIFY;
	rpcret = callrpc("localhost", SM_PROG, SM_VERS, SM_UNMON,
	    xdr_mon, &smon_id, xdr_sm_stat_res, &smstat);
	if (rpcret != 0)
		debuglog("Rpc call to unmonitor statd failed with "
		    "return value: %d\n",rpcret);
#endif 0
	LIST_REMOVE(ihp, hostlst);
	free(ihp);
}

/*
 * notify: Clear all locks from a host if statd complains
 *
 * CAUTION: This routine has probably not been thoroughly tested even in
 * its original incarnation.  The proof is the fact that it only tests
 * for nlm_granted on do_unlock rather than inlcluding nlm4_granted.
 * 
 * Consequently, it has been commented out until it has.
 */

void
notify(const char *hostname, const int state)
{
	struct file_lock *fl, *next_fl;
	int err;
	debuglog("notify from %s, new state %d", hostname, state);
	
	debuglog("****************************\n");
	debuglog("No action taken in notify!!!\n");
	debuglog("****************************\n");

	/* search all lock for this host; if status changed, release the lock */
#if 0
	siglock();
	for (fl = LIST_FIRST(&nfslocklist_head); fl != NULL; fl = next_fl) {
		next_fl = LIST_NEXT(fl, nfslocklist);
		if (strcmp(hostname, fl->client_name) == 0 &&
		    fl->nsm_status != state) {
			debuglog("state %d, nsm_state %d, unlocking",
				 fl->status, fl->nsm_status);
			switch(fl->status) {
			case LKST_LOCKED:
				err = do_unlock(fl);
				if (err != nlm_granted)
				  debuglog("notify: unlock failed for %s (%d)",
					   hostname, err);
				break;
			case LKST_WAITING:
				LIST_REMOVE(fl, nfslocklist);
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
#endif
}

/*
 * Routines below here have not been modified in the overhaul
 */

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
		debuglog("sending v4 reply%s",
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
		debuglog("sending v1 reply%s",
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

/*
 * Are these two routines (siglock/sigunlock) still required since lockd
 * is not spawning off children to service locks anymore?
 * Presumably they were originally put in place to prevent a child's exit
 * from corrupting the lock list so that locks manipulation could be done
 * in the context of the signal handler.
 */
void
siglock(void)
{
	sigset_t block;
	
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);

	if (sigprocmask(SIG_BLOCK, &block, NULL) == -1) {
		syslog(LOG_WARNING, "siglock failed: %s", strerror(errno));
	}
}

void
sigunlock(void)
{
	sigset_t block;
	
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);

	if (sigprocmask(SIG_UNBLOCK, &block, NULL) == -1) {
		syslog(LOG_WARNING, "sigunlock failed: %s", strerror(errno));
	}
}
