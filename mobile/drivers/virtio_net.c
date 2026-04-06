/*
 * VirtIO Network Device Driver
 * uOS(m) - User OS Mobile
 */

#include <stdint.h>
#include <stddef.h>
#include "../kernel/chardev.h"
#include "../kernel/memory_utils.h"

#define VIRTIO_NET_DEVICE_ID 0x1000
#define VIRTIO_NET_VENDOR_ID 0x554D4551  /* "QEMU" */

#define VIRTIO_MMIO_MAGIC_VALUE     0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID        0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES  0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8

#define VIRTIO_NET_F_MAC            (1 << 5)

#define VIRTIO_MAX_DEVICES 8
#define VIRTIO_QUEUE_SIZE 16

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_QUEUE_SIZE];
} virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTIO_QUEUE_SIZE];
} virtq_used_t;

typedef struct {
    virtq_desc_t desc[VIRTIO_QUEUE_SIZE];
    virtq_avail_t avail;
    virtq_used_t used;
    uint16_t free_desc[VIRTIO_QUEUE_SIZE];
    uint16_t free_head;
    uint16_t used_idx;
} virtq_t;

static volatile uint32_t *virtio_net_base = NULL;
static virtq_t virtio_net_queue;
static uint8_t virtio_net_mac[6];

extern void uart_puts(const char *s);
extern void uart_putc(char c);

#define virtio_net_debug(msg) uart_puts("[VIRTIO-NET] " msg "\n")

static void virtio_net_write_reg(uint32_t offset, uint32_t value) {
    if (virtio_net_base) {
        virtio_net_base[offset / 4] = value;
    }
}

static uint32_t virtio_net_read_reg(uint32_t offset) {
    if (virtio_net_base) {
        return virtio_net_base[offset / 4];
    }
    return 0;
}

static int virtio_net_queue_init(virtq_t *vq, int queue_sel) {
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_SEL, queue_sel);
    uint32_t max_size = virtio_net_read_reg(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0) return -1;
    
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_NUM, VIRTIO_QUEUE_SIZE);
    
    /* Allocate queue */
    /* For simplicity, assume physical = virtual */
    uint64_t desc_addr = (uint64_t)&vq->desc;
    uint64_t avail_addr = (uint64_t)&vq->avail;
    uint64_t used_addr = (uint64_t)&vq->used;
    
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_DESC_LOW, desc_addr & 0xFFFFFFFF);
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_DESC_HIGH, desc_addr >> 32);
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_addr & 0xFFFFFFFF);
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_addr >> 32);
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_USED_LOW, used_addr & 0xFFFFFFFF);
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_USED_HIGH, used_addr >> 32);
    
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_READY, 1);
    
    /* Initialize free descriptors */
    for (int i = 0; i < VIRTIO_QUEUE_SIZE; i++) {
        vq->free_desc[i] = i;
    }
    vq->free_head = 0;
    vq->used_idx = 0;
    
    return 0;
}

