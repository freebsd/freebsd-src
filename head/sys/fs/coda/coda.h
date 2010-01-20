/*-
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/coda/coda.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 * $FreeBSD$
 * 
 */


/*
 *
 * Based on cfs.h from Mach, but revamped for increased simplicity.
 * Linux modifications by Peter Braam, Aug 1996
 */

#ifndef _CODA_HEADER_
#define _CODA_HEADER_

#include "opt_coda.h"	/* for CODA_COMPAT_5 option */

/* Avoid CODA_COMPAT_5 redefinition in coda5 module */
#if defined (CODA5_MODULE) && !defined(CODA_COMPAT_5)
#define CODA_COMPAT_5
#endif

/* Catch new _KERNEL defn for NetBSD */
#ifdef __NetBSD__
#include <sys/types.h>
#endif 

#ifndef CODA_MAXSYMLINKS
#define CODA_MAXSYMLINKS 10
#endif

#if defined(DJGPP) || defined(__CYGWIN32__)
#ifdef _KERNEL
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef u_long ino_t;
typedef u_long struct cdev *;
typedef void * caddr_t;
#ifdef DOS
typedef unsigned __int64 u_quad_t;
#else 
typedef unsigned long long u_quad_t;
#endif

#define inline

struct timespec {
        long       ts_sec;
        long       ts_nsec;
};
#else  /* DJGPP but not _KERNEL */
#include <sys/types.h>
#include <sys/time.h>
typedef unsigned long long u_quad_t;
#endif /* !_KERNEL */
#endif /* !DJGPP */


#if defined(__linux__)
#define cdev_t u_quad_t
#if !defined(_UQUAD_T_) && (!defined(__GLIBC__) || __GLIBC__ < 2)
#define _UQUAD_T_ 1
typedef unsigned long long u_quad_t;
#endif
#else
#define cdev_t dev_t
#endif

#ifdef __CYGWIN32__
typedef unsigned char u_int8_t;
struct timespec {
        time_t  tv_sec;         /* seconds */
        long    tv_nsec;        /* nanoseconds */
};
#endif


/*
 * Cfs constants
 */
#define CODA_MAXNAMLEN   255
#define CODA_MAXPATHLEN  1024
#define CODA_MAXSYMLINK  10

/* these are Coda's version of O_RDONLY etc combinations
 * to deal with VFS open modes
 */
#define	C_O_READ	0x001
#define	C_O_WRITE       0x002
#define C_O_TRUNC       0x010
#define C_O_EXCL	0x100
#define C_O_CREAT	0x200

/* these are to find mode bits in Venus */ 
#define C_M_READ  00400
#define C_M_WRITE 00200

/* for access Venus will use */
#define C_A_C_OK    8               /* Test for writing upon create.  */
#define C_A_R_OK    4               /* Test for read permission.  */
#define C_A_W_OK    2               /* Test for write permission.  */
#define C_A_X_OK    1               /* Test for execute permission.  */
#define C_A_F_OK    0               /* Test for existence.  */



#ifndef _VENUS_DIRENT_T_
#define _VENUS_DIRENT_T_ 1
struct venus_dirent {
        unsigned long	d_fileno;		/* file number of entry */
        unsigned short	d_reclen;		/* length of this record */
        char 		d_type;			/* file type, see below */
        char		d_namlen;		/* length of string in d_name */
        char		d_name[CODA_MAXNAMLEN + 1];/* name must be no longer than this */
};
#undef DIRSIZ
#define DIRSIZ(dp)      ((sizeof (struct venus_dirent) - (CODA_MAXNAMLEN+1)) + \
                         (((dp)->d_namlen+1 + 3) &~ 3))

/*
 * File types
 */
#define	CDT_UNKNOWN	 0
#define	CDT_FIFO	 1
#define	CDT_CHR		 2
#define	CDT_DIR		 4
#define	CDT_BLK		 6
#define	CDT_REG		 8
#define	CDT_LNK		10
#define	CDT_SOCK	12
#define	CDT_WHT		14

/*
 * Convert between stat structure types and directory types.
 */
#define	IFTOCDT(mode)	(((mode) & 0170000) >> 12)
#define	CDTTOIF(dirtype)	((dirtype) << 12)

#endif

#ifdef CODA_COMPAT_5

typedef struct {
    u_long Volume;
    u_long Vnode;
    u_long Unique;      
} CodaFid;

