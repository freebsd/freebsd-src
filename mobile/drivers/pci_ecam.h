#ifndef _PCI_ECAM_H_
#define _PCI_ECAM_H_

#include <stdint.h>

#define PCI_VENDOR_VIRTIO 0x1AF4
#define PCI_DEVICE_VIRTIO_GPU 0x1050
#define PCI_DEVICE_VIRTIO_BLK 0x1042
#define PCI_DEVICE_VIRTIO_NET 0x1041

#define PCI_CONFIG_HEADER_VENDOR_ID 0x00
#define PCI_CONFIG_HEADER_DEVICE_ID 0x02

uint32_t pci_read_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t offset);
void pci_write_config(uint32_t bus, uint32_t dev, uint32_t func, uint32_t offset, uint32_t value);
int pci_find_device(uint16_t vendor, uint16_t device, uint32_t *bus_out, uint32_t *dev_out, uint32_t *func_out);

int gpu_init(void);
int gpu_set_mode(uint32_t width, uint32_t height);
void *gpu_framebuffer(void);
void gpu_flush(int x, int y, int w, int h);

#endif
