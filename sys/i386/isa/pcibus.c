/**************************************************************************
**
**  $Id: pcibus.c,v 1.8.4.6 1996/06/26 04:36:43 davidg Exp $
**
**  pci bus subroutines for i386 architecture.
**
**  FreeBSD
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/cpu.h> /* bootverbose */

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/pcibus.h>

/*-----------------------------------------------------------------
**
**	The following functions are provided by the pci bios.
**	They are used only by the pci configuration.
**
**	pcibus_setup():
**		Probes for a pci system.
**		Sets pci_maxdevice and pci_mechanism.
**
**	pcibus_tag():
**		Creates a handle for pci configuration space access.
**		This handle is given to the read/write functions.
**
**	pcibus_ftag():
**		Creates a modified handle.
**
**	pcibus_read():
**		Read a long word from the pci configuration space.
**		Requires a tag (from pcitag) and the register
**		number (should be a long word alligned one).
**
**	pcibus_write():
**		Writes a long word to the pci configuration space.
**		Requires a tag (from pcitag), the register number
**		(should be a long word alligned one), and a value.
**
**	pcibus_regirq():
**		Register an interupt handler for a pci device.
**		Requires a tag (from pcitag), the register number
**		(should be a long word alligned one), and a value.
**
**-----------------------------------------------------------------
*/

static int
pcibus_check (void);

static void
pcibus_setup (void);

static pcici_t
pcibus_tag (u_char bus, u_char device, u_char func);

static pcici_t
pcibus_ftag (pcici_t tag, u_char func);

static u_long
pcibus_read (pcici_t tag, u_long reg);

static void
pcibus_write (pcici_t tag, u_long reg, u_long data);

static int
pcibus_ihandler_attach (int irq, void(*ihandler)(), int arg, unsigned* maskp);

static int
pcibus_ihandler_detach (int irq, void(*handler)());

static int
pcibus_imask_include (int irq, unsigned* maskptr);

static int
pcibus_imask_exclude (int irq, unsigned* maskptr);

struct pcibus i386pci = {
	"pci",
	pcibus_setup,
	pcibus_tag,
	pcibus_ftag,
	pcibus_read,
	pcibus_write,
	ICU_LEN,
	pcibus_ihandler_attach,
	pcibus_ihandler_detach,
	pcibus_imask_include,
	pcibus_imask_exclude,
};

/*
**	Announce structure to generic driver
*/

DATA_SET (pcibus_set, i386pci);

/*--------------------------------------------------------------------
**
**      Determine configuration mode
**
**--------------------------------------------------------------------
*/


#define CONF1_ADDR_PORT    0x0cf8
#define CONF1_DATA_PORT    0x0cfc

#define CONF1_ENABLE       0x80000000ul
#define CONF1_ENABLE_CHK   0x80000000ul
#define CONF1_ENABLE_CHK1  0xFF000001ul
#define CONF1_ENABLE_MSK1  0x80000000ul
#define CONF1_ENABLE_RES1  0x80000000ul

#define CONF2_ENABLE_PORT  0x0cf8
#define CONF2_FORWARD_PORT 0x0cfa

#define CONF2_ENABLE_CHK   0x0e
#define CONF2_ENABLE_RES   0x0e

static int
pcibus_check (void)
{
	u_char device;

	if (bootverbose) printf ("pcibus_check:\tdevice ");

	for (device = 0; device < pci_maxdevice; device++) {
		unsigned long id;
		if (bootverbose) 
			printf ("%d ", device);
		id = pcibus_read (pcibus_tag (0,device,0), 0);
		if (id && id != 0xfffffffful) {
			if (bootverbose) printf ("is there (id=%08lx)\n", id);
			return 1;
		}
	}
	if (bootverbose) 
		printf ("-- nothing found\n");
	return 0;
}

