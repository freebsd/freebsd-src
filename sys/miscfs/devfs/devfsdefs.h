/*
 * Copyright 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $Id: devfsdefs.h,v 1.14 1998/04/19 23:32:20 julian Exp $
 */
#ifdef DEVFS_DEBUG
#define DBPRINT(A) printf(A)
#else
#define DBPRINT(A)
#endif

/* first a couple of defines for compatibility with inodes */

#define	ISUID		04000		/* set user identifier when exec'ing */
#define	ISGID		02000		/* set group identifier when exec'ing */
#define	ISVTX		01000		/* save execution information on exit */
#define	IREAD		0400		/* read permission */
#define	IWRITE		0200		/* write permission */
#define	IEXEC		0100		/* execute permission */


#define	ILOCKED		0x0001		/* inode is locked */
#define	IWANT		0x0002		/* some process waiting on lock */
#define	IRENAME		0x0004		/* inode is being renamed */
#define	IUPD		0x0010		/* file has been modified */
#define	IACC		0x0020		/* inode access time to be updated */
#define	ICHG		0x0040		/* inode has been changed */
#define	IMOD		0x0080		/* inode has been modified */
#define	ISHLOCK		0x0100		/* file has shared lock */
#define	IEXLOCK		0x0200		/* file has exclusive lock */
#define	ILWAIT		0x0400		/* someone waiting on file lock */

/*
 * Lock and unlock inodes.
 */
#ifdef notdef
#define	DNLOCK(ip) { \
	while ((ip)->i_flag & ILOCKED) { \
		(ip)->i_flag |= IWANT; \
		(void) sleep((caddr_t)(ip), PINOD); \
	} \
	(ip)->i_flag |= ILOCKED; \
}

#define	DNUNLOCK(ip) { \
	(ip)->i_flag &= ~ILOCKED; \
	if ((ip)->i_flag&IWANT) { \
		(ip)->i_flag &= ~IWANT; \
		wakeup((caddr_t)(ip)); \
	} \
}
#else
#define DNLOCK(ip)
#define DNUNLOCK(ip)
#endif


#define DEVMAXNAMESIZE 32
#define DEVMAXPATHSIZE 128
#define	DEV_DIR 1
#define DEV_BDEV 2
#define DEV_CDEV 3
#define DEV_DDEV 4
#define	DEV_ALIAS 5
#define DEV_SLNK 6
#define DEV_PIPE 7


extern vop_t **devfs_vnodeop_p;		/* our own vector array for dirs */
extern vop_t **dev_spec_vnodeop_p;	/* our own vector array for devs */

typedef struct dev_name *devnm_p;
typedef	struct devnode	*dn_p;

struct	devnode	/* the equivalent of an INODE */
{
	u_short type;
	int	flags;		/* more inode compatible for now *//*XXXkill*/
	u_short	mode;		/* basically inode compatible (drwxrwxrwx) */
	u_short	uid;		/* basically inode compatible  */
	u_short	gid;		/* basically inode compatible  */
	struct timespec	atime;	/* time of last access */
	struct timespec	mtime;	/* time of last modification */
	struct timespec	ctime;	/* time file changed */
	int	links;		/* how many file links does this node have? */
	struct	devfsmount *dvm; /* the mount structure for this 'plane' */
	struct	vnode *vn;	/* address of last vnode that represented us */
	u_long	vn_id;		/* make sure we have the right vnode */
	int (***ops)(void *);	/* yuk... pointer to pointer(s) to funcs */
	int	len;		/* of any associated info (e.g. dir data) */
	devnm_p	linklist;	/* circular list of hardlinks to this node */
	devnm_p	last_lookup;	/* name I was last looked up from */
	dn_p	nextsibling;	/* the list of equivelent nodes */
	dn_p	*prevsiblingp;	/* backpointer for the above */
	union  typeinfo {
		struct {
			struct	cdevsw	*cdevsw;
			dev_t	dev;
		}Cdev;
		struct {
			struct	cdevsw	*bdevsw;
			dev_t	dev;
		}Bdev;
		struct {
			int (***ops)(void *); /* duplicate, used in dev_add_node */
			int	arg;
		}Ddev;
		struct {
			devnm_p	dirlist;
			devnm_p	*dirlast;
			dn_p	parent;
			devnm_p	myname;		/* my entry in .. */
			int	entrycount;
		}Dir;
		struct {
			char	*name;	/* must be allocated separately */
			int	namelen;
		}Slnk;
		struct {
			devnm_p	realthing;
			devnm_p	next;
		}Alias;
		struct {
			struct socket *sock;
		}Pipe;
	}by;
};
typedef	struct devnode	devnode_t;

struct	dev_name
{
	/*-----------------------directory entry fields-------------*/
	char	name[DEVMAXNAMESIZE];
	dn_p	dnp;		/* the "inode" (devnode) pointer */
	dn_p	parent;		/* backpointer to the directory itself */
	devnm_p	next;		/* next object in this directory */
	devnm_p	*prevp;		/* previous pointer in directory linked list */
	devnm_p	nextlink;	/* next hardlink to this node */
	devnm_p	*prevlinkp;	/* previous hardlink pointer for this node */
	/*-----------------------aliases or backing nodes----------*/
	union {
		struct {
			devnm_p	aliases;	/* aliase chain (kill with us)*/
		} back;
		struct {
			devnm_p	realthing;	/* ptr to the backing node */
		} front;
	} as;
};

typedef struct dev_name devnm_t;
extern int devfs_up_and_going;
extern devnm_p dev_root;


/*
 * Rules for front nodes:
 * Dirs hava a strict 1:1 relationship with their OWN devnode
 * Symlinks similarly
 * Device Nodes ALWAYS point to the devnode that is linked
 * to the Backing node. (with a ref count)
 */

/*
 * DEVFS specific per/mount information, used to link a monted fs to a
 * particular 'plane' of front nodes.
 */
struct devfsmount
{
	struct mount *mount;		/* vfs mount struct for this fs	*/
	devnm_p	plane_root;		/* the root of this 'plane'	*/
	int	flags;			/* usefule some day 8-) 	*/
};

struct dev_vn_data
{
	char	magic[6];		/* = "devfs" if correct */
	devnm_p	front;
	devnm_p	back;
};

extern struct vnodeops spec_vnodeops,devfs_vnodeops;
/*
 * Prototypes for DEVFS virtual filesystem operations
 */
#include <miscfs/devfs/devfs_proto.h>
