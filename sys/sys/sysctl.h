/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)sysctl.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _SYS_SYSCTL_H_
#define	_SYS_SYSCTL_H_

#include <sys/queue.h>

struct thread;
/*
 * Definitions for sysctl call.  The sysctl call uses a hierarchical name
 * for objects that can be examined or modified.  The name is expressed as
 * a sequence of integers.  Like a file path name, the meaning of each
 * component depends on its place in the hierarchy.  The top-level and kern
 * identifiers are defined here, and other identifiers are defined in the
 * respective subsystem header files.
 */

#define CTL_MAXNAME	24	/* largest number of components supported */

/*
 * Each subsystem defined by sysctl defines a list of variables
 * for that subsystem. Each name is either a node with further
 * levels defined below it, or it is a leaf of some particular
 * type given below. Each sysctl level defines a set of name/type
 * pairs to be used by sysctl(8) in manipulating the subsystem.
 */
struct ctlname {
	char	*ctl_name;	/* subsystem name */
	int	ctl_type;	/* type of name */
};

#define CTLTYPE		0xf	/* Mask for the type */
#define	CTLTYPE_NODE	1	/* name is a node */
#define	CTLTYPE_INT	2	/* name describes an integer */
#define	CTLTYPE_STRING	3	/* name describes a string */
#define	CTLTYPE_QUAD	4	/* name describes a 64-bit number */
#define	CTLTYPE_OPAQUE	5	/* name describes a structure */
#define	CTLTYPE_STRUCT	CTLTYPE_OPAQUE	/* name describes a structure */
#define	CTLTYPE_UINT	6	/* name describes an unsigned integer */
#define	CTLTYPE_LONG	7	/* name describes a long */
#define	CTLTYPE_ULONG	8	/* name describes an unsigned long */

#define CTLFLAG_RD	0x80000000	/* Allow reads of variable */
#define CTLFLAG_WR	0x40000000	/* Allow writes to the variable */
#define CTLFLAG_RW	(CTLFLAG_RD|CTLFLAG_WR)
#define CTLFLAG_NOLOCK	0x20000000	/* XXX Don't Lock */
#define CTLFLAG_ANYBODY	0x10000000	/* All users can set this var */
#define CTLFLAG_SECURE	0x08000000	/* Permit set only if securelevel<=0 */
#define CTLFLAG_PRISON	0x04000000	/* Prisoned roots can fiddle */
#define CTLFLAG_DYN	0x02000000	/* Dynamic oid - can be freed */
#define CTLFLAG_SKIP	0x01000000	/* Skip this sysctl when listing */
#define CTLMASK_SECURE	0x00F00000	/* Secure level */
#define CTLFLAG_TUN	0x00080000	/* Tunable variable */
#define CTLFLAG_RDTUN	(CTLFLAG_RD|CTLFLAG_TUN)

/*
 * Secure level.   Note that CTLFLAG_SECURE == CTLFLAG_SECURE1.  
 *
 * Secure when the securelevel is raised to at least N.
 */
#define CTLSHIFT_SECURE	20
#define CTLFLAG_SECURE1	(CTLFLAG_SECURE | (0 << CTLSHIFT_SECURE))
#define CTLFLAG_SECURE2	(CTLFLAG_SECURE | (1 << CTLSHIFT_SECURE))
#define CTLFLAG_SECURE3	(CTLFLAG_SECURE | (2 << CTLSHIFT_SECURE))

/*
 * USE THIS instead of a hardwired number from the categories below
 * to get dynamically assigned sysctl entries using the linker-set
 * technology. This is the way nearly all new sysctl variables should
 * be implemented.
 * e.g. SYSCTL_INT(_parent, OID_AUTO, name, CTLFLAG_RW, &variable, 0, "");
 */ 
#define OID_AUTO	(-1)

/*
 * The starting number for dynamically-assigned entries.  WARNING!
 * ALL static sysctl entries should have numbers LESS than this!
 */
#define CTL_AUTO_START	0x100

#ifdef _KERNEL
#define SYSCTL_HANDLER_ARGS struct sysctl_oid *oidp, void *arg1, int arg2, \
	struct sysctl_req *req