static __inline__ ino_t coda_f2i(CodaFid *fid)
{
	if (!fid) return 0;
	return (fid->Unique + (fid->Vnode<<10) + (fid->Volume<<20));
}

static __inline__ char * coda_f2s(CodaFid *fid)
{
  static char fid_str [35];
  snprintf (fid_str, 35, "[%lx.%lx.%lx]", fid->Volume,
	    fid->Vnode, fid->Unique);
  return fid_str;
}
 
static __inline__ int coda_fid_eq (CodaFid *fid1, CodaFid *fid2)
{
  return (fid1->Volume == fid2->Volume &&
	  fid1->Vnode == fid2->Vnode &&
	  fid1->Unique == fid2->Unique);
}
  
struct coda_cred {
    u_int32_t cr_uid, cr_euid, cr_suid, cr_fsuid; /* Real, efftve, set, fs uid*/
    u_int32_t cr_groupid,     cr_egid, cr_sgid, cr_fsgid; /* same for groups */
};

#else	/* CODA_COMPAT_5 */

typedef struct  {
	u_int32_t opaque[4];
} CodaFid;

static __inline__ ino_t  coda_f2i(CodaFid *fid)
{
    if ( ! fid ) 
	return 0; 
    return (fid->opaque[3] ^ (fid->opaque[2]<<10) ^ (fid->opaque[1]<<20) ^ fid->opaque[0]);
}
	
static __inline__ char * coda_f2s(CodaFid *fid)
 {
     static char fid_str [35];
     snprintf (fid_str, 35, "[%x.%x.%x.%x]", fid->opaque[0],
	       fid->opaque[1], fid->opaque[2], fid->opaque[3]);
     return fid_str;
 }

static __inline__ int coda_fid_eq (CodaFid *fid1, CodaFid *fid2)
{
  return (fid1->opaque[0] == fid2->opaque[0] &&
	  fid1->opaque[1] == fid2->opaque[1] &&
	  fid1->opaque[2] == fid2->opaque[2] &&
	  fid1->opaque[3] == fid2->opaque[3]);
}

#endif	/* CODA_COMPAT_5 */

#ifndef _VENUS_VATTR_T_
#define _VENUS_VATTR_T_
/*
 * Vnode types.  VNON means no type.
 */
enum coda_vtype	{ C_VNON, C_VREG, C_VDIR, C_VBLK, C_VCHR, C_VLNK, C_VSOCK, C_VFIFO, C_VBAD };

