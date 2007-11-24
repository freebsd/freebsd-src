/*-
 * Copyright (c) 1998-1999 Andrew Gallatin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$ 
 */

extern struct sysent osf1_sysent[];
extern int bsd_to_osf1_sig[];
extern int bsd_to_osf1_errno[];

#define	OSF1_MINSIGSTKSZ	4096

/* osf/1 ioctls */
#define	OSF1_IOCPARM_MASK	0x1fff	/* parameter length, at most 13 bits */
#define	OSF1_IOCPARM_LEN(x)	(((x) >> 16) & OSF1_IOCPARM_MASK)
#define	OSF1_IOCGROUP(x)	(((x) >> 8) & 0xff)
#define	OSF1_IOCPARM_MAX	NBPG		/* max size of ioctl */
#define	OSF1_IOC_VOID		0x20000000	/* no parameters */
#define	OSF1_IOC_OUT		0x40000000	/* copy out parameters */
#define	OSF1_IOC_IN		0x80000000	/* copy in parameters */
#define	OSF1_IOC_INOUT		(OSF1_IOC_IN|OSF1_IOC_OUT)
#define	OSF1_IOC_DIRMASK	0xe0000000	/* mask for IN/OUT/VOID */
#define	OSF1_IOCCMD(x)		((x) & 0xff)

/* for get sysinfo */
#define	OSF_GET_MAX_UPROCS	2
#define	OSF_GET_PHYSMEM		19
#define	OSF_GET_MAX_CPU		30
#define	OSF_GET_IEEE_FP_CONTROL 45
#define	OSF_GET_CPUS_IN_BOX	55
#define	OSF_GET_CPU_INFO	59
#define	OSF_GET_PROC_TYPE	60
#define	OSF_GET_HWRPB		101
#define	OSF_GET_PLATFORM_NAME	103

struct	osf1_cpu_info {
	int		current_cpu;
	int     	cpus_in_box;
	int		cpu_type;
	int		ncpus;
	u_int64_t	cpus_present;
	u_int64_t 	cpus_running;
	u_int64_t	cpu_binding;
	u_int64_t	cpu_ex_binding;
	int  		mhz;
	int  		unused[3];
};



/* for set sysinfo */
#define	OSF_SET_IEEE_FP_CONTROL  14

/* for rlimit */
#define	OSF1_RLIMIT_LASTCOMMON	5		/* last one that's common */
#define	OSF1_RLIMIT_NOFILE	6		/* OSF1's RLIMIT_NOFILE */
#define	OSF1_RLIMIT_NLIMITS	8		/* Number of OSF1 rlimits */

/* mmap flags */

#define	OSF1_MAP_SHARED		0x001
#define	OSF1_MAP_PRIVATE	0x002
#define	OSF1_MAP_ANONYMOUS	0x010
#define	OSF1_MAP_FILE		0x000
#define	OSF1_MAP_TYPE		0x0f0
#define	OSF1_MAP_FIXED		0x100
#define	OSF1_MAP_HASSEMAPHORE	0x200
#define	OSF1_MAP_INHERIT	0x400
#define	OSF1_MAP_UNALIGNED	0x800

/* msync flags */

#define	OSF1_MS_ASYNC		1
#define	OSF1_MS_SYNC		2
#define	OSF1_MS_INVALIDATE	4

#define	OSF1_F_DUPFD	0
#define	OSF1_F_GETFD	1
#define	OSF1_F_SETFD	2
#define	OSF1_F_GETFL	3
#define	OSF1_F_SETFL	4


#define _OSF1_PC_CHOWN_RESTRICTED	10
#define _OSF1_PC_LINK_MAX		11
#define _OSF1_PC_MAX_CANON		12
#define _OSF1_PC_MAX_INPUT		13
#define _OSF1_PC_NAME_MAX		14
#define _OSF1_PC_NO_TRUNC		15
#define _OSF1_PC_PATH_MAX		16
#define _OSF1_PC_PIPE_BUF		17
#define _OSF1_PC_VDISABLE		18



#define	OSF1_FNONBLOCK	0x00004		/* XXX OSF1_O_NONBLOCK */
#define	OSF1_FAPPEND	0x00008		/* XXX OSF1_O_APPEND */
#define	OSF1_FDEFER	0x00020
#define	OSF1_FASYNC	0x00040
#define	OSF1_FCREAT	0x00200
#define	OSF1_FTRUNC	0x00400
#define	OSF1_FEXCL	0x00800
#define	OSF1_FSYNC	0x04000		/* XXX OSF1_O_SYNC */
#define	OSF1_FNDELAY	0x08000

#define	OSF1_RB_ASKNAME		0x001
#define	OSF1_RB_SINGLE		0x002
#define	OSF1_RB_NOSYNC		0x004
#define	OSF1_RB_HALT		0x008
#define	OSF1_RB_INITNAME	0x010
#define	OSF1_RB_DFLTROOT	0x020
#define	OSF1_RB_ALTBOOT		0x040
#define	OSF1_RB_UNIPROC		0x080
#define	OSF1_RB_ALLFLAGS	0x0ff		/* all of the above */

/*
 * osf/1 uses ints in its struct timeval, this means that
 * any syscalls which means that any system calls using 
 * timevals need to be intercepted.
 */


struct osf1_timeval {
	int  tv_sec;	/* seconds */
	int  tv_usec;	/* microseconds */
};