/* definitions for sysctl_req 'lock' member */
#define REQ_UNLOCKED	0	/* not locked and not wired */
#define REQ_LOCKED	1	/* locked and not wired */
#define REQ_WIRED	2	/* locked and wired */

/* definitions for sysctl_req 'flags' member */
#if defined(__amd64__) || defined(__ia64__)
#define	SCTL_MASK32	1	/* 32 bit emulation */
#endif

/*
 * This describes the access space for a sysctl request.  This is needed
 * so that we can use the interface from the kernel or from user-space.
 */
struct sysctl_req {
	struct thread	*td;		/* used for access checking */
	int		lock;		/* locking/wiring state */
	void		*oldptr;
	size_t		oldlen;
	size_t		oldidx;
	int		(*oldfunc)(struct sysctl_req *, const void *, size_t);
	void		*newptr;
	size_t		newlen;
	size_t		newidx;
	int		(*newfunc)(struct sysctl_req *, void *, size_t);
	size_t		validlen;
	int		flags;
};

SLIST_HEAD(sysctl_oid_list, sysctl_oid);

/*
 * This describes one "oid" in the MIB tree.  Potentially more nodes can
 * be hidden behind it, expanded by the handler.
 */
struct sysctl_oid {
	struct sysctl_oid_list *oid_parent;
	SLIST_ENTRY(sysctl_oid) oid_link;
	int		oid_number;
	u_int		oid_kind;
	void		*oid_arg1;
	int		oid_arg2;
	const char	*oid_name;
	int 		(*oid_handler)(SYSCTL_HANDLER_ARGS);
	const char	*oid_fmt;
	int		oid_refcnt;
	const char	*oid_descr;
};

#define SYSCTL_IN(r, p, l) (r->newfunc)(r, p, l)
#define SYSCTL_OUT(r, p, l) (r->oldfunc)(r, p, l)

int sysctl_handle_int(SYSCTL_HANDLER_ARGS);
int sysctl_msec_to_ticks(SYSCTL_HANDLER_ARGS);
int sysctl_handle_long(SYSCTL_HANDLER_ARGS);
int sysctl_handle_quad(SYSCTL_HANDLER_ARGS);
int sysctl_handle_intptr(SYSCTL_HANDLER_ARGS);
int sysctl_handle_string(SYSCTL_HANDLER_ARGS);
int sysctl_handle_opaque(SYSCTL_HANDLER_ARGS);

/*
 * These functions are used to add/remove an oid from the mib.
 */
void sysctl_register_oid(struct sysctl_oid *oidp);
void sysctl_unregister_oid(struct sysctl_oid *oidp);

/* Declare a static oid to allow child oids to be added to it. */
#define SYSCTL_DECL(name)					\
	extern struct sysctl_oid_list sysctl_##name##_children

/* Hide these in macros */
#define	SYSCTL_CHILDREN(oid_ptr) (struct sysctl_oid_list *) \
	(oid_ptr)->oid_arg1
#define	SYSCTL_CHILDREN_SET(oid_ptr, val) \
	(oid_ptr)->oid_arg1 = (val);
#define	SYSCTL_STATIC_CHILDREN(oid_name) \
	(&sysctl_##oid_name##_children)

/* === Structs and macros related to context handling === */

/* All dynamically created sysctls can be tracked in a context list. */
struct sysctl_ctx_entry {
	struct sysctl_oid *entry;
	TAILQ_ENTRY(sysctl_ctx_entry) link;
};

TAILQ_HEAD(sysctl_ctx_list, sysctl_ctx_entry);

#define SYSCTL_NODE_CHILDREN(parent, name) \
	sysctl_##parent##_##name##_children

#ifndef NO_SYSCTL_DESCR
#define __DESCR(d) d
#else
#define __DESCR(d) ""
#endif

/* This constructs a "raw" MIB oid. */
#define SYSCTL_OID(parent, nbr, name, kind, a1, a2, handler, fmt, descr) \
	static struct sysctl_oid sysctl__##parent##_##name = {		 \
		&sysctl_##parent##_children, { 0 },			 \
		nbr, kind, a1, a2, #name, handler, fmt, 0, __DESCR(descr) }; \
	DATA_SET(sysctl_set, sysctl__##parent##_##name)

#define SYSCTL_ADD_OID(ctx, parent, nbr, name, kind, a1, a2, handler, fmt, descr) \
	sysctl_add_oid(ctx, parent, nbr, name, kind, a1, a2, handler, fmt, __DESCR(descr))

/* This constructs a node from which other oids can hang. */
#define SYSCTL_NODE(parent, nbr, name, access, handler, descr)		    \
	struct sysctl_oid_list SYSCTL_NODE_CHILDREN(parent, name);	    \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_NODE|(access),		    \
		   (void*)&SYSCTL_NODE_CHILDREN(parent, name), 0, handler, \
		   "N", descr)

#define SYSCTL_ADD_NODE(ctx, parent, nbr, name, access, handler, descr)	    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_NODE|(access),	    \
	0, 0, handler, "N", __DESCR(descr))

