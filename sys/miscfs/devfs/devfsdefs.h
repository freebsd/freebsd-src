#define DEBUG 1
#ifdef DEBUG
#define DBPRINT(A) printf(A)
#else
#define DBPRINT(A)
#endif

/*
 * Written by Julian Elischer (julian@DIALIX.oz.au)
 *
 * $Header: /sys/miscfs/devfs/RCS/devfsdefs.h,v 1.2 1994/12/28 02:43:47 root Exp root $
 */

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


extern int (**devfs_vnodeop_p)();	/* our own vector array for dirs */
extern int (**dev_spec_vnodeop_p)();	/* our own vector array for devs */

typedef struct dev_back *devb_p;
typedef struct dev_front *devf_p;
typedef	struct devnode	*dn_p;

struct	devnode
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
	int (***ops)();		/* yuk... pointer to pointer(s) to funcs */
	int	len;		/* of any associated info (e.g. dir data) */
	union  typeinfo {
		struct {
			struct	cdevsw	*cdevsw;
			dev_t	dev;
		}Cdev;
		struct {
			struct	bdevsw	*bdevsw;
			dev_t	dev;
		}Bdev;
		struct {
			int (***ops)();	 /* duplicate, used in dev_add_node */
			int	arg;
		}Ddev;
		struct {
			devf_p	dirlist;
			devf_p	*dirlast;
			dn_p	parent;
			devf_p	myname;
			int	entrycount;
		}Dir;	
		struct {
			devb_p	dirlist;
			devb_p	*dirlast;
			dn_p	parent;
			devb_p	myname;
			int	entrycount;
		}BackDir;	
		struct {
			char	*name;	/* must be allocated separatly */
			int	namelen;
		}Slnk;
		struct {
			devb_p	realthing;
			devb_p	next;
		}Alias;
	}by;
};
typedef	struct devnode	devnode_t;

struct	dev_back
{
	devb_p	next;		/* next object in this directory */
	devb_p	*prevp;		/* previous pointer in directory linked list */
	devb_p	aliases;	/* chain of aliases (kill if we are deleted)*/
	int	alias_count;	/* how many 'alias' nodes reference us. */
	devf_p	fronts;		/* the linked list of all our front nodes */
	devf_p	*lastfront;	/* the end of the front node chain */
	int	frontcount;	/* number of front nodes that reference us*/
	dn_p parent;	/* backpointer to the directory itself */
	dn_p dnp;	/* info a STAT would look at */
	char	name[DEVMAXNAMESIZE];
};

typedef struct dev_back devb_t;
extern struct dev_back *dev_root;		/* always exists */


/*
 * Rules for front nodes:
 * Dirs hava a strict 1:1 relationship with their OWN devnode
 * Symlinks similarly
 * Device Nodes ALWAYS point to the devnode that is linked
 * to the Backing node. (with a ref count)
 */
struct dev_front
{
	devf_p	next;		/* next item in this directory chain */
	devf_p	*prevp;		/* previous pointer in the directory */
	devf_p	file_node;	/* the file node this represents */
	devf_p	next_front;	/* pointer to next item for this object */
	devf_p	*prev_frontp;	/* previous pointer in object chain */
	devb_p	realthing;	/* backpointer to the backing object */
	dn_p parent;	/* our PARENT directory node */
	dn_p dnp;	/* info a STAT would look at */
	char	name[DEVMAXNAMESIZE];
};
typedef struct dev_front devf_t;



/*
 * DEVFS specific per/mount information, used to link a monted fs to a
 * particular 'plane' of front nodes.
 */
struct devfsmount
{
	struct mount *mount;		/* vfs mount struct for this fs	*/
	devf_p	plane_root;		/* the root of this 'plane'	*/
	int	flags;			/* usefule some day 8-) 	*/
};

struct dev_vn_data
{
	char	magic[6];		/* = "devfs" if correct */
	devf_p	front;
	devb_p	back;
};

extern struct vnodeops spec_vnodeops,devfs_vnodeops;
/*
 * Prototypes for DEVFS virtual filesystem operations
 */
#include "devfs_proto.h"
