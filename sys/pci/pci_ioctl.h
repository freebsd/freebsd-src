#ifndef _PCI_IOCTL_H
#define	_PCI_IOCTL_H	1

#include <sys/ioccom.h>


#define PCI_MAXNAMELEN	16	/* max no. of characters in a device name */

typedef enum {
    PCI_GETCONF_LAST_DEVICE,
    PCI_GETCONF_LIST_CHANGED,
    PCI_GETCONF_MORE_DEVS,
    PCI_GETCONF_ERROR
} pci_getconf_status;

typedef enum {
    PCI_GETCONF_NO_MATCH	= 0x00,
    PCI_GETCONF_MATCH_BUS	= 0x01,
    PCI_GETCONF_MATCH_DEV	= 0x02,
    PCI_GETCONF_MATCH_FUNC	= 0x04,
    PCI_GETCONF_MATCH_NAME	= 0x08,
    PCI_GETCONF_MATCH_UNIT	= 0x10,
    PCI_GETCONF_MATCH_VENDOR	= 0x20,
    PCI_GETCONF_MATCH_DEVICE	= 0x40,
    PCI_GETCONF_MATCH_CLASS	= 0x80
} pci_getconf_flags;

struct pcisel {
    u_int8_t		pc_bus;		/* bus number */
    u_int8_t		pc_dev;		/* device on this bus */
    u_int8_t		pc_func;	/* function on this device */
};

struct	pci_conf {
    struct pcisel	pc_sel;		/* bus+slot+function */
    u_int8_t		pc_hdr;		/* PCI header type */
    u_int16_t		pc_subvendor;	/* card vendor ID */
    u_int16_t		pc_subdevice;	/* card device ID, assigned by 
					   card vendor */
    u_int16_t		pc_vendor;	/* chip vendor ID */
    u_int16_t		pc_device;	/* chip device ID, assigned by 
					   chip vendor */
    u_int8_t		pc_class;	/* chip PCI class */
    u_int8_t		pc_subclass;	/* chip PCI subclass */
    u_int8_t		pc_progif;	/* chip PCI programming interface */
    u_int8_t		pc_revid;	/* chip revision ID */
    char		pd_name[PCI_MAXNAMELEN + 1];  /* Name of peripheral 
						         device */
    u_long		pd_unit;	/* Unit number */
};

struct pci_match_conf {
    struct pcisel	pc_sel;		/* bus+slot+function */
    char		pd_name[PCI_MAXNAMELEN + 1];  /* Name of peripheral 
							 device */
    u_long		pd_unit;	/* Unit number */
    u_int16_t		pc_vendor;	/* PCI Vendor ID */
    u_int16_t		pc_device;	/* PCI Device ID */
    u_int8_t		pc_class;	/* PCI class */
    pci_getconf_flags	flags;		/* Matching expression */
};

struct	pci_conf_io {
    u_int32_t		  pat_buf_len;	/* 
					 * Length of buffer passed in from
					 * user space.
					 */
    u_int32_t		  num_patterns; /* 
					 * Number of pci_match_conf structures 
					 * passed in by the user.
					 */
    struct pci_match_conf *patterns;	/*
					 * Patterns passed in by the user.
					 */
    u_int32_t		  match_buf_len;/*
					 * Length of match buffer passed
					 * in by the user.
					 */
    u_int32_t		  num_matches;	/*
					 * Number of matches returned by
					 * the kernel.
					 */
    struct pci_conf	  *matches;	/*
					 * PCI device matches returned by
					 * the kernel.
					 */
    u_int32_t		  offset;	/*
					 * Passed in by the user code to
					 * indicate where the kernel should
					 * start traversing the device list.
					 * The value passed out by the kernel
					 * points to the record immediately
					 * after the last one returned.
					 * i.e. this value may be passed back
					 * unchanged by the user for a
					 * subsequent call.
					 */
    u_int32_t		  generation;	/*
					 * PCI configuration generation.
					 * This only needs to be set if the
					 * offset is set.  The kernel will
					 * compare its current generation
					 * number to the generation passed 
					 * in by the user to determine
					 * whether the PCI device list has 
					 * changed since the user last
					 * called the GETCONF ioctl.
					 */
    pci_getconf_status	  status;	/* 
					 * Status passed back from the
					 * kernel.
					 */
};

struct pci_io {
    struct pcisel	pi_sel;		/* device to operate on */
    int			pi_reg;		/* configuration register to examine */
    int			pi_width;	/* width (in bytes) of read or write */
    u_int32_t		pi_data;	/* data to write or result of read */
};
	

#define	PCIOCGETCONF	_IOWR('p', 1, struct pci_conf_io)
#define	PCIOCREAD	_IOWR('p', 2, struct pci_io)
#define	PCIOCWRITE	_IOWR('p', 3, struct pci_io)
#define	PCIOCATTACHED	_IOWR('p', 4, struct pci_io)

#endif /* _PCI_IOCTL_H */
