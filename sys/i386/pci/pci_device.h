/**************************************************************************
**
**  $Id: pci_device.h,v 2.0 94/07/10 15:53:31 wolf Rel $
**
**  #define   for pci based device drivers
**
**-------------------------------------------------------------------------
**
**  Copyright (c) 1994	Wolfgang Stanglmeier, Koeln, Germany
**			<wolf@dentaro.GUN.de>
**
**  This is a beta version - use with care.
**
**-------------------------------------------------------------------------
**
**  $Log:	pci_device.h,v $
**  Revision 2.0  94/07/10  15:53:31  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:22  wolf
**  Beta release.
**  
***************************************************************************
*/

#ifndef __PCI_DEVICE_H__
#define __PCI_DEVICE_H__

/*------------------------------------------------------------
**
**  Per driver structure.
**
**------------------------------------------------------------
*/

struct pci_driver {
    int     (*probe )(pcici_t pci_ident);   /* test whether device is present */
    int     (*attach)(pcici_t pci_ident);   /* setup driver for a device */
    pcidi_t device_id;			    /* device pci id */
    char    *name;			    /* device name */
    char    *vendor;			    /* device long name */
    int     (*intr)(int);                   /* interupt handler */
};

/*-----------------------------------------------------------
**
**  Per device structure.
**
**  It is initialized by the config utility and should live in
**  "ioconf.c". At the moment there is only one field.
**
**  This is a first attempt to include the pci bus to 386bsd.
**  So this structure may grow ..
**
**-----------------------------------------------------------
*/

struct pci_device {
	struct pci_driver * pd_driver;
};

/*-----------------------------------------------------------
**
**  This table should be generated in file "ioconf.c"
**  by the config program.
**  It is used at boot time by the configuration function
**  pci_configure()
**
**-----------------------------------------------------------
*/

extern struct pci_device pci_devtab[];

/*-----------------------------------------------------------
**
**  This functions may be used by drivers to map devices
**  to virtual and physical addresses. The va and pa
**  addresses are "in/out" parameters. If they are 0
**  on entry, the mapping function assigns an address.
**
**-----------------------------------------------------------
*/

int pci_map_mem (pcici_t tag, u_long entry, u_long  * va, u_long * pa);

int pci_map_port(pcici_t tag, u_long entry, u_short * pa);

#endif /*__PCI_DEVICE_H__*/
