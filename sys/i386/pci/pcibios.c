/**************************************************************************
**
**  $Id: pcibios.c,v 2.0 94/07/10 15:53:31 wolf Rel $
**
**  #define   for pci-bus bios functions.
**
**-------------------------------------------------------------------------
**
**  Copyright (c) 1994	Wolfgang Stanglmeier, Koeln, Germany
**                             <wolf@dentaro.GUN.de>
**
**  This is a beta version - use with care.
**
**-------------------------------------------------------------------------
**
**  $Log:	pcibios.c,v $
**  Revision 2.0  94/07/10  15:53:31  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:20  wolf
**  Beta release.
**  
***************************************************************************
*/


#include "types.h"
#include "i386/isa/isa.h"
#include "i386/pci/pci.h"
#include "i386/pci/pcibios.h"


extern int printf();

static char pci_mode;

static char ident[] =
	"\n$Id: pcibios.c,v 2.0 94/07/10 15:53:31 wolf Rel $\n"
	"Copyright (c) 1994, Wolfgang Stanglmeier\n";


/*--------------------------------------------------------------------
**
**      Port access
**
**--------------------------------------------------------------------
**
**      @FREEBSD@    inl() and outl() functions are not defined
*/

#define	DIRTY

#ifdef DIRTY

#undef inl
#define inl(port) \
({ u_long data; \
	__asm __volatile("inl %1, %0": "=a" (data): "d" ((u_short)(port))); \
	data; })


#undef outl
#define outl(port, data) \
{__asm __volatile("outl %0, %1"::"a" ((u_long)(data)), "d" ((u_short)(port)));}


#undef inb
#define inb(port) \
({ u_char data; \
	__asm __volatile("inb %1, %0": "=a" (data): "d" ((u_short)(port))); \
	data; })


#undef outb
#define outb(port, data) \
{__asm __volatile("outb %0, %1"::"a" ((u_char)(data)), "d" ((u_short)(port)));}

#endif

/*--------------------------------------------------------------------
**
**      Determine configuration mode
**
**--------------------------------------------------------------------
*/


#define CONF1_ENABLE       0x80000000ul
#define CONF1_ADDR_PORT    0x0cf8
#define CONF1_DATA_PORT    0x0cfc


#define CONF2_ENABLE_PORT  0x0cf8
#define CONF2_FORWARD_PORT 0x0cfa


int pci_conf_mode (void)
{
	u_long result, oldval;

	/*---------------------------------------
	**      Configuration mode 1 ?
	**---------------------------------------
	*/

	oldval = inl (CONF1_ADDR_PORT);
	outl (CONF1_ADDR_PORT, CONF1_ENABLE);
	result = inl (CONF1_ADDR_PORT);
	outl (CONF1_ADDR_PORT, oldval);

	if (result == CONF1_ENABLE) {
		pci_mode = 1;
		return (1);
	};

	/*---------------------------------------
	**      Configuration mode 2 ?
	**---------------------------------------
	*/

	outb (CONF2_ENABLE_PORT,     0);
	outb (CONF2_FORWARD_PORT, 0);
	if (!inb (CONF2_ENABLE_PORT) && !inb (CONF2_FORWARD_PORT)) {
		pci_mode = 2;
		return (2);
	};

	/*---------------------------------------
	**      No PCI bus available.
	**---------------------------------------
	*/
	return (0);
}

/*--------------------------------------------------------------------
**
**      Build a pcitag from bus, device and function number
**
**--------------------------------------------------------------------
*/


pcici_t pcitag (unsigned char bus, 
		unsigned char device,
		unsigned char func)
{
	pcici_t tag;

	tag.cfg1 = 0;
	if (device >= 32) return tag;
	if (func   >=  8) return tag;

	switch (pci_mode) {

	case 1:
		tag.cfg1 = CONF1_ENABLE
			| (((u_long) bus   ) << 16ul)
			| (((u_long) device) << 11ul)
			| (((u_long) func  ) <<  8ul);
		break;
	case 2:
		if (device >= 16) break;
		tag.cfg2.port    = 0xc000 | (device << 8ul);
		tag.cfg2.enable  = 0xf1 | (func << 1ul);
		tag.cfg2.forward = bus;
		break;
	};
	return tag;
}

/*--------------------------------------------------------------------
**
**      Read register from configuration space.
**
**--------------------------------------------------------------------
*/


u_long pci_conf_read (pcici_t tag, u_long reg)
{
	u_long addr, data = 0;

	if (!tag.cfg1) return (0xfffffffful);

	switch (pci_mode) {

	case 1:
		addr = tag.cfg1 | reg & 0xfc;
#ifdef PCI_DEBUG
		printf ("pci_conf_read(1): addr=%x ", addr);
#endif
		outl (CONF1_ADDR_PORT, addr);
		data = inl (CONF1_DATA_PORT);
		outl (CONF1_ADDR_PORT, 0   );
		break;

	case 2:
		addr = tag.cfg2.port | reg & 0xfc;
#ifdef PCI_DEBUG
		printf ("pci_conf_read(2): addr=%x ", addr);
#endif
		outb (CONF2_ENABLE_PORT , tag.cfg2.enable );
		outb (CONF2_FORWARD_PORT, tag.cfg2.forward);

		data = inl ((u_short) addr);

		outb (CONF2_ENABLE_PORT,  0);
		outb (CONF2_FORWARD_PORT, 0);
		break;
	};

#ifdef PCI_DEBUG
	printf ("data=%x\n", data);
#endif

	return (data);
}

/*--------------------------------------------------------------------
**
**      Write register into configuration space.
**
**--------------------------------------------------------------------
*/


void pci_conf_write (pcici_t tag, u_long reg, u_long data)
{
	u_long addr;

	if (!tag.cfg1) return;

	switch (pci_mode) {

	case 1:
		addr = tag.cfg1 | reg & 0xfc;
#ifdef PCI_DEBUG
		printf ("pci_conf_write(1): addr=%x data=%x\n",
			addr, data);
#endif
		outl (CONF1_ADDR_PORT, addr);
		outl (CONF1_DATA_PORT, data);
		outl (CONF1_ADDR_PORT,   0 );
		break;

	case 2:
		addr = tag.cfg2.port | reg & 0xfc;
#ifdef PCI_DEBUG
		printf ("pci_conf_write(2): addr=%x data=%x\n",
			addr, data);
#endif
		outb (CONF2_ENABLE_PORT,  tag.cfg2.enable);
		outb (CONF2_FORWARD_PORT, tag.cfg2.forward);

		outl ((u_short) addr, data);

		outb (CONF2_ENABLE_PORT,  0);
		outb (CONF2_FORWARD_PORT, 0);
		break;
	};
}