/* Oid for a string.  len can be 0 to indicate '\0' termination. */
#define SYSCTL_STRING(parent, nbr, name, access, arg, len, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_STRING|(access), \
		arg, len, sysctl_handle_string, "A", descr)

#define SYSCTL_ADD_STRING(ctx, parent, nbr, name, access, arg, len, descr)  \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_STRING|(access),	    \
	arg, len, sysctl_handle_string, "A", __DESCR(descr))

/* Oid for an int.  If ptr is NULL, val is returned. */
#define SYSCTL_INT(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_INT|(access), \
		ptr, val, sysctl_handle_int, "I", descr)

#define SYSCTL_ADD_INT(ctx, parent, nbr, name, access, ptr, val, descr)	    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_INT|(access),	    \
	ptr, val, sysctl_handle_int, "I", __DESCR(descr))

/* Oid for an unsigned int.  If ptr is NULL, val is returned. */
#define SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_UINT|(access), \
		ptr, val, sysctl_handle_int, "IU", descr)

#define SYSCTL_ADD_UINT(ctx, parent, nbr, name, access, ptr, val, descr)    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_UINT|(access),	    \
	ptr, val, sysctl_handle_int, "IU", __DESCR(descr))

#define SYSCTL_XINT(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_UINT|(access), \
		ptr, val, sysctl_handle_int, "IX", descr)

#define SYSCTL_ADD_XINT(ctx, parent, nbr, name, access, ptr, val, descr)    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_UINT|(access),	    \
	ptr, val, sysctl_handle_int, "IX", __DESCR(descr))

/* Oid for a long.  The pointer must be non NULL. */
#define SYSCTL_LONG(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_LONG|(access), \
		ptr, val, sysctl_handle_long, "L", descr)

#define SYSCTL_ADD_LONG(ctx, parent, nbr, name, access, ptr, descr)	    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_LONG|(access),	    \
	ptr, 0, sysctl_handle_long, "L", __DESCR(descr))

/* Oid for an unsigned long.  The pointer must be non NULL. */
#define SYSCTL_ULONG(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_ULONG|(access), \
		ptr, val, sysctl_handle_long, "LU", __DESCR(descr))

#define SYSCTL_ADD_ULONG(ctx, parent, nbr, name, access, ptr, descr)	    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_ULONG|(access),	    \
	ptr, 0, sysctl_handle_long, "LU", __DESCR(descr))

#define SYSCTL_XLONG(parent, nbr, name, access, ptr, val, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_ULONG|(access), \
		ptr, val, sysctl_handle_long, "LX", __DESCR(descr))

#define SYSCTL_ADD_XLONG(ctx, parent, nbr, name, access, ptr, descr)	    \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_ULONG|(access),	    \
	ptr, 0, sysctl_handle_long, "LX", __DESCR(descr))

/* Oid for an opaque object.  Specified by a pointer and a length. */
#define SYSCTL_OPAQUE(parent, nbr, name, access, ptr, len, fmt, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_OPAQUE|(access), \
		ptr, len, sysctl_handle_opaque, fmt, descr)

#define SYSCTL_ADD_OPAQUE(ctx, parent, nbr, name, access, ptr, len, fmt, descr)\
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_OPAQUE|(access),	    \
	ptr, len, sysctl_handle_opaque, fmt, __DESCR(descr))

