/**************************************************************************
**
**  $Id: pci_device.h,v 2.1 94/09/16 08:01:36 wolf Rel $
**
**  #define   for pci based device drivers
**
**  386bsd / FreeBSD
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**-------------------------------------------------------------------------
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
    char    *name;			    /* device (long) name */
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
**  Extended by Garrett Wollman <wollman@halloran-eldar.lcs.mit.edu>
**  for future loadable drivers .
**
**-----------------------------------------------------------
*/

struct pci_device {
	struct
	pci_driver*	pd_driver;
	pcidi_t		pd_device_id;	/* device pci id */
	const char *	pd_name;	/* for future loadable drivers */
	int		pd_flags;
};

#define PDF_LOADABLE	0x01
#define PDF_COVERED	0x02

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