struct coda_vattr {
	int     	va_type;	/* vnode type (for create) */
	u_short		va_mode;	/* files access mode and type */
	short		va_nlink;	/* number of references to file */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	cdev_t	        va_rdev;	/* device special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
};

#endif 

/* structure used by CODA_STATFS for getting cache information from venus */
struct coda_statfs {
    int32_t f_blocks;
    int32_t f_bfree;
    int32_t f_bavail;
    int32_t f_files;
    int32_t f_ffree;
};

/*
 * Kernel <--> Venus communications.
 */

#define CODA_ROOT	2
#define CODA_OPEN_BY_FD	3
#define CODA_OPEN	4
#define CODA_CLOSE	5
#define CODA_IOCTL	6
#define CODA_GETATTR	7
#define CODA_SETATTR	8
#define CODA_ACCESS	9
#define CODA_LOOKUP	10
#define CODA_CREATE	11
#define CODA_REMOVE	12
#define CODA_LINK	13
#define CODA_RENAME	14
#define CODA_MKDIR	15
#define CODA_RMDIR	16
#define CODA_READDIR	17
#define CODA_SYMLINK	18
#define CODA_READLINK	19
#define CODA_FSYNC	20
#define CODA_INACTIVE	21
#define CODA_VGET	22
#define CODA_SIGNAL	23
#define CODA_REPLACE	24
#define CODA_FLUSH       25
#define CODA_PURGEUSER   26
#define CODA_ZAPFILE     27
#define CODA_ZAPDIR      28
#define CODA_PURGEFID    30
#define CODA_OPEN_BY_PATH 31
#define CODA_RESOLVE     32
#define CODA_REINTEGRATE 33
#define CODA_STATFS	 34
#define CODA_NCALLS 35

#define DOWNCALL(opcode) (opcode >= CODA_REPLACE && opcode <= CODA_PURGEFID)

#define VC_MAXDATASIZE	    8192
#define VC_MAXMSGSIZE      sizeof(union inputArgs)+sizeof(union outputArgs) +\
                            VC_MAXDATASIZE  

#define CIOC_KERNEL_VERSION _IOWR('c', 10, sizeof (int))
#if	0
	/* don't care about kernel version number */
#define CODA_KERNEL_VERSION 0
	/* The old venus 4.6 compatible interface */
#define CODA_KERNEL_VERSION 1
#endif  /* realms/cells */
#ifdef CODA_COMPAT_5
	/* venus_lookup gets an extra parameter to aid windows.*/
#define CODA_KERNEL_VERSION 2
#else
	/* 128-bit fids for realms */
#define CODA_KERNEL_VERSION 3 
#endif

/*
 *        Venus <-> Coda  RPC arguments
 */
#ifdef CODA_COMPAT_5
struct coda_in_hdr {
    unsigned long opcode;
    unsigned long unique;           /* Keep multiple outstanding msgs distinct */
    u_short pid;                    /* Common to all */
    u_short pgid;                   /* Common to all */
    u_short sid;                    /* Common to all */
    struct coda_cred cred;          /* Common to all */    
};
#else
struct coda_in_hdr {
    u_int32_t opcode;
    u_int32_t unique;	    /* Keep multiple outstanding msgs distinct */
    pid_t pid;		    /* Common to all */
    pid_t pgid;		    /* Common to all */
    uid_t uid;		    /* Common to all */
};
#endif

/* Really important that opcode and unique are 1st two fields! */
struct coda_out_hdr {
    unsigned long opcode;
    unsigned long unique;	
    unsigned long result;
};

/* coda_root: NO_IN */
struct coda_root_out {
    struct coda_out_hdr oh;
    CodaFid Fid;
};

struct coda_root_in {
    struct coda_in_hdr in;
};

/* coda_sync: */
/* Nothing needed for coda_sync */

/* coda_open: */
struct coda_open_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int	flags;
};

struct coda_open_out {
    struct coda_out_hdr oh;
    cdev_t	dev;
    ino_t	inode;
};


/* coda_close: */
struct coda_close_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int	flags;
};

struct coda_close_out {
    struct coda_out_hdr out;
};

/* coda_ioctl: */
struct coda_ioctl_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
    int	cmd;
    int	len;
    int	rwflag;
    char *data;			/* Place holder for data. */
};

struct coda_ioctl_out {
    struct coda_out_hdr oh;
    int	len;
    caddr_t	data;		/* Place holder for data. */
};


/* coda_getattr: */
struct coda_getattr_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
};

struct coda_getattr_out {
    struct coda_out_hdr oh;
    struct coda_vattr attr;
};


/* coda_setattr: NO_OUT */
struct coda_setattr_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
    struct coda_vattr attr;
};

struct coda_setattr_out {
    struct coda_out_hdr out;
};

/* coda_access: NO_OUT */
struct coda_access_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int	flags;
};

struct coda_access_out {
    struct coda_out_hdr out;
};


/* lookup flags */
#define CLU_CASE_SENSITIVE     0x01
#define CLU_CASE_INSENSITIVE   0x02

/* coda_lookup: */
struct  coda_lookup_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int         name;		/* Place holder for data. */
    int         flags;	
};

struct coda_lookup_out {
    struct coda_out_hdr oh;
    CodaFid Fid;
    int	vtype;
};


/* coda_create: */
struct coda_create_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
    struct coda_vattr attr;
    int excl;
    int mode;
    int 	name;		/* Place holder for data. */
};

struct coda_create_out {
    struct coda_out_hdr oh;
    CodaFid Fid;
    struct coda_vattr attr;
};


/* coda_remove: NO_OUT */
struct coda_remove_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int name;		/* Place holder for data. */
};

struct coda_remove_out {
    struct coda_out_hdr out;
};

/* coda_link: NO_OUT */
struct coda_link_in {
    struct coda_in_hdr ih;
    CodaFid sourceFid;          /* cnode to link *to* */
    CodaFid destFid;            /* Directory in which to place link */
    int tname;		/* Place holder for data. */
};

struct coda_link_out {
    struct coda_out_hdr out;
};


