#include "pci_ecam.h"

static volatile uint32_t *ecam_base;

void pci_ecam_init(uint64_t base) {
    ecam_base = (volatile uint32_t *)base;
}

uint32_t pci_read_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t offset) {
    uint64_t addr = (uint64_t)bus << 28 | (uint64_t)dev << 19 | (uint64_t)func << 16 | (offset & 0xFFC);
    return ecam_base[addr / 4];
}

void pci_write_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t offset, uint32_t value) {
    uint64_t addr = (uint64_t)bus << 28 | (uint64_t)dev << 19 | (uint64_t)func << 16 | (offset & 0xFFC);
    ecam_base[addr / 4] = value;
}

int pci_find_device(uint16_t vendor, uint16_t device, uint32_t *bus_out, uint32_t *dev_out, uint32_t *func_out) {
    for (uint32_t b = 0; b < 1; b++) {
        for (uint32_t d = 0; d < 32; d++) {
            for (uint32_t f = 0; f < 8; f++) {
                uint16_t vid = pci_read_config(b, d, f, PCI_CONFIG_HEADER_VENDOR_ID) & 0xFFFF;
                uint16_t did = pci_read_config(b, d, f, PCI_CONFIG_HEADER_DEVICE_ID) & 0xFFFF;
                if (vid == vendor && did == device) {
                    if (bus_out) *bus_out = b;
                    if (dev_out) *dev_out = d;
                    if (func_out) *func_out = f;
                    return 0;
                }
            }
        }
    }
    return -1;
}

#define PCI_BAR_NUM 6
#define PCI_CMD_MEMSPACE (1 << 1)

static uint64_t pci_bar_addr(uint32_t bus, uint32_t dev, uint32_t func, int bar) {
    uint32_t low = pci_read_config(bus, dev, func, 0x10 + bar * 4);
    pci_write_config(bus, dev, func, 0x10 + bar * 4, 0xFFFFFFFF);
    uint32_t size_low = pci_read_config(bus, dev, func, 0x10 + bar * 4);
    pci_write_config(bus, dev, func, 0x10 + bar * 4, low);
    if (!(low & 0x1)) {
        uint32_t sz = ~(size_low & 0xFFFFFFF0) + 1;
        return (uint64_t)(low & 0xFFFFFFF0);
    }
    return 0;
}

static void gpu_ctrl_hdr_set(uint32_t *buf, uint32_t type) {
    buf[0] = type;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = 0;
}

static void gpu_notify(uint32_t *bar, uint32_t qidx) {
    bar[0x50 / 4] = qidx;
}

static volatile uint32_t *gpu_bar;
static uint32_t gpu_width, gpu_height;
static uint32_t *gpu_fb;
static uint32_t gpu_resource_id;

int gpu_init(void) {
    uint32_t bus, dev, func;
    if (pci_find_device(PCI_VENDOR_VIRTIO, PCI_DEVICE_VIRTIO_GPU, &bus, &dev, &func) < 0)
        return -1;

    uint16_t cmd = pci_read_config(bus, dev, func, 0x04) & 0xFFFF;
    pci_write_config(bus, dev, func, 0x04, cmd | PCI_CMD_MEMSPACE);

    uint64_t bar0 = pci_bar_addr(bus, dev, func, 0);
    gpu_bar = (volatile uint32_t *)bar0;

    uint32_t magic = gpu_bar[0x00 / 4];
    if (magic != 0x74726976) return -1;

    gpu_bar[0x020 / 4] = 0x1;
    while ((gpu_bar[0x020 / 4] & 0x3) != 0x3);

    gpu_bar[0x024 / 4] = 0x1;
    uint32_t feat = gpu_bar[0x010 / 4];
    gpu_bar[0x020 / 4] = 0x3;
    while ((gpu_bar[0x020 / 4] & 0x3) != 0x3);

    gpu_set_mode(1280, 720);
    return 0;
}

int gpu_set_mode(uint32_t width, uint32_t height) {
    gpu_width = width;
    gpu_height = height;

    uint32_t *cmd = (uint32_t *)0x80100000UL;
    cmd[0] = 0x100;
    cmd[1] = 0;
    cmd[2] = 0;
    cmd[3] = 0;
    cmd[4] = 0;
    cmd[5] = 0;
    gpu_notify((uint32_t *)gpu_bar, 0);

    gpu_resource_id = 1;
    gpu_fb = (uint32_t *)0x80200000UL;

    uint32_t *rc = (uint32_t *)0x80101000UL;
    rc[0] = 0x101;
    rc[1] = 0;
    rc[2] = 0;
    rc[3] = 0;
    rc[4] = 0;
    rc[5] = 0;
    rc[6] = gpu_resource_id;
    rc[7] = 0x345;  /* BGRA8888 */
    rc[8] = gpu_width;
    rc[9] = gpu_height;
    rc[10] = 0;
    rc[11] = 0;
    gpu_notify((uint32_t *)gpu_bar, 0);

    uint32_t *ss = (uint32_t *)0x80102000UL;
    ss[0] = 0x103;
    ss[1] = 0;
    ss[2] = 0;
    ss[3] = 0;
    ss[4] = 0;
    ss[5] = 0;
    ss[6] = 0;
    ss[7] = gpu_resource_id;
    ss[8] = gpu_width;
    ss[9] = gpu_height;
    ss[10] = 0;
    ss[11] = 0;
    gpu_notify((uint32_t *)gpu_bar, 0);

    for (uint32_t i = 0; i < gpu_width * gpu_height; i++)
        gpu_fb[i] = 0xFF0E0E1AUL;

    return 0;
}

void *gpu_framebuffer(void) { return gpu_fb; }
uint32_t gpu_width_get(void) { return gpu_width; }
uint32_t gpu_height_get(void) { return gpu_height; }

void gpu_flush(int x, int y, int w, int h) {
    uint32_t *fl = (uint32_t *)0x80103000UL;
    fl[0] = 0x104;
    fl[1] = 0;
    fl[2] = 0;
    fl[3] = 0;
    fl[4] = 0;
    fl[5] = 0;
    fl[6] = gpu_resource_id;
    fl[7] = x;
    fl[8] = y;
    fl[9] = w;
    fl[10] = h;
    fl[11] = 0;
    gpu_notify((uint32_t *)gpu_bar, 0);
}
