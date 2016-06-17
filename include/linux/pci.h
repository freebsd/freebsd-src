/*
 *	$Id: pci.h,v 1.87 1998/10/11 15:13:12 mj Exp $
 *
 *	PCI defines and function prototypes
 *	Copyright 1994, Drew Eckhardt
 *	Copyright 1997--1999 Martin Mares <mj@ucw.cz>
 *
 *	For more information, please consult the following manuals (look at
 *	http://www.pcisig.com/ for how to get them):
 *
 *	PCI BIOS Specification
 *	PCI Local Bus Specification
 *	PCI to PCI Bridge Specification
 *	PCI System Design Guide
 */

#ifndef LINUX_PCI_H
#define LINUX_PCI_H

/*
 * Under PCI, each device has 256 bytes of configuration address space,
 * of which the first 64 bytes are standardized as follows:
 */
#define PCI_VENDOR_ID		0x00	/* 16 bits */
#define PCI_DEVICE_ID		0x02	/* 16 bits */
#define PCI_COMMAND		0x04	/* 16 bits */
#define  PCI_COMMAND_IO		0x1	/* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY	0x2	/* Enable response in Memory space */
#define  PCI_COMMAND_MASTER	0x4	/* Enable bus mastering */
#define  PCI_COMMAND_SPECIAL	0x8	/* Enable response to special cycles */
#define  PCI_COMMAND_INVALIDATE	0x10	/* Use memory write and invalidate */
#define  PCI_COMMAND_VGA_PALETTE 0x20	/* Enable palette snooping */
#define  PCI_COMMAND_PARITY	0x40	/* Enable parity checking */
#define  PCI_COMMAND_WAIT 	0x80	/* Enable address/data stepping */
#define  PCI_COMMAND_SERR	0x100	/* Enable SERR */
#define  PCI_COMMAND_FAST_BACK	0x200	/* Enable back-to-back writes */
#define  PCI_COMMAND_INTX_DISABLE 0x400 /* INTx Emulation Disable */

#define PCI_STATUS		0x06	/* 16 bits */
#define  PCI_STATUS_CAP_LIST	0x10	/* Support Capability List */
#define  PCI_STATUS_66MHZ	0x20	/* Support 66 Mhz PCI 2.1 bus */
#define  PCI_STATUS_UDF		0x40	/* Support User Definable Features [obsolete] */
#define  PCI_STATUS_FAST_BACK	0x80	/* Accept fast-back to back */
#define  PCI_STATUS_PARITY	0x100	/* Detected parity error */
#define  PCI_STATUS_DEVSEL_MASK	0x600	/* DEVSEL timing */
#define  PCI_STATUS_DEVSEL_FAST	0x000	
#define  PCI_STATUS_DEVSEL_MEDIUM 0x200
#define  PCI_STATUS_DEVSEL_SLOW 0x400
#define  PCI_STATUS_SIG_TARGET_ABORT 0x800 /* Set on target abort */
#define  PCI_STATUS_REC_TARGET_ABORT 0x1000 /* Master ack of " */
#define  PCI_STATUS_REC_MASTER_ABORT 0x2000 /* Set on master abort */
#define  PCI_STATUS_SIG_SYSTEM_ERROR 0x4000 /* Set when we drive SERR */
#define  PCI_STATUS_DETECTED_PARITY 0x8000 /* Set on parity error */

#define PCI_CLASS_REVISION	0x08	/* High 24 bits are class, low 8
					   revision */
#define PCI_REVISION_ID         0x08    /* Revision ID */
#define PCI_CLASS_PROG          0x09    /* Reg. Level Programming Interface */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */

#define PCI_CACHE_LINE_SIZE	0x0c	/* 8 bits */
#define PCI_LATENCY_TIMER	0x0d	/* 8 bits */
#define PCI_HEADER_TYPE		0x0e	/* 8 bits */
#define  PCI_HEADER_TYPE_NORMAL	0
#define  PCI_HEADER_TYPE_BRIDGE 1
#define  PCI_HEADER_TYPE_CARDBUS 2

#define PCI_BIST		0x0f	/* 8 bits */
#define  PCI_BIST_CODE_MASK	0x0f	/* Return result */
#define  PCI_BIST_START		0x40	/* 1 to start BIST, 2 secs or less */
#define  PCI_BIST_CAPABLE	0x80	/* 1 if BIST capable */

/*
 * Base addresses specify locations in memory or I/O space.
 * Decoded size can be determined by writing a value of 
 * 0xffffffff to the register, and reading it back.  Only 
 * 1 bits are decoded.
 */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3	0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4	0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5	0x24	/* 32 bits */
#define  PCI_BASE_ADDRESS_SPACE	0x01	/* 0 = memory, 1 = I/O */
#define  PCI_BASE_ADDRESS_SPACE_IO 0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY 0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32 bit address */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Below 1M [obsolete] */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* prefetchable? */
#define  PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define  PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)
/* bit 1 is reserved if address_space = 1 */

/* Header type 0 (normal devices) */
#define PCI_CARDBUS_CIS		0x28
#define PCI_SUBSYSTEM_VENDOR_ID	0x2c
#define PCI_SUBSYSTEM_ID	0x2e  
#define PCI_ROM_ADDRESS		0x30	/* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE	0x01
#define PCI_ROM_ADDRESS_MASK	(~0x7ffUL)