/* Oid for a struct.  Specified by a pointer and a type. */
#define SYSCTL_STRUCT(parent, nbr, name, access, ptr, type, descr) \
	SYSCTL_OID(parent, nbr, name, CTLTYPE_OPAQUE|(access), \
		ptr, sizeof(struct type), sysctl_handle_opaque, \
		"S," #type, descr)

#define SYSCTL_ADD_STRUCT(ctx, parent, nbr, name, access, ptr, type, descr) \
	sysctl_add_oid(ctx, parent, nbr, name, CTLTYPE_OPAQUE|(access),	    \
	ptr, sizeof(struct type), sysctl_handle_opaque, "S," #type, __DESCR(descr))

/* Oid for a procedure.  Specified by a pointer and an arg. */
#define SYSCTL_PROC(parent, nbr, name, access, ptr, arg, handler, fmt, descr) \
	SYSCTL_OID(parent, nbr, name, (access), \
		ptr, arg, handler, fmt, descr)

#define SYSCTL_ADD_PROC(ctx, parent, nbr, name, access, ptr, arg, handler, fmt, descr) \
	sysctl_add_oid(ctx, parent, nbr, name, (access),			    \
	ptr, arg, handler, fmt, __DESCR(descr))

#endif /* _KERNEL */

/*
 * Top-level identifiers
 */
#define	CTL_UNSPEC	0		/* unused */
#define	CTL_KERN	1		/* "high kernel": proc, limits */
#define	CTL_VM		2		/* virtual memory */
#define	CTL_VFS		3		/* filesystem, mount type is next */
#define	CTL_NET		4		/* network, see socket.h */
#define	CTL_DEBUG	5		/* debugging parameters */
#define	CTL_HW		6		/* generic cpu/io */
#define	CTL_MACHDEP	7		/* machine dependent */
#define	CTL_USER	8		/* user-level */
#define	CTL_P1003_1B	9		/* POSIX 1003.1B */
#define	CTL_MAXID	10		/* number of valid top-level ids */

#define CTL_NAMES { \
	{ 0, 0 }, \
	{ "kern", CTLTYPE_NODE }, \
	{ "vm", CTLTYPE_NODE }, \
	{ "vfs", CTLTYPE_NODE }, \
	{ "net", CTLTYPE_NODE }, \
	{ "debug", CTLTYPE_NODE }, \
	{ "hw", CTLTYPE_NODE }, \
	{ "machdep", CTLTYPE_NODE }, \
	{ "user", CTLTYPE_NODE }, \
	{ "p1003_1b", CTLTYPE_NODE }, \
}

/*
 * CTL_KERN identifiers
 */
#define	KERN_OSTYPE	 	 1	/* string: system version */
#define	KERN_OSRELEASE	 	 2	/* string: system release */
#define	KERN_OSREV	 	 3	/* int: system revision */
#define	KERN_VERSION	 	 4	/* string: compile time info */
#define	KERN_MAXVNODES	 	 5	/* int: max vnodes */
#define	KERN_MAXPROC	 	 6	/* int: max processes */
#define	KERN_MAXFILES	 	 7	/* int: max open files */
#define	KERN_ARGMAX	 	 8	/* int: max arguments to exec */
#define	KERN_SECURELVL	 	 9	/* int: system security level */
#define	KERN_HOSTNAME		10	/* string: hostname */
#define	KERN_HOSTID		11	/* int: host identifier */
#define	KERN_CLOCKRATE		12	/* struct: struct clockrate */
#define	KERN_VNODE		13	/* struct: vnode structures */
#define	KERN_PROC		14	/* struct: process entries */
#define	KERN_FILE		15	/* struct: file entries */
#define	KERN_PROF		16	/* node: kernel profiling info */
#define	KERN_POSIX1		17	/* int: POSIX.1 version */
#define	KERN_NGROUPS		18	/* int: # of supplemental group ids */
#define	KERN_JOB_CONTROL	19	/* int: is job control available */
#define	KERN_SAVED_IDS		20	/* int: saved set-user/group-ID */
#define	KERN_BOOTTIME		21	/* struct: time kernel was booted */
#define KERN_NISDOMAINNAME	22	/* string: YP domain name */
#define KERN_UPDATEINTERVAL	23	/* int: update process sleep time */
#define KERN_OSRELDATE		24	/* int: kernel release date */
#define KERN_NTP_PLL		25	/* node: NTP PLL control */
#define	KERN_BOOTFILE		26	/* string: name of booted kernel */
#define	KERN_MAXFILESPERPROC	27	/* int: max open files per proc */
#define	KERN_MAXPROCPERUID 	28	/* int: max processes per uid */
#define KERN_DUMPDEV		29	/* struct cdev *: device to dump on */
#define	KERN_IPC		30	/* node: anything related to IPC */
#define	KERN_DUMMY		31	/* unused */
#define	KERN_PS_STRINGS		32	/* int: address of PS_STRINGS */
#define	KERN_USRSTACK		33	/* int: address of USRSTACK */
#define	KERN_LOGSIGEXIT		34	/* int: do we log sigexit procs? */
#define	KERN_IOV_MAX		35	/* int: value of UIO_MAXIOV */
#define	KERN_HOSTUUID		36	/* string: host UUID identifier */
#define	KERN_ARND		37	/* int: from arc4rand() */
#define	KERN_MAXID		38	/* number of valid kern ids */

#define CTL_KERN_NAMES { \
	{ 0, 0 }, \
	{ "ostype", CTLTYPE_STRING }, \
	{ "osrelease", CTLTYPE_STRING }, \
	{ "osrevision", CTLTYPE_INT }, \
	{ "version", CTLTYPE_STRING }, \
	{ "maxvnodes", CTLTYPE_INT }, \
	{ "maxproc", CTLTYPE_INT }, \
	{ "maxfiles", CTLTYPE_INT }, \
	{ "argmax", CTLTYPE_INT }, \
	{ "securelevel", CTLTYPE_INT }, \
	{ "hostname", CTLTYPE_STRING }, \
	{ "hostid", CTLTYPE_UINT }, \
	{ "clockrate", CTLTYPE_STRUCT }, \
	{ "vnode", CTLTYPE_STRUCT }, \
	{ "proc", CTLTYPE_STRUCT }, \
	{ "file", CTLTYPE_STRUCT }, \
	{ "profiling", CTLTYPE_NODE }, \
	{ "posix1version", CTLTYPE_INT }, \
	{ "ngroups", CTLTYPE_INT }, \
	{ "job_control", CTLTYPE_INT }, \
	{ "saved_ids", CTLTYPE_INT }, \
	{ "boottime", CTLTYPE_STRUCT }, \
	{ "nisdomainname", CTLTYPE_STRING }, \
	{ "update", CTLTYPE_INT }, \
	{ "osreldate", CTLTYPE_INT }, \
	{ "ntp_pll", CTLTYPE_NODE }, \
	{ "bootfile", CTLTYPE_STRING }, \
	{ "maxfilesperproc", CTLTYPE_INT }, \
	{ "maxprocperuid", CTLTYPE_INT }, \
	{ "ipc", CTLTYPE_NODE }, \
	{ "dummy", CTLTYPE_INT }, \
	{ "ps_strings", CTLTYPE_INT }, \
	{ "usrstack", CTLTYPE_INT }, \
	{ "logsigexit", CTLTYPE_INT }, \
	{ "iov_max", CTLTYPE_INT }, \
	{ "hostuuid", CTLTYPE_STRING }, \
}

