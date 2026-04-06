/*
 * Virtual FileSystem Implementation
 * uOS(m) - User OS Mobile
 */

#include "vfs.h"
#include "memory.h"
#include "page_cache.h"
#include "memory_utils.h"

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

/* Writeback callback for page cache */
static int vfs_writeback(uint32_t ino, uint32_t page_offset, uint8_t *data, uint32_t len) {
    /* Find the inode */
    inode_t *inode = NULL;
    for (int i = 0; i < INODE_CACHE_SIZE; i++) {
        if (inode_cache[i].ino == ino) {
            inode = &inode_cache[i];
            break;
        }
    }
    
    if (!inode) return -1;
    
    /* Write to filesystem */
    uint64_t offset = page_offset * PAGE_SIZE;
    for (int i = 0; i < fs_count; i++) {
        if (fs_registry[i]->write) {
            int bytes_written = fs_registry[i]->write(inode, offset, data, len);
            if (bytes_written > 0) {
                return bytes_written;
            }
        }
    }
    
    return -1;
}

/* Initialize VFS */
int vfs_init(void) {
    uart_puts("Virtual FileSystem initializing...\n");
    
    /* Initialize page cache */
    if (page_cache_init() < 0) {
        uart_puts("Page cache initialization failed\n");
        return -1;
    }
    
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
            /* Flush dirty pages for this inode */
            if (fd_table[fd].inode) {
                page_cache_flush_inode(fd_table[fd].inode->ino, vfs_writeback);
            }
            
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
    
    uint64_t bytes_read = 0;
    uint64_t remaining = len;
    uint64_t file_offset = f->offset;
    
    while (remaining > 0 && file_offset < f->inode->size) {
        uint32_t page_offset = file_offset / PAGE_SIZE;
        uint32_t page_byte_offset = file_offset % PAGE_SIZE;
        uint32_t bytes_in_page = PAGE_SIZE - page_byte_offset;
        if (bytes_in_page > remaining) bytes_in_page = remaining;
        if (file_offset + bytes_in_page > f->inode->size) {
            bytes_in_page = f->inode->size - file_offset;
        }
        
        /* Get page from cache */
        page_cache_entry_t *page = page_cache_get(f->inode->ino, page_offset);
        if (!page) {
            /* Page not in cache, need to load from filesystem */
            /* For now, fall back to direct filesystem read */
            for (int i = 0; i < fs_count; i++) {
                if (fs_registry[i]->read) {
                    uint8_t temp_buf[PAGE_SIZE];
                    int fs_bytes = fs_registry[i]->read(f->inode, page_offset * PAGE_SIZE, temp_buf, PAGE_SIZE);
                    if (fs_bytes > 0) {
                        page = page_cache_get(f->inode->ino, page_offset);
                        if (page) {
                            memcpy(page->data, temp_buf, fs_bytes);
                            page->valid = 1;
                        }
                        break;
                    }
                }
            }
            if (!page) return -1;
        }
        
        /* Copy from page cache */
        memcpy(buf + bytes_read, page->data + page_byte_offset, bytes_in_page);
        page_cache_put(page);
        
        bytes_read += bytes_in_page;
        remaining -= bytes_in_page;
        file_offset += bytes_in_page;
    }
    
    f->offset = file_offset;
    return bytes_read;
}

/* Write to file */
int vfs_write(int fd, uint8_t *buf, uint64_t len) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !buf) return -1;
    
    file_desc_t *f = &fd_table[fd];
    if (!f->inode) return -1;
    
    uint64_t bytes_written = 0;
    uint64_t remaining = len;
    uint64_t file_offset = f->offset;
    
    while (remaining > 0) {
        uint32_t page_offset = file_offset / PAGE_SIZE;
        uint32_t page_byte_offset = file_offset % PAGE_SIZE;
        uint32_t bytes_in_page = PAGE_SIZE - page_byte_offset;
        if (bytes_in_page > remaining) bytes_in_page = remaining;
        
        /* Get page from cache */
        page_cache_entry_t *page = page_cache_get(f->inode->ino, page_offset);
        if (!page) {
            /* Page not in cache, need to allocate */
            page = page_cache_get(f->inode->ino, page_offset);
            if (!page) return -1;
            
            /* If writing beyond current file size, zero-fill */
            if (file_offset >= f->inode->size) {
                memset(page->data, 0, PAGE_SIZE);
            } else {
                /* Load existing data from filesystem */
                for (int i = 0; i < fs_count; i++) {
                    if (fs_registry[i]->read) {
                        int fs_bytes = fs_registry[i]->read(f->inode, page_offset * PAGE_SIZE, page->data, PAGE_SIZE);
                        if (fs_bytes > 0) break;
                    }
                }
            }
            page->valid = 1;
        }
        
        /* Copy to page cache */
        memcpy(page->data + page_byte_offset, buf + bytes_written, bytes_in_page);
        page_cache_dirty(page);
        page_cache_put(page);
        
        bytes_written += bytes_in_page;
        remaining -= bytes_in_page;
        file_offset += bytes_in_page;
        
        /* Update file size if necessary */
        if (file_offset > f->inode->size) {
            f->inode->size = file_offset;
        }
    }
    
    f->offset = file_offset;
    return bytes_written;
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