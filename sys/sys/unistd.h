/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)unistd.h	8.2 (Berkeley) 1/7/94
 * $FreeBSD$
 */

#ifndef _SYS_UNISTD_H_
#define	_SYS_UNISTD_H_

#include <sys/_posix.h>

/* compile-time symbolic constants */
#define	_POSIX_JOB_CONTROL	/* implementation supports job control */

/*
 * Although we have saved user/group IDs, we do not use them in setuid
 * as described in POSIX 1003.1, because the feature does not work for
 * root.  We use the saved IDs in seteuid/setegid, which are not currently
 * part of the POSIX 1003.1 specification.
 */
#ifdef	_NOT_AVAILABLE
#define	_POSIX_SAVED_IDS	/* saved set-user-ID and set-group-ID */
#endif

#define	_POSIX2_VERSION		199212L

/* execution-time symbolic constants */
				/* chown requires appropriate privileges */
#define	_POSIX_CHOWN_RESTRICTED	1
				/* too-long path components generate errors */
#define	_POSIX_NO_TRUNC		1
				/* may disable terminal special characters */
#define	_POSIX_VDISABLE		0xff

/*
 * Threads features:
 *
 * Note that those commented out are not currently supported by the
 * implementation.
 */
#define _POSIX_THREADS
#define _POSIX_THREAD_ATTR_STACKADDR
#define _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREAD_PRIORITY_SCHEDULING
#define _POSIX_THREAD_PRIO_INHERIT
#define _POSIX_THREAD_PRIO_PROTECT
/* #define _POSIX_THREAD_PROCESS_SHARED */
/*
 * 1003.1c-1995 says on page 38 (2.9.3, paragraph 3) that if _POSIX_THREADS is
 * defined, then _POSIX_THREAD_SAFE_FUNCTIONS must also be defined.  (This is
 * likely a typo (reversed dependency), in which case we would be compliant if
 * the typo were officially acknowledged.)  However, we do not support all of
 * the required _r() interfaces, which means we cannot legitimately define
 * _POSIX_THREAD_SAFE_FUNCTIONS.  Therefore, we are non-compliant here in two
 * ways.
 */
/* #define _POSIX_THREAD_SAFE_FUNCTIONS */
#define _POSIX_SEMAPHORES

/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */

/* whence values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#ifndef _POSIX_SOURCE
/* whence values for lseek(2); renamed by POSIX 1003.1 */
#define	L_SET		SEEK_SET
#define	L_INCR		SEEK_CUR
#define	L_XTND		SEEK_END
#endif

/* configurable pathname variables */
#define	_PC_LINK_MAX		 1
#define	_PC_MAX_CANON		 2
#define	_PC_MAX_INPUT		 3
#define	_PC_NAME_MAX		 4
#define	_PC_PATH_MAX		 5
#define	_PC_PIPE_BUF		 6
#define	_PC_CHOWN_RESTRICTED	 7
#define	_PC_NO_TRUNC		 8
#define	_PC_VDISABLE		 9

/* configurable system variables */
#define	_SC_ARG_MAX		 1
#define	_SC_CHILD_MAX		 2
#define	_SC_CLK_TCK		 3
#define	_SC_NGROUPS_MAX		 4
#define	_SC_OPEN_MAX		 5
#define	_SC_JOB_CONTROL		 6
#define	_SC_SAVED_IDS		 7
#define	_SC_VERSION		 8
#define	_SC_BC_BASE_MAX		 9
#define	_SC_BC_DIM_MAX		10
#define	_SC_BC_SCALE_MAX	11
#define	_SC_BC_STRING_MAX	12
#define	_SC_COLL_WEIGHTS_MAX	13
#define	_SC_EXPR_NEST_MAX	14
#define	_SC_LINE_MAX		15
#define	_SC_RE_DUP_MAX		16
#define	_SC_2_VERSION		17
#define	_SC_2_C_BIND		18
#define	_SC_2_C_DEV		19
#define	_SC_2_CHAR_TERM		20
#define	_SC_2_FORT_DEV		21
#define	_SC_2_FORT_RUN		22
#define	_SC_2_LOCALEDEF		23
#define	_SC_2_SW_DEV		24
#define	_SC_2_UPE		25
#define	_SC_STREAM_MAX		26
#define	_SC_TZNAME_MAX		27

