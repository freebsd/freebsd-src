/**************************************************************************
**
**  $Id: pci_config.c,v 1.9 1994/10/13 01:12:30 se Exp $
**
**  @PCI@ this should be part of "ioconf.c".
**
**  The config-utility should build it!
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
***************************************************************************
*/

#include <sys/types.h>
#include <i386/pci/pcireg.h>

#include <ncr.h>
#if NNCR>0
extern struct pci_driver ncr_device;
#endif

#include <de.h>
#if NDE > 0
extern struct pci_driver dedevice;
#endif

#include <ahc.h>
#if NAHC > 0
extern struct pci_driver ahc_device;
#endif

extern struct pci_driver chipset_device;
extern struct pci_driver vga_device;
extern struct pci_driver ign_device;
extern struct pci_driver lkm_device;

struct pci_device pci_devtab[] = {

#if NNCR>0
	{&ncr_device,     "ncr",      0 },
#endif

#if NDE>0
	{&dedevice,       "de",       0 },
#endif

#if NAHC>0
	{&ahc_device,	  "ahc",      0 },
#endif

	{&chipset_device, "chip",     0 },
	{&vga_device,     "graphics", 0 },
	{&ign_device,     "ign",      0 },
	{&lkm_device,     "lkm",      0 },
	{0,               0,          0 }
};