/*
 * CTL_VFS identifiers
 */
#define CTL_VFS_NAMES { \
	{ "vfsconf", CTLTYPE_STRUCT }, \
}

/*
 * KERN_PROC subtypes
 */
#define KERN_PROC_ALL		0	/* everything */
#define	KERN_PROC_PID		1	/* by process id */
#define	KERN_PROC_PGRP		2	/* by process group id */
#define	KERN_PROC_SESSION	3	/* by session of pid */
#define	KERN_PROC_TTY		4	/* by controlling tty */
#define	KERN_PROC_UID		5	/* by effective uid */
#define	KERN_PROC_RUID		6	/* by real uid */
#define	KERN_PROC_ARGS		7	/* get/set arguments/proctitle */
#define	KERN_PROC_PROC		8	/* only return procs */
#define	KERN_PROC_SV_NAME	9	/* get syscall vector name */
#define	KERN_PROC_RGID		10	/* by real group id */
#define	KERN_PROC_GID		11	/* by effective group id */
#define	KERN_PROC_PATHNAME	12	/* path to executable */
#define	KERN_PROC_INC_THREAD	0x10	/*
					 * modifier for pid, pgrp, tty,
					 * uid, ruid, gid, rgid and proc
					 */

/*
 * KERN_IPC identifiers
 */
