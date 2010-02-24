/*-
 * Copyright (c) 2009 Alex Keda <admin@lissyara.su>
 * Copyright (c) 2009 Jung-uk Kim <jkim@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_x86bios.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <contrib/x86emu/x86emu.h>
#include <contrib/x86emu/x86emu_regs.h>
#include <compat/x86bios/x86bios.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/cpufunc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#define	X86BIOS_PAGE_SIZE	0x00001000	/* 4K */

#define	X86BIOS_IVT_SIZE	0x00000500	/* 1K + 256 (BDA) */
#define	X86BIOS_SEG_SIZE	0x00010000	/* 64K */
#define	X86BIOS_MEM_SIZE	0x00100000	/* 1M */

#define	X86BIOS_IVT_BASE	0x00000000
#define	X86BIOS_RAM_BASE	0x00001000
#define	X86BIOS_ROM_BASE	0x000a0000	/* XXX EBDA? */

#define	X86BIOS_ROM_SIZE	(X86BIOS_MEM_SIZE - X86BIOS_ROM_BASE)

#define	X86BIOS_PAGES		(X86BIOS_MEM_SIZE / X86BIOS_PAGE_SIZE)

#define	X86BIOS_R_DS		_pad1
#define	X86BIOS_R_SS		_pad2

static struct x86emu x86bios_emu;

static struct mtx x86bios_lock;

static void *x86bios_ivt;
static void *x86bios_rom;
static void *x86bios_seg;

static vm_offset_t *x86bios_map;

static vm_paddr_t x86bios_seg_phys;

SYSCTL_NODE(_debug, OID_AUTO, x86bios, CTLFLAG_RD, NULL, "x86bios debugging");
static int x86bios_trace_call;
TUNABLE_INT("debug.x86bios.call", &x86bios_trace_call);
SYSCTL_INT(_debug_x86bios, OID_AUTO, call, CTLFLAG_RW, &x86bios_trace_call, 0,
    "Trace far function calls");
static int x86bios_trace_int;
TUNABLE_INT("debug.x86bios.int", &x86bios_trace_int);
SYSCTL_INT(_debug_x86bios, OID_AUTO, int, CTLFLAG_RW, &x86bios_trace_int, 0,
    "Trace software interrupt handlers");

static void *
x86bios_get_pages(uint32_t offset, size_t size)
{
	int i;

	if (offset + size > X86BIOS_MEM_SIZE)
		return (NULL);

	i = offset / X86BIOS_PAGE_SIZE;
	if (x86bios_map[i] != 0)
		return ((void *)(x86bios_map[i] + offset -
		    i * X86BIOS_PAGE_SIZE));

	return (NULL);
}

static void
x86bios_set_pages(vm_offset_t va, vm_paddr_t pa, size_t size)
{
	int i, j;

	for (i = pa / X86BIOS_PAGE_SIZE, j = 0;
	    j < howmany(size, X86BIOS_PAGE_SIZE); i++, j++)
		x86bios_map[i] = va + j * X86BIOS_PAGE_SIZE;
}

static uint8_t
x86bios_emu_rdb(struct x86emu *emu, uint32_t addr)
{
	uint8_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	return (*va);
}

static uint16_t
x86bios_emu_rdw(struct x86emu *emu, uint32_t addr)
{
	uint16_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	return (le16toh(*va));
}

static uint32_t
x86bios_emu_rdl(struct x86emu *emu, uint32_t addr)
{
	uint32_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	return (le32toh(*va));
}

static void
x86bios_emu_wrb(struct x86emu *emu, uint32_t addr, uint8_t val)
{
	uint8_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	*va = val;
}

static void
x86bios_emu_wrw(struct x86emu *emu, uint32_t addr, uint16_t val)
{
	uint16_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	*va = htole16(val);
}

static void
x86bios_emu_wrl(struct x86emu *emu, uint32_t addr, uint32_t val)
{
	uint32_t *va;

	va = x86bios_get_pages(addr, sizeof(*va));
	if (va == NULL)
		x86emu_halt_sys(emu);

	*va = htole32(val);
}

static uint8_t
x86bios_emu_inb(struct x86emu *emu, uint16_t port)
{

	if (port == 0xb2) /* APM scratch register */
		return (0);
	if (port >= 0x80 && port < 0x88) /* POST status register */
		return (0);

	return (inb(port));
}

static uint16_t
x86bios_emu_inw(struct x86emu *emu, uint16_t port)
{

	if (port >= 0x80 && port < 0x88) /* POST status register */
		return (0);

	return (inw(port));
}

static uint32_t
x86bios_emu_inl(struct x86emu *emu, uint16_t port)
{

	if (port >= 0x80 && port < 0x88) /* POST status register */
		return (0);

	return (inl(port));
}

static void
x86bios_emu_outb(struct x86emu *emu, uint16_t port, uint8_t val)
{

	if (port == 0xb2) /* APM scratch register */
		return;
	if (port >= 0x80 && port < 0x88) /* POST status register */
		return;

	outb(port, val);
}

