/*
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
 * 	@(#) src/sys/cfs/coda.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 *  $Id: $
 * 
 */

/*
 *
 * Based on cfs.h from Mach, but revamped for increased simplicity.
 * Linux modifications by Peter Braam, Aug 1996
 */

#ifndef _CFS_HEADER_
#define _CFS_HEADER_

/* Catch new _KERNEL defn for NetBSD */
#ifdef __NetBSD__
#include <sys/types.h>
#endif 

#if defined(__linux__) || defined(__CYGWIN32__)
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
#define CFS_MAXNAMLEN   255
#define CFS_MAXPATHLEN  1024
#define CFS_MAXSYMLINK  10

/* these are Coda's version of O_RDONLY etc combinations
 * to deal with VFS open modes
 */
#define	C_O_READ	0x001
#define	C_O_WRITE       0x002
#define C_O_TRUNC       0x010
#define C_O_EXCL	0x100

/* these are to find mode bits in Venus */ 
#define C_M_READ  00400
#define C_M_WRITE 00200

/* for access Venus will use */
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
        char		d_name[CFS_MAXNAMLEN + 1];/* name must be no longer than this */
};
#undef DIRSIZ
#define DIRSIZ(dp)      ((sizeof (struct venus_dirent) - (CFS_MAXNAMLEN+1)) + \
                         (((dp)->d_namlen+1 + 3) &~ 3))

/*
 * File types
 */
#define	CDT_UNKNOWN	 0
#define	CDT_FIFO		 1
#define	CDT_CHR		 2
#define	CDT_DIR		 4
#define	CDT_BLK		 6
#define	CDT_REG		 8
#define	CDT_LNK		10
#define	CDT_SOCK		12
#define	CDT_WHT		14

/*
 * Convert between stat structure types and directory types.
 */
#define	IFTOCDT(mode)	(((mode) & 0170000) >> 12)
#define	CDTTOIF(dirtype)	((dirtype) << 12)

#endif

#ifndef	_FID_T_
#define _FID_T_	1
typedef u_long VolumeId;
typedef u_long VnodeId;
typedef u_long Unique_t;
typedef u_long FileVersion;
#endif 

#ifndef	_VICEFID_T_
#define _VICEFID_T_	1
typedef struct ViceFid {
    VolumeId Volume;
    VnodeId Vnode;
    Unique_t Unique;
} ViceFid;
#endif	/* VICEFID */

#ifdef	__linux__
static inline ino_t coda_f2i(struct ViceFid *fid)
{
      if ( fid ) {
              return (fid->Unique + (fid->Vnode << 10) + (fid->Volume << 20));
      } else { 
              return 0;
      }
}
#endif

#ifndef _VUID_T_
#define _VUID_T_
typedef u_long vuid_t;
typedef u_long vgid_t;
#endif /*_VUID_T_ */

#ifndef _CODACRED_T_
#define _CODACRED_T_
struct coda_cred {
    vuid_t cr_uid, cr_euid, cr_suid, cr_fsuid; /* Real, efftve, set, fs uid*/
#if	defined(__NetBSD__) || defined(__FreeBSD__)
    vgid_t cr_groupid, cr_egid, cr_sgid, cr_fsgid; /* same for groups */
#else
    vgid_t cr_gid,     cr_egid, cr_sgid, cr_fsgid; /* same for groups */
#endif
};
#endif 

#ifndef _VENUS_VATTR_T_
#define _VENUS_VATTR_T_
/*
 * Vnode types.  VNON means no type.
 */
enum coda_vtype	{ C_VNON, C_VREG, C_VDIR, C_VBLK, C_VCHR, C_VLNK, C_VSOCK, C_VFIFO, C_VBAD };

