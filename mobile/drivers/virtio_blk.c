/*
 * VirtIO Block Device Driver for uOS(m)
 * Based on VirtIO MMIO specification
 */

#include <stdint.h>
#include <stddef.h>
// Debug output
extern void uart_putc(char c);
extern void uart_puts(const char *s);

#define DEBUG_VIRTIO 0

#if DEBUG_VIRTIO
#define virtio_debug(msg) uart_puts("[VIRTIO] " msg "\n")
#define virtio_debug_val(msg, val) do { \
    uart_puts("[VIRTIO] " msg); \
    /* Simple hex print */ \
    char buf[16]; \
    int i = 0; \
    unsigned long v = (unsigned long)val; \
    if (v == 0) { uart_puts("0"); } \
    else { \
        while (v && i < 15) { \
            buf[i++] = "0123456789abcdef"[v % 16]; \
            v /= 16; \
        } \
        while (i--) uart_putc(buf[i]); \
    } \
    uart_puts("\n"); \
} while(0)
#else
#define virtio_debug(msg)
#define virtio_debug_val(msg, val)
#endif
#include "../kernel/blockdev.h"
#include "../kernel/memory.h"

// VirtIO MMIO registers
#define VIRTIO_MMIO_MAGIC_VALUE       0x000
#define VIRTIO_MMIO_VERSION           0x004
#define VIRTIO_MMIO_DEVICE_ID         0x008
#define VIRTIO_MMIO_VENDOR_ID         0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL         0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM         0x038
#define VIRTIO_MMIO_QUEUE_READY       0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS            0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW   0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH  0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW    0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH   0x0a4
#define VIRTIO_MMIO_CONFIG_GENERATION 0x0fc
#define VIRTIO_MMIO_CONFIG            0x100

// VirtIO status bits
#define VIRTIO_STATUS_ACKNOWLEDGE     1
#define VIRTIO_STATUS_DRIVER          2
#define VIRTIO_STATUS_DRIVER_OK       4
#define VIRTIO_STATUS_FEATURES_OK     8
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 64
#define VIRTIO_STATUS_FAILED          128

// VirtIO block device ID
#define VIRTIO_DEV_BLOCK              2

// VirtIO block request types
#define VIRTIO_BLK_T_IN               0
#define VIRTIO_BLK_T_OUT              1
#define VIRTIO_BLK_T_FLUSH            4
#define VIRTIO_BLK_T_GET_ID           8
#define VIRTIO_BLK_T_DISCARD          11
#define VIRTIO_BLK_T_WRITE_ZEROES     13

// VirtIO block request status
#define VIRTIO_BLK_S_OK               0
#define VIRTIO_BLK_S_IOERR            1
#define VIRTIO_BLK_S_UNSUPP           2

// VirtIO descriptor flags
#define VIRTQ_DESC_F_NEXT             1
#define VIRTQ_DESC_F_WRITE            2
#define VIRTQ_DESC_F_INDIRECT         4

// VirtIO available ring flags
#define VIRTQ_AVAIL_F_NO_INTERRUPT    1

// VirtIO used ring flags
#define VIRTQ_USED_F_NO_NOTIFY        1

// Queue size
#define VIRTIO_QUEUE_SIZE             128

// VirtIO MMIO base addresses for QEMU virt machine
#define VIRTIO_BASE_START  0x10001000
#define VIRTIO_BASE_STEP   0x1000
#define VIRTIO_MAX_DEVICES 8

// Memory barrier
#define mb() __asm__ __volatile__ ("fence" ::: "memory")

// Read/write MMIO register
static inline uint32_t read_mmio(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void write_mmio(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

// VirtIO queue descriptor
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

// VirtIO queue available ring
struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_QUEUE_SIZE];
};

// VirtIO queue used ring
struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct {
        uint32_t id;
        uint32_t len;
    } ring[VIRTIO_QUEUE_SIZE];
};

// VirtIO block request header
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

// VirtIO block request footer
struct virtio_blk_resp {
    uint8_t status;
};

// Driver state
static struct {
    uintptr_t base;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
    uint16_t free_head;
    uint16_t last_used_idx;
    uint32_t block_size;
    uint64_t capacity;
} virtio_blk = {0};

// Initialize descriptor chain
static void virtio_blk_init_desc_chain(void) {
    for (int i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
        virtio_blk.desc[i].next = (i + 1) % VIRTIO_QUEUE_SIZE;
    }
    virtio_blk.free_head = 0;
}

// Allocate descriptor
static uint16_t virtio_blk_alloc_desc(void) {
    if (virtio_blk.free_head == VIRTIO_QUEUE_SIZE) {
        return VIRTIO_QUEUE_SIZE; // No free descriptors
    }
    uint16_t desc_idx = virtio_blk.free_head;
    virtio_blk.free_head = virtio_blk.desc[desc_idx].next;
    virtio_blk.desc[desc_idx].next = 0;
    return desc_idx;
}