#define PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */

/* 0x35-0x3b are reserved */
#define PCI_INTERRUPT_LINE	0x3c	/* 8 bits */
#define PCI_INTERRUPT_PIN	0x3d	/* 8 bits */
#define PCI_MIN_GNT		0x3e	/* 8 bits */
#define PCI_MAX_LAT		0x3f	/* 8 bits */

/* Header type 1 (PCI-to-PCI bridges) */
#define PCI_PRIMARY_BUS		0x18	/* Primary bus number */
#define PCI_SECONDARY_BUS	0x19	/* Secondary bus number */
#define PCI_SUBORDINATE_BUS	0x1a	/* Highest bus number behind the bridge */
#define PCI_SEC_LATENCY_TIMER	0x1b	/* Latency timer for secondary interface */
#define PCI_IO_BASE		0x1c	/* I/O range behind the bridge */
#define PCI_IO_LIMIT		0x1d
#define  PCI_IO_RANGE_TYPE_MASK	0x0fUL	/* I/O bridging type */
#define  PCI_IO_RANGE_TYPE_16	0x00
#define  PCI_IO_RANGE_TYPE_32	0x01
#define  PCI_IO_RANGE_MASK	(~0x0fUL)
#define PCI_SEC_STATUS		0x1e	/* Secondary status register, only bit 14 used */
#define PCI_MEMORY_BASE		0x20	/* Memory range behind */
#define PCI_MEMORY_LIMIT	0x22
#define  PCI_MEMORY_RANGE_TYPE_MASK 0x0fUL
#define  PCI_MEMORY_RANGE_MASK	(~0x0fUL)
#define PCI_PREF_MEMORY_BASE	0x24	/* Prefetchable memory range behind */
#define PCI_PREF_MEMORY_LIMIT	0x26
#define  PCI_PREF_RANGE_TYPE_MASK 0x0fUL
#define  PCI_PREF_RANGE_TYPE_32	0x00
#define  PCI_PREF_RANGE_TYPE_64	0x01
#define  PCI_PREF_RANGE_MASK	(~0x0fUL)
#define PCI_PREF_BASE_UPPER32	0x28	/* Upper half of prefetchable memory range */
#define PCI_PREF_LIMIT_UPPER32	0x2c
#define PCI_IO_BASE_UPPER16	0x30	/* Upper half of I/O addresses */
#define PCI_IO_LIMIT_UPPER16	0x32
/* 0x34 same as for htype 0 */
/* 0x35-0x3b is reserved */
#define PCI_ROM_ADDRESS1	0x38	/* Same as PCI_ROM_ADDRESS, but for htype 1 */
/* 0x3c-0x3d are same as for htype 0 */
#define PCI_BRIDGE_CONTROL	0x3e
#define  PCI_BRIDGE_CTL_PARITY	0x01	/* Enable parity detection on secondary interface */
#define  PCI_BRIDGE_CTL_SERR	0x02	/* The same for SERR forwarding */
#define  PCI_BRIDGE_CTL_NO_ISA	0x04	/* Disable bridging of ISA ports */
#define  PCI_BRIDGE_CTL_VGA	0x08	/* Forward VGA addresses */
#define  PCI_BRIDGE_CTL_MASTER_ABORT 0x20  /* Report master aborts */
#define  PCI_BRIDGE_CTL_BUS_RESET 0x40	/* Secondary bus reset */
#define  PCI_BRIDGE_CTL_FAST_BACK 0x80	/* Fast Back2Back enabled on secondary interface */

/* Header type 2 (CardBus bridges) */
#define PCI_CB_CAPABILITY_LIST	0x14
/* 0x15 reserved */
#define PCI_CB_SEC_STATUS	0x16	/* Secondary status */
#define PCI_CB_PRIMARY_BUS	0x18	/* PCI bus number */
#define PCI_CB_CARD_BUS		0x19	/* CardBus bus number */
#define PCI_CB_SUBORDINATE_BUS	0x1a	/* Subordinate bus number */
#define PCI_CB_LATENCY_TIMER	0x1b	/* CardBus latency timer */
#define PCI_CB_MEMORY_BASE_0	0x1c
#define PCI_CB_MEMORY_LIMIT_0	0x20
#define PCI_CB_MEMORY_BASE_1	0x24
#define PCI_CB_MEMORY_LIMIT_1	0x28
#define PCI_CB_IO_BASE_0	0x2c
#define PCI_CB_IO_BASE_0_HI	0x2e
#define PCI_CB_IO_LIMIT_0	0x30
#define PCI_CB_IO_LIMIT_0_HI	0x32
#define PCI_CB_IO_BASE_1	0x34
#define PCI_CB_IO_BASE_1_HI	0x36
#define PCI_CB_IO_LIMIT_1	0x38
#define PCI_CB_IO_LIMIT_1_HI	0x3a
#define  PCI_CB_IO_RANGE_MASK	(~0x03UL)
/* 0x3c-0x3d are same as for htype 0 */
#define PCI_CB_BRIDGE_CONTROL	0x3e
#define  PCI_CB_BRIDGE_CTL_PARITY	0x01	/* Similar to standard bridge control register */
#define  PCI_CB_BRIDGE_CTL_SERR		0x02
#define  PCI_CB_BRIDGE_CTL_ISA		0x04
#define  PCI_CB_BRIDGE_CTL_VGA		0x08
#define  PCI_CB_BRIDGE_CTL_MASTER_ABORT	0x20
#define  PCI_CB_BRIDGE_CTL_CB_RESET	0x40	/* CardBus reset */
#define  PCI_CB_BRIDGE_CTL_16BIT_INT	0x80	/* Enable interrupt for 16-bit cards */
#define  PCI_CB_BRIDGE_CTL_PREFETCH_MEM0 0x100	/* Prefetch enable for both memory regions */
#define  PCI_CB_BRIDGE_CTL_PREFETCH_MEM1 0x200
#define  PCI_CB_BRIDGE_CTL_POST_WRITES	0x400
#define PCI_CB_SUBSYSTEM_VENDOR_ID 0x40
#define PCI_CB_SUBSYSTEM_ID	0x42
#define PCI_CB_LEGACY_MODE_BASE	0x44	/* 16-bit PC Card legacy mode base address (ExCa) */
/* 0x48-0x7f reserved */