/* coda_rename: NO_OUT */
struct coda_rename_in {
    struct coda_in_hdr ih;
    CodaFid	sourceFid;
    int 	srcname;
    CodaFid destFid;
    int 	destname;
};

struct coda_rename_out {
    struct coda_out_hdr out;
};

/* coda_mkdir: */
struct coda_mkdir_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    struct coda_vattr attr;
    int	   name;		/* Place holder for data. */
};

struct coda_mkdir_out {
    struct coda_out_hdr oh;
    CodaFid Fid;
    struct coda_vattr attr;
};


/* coda_rmdir: NO_OUT */
struct coda_rmdir_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int name;		/* Place holder for data. */
};

struct coda_rmdir_out {
    struct coda_out_hdr out;
};

/* coda_readdir: */
struct coda_readdir_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int	count;
    int	offset;
};

struct coda_readdir_out {
    struct coda_out_hdr oh;
    int	size;
    caddr_t	data;		/* Place holder for data. */
};

/* coda_symlink: NO_OUT */
struct coda_symlink_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;          /* Directory to put symlink in */
    int srcname;
    struct coda_vattr attr;
    int tname;
};

struct coda_symlink_out {
    struct coda_out_hdr out;
};

/* coda_readlink: */
struct coda_readlink_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
};

struct coda_readlink_out {
    struct coda_out_hdr oh;
    int	count;
    caddr_t	data;		/* Place holder for data. */
};


/* coda_fsync: NO_OUT */
struct coda_fsync_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
};

struct coda_fsync_out {
    struct coda_out_hdr out;
};

/* coda_inactive: NO_OUT */
struct coda_inactive_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
};

/* coda_vget: */
struct coda_vget_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
};

struct coda_vget_out {
    struct coda_out_hdr oh;
    CodaFid Fid;
    int	vtype;
};


/* CODA_SIGNAL is out-of-band, doesn't need data. */
/* CODA_INVALIDATE is a venus->kernel call */
/* CODA_FLUSH is a venus->kernel call */

/* coda_purgeuser: */
/* CODA_PURGEUSER is a venus->kernel call */
struct coda_purgeuser_out {
    struct coda_out_hdr oh;
#ifdef CODA_COMPAT_5
    struct coda_cred cred;
#else
    uid_t uid;
#endif
};

/* coda_zapfile: */
/* CODA_ZAPFILE is a venus->kernel call */
struct coda_zapfile_out {  
    struct coda_out_hdr oh;
    CodaFid Fid;
};

/* coda_zapdir: */
/* CODA_ZAPDIR is a venus->kernel call */	
struct coda_zapdir_out {	  
    struct coda_out_hdr oh;
    CodaFid Fid;
};

/* coda_zapnode: */
/* CODA_ZAPVNODE is a venus->kernel call */	
struct coda_zapvnode_out { 
    struct coda_out_hdr oh;
#ifdef CODA_COMPAT_5
    struct coda_cred cred;
#endif
    CodaFid Fid;
};

/* coda_purgefid: */
/* CODA_PURGEFID is a venus->kernel call */	
struct coda_purgefid_out { 
    struct coda_out_hdr oh;
    CodaFid Fid;
};

/* coda_replace: */
/* CODA_REPLACE is a venus->kernel call */	
struct coda_replace_out { /* coda_replace is a venus->kernel call */
     struct coda_out_hdr oh;
    CodaFid NewFid;
    CodaFid OldFid;
};

/* coda_open_by_fd: */
struct coda_open_by_fd_in {
    struct coda_in_hdr ih;
    CodaFid Fid;
    int	flags;
};

struct coda_open_by_fd_out {
    struct coda_out_hdr oh;
    int fd;
    struct vnode *vp;
};

/* coda_open_by_path: */
struct coda_open_by_path_in {
    struct coda_in_hdr ih;
    CodaFid	Fid;
    int	flags;
};

struct coda_open_by_path_out {
    struct coda_out_hdr oh;
    int path;
};

/* coda_statfs: NO_IN */
struct coda_statfs_in {
    struct coda_in_hdr ih;
};

struct coda_statfs_out {
    struct coda_out_hdr oh;
    struct coda_statfs stat;
};

/* 
 * Occasionally, we don't cache the fid returned by CODA_LOOKUP. 
 * For instance, if the fid is inconsistent. 
 * This case is handled by setting the top bit of the type result parameter.
 */
#define CODA_NOCACHE          0x80000000

