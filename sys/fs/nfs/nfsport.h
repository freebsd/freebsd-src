/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 * $FreeBSD$
 */

#ifndef _NFS_NFSPORT_H_
#define	_NFS_NFSPORT_H_

/*
 * In general, I'm not fond of #includes in .h files, but this seems
 * to be the cleanest way to handle #include files for the ports.
 */
#ifdef _KERNEL
#include <sys/unistd.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/domain.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/acl.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/priv.h>
#include <sys/kthread.h>
#include <sys/syscallsubr.h>
#include <fs/fifofs/fifo.h>
#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <machine/in_cksum.h>
#include <crypto/des/des.h>
#include <sys/md5.h>
#include <rpc/rpc.h>
#include <rpc/rpcsec_gss.h>

/*
 * For Darwin, these functions should be "static" when built in a kext.
 * (This is always defined as nil otherwise.)
 */
#define	APPLESTATIC
#include <ufs/ufs/dir.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/ufsmount.h>
#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <nfs/nfssvc.h>
#include "opt_nfs.h"
#include "opt_ufs.h"

/*
 * These types must be defined before the nfs includes.
 */
#define	NFSSOCKADDR_T	struct sockaddr *
#define	NFSPROC_T	struct thread
#define	NFSDEV_T	dev_t
#define	NFSSVCARGS	nfssvc_args
#define	NFSACL_T	struct acl

/*
 * These should be defined as the types used for the corresponding VOP's
 * argument type.
 */
#define	NFS_ACCESS_ARGS		struct vop_access_args
#define	NFS_OPEN_ARGS		struct vop_open_args
#define	NFS_GETATTR_ARGS	struct vop_getattr_args
#define	NFS_LOOKUP_ARGS		struct vop_lookup_args
#define	NFS_READDIR_ARGS	struct vop_readdir_args

/*
 * Allocate mbufs. Must succeed and never set the mbuf ptr to NULL.
 */
#define	NFSMGET(m)	do { 					\
		MGET((m), M_TRYWAIT, MT_DATA); 			\
		while ((m) == NULL ) { 				\
			(void) nfs_catnap(PZERO, 0, "nfsmget");	\
			MGET((m), M_TRYWAIT, MT_DATA); 		\
		} 						\
	} while (0)
#define	NFSMGETHDR(m)	do { 					\
		MGETHDR((m), M_TRYWAIT, MT_DATA);		\
		while ((m) == NULL ) { 				\
			(void) nfs_catnap(PZERO, 0, "nfsmget");	\
			MGETHDR((m), M_TRYWAIT, MT_DATA); 	\
		} 						\
	} while (0)
#define	NFSMCLGET(m, w)	do { 					\
		MGET((m), M_TRYWAIT, MT_DATA); 			\
		while ((m) == NULL ) { 				\
			(void) nfs_catnap(PZERO, 0, "nfsmget");	\
			MGET((m), M_TRYWAIT, MT_DATA); 		\
		} 						\
		MCLGET((m), (w));				\
	} while (0)
#define	NFSMCLGETHDR(m, w) do { 				\
		MGETHDR((m), M_TRYWAIT, MT_DATA);		\
		while ((m) == NULL ) { 				\
			(void) nfs_catnap(PZERO, 0, "nfsmget");	\
			MGETHDR((m), M_TRYWAIT, MT_DATA); 	\
		} 						\
	} while (0)
#define	NFSMTOD	mtod

/*
 * Client side constant for size of a lockowner name.
 */
#define	NFSV4CL_LOCKNAMELEN	12

/*
 * Type for a mutex lock.
 */
#define	NFSMUTEX_T		struct mtx

#endif	/* _KERNEL */

/*
 * NFSv4 Operation numbers.
 */
