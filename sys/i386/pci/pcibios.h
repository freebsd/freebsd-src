/**************************************************************************
**
**  $Id: pcibios.h,v 1.2 1994/09/01 02:01:45 se Exp $
**
**  #define   for pci-bus bios functions.
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
**
**  $Log: pcibios.h,v $
 * Revision 1.2  1994/09/01  02:01:45  se
 * Submitted by:	Wolfgang Stanglmeier <wolf@dentaro.GUN.de>
 * Merged in changes required for NetBSD support (by mycroft@gnu.ai.mit.edu)
 * and support for multiple NCR chips.
 *
**  Revision 2.0.0.1  94/08/18  23:05:36  wolf
**  
**  Copyright message.
**  New function: pci_last_bus().
**  Symbolic names for pci configuration space registers.
**  
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
**	The number of the last available pci bus.
*/

int pci_last_bus  (void);

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


/*
**	Names for PCI configuration space registers.
**	Copied from pcireg.h
*/

#define	PCI_ID_REG			0x00

#define	PCI_COMMAND_STATUS_REG		0x04
#define	PCI_COMMAND_MEM_ENABLE		0x00000002

#define PCI_CLASS_REV_REG		0x08
#define PCI_MAJCLASS_OF(x)		(((x) >> 24) & 0xff)
#define PCI_MINCLASS_OF(x)		(((x) >> 16) & 0xff)
#define PCI_PROGINT_OF(x)		(((x) >>  8) & 0xff)
#define PCI_REV_OF(x)			((x) & 0xff)
enum pci_majclass { PCI_MJC_OLD = 0, PCI_MJC_STOR, PCI_MJC_NET,
		      PCI_MJC_DISPLAY, PCI_MJC_MEDIA, PCI_MJC_MEM,
		      PCI_MJC_BRIDGE };
enum pci_old_minclass { PCI_MIN_OLD = 0, PCI_MIN_OVGA };
enum pci_stor_minclass { PCI_MIN_SCSI = 0, PCI_MIN_IDE, PCI_MIN_FLOP,
			   PCI_MIN_IPI, PCI_MIN_OSTOR };
enum pci_net_minclass { PCI_MIN_ETHER = 0, PCI_MIN_TOKEN, PCI_MIN_FDDI,
			  PCI_MIN_ONET };
enum pci_disp_minclass { PCI_MIN_VGA = 0, PCI_MIN_XGA, PCI_MIN_ODISP };
enum pci_media_minclass { PCI_MIN_VIDEO = 0, PCI_MIN_AUDIO, PCI_MIN_OMEDIA };
enum pci_mem_minclass { PCI_MIN_RAM = 0, PCI_MIN_FLASH, PCI_MIN_OMEM };
enum pci_bridge_minclass { PCI_MIN_HOST = 0, PCI_MIN_ISA, PCI_MIN_EISA,
			     PCI_MIN_MC, PCI_MIN_PCI, PCI_MIN_PCMCIA,
			     PCI_MIN_OBRIDGE };

#define	PCI_MAP_REG_START		0x10
#define	PCI_MAP_REG_END			0x28
#define	PCI_MAP_MEMORY			0x00000000
#define	PCI_MAP_IO			0x00000001
#define	PCI_MAP_MEMORY_TYPE_32BIT	0x00000000
#define	PCI_MAP_MEMORY_TYPE_32BIT_1M	0x00000002
#define	PCI_MAP_MEMORY_TYPE_64BIT	0x00000004
#define	PCI_MAP_MEMORY_TYPE_MASK	0x00000006
#define	PCI_MAP_MEMORY_CACHABLE		0x00000008
#define	PCI_MAP_MEMORY_ADDRESS_MASK	0xfffffff0

#define	PCI_INTERRUPT_REG		0x3c
#define	PCI_INTERRUPT_PIN_MASK		0x0000ff00
#define	PCI_INTERRUPT_PIN_EXTRACT(x)	((((x) & PCI_INTERRUPT_PIN_MASK) >> 8) & 0xff)
#define	PCI_INTERRUPT_LINE_MASK		0x000000ff
#define	PCI_INTERRUPT_LINE_EXTRACT(x)	((((x) & PCI_INTERRUPT_LINE_MASK) >> 0) & 0xff)

#endif