int virtio_net_init(void) {
    virtio_net_debug("Initializing VirtIO network device");
    
    /* Probe for virtio devices */
    for (int i = 0; i < VIRTIO_MAX_DEVICES; i++) {
        volatile uint32_t *base = (volatile uint32_t *)(0x10001000 + i * 0x1000);
        
        if (base[VIRTIO_MMIO_MAGIC_VALUE / 4] != 0x74726976) continue;  /* "virt" */
        if (base[VIRTIO_MMIO_VERSION / 4] != 2) continue;
        if (base[VIRTIO_MMIO_DEVICE_ID / 4] != VIRTIO_NET_DEVICE_ID) continue;
        
        virtio_net_base = base;
        virtio_net_debug("Found VirtIO network device");
        break;
    }
    
    if (!virtio_net_base) {
        virtio_net_debug("No VirtIO network device found");
        return -1;
    }
    
    /* Reset device */
    virtio_net_write_reg(VIRTIO_MMIO_STATUS, 0);
    
    /* Acknowledge device */
    uint32_t status = virtio_net_read_reg(VIRTIO_MMIO_STATUS);
    status |= VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_net_write_reg(VIRTIO_MMIO_STATUS, status);
    
    /* Driver present */
    status |= VIRTIO_STATUS_DRIVER;
    virtio_net_write_reg(VIRTIO_MMIO_STATUS, status);
    
    /* Negotiate features */
    uint32_t features = virtio_net_read_reg(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= VIRTIO_NET_F_MAC;  /* Only MAC feature for now */
    virtio_net_write_reg(VIRTIO_MMIO_DRIVER_FEATURES, features);
    
    /* Features OK */
    status |= VIRTIO_STATUS_FEATURES_OK;
    virtio_net_write_reg(VIRTIO_MMIO_STATUS, status);
    
    if (!(virtio_net_read_reg(VIRTIO_MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        virtio_net_debug("Feature negotiation failed");
        return -1;
    }
    
    /* Get MAC address */
    for (int i = 0; i < 6; i++) {
        virtio_net_mac[i] = virtio_net_read_reg(0x100 + i * 4) & 0xFF;
    }
    
    virtio_net_debug("MAC address:");
    for (int i = 0; i < 6; i++) {
        uart_putc("0123456789ABCDEF"[(virtio_net_mac[i] >> 4) & 0xF]);
        uart_putc("0123456789ABCDEF"[virtio_net_mac[i] & 0xF]);
        if (i < 5) uart_putc(':');
    }
    uart_puts("\n");
    
    /* Initialize queues */
    if (virtio_net_queue_init(&virtio_net_queue, 0) < 0) {
        virtio_net_debug("Queue initialization failed");
        return -1;
    }
    
    /* Driver OK */
    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_net_write_reg(VIRTIO_MMIO_STATUS, status);
    
    virtio_net_debug("VirtIO network device initialized");
    return 0;
}

int virtio_net_send_packet(const uint8_t *data, size_t len) {
    if (!virtio_net_base || len > 1514) return -1;
    
    /* Get free descriptor */
    if (virtio_net_queue.free_head >= VIRTIO_QUEUE_SIZE) return -1;
    uint16_t desc_idx = virtio_net_queue.free_desc[virtio_net_queue.free_head++];
    
    /* Set up descriptor */
    virtio_net_queue.desc[desc_idx].addr = (uint64_t)data;
    virtio_net_queue.desc[desc_idx].len = len;
    virtio_net_queue.desc[desc_idx].flags = 0;  /* No next */
    virtio_net_queue.desc[desc_idx].next = 0;
    
    /* Add to available ring */
    uint16_t avail_idx = virtio_net_queue.avail.idx % VIRTIO_QUEUE_SIZE;
    virtio_net_queue.avail.ring[avail_idx] = desc_idx;
    virtio_net_queue.avail.idx++;
    
    /* Notify device */
    virtio_net_write_reg(VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    
    return 0;
}

int virtio_net_receive_packet(uint8_t *buffer, size_t *len) {
    if (!virtio_net_base) return -1;
    
    /* Check for used descriptors */
    if (virtio_net_queue.used.idx == virtio_net_queue.used_idx) return -1;
    
    uint16_t used_idx = virtio_net_queue.used_idx % VIRTIO_QUEUE_SIZE;
    uint16_t desc_idx = virtio_net_queue.used.ring[used_idx].id;
    uint32_t pkt_len = virtio_net_queue.used.ring[used_idx].len;
    
    if (pkt_len > *len) pkt_len = *len;
    
    /* Copy packet data */
    memcpy(buffer, (void *)virtio_net_queue.desc[desc_idx].addr, pkt_len);
    *len = pkt_len;
    
    /* Free descriptor */
    virtio_net_queue.free_desc[--virtio_net_queue.free_head] = desc_idx;
    virtio_net_queue.used_idx++;
    
    return 0;
}