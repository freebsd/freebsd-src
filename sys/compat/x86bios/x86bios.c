/*-
 * Copyright (c) 2009 Alex Keda <admin@lissyara.su>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_x86bios.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>

#include <contrib/x86emu/x86emu.h>
#include <contrib/x86emu/x86emu_regs.h>
#include <compat/x86bios/x86bios.h>

u_char *pbiosMem = NULL;
static u_char *pbiosStack = NULL;

int busySegMap[5];

static struct x86emu x86bios_emu;

static struct mtx x86bios_lock;

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

void
x86bios_intr(struct x86regs *regs, int intno)
{

	if (intno < 0 || intno > 255)
		return;

	if (bootverbose)
		printf("Calling int 0x%x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    intno, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);

	mtx_lock_spin(&x86bios_lock);

	memcpy(&x86bios_emu.x86, regs, sizeof(*regs));
	x86emu_exec_intr(&x86bios_emu, intno);
	memcpy(regs, &x86bios_emu.x86, sizeof(*regs));

	mtx_unlock_spin(&x86bios_lock);

	if (bootverbose)
		printf("Exiting int 0x%x (ax=0x%04x bx=0x%04x "
		    "cx=0x%04x dx=0x%04x es=0x%04x di=0x%04x)\n",
		    intno, regs->R_AX, regs->R_BX, regs->R_CX,
		    regs->R_DX, regs->R_ES, regs->R_DI);
}

void *
x86bios_offset(uint32_t offs)
{

	return (pbiosMem + offs);
}

static void
x86bios_init(void *arg __unused)
{
	int offs;

	mtx_init(&x86bios_lock, "x86bios lock", NULL, MTX_SPIN);

	/* Can pbiosMem be NULL here? */
	pbiosMem = pmap_mapbios(0x0, MAPPED_MEMORY_SIZE);

	memset(&x86bios_emu, 0, sizeof(x86bios_emu));
	x86emu_init_default(&x86bios_emu);

	x86bios_emu.emu_inb = x86bios_emu_inb;
	x86bios_emu.emu_inw = x86bios_emu_inw;
	x86bios_emu.emu_inl = x86bios_emu_inl;
	x86bios_emu.emu_outb = x86bios_emu_outb;
	x86bios_emu.emu_outw = x86bios_emu_outw;
	x86bios_emu.emu_outl = x86bios_emu_outl;

	x86bios_emu.mem_base = (char *)pbiosMem;
	x86bios_emu.mem_size = MAPPED_MEMORY_SIZE;

	memset(busySegMap, 0, sizeof(busySegMap));

	pbiosStack = x86bios_alloc(1, &offs);
}

static void
x86bios_uninit(void *arg __unused)
{

	x86bios_free(pbiosStack, 1);

	if (pbiosMem)
		pmap_unmapdev((vm_offset_t)pbiosMem,
		    MAPPED_MEMORY_SIZE);

	mtx_destroy(&x86bios_lock);
}

static int
x86bios_modevent(module_t mod __unused, int type, void *data __unused)
{
	int err = 0;

	switch (type) {
	case MOD_LOAD:
		x86bios_init(NULL);
		break;
	case MOD_UNLOAD:
		x86bios_uninit(NULL);
		break;
	default:
		err = ENOTSUP;
		break;
	}

	return (err);
}

static moduledata_t x86bios_mod = {
	"x86bios",
	x86bios_modevent,
	NULL,
};

DECLARE_MODULE(x86bios, x86bios_mod, SI_SUB_CPU, SI_ORDER_ANY);
MODULE_VERSION(x86bios, 1);