static void
x86bios_emu_outw(struct x86emu *emu, uint16_t port, uint16_t val)
{

	if (port >= 0x80 && port < 0x88) /* POST status register */
		return;

	outw(port, val);
}

static void
x86bios_emu_outl(struct x86emu *emu, uint16_t port, uint32_t val)
{

	if (port >= 0x80 && port < 0x88) /* POST status register */
		return;

	outl(port, val);
}

static void
x86bios_emu_get_intr(struct x86emu *emu, int intno)
{
	uint16_t *sp;
	uint32_t iv;

	emu->x86.R_SP -= 6;

	sp = (uint16_t *)((vm_offset_t)x86bios_seg + emu->x86.R_SP);
	sp[0] = htole16(emu->x86.R_IP);
	sp[1] = htole16(emu->x86.R_CS);
	sp[2] = htole16(emu->x86.R_FLG);

	iv = x86bios_get_intr(intno);
	emu->x86.R_IP = iv & 0x000f;
	emu->x86.R_CS = (iv >> 12) & 0xffff;
	emu->x86.R_FLG &= ~(F_IF | F_TF);
}

void *
x86bios_alloc(uint32_t *offset, size_t size)
{
	void *vaddr;

	if (offset == NULL || size == 0)
		return (NULL);

	vaddr = contigmalloc(size, M_DEVBUF, M_NOWAIT, X86BIOS_RAM_BASE,
	    X86BIOS_ROM_BASE, X86BIOS_PAGE_SIZE, 0);
	if (vaddr != NULL) {
		*offset = vtophys(vaddr);
		x86bios_set_pages((vm_offset_t)vaddr, *offset, size);
	}

	return (vaddr);
}

void
x86bios_free(void *addr, size_t size)
{
	vm_paddr_t paddr;

	if (addr == NULL || size == 0)
		return;

	paddr = vtophys(addr);
	if (paddr < X86BIOS_RAM_BASE || paddr >= X86BIOS_ROM_BASE ||
	    paddr % X86BIOS_PAGE_SIZE != 0)
		return;

	bzero(x86bios_map + paddr / X86BIOS_PAGE_SIZE,
	    sizeof(*x86bios_map) * howmany(size, X86BIOS_PAGE_SIZE));
	contigfree(addr, size, M_DEVBUF);
}

void
x86bios_init_regs(struct x86regs *regs)
{

	bzero(regs, sizeof(*regs));
	regs->X86BIOS_R_DS = regs->X86BIOS_R_SS = x86bios_seg_phys >> 4;
}

