/* tdfx_vars.h -- constants and structs used in the tdfx driver
	Copyright (C) 2000 by Coleman Kane <cokane@pohl.ececs.uc.edu>
*/
#ifndef	TDFX_VARS_H
#define	TDFX_VARS_H

#include <sys/memrange.h>

#define	CDEV_MAJOR	107
#define  PCI_DEVICE_ALLIANCE_AT3D	0x643d1142
#define	PCI_DEVICE_3DFX_VOODOO1		0x0000121a
#define	PCI_DEVICE_3DFX_VOODOO2		0x0002121a
#define	PCI_DEVICE_3DFX_BANSHEE		0x0003121a
#define	PCI_DEVICE_3DFX_VOODOO3		0x0005121a

#define PCI_VENDOR_ID_FREEBSD 0x0
#define PCI_DEVICE_ID_FREEBSD 0x2
#define PCI_COMMAND_FREEBSD 0x4
#define PCI_REVISION_ID_FREEBSD 0x8
#define PCI_BASE_ADDRESS_0_FREEBSD 0x10
#define SST1_PCI_SPECIAL1_FREEBSD 0x40
#define SST1_PCI_SPECIAL2_FREEBSD 0x44
#define SST1_PCI_SPECIAL3_FREEBSD 0x48
#define SST1_PCI_SPECIAL4_FREEBSD 0x54

#define VGA_INPUT_STATUS_1C 0x3DA
#define VGA_MISC_OUTPUT_READ 0x3cc
#define VGA_MISC_OUTPUT_WRITE 0x3c2
#define SC_INDEX 0x3c4
#define SC_DATA  0x3c5

#define PCI_MAP_REG_START 0x10
#define UNIT(m)	(m & 0xf)

/* IOCTL Calls */
#define	TDFX_IOC_TYPE_PIO		0
#define	TDFX_IOC_TYPE_QUERY	'3'
#define	TDFX_IOC_QRY_BOARDS	2
#define	TDFX_IOC_QRY_FETCH	3
#define	TDFX_IOC_QRY_UPDATE	4
#include <sys/param.h>
#include <sys/bus_private.h>
#include <sys/bus.h>
#include <sys/cdefs.h>

struct tdfx_softc {
	int cardno;
	vm_offset_t addr;
	struct resource *memrange, *piorange;
	int memrid, piorid;
	long range;
	int vendor;
	int type;
	int addr0;
	unsigned char bus;
	unsigned char dv;
	struct file *curFile;
	device_t dev;
	struct mem_range_desc mrdesc;
	int busy;
};

struct tdfx_pio_data {
	short port;
	short size;
	int device;
	void *value;
};

#endif /* TDFX_VARS_H */
