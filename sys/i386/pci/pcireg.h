/**************************************************************************
**
**  $Id: pcireg.h,v 2.2 94/10/11 19:01:08 wolf Oct11 $
**
**  Declarations for pci bus drivers.
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
***************************************************************************
*/

#ifndef __PCI_REG_H__
#define __PCI_REG_H__

/*-----------------------------------------------------------------
**
**	main pci initialization function.
**	called at boot time from autoconf.c
**
**-----------------------------------------------------------------
*/

void pci_configure (void);

/*-----------------------------------------------------------------
**
**	The pci configuration id describes a pci device on the bus.
**	It is constructed from: bus, device & function numbers.
**
**-----------------------------------------------------------------
*/

typedef union {
	u_long	 cfg1;
        struct {
		 u_char   enable;
		 u_char   forward;
		 u_short  port;
	       } cfg2;
	} pcici_t;

/*-----------------------------------------------------------------
**
**	Each pci device has an unique device id.
**	It is used to find a matching driver.
**
**-----------------------------------------------------------------
*/

typedef u_long pcidi_t;

/*-----------------------------------------------------------------
**
**	The pci driver structure.
**
**	probe:	Checks if the driver can support a device
**		with this type. The tag may be used to get
**		more info with pci_read_conf(). See below.
**		It returns a string with the devices name,
**		or a NULL pointer, if the driver cannot
**		support this device.
**
**	attach:	Allocate a control structure and prepare
**		it. This function may use the pci mapping
**		functions. See below.
**		(configuration id) or type.
**
**	count:	A pointer to a unit counter.
**		It's used by the pci configurator to
**		allocate unit numbers.
**
**-----------------------------------------------------------------
*/

struct pci_driver {
    char*  (*probe ) (pcici_t tag, pcidi_t type);
    void   (*attach) (pcici_t tag, int     unit);
    u_long  *count;
};

/*-----------------------------------------------------------------
**
**	Per device structure.
**
**	An array of this structure should be created by the
**	config utility and live in "ioconf.c".
**
**	At the moment it's created by hand and lives in
**	pci_config.c
**
**	pd_driver:
**		a pointer to the driver structure.
**
**	pd_name:
**		the name of the devices which are supported
**		by this driver for kernel messages.
**
**	pd_flags:
**		for further study.
**
**-----------------------------------------------------------------
*/

struct pci_device {
	struct
	pci_driver*	pd_driver;
	const char *	pd_name;
	int		pd_flags;
};

/*-----------------------------------------------------------------
**
**	This table should be generated in file "ioconf.c"
**	by the config program.
**	It is used at boot time by the configuration function
**	pci_configure()
**
**-----------------------------------------------------------------
*/

extern struct pci_device pci_devtab[];

/*-----------------------------------------------------------------
**
**	Map a pci device to physical and virtual memory.
**
**	The va and pa addresses are "in/out" parameters.
**	If they are 0 on entry, the function assigns an address.
**
**	Entry selects the register in the pci configuration
**	space, which supplies the size of the region, and
**	receives the physical address.
**
**	If there is any error, a message is written, and
**	the function returns with zero.
**	Else it returns with a value different to zero.
**
**-----------------------------------------------------------------
*/

int pci_map_mem (pcici_t tag, u_long entry, u_long  * va, u_long * pa);

/*-----------------------------------------------------------------
**
**	Map a pci device to an io port area.
**
**	*pa is an "in/out" parameter.
**	If it's 0 on entry, the function assigns an port number..
**
**	Entry selects the register in the pci configuration
**	space, which supplies the size of the region, and
**	receives the port number.
**
**	If there is any error, a message is written, and
**	the function returns with zero.
**	Else it returns with a value different to zero.
**
**-----------------------------------------------------------------
*/

int pci_map_port(pcici_t tag, u_long entry, u_short * pa);