void
x86bios_call(struct x86regs *regs, uint16_t seg, uint16_t off)
{

	if (x86bios_map == NULL)
		return;

	if (x86bios_trace_call)
		printf("Calling 0x%05x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    (seg << 4) + off, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);

	mtx_lock_spin(&x86bios_lock);
	memcpy(&x86bios_emu.x86, regs, sizeof(*regs));
	x86emu_exec_call(&x86bios_emu, seg, off);
	memcpy(regs, &x86bios_emu.x86, sizeof(*regs));
	mtx_unlock_spin(&x86bios_lock);

	if (x86bios_trace_call)
		printf("Exiting 0x%05x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    (seg << 4) + off, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);
}

uint32_t
x86bios_get_intr(int intno)
{
	uint32_t *iv;

	iv = (uint32_t *)((vm_offset_t)x86bios_ivt + intno * 4);

	return (le32toh(*iv));
}

void
x86bios_intr(struct x86regs *regs, int intno)
{

	if (intno < 0 || intno > 255)
		return;

	if (x86bios_map == NULL)
		return;

	if (x86bios_trace_int)
		printf("Calling int 0x%x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    intno, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);

	mtx_lock_spin(&x86bios_lock);
	memcpy(&x86bios_emu.x86, regs, sizeof(*regs));
	x86emu_exec_intr(&x86bios_emu, intno);
	memcpy(regs, &x86bios_emu.x86, sizeof(*regs));
	mtx_unlock_spin(&x86bios_lock);

	if (x86bios_trace_int)
		printf("Exiting int 0x%x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    intno, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);
}

void *
x86bios_offset(uint32_t offset)
{

	return (x86bios_get_pages(offset, 1));
}

void *
x86bios_get_orm(uint32_t offset)
{
	uint8_t *p;

	/* Does the shadow ROM contain BIOS POST code for x86? */
	p = x86bios_offset(offset);
	if (p == NULL || p[0] != 0x55 || p[1] != 0xaa || p[3] != 0xe9)
		return (NULL);

	return (p);
}

int
x86bios_match_device(uint32_t offset, device_t dev)
{
	uint8_t *p;
	uint16_t device, vendor;
	uint8_t class, progif, subclass;

	/* Does the shadow ROM contain BIOS POST code for x86? */
	p = x86bios_get_orm(offset);
	if (p == NULL)
		return (0);

	/* Does it contain PCI data structure? */
	p += le16toh(*(uint16_t *)(p + 0x18));
	if (bcmp(p, "PCIR", 4) != 0 ||
	    le16toh(*(uint16_t *)(p + 0x0a)) < 0x18 || *(p + 0x14) != 0)
		return (0);

	/* Does it match the vendor, device, and classcode? */
	vendor = le16toh(*(uint16_t *)(p + 0x04));
	device = le16toh(*(uint16_t *)(p + 0x06));
	progif = *(p + 0x0d);
	subclass = *(p + 0x0e);
	class = *(p + 0x0f);
	if (vendor != pci_get_vendor(dev) || device != pci_get_device(dev) ||
	    class != pci_get_class(dev) || subclass != pci_get_subclass(dev) ||
	    progif != pci_get_progif(dev))
		return (0);

	return (1);
}

static __inline int
x86bios_map_mem(void)
{

	x86bios_ivt = pmap_mapbios(X86BIOS_IVT_BASE, X86BIOS_IVT_SIZE);
	if (x86bios_ivt == NULL)
		return (1);
	x86bios_rom = pmap_mapdev(X86BIOS_ROM_BASE, X86BIOS_ROM_SIZE);
	if (x86bios_rom == NULL) {
		pmap_unmapdev((vm_offset_t)x86bios_ivt, X86BIOS_IVT_SIZE);
		return (1);
	}
	x86bios_seg = contigmalloc(X86BIOS_SEG_SIZE, M_DEVBUF, M_WAITOK,
	    X86BIOS_RAM_BASE, X86BIOS_ROM_BASE, X86BIOS_PAGE_SIZE, 0);
	x86bios_seg_phys = vtophys(x86bios_seg);

	return (0);
}

static __inline void
x86bios_unmap_mem(void)
{

	pmap_unmapdev((vm_offset_t)x86bios_ivt, X86BIOS_IVT_SIZE);
	pmap_unmapdev((vm_offset_t)x86bios_rom, X86BIOS_ROM_SIZE);
	contigfree(x86bios_seg, X86BIOS_SEG_SIZE, M_DEVBUF);
}

static void
x86bios_init(void *arg __unused)
{
	int i;

	mtx_init(&x86bios_lock, "x86bios lock", NULL, MTX_SPIN);

	if (x86bios_map_mem() != 0)
		return;

	x86bios_map = malloc(sizeof(*x86bios_map) * X86BIOS_PAGES, M_DEVBUF,
	    M_WAITOK | M_ZERO);
	x86bios_set_pages((vm_offset_t)x86bios_ivt, X86BIOS_IVT_BASE,
	    X86BIOS_IVT_SIZE);
	x86bios_set_pages((vm_offset_t)x86bios_rom, X86BIOS_ROM_BASE,
	    X86BIOS_ROM_SIZE);
	x86bios_set_pages((vm_offset_t)x86bios_seg, x86bios_seg_phys,
	    X86BIOS_SEG_SIZE);

	bzero(&x86bios_emu, sizeof(x86bios_emu));

	x86bios_emu.emu_rdb = x86bios_emu_rdb;
	x86bios_emu.emu_rdw = x86bios_emu_rdw;
	x86bios_emu.emu_rdl = x86bios_emu_rdl;
	x86bios_emu.emu_wrb = x86bios_emu_wrb;
	x86bios_emu.emu_wrw = x86bios_emu_wrw;
	x86bios_emu.emu_wrl = x86bios_emu_wrl;

	x86bios_emu.emu_inb = x86bios_emu_inb;
	x86bios_emu.emu_inw = x86bios_emu_inw;
	x86bios_emu.emu_inl = x86bios_emu_inl;
	x86bios_emu.emu_outb = x86bios_emu_outb;
	x86bios_emu.emu_outw = x86bios_emu_outw;
	x86bios_emu.emu_outl = x86bios_emu_outl;

	for (i = 0; i < 256; i++)
		x86bios_emu._x86emu_intrTab[i] = x86bios_emu_get_intr;
}

static void
x86bios_uninit(void *arg __unused)
{
	vm_offset_t *map = x86bios_map;

	mtx_lock_spin(&x86bios_lock);
	if (x86bios_map != NULL) {
		free(x86bios_map, M_DEVBUF);
		x86bios_map = NULL;
	}
	mtx_unlock_spin(&x86bios_lock);

	if (map != NULL)
		x86bios_unmap_mem();

	mtx_destroy(&x86bios_lock);
}

static int
x86bios_modevent(module_t mod __unused, int type, void *data __unused)
{

	switch (type) {
	case MOD_LOAD:
		x86bios_init(NULL);
		break;
	case MOD_UNLOAD:
		x86bios_uninit(NULL);
		break;
	default:
		return (ENOTSUP);
	}

	return (0);
}

static moduledata_t x86bios_mod = {
	"x86bios",
	x86bios_modevent,
	NULL,
};

DECLARE_MODULE(x86bios, x86bios_mod, SI_SUB_CPU, SI_ORDER_ANY);
MODULE_VERSION(x86bios, 1);
