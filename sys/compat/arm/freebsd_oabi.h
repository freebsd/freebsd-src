/*-
 * Copyright (c) 2001 Doug Rabson
 * Copyright (C) 2010 Andrew Turner
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _COMPAT_ARM_FREEBSD_OABI_H_
#define _COMPAT_ARM_FREEBSD_OABI_H_

#define CP(src,dst,fld) do { (dst).fld = (src).fld; } while (0)

#define PAIR32TO64(type, name) ((name ## 1) | ((type)(name ## 2) << 32))

/* Copy an 64 bit type split into two 32 bit argunemts, e.g. off_t */
#define CP64(src,dst,fld) do {				\
	(dst).fld = (src).fld ## 1;			\
	(dst).fld |= ((uint64_t)(src).fld ## 2) << 32;	\
} while (0)

struct timeval_oabi {
        time_t          tv_sec;
        suseconds_t     tv_usec;
} __packed;
#define TV_CP(src,dst,fld) do {			\
	CP((src).fld,(dst).fld,tv_sec);		\
	CP((src).fld,(dst).fld,tv_usec);	\
} while (0)

struct itimerval_oabi {
	struct timeval_oabi it_interval;
	struct timeval_oabi it_value;
};

struct timespec_oabi {
	time_t	tv_sec;
	long	tv_nsec;
} __packed;
#define TS_CP(src,dst,fld) do {			\
	CP((src).fld,(dst).fld,tv_sec);		\
	CP((src).fld,(dst).fld,tv_nsec);	\
} while (0);

struct rusage_oabi {
	struct timeval_oabi ru_utime;
	struct timeval_oabi ru_stime;
	long	ru_maxrss;
	long	ru_ixrss;
	long	ru_idrss;
	long	ru_isrss;
	long	ru_minflt;
	long	ru_majflt;
	long	ru_nswap;
	long	ru_inblock;
	long	ru_oublock;
	long	ru_msgsnd;
	long	ru_msgrcv;
	long	ru_nsignals;
	long	ru_nvcsw;
	long	ru_nivcsw;
} __packed;

struct stat_oabi {
	__dev_t	  st_dev;
	ino_t	  st_ino;
	mode_t	  st_mode;
	nlink_t	  st_nlink;
	uid_t	  st_uid;
	gid_t	  st_gid;
	__dev_t	  st_rdev;
	struct timespec_oabi st_atim;
	struct timespec_oabi st_mtim;
	struct timespec_oabi st_ctim;
	off_t	  st_size;
	blkcnt_t  st_blocks;
	blksize_t st_blksize;
	fflags_t  st_flags;
	__uint32_t st_gen;
	__int32_t st_lspare;
	struct timespec_oabi st_birthtim;
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec_oabi));
	unsigned int :(8 / 2) * (16 - (int)sizeof(struct timespec_oabi));
} __packed;

struct __aiocb_private {
	long	status;
	long	error;
	void	*kernelinfo;
};

struct aiocb_oabi {
	int	aio_fildes;
	off_t	aio_offset;
	volatile void *aio_buf;
	size_t	aio_nbytes;
	int	__spare__[2];
	void	*__spare2__;
	int	aio_lio_opcode;
	int	aio_reqprio;
	struct	__aiocb_private	_aiocb_private;
	struct	sigevent aio_sigevent;
} __packed;

struct msqid_ds_oabi {
	struct	ipc_perm msg_perm;	/* msg queue permission bits */
	struct	msg *msg_first;	/* first message in the queue */
	struct	msg *msg_last;	/* last message in the queue */
	msglen_t msg_cbytes;	/* number of bytes in use on the queue */
	msgqnum_t msg_qnum;	/* number of msgs in the queue */
	msglen_t msg_qbytes;	/* max # of bytes on the queue */
	pid_t	msg_lspid;	/* pid of last msgsnd() */
	pid_t	msg_lrpid;	/* pid of last msgrcv() */
	time_t	msg_stime;	/* time of last msgsnd() */
	time_t	msg_rtime;	/* time of last msgrcv() */
	time_t	msg_ctime;	/* time of last msgctl() */
} __packed;

#endif /* _COMPAT_ARM_FREEBSD_OABI_H_ */