#define	NFSV4OP_ACCESS		3
#define	NFSV4OP_CLOSE		4
#define	NFSV4OP_COMMIT		5
#define	NFSV4OP_CREATE		6
#define	NFSV4OP_DELEGPURGE	7
#define	NFSV4OP_DELEGRETURN	8
#define	NFSV4OP_GETATTR		9
#define	NFSV4OP_GETFH		10
#define	NFSV4OP_LINK		11
#define	NFSV4OP_LOCK		12
#define	NFSV4OP_LOCKT		13
#define	NFSV4OP_LOCKU		14
#define	NFSV4OP_LOOKUP		15
#define	NFSV4OP_LOOKUPP		16
#define	NFSV4OP_NVERIFY		17
#define	NFSV4OP_OPEN		18
#define	NFSV4OP_OPENATTR	19
#define	NFSV4OP_OPENCONFIRM	20
#define	NFSV4OP_OPENDOWNGRADE	21
#define	NFSV4OP_PUTFH		22
#define	NFSV4OP_PUTPUBFH	23
#define	NFSV4OP_PUTROOTFH	24
#define	NFSV4OP_READ		25
#define	NFSV4OP_READDIR		26
#define	NFSV4OP_READLINK	27
#define	NFSV4OP_REMOVE		28
#define	NFSV4OP_RENAME		29
#define	NFSV4OP_RENEW		30
#define	NFSV4OP_RESTOREFH	31
#define	NFSV4OP_SAVEFH		32
#define	NFSV4OP_SECINFO		33
#define	NFSV4OP_SETATTR		34
#define	NFSV4OP_SETCLIENTID	35
#define	NFSV4OP_SETCLIENTIDCFRM	36
#define	NFSV4OP_VERIFY		37
#define	NFSV4OP_WRITE		38
#define	NFSV4OP_RELEASELCKOWN	39

/*
 * Must be one greater than the last Operation#.
 */
#define	NFSV4OP_NOPS		40

/* Quirky case if the illegal op code */
#define	NFSV4OP_OPILLEGAL	10044

/*
 * Fake NFSV4OP_xxx used for nfsstat. Start at NFSV4OP_NOPS.
 */
#define	NFSV4OP_SYMLINK		(NFSV4OP_NOPS)
#define	NFSV4OP_MKDIR		(NFSV4OP_NOPS + 1)
#define	NFSV4OP_RMDIR		(NFSV4OP_NOPS + 2)
#define	NFSV4OP_READDIRPLUS	(NFSV4OP_NOPS + 3)
#define	NFSV4OP_MKNOD		(NFSV4OP_NOPS + 4)
#define	NFSV4OP_FSSTAT		(NFSV4OP_NOPS + 5)
#define	NFSV4OP_FSINFO		(NFSV4OP_NOPS + 6)
#define	NFSV4OP_PATHCONF	(NFSV4OP_NOPS + 7)
#define	NFSV4OP_V3CREATE	(NFSV4OP_NOPS + 8)

/*
 * This is the count of the fake operations listed above.
 */
#define	NFSV4OP_FAKENOPS	9

/*
 * and the Callback OPs
 */
#define	NFSV4OP_CBGETATTR	3
#define	NFSV4OP_CBRECALL	4

/*
 * Must be one greater than the last Callback Operation#.
 */
#define	NFSV4OP_CBNOPS		5

/*
 * The lower numbers -> 21 are used by NFSv2 and v3. These define higher
 * numbers used by NFSv4.
 * NFS_V3NPROCS is one greater than the last V3 op and NFS_NPROCS is
 * one greater than the last number.
 */
#define	NFS_V3NPROCS		22

#define	NFSPROC_LOOKUPP		22
#define	NFSPROC_SETCLIENTID	23
#define	NFSPROC_SETCLIENTIDCFRM	24
#define	NFSPROC_LOCK		25
#define	NFSPROC_LOCKU		26
#define	NFSPROC_OPEN		27
#define	NFSPROC_CLOSE		28
#define	NFSPROC_OPENCONFIRM	29
#define	NFSPROC_LOCKT		30
#define	NFSPROC_OPENDOWNGRADE	31
#define	NFSPROC_RENEW		32
#define	NFSPROC_PUTROOTFH	33
#define	NFSPROC_RELEASELCKOWN	34
#define	NFSPROC_DELEGRETURN	35
#define	NFSPROC_RETDELEGREMOVE	36
#define	NFSPROC_RETDELEGRENAME1	37
#define	NFSPROC_RETDELEGRENAME2	38
#define	NFSPROC_GETACL		39
#define	NFSPROC_SETACL		40

/*
 * Must be defined as one higher than the last Proc# above.
 */
#define	NFSV4_NPROCS		41

/*
 * Stats structure
 */