union inputArgs {
    struct coda_in_hdr ih;		/* NB: every struct below begins with an ih */
    struct coda_open_in coda_open;
    struct coda_close_in coda_close;
    struct coda_ioctl_in coda_ioctl;
    struct coda_getattr_in coda_getattr;
    struct coda_setattr_in coda_setattr;
    struct coda_access_in coda_access;
    struct coda_lookup_in coda_lookup;
    struct coda_create_in coda_create;
    struct coda_remove_in coda_remove;
    struct coda_link_in coda_link;
    struct coda_rename_in coda_rename;
    struct coda_mkdir_in coda_mkdir;
    struct coda_rmdir_in coda_rmdir;
    struct coda_readdir_in coda_readdir;
    struct coda_symlink_in coda_symlink;
    struct coda_readlink_in coda_readlink;
    struct coda_fsync_in coda_fsync;
    struct coda_vget_in coda_vget;
    struct coda_open_by_fd_in coda_open_by_fd;
    struct coda_open_by_path_in coda_open_by_path;
    struct coda_statfs_in coda_statfs;
};

union outputArgs {
    struct coda_out_hdr oh;		/* NB: every struct below begins with an oh */
    struct coda_root_out coda_root;
    struct coda_open_out coda_open;
    struct coda_ioctl_out coda_ioctl;
    struct coda_getattr_out coda_getattr;
    struct coda_lookup_out coda_lookup;
    struct coda_create_out coda_create;
    struct coda_mkdir_out coda_mkdir;
    struct coda_readdir_out coda_readdir;
    struct coda_readlink_out coda_readlink;
    struct coda_vget_out coda_vget;
    struct coda_purgeuser_out coda_purgeuser;
    struct coda_zapfile_out coda_zapfile;
    struct coda_zapdir_out coda_zapdir;
    struct coda_zapvnode_out coda_zapvnode;
    struct coda_purgefid_out coda_purgefid;
    struct coda_replace_out coda_replace;
    struct coda_open_by_fd_out coda_open_by_fd;
    struct coda_open_by_path_out coda_open_by_path;
    struct coda_statfs_out coda_statfs;
};    

union coda_downcalls {
    /* CODA_INVALIDATE is a venus->kernel call */
    /* CODA_FLUSH is a venus->kernel call */
    struct coda_purgeuser_out purgeuser;
    struct coda_zapfile_out zapfile;
    struct coda_zapdir_out zapdir;
    struct coda_zapvnode_out zapvnode;
    struct coda_purgefid_out purgefid;
    struct coda_replace_out replace;
};


/*
 * Used for identifying usage of "Control" and pioctls
 */

#define PIOCPARM_MASK 0x0000ffff
struct ViceIoctl {
        caddr_t in, out;        /* Data to be transferred in, or out */
        short in_size;          /* Size of input buffer <= 2K */
        short out_size;         /* Maximum size of output buffer, <= 2K */
};

#if defined(__CYGWIN32__) || defined(DJGPP)
struct PioctlData {
	unsigned long cmd;
        const char *path;
        int follow;
        struct ViceIoctl vi;
};
#else
struct PioctlData {
        const char *path;
        int follow;
        struct ViceIoctl vi;
};
#endif

#define	CODA_CONTROL		".CONTROL"
#define CODA_CONTROLLEN           8
#define CTL_INO                 -1
#define	CTL_FILE		"/coda/.CONTROL"

#ifdef CODA_COMPAT_5
#define CTL_FID			{ -1, -1, -1 }
#define IS_CTL_FID(fidp)	((fidp)->Volume == -1 &&\
				 (fidp)->Vnode == -1 &&\
				 (fidp)->Unique == -1)
#define INVAL_FID		{ 0, 0, 0 }
#else
#define	CTL_FID			{ { -1, -1, -1, -1 } }
#define	IS_CTL_FID(fidp)	((fidp)->opaque[0] == -1 &&\
				 (fidp)->opaque[1] == -1 &&\
				 (fidp)->opaque[2] == -1 &&\
				 (fidp)->opaque[3] == -1)
#define	INVAL_FID		{ { 0, 0, 0, 0 } }
#endif

/* Data passed to mount */

#define CODA_MOUNT_VERSION 1

struct coda_mount_data {
	int		version;
	int		fd;       /* Opened device */
};

#endif 

