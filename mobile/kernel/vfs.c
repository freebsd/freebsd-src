/*
 * Virtual FileSystem Implementation
 * uOS(m) - User OS Mobile
 */

#include "vfs.h"
#include "memory.h"

/* File descriptor table */
#define MAX_OPEN_FILES 256
static file_desc_t fd_table[MAX_OPEN_FILES];
static int fd_next = 3;  /* Skip stdin/stdout/stderr */

/* Inode cache */
#define INODE_CACHE_SIZE 256
static inode_t inode_cache[INODE_CACHE_SIZE];
static int inode_next_id = 1;

/* Filesystem registry */
#define MAX_FILESYSTEMS 8
static filesystem_t *fs_registry[MAX_FILESYSTEMS];
static int fs_count = 0;

extern void uart_puts(const char *s);

/* Initialize VFS */
int vfs_init(void) {
    uart_puts("Virtual FileSystem initializing...\n");
    
    /* Clear FD table */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        fd_table[i].inode = NULL;
        fd_table[i].offset = 0;
        fd_table[i].flags = 0;
        fd_table[i].refcount = 0;
    }
    
    /* Clear inode cache */
    for (int i = 0; i < INODE_CACHE_SIZE; i++) {
        inode_cache[i].ino = 0;
        inode_cache[i].data = NULL;
    }
    
    uart_puts("VFS ready\n");
    return 0;
}

/* Register a filesystem */
int vfs_register_fs(filesystem_t *fs) {
    if (!fs) return -1;
    
    if (fs_count >= MAX_FILESYSTEMS) {
        return -1;
    }
    
    fs_registry[fs_count++] = fs;
    
    char msg[] = "Registered filesystem: ";
    uart_puts(msg);
    uart_puts(fs->name);
    uart_puts("\n");
    
    return 0;
}

/* Mount a filesystem */
int vfs_mount(filesystem_t *fs, const char *path) {
    if (!fs || !path) return -1;
    
    if (fs->mount) {
        return fs->mount(path);
    }
    
    return 0;
}

/* Open a file */
int vfs_open(const char *path, int flags) {
    if (!path) return -1;
    
    /* Find free FD */
    int fd = -1;
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].refcount == 0) {
            fd = i;
            break;
        }
    }
    
    if (fd < 0) return -1;
    
    /* Try each filesystem */
    for (int i = 0; i < fs_count; i++) {
        inode_t *inode = NULL;
        
        if (fs_registry[i]->open) {
            if (fs_registry[i]->open(path, flags, &inode) == 0) {
                fd_table[fd].inode = inode;
                fd_table[fd].offset = 0;
                fd_table[fd].flags = flags;
                fd_table[fd].refcount = 1;
                return fd;
            }
        }
    }
    
    return -1;
}

/* Close a file */
int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    
    if (fd_table[fd].refcount > 0) {
        fd_table[fd].refcount--;
        
        if (fd_table[fd].refcount == 0) {
            fd_table[fd].inode = NULL;
            fd_table[fd].offset = 0;
        }
        
        return 0;
    }
    
    return -1;
}

/* Read from file */
int vfs_read(int fd, uint8_t *buf, uint64_t len) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !buf) return -1;
    
    file_desc_t *f = &fd_table[fd];
    if (!f->inode) return -1;
    
    /* Find filesystem and call read */
    for (int i = 0; i < fs_count; i++) {
        if (fs_registry[i]->read) {
            int bytes_read = fs_registry[i]->read(f->inode, f->offset, buf, len);
            if (bytes_read > 0) {
                f->offset += bytes_read;
                return bytes_read;
            }
        }
    }
    
    return -1;
}

/* Write to file */
int vfs_write(int fd, uint8_t *buf, uint64_t len) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !buf) return -1;
    
    file_desc_t *f = &fd_table[fd];
    if (!f->inode) return -1;
    
    /* Find filesystem and call write */
    for (int i = 0; i < fs_count; i++) {
        if (fs_registry[i]->write) {
            int bytes_written = fs_registry[i]->write(f->inode, f->offset, buf, len);
            if (bytes_written > 0) {
                f->offset += bytes_written;
                return bytes_written;
            }
        }
    }
    
    return -1;
}

/* Seek in file */
int vfs_seek(int fd, uint64_t offset) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    
    file_desc_t *f = &fd_table[fd];
    if (!f->inode) return -1;
    
    if (offset <= f->inode->size) {
        f->offset = offset;
        return 0;
    }
    
    return -1;
}

/* Get file statistics */
int vfs_stat(const char *path, inode_t *stat) {
    if (!path || !stat) return -1;
    
    /* Try to open and get inode */
    int fd = vfs_open(path, 0);
    if (fd >= 0) {
        *stat = *fd_table[fd].inode;
        vfs_close(fd);
        return 0;
    }
    
    return -1;
}

/* Allocate an inode */
inode_t *vfs_alloc_inode(void) {
    for (int i = 0; i < INODE_CACHE_SIZE; i++) {
        if (inode_cache[i].ino == 0) {
            inode_cache[i].ino = inode_next_id++;
            return &inode_cache[i];
        }
    }
    return NULL;
}

/* Free an inode */
void vfs_free_inode(inode_t *inode) {
    if (inode) {
        inode->ino = 0;
        if (inode->data) {
            mem_free(inode->data);
            inode->data = NULL;
        }
    }
}