struct ext_nfsstats {
	int	attrcache_hits;
	int	attrcache_misses;
	int	lookupcache_hits;
	int	lookupcache_misses;
	int	direofcache_hits;
	int	direofcache_misses;
	int	accesscache_hits;
	int	accesscache_misses;
	int	biocache_reads;
	int	read_bios;
	int	read_physios;
	int	biocache_writes;
	int	write_bios;
	int	write_physios;
	int	biocache_readlinks;
	int	readlink_bios;
	int	biocache_readdirs;
	int	readdir_bios;
	int	rpccnt[NFSV4_NPROCS];
	int	rpcretries;
	int	srvrpccnt[NFSV4OP_NOPS + NFSV4OP_FAKENOPS];
	int	srvrpc_errs;
	int	srv_errs;
	int	rpcrequests;
	int	rpctimeouts;
	int	rpcunexpected;
	int	rpcinvalid;
	int	srvcache_inproghits;
	int	srvcache_idemdonehits;
	int	srvcache_nonidemdonehits;
	int	srvcache_misses;
	int	srvcache_tcppeak;
	int	srvcache_size;
	int	srvclients;
	int	srvopenowners;
	int	srvopens;
	int	srvlockowners;
	int	srvlocks;
	int	srvdelegates;
	int	cbrpccnt[NFSV4OP_CBNOPS];
	int	clopenowners;
	int	clopens;
	int	cllockowners;
	int	cllocks;
	int	cldelegates;
	int	cllocalopenowners;
	int	cllocalopens;
	int	cllocallockowners;
	int	cllocallocks;
};

#ifdef _KERNEL
/*
 * Define the ext_nfsstats as nfsstats for the kernel code.
 */
#define nfsstats	ext_nfsstats

/*
 * Define NFS_NPROCS as NFSV4_NPROCS for the experimental kernel code.
 */
#define	NFS_NPROCS		NFSV4_NPROCS

#include <fs/nfs/nfskpiport.h>
#include <fs/nfs/nfsdport.h>
#include <fs/nfs/rpcv2.h>
#include <fs/nfs/nfsproto.h>
#include <fs/nfs/nfs.h>
#include <fs/nfs/nfs_var.h>
#include <fs/nfs/nfsm_subs.h>
#include <fs/nfs/nfsrvcache.h>
#include <fs/nfs/nfsrvstate.h>
#include <fs/nfs/xdr_subs.h>
#include <fs/nfs/nfscl.h>
#include <fs/nfs/nfsclstate.h>
#include <nfsclient/nfsargs.h>
#include <fs/nfsclient/nfsmount.h>

/*
 * Just to keep nfs_var.h happy.
 */
struct nfs_vattr {
	int	junk;
};

struct nfsvattr {
	struct vattr	na_vattr;
	nfsattrbit_t	na_suppattr;
	u_int32_t	na_mntonfileno;
	u_int64_t	na_filesid[2];
};

#define	na_type		na_vattr.va_type
#define	na_mode		na_vattr.va_mode
#define	na_nlink	na_vattr.va_nlink
#define	na_uid		na_vattr.va_uid
#define	na_gid		na_vattr.va_gid
#define	na_fsid		na_vattr.va_fsid
#define	na_fileid	na_vattr.va_fileid
#define	na_size		na_vattr.va_size
#define	na_blocksize	na_vattr.va_blocksize
#define	na_atime	na_vattr.va_atime
#define	na_mtime	na_vattr.va_mtime
#define	na_ctime	na_vattr.va_ctime
#define	na_gen		na_vattr.va_gen
#define	na_flags	na_vattr.va_flags
#define	na_rdev		na_vattr.va_rdev
#define	na_bytes	na_vattr.va_bytes
#define	na_filerev	na_vattr.va_filerev
#define	na_vaflags	na_vattr.va_vaflags

#include <fs/nfsclient/nfsnode.h>

/*
 * This is the header structure used for the lists, etc. (It has the
 * above record in it.
 */
struct nfsrv_stablefirst {
	LIST_HEAD(, nfsrv_stable) nsf_head;	/* Head of nfsrv_stable list */
	time_t		nsf_eograce;	/* Time grace period ends */
	time_t		*nsf_bootvals;	/* Previous boottime values */
	struct file	*nsf_fp;	/* File table pointer */
	u_char		nsf_flags;	/* NFSNSF_ flags */
	struct nfsf_rec	nsf_rec;	/* and above first record */
};
#define	nsf_lease	nsf_rec.lease
#define	nsf_numboots	nsf_rec.numboots

/* NFSNSF_xxx flags */
#define	NFSNSF_UPDATEDONE	0x01
#define	NFSNSF_GRACEOVER	0x02
#define	NFSNSF_NEEDLOCK		0x04
#define	NFSNSF_EXPIREDCLIENT	0x08
#define	NFSNSF_NOOPENS		0x10
#define	NFSNSF_OK		0x20

