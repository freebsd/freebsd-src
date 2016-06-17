// for user apps, make sure data size types are defined
// with 


#define CCPQFCTS_IOC_MAGIC 'Z'

typedef struct {
	__u8 bus;
	__u8 dev_fn;
	__u32 board_id;
} cpqfc_pci_info_struct;

typedef __u32 DriverVer_type;

/* this is nearly duplicated in idashare.h */
typedef struct {
	int lc;			/* Controller number */
	int node;		/* Node (box) number */
	int ld;			/* Logical Drive on this box, if required */
	__u32 nexus;		/* SCSI Nexus */
	void *argp;		/* Argument pointer */
} VENDOR_IOCTL_REQ;


typedef struct {
	char cdb[16];		/* SCSI CDB for the pass-through */
	ushort bus;		/* Target bus on the box */
	ushort pdrive;		/* Physical drive on the box */
	int len;		/* Length of the data area of the CDB */
	int sense_len;		/* Length of the sense data */
	char sense_data[40];	/* Sense data */
	void *bufp;		/* Data area for the CDB */
	char rw_flag;		/* Read CDB or Write CDB */
} cpqfc_passthru_t;

/*
 * Defines for the IOCTLS.
 */

#define VENDOR_READ_OPCODE			0x26
#define VENDOR_WRITE_OPCODE			0x27

#define CPQFCTS_GETPCIINFO _IOR( CCPQFCTS_IOC_MAGIC, 1, cpqfc_pci_info_struct)
#define CPQFCTS_GETDRIVVER _IOR( CCPQFCTS_IOC_MAGIC, 9, DriverVer_type)

#define CPQFCTS_SCSI_PASSTHRU _IOWR( CCPQFCTS_IOC_MAGIC,11, VENDOR_IOCTL_REQ)

/*
 *	We would rather have equivalent generic, low-level driver agnostic 
 *	ioctls that do what CPQFC_IOCTL_FC_TARGET_ADDRESS and 
 *	CPQFC_IOCTL_FC_TDR 0x5388 do, but currently, we do not have them, 
 *	consequently applications would have to know they are talking to cpqfc.
 */

/*
 *	Used to get Fibre Channel WWN and port_id from device
 */

#define CPQFC_IOCTL_FC_TARGET_ADDRESS \
	_IOR( CCPQFCTS_IOC_MAGIC, 13, Scsi_FCTargAddress)

/*
 *	Used to invoke Target Defice Reset for Fibre Channel
 */
 
#define CPQFC_IOCTL_FC_TDR _IO( CCPQFCTS_IOC_MAGIC, 15)
