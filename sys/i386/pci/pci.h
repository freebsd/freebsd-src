/**************************************************************************
**
**  $Id: pci.h,v 2.0 94/07/10 15:53:30 wolf Rel $
**
**  #define   for pci bus device drivers
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
**  $Log:	pci.h,v $
**  Revision 2.0  94/07/10  15:53:30  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:21  wolf
**  Beta release.
**  
***************************************************************************
*/

#ifndef __PCI_H__
#define __PCI_H__

/*
**  main pci initialization function.
**  called at boot time from autoconf.c
*/

void pci_configure(void);

/*
**  pci configuration id
**
**  is constructed from: bus, device & function numbers.
*/

typedef union {
	u_long	 cfg1;
        struct {
		 u_char   enable;
		 u_char   forward;
		 u_short  port;
	       } cfg2;
	} pcici_t;

/*
**  Each pci device has an unique device id.
**  It is used to find a matching driver.
*/

typedef u_long pcidi_t;

#endif	/*__PCI_H__*/