#define KIPC_MAXSOCKBUF		1	/* int: max size of a socket buffer */
#define	KIPC_SOCKBUF_WASTE	2	/* int: wastage factor in sockbuf */
#define	KIPC_SOMAXCONN		3	/* int: max length of connection q */
#define	KIPC_MAX_LINKHDR	4	/* int: max length of link header */
#define	KIPC_MAX_PROTOHDR	5	/* int: max length of network header */
#define	KIPC_MAX_HDR		6	/* int: max total length of headers */
#define	KIPC_MAX_DATALEN	7	/* int: max length of data? */

/*
 * CTL_HW identifiers
 */
#define	HW_MACHINE	 1		/* string: machine class */
#define	HW_MODEL	 2		/* string: specific machine model */
#define	HW_NCPU		 3		/* int: number of cpus */
#define	HW_BYTEORDER	 4		/* int: machine byte order */
#define	HW_PHYSMEM	 5		/* int: total memory */
#define	HW_USERMEM	 6		/* int: non-kernel memory */
#define	HW_PAGESIZE	 7		/* int: software page size */
#define	HW_DISKNAMES	 8		/* strings: disk drive names */
#define	HW_DISKSTATS	 9		/* struct: diskstats[] */
#define HW_FLOATINGPT	10		/* int: has HW floating point? */
#define HW_MACHINE_ARCH	11		/* string: machine architecture */
#define	HW_REALMEM	12		/* int: 'real' memory */
#define	HW_MAXID	13		/* number of valid hw ids */

#define CTL_HW_NAMES { \
	{ 0, 0 }, \
	{ "machine", CTLTYPE_STRING }, \
	{ "model", CTLTYPE_STRING }, \
	{ "ncpu", CTLTYPE_INT }, \
	{ "byteorder", CTLTYPE_INT }, \
	{ "physmem", CTLTYPE_ULONG }, \
	{ "usermem", CTLTYPE_ULONG }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "disknames", CTLTYPE_STRUCT }, \
	{ "diskstats", CTLTYPE_STRUCT }, \
	{ "floatingpoint", CTLTYPE_INT }, \
	{ "machine_arch", CTLTYPE_STRING }, \
	{ "realmem", CTLTYPE_ULONG }, \
}

/*
 * CTL_USER definitions
 */
#define	USER_CS_PATH		 1	/* string: _CS_PATH */
#define	USER_BC_BASE_MAX	 2	/* int: BC_BASE_MAX */
#define	USER_BC_DIM_MAX		 3	/* int: BC_DIM_MAX */
#define	USER_BC_SCALE_MAX	 4	/* int: BC_SCALE_MAX */
#define	USER_BC_STRING_MAX	 5	/* int: BC_STRING_MAX */
#define	USER_COLL_WEIGHTS_MAX	 6	/* int: COLL_WEIGHTS_MAX */
#define	USER_EXPR_NEST_MAX	 7	/* int: EXPR_NEST_MAX */
#define	USER_LINE_MAX		 8	/* int: LINE_MAX */
#define	USER_RE_DUP_MAX		 9	/* int: RE_DUP_MAX */
#define	USER_POSIX2_VERSION	10	/* int: POSIX2_VERSION */
#define	USER_POSIX2_C_BIND	11	/* int: POSIX2_C_BIND */
#define	USER_POSIX2_C_DEV	12	/* int: POSIX2_C_DEV */
#define	USER_POSIX2_CHAR_TERM	13	/* int: POSIX2_CHAR_TERM */
#define	USER_POSIX2_FORT_DEV	14	/* int: POSIX2_FORT_DEV */
#define	USER_POSIX2_FORT_RUN	15	/* int: POSIX2_FORT_RUN */
#define	USER_POSIX2_LOCALEDEF	16	/* int: POSIX2_LOCALEDEF */
#define	USER_POSIX2_SW_DEV	17	/* int: POSIX2_SW_DEV */
#define	USER_POSIX2_UPE		18	/* int: POSIX2_UPE */
#define	USER_STREAM_MAX		19	/* int: POSIX2_STREAM_MAX */
#define	USER_TZNAME_MAX		20	/* int: POSIX2_TZNAME_MAX */
#define	USER_MAXID		21	/* number of valid user ids */