/* Capability lists */

#define PCI_CAP_LIST_ID		0	/* Capability ID */
#define  PCI_CAP_ID_PM		0x01	/* Power Management */
#define  PCI_CAP_ID_AGP		0x02	/* Accelerated Graphics Port */
#define  PCI_CAP_ID_VPD		0x03	/* Vital Product Data */
#define  PCI_CAP_ID_SLOTID	0x04	/* Slot Identification */
#define  PCI_CAP_ID_MSI		0x05	/* Message Signalled Interrupts */
#define  PCI_CAP_ID_CHSWP	0x06	/* CompactPCI HotSwap */
#define  PCI_CAP_ID_PCIX	0x07	/* PCI-X */
#define PCI_CAP_LIST_NEXT	1	/* Next capability in the list */
#define PCI_CAP_FLAGS		2	/* Capability defined flags (16 bits) */
#define PCI_CAP_SIZEOF		4

/* Power Management Registers */

#define PCI_PM_PMC              2       /* PM Capabilities Register */
#define  PCI_PM_CAP_VER_MASK	0x0007	/* Version */
#define  PCI_PM_CAP_PME_CLOCK	0x0008	/* PME clock required */
#define  PCI_PM_CAP_RESERVED    0x0010  /* Reserved field */
#define  PCI_PM_CAP_DSI		0x0020	/* Device specific initialization */
#define  PCI_PM_CAP_AUX_POWER	0x01C0	/* Auxilliary power support mask */
#define  PCI_PM_CAP_D1		0x0200	/* D1 power state support */
#define  PCI_PM_CAP_D2		0x0400	/* D2 power state support */
#define  PCI_PM_CAP_PME		0x0800	/* PME pin supported */
#define  PCI_PM_CAP_PME_MASK    0xF800  /* PME Mask of all supported states */
#define  PCI_PM_CAP_PME_D0      0x0800  /* PME# from D0 */
#define  PCI_PM_CAP_PME_D1      0x1000  /* PME# from D1 */
#define  PCI_PM_CAP_PME_D2      0x2000  /* PME# from D2 */
#define  PCI_PM_CAP_PME_D3      0x4000  /* PME# from D3 (hot) */
#define  PCI_PM_CAP_PME_D3cold  0x8000  /* PME# from D3 (cold) */
#define PCI_PM_CTRL		4	/* PM control and status register */
#define  PCI_PM_CTRL_STATE_MASK	0x0003	/* Current power state (D0 to D3) */
#define  PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define  PCI_PM_CTRL_DATA_SEL_MASK	0x1e00	/* Data select (??) */
#define  PCI_PM_CTRL_DATA_SCALE_MASK	0x6000	/* Data scale (??) */
#define  PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */
#define PCI_PM_PPB_EXTENSIONS	6	/* PPB support extensions (??) */
#define  PCI_PM_PPB_B2_B3	0x40	/* Stop clock when in D3hot (??) */
#define  PCI_PM_BPCC_ENABLE	0x80	/* Bus power/clock control enable (??) */
#define PCI_PM_DATA_REGISTER	7	/* (??) */
#define PCI_PM_SIZEOF		8

/* AGP registers */

#define PCI_AGP_VERSION		2	/* BCD version number */
#define PCI_AGP_RFU		3	/* Rest of capability flags */
#define PCI_AGP_STATUS		4	/* Status register */
#define  PCI_AGP_STATUS_RQ_MASK	0xff000000	/* Maximum number of requests - 1 */
#define  PCI_AGP_STATUS_SBA	0x0200	/* Sideband addressing supported */
#define  PCI_AGP_STATUS_64BIT	0x0020	/* 64-bit addressing supported */
#define  PCI_AGP_STATUS_FW	0x0010	/* FW transfers supported */
#define  PCI_AGP_STATUS_RATE4	0x0004	/* 4x transfer rate supported */
#define  PCI_AGP_STATUS_RATE2	0x0002	/* 2x transfer rate supported */
#define  PCI_AGP_STATUS_RATE1	0x0001	/* 1x transfer rate supported */
#define PCI_AGP_COMMAND		8	/* Control register */
#define  PCI_AGP_COMMAND_RQ_MASK 0xff000000  /* Master: Maximum number of requests */
#define  PCI_AGP_COMMAND_SBA	0x0200	/* Sideband addressing enabled */
#define  PCI_AGP_COMMAND_AGP	0x0100	/* Allow processing of AGP transactions */
#define  PCI_AGP_COMMAND_64BIT	0x0020 	/* Allow processing of 64-bit addresses */
#define  PCI_AGP_COMMAND_FW	0x0010 	/* Force FW transfers */
#define  PCI_AGP_COMMAND_RATE4	0x0004	/* Use 4x rate */
#define  PCI_AGP_COMMAND_RATE2	0x0002	/* Use 2x rate */
#define  PCI_AGP_COMMAND_RATE1	0x0001	/* Use 1x rate */
#define PCI_AGP_SIZEOF		12

