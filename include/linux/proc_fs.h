#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

#include <linux/config.h>
#include <linux/slab.h>

/*
 * The proc filesystem constants/structures
 */

/*
 * Offset of the first process in the /proc root directory..
 */
#define FIRST_PROCESS_ENTRY 256


/*
 * We always define these enumerators
 */

enum {
	PROC_ROOT_INO = 1,
};

/* Finally, the dynamically allocatable proc entries are reserved: */

#define PROC_DYNAMIC_FIRST 4096
#define PROC_NDYNAMIC      4096

#define PROC_SUPER_MAGIC 0x9fa0

/*
 * This is not completely implemented yet. The idea is to
 * create an in-memory tree (like the actual /proc filesystem
 * tree) of these proc_dir_entries, so that we can dynamically
 * add new files to /proc.
 *
 * The "next" pointer creates a linked list of one /proc directory,
 * while parent/subdir create the directory structure (every
 * /proc file has a parent, but "subdir" is NULL for all
 * non-directory entries).
 *
 * "get_info" is called at "read", while "owner" is used to protect module
 * from unloading while proc_dir_entry is in use
 */

typedef	int (read_proc_t)(char *page, char **start, off_t off,
			  int count, int *eof, void *data);
typedef	int (write_proc_t)(struct file *file, const char *buffer,
			   unsigned long count, void *data);
typedef int (get_info_t)(char *, char **, off_t, int);

struct proc_dir_entry {
	unsigned short low_ino;
	unsigned short namelen;
	const char *name;
	mode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	unsigned long size;
	struct inode_operations * proc_iops;
	struct file_operations * proc_fops;
	get_info_t *get_info;
	struct module *owner;
	struct proc_dir_entry *next, *parent, *subdir;
	void *data;
	read_proc_t *read_proc;
	write_proc_t *write_proc;
	atomic_t count;		/* use count */
	int deleted;		/* delete flag */
	kdev_t	rdev;
};

#define PROC_INODE_PROPER(inode) ((inode)->i_ino & ~0xffff)

#ifdef CONFIG_PROC_FS

extern struct proc_dir_entry proc_root;
extern struct proc_dir_entry *proc_root_fs;
extern struct proc_dir_entry *proc_net;
extern struct proc_dir_entry *proc_bus;
extern struct proc_dir_entry *proc_root_driver;
extern struct proc_dir_entry *proc_root_kcore;

extern void proc_root_init(void);
extern void proc_misc_init(void);

struct dentry *proc_pid_lookup(struct inode *dir, struct dentry * dentry);
void proc_pid_delete_inode(struct inode *inode);
int proc_pid_readdir(struct file * filp, void * dirent, filldir_t filldir);

extern struct proc_dir_entry *create_proc_entry(const char *name, mode_t mode,
						struct proc_dir_entry *parent);
extern void remove_proc_entry(const char *name, struct proc_dir_entry *parent);

extern struct vfsmount *proc_mnt;
extern struct super_block *proc_read_super(struct super_block *,void *,int);
extern struct inode * proc_get_inode(struct super_block *, int, struct proc_dir_entry *);

extern int proc_match(int, const char *,struct proc_dir_entry *);

/*
 * These are generic /proc routines that use the internal
 * "struct proc_dir_entry" tree to traverse the filesystem.
 *
 * The /proc root directory has extended versions to take care
 * of the /proc/<pid> subdirectories.
 */
extern int proc_readdir(struct file *, void *, filldir_t);
extern struct dentry *proc_lookup(struct inode *, struct dentry *);

extern struct file_operations proc_kcore_operations;
extern struct file_operations proc_kmsg_operations;
extern struct file_operations ppc_htab_operations;

/*
 * proc_tty.c
 */
struct tty_driver;
extern void proc_tty_init(void);
extern void proc_tty_register_driver(struct tty_driver *driver);
extern void proc_tty_unregister_driver(struct tty_driver *driver);

/*
 * proc_devtree.c
 */
extern void proc_device_tree_init(void);

/*
 * proc_rtas.c
 */
extern void proc_rtas_init(void);

/*
 * PPC64
 */ 
extern void proc_ppc64_init(void);
extern void iSeries_proc_create(void);

extern struct proc_dir_entry *proc_symlink(const char *,
		struct proc_dir_entry *, const char *);
extern struct proc_dir_entry *proc_mknod(const char *,mode_t,
		struct proc_dir_entry *,kdev_t);
extern struct proc_dir_entry *proc_mkdir(const char *,struct proc_dir_entry *);

static inline struct proc_dir_entry *create_proc_read_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, 
	read_proc_t *read_proc, void * data)
{
	struct proc_dir_entry *res=create_proc_entry(name,mode,base);
	if (res) {
		res->read_proc=read_proc;
		res->data=data;
	}
	return res;
}
 
static inline struct proc_dir_entry *create_proc_info_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, get_info_t *get_info)
{
	struct proc_dir_entry *res=create_proc_entry(name,mode,base);
	if (res) res->get_info=get_info;
	return res;
}
 
static inline struct proc_dir_entry *proc_net_create(const char *name,
	mode_t mode, get_info_t *get_info)
{
	return create_proc_info_entry(name,mode,proc_net,get_info);
}

static inline void proc_net_remove(const char *name)
{
	remove_proc_entry(name,proc_net);
}

#else

#define proc_root_driver NULL

static inline struct proc_dir_entry *proc_net_create(const char *name, mode_t mode, 
	get_info_t *get_info) {return NULL;}
static inline void proc_net_remove(const char *name) {}

static inline struct proc_dir_entry *create_proc_entry(const char *name,
	mode_t mode, struct proc_dir_entry *parent) { return NULL; }

static inline void remove_proc_entry(const char *name, struct proc_dir_entry *parent) {};
static inline struct proc_dir_entry *proc_symlink(const char *name,
		struct proc_dir_entry *parent,char *dest) {return NULL;}
static inline struct proc_dir_entry *proc_mknod(const char *name,mode_t mode,
		struct proc_dir_entry *parent,kdev_t rdev) {return NULL;}
static inline struct proc_dir_entry *proc_mkdir(const char *name,
	struct proc_dir_entry *parent) {return NULL;}

static inline struct proc_dir_entry *create_proc_read_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, 
	int (*read_proc)(char *, char **, off_t, int, int *, void *),
	void * data) { return NULL; }
static inline struct proc_dir_entry *create_proc_info_entry(const char *name,
	mode_t mode, struct proc_dir_entry *base, get_info_t *get_info)
	{ return NULL; }

static inline void proc_tty_register_driver(struct tty_driver *driver) {};
static inline void proc_tty_unregister_driver(struct tty_driver *driver) {};

extern struct proc_dir_entry proc_root;

#endif /* CONFIG_PROC_FS */

static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
	return (struct proc_dir_entry *)inode->u.generic_ip;
}

#endif /* _LINUX_PROC_FS_H */