/*
 * Maximum number of boot times allowed in record. Although there is
 * really no need for a fixed upper bound, this serves as a sanity check
 * for a corrupted file.
 */
#define	NFSNSF_MAXNUMBOOTS	10000

/*
 * This structure defines the other records in the file. The
 * nst_client array is actually the size of the client string name.
 */
struct nfst_rec {
	u_int16_t	len;
	u_char		flag;
	u_char		client[1];
};
/* and the values for flag */
#define	NFSNST_NEWSTATE	0x1
#define	NFSNST_REVOKE		0x2
#define	NFSNST_GOTSTATE		0x4

/*
 * This structure is linked onto nfsrv_stablefirst for the duration of
 * reclaim.
 */
struct nfsrv_stable {
	LIST_ENTRY(nfsrv_stable) nst_list;
	struct nfsclient	*nst_clp;
	struct nfst_rec		nst_rec;
};
#define	nst_timestamp	nst_rec.timestamp
#define	nst_len		nst_rec.len
#define	nst_flag	nst_rec.flag
#define	nst_client	nst_rec.client

/*
 * At some point the server will run out of kernel storage for
 * state structures. For FreeBSD5.2, this results in a panic
 * kmem_map is full. It happens at well over 1000000 opens plus
 * locks on a PIII-800 with 256Mbytes, so that is where I've set
 * the limit. If your server panics due to too many opens/locks,
 * decrease the size of NFSRV_V4STATELIMIT. If you find the server
 * returning NFS4ERR_RESOURCE a lot and have lots of memory, try
 * increasing it.
 */
#define	NFSRV_V4STATELIMIT	500000	/* Max # of Opens + Locks */

/*
 * The type required differs with BSDen (just the second arg).
 */
void nfsrvd_rcv(struct socket *, void *, int);

/*
 * Macros for handling socket addresses. (Hopefully this makes the code
 * more portable, since I've noticed some 'BSD don't have sockaddrs in
 * mbufs any more.)
 */
#define	NFSSOCKADDR(a, t)	((t)(a))
#define	NFSSOCKADDRALLOC(a) 					\
    do {							\
	MALLOC((a), struct sockaddr *, sizeof (struct sockaddr), \
	    M_SONAME, M_WAITOK); 				\
	NFSBZERO((a), sizeof (struct sockaddr)); 		\
    } while (0)
#define	NFSSOCKADDRSIZE(a, s)		((a)->sa_len = (s))
#define	NFSSOCKADDRFREE(a) 					\
	do { 							\
		if (a) 						\
			FREE((caddr_t)(a), M_SONAME); 		\
	} while (0)

/*
 * These should be defined as a process or thread structure, as required
 * for signal handling, etc.
 */
#define	NFSNEWCRED(c)		(crdup(c))
#define	NFSPROCCRED(p)		((p)->td_ucred)
#define	NFSFREECRED(c)		(crfree(c))
#define	NFSUIOPROC(u, p)	((u)->uio_td = NULL)
#define	NFSPROCP(p)		((p)->td_proc)

/*
 * Define these so that cn_hash and its length is ignored.
 */
#define	NFSCNHASHZERO(c)
#define	NFSCNHASH(c, v)
#define	NCHNAMLEN	9999999

/*
 * Define these to use the time of day clock.
 */
#define	NFSGETTIME(t)		(getmicrotime(t))
#define	NFSGETNANOTIME(t)	(getnanotime(t))

/*
 * These macros are defined to initialize and set the timer routine.
 */
#define	NFS_TIMERINIT \
	newnfs_timer(NULL)

/*
 * Handle SMP stuff:
 */