/* Slot Identification */

#define PCI_SID_ESR		2	/* Expansion Slot Register */
#define  PCI_SID_ESR_NSLOTS	0x1f	/* Number of expansion slots available */
#define  PCI_SID_ESR_FIC	0x20	/* First In Chassis Flag */
#define PCI_SID_CHASSIS_NR	3	/* Chassis Number */

/* Message Signalled Interrupts registers */

#define PCI_MSI_FLAGS		2	/* Various flags */
#define  PCI_MSI_FLAGS_64BIT	0x80	/* 64-bit addresses allowed */
#define  PCI_MSI_FLAGS_QSIZE	0x70	/* Message queue size configured */
#define  PCI_MSI_FLAGS_QMASK	0x0e	/* Maximum queue size available */
#define  PCI_MSI_FLAGS_ENABLE	0x01	/* MSI feature enabled */
#define PCI_MSI_RFU		3	/* Rest of capability flags */
#define PCI_MSI_ADDRESS_LO	4	/* Lower 32 bits */
#define PCI_MSI_ADDRESS_HI	8	/* Upper 32 bits (if PCI_MSI_FLAGS_64BIT set) */
#define PCI_MSI_DATA_32		8	/* 16 bits of data for 32-bit devices */
#define PCI_MSI_DATA_64		12	/* 16 bits of data for 64-bit devices */

/* CompactPCI Hotswap Register */

#define PCI_CHSWP_CSR		2	/* Control and Status Register */
#define  PCI_CHSWP_DHA		0x01	/* Device Hiding Arm */
#define  PCI_CHSWP_EIM		0x02	/* ENUM# Signal Mask */
#define  PCI_CHSWP_PIE		0x04	/* Pending Insert or Extract */
#define  PCI_CHSWP_LOO		0x08	/* LED On / Off */
#define  PCI_CHSWP_PI		0x30	/* Programming Interface */
#define  PCI_CHSWP_EXT		0x40	/* ENUM# status - extraction */
#define  PCI_CHSWP_INS		0x80	/* ENUM# status - insertion */

/* PCI-X registers */

#define PCI_X_CMD		2	/* Modes & Features */
#define  PCI_X_CMD_DPERR_E	0x0001	/* Data Parity Error Recovery Enable */
#define  PCI_X_CMD_ERO		0x0002	/* Enable Relaxed Ordering */
#define  PCI_X_CMD_MAX_READ	0x000c	/* Max Memory Read Byte Count */
#define  PCI_X_CMD_MAX_SPLIT	0x0070	/* Max Outstanding Split Transactions */
#define PCI_X_DEVFN		4	/* A copy of devfn. */
#define PCI_X_BUSNR		5	/* Bus segment number */
#define PCI_X_STATUS		6	/* PCI-X capabilities */
#define  PCI_X_STATUS_64BIT	0x0001	/* 64-bit device */
#define  PCI_X_STATUS_133MHZ	0x0002	/* 133 MHz capable */
#define  PCI_X_STATUS_SPL_DISC	0x0004	/* Split Completion Discarded */
#define  PCI_X_STATUS_UNX_SPL	0x0008	/* Unexpected Split Completion */
#define  PCI_X_STATUS_COMPLEX	0x0010	/* Device Complexity */
#define  PCI_X_STATUS_MAX_READ	0x0060	/* Designed Maximum Memory Read Count */
#define  PCI_X_STATUS_MAX_SPLIT	0x0380	/* Design Max Outstanding Split Trans */
#define  PCI_X_STATUS_MAX_CUM	0x1c00	/* Designed Max Cumulative Read Size */
#define  PCI_X_STATUS_SPL_ERR	0x2000	/* Rcvd Split Completion Error Msg */

/* Include the ID list */

#include <linux/pci_ids.h>

/*
 * The PCI interface treats multi-function devices as independent
 * devices.  The slot/function address of each device is encoded
 * in a single byte as follows:
 *
 *	7:3 = slot
 *	2:0 = function
 */
#define PCI_DEVFN(slot,func)	((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

/* Ioctls for /proc/bus/pci/X/Y nodes. */
#define PCIIOC_BASE		('P' << 24 | 'C' << 16 | 'I' << 8)
#define PCIIOC_CONTROLLER	(PCIIOC_BASE | 0x00)	/* Get controller for PCI device. */
#define PCIIOC_MMAP_IS_IO	(PCIIOC_BASE | 0x01)	/* Set mmap state to I/O space. */
#define PCIIOC_MMAP_IS_MEM	(PCIIOC_BASE | 0x02)	/* Set mmap state to MEM space. */
#define PCIIOC_WRITE_COMBINE	(PCIIOC_BASE | 0x03)	/* Enable/disable write-combining. */

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/config.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/errno.h>