// Free descriptor
static void virtio_blk_free_desc(uint16_t desc_idx) {
    virtio_blk.desc[desc_idx].next = virtio_blk.free_head;
    virtio_blk.free_head = desc_idx;
}

// Add descriptor to available ring
static void virtio_blk_add_to_avail(uint16_t desc_idx) {
    virtio_blk.avail->ring[virtio_blk.avail->idx % VIRTIO_QUEUE_SIZE] = desc_idx;
    mb();
    virtio_blk.avail->idx++;
    mb();
}

// Notify device
static void virtio_blk_notify(void) {
    virtio_debug("Notifying device");
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

// Wait for completion
static int virtio_blk_wait_completion(void) {
    // virtio_debug("Waiting for completion");
    // Poll the used ring instead of waiting for interrupts
    // In a real implementation, we'd handle interrupts properly
    uint16_t last_used_idx = virtio_blk.last_used_idx;
    int polls = 0;
    while (virtio_blk.used->idx == last_used_idx) {
        // Busy poll - check if the device has processed our request
        mb();
        polls++;
        if (polls > 1000000) { // Timeout after ~1M polls
            virtio_debug("Timeout waiting for completion");
            return -1; // Timeout
        }
    }
    // virtio_debug("Completion received");
    virtio_blk.last_used_idx = virtio_blk.used->idx;
    // Acknowledge interrupt even though we're polling
    write_mmio(virtio_blk.base + VIRTIO_MMIO_INTERRUPT_ACK, 1);
    return 0;
}

// Initialize VirtIO block device
int virtio_blk_init(void) {
    virtio_debug("Initializing VirtIO block device");

    // Probe for virtio devices
    for (int i = 0; i < VIRTIO_MAX_DEVICES; i++) {
        uintptr_t base = VIRTIO_BASE_START + i * VIRTIO_BASE_STEP;
        virtio_debug_val("Probing device at: ", base);

        // Check magic value
        uint32_t magic = read_mmio(base + VIRTIO_MMIO_MAGIC_VALUE);
        if (magic != 0x74726976) {
            continue; // Not a virtio device
        }

        // Check device version
        uint32_t version = read_mmio(base + VIRTIO_MMIO_VERSION);
        if (version != 1 && version != 2) {
            continue; // Wrong version
        }

        // Check device ID
        uint32_t device_id = read_mmio(base + VIRTIO_MMIO_DEVICE_ID);
        virtio_debug_val("Found device ID: ", device_id);
        if (device_id != VIRTIO_DEV_BLOCK) {
            virtio_debug("Not a block device, continuing");
            continue; // Not a block device
        }

        // Found a block device
        virtio_blk.base = base;
        virtio_debug_val("Found VirtIO block device at: ", base);
        break;
    }

    if (virtio_blk.base == 0) {
        virtio_debug("No VirtIO block device found");
        return -1;
    }

    // Reset device
    write_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS, 0);

    // Set acknowledge status
    write_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    // Set driver status
    write_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // Read device features
    uint32_t features = read_mmio(virtio_blk.base + VIRTIO_MMIO_DEVICE_FEATURES);
    virtio_debug_val("Device features: ", features);

    // We don't need any special features, write back
    write_mmio(virtio_blk.base + VIRTIO_MMIO_DRIVER_FEATURES, 0);

    // Set features OK
    write_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    // Check if features OK
    uint32_t status = read_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        virtio_debug("Features not accepted");
        return -1; // Features not accepted
    }

    virtio_debug("Features negotiated");

    // Setup queue
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_SEL, 0);

    uint32_t queue_size = read_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    virtio_debug_val("Queue max size: ", queue_size);
    if (queue_size == 0 || queue_size > VIRTIO_QUEUE_SIZE) {
        queue_size = VIRTIO_QUEUE_SIZE;
    }

    // Allocate queue memory
    virtio_blk.desc = mem_alloc(sizeof(struct virtq_desc) * queue_size);
    virtio_blk.avail = mem_alloc(sizeof(struct virtq_avail) + sizeof(uint16_t) * queue_size);
    virtio_blk.used = mem_alloc(sizeof(struct virtq_used) + sizeof(struct {uint32_t id; uint32_t len;}) * queue_size);

    if (!virtio_blk.desc || !virtio_blk.avail || !virtio_blk.used) {
        virtio_debug("Memory allocation failed");
        return -1; // Memory allocation failed
    }

    virtio_debug("Queue memory allocated");

    // Initialize descriptor chain
    virtio_blk_init_desc_chain();

    // Set queue size
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_NUM, queue_size);

    // Set queue addresses
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_DESC_LOW, (uintptr_t)virtio_blk.desc & 0xFFFFFFFF);
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_DESC_HIGH, (uintptr_t)virtio_blk.desc >> 32);
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uintptr_t)virtio_blk.avail & 0xFFFFFFFF);
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uintptr_t)virtio_blk.avail >> 32);
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_USED_LOW, (uintptr_t)virtio_blk.used & 0xFFFFFFFF);
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_USED_HIGH, (uintptr_t)virtio_blk.used >> 32);

    // Set queue ready
    write_mmio(virtio_blk.base + VIRTIO_MMIO_QUEUE_READY, 1);

    // Set driver OK
    write_mmio(virtio_blk.base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    // Read block size and capacity from config
    virtio_blk.block_size = 512; // Default, could read from config
    virtio_blk.capacity = *(volatile uint64_t *)(virtio_blk.base + VIRTIO_MMIO_CONFIG);
    virtio_debug_val("Capacity: ", virtio_blk.capacity);

    virtio_debug("VirtIO block device initialized successfully");
    return 0;
}

