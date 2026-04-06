/*
 * Hybrid UFS-on-ZFS Disk Format for uOS(m)
 * Uses a block device-backed inode layout with UFS-style directory entries
 * and a ZFS-like pool metadata foundation.
 */

#include "hybridfs.h"
#include "blockdev.h"
#include "memory.h"
#include "posix_syscalls.h"
#include "vfs.h"
#include <stdint.h>
#include "memory_utils.h"

#define HYBRID_MAGIC 0x555A4653  /* 'UZFS' */
#define HYBRID_VERSION 1
#define HYBRID_MAX_FILENAME 64
#define HYBRID_MAX_DIR_ENTRIES 32
#define HYBRID_DIRECT_BLOCKS 12
#define HYBRID_INODE_COUNT 128
#define HYBRID_ROOT_INO 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t root_inode;
    uint32_t reserved[10];
} hybrid_superblock_t;

typedef struct {
    uint32_t ino;
    uint32_t type;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint32_t direct[HYBRID_DIRECT_BLOCKS];
    uint32_t indirect;
    uint64_t created;
    uint64_t modified;
} hybrid_inode_t;

typedef struct {
    uint32_t ino;
    uint8_t type;
    char name[HYBRID_MAX_FILENAME];
} hybrid_dirent_t;

static hybrid_superblock_t superblock;
static int hybrid_initialized = 0;

static void hybrid_zero(void *dest, size_t n) {
    uint8_t *d = dest;
    while (n--) *d++ = 0;
}

static void hybrid_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
}

