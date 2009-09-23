/*-
 * Written by paradox <ddkprog@yahoo.com>
 * Public domain.
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
#include <dev/x86bios/x86bios.h>

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
x86biosCall(struct x86regs *regs, int intno)
{

	if (intno < 0 || intno > 255)
		return;

	mtx_lock_spin(&x86bios_lock);

	memcpy(&x86bios_emu.x86, regs, sizeof(*regs));
	x86emu_exec_intr(&x86bios_emu, intno);
	memcpy(regs, &x86bios_emu.x86, sizeof(*regs));

	mtx_unlock_spin(&x86bios_lock);
}

void *
x86biosOffs(uint32_t offs)
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
	x86bios_emu.mem_size = 1024 * 1024;

	memset(busySegMap, 0, sizeof(busySegMap));

	pbiosStack = x86biosAlloc(1, &offs);
}

static void
x86bios_uninit(void *arg __unused)
{

	x86biosFree(pbiosStack, 1);

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