struct coda_vattr {
	enum coda_vtype	va_type;	/* vnode type (for create) */
	u_short		va_mode;	/* files access mode and type */
	short		va_nlink;	/* number of references to file */
	vuid_t		va_uid;		/* owner user id */
	vgid_t		va_gid;		/* owner group id */
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

/*
 * Kernel <--> Venus communications.
 */

#define CFS_ROOT	((u_long) 2)
#define CFS_SYNC	((u_long) 3)
#define CFS_OPEN	((u_long) 4)
#define CFS_CLOSE	((u_long) 5)
#define CFS_IOCTL	((u_long) 6)
#define CFS_GETATTR	((u_long) 7)
#define CFS_SETATTR	((u_long) 8)
#define CFS_ACCESS	((u_long) 9)
#define CFS_LOOKUP	((u_long) 10)
#define CFS_CREATE	((u_long) 11)
#define CFS_REMOVE	((u_long) 12)
#define CFS_LINK	((u_long) 13)
#define CFS_RENAME	((u_long) 14)
#define CFS_MKDIR	((u_long) 15)
#define CFS_RMDIR	((u_long) 16)
#define CFS_READDIR	((u_long) 17)
#define CFS_SYMLINK	((u_long) 18)
#define CFS_READLINK	((u_long) 19)
#define CFS_FSYNC	((u_long) 20)
#define CFS_INACTIVE	((u_long) 21)
#define CFS_VGET	((u_long) 22)
#define CFS_SIGNAL	((u_long) 23)
#define CFS_REPLACE	((u_long) 24)
#define CFS_FLUSH       ((u_long) 25)
#define CFS_PURGEUSER   ((u_long) 26)
#define CFS_ZAPFILE     ((u_long) 27)
#define CFS_ZAPDIR      ((u_long) 28)
#define CFS_ZAPVNODE    ((u_long) 29)
#define CFS_PURGEFID    ((u_long) 30)
#define CFS_NCALLS 31

#define DOWNCALL(opcode) (opcode >= CFS_REPLACE && opcode <= CFS_PURGEFID)

#define VC_MAXDATASIZE	    8192
#define VC_MAXMSGSIZE      sizeof(union inputArgs)+sizeof(union outputArgs) +\
                            VC_MAXDATASIZE  



/*
 *        Venus <-> Coda  RPC arguments
 */
struct cfs_in_hdr {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    u_short pid;		    /* Common to all */
    u_short pgid;		    /* Common to all */
    u_short sid;                    /* Common to all */
    struct coda_cred cred;	    /* Common to all */
};

/* Really important that opcode and unique are 1st two fields! */
struct cfs_out_hdr {
    unsigned long opcode;
    unsigned long unique;	
    unsigned long result;
};

/* cfs_root: NO_IN */
struct cfs_root_out {
    struct cfs_out_hdr oh;
    ViceFid VFid;
};

struct cfs_root_in {
    struct cfs_in_hdr in;
};

/* cfs_sync: */
/* Nothing needed for cfs_sync */

/* cfs_open: */
struct cfs_open_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct cfs_open_out {
    struct cfs_out_hdr oh;
    cdev_t	dev;
    ino_t	inode;
};


/* cfs_close: */
struct cfs_close_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct cfs_close_out {
    struct cfs_out_hdr out;
};

/* cfs_ioctl: */
struct cfs_ioctl_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
    int	cmd;
    int	len;
    int	rwflag;
    char *data;			/* Place holder for data. */
};

struct cfs_ioctl_out {
    struct cfs_out_hdr oh;
    int	len;
    caddr_t	data;		/* Place holder for data. */
};


/* cfs_getattr: */
struct cfs_getattr_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
};

struct cfs_getattr_out {
    struct cfs_out_hdr oh;
    struct coda_vattr attr;
};


/* cfs_setattr: NO_OUT */
struct cfs_setattr_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
    struct coda_vattr attr;
};

struct cfs_setattr_out {
    struct cfs_out_hdr out;
};

/* cfs_access: NO_OUT */
struct cfs_access_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct cfs_access_out {
    struct cfs_out_hdr out;
};

/* cfs_lookup: */
struct  cfs_lookup_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int         name;		/* Place holder for data. */
};

struct cfs_lookup_out {
    struct cfs_out_hdr oh;
    ViceFid VFid;
    int	vtype;
};


/* cfs_create: */
struct cfs_create_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
    struct coda_vattr attr;
    int excl;
    int mode;
    int 	name;		/* Place holder for data. */
};