#define	CTL_USER_NAMES { \
	{ 0, 0 }, \
	{ "cs_path", CTLTYPE_STRING }, \
	{ "bc_base_max", CTLTYPE_INT }, \
	{ "bc_dim_max", CTLTYPE_INT }, \
	{ "bc_scale_max", CTLTYPE_INT }, \
	{ "bc_string_max", CTLTYPE_INT }, \
	{ "coll_weights_max", CTLTYPE_INT }, \
	{ "expr_nest_max", CTLTYPE_INT }, \
	{ "line_max", CTLTYPE_INT }, \
	{ "re_dup_max", CTLTYPE_INT }, \
	{ "posix2_version", CTLTYPE_INT }, \
	{ "posix2_c_bind", CTLTYPE_INT }, \
	{ "posix2_c_dev", CTLTYPE_INT }, \
	{ "posix2_char_term", CTLTYPE_INT }, \
	{ "posix2_fort_dev", CTLTYPE_INT }, \
	{ "posix2_fort_run", CTLTYPE_INT }, \
	{ "posix2_localedef", CTLTYPE_INT }, \
	{ "posix2_sw_dev", CTLTYPE_INT }, \
	{ "posix2_upe", CTLTYPE_INT }, \
	{ "stream_max", CTLTYPE_INT }, \
	{ "tzname_max", CTLTYPE_INT }, \
}

#define CTL_P1003_1B_ASYNCHRONOUS_IO		1	/* boolean */
#define CTL_P1003_1B_MAPPED_FILES		2	/* boolean */
#define CTL_P1003_1B_MEMLOCK			3	/* boolean */
#define CTL_P1003_1B_MEMLOCK_RANGE		4	/* boolean */
#define CTL_P1003_1B_MEMORY_PROTECTION		5	/* boolean */
#define CTL_P1003_1B_MESSAGE_PASSING		6	/* boolean */
#define CTL_P1003_1B_PRIORITIZED_IO		7	/* boolean */
#define CTL_P1003_1B_PRIORITY_SCHEDULING	8	/* boolean */
#define CTL_P1003_1B_REALTIME_SIGNALS		9	/* boolean */
#define CTL_P1003_1B_SEMAPHORES			10	/* boolean */
#define CTL_P1003_1B_FSYNC			11	/* boolean */
#define CTL_P1003_1B_SHARED_MEMORY_OBJECTS	12	/* boolean */
#define CTL_P1003_1B_SYNCHRONIZED_IO		13	/* boolean */
#define CTL_P1003_1B_TIMERS			14	/* boolean */
#define CTL_P1003_1B_AIO_LISTIO_MAX		15	/* int */
#define CTL_P1003_1B_AIO_MAX			16	/* int */
#define CTL_P1003_1B_AIO_PRIO_DELTA_MAX		17	/* int */
#define CTL_P1003_1B_DELAYTIMER_MAX		18	/* int */
#define CTL_P1003_1B_MQ_OPEN_MAX		19	/* int */
#define CTL_P1003_1B_PAGESIZE			20	/* int */
#define CTL_P1003_1B_RTSIG_MAX			21	/* int */
#define CTL_P1003_1B_SEM_NSEMS_MAX		22	/* int */
#define CTL_P1003_1B_SEM_VALUE_MAX		23	/* int */
#define CTL_P1003_1B_SIGQUEUE_MAX		24	/* int */
#define CTL_P1003_1B_TIMER_MAX			25	/* int */

#define CTL_P1003_1B_MAXID		26