struct osf1_itimerval {
	struct osf1_timeval it_interval;	/* timer interval */
        struct osf1_timeval it_value;		/* current value */
};
#define TV_CP(src,dst) {dst.tv_usec = src.tv_usec; dst.tv_sec = src.tv_sec;}

#define timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)

struct osf1_rusage {
	struct osf1_timeval	ru_utime;	/* user time used */
	struct osf1_timeval	ru_stime;	/* system time used */
	long	ru_maxrss;		/* max resident set size */
#define	ru_first ru_ixrss
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data " */
	long	ru_isrss;		/* integral unshared stack " */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define ru_last ru_nivcsw
};

#define	OSF1_USC_GET	1
#define	OSF1_USC_SET	2
#define	OSF1_USW_NULLP	0x100


/* File system type numbers. */
#define	OSF1_MOUNT_NONE		0
#define	OSF1_MOUNT_UFS		1
#define	OSF1_MOUNT_NFS		2
#define	OSF1_MOUNT_MFS		3
#define	OSF1_MOUNT_PC		4
#define	OSF1_MOUNT_S5FS		5
#define	OSF1_MOUNT_CDFS		6
#define	OSF1_MOUNT_DFS		7
#define	OSF1_MOUNT_EFS		8
#define	OSF1_MOUNT_PROCFS	9
#define	OSF1_MOUNT_MSFS		10
#define	OSF1_MOUNT_FFM		11
#define	OSF1_MOUNT_FDFS		12
#define	OSF1_MOUNT_ADDON	13
#define	OSF1_MOUNT_MAXTYPE	OSF1_MOUNT_ADDON

#define	OSF1_MNT_WAIT		0x1
#define	OSF1_MNT_NOWAIT		0x2

#define	OSF1_MNT_FORCE		0x1
#define	OSF1_MNT_NOFORCE	0x2

/* acceptable flags for various calls */
#define	OSF1_GETFSSTAT_FLAGS	(OSF1_MNT_WAIT|OSF1_MNT_NOWAIT)
#define	OSF1_MOUNT_FLAGS	0xffffffff			/* XXX */
#define	OSF1_UNMOUNT_FLAGS	(OSF1_MNT_FORCE|OSF1_MNT_NOFORCE)

struct osf1_statfs {
	int16_t	f_type;				/*   0 */
	int16_t	f_flags;			/*   2 */
	int32_t	f_fsize;			/*   4 */
	int32_t	f_bsize;			/*   8 */
	int32_t	f_blocks;			/*  12 */
	int32_t	f_bfree;			/*  16 */
	int32_t	f_bavail;			/*  20 */
	int32_t	f_files;			/*  24 */
	int32_t	f_ffree;			/*  28 */
	int64_t	f_fsid;				/*  32 */
	int32_t	f_spare[9];			/*  40 (36 bytes) */
	char	f_mntonname[90];		/*  76 (90 bytes) */
	char	f_mntfromname[90];		/* 166 (90 bytes) */
	char	f_xxx[80];			/* 256 (80 bytes) XXX */
};
/* Arguments to mount() for various FS types. */
#ifdef notyet /* XXX */
struct osf1_ufs_args {
	char		*fspec;
	int32_t		exflags;
	u_int32_t	exroot;
};

struct osf1_cdfs_args {
	char		*fspec;
	int32_t		exflags;
	u_int32_t	exroot;
	int32_t		flags;
};
#endif

struct osf1_mfs_args {
	char		*name;
	caddr_t		base;
	u_int		size;
};

struct osf1_nfs_args {
	struct sockaddr_in	*addr;
	void			*fh;
	int32_t			flags;
	int32_t			wsize;
	int32_t			rsize;
	int32_t			timeo;
	int32_t			retrans;
	char			*hostname;
	int32_t			acregmin;
	int32_t			acregmax;
	int32_t			acdirmin;
	int32_t			acdirmax;
	char			*netname;
	void			*pathconf;
};

#define	OSF1_NFSMNT_SOFT	0x00001
#define	OSF1_NFSMNT_WSIZE	0x00002
#define	OSF1_NFSMNT_RSIZE	0x00004
#define	OSF1_NFSMNT_TIMEO	0x00008
#define	OSF1_NFSMNT_RETRANS	0x00010
#define	OSF1_NFSMNT_HOSTNAME	0x00020
#define	OSF1_NFSMNT_INT		0x00040
#define	OSF1_NFSMNT_NOCONN	0x00080
#define	OSF1_NFSMNT_NOAC	0x00100			/* ??? */
#define	OSF1_NFSMNT_ACREGMIN	0x00200			/* ??? */
#define	OSF1_NFSMNT_ACREGMAX	0x00400			/* ??? */
#define	OSF1_NFSMNT_ACDIRMIN	0x00800			/* ??? */
#define	OSF1_NFSMNT_ACDIRMAX	0x01000			/* ??? */
#define	OSF1_NFSMNT_NOCTO	0x02000			/* ??? */
#define	OSF1_NFSMNT_POSIX	0x04000			/* ??? */
#define	OSF1_NFSMNT_AUTO	0x08000			/* ??? */

#define	OSF1_NFSMNT_FLAGS						\
	(OSF1_NFSMNT_SOFT|OSF1_NFSMNT_WSIZE|OSF1_NFSMNT_RSIZE|		\
	OSF1_NFSMNT_TIMEO|OSF1_NFSMNT_RETRANS|OSF1_NFSMNT_HOSTNAME|	\
	OSF1_NFSMNT_INT|OSF1_NFSMNT_NOCONN)

#define	memset(x,y,z) bzero((x),(z))