struct cfs_create_out {
    struct cfs_out_hdr oh;
    ViceFid VFid;
    struct coda_vattr attr;
};


/* cfs_remove: NO_OUT */
struct cfs_remove_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int name;		/* Place holder for data. */
};

struct cfs_remove_out {
    struct cfs_out_hdr out;
};

/* cfs_link: NO_OUT */
struct cfs_link_in {
    struct cfs_in_hdr ih;
    ViceFid sourceFid;          /* cnode to link *to* */
    ViceFid destFid;            /* Directory in which to place link */
    int tname;		/* Place holder for data. */
};

struct cfs_link_out {
    struct cfs_out_hdr out;
};


/* cfs_rename: NO_OUT */
struct cfs_rename_in {
    struct cfs_in_hdr ih;
    ViceFid	sourceFid;
    int 	srcname;
    ViceFid destFid;
    int 	destname;
};

struct cfs_rename_out {
    struct cfs_out_hdr out;
};

/* cfs_mkdir: */
struct cfs_mkdir_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    struct coda_vattr attr;
    int	   name;		/* Place holder for data. */
};

struct cfs_mkdir_out {
    struct cfs_out_hdr oh;
    ViceFid VFid;
    struct coda_vattr attr;
};


/* cfs_rmdir: NO_OUT */
struct cfs_rmdir_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int name;		/* Place holder for data. */
};

struct cfs_rmdir_out {
    struct cfs_out_hdr out;
};

/* cfs_readdir: */
struct cfs_readdir_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int	count;
    int	offset;
};

struct cfs_readdir_out {
    struct cfs_out_hdr oh;
    int	size;
    caddr_t	data;		/* Place holder for data. */
};

/* cfs_symlink: NO_OUT */
struct cfs_symlink_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;          /* Directory to put symlink in */
    int srcname;
    struct coda_vattr attr;
    int tname;
};

struct cfs_symlink_out {
    struct cfs_out_hdr out;
};

/* cfs_readlink: */
struct cfs_readlink_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
};

struct cfs_readlink_out {
    struct cfs_out_hdr oh;
    int	count;
    caddr_t	data;		/* Place holder for data. */
};


/* cfs_fsync: NO_OUT */
struct cfs_fsync_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
};

struct cfs_fsync_out {
    struct cfs_out_hdr out;
};

/* cfs_inactive: NO_OUT */
struct cfs_inactive_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
};

/* cfs_vget: */
struct cfs_vget_in {
    struct cfs_in_hdr ih;
    ViceFid VFid;
};

struct cfs_vget_out {
    struct cfs_out_hdr oh;
    ViceFid VFid;
    int	vtype;
};


/* CFS_SIGNAL is out-of-band, doesn't need data. */
/* CFS_INVALIDATE is a venus->kernel call */
/* CFS_FLUSH is a venus->kernel call */

/* cfs_purgeuser: */
/* CFS_PURGEUSER is a venus->kernel call */
struct cfs_purgeuser_out {
    struct cfs_out_hdr oh;
    struct coda_cred cred;
};

/* cfs_zapfile: */
/* CFS_ZAPFILE is a venus->kernel call */
struct cfs_zapfile_out {  
    struct cfs_out_hdr oh;
    ViceFid CodaFid;
};

/* cfs_zapdir: */
/* CFS_ZAPDIR is a venus->kernel call */	
struct cfs_zapdir_out {	  
    struct cfs_out_hdr oh;
    ViceFid CodaFid;
};

/* cfs_zapnode: */
/* CFS_ZAPVNODE is a venus->kernel call */	
struct cfs_zapvnode_out { 
    struct cfs_out_hdr oh;
    struct coda_cred cred;
    ViceFid VFid;
};

/* cfs_purgefid: */
/* CFS_PURGEFID is a venus->kernel call */	
struct cfs_purgefid_out { 
    struct cfs_out_hdr oh;
    ViceFid CodaFid;
};