#define	NFSSTATESPINLOCK	extern struct mtx nfs_state_mutex
#define	NFSLOCKSTATE()		mtx_lock(&nfs_state_mutex)
#define	NFSUNLOCKSTATE()	mtx_unlock(&nfs_state_mutex)
#define	NFSSTATEMUTEXPTR	(&nfs_state_mutex)
#define	NFSREQSPINLOCK		extern struct mtx nfs_req_mutex
#define	NFSLOCKREQ()		mtx_lock(&nfs_req_mutex)
#define	NFSUNLOCKREQ()		mtx_unlock(&nfs_req_mutex)
#define	NFSCACHEMUTEX		extern struct mtx nfs_cache_mutex
#define	NFSCACHEMUTEXPTR	(&nfs_cache_mutex)
#define	NFSLOCKCACHE()		mtx_lock(&nfs_cache_mutex)
#define	NFSUNLOCKCACHE()	mtx_unlock(&nfs_cache_mutex)
#define	NFSCACHELOCKREQUIRED()	mtx_assert(&nfs_cache_mutex, MA_OWNED)
#define	NFSSOCKMUTEX		extern struct mtx nfs_slock_mutex
#define	NFSSOCKMUTEXPTR		(&nfs_slock_mutex)
#define	NFSLOCKSOCK()		mtx_lock(&nfs_slock_mutex)
#define	NFSUNLOCKSOCK()		mtx_unlock(&nfs_slock_mutex)
#define	NFSNAMEIDMUTEX		extern struct mtx nfs_nameid_mutex
#define	NFSLOCKNAMEID()		mtx_lock(&nfs_nameid_mutex)
#define	NFSUNLOCKNAMEID()	mtx_unlock(&nfs_nameid_mutex)
#define	NFSNAMEIDREQUIRED()	mtx_assert(&nfs_nameid_mutex, MA_OWNED)
#define	NFSCLSTATEMUTEX		extern struct mtx nfs_clstate_mutex
#define	NFSCLSTATEMUTEXPTR	(&nfs_clstate_mutex)
#define	NFSLOCKCLSTATE()	mtx_lock(&nfs_clstate_mutex)
#define	NFSUNLOCKCLSTATE()	mtx_unlock(&nfs_clstate_mutex)
#define	NFSDLOCKMUTEX		extern struct mtx newnfsd_mtx
#define	NFSDLOCKMUTEXPTR	(&newnfsd_mtx)
#define	NFSD_LOCK()		mtx_lock(&newnfsd_mtx)
#define	NFSD_UNLOCK()		mtx_unlock(&newnfsd_mtx)
#define	NFSD_LOCK_ASSERT()	mtx_assert(&newnfsd_mtx, MA_OWNED)
#define	NFSD_UNLOCK_ASSERT()	mtx_assert(&newnfsd_mtx, MA_NOTOWNED)
#define	NFSV4ROOTLOCKMUTEX	extern struct mtx nfs_v4root_mutex
#define	NFSV4ROOTLOCKMUTEXPTR	(&nfs_v4root_mutex)
#define	NFSLOCKV4ROOTMUTEX()	mtx_lock(&nfs_v4root_mutex)
#define	NFSUNLOCKV4ROOTMUTEX()	mtx_unlock(&nfs_v4root_mutex)
#define	NFSLOCKNODE(n)		mtx_lock(&((n)->n_mtx))
#define	NFSUNLOCKNODE(n)	mtx_unlock(&((n)->n_mtx))
#define	NFSLOCKMNT(m)		mtx_lock(&((m)->nm_mtx))
#define	NFSUNLOCKMNT(m)		mtx_unlock(&((m)->nm_mtx))
#define	NFSLOCKREQUEST(r)	mtx_lock(&((r)->r_mtx))
#define	NFSUNLOCKREQUEST(r)	mtx_unlock(&((r)->r_mtx))
#define	NFSPROCLISTLOCK()	sx_slock(&allproc_lock)
#define	NFSPROCLISTUNLOCK()	sx_sunlock(&allproc_lock)
#define	NFSLOCKSOCKREQ(r)	mtx_lock(&((r)->nr_mtx))
#define	NFSUNLOCKSOCKREQ(r)	mtx_unlock(&((r)->nr_mtx))

/*
 * Use these macros to initialize/free a mutex.
 */
#define	NFSINITSOCKMUTEX(m)	mtx_init((m), "nfssock", NULL, MTX_DEF)
#define	NFSFREEMUTEX(m)		mtx_destroy((m))

int nfsmsleep(void *, void *, int, const char *, struct timespec *);

/*
 * And weird vm stuff in the nfs server.
 */
#define	PDIRUNLOCK	0x0
#define	MAX_COMMIT_COUNT	(1024 * 1024)

/*
 * Define these to handle the type of va_rdev.
 */
#define	NFSMAKEDEV(m, n)	makedev((m), (n))
#define	NFSMAJOR(d)		major(d)
#define	NFSMINOR(d)		minor(d)