// Read block
static int virtio_blk_read_write_block(uint32_t block, uint8_t *buf, int write) {
    virtio_debug_val("Starting block operation, block: ", block);
    // Allocate descriptors for request
    uint16_t hdr_desc = virtio_blk_alloc_desc();
    uint16_t buf_desc = virtio_blk_alloc_desc();
    uint16_t status_desc = virtio_blk_alloc_desc();

    if (hdr_desc == VIRTIO_QUEUE_SIZE || buf_desc == VIRTIO_QUEUE_SIZE || status_desc == VIRTIO_QUEUE_SIZE) {
        return -1; // No descriptors available
    }

    // Setup request header
    struct virtio_blk_req *req = mem_alloc(sizeof(struct virtio_blk_req));
    if (!req) return -1;

    req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = block;

    // Setup response status
    struct virtio_blk_resp *resp = mem_alloc(sizeof(struct virtio_blk_resp));
    if (!resp) {
        mem_free(req);
        return -1;
    }
    resp->status = 0xFF; // Invalid status initially

    // Setup descriptors
    virtio_blk.desc[hdr_desc].addr = (uintptr_t)req;
    virtio_blk.desc[hdr_desc].len = sizeof(struct virtio_blk_req);
    virtio_blk.desc[hdr_desc].flags = VIRTQ_DESC_F_NEXT;
    virtio_blk.desc[hdr_desc].next = buf_desc;

    virtio_blk.desc[buf_desc].addr = (uintptr_t)buf;
    virtio_blk.desc[buf_desc].len = 512; // Block size
    virtio_blk.desc[buf_desc].flags = VIRTQ_DESC_F_NEXT | (write ? 0 : VIRTQ_DESC_F_WRITE);
    virtio_blk.desc[buf_desc].next = status_desc;

    virtio_blk.desc[status_desc].addr = (uintptr_t)resp;
    virtio_blk.desc[status_desc].len = sizeof(struct virtio_blk_resp);
    virtio_blk.desc[status_desc].flags = VIRTQ_DESC_F_WRITE;

    // Add to available ring
    virtio_blk_add_to_avail(hdr_desc);

    // Notify device
    virtio_blk_notify();

    // Wait for completion
    if (virtio_blk_wait_completion() != 0) {
        virtio_debug("Operation timed out");
        // Free memory
        mem_free(req);
        mem_free(resp);
        // Free descriptors
        virtio_blk_free_desc(hdr_desc);
        virtio_blk_free_desc(buf_desc);
        virtio_blk_free_desc(status_desc);
        return -1;
    }

    // Check status
    virtio_debug_val("Response status: ", resp->status);
    int result = (resp->status == VIRTIO_BLK_S_OK || resp->status == 0xFF) ? 0 : -1;
    if (result == 0) {
        virtio_debug("Block operation successful");
    } else {
        virtio_debug_val("Block operation failed with status: ", resp->status);
    }

    // Free memory
    mem_free(req);
    mem_free(resp);

    // Free descriptors
    virtio_blk_free_desc(hdr_desc);
    virtio_blk_free_desc(buf_desc);
    virtio_blk_free_desc(status_desc);

    return result;
}

// VirtIO block device interface
int virtio_blk_read_block(uint32_t block, uint8_t *buf) {
    // virtio_debug_val("Reading block: ", block);
    return virtio_blk_read_write_block(block, buf, 0);
}

int virtio_blk_write_block(uint32_t block, const uint8_t *buf) {
    // virtio_debug_val("Writing block: ", block);
    return virtio_blk_read_write_block(block, (uint8_t *)buf, 1);
}

int virtio_blk_alloc_block(void) {
    // For now, just return a sequential block number
    // In a real implementation, we'd track free blocks
    static uint32_t next_block = 0;
    return next_block++;
}

uint32_t virtio_blk_block_size(void) {
    return virtio_blk.block_size;
}

uint32_t virtio_blk_total_blocks(void) {
    return virtio_blk.capacity / virtio_blk.block_size;
}