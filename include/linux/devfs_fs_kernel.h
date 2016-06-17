#ifndef _LINUX_DEVFS_FS_KERNEL_H
#define _LINUX_DEVFS_FS_KERNEL_H

#include <linux/fs.h>
#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/kdev_t.h>
#include <linux/types.h>

#include <asm/semaphore.h>

#define DEVFS_SUPER_MAGIC                0x1373

#define IS_DEVFS_INODE(inode) (DEVFS_SUPER_MAGIC == (inode)->i_sb->s_magic)

#define DEVFS_MINOR(inode) \
    ({unsigned int m; /* evil GCC trickery */ \
      ((inode)->i_sb && \
       ((inode)->i_sb->s_magic==DEVFS_SUPER_MAGIC) && \
       (devfs_get_maj_min(devfs_get_handle_from_inode((inode)),NULL,&m)==0) \
      ) ? m : MINOR((inode)->r_dev); })


#define DEVFS_FL_NONE           0x000 /* This helps to make code more readable
				       */
#define DEVFS_FL_AUTO_OWNER     0x001 /* When a closed inode is opened the
					 ownerships are set to the opening
					 process and the protection is set to
					 that given in <<mode>>. When the inode
					 is closed, ownership reverts back to
					 <<uid>> and <<gid>> and the protection
					 is set to read-write for all        */
#define DEVFS_FL_HIDE           0x002 /* Do not show entry in directory list */
#define DEVFS_FL_AUTO_DEVNUM    0x004 /* Automatically generate device number
				       */
#define DEVFS_FL_AOPEN_NOTIFY   0x008 /* Asynchronously notify devfsd on open
				       */
#define DEVFS_FL_REMOVABLE      0x010 /* This is a removable media device    */
#define DEVFS_FL_WAIT           0x020 /* Wait for devfsd to finish           */
#define DEVFS_FL_CURRENT_OWNER  0x040 /* Set initial ownership to current    */
#define DEVFS_FL_DEFAULT        DEVFS_FL_NONE


#define DEVFS_SPECIAL_CHR     0
#define DEVFS_SPECIAL_BLK     1

typedef struct devfs_entry * devfs_handle_t;

#ifdef CONFIG_DEVFS_FS

struct unique_numspace
{
    spinlock_t init_lock;
    unsigned char sem_initialised;
    unsigned int num_free;          /*  Num free in bits       */
    unsigned int length;            /*  Array length in bytes  */
    unsigned long *bits;
    struct semaphore semaphore;
};

#define UNIQUE_NUMBERSPACE_INITIALISER {SPIN_LOCK_UNLOCKED, 0, 0, 0, NULL}

extern void devfs_put (devfs_handle_t de);
extern devfs_handle_t devfs_register (devfs_handle_t dir, const char *name,
				      unsigned int flags,
				      unsigned int major, unsigned int minor,
				      umode_t mode, void *ops, void *info);
extern void devfs_unregister (devfs_handle_t de);
extern int devfs_mk_symlink (devfs_handle_t dir, const char *name,
			     unsigned int flags, const char *link,
			     devfs_handle_t *handle, void *info);
extern devfs_handle_t devfs_mk_dir (devfs_handle_t dir, const char *name,
				    void *info);
extern devfs_handle_t devfs_get_handle (devfs_handle_t dir, const char *name,
					unsigned int major,unsigned int minor,
					char type, int traverse_symlinks);
extern devfs_handle_t devfs_find_handle (devfs_handle_t dir, const char *name,
					 unsigned int major,unsigned int minor,
					 char type, int traverse_symlinks);
extern int devfs_get_flags (devfs_handle_t de, unsigned int *flags);
extern int devfs_set_flags (devfs_handle_t de, unsigned int flags);
extern int devfs_get_maj_min (devfs_handle_t de, 
			      unsigned int *major, unsigned int *minor);
extern devfs_handle_t devfs_get_handle_from_inode (struct inode *inode);
extern int devfs_generate_path (devfs_handle_t de, char *path, int buflen);
extern void *devfs_get_ops (devfs_handle_t de);
extern void devfs_put_ops (devfs_handle_t de);
extern int devfs_set_file_size (devfs_handle_t de, unsigned long size);
extern void *devfs_get_info (devfs_handle_t de);
extern int devfs_set_info (devfs_handle_t de, void *info);
extern devfs_handle_t devfs_get_parent (devfs_handle_t de);
extern devfs_handle_t devfs_get_first_child (devfs_handle_t de);
extern devfs_handle_t devfs_get_next_sibling (devfs_handle_t de);
extern void devfs_auto_unregister (devfs_handle_t master,devfs_handle_t slave);
extern devfs_handle_t devfs_get_unregister_slave (devfs_handle_t master);
extern const char *devfs_get_name (devfs_handle_t de, unsigned int *namelen);
extern int devfs_register_chrdev (unsigned int major, const char *name,
				  struct file_operations *fops);
extern int devfs_register_blkdev (unsigned int major, const char *name,
				  struct block_device_operations *bdops);
extern int devfs_unregister_chrdev (unsigned int major, const char *name);
extern int devfs_unregister_blkdev (unsigned int major, const char *name);

extern void devfs_register_tape (devfs_handle_t de);
extern void devfs_register_series (devfs_handle_t dir, const char *format,
				   unsigned int num_entries,
				   unsigned int flags, unsigned int major,
				   unsigned int minor_start,
				   umode_t mode, void *ops, void *info);
