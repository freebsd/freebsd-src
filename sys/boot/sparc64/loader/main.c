/*-
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file. 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/queue.h>
#include <sys/linker.h>

#include <machine/asi.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/lsu.h>
#include <machine/metadata.h>
#include <machine/tte.h>
#include <machine/upa.h>

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

extern void itlb_enter(u_long vpn, u_long data);
extern void dtlb_enter(u_long vpn, u_long data);
extern vm_offset_t itlb_va_to_pa(vm_offset_t);
extern vm_offset_t dtlb_va_to_pa(vm_offset_t);
extern vm_offset_t md_load(char *, vm_offset_t *);
static int __elfN(exec)(struct preloaded_file *);
static int sparc64_autoload(void);
static int mmu_mapin(vm_offset_t, vm_size_t);

extern char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

struct tlb_entry *dtlb_store;
struct tlb_entry *itlb_store;

int dtlb_slot;
int itlb_slot;
int dtlb_slot_max;
int itlb_slot_max;

vm_offset_t curkva = 0;
vm_offset_t heapva;
phandle_t pmemh;	/* OFW memory handle */

struct memory_slice memslices[18];

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
	__elfN(loadfile),
	__elfN(exec)
};
struct file_format *file_formats[] = {
	&sparc64_elf,
	0
};
struct fs_ops *file_system[] = {
#ifdef LOADER_UFS_SUPPORT
	&ufs_fsops,
#endif
#ifdef LOADER_CD9660_SUPPORT
	&cd9660_fsops,
#endif
#ifdef LOADER_ZIP_SUPPORT
	&zipfs_fsops,
#endif
#ifdef LOADER_GZIP_SUPPORT
	&gzipfs_fsops,
#endif
#ifdef LOADER_BZIP2_SUPPORT
	&bzipfs_fsops,
#endif
#ifdef LOADER_NFS_SUPPORT
	&nfs_fsops,
#endif
#ifdef LOADER_TFTP_SUPPORT
	&tftp_fsops,
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

#ifdef LOADER_DEBUG
static int
watch_phys_set_mask(vm_offset_t pa, u_long mask)
{
	u_long lsucr;

	stxa(AA_DMMU_PWPR, ASI_DMMU, pa & (((2UL << 38) - 1) << 3));
	lsucr = ldxa(0, ASI_LSU_CTL_REG);
	lsucr = ((lsucr | LSU_PW) & ~LSU_PM_MASK) |
	    (mask << LSU_PM_SHIFT);
	stxa(0, ASI_LSU_CTL_REG, lsucr);
	return (0);
}

static int
watch_phys_set(vm_offset_t pa, int sz)
{
	u_long off;

	off = (u_long)pa & 7;
	/* Test for misaligned watch points. */
	if (off + sz > 8)
		return (-1);
	return (watch_phys_set_mask(pa, ((1 << sz) - 1) << off));
}


static int
watch_virt_set_mask(vm_offset_t va, u_long mask)
{
	u_long lsucr;

	stxa(AA_DMMU_VWPR, ASI_DMMU, va & (((2UL << 41) - 1) << 3));
	lsucr = ldxa(0, ASI_LSU_CTL_REG);
	lsucr = ((lsucr | LSU_VW) & ~LSU_VM_MASK) |
	    (mask << LSU_VM_SHIFT);
	stxa(0, ASI_LSU_CTL_REG, lsucr);
	return (0);
}

static int
watch_virt_set(vm_offset_t va, int sz)
{
	u_long off;

	off = (u_long)va & 7;
	/* Test for misaligned watch points. */
	if (off + sz > 8)
		return (-1);
	return (watch_virt_set_mask(va, ((1 << sz) - 1) << off));
}
#endif

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
__elfN(exec)(struct preloaded_file *fp)
{
	struct file_metadata *fmp;
	vm_offset_t mdp;
	Elf_Addr entry;
	Elf_Ehdr *e;
	int error;

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == 0) {
		return EFTYPE;
	}
	e = (Elf_Ehdr *)&fmp->md_data;

	if ((error = md_load(fp->f_args, &mdp)) != 0)
		return error;

	printf("jumping to kernel entry at %#lx.\n", e->e_entry);
#if 0
	pmap_print_tlb('i');
	pmap_print_tlb('d');
#endif

	entry = e->e_entry;

	OF_release(heapva, HEAPSZ);

	((kernel_entry_t *)entry)(mdp, 0, 0, 0, openfirmware);

	panic("exec returned");
}

