/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumio.h,v 1.19 2000/04/26 04:17:33 grog Exp grog $
 * $FreeBSD$
 */

#define L 'F'						    /* ID letter of our ioctls */

#ifdef VINUMDEBUG
#define MAX_IOCTL_REPLY 1024
#else
#define MAX_IOCTL_REPLY 256
#endif

#ifdef VINUMDEBUG
struct debuginfo {
    int changeit;
    int param;
};

#endif

enum objecttype {
    drive_object,
    sd_object,
    plex_object,
    volume_object,
    invalid_object
};

/*
 * The state to set with VINUM_SETSTATE.  Since each object has a
 * different set of states, we need to translate later.
 */
enum objectstate {
    object_down,
    object_initializing,
    object_initialized,
    object_up
};

/*
 * This structure is used for modifying objects
 * (VINUM_SETSTATE, VINUM_REMOVE, VINUM_RESETSTATS, VINUM_ATTACH,
 * VINUM_DETACH, VINUM_REPLACE
 */
struct vinum_ioctl_msg {
    int index;
    enum objecttype type;
    enum objectstate state;				    /* state to set (VINUM_SETSTATE) */
    enum parityop op;					    /* for parity ops */
    int force;						    /* do it even if it doesn't make sense */
    int recurse;					    /* recurse (VINUM_REMOVE) */
    int verify;						    /* verify (initsd, rebuildparity) */
    int otherobject;					    /* superordinate object (attach),
							    * replacement object (replace) */
    int rename;						    /* rename object (attach) */
    int64_t offset;					    /* offset of subdisk (for attach) */
    int blocksize;					    /* size of block to revive (bytes) */
};

/* VINUM_CREATE returns a buffer of this kind */
struct _ioctl_reply {
    int error;
    char msg[MAX_IOCTL_REPLY];
};

struct vinum_rename_msg {
    int index;
    int recurse;					    /* rename subordinate objects too */
    enum objecttype type;
    char newname[MAXNAME];				    /* new name to give to object */
};

/* ioctl requests */
#define BUFSIZE 1024					    /* size of buffer, including continuations */
#define VINUM_CREATE		_IOC(IOC_IN | IOC_OUT, L, 64, BUFSIZE) /* configure vinum */
#define VINUM_GETCONFIG		_IOR(L, 65, struct __vinum_conf) /* get global config */
#define VINUM_DRIVECONFIG	_IOWR(L, 66, struct _drive) /* get drive config */
#define VINUM_SDCONFIG		_IOWR(L, 67, struct _sd)    /* get subdisk config */
#define VINUM_PLEXCONFIG	_IOWR(L, 68, struct _plex)  /* get plex config */
#define VINUM_VOLCONFIG		_IOWR(L, 69, struct _volume) /* get volume config */
#define VINUM_PLEXSDCONFIG	_IOWR(L, 70, struct _sd)    /* get sd config for plex (plex, sdno) */
#define VINUM_GETFREELIST	_IOWR(L, 71, struct drive_freelist) /* get freelist element (drive, fe) */
#define VINUM_SAVECONFIG	_IOW(L, 72, int)	    /* write config to disk */
#define VINUM_RESETCONFIG	_IOC(0, L, 73, 0)	    /* trash config on disk */
#define VINUM_INIT		_IOC(0, L, 74, 0)	    /* read config from disk */
#ifdef VINUMDEBUG
#define VINUM_DEBUG		_IOWR(L, 75, struct debuginfo) /* call the debugger from ioctl () */
#endif

/*
 * Start an object.  Pass two integers:
 * msg [0] index in vinum_conf.<object>
 * msg [1] type of object (see below)
 *
 * Return ioctl_reply
 */
#define VINUM_SETSTATE 		_IOC(IOC_IN | IOC_OUT, L, 76, MAX_IOCTL_REPLY) /* start an object */
#define VINUM_RELEASECONFIG	_IOC(0, L, 77, 0)	    /* release locks and write config to disk */
#define VINUM_STARTCONFIG	_IOW(L, 78, int)	    /* start a configuration operation */
#define VINUM_MEMINFO 		_IOR(L, 79, struct meminfo) /* get memory usage summary */
#define VINUM_MALLOCINFO	_IOWR(L, 80, struct mc)	    /* get specific malloc information [i] */
#define VINUM_LABEL 		_IOC(IOC_IN | IOC_OUT, L, 81, MAX_IOCTL_REPLY) /* label a volume */
#define VINUM_INITSD 		_IOW(L, 82, int)	    /* initialize a subdisk */
#define VINUM_REMOVE 		_IOWR(L, 83, struct _ioctl_reply) /* remove an object */
#define VINUM_READPOL 		_IOWR(L, 84, struct _ioctl_reply) /* set read policy */
#define VINUM_SETSTATE_FORCE	_IOC(IOC_IN | IOC_OUT, L, 85, MAX_IOCTL_REPLY) /* diddle object state */
#define VINUM_RESETSTATS	_IOWR(L, 86, struct _ioctl_reply) /* reset object stats */
#define VINUM_ATTACH		_IOWR(L, 87, struct _ioctl_reply) /* attach an object */
#define VINUM_DETACH		_IOWR(L, 88, struct _ioctl_reply) /* remove an object */

#define VINUM_RENAME		_IOWR(L, 89, struct _ioctl_reply) /* rename an object */
#define VINUM_REPLACE		_IOWR(L, 90, struct _ioctl_reply) /* replace an object */

#ifdef VINUMDEBUG
#define VINUM_RQINFO		_IOWR(L, 91, struct rqinfo) /* get request info [i] from trace buffer */
#endif

#define VINUM_DAEMON		_IOC(0, L, 92, 0)	    /* perform the kernel part of Vinum daemon */
#define VINUM_FINDDAEMON	_IOC(0, L, 93, 0)	    /* check for presence of Vinum daemon */
#define VINUM_SETDAEMON		_IOW(L, 94, int)	    /* set daemon flags */
#define VINUM_GETDAEMON		_IOR(L, 95, int)	    /* get daemon flags */
#define VINUM_PARITYOP   	_IOWR(L, 96, struct _ioctl_reply) /* check/rebuild RAID-4/5 parity */
#define VINUM_MOVE		_IOWR(L, 98, struct _ioctl_reply) /* move an object */