/* File state for mmap()s on /proc/bus/pci/X/Y */
enum pci_mmap_state {
	pci_mmap_io,
	pci_mmap_mem
};

/* This defines the direction arg to the DMA mapping routines. */
#define PCI_DMA_BIDIRECTIONAL	0
#define PCI_DMA_TODEVICE	1
#define PCI_DMA_FROMDEVICE	2
#define PCI_DMA_NONE		3

#define DEVICE_COUNT_COMPATIBLE	4
#define DEVICE_COUNT_IRQ	2
#define DEVICE_COUNT_DMA	2
#define DEVICE_COUNT_RESOURCE	12

#define PCI_ANY_ID (~0)

#define pci_present pcibios_present


#define pci_for_each_dev_reverse(dev) \
	for(dev = pci_dev_g(pci_devices.prev); dev != pci_dev_g(&pci_devices); dev = pci_dev_g(dev->global_list.prev))

#define pci_for_each_bus(bus) \
for(bus = pci_bus_b(pci_root_buses.next); bus != pci_bus_b(&pci_root_buses); bus = pci_bus_b(bus->node.next))

/*
 * The pci_dev structure is used to describe both PCI and ISAPnP devices.
 */
struct pci_dev {
	struct list_head global_list;	/* node in list of all PCI devices */
	struct list_head bus_list;	/* node in per-bus list */
	struct pci_bus	*bus;		/* bus this device is on */
	struct pci_bus	*subordinate;	/* bus this device bridges to */

	void		*sysdata;	/* hook for sys-specific extension */
	struct proc_dir_entry *procent;	/* device entry in /proc/bus/pci */

	unsigned int	devfn;		/* encoded device & function index */
	unsigned short	vendor;
	unsigned short	device;
	unsigned short	subsystem_vendor;
	unsigned short	subsystem_device;
	unsigned int	class;		/* 3 bytes: (base,sub,prog-if) */
	u8		hdr_type;	/* PCI header type (`multi' flag masked out) */
	u8		rom_base_reg;	/* which config register controls the ROM */

	struct pci_driver *driver;	/* which driver has allocated this device */
	void		*driver_data;	/* data private to the driver */
	u64		dma_mask;	/* Mask of the bits of bus address this
					   device implements.  Normally this is
					   0xffffffff.  You only need to change
					   this if your device has broken DMA
					   or supports 64-bit transfers.  */

	u32             current_state;  /* Current operating state. In ACPI-speak,
					   this is D0-D3, D0 being fully functional,
					   and D3 being off. */

	/* device is compatible with these IDs */
	unsigned short vendor_compatible[DEVICE_COUNT_COMPATIBLE];
	unsigned short device_compatible[DEVICE_COUNT_COMPATIBLE];

	/*
	 * Instead of touching interrupt line and base address registers
	 * directly, use the values stored here. They might be different!
	 */
	unsigned int	irq;
	struct resource resource[DEVICE_COUNT_RESOURCE]; /* I/O and memory regions + expansion ROMs */
	struct resource dma_resource[DEVICE_COUNT_DMA];
	struct resource irq_resource[DEVICE_COUNT_IRQ];

	char		name[90];	/* device name */
	char		slot_name[8];	/* slot name */
	int		active;		/* ISAPnP: device is active */
	int		ro;		/* ISAPnP: read only */
	unsigned short	regs;		/* ISAPnP: supported registers */

	/* These fields are used by common fixups */
	unsigned short	transparent:1;	/* Transparent PCI bridge */

	int (*prepare)(struct pci_dev *dev);	/* ISAPnP hooks */
	int (*activate)(struct pci_dev *dev);
	int (*deactivate)(struct pci_dev *dev);
};

#define pci_dev_g(n) list_entry(n, struct pci_dev, global_list)
#define pci_dev_b(n) list_entry(n, struct pci_dev, bus_list)

/*
 *  For PCI devices, the region numbers are assigned this way:
 *
 *	0-5	standard PCI regions
 *	6	expansion ROM
 *	7-10	bridges: address space assigned to buses behind the bridge
 */

#define PCI_ROM_RESOURCE 6
#define PCI_BRIDGE_RESOURCES 7
#define PCI_NUM_RESOURCES 11
  
#define PCI_REGION_FLAG_MASK 0x0fU	/* These bits of resource flags tell us the PCI region flags */

struct pci_bus {
	struct list_head node;		/* node in list of buses */
	struct pci_bus	*parent;	/* parent bus this bridge is on */
	struct list_head children;	/* list of child buses */
	struct list_head devices;	/* list of devices on this bus */
	struct pci_dev	*self;		/* bridge device as seen by parent */
	struct resource	*resource[4];	/* address space routed to this bus */

	struct pci_ops	*ops;		/* configuration access functions */
	void		*sysdata;	/* hook for sys-specific extension */
	struct proc_dir_entry *procdir;	/* directory entry in /proc/bus/pci */

	unsigned char	number;		/* bus number */
	unsigned char	primary;	/* number of primary bridge */
	unsigned char	secondary;	/* number of secondary bridge */
	unsigned char	subordinate;	/* max number of subordinate buses */

	char		name[48];
	unsigned short	vendor;
	unsigned short	device;
	unsigned int	serial;		/* serial number */
	unsigned char	pnpver;		/* Plug & Play version */
	unsigned char	productver;	/* product version */
	unsigned char	checksum;	/* if zero - checksum passed */
	unsigned char	pad1;
};