/*
 * Define this to be the macro that returns the minimum size required
 * for a directory entry.
 */
#define	DIRENT_SIZE(dp)		GENERIC_DIRSIZ(dp)

/*
 * The vnode tag for nfsv4root.
 */
#define	VT_NFSV4ROOT		"nfsv4root"

/*
 * Define whatever it takes to do a vn_rdwr().
 */
#define	NFSD_RDWR(r, v, b, l, o, s, i, c, a, p) \
	vn_rdwr((r), (v), (b), (l), (o), (s), (i), (c), NULL, (a), (p))

/*
 * Macros for handling memory for different BSDen.
 * NFSBCOPY(src, dst, len) - copies len bytes, non-overlapping
 * NFSOVBCOPY(src, dst, len) - ditto, but data areas might overlap
 * NFSBCMP(cp1, cp2, len) - compare len bytes, return 0 if same
 * NFSBZERO(cp, len) - set len bytes to 0x0
 */
#define	NFSBCOPY(s, d, l)	bcopy((s), (d), (l))
#define	NFSOVBCOPY(s, d, l)	ovbcopy((s), (d), (l))
#define	NFSBCMP(s, d, l)	bcmp((s), (d), (l))
#define	NFSBZERO(s, l)		bzero((s), (l))

/*
 * Some queue.h files don't have these dfined in them.
 */
#define	LIST_END(head)		NULL
#define	SLIST_END(head)		NULL
#define	TAILQ_END(head)		NULL

/*
 * This must be defined to be a global variable the increments once
 * per second, but never stops or goes backwards, even when a "date"
 * command changes the tod clock. It is used for delta times for
 * leases, etc.
 */
#define	NFSD_MONOSEC		time_uptime

/*
 * Declare the malloc types.
 */
MALLOC_DECLARE(M_NEWNFSRVCACHE);
MALLOC_DECLARE(M_NEWNFSDCLIENT);
MALLOC_DECLARE(M_NEWNFSDSTATE);
MALLOC_DECLARE(M_NEWNFSDLOCK);
MALLOC_DECLARE(M_NEWNFSDLOCKFILE);
MALLOC_DECLARE(M_NEWNFSSTRING);
MALLOC_DECLARE(M_NEWNFSUSERGROUP);
MALLOC_DECLARE(M_NEWNFSDREQ);
MALLOC_DECLARE(M_NEWNFSFH);
MALLOC_DECLARE(M_NEWNFSCLOWNER);
MALLOC_DECLARE(M_NEWNFSCLOPEN);
MALLOC_DECLARE(M_NEWNFSCLDELEG);
MALLOC_DECLARE(M_NEWNFSCLCLIENT);
MALLOC_DECLARE(M_NEWNFSCLLOCKOWNER);
MALLOC_DECLARE(M_NEWNFSCLLOCK);
MALLOC_DECLARE(M_NEWNFSDIROFF);
MALLOC_DECLARE(M_NEWNFSV4NODE);
MALLOC_DECLARE(M_NEWNFSDIRECTIO);
MALLOC_DECLARE(M_NEWNFSMNT);
MALLOC_DECLARE(M_NEWNFSDROLLBACK);
#define	M_NFSRVCACHE	M_NEWNFSRVCACHE
#define	M_NFSDCLIENT	M_NEWNFSDCLIENT
#define	M_NFSDSTATE	M_NEWNFSDSTATE
#define	M_NFSDLOCK	M_NEWNFSDLOCK
#define	M_NFSDLOCKFILE	M_NEWNFSDLOCKFILE
#define	M_NFSSTRING	M_NEWNFSSTRING
#define	M_NFSUSERGROUP	M_NEWNFSUSERGROUP
#define	M_NFSDREQ	M_NEWNFSDREQ
#define	M_NFSFH		M_NEWNFSFH
#define	M_NFSCLOWNER	M_NEWNFSCLOWNER
#define	M_NFSCLOPEN	M_NEWNFSCLOPEN
#define	M_NFSCLDELEG	M_NEWNFSCLDELEG
#define	M_NFSCLCLIENT	M_NEWNFSCLCLIENT
#define	M_NFSCLLOCKOWNER M_NEWNFSCLLOCKOWNER
#define	M_NFSCLLOCK	M_NEWNFSCLLOCK
#define	M_NFSDIROFF	M_NEWNFSDIROFF
#define	M_NFSV4NODE	M_NEWNFSV4NODE
#define	M_NFSDIRECTIO	M_NEWNFSDIRECTIO
#define	M_NFSDROLLBACK	M_NEWNFSDROLLBACK

