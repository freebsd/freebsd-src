#ifndef _PCI_IOCTL_H
#define	_PCI_IOCTL_H	1

#include <sys/ioccom.h>

struct pcisel {
    u_char		pc_bus;		/* bus number */
    u_char		pc_dev;		/* device on this bus */
    u_char		pc_func;	/* function on this device */
};

struct	pci_conf {
    struct pcisel	pc_sel;		/* bus+slot+function */
    u_char		pc_hdr;		/* PCI header type */
    pcidi_t		pc_devid;	/* device ID */
    pcidi_t		pc_subid;	/* subvendor ID */
    u_int32_t		pc_class;	/* device class */
    struct pci_device	*pc_dvp;	/* device driver pointer or NULL */
    struct pcicb	*pc_cb;		/* pointer to bus parameters */
};

struct	pci_conf_io {
    size_t		pci_len;	/* length of buffer */
    struct pci_conf	*pci_buf;	/* buffer */
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