/*-----------------------------------------------------------------
**
**	Map a pci interrupt to an isa irq line,
**	and enable the interrupt.
**
**	func is the interrupt handler, arg is the argument
**	to this function.
**
**	The maskptr argument should be  &bio_imask,
**	&net_imask etc. or NULL.
**
**	If there is any error, a message is written, and
**	the function returns with zero.
**	Else it returns with a value different to zero.
**
**	A word of caution for FreeBSD 2.0:
**
**	We use the register_intr() function.
**
**	The interrupt line of the selected device is included
**	into the supplied mask: after the corresponding splXXX
**	this drivers interrupts are blocked.
**
**	But in the interrupt handlers startup code ONLY
**	the interrupt of the driver is blocked, and NOT
**	all interrupts of the spl group.
**
**	It may be required to additional block the group
**	interrupts by splXXX() inside the interrupt handler.
**
**	In pre 2.0 kernels we emulate the register_intr
**	function. The emulating function blocks all interrupts
**	of the group in the interrupt handler prefix code.
**
**-----------------------------------------------------------------
*/

int pci_map_int (pcici_t tag, int (*func)(), void* arg, unsigned * maskptr);

/*-----------------------------------------------------------------
**
**	The following functions are provided by the pci bios.
**	They are used only by the pci configuration.
**
**	pci_conf_mode():
**		Probes for a pci system.
**		Returns 1 or 2 for pci configuration mechanism.
**		Returns 0 if no pci system.
**
**	pcitag():
**		Gets a handle for accessing the pci configuration
**		space.
**		This handle is given to the mapping functions (see
**		above) or to the read/write functions.
**
**	pci_conf_read():
**		Read a long word from the pci configuration space.
**		Requires a tag (from pcitag) and the register
**		number (should be a long word alligned one).
**
**	pci_conf_write():
**		Writes a long word to the pci configuration space.
**		Requires a tag (from pcitag), the register number
**		(should be a long word alligned one), and a value.
**
**-----------------------------------------------------------------
*/

int pci_conf_mode (void);

pcici_t pcitag (unsigned char bus,
		unsigned char device,
                unsigned char func);

u_long pci_conf_read  (pcici_t tag, u_long reg		   );
void   pci_conf_write (pcici_t tag, u_long reg, u_long data);


/*------------------------------------------------------------------
**
**	Names for PCI configuration space registers.
**
**	Copyright (c) 1994 Charles Hannum.  All rights reserved.
**
**------------------------------------------------------------------
*/

/*
 * Device identification register; contains a vendor ID and a device ID.
 * We have little need to distinguish the two parts.
 */
#define	PCI_ID_REG			0x00

/*
 * Command and status register.
 */
#define	PCI_COMMAND_STATUS_REG		0x04

#define	PCI_COMMAND_IO_ENABLE		0x00000001
#define	PCI_COMMAND_MEM_ENABLE		0x00000002
#define	PCI_COMMAND_MASTER_ENABLE	0x00000004
#define	PCI_COMMAND_SPECIAL_ENABLE	0x00000008
#define	PCI_COMMAND_INVALIDATE_ENABLE	0x00000010
#define	PCI_COMMAND_PALETTE_ENABLE	0x00000020
#define	PCI_COMMAND_PARITY_ENABLE	0x00000040
#define	PCI_COMMAND_STEPPING_ENABLE	0x00000080
#define	PCI_COMMAND_SERR_ENABLE		0x00000100
#define	PCI_COMMAND_BACKTOBACK_ENABLE	0x00000200

#define	PCI_STATUS_BACKTOBACK_OKAY	0x00800000
#define	PCI_STATUS_PARITY_ERROR		0x01000000
#define	PCI_STATUS_DEVSEL_FAST		0x00000000
#define	PCI_STATUS_DEVSEL_MEDIUM	0x02000000
#define	PCI_STATUS_DEVSEL_SLOW		0x04000000
#define	PCI_STATUS_DEVSEL_MASK		0x06000000
#define	PCI_STATUS_TARGET_TARGET_ABORT	0x08000000
#define	PCI_STATUS_MASTER_TARGET_ABORT	0x10000000
#define	PCI_STATUS_MASTER_ABORT		0x20000000
#define	PCI_STATUS_SPECIAL_ERROR	0x40000000
#define	PCI_STATUS_PARITY_DETECT	0x80000000

/*
 * Class register; defines basic type of device.
 */
#define	PCI_CLASS_REG			0x08

#define	PCI_CLASS_MASK			0xff000000
#define	PCI_SUBCLASS_MASK		0x00ff0000