#define	NFSINT_SIGMASK(set) 						\
	(SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) ||	\
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) ||	\
	 SIGISMEMBER(set, SIGQUIT))

/*
 * Convert a quota block count to byte count.
 */
#define	NFSQUOTABLKTOBYTE(q, b)	(q) *= (b)

/*
 * Define this as the largest file size supported. (It should probably
 * be available via a VFS_xxx Op, but it isn't.
 */
#define	NFSRV_MAXFILESIZE	((u_int64_t)0x800000000000)

/*
 * Set this macro to index() or strchr(), whichever is supported.
 */
#define	STRCHR(s, c)	index((s), (c))

/*
 * Set the n_time in the client write rpc, as required.
 */
#define	NFSWRITERPC_SETTIME(w, n, v4)					\
	do {								\
		if (w) {						\
			(n)->n_mtime = (n)->n_vattr.na_vattr.va_mtime; \
			if (v4)						\
			    (n)->n_change = (n)->n_vattr.na_vattr.va_filerev; \
		}							\
	} while (0)

/*
 * Fake value, just to make the client work.
 */
#define	NFS_LATTR_NOSHRINK	1

/*
 * Prototypes for functions where the arguments vary for different ports.
 */
int nfscl_loadattrcache(struct vnode **, struct nfsvattr *, void *, void *,
    int, int);
void newnfs_realign(struct mbuf **);

/*
 * If the port runs on an SMP box that can enforce Atomic ops with low
 * overheads, define these as atomic increments/decrements. If not,
 * don't worry about it, since these are used for stats that can be
 * "out by one" without disastrous consequences.
 */
#define	NFSINCRGLOBAL(a)	((a)++)

/*
 * Assorted funky stuff to make things work under Darwin8.
 */
/*
 * These macros checks for a field in vattr being set.
 */
#define	NFSATTRISSET(t, v, a)	((v)->a != (t)VNOVAL)
#define	NFSATTRISSETTIME(v, a)	((v)->a.tv_sec != VNOVAL)

/*
 * Manipulate mount flags.
 */
#define	NFSSTA_HASWRITEVERF	0x00040000  /* Has write verifier */
#define	NFSSTA_GOTFSINFO	0x00100000  /* Got the fsinfo */
#define	NFSSTA_TIMEO		0x10000000  /* Experiencing a timeout */
#define	NFSSTA_LOCKTIMEO	0x20000000  /* Experiencing a lockd timeout */
#define	NFSSTA_HASSETFSID	0x40000000  /* Has set the fsid */

#define	NFSHASNFSV3(n)		((n)->nm_flag & NFSMNT_NFSV3)
#define	NFSHASNFSV4(n)		((n)->nm_flag & NFSMNT_NFSV4)
#define	NFSHASNFSV3OR4(n)	((n)->nm_flag & (NFSMNT_NFSV3 | NFSMNT_NFSV4))
#define	NFSHASGOTFSINFO(n)	((n)->nm_state & NFSSTA_GOTFSINFO)
#define	NFSHASHASSETFSID(n)	((n)->nm_state & NFSSTA_HASSETFSID)
#define	NFSHASSTRICT3530(n)	((n)->nm_flag & NFSMNT_STRICT3530)
#define	NFSHASWRITEVERF(n)	((n)->nm_state & NFSSTA_HASWRITEVERF)
#define	NFSHASINT(n)		((n)->nm_flag & NFSMNT_INT)
#define	NFSHASSOFT(n)		((n)->nm_flag & NFSMNT_SOFT)
#define	NFSHASINTORSOFT(n)	((n)->nm_flag & (NFSMNT_INT | NFSMNT_SOFT))
#define	NFSHASDUMBTIMR(n)	((n)->nm_flag & NFSMNT_DUMBTIMR)
#define	NFSHASNOCONN(n)		((n)->nm_flag & NFSMNT_MNTD)
#define	NFSHASKERB(n)		((n)->nm_flag & NFSMNT_KERB)
#define	NFSHASALLGSSNAME(n)	((n)->nm_flag & NFSMNT_ALLGSSNAME)
#define	NFSHASINTEGRITY(n)	((n)->nm_flag & NFSMNT_INTEGRITY)
#define	NFSHASPRIVACY(n)	((n)->nm_flag & NFSMNT_PRIVACY)
#define	NFSSETWRITEVERF(n)	((n)->nm_state |= NFSSTA_HASWRITEVERF)
#define	NFSSETHASSETFSID(n)	((n)->nm_state |= NFSSTA_HASSETFSID)