#define pci_bus_b(n) list_entry(n, struct pci_bus, node)

extern struct list_head pci_root_buses;	/* list of all known PCI buses */
extern struct list_head pci_devices;	/* list of all devices */

extern struct proc_dir_entry *proc_bus_pci_dir;
/*
 * Error values that may be returned by PCI functions.
 */
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

/* Low-level architecture-dependent routines */

struct pci_ops {
	int (*read_byte)(struct pci_dev *, int where, u8 *val);
	int (*read_word)(struct pci_dev *, int where, u16 *val);
	int (*read_dword)(struct pci_dev *, int where, u32 *val);
	int (*write_byte)(struct pci_dev *, int where, u8 val);
	int (*write_word)(struct pci_dev *, int where, u16 val);
	int (*write_dword)(struct pci_dev *, int where, u32 val);
};

struct pbus_set_ranges_data
{
	unsigned long io_start, io_end;
	unsigned long mem_start, mem_end;
	unsigned long prefetch_start, prefetch_end;
};

struct pci_device_id {
	unsigned int vendor, device;		/* Vendor and device ID or PCI_ANY_ID */
	unsigned int subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	unsigned int class, class_mask;		/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;		/* Data private to the driver */
};

struct pci_driver {
	struct list_head node;
	char *name;
	const struct pci_device_id *id_table;	/* NULL if wants all devices */
	int  (*probe)  (struct pci_dev *dev, const struct pci_device_id *id);	/* New device inserted */
	void (*remove) (struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
	int  (*save_state) (struct pci_dev *dev, u32 state);    /* Save Device Context */
	int  (*suspend) (struct pci_dev *dev, u32 state);	/* Device suspended */
	int  (*resume) (struct pci_dev *dev);	                /* Device woken up */
	int  (*enable_wake) (struct pci_dev *dev, u32 state, int enable);   /* Enable wake event */
};

/**
 * PCI_DEVICE - macro used to describe a specific pci device
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific device.  The subvendor and subdevice fields will be set to
 * PCI_ANY_ID.
 */
#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

/**
 * PCI_DEVICE_CLASS - macro used to describe a specific pci device class
 * @dev_class: the class, subclass, prog-if triple for this device
 * @dev_class_mask: the class mask for this device
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific PCI class.  The vendor, device, subvendor, and subdevice 
 * fields will be set to PCI_ANY_ID.
 */
#define PCI_DEVICE_CLASS(dev_class,dev_class_mask) \
	.class = (dev_class), .class_mask = (dev_class_mask), \
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

/* these external functions are only available when PCI support is enabled */
#ifdef CONFIG_PCI

#define pci_for_each_dev(dev) \
	for(dev = pci_dev_g(pci_devices.next); dev != pci_dev_g(&pci_devices); dev = pci_dev_g(dev->global_list.next))

void pcibios_init(void);
void pcibios_fixup_bus(struct pci_bus *);
int pcibios_enable_device(struct pci_dev *, int mask);
char *pcibios_setup (char *str);

/* Used only when drivers/pci/setup.c is used */
void pcibios_align_resource(void *, struct resource *,
			    unsigned long, unsigned long);
void pcibios_update_resource(struct pci_dev *, struct resource *,
			     struct resource *, int);
void pcibios_update_irq(struct pci_dev *, int irq);
void pcibios_fixup_pbus_ranges(struct pci_bus *, struct pbus_set_ranges_data *);

/* Backward compatibility, don't use in new code! */

int pcibios_present(void);
int pcibios_read_config_byte (unsigned char bus, unsigned char dev_fn,
			      unsigned char where, unsigned char *val);
int pcibios_read_config_word (unsigned char bus, unsigned char dev_fn,
			      unsigned char where, unsigned short *val);
int pcibios_read_config_dword (unsigned char bus, unsigned char dev_fn,
			       unsigned char where, unsigned int *val);
int pcibios_write_config_byte (unsigned char bus, unsigned char dev_fn,
			       unsigned char where, unsigned char val);
int pcibios_write_config_word (unsigned char bus, unsigned char dev_fn,
			       unsigned char where, unsigned short val);
int pcibios_write_config_dword (unsigned char bus, unsigned char dev_fn,
				unsigned char where, unsigned int val);
int pcibios_find_class (unsigned int class_code, unsigned short index, unsigned char *bus, unsigned char *dev_fn);
int pcibios_find_device (unsigned short vendor, unsigned short dev_id,
			 unsigned short index, unsigned char *bus,
			 unsigned char *dev_fn);

/* Generic PCI functions used internally */

void pci_init(void);
int pci_bus_exists(const struct list_head *list, int nr);
struct pci_bus *pci_scan_bus(int bus, struct pci_ops *ops, void *sysdata);
struct pci_bus *pci_alloc_primary_bus(int bus);
struct pci_dev *pci_scan_slot(struct pci_dev *temp);
int pci_proc_attach_device(struct pci_dev *dev);
int pci_proc_detach_device(struct pci_dev *dev);
int pci_proc_attach_bus(struct pci_bus *bus);
int pci_proc_detach_bus(struct pci_bus *bus);
void pci_name_device(struct pci_dev *dev);
char *pci_class_name(u32 class);
void pci_read_bridge_bases(struct pci_bus *child);
struct resource *pci_find_parent_resource(const struct pci_dev *dev, struct resource *res);
int pci_setup_device(struct pci_dev *dev);
int pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge);

