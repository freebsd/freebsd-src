/*
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * $FreeBSD$
 */

#ifndef _VIRTIO_BLK_H
#define _VIRTIO_BLK_H

#include <sys/types.h>

/* Feature bits */
#define VIRTIO_BLK_F_BARRIER	0x0001	/* Does host support barriers? */
#define VIRTIO_BLK_F_SIZE_MAX	0x0002	/* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX	0x0004	/* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY	0x0010	/* Legacy geometry available  */
#define VIRTIO_BLK_F_RO		0x0020	/* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE	0x0040	/* Block size of disk is available*/
#define VIRTIO_BLK_F_SCSI	0x0080	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH	0x0200	/* Cache flush command support */
#define VIRTIO_BLK_F_TOPOLOGY	0x0400	/* Topology information is available */

#define VIRTIO_BLK_ID_BYTES	20	/* ID string length */

struct virtio_blk_config {
	/* The capacity (in 512-byte sectors). */
	uint64_t capacity;
	/* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
	uint32_t size_max;
	/* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
	uint32_t seg_max;
	/* geometry the device (if VIRTIO_BLK_F_GEOMETRY) */
	struct virtio_blk_geometry {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;

	/* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
	uint32_t blk_size;

	/* the next 4 entries are guarded by VIRTIO_BLK_F_TOPOLOGY  */
	/* exponent for physical block per logical block. */
	uint8_t physical_block_exp;
	/* alignment offset in logical blocks. */
	uint8_t alignment_offset;
	/* minimum I/O size without performance penalty in logical blocks. */
	uint16_t min_io_size;
	/* optimal sustained I/O size in logical blocks. */
	uint32_t opt_io_size;
} __packed;

/*
 * Command types
 *
 * Usage is a bit tricky as some bits are used as flags and some are not.
 *
 * Rules:
 *   VIRTIO_BLK_T_OUT may be combined with VIRTIO_BLK_T_SCSI_CMD or
 *   VIRTIO_BLK_T_BARRIER.  VIRTIO_BLK_T_FLUSH is a command of its own
 *   and may not be combined with any of the other flags.
 */

/* These two define direction. */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1

/* This bit says it's a scsi command, not an actual read or write. */
#define VIRTIO_BLK_T_SCSI_CMD	2

/* Cache flush command */
#define VIRTIO_BLK_T_FLUSH	4

/* Get device ID command */
#define VIRTIO_BLK_T_GET_ID	8

/* Barrier before this op. */
#define VIRTIO_BLK_T_BARRIER	0x80000000

/* ID string length */
#define VIRTIO_BLK_ID_BYTES	20

/* This is the first element of the read scatter-gather list. */
struct virtio_blk_outhdr {
	/* VIRTIO_BLK_T* */
	uint32_t type;
	/* io priority. */
	uint32_t ioprio;
	/* Sector (ie. 512 byte offset) */
	uint64_t sector;
};

struct virtio_scsi_inhdr {
	uint32_t errors;
	uint32_t data_len;
	uint32_t sense_len;
	uint32_t residual;
};

/* And this is the final byte of the write scatter-gather list. */
#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1
#define VIRTIO_BLK_S_UNSUPP	2

#endif /* _VIRTIO_BLK_H */
