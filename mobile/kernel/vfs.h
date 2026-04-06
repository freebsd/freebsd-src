/*
 * Simple Virtual FileSystem
 * uOS(m) - User OS Mobile
 * Basic VFS abstraction layer
 */

#ifndef _VFS_H_
#define _VFS_H_

#include <stdint.h>

/* File types */
#define FILE_TYPE_REGULAR 1
#define FILE_TYPE_DIR     2
#define FILE_TYPE_CHAR    3
#define FILE_TYPE_BLOCK   4
#define FILE_TYPE_FIFO    5
#define FILE_TYPE_LINK    6
#define FILE_TYPE_SOCK    7

/* File mode bits */
#define FILE_MODE_READ  0x0001
#define FILE_MODE_WRITE 0x0002
#define FILE_MODE_EXEC  0x0004

#define MAX_FILENAME 256
#define MAX_FILES 512
#define MAX_MOUNTS 16

/* Inode structure */
typedef struct {
    uint32_t ino;               /* Inode number */
    uint32_t type;              /* File type */
    uint32_t mode;              /* Permissions */
    uint32_t owner;             /* UID */
    uint32_t group;             /* GID */
    uint64_t size;              /* File size */
    uint64_t accessed;          /* Last accessed */
    uint64_t modified;          /* Last modified */
    uint64_t created;           /* Created time */
    uint32_t link_count;        /* Hard link count */
    void *data;                 /* File data pointer */
} inode_t;

/* Directory entry */
typedef struct {
    uint32_t ino;
    char name[MAX_FILENAME];
    uint8_t type;
} dirent_t;

/* File descriptor */
typedef struct {
    inode_t *inode;
    uint64_t offset;
    int flags;
    int refcount;
} file_desc_t;

/* FileSystem operations */
typedef struct {
    const char *name;
    int (*init)(void);
    int (*mount)(const char *path);
    int (*unmount)(const char *path);
    int (*open)(const char *path, int flags, inode_t **inode);
    int (*close)(inode_t *inode);
    int (*read)(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len);
    int (*write)(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len);
} filesystem_t;

/* VFS operations */
int vfs_init(void);
int vfs_register_fs(filesystem_t *fs);
int vfs_mount(filesystem_t *fs, const char *path);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, uint8_t *buf, uint64_t len);
int vfs_write(int fd, uint8_t *buf, uint64_t len);
int vfs_seek(int fd, uint64_t offset);
int vfs_stat(const char *path, inode_t *stat);

/* Inode cache management */
inode_t *vfs_alloc_inode(void);
void vfs_free_inode(inode_t *inode);

#endif /* _VFS_H_ */