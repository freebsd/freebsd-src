/*
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file. 
 *
 * $FreeBSD$
 */
/*
 * FreeBSD/sparc64 kernel loader - machine dependent part
 *
 *  - implements copyin and readin functions that map kernel
 *    pages on demand.  The machine independent code does not
 *    know the size of the kernel early enough to pre-enter
 *    TTEs and install just one 4MB mapping seemed to limiting
 *    to me.
 */
#include <stand.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/linker.h>

#include <machine/asi.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <machine/tte.h>

#include "bootstrap.h"
#include "libofw.h"
#include "dev_net.h"

enum {
	HEAPVA		= 0x800000,
	HEAPSZ		= 0x1000000,
	LOADSZ		= 0x1000000	/* for kernel and modules */
};

struct memory_slice {
	vm_offset_t pstart;
	vm_offset_t size;
};

typedef void kernel_entry_t(vm_offset_t mdp, u_long o1, u_long o2, u_long o3,
			    void *openfirmware);

extern void itlb_enter(int, vm_offset_t, vm_offset_t, unsigned long);
extern void dtlb_enter(int, vm_offset_t, vm_offset_t, unsigned long);
extern vm_offset_t itlb_va_to_pa(vm_offset_t);
extern vm_offset_t dtlb_va_to_pa(vm_offset_t);
extern vm_offset_t md_load(char *, vm_offset_t *);
static int elf_exec(struct preloaded_file *);
static int sparc64_autoload(void);
static int mmu_mapin(vm_offset_t, vm_size_t);

char __progname[] = "FreeBSD/sparc64 loader";

vm_offset_t kernelpa;	/* Begin of kernel and mod memory. */
vm_offset_t curkpg;	/* (PA) used for on-demand map-in. */
vm_offset_t curkva = 0;
vm_offset_t heapva;
int tlbslot = 63;	/* Insert first entry at this TLB slot. XXX */
phandle_t pmemh;	/* OFW memory handle */

struct memory_slice memslices[18];
struct ofw_devdesc bootdev;

/*
 * Machine dependent structures that the machine independent
 * loader part uses.
 */
struct devsw *devsw[] = {
#ifdef LOADER_DISK_SUPPORT
	&ofwdisk,
#endif
#ifdef LOADER_NET_SUPPORT
	&netdev,
#endif
	0
};
struct arch_switch archsw;

struct file_format sparc64_elf = {
	elf_loadfile,
	elf_exec
};
struct file_format *file_formats[] = {
	&sparc64_elf,
	0
};
struct fs_ops *file_system[] = {
#ifdef LOAD_DISK_SUPPORT
	&ufs_fsops,
#endif
#ifdef LOADER_NET_SUPPORT
	&nfs_fsops,
#endif
	0
};
struct netif_driver *netif_drivers[] = {
#ifdef LOADER_NET_SUPPORT
	&ofwnet,
#endif
	0
};

extern struct console ofwconsole;
struct console *consoles[] = {
	&ofwconsole,
	0
};

/*
 * archsw functions
 */
static int
sparc64_autoload(void)
{
	printf("nothing to autoload yet.\n");
	return 0;
}

static ssize_t
sparc64_readin(const int fd, vm_offset_t va, const size_t len)
{
	mmu_mapin(va, len);
	return read(fd, (void *)va, len);
}

static ssize_t
sparc64_copyin(const void *src, vm_offset_t dest, size_t len)
{
	mmu_mapin(dest, len);
	memcpy((void *)dest, src, len);
	return len;
}

/*
 * other MD functions
 */
static int
elf_exec(struct preloaded_file *fp)
{
	struct file_metadata *fmp;
	vm_offset_t entry;
	vm_offset_t mdp;
	Elf_Ehdr *Ehdr;
	int error;

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == 0) {
		return EFTYPE;
	}
	Ehdr = (Elf_Ehdr *)&fmp->md_data;
	entry = Ehdr->e_entry;

	if ((error = md_load(fp->f_args, &mdp)) != 0)
		return error;

	printf("jumping to kernel entry at 0x%lx.\n", entry);
#if 0
	pmap_print_tlb('i');
	pmap_print_tlb('d');
#endif
	((kernel_entry_t *)entry)(mdp, 0, 0, 0, openfirmware);

	panic("exec returned");
}

static int
mmu_mapin(vm_offset_t va, vm_size_t len)
{

	if (va + len > curkva)
		curkva = va + len;

	len += va & PAGE_MASK_4M;
	va &= ~PAGE_MASK_4M;
	while (len) {
		if (dtlb_va_to_pa(va) == (vm_offset_t)-1 ||
		    itlb_va_to_pa(va) == (vm_offset_t)-1) {
			dtlb_enter(tlbslot, curkpg, va,
			    TD_V | TD_4M | TD_L | TD_CP | TD_CV | TD_P | TD_W);
			itlb_enter(tlbslot, curkpg, va,
			    TD_V | TD_4M | TD_L | TD_CP | TD_CV | TD_P | TD_W);
			tlbslot--;
			curkpg += PAGE_SIZE_4M;
		}
		len -= len > PAGE_SIZE_4M ? PAGE_SIZE_4M : len;
		va += PAGE_SIZE_4M;
	}
	return 0;
}