/* configurable system strings */
#define	_CS_PATH		 1

#ifdef _P1003_1B_VISIBLE

#define _POSIX_PRIORITY_SCHEDULING

#if 0
/* Not until the dust settles after the header commit
 */
#define _POSIX_ASYNCHRONOUS_IO
#define _POSIX_MEMLOCK
#define _POSIX_MEMLOCK_RANGE
#endif

/* ??? #define	_POSIX_FSYNC			1 */
#define	_POSIX_MAPPED_FILES		1
#define _POSIX_SHARED_MEMORY_OBJECTS	1

/* POSIX.1B sysconf options */
#define _SC_ASYNCHRONOUS_IO	28
#define _SC_MAPPED_FILES	29
#define _SC_MEMLOCK		30
#define _SC_MEMLOCK_RANGE	31
#define _SC_MEMORY_PROTECTION	32
#define _SC_MESSAGE_PASSING	33
#define _SC_PRIORITIZED_IO	34
#define _SC_PRIORITY_SCHEDULING	35
#define _SC_REALTIME_SIGNALS	36
#define _SC_SEMAPHORES		37
#define _SC_FSYNC		38
#define _SC_SHARED_MEMORY_OBJECTS 39
#define _SC_SYNCHRONIZED_IO	40
#define _SC_TIMERS		41
#define _SC_AIO_LISTIO_MAX	42
#define _SC_AIO_MAX		43
#define _SC_AIO_PRIO_DELTA_MAX	44
#define _SC_DELAYTIMER_MAX	45
#define _SC_MQ_OPEN_MAX		46
#define _SC_PAGESIZE		47
#define _SC_RTSIG_MAX		48
#define _SC_SEM_NSEMS_MAX	49
#define _SC_SEM_VALUE_MAX	50
#define _SC_SIGQUEUE_MAX	51
#define _SC_TIMER_MAX		52

/* POSIX.1B pathconf and fpathconf options */
#define _PC_ASYNC_IO		53
#define _PC_PRIO_IO		54
#define _PC_SYNC_IO		55

#endif /* _P1003_1B_VISIBLE */

#ifndef _POSIX_SOURCE
/*
 * rfork() options.
 *
 * XXX currently, operations without RFPROC set are not supported.
 */
#define RFNAMEG		(1<<0)  /* UNIMPL new plan9 `name space' */
#define RFENVG		(1<<1)  /* UNIMPL copy plan9 `env space' */
#define RFFDG		(1<<2)  /* copy fd table */
#define RFNOTEG		(1<<3)  /* UNIMPL create new plan9 `note group' */
#define RFPROC		(1<<4)  /* change child (else changes curproc) */
#define RFMEM		(1<<5)  /* share `address space' */
#define RFNOWAIT	(1<<6)  /* parent need not wait() on child */ 
#define RFCNAMEG	(1<<10) /* UNIMPL zero plan9 `name space' */
#define RFCENVG		(1<<11) /* UNIMPL zero plan9 `env space' */
#define RFCFDG		(1<<12) /* zero fd table */
#define RFTHREAD	(1<<13)	/* enable kernel thread support */
#define RFSIGSHARE	(1<<14)	/* share signal handlers */
#define RFLINUXTHPN     (1<<16) /* do linux clone exit parent notification */
#define RFPPWAIT	(1<<31) /* parent sleeps until child exits (vfork) */

#endif /* !_POSIX_SOURCE */

#endif /* !_SYS_UNISTD_H_ */