/*
 * Gets the stats field out of the mount structure.
 */
#define	vfs_statfs(m)	(&((m)->mnt_stat))

/*
 * Set boottime.
 */
#define	NFSSETBOOTTIME(b)	((b) = boottime)

/*
 * The size of directory blocks in the buffer cache.
 * MUST BE in the range of PAGE_SIZE <= NFS_DIRBLKSIZ <= MAXBSIZE!!
 */
#define	NFS_DIRBLKSIZ	(16 * DIRBLKSIZ) /* Must be a multiple of DIRBLKSIZ */

/*
 * Define these macros to access mnt_flag fields.
 */
#define	NFSMNT_RDONLY(m)	((m)->mnt_flag & MNT_RDONLY)
#endif	/* _KERNEL */

/*
 * Define a structure similar to ufs_args for use in exporting the V4 root.
 */
struct nfsex_args {
	char	*fspec;
	struct export_args	export;
};

/*
 * These export flags should be defined, but there are no bits left.
 * Maybe a separate mnt_exflag field could be added or the mnt_flag
 * field increased to 64 bits?
 */
#ifndef	MNT_EXSTRICTACCESS
#define	MNT_EXSTRICTACCESS	0x0
#endif
#ifndef MNT_EXV4ONLY
#define	MNT_EXV4ONLY		0x0
#endif

#ifdef _KERNEL
/*
 * Define this to invalidate the attribute cache for the nfs node.
 */
#define	NFSINVALATTRCACHE(n)	((n)->n_attrstamp = 0)

/* Used for FreeBSD only */
void nfsd_mntinit(void);

/*
 * Define these for vnode lock/unlock ops.
 *
 * These are good abstractions to macro out, so that they can be added to
 * later, for debugging or stats, etc.
 */
#define	NFSVOPLOCK(v, f)	vn_lock((v), (f))
#define	NFSVOPUNLOCK(v, f)	VOP_UNLOCK((v), (f))
#define	NFSVOPISLOCKED(v)	VOP_ISLOCKED((v))

/*
 * Define ncl_hash().
 */
#define	ncl_hash(f, l)	(fnv_32_buf((f), (l), FNV1_32_INIT))

int newnfs_iosize(struct nfsmount *);

#ifdef NFS_DEBUG

extern int nfs_debug;
#define	NFS_DEBUG_ASYNCIO	1 /* asynchronous i/o */
#define	NFS_DEBUG_WG		2 /* server write gathering */
#define	NFS_DEBUG_RC		4 /* server request caching */

#define	NFS_DPF(cat, args)					\
	do {							\
		if (nfs_debug & NFS_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define	NFS_DPF(cat, args)

#endif

int newnfs_vncmpf(struct vnode *, void *);

#ifndef NFS_MINDIRATTRTIMO
#define	NFS_MINDIRATTRTIMO 3		/* VDIR attrib cache timeout in sec */
#endif
#ifndef NFS_MAXDIRATTRTIMO
#define	NFS_MAXDIRATTRTIMO 60
#endif

/*
 * Nfs outstanding request list element
 */
struct nfsreq {
	TAILQ_ENTRY(nfsreq) r_chain;
	u_int32_t	r_flags;	/* flags on request, see below */
	struct nfsmount *r_nmp;		/* Client mnt ptr */
	struct mtx	r_mtx;		/* Mutex lock for this structure */
};

#ifndef NFS_MAXBSIZE
#define	NFS_MAXBSIZE	MAXBSIZE
#endif

/*
 * This macro checks to see if issuing of delegations is allowed for this
 * vnode.
 */
#ifdef VV_DISABLEDELEG
#define	NFSVNO_DELEGOK(v)						\
	((v) == NULL || ((v)->v_vflag & VV_DISABLEDELEG) == 0)
#else
#define	NFSVNO_DELEGOK(v)	(1)
#endif

/*
 * Define this as the flags argument for msleep() when catching signals
 * while holding a resource that other threads would block for, such as
 * a vnode lock.
 */
#define	NFS_PCATCH	(PCATCH | PBDRY)

#endif	/* _KERNEL */

#endif	/* _NFS_NFSPORT_H */