/* Generic PCI functions exported to card drivers */

struct pci_dev *pci_find_device (unsigned int vendor, unsigned int device, const struct pci_dev *from);
struct pci_dev *pci_find_subsys (unsigned int vendor, unsigned int device,
				 unsigned int ss_vendor, unsigned int ss_device,
				 const struct pci_dev *from);
struct pci_dev *pci_find_class (unsigned int class, const struct pci_dev *from);
struct pci_dev *pci_find_slot (unsigned int bus, unsigned int devfn);
int pci_find_capability (struct pci_dev *dev, int cap);

int pci_read_config_byte(struct pci_dev *dev, int where, u8 *val);
int pci_read_config_word(struct pci_dev *dev, int where, u16 *val);
int pci_read_config_dword(struct pci_dev *dev, int where, u32 *val);
int pci_write_config_byte(struct pci_dev *dev, int where, u8 val);
int pci_write_config_word(struct pci_dev *dev, int where, u16 val);
int pci_write_config_dword(struct pci_dev *dev, int where, u32 val);

int pci_enable_device(struct pci_dev *dev);
int pci_enable_device_bars(struct pci_dev *dev, int mask);
void pci_disable_device(struct pci_dev *dev);
void pci_set_master(struct pci_dev *dev);
#define HAVE_PCI_SET_MWI
int pci_set_mwi(struct pci_dev *dev);
void pci_clear_mwi(struct pci_dev *dev);
int pci_set_dma_mask(struct pci_dev *dev, u64 mask);
int pci_dac_set_dma_mask(struct pci_dev *dev, u64 mask);
int pci_assign_resource(struct pci_dev *dev, int i);

/* Power management related routines */
int pci_save_state(struct pci_dev *dev, u32 *buffer);
int pci_restore_state(struct pci_dev *dev, u32 *buffer);
int pci_set_power_state(struct pci_dev *dev, int state);
int pci_enable_wake(struct pci_dev *dev, u32 state, int enable);

/* Helper functions for low-level code (drivers/pci/setup-[bus,res].c) */

int pci_claim_resource(struct pci_dev *, int);
void pci_assign_unassigned_resources(void);
void pdev_enable_device(struct pci_dev *);
void pdev_sort_resources(struct pci_dev *, struct resource_list *);
unsigned long pci_bridge_check_io(struct pci_dev *);
void pci_fixup_irqs(u8 (*)(struct pci_dev *, u8 *),
		    int (*)(struct pci_dev *, u8, u8));
#define HAVE_PCI_REQ_REGIONS	2
int pci_request_regions(struct pci_dev *, char *);
void pci_release_regions(struct pci_dev *);
int pci_request_region(struct pci_dev *, int, char *);
void pci_release_region(struct pci_dev *, int);

/* New-style probing supporting hot-pluggable devices */
int pci_register_driver(struct pci_driver *);
void pci_unregister_driver(struct pci_driver *);
void pci_insert_device(struct pci_dev *, struct pci_bus *);
void pci_remove_device(struct pci_dev *);
struct pci_driver *pci_dev_driver(const struct pci_dev *);
const struct pci_device_id *pci_match_device(const struct pci_device_id *ids, const struct pci_dev *dev);
void pci_announce_device_to_drivers(struct pci_dev *);
unsigned int pci_do_scan_bus(struct pci_bus *bus);
struct pci_bus * pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev, int busnr);

/* kmem_cache style wrapper around pci_alloc_consistent() */
struct pci_pool *pci_pool_create (const char *name, struct pci_dev *dev,
		size_t size, size_t align, size_t allocation, int flags);
void pci_pool_destroy (struct pci_pool *pool);

void *pci_pool_alloc (struct pci_pool *pool, int flags, dma_addr_t *handle);
void pci_pool_free (struct pci_pool *pool, void *vaddr, dma_addr_t addr);

#endif /* CONFIG_PCI */

/* Include architecture-dependent settings and functions */

#include <asm/pci.h>

/*
 *  If the system does not have PCI, clearly these return errors.  Define
 *  these as simple inline functions to avoid hair in drivers.
 */

#ifndef CONFIG_PCI
static inline int pcibios_present(void) { return 0; }
static inline int pcibios_find_class (unsigned int class_code, unsigned short index, unsigned char *bus, unsigned char *dev_fn) 
{ 	return PCIBIOS_DEVICE_NOT_FOUND; }

#define _PCI_NOP(o,s,t) \
	static inline int pcibios_##o##_config_##s (u8 bus, u8 dfn, u8 where, t val) \
		{ return PCIBIOS_FUNC_NOT_SUPPORTED; } \
	static inline int pci_##o##_config_##s (struct pci_dev *dev, int where, t val) \
		{ return PCIBIOS_FUNC_NOT_SUPPORTED; }
#define _PCI_NOP_ALL(o,x)	_PCI_NOP(o,byte,u8 x) \
				_PCI_NOP(o,word,u16 x) \
				_PCI_NOP(o,dword,u32 x)
_PCI_NOP_ALL(read, *)
_PCI_NOP_ALL(write,)

static inline struct pci_dev *pci_find_device(unsigned int vendor, unsigned int device, const struct pci_dev *from)
{ return NULL; }