static void
pcibus_setup (void)
{
	unsigned long mode1res,oldval;
	unsigned char mode2res;

	oldval = inl (CONF1_ADDR_PORT);
	outl (CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
	outb (CONF2_ENABLE_PORT, CONF2_ENABLE_CHK);
	mode1res = inl(CONF1_ADDR_PORT);
	mode2res = inb(CONF2_ENABLE_PORT);
	outb (CONF2_ENABLE_PORT, 0);
	outl (CONF1_ADDR_PORT, oldval);

	if (bootverbose) {
		printf ("pcibus_setup(1):\tmode1res=0x%08lx (0x%08lx), "
			"mode2res=0x%02x (0x%02x)\n",
			mode1res,CONF1_ENABLE_CHK,
			(int)mode2res,CONF2_ENABLE_CHK);
	}

	/*---------------------------------------
	**	No PCI, if neither mode1res nor mode2res could be read back
	**---------------------------------------
	*/

	if ((mode1res != CONF1_ENABLE_CHK) 
	    && ((mode1res & -4) != ((CONF1_ENABLE_CHK+CONF2_ENABLE_CHK) & -4))
	    && (mode2res != CONF2_ENABLE_RES)) {
		return;
	}

	/*---------------------------------------
	**      Assume configuration mechanism 1 for now ...
	**---------------------------------------
	*/

	pci_mechanism = 1;
	pci_maxdevice = 32;

	outl (CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
	outb (CONF1_ADDR_PORT +3, 0);
	mode1res = inl (CONF1_ADDR_PORT);
	outl (CONF1_ADDR_PORT, oldval);

	if (bootverbose)
		printf ("pcibus_setup(2):\tmode1res=0x%08lx (0x%08lx)\n", 
			mode1res, CONF1_ENABLE_CHK);

	if (mode1res) {
		if (pcibus_check()) 
			return;
	};

	outl (CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
	mode1res = inl(CONF1_ADDR_PORT);
	outl (CONF1_ADDR_PORT, oldval);

	if (bootverbose)
		printf ("pcibus_setup(3):\tmode1res=0x%08lx (0x%08lx)\n", 
			mode1res, CONF1_ENABLE_CHK1);

	if ((mode1res & CONF1_ENABLE_MSK1) == CONF1_ENABLE_RES1) {
		if (pcibus_check()) 
			return;
	};

	/*---------------------------------------
	**      Try configuration mechanism 2 ...
	**---------------------------------------
	*/

	if (bootverbose)
		printf ("pcibus_setup(4):\tnow trying mechanism 2\n");

	pci_mechanism = 2;
	pci_maxdevice = 16;

	if (pcibus_check()) 
	    return;

	/*---------------------------------------
	**      No PCI bus host bridge found
	**---------------------------------------
	*/

	pci_mechanism = 0;
	pci_maxdevice = 0;
}

/*--------------------------------------------------------------------
**
**      Build a pcitag from bus, device and function number
**
**--------------------------------------------------------------------
*/

static pcici_t
pcibus_tag (unsigned char bus, unsigned char device, unsigned char func)
{
	pcici_t tag;

	tag.cfg1 = 0;
	if (func   >=  8) return tag;

	switch (pci_mechanism) {

	case 1:
		if (device < 32) {
			tag.cfg1 = CONF1_ENABLE
				| (((u_long) bus   ) << 16ul)
				| (((u_long) device) << 11ul)
				| (((u_long) func  ) <<  8ul);
		}
		break;
	case 2:
		if (device < 16) {
			tag.cfg2.port    = 0xc000 | (device << 8ul);
			tag.cfg2.enable  = 0xf0 | (func << 1ul);
			tag.cfg2.forward = bus;
		}
		break;
	};
	return tag;
}

static pcici_t
pcibus_ftag (pcici_t tag, u_char func)
{
	switch (pci_mechanism) {

	case 1:
		tag.cfg1 &= ~0x700ul;
		tag.cfg1 |= (((u_long) func) << 8ul);
		break;
	case 2:
		tag.cfg2.enable  = 0xf0 | (func << 1ul);
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

static u_long
pcibus_read (pcici_t tag, u_long reg)
{
	u_long addr, data = 0;

	if (!tag.cfg1) return (0xfffffffful);

	switch (pci_mechanism) {

	case 1:
		addr = tag.cfg1 | (reg & 0xfc);
#ifdef PCI_DEBUG
		printf ("pci_conf_read(1): addr=%x ", addr);
#endif
		outl (CONF1_ADDR_PORT, addr);
		data = inl (CONF1_DATA_PORT);
		outl (CONF1_ADDR_PORT, 0   );
		break;

	case 2:
		addr = tag.cfg2.port | (reg & 0xfc);
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

static void
pcibus_write (pcici_t tag, u_long reg, u_long data)
{
	u_long addr;

	if (!tag.cfg1) return;

	switch (pci_mechanism) {

	case 1:
		addr = tag.cfg1 | (reg & 0xfc);
#ifdef PCI_DEBUG
		printf ("pci_conf_write(1): addr=%x data=%x\n",
			addr, data);
#endif
		outl (CONF1_ADDR_PORT, addr);
		outl (CONF1_DATA_PORT, data);
		outl (CONF1_ADDR_PORT,   0 );
		break;

	case 2:
		addr = tag.cfg2.port | (reg & 0xfc);
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

/*-----------------------------------------------------------------------
**
**	Register an interupt handler for a pci device.
**
**-----------------------------------------------------------------------
*/

static int
pcibus_ihandler_attach (int irq, void(*func)(), int arg, unsigned * maskptr)
{
	int result;
	result = register_intr(
		irq,		    /* isa irq	    */
		0,		    /* deviced??    */
		0,		    /* flags?	    */
		(inthand2_t*) func, /* handler	    */
		maskptr,	    /* mask pointer */
		arg);		    /* handler arg  */

	if (result) {
		printf ("@@@ pcibus_ihandler_attach: result=%d\n", result);
		return (result);
	};
	update_intr_masks();

	INTREN ((1ul<<irq));
	return (0);
}

static int
pcibus_ihandler_detach (int irq, void(*func)())
{
	int result;

	INTRDIS ((1ul<<irq));

	result = unregister_intr (irq, (inthand2_t*) func);

	if (result)
		printf ("@@@ pcibus_ihandler_detach: result=%d\n", result);

	update_intr_masks();

	return (result);
}

static int
pcibus_imask_include (int irq, unsigned* maskptr)
{
	unsigned mask;

	if (!maskptr) return (0);

	mask = 1ul << irq;

	if (*maskptr & mask)
		return (-1);

	INTRMASK (*maskptr, mask);
	update_intr_masks();

	return (0);
}

static int
pcibus_imask_exclude (int irq, unsigned* maskptr)
{
	unsigned mask;

	if (!maskptr) return (0);

	mask = 1ul << irq;

	if (! (*maskptr & mask))
		return (-1);

	*maskptr &= ~mask;
	update_intr_masks();

	return (0);
}