/* cfs_rdwr: */
struct cfs_rdwr_in {
    struct cfs_in_hdr ih;
    ViceFid	VFid;
    int	rwflag;
    int	count;
    int	offset;
    int	ioflag;
    caddr_t	data;		/* Place holder for data. */	
};

struct cfs_rdwr_out {
    struct cfs_out_hdr oh;
    int	rwflag;
    int	count;
    caddr_t	data;	/* Place holder for data. */
};


/* cfs_replace: */
/* CFS_REPLACE is a venus->kernel call */	
struct cfs_replace_out { /* cfs_replace is a venus->kernel call */
    struct cfs_out_hdr oh;
    ViceFid NewFid;
    ViceFid OldFid;
};

/* 
 * Occasionally, don't cache the fid returned by CFS_LOOKUP. For instance, if
 * the fid is inconsistent. This case is handled by setting the top bit of the
 * return result parameter.
 */
#define CFS_NOCACHE          0x80000000

union inputArgs {
    struct cfs_in_hdr ih;		/* NB: every struct below begins with an ih */
    struct cfs_open_in cfs_open;
    struct cfs_close_in cfs_close;
    struct cfs_ioctl_in cfs_ioctl;
    struct cfs_getattr_in cfs_getattr;
    struct cfs_setattr_in cfs_setattr;
    struct cfs_access_in cfs_access;
    struct cfs_lookup_in cfs_lookup;
    struct cfs_create_in cfs_create;
    struct cfs_remove_in cfs_remove;
    struct cfs_link_in cfs_link;
    struct cfs_rename_in cfs_rename;
    struct cfs_mkdir_in cfs_mkdir;
    struct cfs_rmdir_in cfs_rmdir;
    struct cfs_readdir_in cfs_readdir;
    struct cfs_symlink_in cfs_symlink;
    struct cfs_readlink_in cfs_readlink;
    struct cfs_fsync_in cfs_fsync;
    struct cfs_inactive_in cfs_inactive;
    struct cfs_vget_in cfs_vget;
    struct cfs_rdwr_in cfs_rdwr;
};

union outputArgs {
    struct cfs_out_hdr oh;		/* NB: every struct below begins with an oh */
    struct cfs_root_out cfs_root;
    struct cfs_open_out cfs_open;
    struct cfs_ioctl_out cfs_ioctl;
    struct cfs_getattr_out cfs_getattr;
    struct cfs_lookup_out cfs_lookup;
    struct cfs_create_out cfs_create;
    struct cfs_mkdir_out cfs_mkdir;
    struct cfs_readdir_out cfs_readdir;
    struct cfs_readlink_out cfs_readlink;
    struct cfs_vget_out cfs_vget;
    struct cfs_purgeuser_out cfs_purgeuser;
    struct cfs_zapfile_out cfs_zapfile;
    struct cfs_zapdir_out cfs_zapdir;
    struct cfs_zapvnode_out cfs_zapvnode;
    struct cfs_purgefid_out cfs_purgefid;
    struct cfs_rdwr_out cfs_rdwr;
    struct cfs_replace_out cfs_replace;
};    

union cfs_downcalls {
    /* CFS_INVALIDATE is a venus->kernel call */
    /* CFS_FLUSH is a venus->kernel call */
    struct cfs_purgeuser_out purgeuser;
    struct cfs_zapfile_out zapfile;
    struct cfs_zapdir_out zapdir;
    struct cfs_zapvnode_out zapvnode;
    struct cfs_purgefid_out purgefid;
    struct cfs_replace_out replace;
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

struct PioctlData {
        const char *path;
        int follow;
        struct ViceIoctl vi;
};

#define	CFS_CONTROL		".CONTROL"
#define CFS_CONTROLLEN           8
#define	CTL_VOL			-1
#define	CTL_VNO			-1
#define	CTL_UNI			-1
#define CTL_INO                 -1
#define	CTL_FILE		"/coda/.CONTROL"


#define	IS_CTL_FID(fidp)	((fidp)->Volume == CTL_VOL &&\
				 (fidp)->Vnode == CTL_VNO &&\
				 (fidp)->Unique == CTL_UNI)
#endif 

