/**************************************************************************
**
**  $Id: pci_config.c,v 1.5 1994/09/28 16:34:09 se Exp $
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
**-------------------------------------------------------------------------
*/

#include <sys/types.h>
#include <i386/pci/pci.h>
#include <i386/pci/pci_device.h>

#include "ncr.h"
#if NNCR>0
extern struct pci_driver ncr810_device;
extern struct pci_driver ncr825_device;
#endif

#include "de.h"
#if NDE > 0
extern struct pci_driver dedevice;
#endif

extern struct pci_driver intel82378_device;
extern struct pci_driver intel82424_device;
extern struct pci_driver intel82375_device;
extern struct pci_driver intel82434_device;

struct pci_device pci_devtab[] = {

#if NNCR>0
	{&ncr810_device, 0x00011000ul, "ncr", 0},
	{&ncr825_device, 0x00031000ul, "ncr", 0},
#else
	{0, 0x00011000ul, "ncr", PDF_LOADABLE},
	{0, 0x00031000ul, "ncr", PDF_LOADABLE},
#endif

#if NDE>0
	{&dedevice, 0x00021011ul, "de", 0}, /* FIXME!!! */
#else
	{0, 0x00021011ul, "de", PDF_LOADABLE}, /* FIXME!!! */
#endif

	{0, 0x10001042ul, "wd", PDF_COVERED},

	{&intel82378_device, 0x04848086, "ichip", 0},
	{&intel82424_device, 0x04838086, "ichip", 0},
	{&intel82375_device, 0x04828086, "ichip", 0},
	{&intel82434_device, 0x04a38086, "ichip", 0},
	{0, 0, 0, 0}
};
