/**************************************************************************
**
**  $Id: pci_config.c,v 2.0 94/07/10 15:53:30 wolf Rel $
**
**  @PCI@ this should be part of "ioconf.c".
**
**  The config-utility should build it!
**  When struct pci_driver has become stable
**  I'll extend the config utility.
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
**  $Log:	pci_config.c,v $
**  Revision 2.0  94/07/10  15:53:30  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:04:37  wolf
**  Beta release.
**  
***************************************************************************
*/

#include "types.h"
#include "i386/pci/pci.h"
#include "i386/pci/pci_device.h"

#include "ncr.h"
#if NNCR>0
extern struct pci_driver ncrdevice;
#endif

struct pci_device pci_devtab[] = {

#if NNCR>0
	{&ncrdevice},
#endif
	{0}
};