#define	CTL_P1003_1B_NAMES { \
	{ 0, 0 }, \
	{ "asynchronous_io", CTLTYPE_INT }, \
	{ "mapped_files", CTLTYPE_INT }, \
	{ "memlock", CTLTYPE_INT }, \
	{ "memlock_range", CTLTYPE_INT }, \
	{ "memory_protection", CTLTYPE_INT }, \
	{ "message_passing", CTLTYPE_INT }, \
	{ "prioritized_io", CTLTYPE_INT }, \
	{ "priority_scheduling", CTLTYPE_INT }, \
	{ "realtime_signals", CTLTYPE_INT }, \
	{ "semaphores", CTLTYPE_INT }, \
	{ "fsync", CTLTYPE_INT }, \
	{ "shared_memory_objects", CTLTYPE_INT }, \
	{ "synchronized_io", CTLTYPE_INT }, \
	{ "timers", CTLTYPE_INT }, \
	{ "aio_listio_max", CTLTYPE_INT }, \
	{ "aio_max", CTLTYPE_INT }, \
	{ "aio_prio_delta_max", CTLTYPE_INT }, \
	{ "delaytimer_max", CTLTYPE_INT }, \
	{ "mq_open_max", CTLTYPE_INT }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "rtsig_max", CTLTYPE_INT }, \
	{ "nsems_max", CTLTYPE_INT }, \
	{ "sem_value_max", CTLTYPE_INT }, \
	{ "sigqueue_max", CTLTYPE_INT }, \
	{ "timer_max", CTLTYPE_INT }, \
}

#ifdef _KERNEL

/*
 * Declare some common oids.
 */
extern struct sysctl_oid_list sysctl__children;
SYSCTL_DECL(_kern);
SYSCTL_DECL(_kern_ipc);
SYSCTL_DECL(_sysctl);
SYSCTL_DECL(_vm);
SYSCTL_DECL(_vm_stats);
SYSCTL_DECL(_vm_stats_misc);
SYSCTL_DECL(_vfs);
SYSCTL_DECL(_net);
SYSCTL_DECL(_debug);
SYSCTL_DECL(_debug_sizeof);
SYSCTL_DECL(_hw);
SYSCTL_DECL(_hw_bus);
SYSCTL_DECL(_machdep);
SYSCTL_DECL(_user);
SYSCTL_DECL(_compat);
SYSCTL_DECL(_regression);
SYSCTL_DECL(_security);
SYSCTL_DECL(_security_bsd);

extern char	machine[];
extern char	osrelease[];
extern char	ostype[];
extern char	kern_ident[];

/* Dynamic oid handling */
struct sysctl_oid *sysctl_add_oid(struct sysctl_ctx_list *clist,
		struct sysctl_oid_list *parent, int nbr, const char *name,
		int kind, void *arg1, int arg2,
		int (*handler) (SYSCTL_HANDLER_ARGS),
		const char *fmt, const char *descr);
void	sysctl_rename_oid(struct sysctl_oid *oidp, const char *name);
int	sysctl_move_oid(struct sysctl_oid *oidp,
		struct sysctl_oid_list *parent);
int	sysctl_remove_oid(struct sysctl_oid *oidp, int del, int recurse);
int	sysctl_ctx_init(struct sysctl_ctx_list *clist);
int	sysctl_ctx_free(struct sysctl_ctx_list *clist);
struct	sysctl_ctx_entry *sysctl_ctx_entry_add(struct sysctl_ctx_list *clist,
		struct sysctl_oid *oidp);
struct	sysctl_ctx_entry *sysctl_ctx_entry_find(struct sysctl_ctx_list *clist,
		struct sysctl_oid *oidp);
int	sysctl_ctx_entry_del(struct sysctl_ctx_list *clist,
		struct sysctl_oid *oidp);

int	kernel_sysctl(struct thread *td, int *name, u_int namelen, void *old,
		      size_t *oldlenp, void *new, size_t newlen,
		      size_t *retval, int flags);
int	kernel_sysctlbyname(struct thread *td, char *name,
		void *old, size_t *oldlenp, void *new, size_t newlen,
		size_t *retval, int flags);
int	userland_sysctl(struct thread *td, int *name, u_int namelen, void *old,
			size_t *oldlenp, int inkernel, void *new, size_t newlen,
			size_t *retval, int flags);
int	sysctl_find_oid(int *name, u_int namelen, struct sysctl_oid **noid,
			int *nindx, struct sysctl_req *req);
int	sysctl_wire_old_buffer(struct sysctl_req *req, size_t len);

#else	/* !_KERNEL */
#include <sys/cdefs.h>

__BEGIN_DECLS
int	sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	sysctlbyname(const char *, void *, size_t *, void *, size_t);
int	sysctlnametomib(const char *, int *, size_t *);
__END_DECLS
#endif	/* _KERNEL */

#endif	/* !_SYS_SYSCTL_H_ */
