#ifndef _LINUX_NTFS_FS_H
#define _LINUX_NTFS_FS_H

#include <asm/byteorder.h>

#define NTFS_SECTOR_BITS 9
#define NTFS_SECTOR_SIZE 512

/*
 * Attribute flags (16-bit).
 */
typedef enum {
	ATTR_IS_COMPRESSED      = __constant_cpu_to_le16(0x0001),
	ATTR_COMPRESSION_MASK   = __constant_cpu_to_le16(0x00ff),
					/* Compression method mask. Also,
					 * first illegal value. */
	ATTR_IS_ENCRYPTED       = __constant_cpu_to_le16(0x4000),
	ATTR_IS_SPARSE          = __constant_cpu_to_le16(0x8000),
} __attribute__ ((__packed__)) ATTR_FLAGS;

/*
 * The two zones from which to allocate clusters.
 */
typedef enum {
	MFT_ZONE,
	DATA_ZONE
} NTFS_CLUSTER_ALLOCATION_ZONES;

#endif