static inline struct pci_dev *pci_find_class(unsigned int class, const struct pci_dev *from)
{ return NULL; }

static inline struct pci_dev *pci_find_slot(unsigned int bus, unsigned int devfn)
{ return NULL; }

static inline struct pci_dev *pci_find_subsys(unsigned int vendor, unsigned int device,
unsigned int ss_vendor, unsigned int ss_device, const struct pci_dev *from)
{ return NULL; }

static inline void pci_set_master(struct pci_dev *dev) { }
static inline int pci_enable_device_bars(struct pci_dev *dev, int mask) { return -EBUSY; }
static inline int pci_enable_device(struct pci_dev *dev) { return -EIO; }
static inline void pci_disable_device(struct pci_dev *dev) { }
static inline int pci_module_init(struct pci_driver *drv) { return -ENODEV; }
static inline int pci_set_dma_mask(struct pci_dev *dev, u64 mask) { return -EIO; }
static inline int pci_dac_set_dma_mask(struct pci_dev *dev, u64 mask) { return -EIO; }
static inline int pci_assign_resource(struct pci_dev *dev, int i) { return -EBUSY;}
static inline int pci_register_driver(struct pci_driver *drv) { return 0;}
static inline void pci_unregister_driver(struct pci_driver *drv) { }
static inline int scsi_to_pci_dma_dir(unsigned char scsi_dir) { return scsi_dir; }
static inline int pci_find_capability (struct pci_dev *dev, int cap) {return 0; }
static inline const struct pci_device_id *pci_match_device(const struct pci_device_id *ids, const struct pci_dev *dev) { return NULL; }

/* Power management related routines */
static inline int pci_save_state(struct pci_dev *dev, u32 *buffer) { return 0; }
static inline int pci_restore_state(struct pci_dev *dev, u32 *buffer) { return 0; }
static inline int pci_set_power_state(struct pci_dev *dev, int state) { return 0; }
static inline int pci_enable_wake(struct pci_dev *dev, u32 state, int enable) { return 0; }

#define pci_for_each_dev(dev) \
	for(dev = NULL; 0; )

#else

/*
 * a helper function which helps ensure correct pci_driver
 * setup and cleanup for commonly-encountered hotplug/modular cases
 *
 * This MUST stay in a header, as it checks for -DMODULE
 */
static inline int pci_module_init(struct pci_driver *drv)
{
	int rc = pci_register_driver (drv);

	if (rc > 0)
		return 0;

	/* iff CONFIG_HOTPLUG and built into kernel, we should
	 * leave the driver around for future hotplug events.
	 * For the module case, a hotplug daemon of some sort
	 * should load a module in response to an insert event. */
#if defined(CONFIG_HOTPLUG) && !defined(MODULE)
	if (rc == 0)
		return 0;
#else
	if (rc == 0)
		rc = -ENODEV;		
#endif

	/* if we get here, we need to clean up pci driver instance
	 * and return some sort of error */
	pci_unregister_driver (drv);
	
	return rc;
}

#endif /* !CONFIG_PCI */

/* these helpers provide future and backwards compatibility
 * for accessing popular PCI BAR info */
#define pci_resource_start(dev,bar)   ((dev)->resource[(bar)].start)
#define pci_resource_end(dev,bar)     ((dev)->resource[(bar)].end)
#define pci_resource_flags(dev,bar)   ((dev)->resource[(bar)].flags)
#define pci_resource_len(dev,bar) \
	((pci_resource_start((dev),(bar)) == 0 &&	\
	  pci_resource_end((dev),(bar)) ==		\
	  pci_resource_start((dev),(bar))) ? 0 :	\
	  						\
	 (pci_resource_end((dev),(bar)) -		\
	  pci_resource_start((dev),(bar)) + 1))

/* Similar to the helpers above, these manipulate per-pci_dev
 * driver-specific data.  Currently stored as pci_dev::driver_data,
 * a void pointer, but it is not present on older kernels.
 */
static inline void *pci_get_drvdata (struct pci_dev *pdev)
{
	return pdev->driver_data;
}

static inline void pci_set_drvdata (struct pci_dev *pdev, void *data)
{
	pdev->driver_data = data;
}

static inline char *pci_name(struct pci_dev *pdev)
{
	return pdev->slot_name;
}

/*
 *  The world is not perfect and supplies us with broken PCI devices.
 *  For at least a part of these bugs we need a work-around, so both
 *  generic (drivers/pci/quirks.c) and per-architecture code can define
 *  fixup hooks to be called for particular buggy devices.
 */

struct pci_fixup {
	int pass;
	u16 vendor, device;			/* You can use PCI_ANY_ID here of course */
	void (*hook)(struct pci_dev *dev);
};

extern struct pci_fixup pcibios_fixups[];

#define PCI_FIXUP_HEADER	1		/* Called immediately after reading configuration header */
#define PCI_FIXUP_FINAL		2		/* Final phase of device fixups */

void pci_fixup_device(int pass, struct pci_dev *dev);

extern int pci_pci_problems;
#define PCIPCI_FAIL		1
#define PCIPCI_TRITON		2
#define PCIPCI_NATOMA		4
#define PCIPCI_VIAETBF		8
#define PCIPCI_VSFX		16
#define PCIPCI_ALIMAGIK		32

#endif /* __KERNEL__ */
#endif /* LINUX_PCI_H */
