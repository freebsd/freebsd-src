/*-
 * Copyright (c) 2002 Jake Burkholder.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/reboot.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/tty.h>
#include <sys/ktr.h>

#ifdef __sparc64__

#include <ofw/openfirm.h>
#include <ofw/ofw_pci.h>

#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/isa/ofw_isa.h>
#include <sparc64/ebus/ebusvar.h>

#include <dev/se/sereg.h>

#define	SE_CNREAD_1(off) \
	bus_space_read_1(&se_cntag, se_cnhandle, se_cnchan + (off))
#define	SE_CNWRITE_1(off, val) \
	bus_space_write_1(&se_cntag, se_cnhandle, se_cnchan + (off), (val))

#define	SE_CONSOLE(flags)	((flags) & 0x10)
#define	SE_FORCECONSOLE(flags)	((flags) & 0x20)

#define	SE_CHANNELS	2

#define	KTR_SE	KTR_CT4

#define	CDEV_MAJOR	200

static	cn_probe_t	se_cnprobe;
static	cn_init_t	se_cninit;
static	cn_getc_t	se_cngetc;
static	cn_checkc_t	se_cncheckc;
static	cn_putc_t	se_cnputc;

static	void se_cnregdump(void);

static	u_char se_cnchan;
static	struct bus_space_tag se_cntag;
static	bus_space_handle_t se_cnhandle;

CONS_DRIVER(se, se_cnprobe, se_cninit, NULL, se_cngetc, se_cncheckc,
	    se_cnputc, NULL);

static int
OF_traverse(phandle_t root, phandle_t *node,
    int (*func)(phandle_t, phandle_t *))
{
	phandle_t child;

	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (func(child, node) == 0 ||
		    OF_traverse(child, node, func) == 0)
			return (0);
	}
	return (-1);
}

static int
se_cnfind(phandle_t child, phandle_t *node)
{
	char name[8];

	if (OF_getprop(child, "name", name, sizeof(name)) != -1 &&
	    strncmp(name, "se", sizeof(name)) == 0) {
		*node = child;
		return (0);
	}
	return (-1);
}

static int
se_cnmap(phandle_t node, phandle_t parent)
{
	struct isa_ranges ir[4];
	struct upa_ranges ur[4];
	struct isa_regs reg;
	vm_offset_t child;
	vm_offset_t dummy;
	vm_offset_t phys;
	phandle_t pbus;
	phandle_t bus;
	char name[32];
	int error;
	int type;
	int rsz;
	int bs;
	int cs;
	int i;

	if (OF_getprop(node, "reg", &reg, sizeof(reg)) == -1 ||
	    (rsz = OF_getprop(parent, "ranges", ir, sizeof(ir))) == -1) {
		return (ENXIO);
	}
	phys = ISA_REG_PHYS(&reg);
	dummy = phys + 8;
	type = ofw_isa_map_iorange(ir, rsz / sizeof(*ir), &phys, &dummy);
	if (type == SYS_RES_MEMORY) {
		cs = PCI_CS_MEM32;
		bs = PCI_MEMORY_BUS_SPACE;
	} else {
		cs = PCI_CS_IO;
		bs = PCI_IO_BUS_SPACE;
	}
	bus = OF_parent(parent);
	if (OF_getprop(bus, "name", name, sizeof(name)) == -1)
		return (ENXIO);
	name[sizeof(name) - 1] = '\0';
	if (strcmp(name, "pci") != 0)
		return (ENXIO);
	while ((pbus = OF_parent(bus)) != 0) {
		if (OF_getprop(pbus, "name", name, sizeof(name)) != -1) {
			name[sizeof(name) - 1] = '\0';
			if (strcmp(name, "pci") != 0)
				break;
		}
		bus = pbus;
	}
	if (pbus == 0)
		return (ENXIO);
	if ((rsz = OF_getprop(bus, "ranges", ur, sizeof(ur))) == -1)
		return (ENXIO);
	error = ENXIO;
	for (i = 0; i < (rsz / sizeof(ur[0])); i++) {
		child = UPA_RANGE_CHILD(&ur[i]);
		if (UPA_RANGE_CS(&ur[i]) == cs && phys >= child &&
		    phys - child < UPA_RANGE_SIZE(&ur[i])) {
			se_cnhandle = sparc64_fake_bustag(bs,
			    UPA_RANGE_PHYS(&ur[i]) + phys, &se_cntag);
			error = 0;
			break;
		}
	}
	return (error);
}

static void
se_cnprobe(struct consdev *cn)
{
	phandle_t parent;
	phandle_t node;
	phandle_t root;
	char name[8];
	int channel;
	int disabled;
	int flags;

	disabled = 0;
	cn->cn_pri = CN_DEAD;
	if ((root = OF_peer(0)) == -1 ||
	    OF_traverse(root, &node, se_cnfind) == -1)
		return;
	for (channel = 0; channel < SE_CHANNELS; channel++) {
		if (resource_int_value("se", channel, "disabled",
		    &disabled) != 0) {
			disabled = 0;
		}
		if (resource_int_value("se", channel, "flags", &flags) == 0) {
			if (!disabled && SE_CONSOLE(flags))
				goto map;
		}
	}
	return;

map:
	if ((parent = OF_parent(node)) <= 0 ||
	    OF_getprop(parent, "name", name, sizeof(name)) <= 0)
		return;
	if (strncmp(name, "ebus", sizeof(name)) != 0)
		return;
	if (se_cnmap(node, parent) != 0)
		return;
	se_cnchan = (channel == 0 ? SE_CHA : SE_CHB);

	cn->cn_dev = makedev(CDEV_MAJOR, channel);
	cn->cn_pri = SE_FORCECONSOLE(flags) || boothowto & RB_SERIAL ?
	    CN_REMOTE : CN_NORMAL;
}

static void
se_cninit(struct consdev *cn)
{
	u_char ccr0;

	/*
	 * Power down the chip for initialization.
	 */
	SE_CNWRITE_1(SE_CCR0, 0x0);

	/*
	 * Now program the chip for polled asynchronous serial io.
	 */
	SE_CNWRITE_1(SE_CCR0, CCR0_MCE | CCR0_SM_ASYNC);
	SE_CNWRITE_1(SE_CMDR, CMDR_RRES | CMDR_XRES);
	SE_CNWRITE_1(SE_CCR1, CCR1_ODS | CCR1_BCR | CCR1_CM_7);
	SE_CNWRITE_1(SE_BGR, SE_DIV_9600);
	SE_CNWRITE_1(SE_CCR2, CCR2_TOE | CCR2_SSEL | CCR2_BDF);
	SE_CNWRITE_1(SE_CCR3, 0x0);
	SE_CNWRITE_1(SE_CCR4, CCR4_EBRG | CCR4_MCK4);
	SE_CNWRITE_1(SE_MODE, MODE_FCTS | MODE_RAC | MODE_RTS);
	SE_CNWRITE_1(SE_DAFO, DAFO_CHL_8);
	SE_CNWRITE_1(SE_RFC, RFC_DPS | RFC_RFTH_32);
	SE_CNWRITE_1(SE_IPC, IPC_VIS);

	/*
	 * Now power up the chip again.
	 */
	ccr0 = SE_CNREAD_1(SE_CCR0);
	ccr0 |= CCR0_PU;
	SE_CNWRITE_1(SE_CCR0, ccr0);

	SE_CNWRITE_1(SE_CMDR, CMDR_RRES | CMDR_XRES);
}

