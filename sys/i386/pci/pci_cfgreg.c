/**************************************************************************
**
**  $Id: pcibus.c,v 1.2 1995/02/09 20:16:19 se Exp $
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

#ifndef __FreeBSD2__
#if __FreeBSD__ >= 2
#define __FreeBSD2__
#endif
#endif

#ifdef __FreeBSD2__
#define HAS_CPUFUNC_H
#endif

#include <types.h>
#include <param.h>
#include <kernel.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#ifdef HAS_CPUFUNC_H
#include <i386/include/cpufunc.h>
#endif

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/pcibus.h>

extern int printf();

static char pci_mode;

/*-----------------------------------------------------------------
**
**	The following functions are provided by the pci bios.
**	They are used only by the pci configuration.
**
**	pcibus_mode():
**		Probes for a pci system.
**		Returns 1 or 2 for pci configuration mechanism.
**		Returns 0 if no pci system.
**
**	pcibus_tag():
**		Gets a handle for accessing the pci configuration
**		space.
**		This handle is given to the mapping functions (see
**		above) or to the read/write functions.
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
pcibus_mode (void);

static pcici_t
pcibus_tag (u_char bus, u_char device, u_char func);

static u_long
pcibus_read (pcici_t tag, u_long reg);

static void
pcibus_write (pcici_t tag, u_long reg, u_long data);

static int
pcibus_regint (pcici_t tag, int(*func)(), void* arg, unsigned* maskptr);

struct pcibus i386pci = {
	"pci",
	pcibus_mode,
	pcibus_tag,
	pcibus_read,
	pcibus_write,
	pcibus_regint,
};

/*
**	Announce structure to generic driver
*/

DATA_SET (pcibus_set, i386pci);

/*--------------------------------------------------------------------
**
**      Port access
**
**--------------------------------------------------------------------
*/

#ifndef HAS_CPUFUNC_H

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

#endif /* HAS_CPUFUNC_H */

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


static int
pcibus_mode (void)
{
#ifdef PCI_CONF_MODE
	pci_mode = PCI_CONF_MODE;
	return (PCI_CONF_MODE)
#else /* PCI_CONF_MODE */
	u_long result, oldval;

	/*---------------------------------------
	**      Configuration mode 2 ?
	**---------------------------------------
	*/

	outb (CONF2_ENABLE_PORT,  0);
	outb (CONF2_FORWARD_PORT, 0);
	if (!inb (CONF2_ENABLE_PORT) && !inb (CONF2_FORWARD_PORT)) {
		pci_mode = 2;
		return (2);
	};

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
	**      No PCI bus available.
	**---------------------------------------
	*/
	return (0);
#endif /* PCI_CONF_MODE */
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


static u_long
pcibus_read (pcici_t tag, u_long reg)
{
	u_long addr, data = 0;

	if (!tag.cfg1) return (0xfffffffful);

	switch (pci_mode) {

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

	switch (pci_mode) {

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

#ifndef __FreeBSD2__
/*
 * Type of the first (asm) part of an interrupt handler.
 */
typedef void inthand_t __P((u_int cs, u_int ef, u_int esp, u_int ss));

/*
 * Usual type of the second (C) part of an interrupt handler.  Some bogus
 * ones need the arg to be the interrupt frame (and not a copy of it, which
 * is all that is possible in C).
 */
typedef void inthand2_t __P((int unit));

/*
**	XXX	@FreeBSD2@
**
**	Unfortunately, the mptr argument is _no_ pointer in 2.0 FreeBSD.
**	We would prefer a pointer because it enables us to install
**	new interrupt handlers at any time.
**	(This is just going to be changed ... <se> :)
**	In 2.0 FreeBSD later installed interrupt handlers may change
**	the xyz_imask, but this would not be recognized by handlers
**	which are installed before.
*/

static int
register_intr __P((int intr, int device_id, unsigned int flags,
		       inthand2_t *handler, unsigned int * mptr, int unit));
extern unsigned intr_mask[ICU_LEN];

#endif /* !__FreeBSD2__ */
static	unsigned int	pci_int_mask [16];

int pcibus_regint (pcici_t tag, int(*func)(), void* arg, unsigned* maskptr)
{
	int irq;
	unsigned mask, oldmask;

	irq = PCI_INTERRUPT_LINE_EXTRACT(
		pci_conf_read (tag, PCI_INTERRUPT_REG));

	mask = 1ul << irq;

	if (!maskptr)
		maskptr = &pci_int_mask[irq];
	oldmask = *maskptr;

	INTRMASK (*maskptr, mask);

	register_intr(
		irq,		/* isa irq	*/
		0,		/* deviced??	*/
		0,		/* flags?	*/
		(inthand2_t*) func, /* handler	*/
		maskptr,	/* mask pointer	*/
		(int) arg);	/* handler arg	*/

#ifdef __FreeBSD2__
	/*
	**	XXX See comment at beginning of file.
	**
	**	Have to update all the interrupt masks ... Grrrrr!!!
	*/
	{
		unsigned * mp = &intr_mask[0];
		/*
		**	update the isa interrupt masks.
		*/
		for (mp=&intr_mask[0]; mp<&intr_mask[ICU_LEN]; mp++)
			if ((~*mp & oldmask)==0)
				*mp |= mask;
		/*
		**	update the pci interrupt masks.
		*/
		for (mp=&pci_int_mask[0]; mp<&pci_int_mask[16]; mp++)
			if ((~*mp & oldmask)==0)
				*mp |= mask;
	};
#endif

	INTREN (mask);

	return (1);
}