static vm_offset_t
init_heap(void)
{
	if ((pmemh = OF_finddevice("/memory")) == (phandle_t)-1)
		OF_exit();
	if (OF_getprop(pmemh, "available", memslices, sizeof(memslices)) <= 0)
		OF_exit();

	/* Reserve 16 MB continuous for kernel and modules. */
	kernelpa = (vm_offset_t)OF_alloc_phys(LOADSZ, 0x400000);
	curkpg = kernelpa;
	/* There is no need for continuous physical heap memory. */
	heapva = (vm_offset_t)OF_claim((void *)HEAPVA, HEAPSZ, 32);
	return heapva;
}

int
main(int (*openfirm)(void *))
{
	char bootpath[64];
	struct devsw **dp;
	phandle_t chosenh;

	/*
	 * Tell the OpenFirmware functions where they find the ofw gate.
	 */
	OF_init(openfirm);

	archsw.arch_getdev = ofw_getdev;
	archsw.arch_copyin = sparc64_copyin;
	archsw.arch_copyout = ofw_copyout;
	archsw.arch_readin = sparc64_readin;
	archsw.arch_autoload = sparc64_autoload;

	init_heap();
	setheap((void *)heapva, (void *)(heapva + HEAPSZ));

	/*
	 * Probe for a console.
	 */
	cons_probe();

	bcache_init(32, 512);

	/*
	 * Initialize devices.
	 */
	for (dp = devsw; *dp != 0; dp++) {
		if ((*dp)->dv_init != 0)
			(*dp)->dv_init();
	}

	/*
	 * Set up the current device.
	 */
	chosenh = OF_finddevice("/chosen");
	OF_getprop(chosenh, "bootpath", bootpath, sizeof(bootpath));

	bootdev.d_type = ofw_devicetype(bootpath);
	switch (bootdev.d_type) {
	case DEVT_DISK:
		bootdev.d_dev = &ofwdisk;
		strncpy(bootdev.d_kind.ofwdisk.path, bootpath, 64);
		ofw_parseofwdev(&bootdev, bootpath);
		break;
	case DEVT_NET:
		bootdev.d_dev = &netdev;
		strncpy(bootdev.d_kind.netif.path, bootpath, 64);
		bootdev.d_kind.netif.unit = 0;
		break;
	}

	env_setenv("currdev", EV_VOLATILE, ofw_fmtdev(&bootdev),
	    ofw_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, ofw_fmtdev(&bootdev),
	    env_noset, env_nounset);

	printf("%s\n", __progname);
	printf("bootpath=\"%s\"\n", bootpath);
	printf("loaddev=%s\n", getenv("loaddev"));
	printf("kernelpa=0x%lx\n", curkpg);

	/* Give control to the machine independent loader code. */
	interact();
	return 1;
}

typedef u_int64_t tte_t;

const char *page_sizes[] = {
	"  8k", " 64k", "512k", "  4m"
};

static void
pmap_print_tte(tte_t tag, tte_t tte)
{
	printf("%s %s ",
	    page_sizes[(tte & TD_SIZE_MASK) >> TD_SIZE_SHIFT],
	    tag & TD_G ? "G" : " ");
	printf(tte & TD_W ? "W " : "  ");
	printf(tte & TD_P ? "\e[33mP\e[0m " : "  ");
	printf(tte & TD_E ? "E " : "  ");
	printf(tte & TD_CV ? "CV " : "   ");
	printf(tte & TD_CP ? "CP " : "   ");
	printf(tte & TD_L ? "\e[32mL\e[0m " : "  ");
	printf(tte & TD_IE ? "IE " : "   ");
	printf(tte & TD_NFO ? "NFO " : "    ");
	printf("tag=0x%lx pa=0x%lx va=0x%lx ctx=%ld\n", tag, TD_PA(tte),
	    TT_VA(tag), TT_CTX(tag));
}
void
pmap_print_tlb(char which)
{
	int i;
	tte_t tte, tag;

	for (i = 0; i < 64*8; i += 8) {
		if (which == 'i') {
			__asm__ __volatile__("ldxa	[%1] %2, %0\n" :
			    "=r" (tag) : "r" (i),
			    "i" (ASI_ITLB_TAG_READ_REG));
			__asm__ __volatile__("ldxa	[%1] %2, %0\n" :
			    "=r" (tte) : "r" (i),
			    "i" (ASI_ITLB_DATA_ACCESS_REG));
		}
		else {
			__asm__ __volatile__("ldxa	[%1] %2, %0\n" :
			    "=r" (tag) : "r" (i),
			    "i" (ASI_DTLB_TAG_READ_REG));
			__asm__ __volatile__("ldxa	[%1] %2, %0\n" :
			    "=r" (tte) : "r" (i),
			    "i" (ASI_DTLB_DATA_ACCESS_REG));
		}
		if (!(tte & TD_V))
			continue;
		printf("%cTLB-%2u: ", which, i>>3);
		pmap_print_tte(tag, tte);
	}
}