static int hybrid_strcmp(const char *a, const char *b) {
    for (uint32_t i = 0; i < HYBRID_MAX_FILENAME; i++) {
        if (a[i] != b[i]) return 1;
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static int hybrid_read_block(uint32_t block, uint8_t *buf) {
    return blockdev_read_block(block, buf);
}

static int hybrid_write_block(uint32_t block, const uint8_t *buf) {
    return blockdev_write_block(block, buf);
}

static int hybrid_write_superblock(void) {
    uint8_t buffer[4096];
    for (uint32_t i = 0; i < sizeof(buffer); i++) {
        buffer[i] = 0;
    }
    hybrid_memcpy(buffer, &superblock, sizeof(superblock));
    return hybrid_write_block(0, buffer);
}

static int hybrid_read_superblock(void) {
    uint8_t buffer[4096];
    if (hybrid_read_block(0, buffer) != 0) return -1;
    hybrid_memcpy(&superblock, buffer, sizeof(superblock));
    return 0;
}

static uint32_t hybrid_inodes_per_block(void) {
    return blockdev_block_size() / sizeof(hybrid_inode_t);
}

static uint32_t hybrid_inode_block_count(void) {
    uint32_t per_block = hybrid_inodes_per_block();
    return (superblock.inode_count + per_block - 1) / per_block;
}

static uint32_t hybrid_root_dir_block(void) {
    return 1 + hybrid_inode_block_count();
}

static int hybrid_read_inode(uint32_t ino, hybrid_inode_t *inode) {
    if (!inode || ino == 0 || ino > superblock.inode_count) return -1;
    uint32_t block = 1 + ((ino - 1) / hybrid_inodes_per_block());
    uint32_t index = (ino - 1) % hybrid_inodes_per_block();
    uint8_t buffer[4096];

    if (hybrid_read_block(block, buffer) != 0) return -1;
    hybrid_memcpy(inode, &buffer[index * sizeof(hybrid_inode_t)], sizeof(hybrid_inode_t));
    return 0;
}

static int hybrid_write_inode(const hybrid_inode_t *inode) {
    if (!inode || inode->ino == 0 || inode->ino > superblock.inode_count) return -1;
    uint32_t block = 1 + ((inode->ino - 1) / hybrid_inodes_per_block());
    uint32_t index = (inode->ino - 1) % hybrid_inodes_per_block();
    uint8_t buffer[4096];

    if (hybrid_read_block(block, buffer) != 0) return -1;
    hybrid_memcpy(&buffer[index * sizeof(hybrid_inode_t)], inode, sizeof(hybrid_inode_t));
    return hybrid_write_block(block, buffer);
}

static int hybrid_format(void) {
    uart_puts("[HYBRID] Formatting filesystem...\n");
    superblock.magic = HYBRID_MAGIC;
    superblock.version = HYBRID_VERSION;
    superblock.block_size = blockdev_block_size();
    superblock.total_blocks = blockdev_total_blocks();
    superblock.inode_count = HYBRID_INODE_COUNT;
    superblock.root_inode = HYBRID_ROOT_INO;

    /* Initialize empty inode table */
    uint8_t empty_block[4096];
    hybrid_zero(empty_block, sizeof(empty_block));

    uint32_t inode_blocks = hybrid_inode_block_count();
    for (uint32_t block = 1; block <= inode_blocks; block++) {
        hybrid_write_block(block, empty_block);
    }

    hybrid_inode_t root = {0};
    root.ino = HYBRID_ROOT_INO;
    root.type = FILE_TYPE_DIR;
    root.mode = FILE_MODE_READ | FILE_MODE_WRITE;
    root.uid = 0;
    root.gid = 0;
    root.size = 0;
    root.created = 0;
    root.modified = 0;
    root.direct[0] = hybrid_root_dir_block();

    hybrid_write_inode(&root);

    /* Initialize root directory block */
    hybrid_zero(empty_block, sizeof(empty_block));
    hybrid_write_block(hybrid_root_dir_block(), empty_block);
    return hybrid_write_superblock();
}

static int hybrid_find_dirent(const char *name, hybrid_dirent_t *entry, uint32_t *entry_index) {
    uint8_t buffer[4096];
    if (hybrid_read_block(hybrid_root_dir_block(), buffer) != 0) return -1;

    hybrid_dirent_t *entries = (hybrid_dirent_t *)buffer;
    for (uint32_t i = 0; i < HYBRID_MAX_DIR_ENTRIES; i++) {
        if (entries[i].ino == 0) continue;
        if (entries[i].type == 0) continue;
        if (hybrid_strcmp(entries[i].name, name) == 0) {
            if (entry) hybrid_memcpy(entry, &entries[i], sizeof(hybrid_dirent_t));
            if (entry_index) *entry_index = i;
            return 0;
        }
    }
    return -1;
}

static int hybrid_add_dirent(uint32_t ino, const char *name, uint8_t type) {
    uint8_t buffer[4096];
    if (hybrid_read_block(hybrid_root_dir_block(), buffer) != 0) return -1;

    hybrid_dirent_t *entries = (hybrid_dirent_t *)buffer;
    for (uint32_t i = 0; i < HYBRID_MAX_DIR_ENTRIES; i++) {
        if (entries[i].ino == 0) {
            entries[i].ino = ino;
            entries[i].type = type;
            for (uint32_t j = 0; j < HYBRID_MAX_FILENAME; j++) {
                entries[i].name[j] = name[j] ? name[j] : '\0';
            }
            return hybrid_write_block(hybrid_root_dir_block(), buffer);
        }
    }
    return -1;
}

static int hybrid_allocate_inode(uint32_t type, const char *name, inode_t **out) {
    hybrid_inode_t inode = {0};
    for (uint32_t id = 1; id <= superblock.inode_count; id++) {
        hybrid_inode_t existing;
        hybrid_read_inode(id, &existing);
        if (existing.ino == 0) {
            inode.ino = id;
            inode.type = type;
            inode.mode = FILE_MODE_READ | FILE_MODE_WRITE;
            inode.uid = 0;
            inode.gid = 0;
            inode.size = 0;
            inode.created = 0;
            inode.modified = 0;
            int data_block = blockdev_alloc_block();
            if (data_block < 0) return -1;
            inode.direct[0] = data_block;
            hybrid_write_inode(&inode);

            if (hybrid_add_dirent(id, name, type) != 0) return -1;

            inode_t *file_inode = vfs_alloc_inode();
            if (!file_inode) return -1;
            file_inode->ino = id;
            file_inode->type = type;
            file_inode->mode = inode.mode;
            file_inode->owner = inode.uid;
            file_inode->group = inode.gid;
            file_inode->size = inode.size;
            file_inode->data = mem_alloc(sizeof(hybrid_inode_t));
            if (!file_inode->data) return -1;
            hybrid_memcpy(file_inode->data, &inode, sizeof(hybrid_inode_t));
            *out = file_inode;
            return 0;
        }
    }
    return -1;
}

static int hybrid_load_inode(uint32_t ino, inode_t **out) {
    hybrid_inode_t inode;
    if (hybrid_read_inode(ino, &inode) != 0) return -1;

    inode_t *file_inode = vfs_alloc_inode();
    if (!file_inode) return -1;

    file_inode->ino = ino;
    file_inode->type = inode.type;
    file_inode->mode = inode.mode;
    file_inode->owner = inode.uid;
    file_inode->group = inode.gid;
    file_inode->size = inode.size;
    file_inode->data = mem_alloc(sizeof(hybrid_inode_t));
    if (!file_inode->data) return -1;
    hybrid_memcpy(file_inode->data, &inode, sizeof(hybrid_inode_t));

    *out = file_inode;
    return 0;
}

static int hybrid_read_file(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len) {
    if (!inode || !buf) return -1;
    hybrid_inode_t *hybrid = (hybrid_inode_t *)inode->data;
    if (!hybrid) return -1;
    if (offset >= hybrid->size) return 0;

    uint64_t remaining = len;
    uint64_t position = offset;
    uint64_t bytes_read = 0;
    uint8_t block_buffer[4096];

    while (remaining > 0 && position < hybrid->size) {
        uint32_t block_index = position / blockdev_block_size();
        if (block_index >= HYBRID_DIRECT_BLOCKS) break;
        uint32_t block_num = hybrid->direct[block_index];
        if (block_num == 0) break;

        if (hybrid_read_block(block_num, block_buffer) != 0) break;

        uint64_t block_offset = position % blockdev_block_size();
        uint64_t chunk = blockdev_block_size() - block_offset;
        if (chunk > remaining) chunk = remaining;
        if (chunk > hybrid->size - position) chunk = hybrid->size - position;

        hybrid_memcpy(buf + bytes_read, block_buffer + block_offset, chunk);
        bytes_read += chunk;
        position += chunk;
        remaining -= chunk;
    }

    return (int)bytes_read;
}

static int hybrid_write_file(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len) {
    if (!inode || !buf) return -1;
    hybrid_inode_t *hybrid = (hybrid_inode_t *)inode->data;
    if (!hybrid) return -1;

    uint64_t remaining = len;
    uint64_t position = offset;
    uint64_t bytes_written = 0;
    uint8_t block_buffer[4096];

    while (remaining > 0) {
        uint32_t block_index = position / blockdev_block_size();
        if (block_index >= HYBRID_DIRECT_BLOCKS) return -1;

        if (hybrid->direct[block_index] == 0) {
            int new_block = blockdev_alloc_block();
            if (new_block < 0) return -1;
            hybrid->direct[block_index] = new_block;
        }

        uint32_t block_num = hybrid->direct[block_index];
        if (hybrid_read_block(block_num, block_buffer) != 0) return -1;

        uint64_t block_offset = position % blockdev_block_size();
        uint64_t chunk = blockdev_block_size() - block_offset;
        if (chunk > remaining) chunk = remaining;

        hybrid_memcpy(block_buffer + block_offset, buf + bytes_written, chunk);
        if (hybrid_write_block(block_num, block_buffer) != 0) return -1;

        bytes_written += chunk;
        position += chunk;
        remaining -= chunk;
    }

    if (position > hybrid->size) {
        hybrid->size = position;
        inode->size = position;
        hybrid->modified = hybrid->created + 1;
        hybrid_write_inode(hybrid);
    }

    return (int)bytes_written;
}

static int hybrid_init(void) {
    uart_puts("[HYBRID] Calling blockdev_init\n");
    blockdev_init();
    uart_puts("[HYBRID] Blockdev initialized, reading superblock\n");
    if (hybrid_read_superblock() != 0 || superblock.magic != HYBRID_MAGIC) {
        uart_puts("[HYBRID] Filesystem not found, formatting...\n");
        hybrid_format();
    } else {
        uart_puts("[HYBRID] Filesystem found, loading...\n");
    }
    hybrid_initialized = 1;
    uart_puts("[HYBRID] Filesystem ready\n");
    return 0;
}

static int hybrid_mount(const char *path) {
    uart_puts("[HYBRID] Mounting filesystem at: ");
    uart_puts(path);
    uart_puts("\n");
    if (!hybrid_initialized) {
        uart_puts("[HYBRID] Calling hybrid_init\n");
        return hybrid_init();
    }
    return 0;
}

static int hybrid_open(const char *path, int flags, inode_t **inode_out) {
    if (!path || !inode_out) return -1;

    const char *name = path;
    if (*name == '/') name++;
    if (*name == '\0') return -1;

    hybrid_dirent_t entry;
    if (hybrid_find_dirent(name, &entry, NULL) == 0) {
        return hybrid_load_inode(entry.ino, inode_out);
    }

    if (flags & O_CREAT) {
        return hybrid_allocate_inode(FILE_TYPE_REGULAR, name, inode_out);
    }

    return -1;
}

static int hybrid_close(inode_t *inode) {
    if (!inode) return -1;
    vfs_free_inode(inode);
    return 0;
}

static int hybrid_read(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len) {
    return hybrid_read_file(inode, offset, buf, len);
}

static int hybrid_write(inode_t *inode, uint64_t offset, uint8_t *buf, uint64_t len) {
    return hybrid_write_file(inode, offset, buf, len);
}

static filesystem_t hybrid_fs = {
    .name = "hybridfs",
    .init = hybrid_init,
    .mount = hybrid_mount,
    .open = hybrid_open,
    .close = hybrid_close,
    .read = hybrid_read,
    .write = hybrid_write
};

filesystem_t *get_hybrid_fs(void) {
    return &hybrid_fs;
}