extern int devfs_alloc_major (char type);
extern void devfs_dealloc_major (char type, int major);
extern kdev_t devfs_alloc_devnum (char type);
extern void devfs_dealloc_devnum (char type, kdev_t devnum);
extern int devfs_alloc_unique_number (struct unique_numspace *space);
extern void devfs_dealloc_unique_number (struct unique_numspace *space,
					 int number);

extern void mount_devfs_fs (void);

#else  /*  CONFIG_DEVFS_FS  */

struct unique_numspace
{
    char dummy;
};

#define UNIQUE_NUMBERSPACE_INITIALISER {0}

static inline void devfs_put (devfs_handle_t de)
{
    return;
}
static inline devfs_handle_t devfs_register (devfs_handle_t dir,
					     const char *name,
					     unsigned int flags,
					     unsigned int major,
					     unsigned int minor,
					     umode_t mode,
					     void *ops, void *info)
{
    return NULL;
}
static inline void devfs_unregister (devfs_handle_t de)
{
    return;
}
static inline int devfs_mk_symlink (devfs_handle_t dir, const char *name,
				    unsigned int flags, const char *link,
				    devfs_handle_t *handle, void *info)
{
    return 0;
}
static inline devfs_handle_t devfs_mk_dir (devfs_handle_t dir,
					   const char *name, void *info)
{
    return NULL;
}
static inline devfs_handle_t devfs_get_handle (devfs_handle_t dir,
					       const char *name,
					       unsigned int major,
					       unsigned int minor,
					       char type,
					       int traverse_symlinks)
{
    return NULL;
}
static inline devfs_handle_t devfs_find_handle (devfs_handle_t dir,
						const char *name,
						unsigned int major,
						unsigned int minor,
						char type,
						int traverse_symlinks)
{
    return NULL;
}
static inline int devfs_get_flags (devfs_handle_t de, unsigned int *flags)
{
    return 0;
}
static inline int devfs_set_flags (devfs_handle_t de, unsigned int flags)
{
    return 0;
}
static inline int devfs_get_maj_min (devfs_handle_t de, 
				     unsigned int *major, unsigned int *minor)
{
    return 0;
}
static inline devfs_handle_t devfs_get_handle_from_inode (struct inode *inode)
{
    return NULL;
}
static inline int devfs_generate_path (devfs_handle_t de, char *path,
				       int buflen)
{
    return -ENOSYS;
}
static inline void *devfs_get_ops (devfs_handle_t de)
{
    return NULL;
}
static inline void devfs_put_ops (devfs_handle_t de)
{
    return;
}
static inline int devfs_set_file_size (devfs_handle_t de, unsigned long size)
{
    return -ENOSYS;
}
static inline void *devfs_get_info (devfs_handle_t de)
{
    return NULL;
}
static inline int devfs_set_info (devfs_handle_t de, void *info)
{
    return 0;
}
static inline devfs_handle_t devfs_get_parent (devfs_handle_t de)
{
    return NULL;
}
static inline devfs_handle_t devfs_get_first_child (devfs_handle_t de)
{
    return NULL;
}
static inline devfs_handle_t devfs_get_next_sibling (devfs_handle_t de)
{
    return NULL;
}
static inline void devfs_auto_unregister (devfs_handle_t master,
					  devfs_handle_t slave)
{
    return;
}
static inline devfs_handle_t devfs_get_unregister_slave (devfs_handle_t master)
{
    return NULL;
}
static inline const char *devfs_get_name (devfs_handle_t de,
					  unsigned int *namelen)
{
    return NULL;
}
static inline int devfs_register_chrdev (unsigned int major, const char *name,
					 struct file_operations *fops)
{
    return register_chrdev (major, name, fops);
}
static inline int devfs_register_blkdev (unsigned int major, const char *name,
					 struct block_device_operations *bdops)
{
    return register_blkdev (major, name, bdops);
}
static inline int devfs_unregister_chrdev (unsigned int major,const char *name)
{
    return unregister_chrdev (major, name);
}
static inline int devfs_unregister_blkdev (unsigned int major,const char *name)
{
    return unregister_blkdev (major, name);
}

static inline void devfs_register_tape (devfs_handle_t de)
{
    return;
}

static inline void devfs_register_series (devfs_handle_t dir,
					  const char *format,
					  unsigned int num_entries,
					  unsigned int flags,
					  unsigned int major,
					  unsigned int minor_start,
					  umode_t mode, void *ops, void *info)
{
    return;
}

static inline int devfs_alloc_major (char type)
{
    return -1;
}

static inline void devfs_dealloc_major (char type, int major)
{
    return;
}

static inline kdev_t devfs_alloc_devnum (char type)
{
    return NODEV;
}

static inline void devfs_dealloc_devnum (char type, kdev_t devnum)
{
    return;
}

static inline int devfs_alloc_unique_number (struct unique_numspace *space)
{
    return -1;
}

static inline void devfs_dealloc_unique_number (struct unique_numspace *space,
						int number)
{
    return;
}

static inline void mount_devfs_fs (void)
{
    return;
}
#endif  /*  CONFIG_DEVFS_FS  */

#endif  /*  _LINUX_DEVFS_FS_KERNEL_H  */
