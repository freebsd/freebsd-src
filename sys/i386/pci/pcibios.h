/**************************************************************************
**
**  $Id: pcibios.h,v 2.0 94/07/10 15:53:32 wolf Rel $
**
**  #define   for pci-bus bios functions.
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
**  $Log:	pcibios.h,v $
**  Revision 2.0  94/07/10  15:53:32  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:23  wolf
**  Beta release.
**  
***************************************************************************
*/

#ifndef __PCIBIOS_H__
#define __PCIBIOS_H__

/*
**	the availability of a pci bus.
**	configuration mode (1 or 2)
**	0 if no pci bus found.
*/

int pci_conf_mode (void);

/*
**	get a "ticket" for accessing a pci device
**	configuration space.
*/

pcici_t pcitag (unsigned char bus,
		unsigned char device,
                unsigned char func);

/*
**	read or write the configuration space.
*/

u_long pci_conf_read  (pcici_t tag, u_long reg		   );
void   pci_conf_write (pcici_t tag, u_long reg, u_long data);

#endif
