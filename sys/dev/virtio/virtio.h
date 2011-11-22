/*
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * $FreeBSD$
 */

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <sys/types.h>

struct vq_alloc_info;

/* VirtIO device IDs. */
#define VIRTIO_ID_NETWORK	0x01
#define VIRTIO_ID_BLOCK		0x02
#define VIRTIO_ID_CONSOLE	0x03
#define VIRTIO_ID_ENTROPY	0x04
#define VIRTIO_ID_BALLOON	0x05
#define VIRTIO_ID_IOMEMORY	0x06
#define VIRTIO_ID_9P		0x09

/* Status byte for guest to report progress. */
#define VIRTIO_CONFIG_STATUS_RESET	0x00
#define VIRTIO_CONFIG_STATUS_ACK	0x01
#define VIRTIO_CONFIG_STATUS_DRIVER	0x02
#define VIRTIO_CONFIG_STATUS_DRIVER_OK	0x04
#define VIRTIO_CONFIG_STATUS_FAILED	0x80

/*
 * Generate interrupt when the virtqueue ring is
 * completely used, even if we've suppressed them.
 */
#define VIRTIO_F_NOTIFY_ON_EMPTY (1 << 24)

/*
 * The guest should never negotiate this feature; it
 * is used to detect faulty drivers.
 */
#define VIRTIO_F_BAD_FEATURE (1 << 30)

/*
 * Some VirtIO feature bits (currently bits 28 through 31) are
 * reserved for the transport being used (eg. virtio_ring), the
 * rest are per-device feature bits.
 */
#define VIRTIO_TRANSPORT_F_START	28
#define VIRTIO_TRANSPORT_F_END		32

/*
 * Maximum number of virtqueues per device.
 */
#define VIRTIO_MAX_VIRTQUEUES 8

/*
 * Each virtqueue indirect descriptor list must be physically contiguous.
 * To allow us to malloc(9) each list individually, limit the number
 * supported to what will fit in one page. With 4KB pages, this is a limit
 * of 256 descriptors. If there is ever a need for more, we can switch to
 * contigmalloc(9) for the larger allocations, similar to what
 * bus_dmamem_alloc(9) does.
 *
 * Note the sizeof(struct vring_desc) is 16 bytes.
 */
#define VIRTIO_MAX_INDIRECT ((int) (PAGE_SIZE / 16))

/*
 * VirtIO instance variables indices.
 */
#define VIRTIO_IVAR_DEVTYPE		1
#define VIRTIO_IVAR_FEATURE_DESC	2

struct virtio_feature_desc {
	uint64_t	 vfd_val;
	char		*vfd_str;
};

const char *virtio_device_name(uint16_t devid);
int	 virtio_get_device_type(device_t dev);
void	 virtio_set_feature_desc(device_t dev,
	     struct virtio_feature_desc *feature_desc);
void	 virtio_describe(device_t dev, const char *msg,
	     uint64_t features, struct virtio_feature_desc *feature_desc);

/*
 * VirtIO Bus Methods.
 */
uint64_t virtio_negotiate_features(device_t dev, uint64_t child_features);
int	 virtio_alloc_virtqueues(device_t dev, int flags, int nvqs,
	     struct vq_alloc_info *info);
int	 virtio_setup_intr(device_t dev, enum intr_type type);
int	 virtio_with_feature(device_t dev, uint64_t feature);
void	 virtio_stop(device_t dev);
int	 virtio_reinit(device_t dev, uint64_t features);
void	 virtio_reinit_complete(device_t dev);

/*
 * Read/write a variable amount from the device specific (ie, network)
 * configuration region. This region is encoded in the same endian as
 * the guest.
 */
void	 virtio_read_device_config(device_t dev, bus_size_t offset,
	     void *dst, int length);
void	 virtio_write_device_config(device_t dev, bus_size_t offset,
	     void *src, int length);

/* Inlined device specific read/write functions for common lengths. */
#define VIRTIO_RDWR_DEVICE_CONFIG(size, type)				\
static inline type							\
__CONCAT(virtio_read_dev_config_,size)(device_t dev,			\
    bus_size_t offset)							\
{									\
	type val;							\
	virtio_read_device_config(dev, offset, &val, sizeof(type));	\
	return (val);							\
}									\
									\
static inline void							\
__CONCAT(virtio_write_dev_config_,size)(device_t dev,			\
    bus_size_t offset, type val)					\
{									\
	virtio_write_device_config(dev, offset, &val, sizeof(type));	\
}

VIRTIO_RDWR_DEVICE_CONFIG(1, uint8_t);
VIRTIO_RDWR_DEVICE_CONFIG(2, uint16_t);
VIRTIO_RDWR_DEVICE_CONFIG(4, uint32_t);

#endif /* _VIRTIO_H_ */