static int
mmu_mapin(vm_offset_t va, vm_size_t len)
{
	vm_offset_t pa, mva;
	u_long data;

	if (va + len > curkva)
		curkva = va + len;

	pa = (vm_offset_t)-1;
	len += va & PAGE_MASK_4M;
	va &= ~PAGE_MASK_4M;
	while (len) {
		if (dtlb_va_to_pa(va) == (vm_offset_t)-1 ||
		    itlb_va_to_pa(va) == (vm_offset_t)-1) {
			/* Allocate a physical page, claim the virtual area */
			if (pa == (vm_offset_t)-1) {
				pa = (vm_offset_t)OF_alloc_phys(PAGE_SIZE_4M,
				    PAGE_SIZE_4M);
				if (pa == (vm_offset_t)-1)
					panic("out of memory");
				mva = (vm_offset_t)OF_claim_virt(va,
				    PAGE_SIZE_4M, 0);
				if (mva != va) {
					panic("can't claim virtual page "
					    "(wanted %#lx, got %#lx)",
					    va, mva);
				}
				/* The mappings may have changed, be paranoid. */
				continue;
			}
			/*
			 * Actually, we can only allocate two pages less at
			 * most (depending on the kernel TSB size).
			 */
			if (dtlb_slot >= dtlb_slot_max)
				panic("mmu_mapin: out of dtlb_slots");
			if (itlb_slot >= itlb_slot_max)
				panic("mmu_mapin: out of itlb_slots");
			data = TD_V | TD_4M | TD_PA(pa) | TD_L | TD_CP |
			    TD_CV | TD_P | TD_W;
			dtlb_store[dtlb_slot].te_pa = pa;
			dtlb_store[dtlb_slot].te_va = va;
			itlb_store[itlb_slot].te_pa = pa;
			itlb_store[itlb_slot].te_va = va;
			dtlb_slot++;
			itlb_slot++;
			dtlb_enter(va, data);
			itlb_enter(va, data);
			pa = (vm_offset_t)-1;
		}
		len -= len > PAGE_SIZE_4M ? PAGE_SIZE_4M : len;
		va += PAGE_SIZE_4M;
	}
	if (pa != (vm_offset_t)-1)
		OF_release_phys(pa, PAGE_SIZE_4M);
	return 0;
}

static vm_offset_t
init_heap(void)
{
	if ((pmemh = OF_finddevice("/memory")) == (phandle_t)-1)
		OF_exit();
	if (OF_getprop(pmemh, "available", memslices, sizeof(memslices)) <= 0)
		OF_exit();

	/* There is no need for continuous physical heap memory. */
	heapva = (vm_offset_t)OF_claim((void *)HEAPVA, HEAPSZ, 32);
	return heapva;
}

static void
tlb_init(void)
{
	phandle_t child;
	phandle_t root;
	char buf[128];
	u_int bootcpu;
	u_int cpu;

	bootcpu = UPA_CR_GET_MID(ldxa(0, ASI_UPA_CONFIG_REG));
	if ((root = OF_peer(0)) == -1)
		panic("main: OF_peer");
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (child == -1)
			panic("main: OF_child");
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "cpu") == 0) {
			if (OF_getprop(child, "upa-portid", &cpu,
			    sizeof(cpu)) == -1 && OF_getprop(child, "portid",
			    &cpu, sizeof(cpu)) == -1)
				panic("main: OF_getprop");
			if (cpu == bootcpu)
				break;
		}
	}
	if (cpu != bootcpu)
		panic("init_tlb: no node for bootcpu?!?!");
	if (OF_getprop(child, "#dtlb-entries", &dtlb_slot_max,
	    sizeof(dtlb_slot_max)) == -1 ||
	    OF_getprop(child, "#itlb-entries", &itlb_slot_max,
	    sizeof(itlb_slot_max)) == -1)
		panic("init_tlb: OF_getprop");
	dtlb_store = malloc(dtlb_slot_max * sizeof(*dtlb_store));
	itlb_store = malloc(itlb_slot_max * sizeof(*itlb_store));
	if (dtlb_store == NULL || itlb_store == NULL)
		panic("init_tlb: malloc");
}

int
main(int (*openfirm)(void *))
{
	char bootpath[64];
	struct devsw **dp;
	phandle_t chosenh;

	/*
	 * Tell the Open Firmware functions where they find the ofw gate.
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

	tlb_init();

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

	/*
	 * Sun compatible bootable CD-ROMs have a disk label placed
	 * before the cd9660 data, with the actual filesystem being
	 * in the first partition, while the other partitions contain
	 * pseudo disk labels with embedded boot blocks for different
	 * architectures, which may be followed by UFS filesystems.
	 * The firmware will set the boot path to the partition it
	 * boots from ('f' in the sun4u case), but we want the kernel
	 * to be loaded from the cd9660 fs ('a'), so the boot path
	 * needs to be altered.
	 */
	if (bootpath[strlen(bootpath) - 2] == ':' &&
	    bootpath[strlen(bootpath) - 1] == 'f') {
		bootpath[strlen(bootpath) - 1] = 'a';
		printf("Boot path set to %s\n", bootpath);
	}

	env_setenv("currdev", EV_VOLATILE, bootpath,
	    ofw_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, bootpath,
	    env_noset, env_nounset);

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("bootpath=\"%s\"\n", bootpath);

	/* Give control to the machine independent loader code. */
	interact();
	return 1;
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	printf("Rebooting...\n");
	OF_exit();
}

/* provide this for panic, as it's not in the startup code */
void
exit(int code)
{
	OF_exit();
}

#ifdef LOADER_DEBUG
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
#endif