static int
se_cngetc(dev_t dev)
{
	u_char c;

	while ((SE_CNREAD_1(SE_STAR) & (STAR_CEC | STAR_RFNE)) != STAR_RFNE)
		;
	SE_CNWRITE_1(SE_CMDR, CMDR_RFRD);
	while ((SE_CNREAD_1(SE_ISR0) & ISR0_TCD) == 0)
		;
	c = SE_CNREAD_1(SE_RFIFO);
	SE_CNWRITE_1(SE_CMDR, CMDR_RMC);
	return (c);
}

static int
se_cncheckc(dev_t dev)
{
	u_char c;

	if ((SE_CNREAD_1(SE_STAR) & STAR_RFNE) != 0) {
		while ((SE_CNREAD_1(SE_STAR) & STAR_CEC) != 0)
			;
		SE_CNWRITE_1(SE_CMDR, CMDR_RFRD);
		while ((SE_CNREAD_1(SE_ISR0) & ISR0_TCD) == 0)
			;
		c = SE_CNREAD_1(SE_RFIFO);
		SE_CNWRITE_1(SE_CMDR, CMDR_RMC);
		return (c);
	}
	return (-1);
}

static void
se_cnputc(dev_t dev, int c)
{

	while ((SE_CNREAD_1(SE_STAR) & (STAR_CTS | STAR_CEC | STAR_XFW)) !=
	    (STAR_CTS | STAR_XFW))
		;
	SE_CNWRITE_1(SE_XFIFO, c);
	SE_CNWRITE_1(SE_CMDR, CMDR_XF);
}

static void
se_cnregdump(void)
{

	CTR1(KTR_SE, "se_cnprobe: mode=%#x", SE_CNREAD_1(SE_MODE));
	CTR1(KTR_SE, "se_cnprobe: timr=%#x", SE_CNREAD_1(SE_TIMR));
	CTR1(KTR_SE, "se_cnprobe: xon=%#x", SE_CNREAD_1(SE_XON));
	CTR1(KTR_SE, "se_cnprobe: xoff=%#x", SE_CNREAD_1(SE_XOFF));
	CTR1(KTR_SE, "se_cnprobe: tcr=%#x", SE_CNREAD_1(SE_TCR));
	CTR1(KTR_SE, "se_cnprobe: dafo=%#x", SE_CNREAD_1(SE_DAFO));
	CTR1(KTR_SE, "se_cnprobe: rfc=%#x", SE_CNREAD_1(SE_RFC));
	CTR1(KTR_SE, "se_cnprobe: ccr0=%#x", SE_CNREAD_1(SE_CCR0));
	CTR1(KTR_SE, "se_cnprobe: ccr1=%#x", SE_CNREAD_1(SE_CCR1));
	CTR1(KTR_SE, "se_cnprobe: ccr2=%#x", SE_CNREAD_1(SE_CCR2));
	CTR1(KTR_SE, "se_cnprobe: ccr3=%#x", SE_CNREAD_1(SE_CCR3));
	CTR1(KTR_SE, "se_cnprobe: vstr=%#x", SE_CNREAD_1(SE_VSTR));
	CTR1(KTR_SE, "se_cnprobe: ipc=%#x", SE_CNREAD_1(SE_IPC));
	CTR1(KTR_SE, "se_cnprobe: ccr4=%#x", SE_CNREAD_1(SE_CCR4));
}

#endif