/* base classes */
#define	PCI_CLASS_PREHISTORIC		0x00000000
#define	PCI_CLASS_MASS_STORAGE		0x01000000
#define	PCI_CLASS_NETWORK		0x02000000
#define	PCI_CLASS_DISPLAY		0x03000000
#define	PCI_CLASS_MULTIMEDIA		0x04000000
#define	PCI_CLASS_MEMORY		0x05000000
#define	PCI_CLASS_BRIDGE		0x06000000
#define	PCI_CLASS_UNDEFINED		0xff000000

/* 0x00 prehistoric subclasses */
#define	PCI_SUBCLASS_PREHISTORIC_MISC	0x00000000
#define	PCI_SUBCLASS_PREHISTORIC_VGA	0x00010000

/* 0x01 mass storage subclasses */
#define	PCI_SUBCLASS_MASS_STORAGE_SCSI	0x00000000
#define	PCI_SUBCLASS_MASS_STORAGE_IDE	0x00010000
#define	PCI_SUBCLASS_MASS_STORAGE_FLOPPY	0x00020000
#define	PCI_SUBCLASS_MASS_STORAGE_IPI	0x00030000
#define	PCI_SUBCLASS_MASS_STORAGE_MISC	0x00800000

/* 0x02 network subclasses */
#define	PCI_SUBCLASS_NETWORK_ETHERNET	0x00000000
#define	PCI_SUBCLASS_NETWORK_TOKENRING	0x00010000
#define	PCI_SUBCLASS_NETWORK_FDDI	0x00020000
#define	PCI_SUBCLASS_NETWORK_MISC	0x00800000

/* 0x03 display subclasses */
#define	PCI_SUBCLASS_DISPLAY_VGA	0x00000000
#define	PCI_SUBCLASS_DISPLAY_XGA	0x00010000
#define	PCI_SUBCLASS_DISPLAY_MISC	0x00800000

/* 0x04 multimedia subclasses */
#define	PCI_SUBCLASS_MULTIMEDIA_VIDEO	0x00000000
#define	PCI_SUBCLASS_MULTIMEDIA_AUDIO	0x00010000
#define	PCI_SUBCLASS_MULTIMEDIA_MISC	0x00800000

/* 0x05 memory subclasses */
#define	PCI_SUBCLASS_MEMORY_RAM		0x00000000
#define	PCI_SUBCLASS_MEMORY_FLASH	0x00010000
#define	PCI_SUBCLASS_MEMORY_MISC	0x00800000

/* 0x06 bridge subclasses */
#define	PCI_SUBCLASS_BRIDGE_HOST	0x00000000
#define	PCI_SUBCLASS_BRIDGE_ISA		0x00010000
#define	PCI_SUBCLASS_BRIDGE_EISA	0x00020000
#define	PCI_SUBCLASS_BRIDGE_MC		0x00030000
#define	PCI_SUBCLASS_BRIDGE_PCI		0x00040000
#define	PCI_SUBCLASS_BRIDGE_PCMCIA	0x00050000
#define	PCI_SUBCLASS_BRIDGE_MISC	0x00800000

/*
 * Mapping registers
 */
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

/*
 * Interrupt configuration register
 */
#define	PCI_INTERRUPT_REG		0x3c

#define	PCI_INTERRUPT_PIN_MASK		0x0000ff00
#define	PCI_INTERRUPT_PIN_EXTRACT(x)	((((x) & PCI_INTERRUPT_PIN_MASK) >> 8) & 0xff)
#define	PCI_INTERRUPT_PIN_NONE		0x00
#define	PCI_INTERRUPT_PIN_A		0x01
#define	PCI_INTERRUPT_PIN_B		0x02
#define	PCI_INTERRUPT_PIN_C		0x03
#define	PCI_INTERRUPT_PIN_D		0x04

#define	PCI_INTERRUPT_LINE_MASK		0x000000ff
#define	PCI_INTERRUPT_LINE_EXTRACT(x)	((((x) & PCI_INTERRUPT_LINE_MASK) >> 0) & 0xff)
#define	PCI_INTERRUPT_LINE_INSERT(x,v)	(((x) & ~PCI_INTERRUPT_LINE_MASK) | ((v) << 0))

#endif /* __PCI_REG_H__ */
