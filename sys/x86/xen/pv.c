/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 Christian Limpach.
 * Copyright (c) 2004-2006,2008 Kip Macy
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * Copyright (c) 2013 Roger Pau Monné <roger.pau@citrix.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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

#include "opt_ddb.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/boot.h>
#include <sys/ctype.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/efi.h>
#include <sys/tslog.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#include <machine/_inttypes.h>
#include <machine/intr_machdep.h>
#include <x86/apicvar.h>
#include <x86/init.h>
#include <machine/pc/bios.h>
#include <machine/smp.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/cpu.h>

#include <xen/xen-os.h>
#include <xen/hvm.h>
#include <xen/hypervisor.h>
#include <xen/xenstore/xenstorevar.h>
#include <xen/xen_pv.h>

#include <contrib/xen/arch-x86/cpuid.h>
#include <contrib/xen/arch-x86/hvm/start_info.h>
#include <contrib/xen/vcpu.h>

#include <dev/xen/timer/timer.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

/* Native initial function */
extern u_int64_t hammer_time(u_int64_t, u_int64_t);
/* Xen initial function */
uint64_t hammer_time_xen(vm_paddr_t);

#define MAX_E820_ENTRIES	128

/*--------------------------- Forward Declarations ---------------------------*/
static caddr_t xen_pvh_parse_preload_data(uint64_t);
static void pvh_parse_memmap(caddr_t, vm_paddr_t *, int *);

/*---------------------------- Extern Declarations ---------------------------*/
/*
 * Placed by the linker at the end of the bss section, which is the last
 * section loaded by Xen before loading the symtab and strtab.
 */
extern uint32_t end;

/*-------------------------------- Global Data -------------------------------*/
struct init_ops xen_pvh_init_ops = {
	.parse_preload_data		= xen_pvh_parse_preload_data,
	.early_clock_source_init	= xen_clock_init,
	.early_delay			= xen_delay,
	.parse_memmap			= pvh_parse_memmap,
};

static struct bios_smap xen_smap[MAX_E820_ENTRIES];

static struct hvm_start_info *start_info;

/*-------------------------------- Xen PV init -------------------------------*/

static int
isxen(void)
{
	static int xen = -1;
	uint32_t base;
	u_int regs[4];

	if (xen != -1)
		return (xen);

	/*
	 * The full code for identifying which hypervisor we're running under
	 * is in sys/x86/x86/identcpu.c and runs later in the boot process;
	 * this is sufficient to distinguish Xen PVH booting from non-Xen PVH
	 * and skip some very early Xen-specific code in the non-Xen case.
	 */
	xen = 0;
	for (base = 0x40000000; base < 0x40010000; base += 0x100) {
		do_cpuid(base, regs);
		if (regs[1] == XEN_CPUID_SIGNATURE_EBX &&
		    regs[2] == XEN_CPUID_SIGNATURE_ECX &&
		    regs[3] == XEN_CPUID_SIGNATURE_EDX) {
			xen = 1;
			break;
		}
	}
	return (xen);
}

#define CRASH(...) do {					\
	if (isxen()) {					\
		xc_printf(__VA_ARGS__);			\
		HYPERVISOR_shutdown(SHUTDOWN_crash);	\
	} else {					\
		halt();					\
	}						\
} while (0)

uint64_t
hammer_time_xen(vm_paddr_t start_info_paddr)
{
	struct hvm_modlist_entry *mod;
	struct xen_add_to_physmap xatp;
	uint64_t physfree;
	char *kenv;
	int rc;

	if (isxen()) {
		xen_domain_type = XEN_HVM_DOMAIN;
		vm_guest = VM_GUEST_XEN;
		rc = xen_hvm_init_hypercall_stubs(XEN_HVM_INIT_EARLY);
		if (rc) {
			xc_printf("ERROR: failed to initialize hypercall page: %d\n",
			    rc);
			HYPERVISOR_shutdown(SHUTDOWN_crash);
		}
	}

	start_info = (struct hvm_start_info *)(start_info_paddr + KERNBASE);
	if (start_info->magic != XEN_HVM_START_MAGIC_VALUE) {
		CRASH("Unknown magic value in start_info struct: %#x\n",
		    start_info->magic);
	}

	/*
	 * Select the higher address to use as physfree: either after
	 * start_info, after the kernel, after the memory map or after any of
	 * the modules.  We assume enough memory to be available after the
	 * selected address for the needs of very early memory allocations.
	 */
	physfree = roundup2(start_info_paddr + sizeof(struct hvm_start_info),
	    PAGE_SIZE);
	physfree = MAX(roundup2((vm_paddr_t)_end - KERNBASE, PAGE_SIZE),
	    physfree);

	if (start_info->memmap_paddr != 0)
		physfree = MAX(roundup2(start_info->memmap_paddr +
		    start_info->memmap_entries *
		    sizeof(struct hvm_memmap_table_entry), PAGE_SIZE),
		    physfree);

	if (start_info->modlist_paddr != 0) {
		unsigned int i;

		if (start_info->nr_modules == 0) {
			CRASH(
			    "ERROR: modlist_paddr != 0 but nr_modules == 0\n");
		}
		mod = (struct hvm_modlist_entry *)
		    (start_info->modlist_paddr + KERNBASE);
		for (i = 0; i < start_info->nr_modules; i++)
			physfree = MAX(roundup2(mod[i].paddr + mod[i].size,
			    PAGE_SIZE), physfree);
	}

	if (isxen()) {
		xatp.domid = DOMID_SELF;
		xatp.idx = 0;
		xatp.space = XENMAPSPACE_shared_info;
		xatp.gpfn = atop(physfree);
		if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp)) {
			xc_printf("ERROR: failed to setup shared_info page\n");
			HYPERVISOR_shutdown(SHUTDOWN_crash);
		}
		HYPERVISOR_shared_info = (shared_info_t *)(physfree + KERNBASE);
		physfree += PAGE_SIZE;
	}

	/*
	 * Init a static kenv using a free page. The contents will be filled
	 * from the parse_preload_data hook.
	 */
	kenv = (void *)(physfree + KERNBASE);
	physfree += PAGE_SIZE;
	bzero_early(kenv, PAGE_SIZE);
	init_static_kenv(kenv, PAGE_SIZE);

	/* Set the hooks for early functions that diverge from bare metal */
	init_ops = xen_pvh_init_ops;
	hvm_start_flags = start_info->flags;

	/* Now we can jump into the native init function */
	return (hammer_time(0, physfree));
}

/*-------------------------------- PV specific -------------------------------*/

/*
 * When booted as a PVH guest FreeBSD needs to avoid using the RSDP address
 * hint provided by the loader because it points to the native set of ACPI
 * tables instead of the ones crafted by Xen. The acpi.rsdp env variable is
 * removed from kenv if present, and a new acpi.rsdp is added to kenv that
 * points to the address of the Xen crafted RSDP.
 */
static bool reject_option(const char *option)
{
	static const char *reject[] = {
		"acpi.rsdp",
	};
	unsigned int i;

	for (i = 0; i < nitems(reject); i++)
		if (strncmp(option, reject[i], strlen(reject[i])) == 0)
			return (true);

	return (false);
}

static void
xen_pvh_set_env(char *env, bool (*filter)(const char *))
{
	char *option;

	if (env == NULL)
		return;

	option = env;
	while (*option != 0) {
		char *value;

		if (filter != NULL && filter(option)) {
			option += strlen(option) + 1;
			continue;
		}

		value = option;
		option = strsep(&value, "=");
		if (kern_setenv(option, value) != 0 && isxen())
			xc_printf("unable to add kenv %s=%s\n", option, value);
		option = value + strlen(value) + 1;
	}
}

#ifdef DDB
/*
 * The way Xen loads the symtab is different from the native boot loader,
 * because it's tailored for NetBSD. So we have to adapt and use the same
 * method as NetBSD. Portions of the code below have been picked from NetBSD:
 * sys/kern/kern_ksyms.c CVS Revision 1.71.
 */
static void
xen_pvh_parse_symtab(void)
{
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	int i, j;

	ehdr = (Elf_Ehdr *)(&end + 1);
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
	    ehdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    ehdr->e_version > 1) {
		if (isxen())
			xc_printf("Unable to load ELF symtab: invalid symbol table\n");
		return;
	}

	shdr = (Elf_Shdr *)((uint8_t *)ehdr + ehdr->e_shoff);
	/* Find the symbol table and the corresponding string table. */
	for (i = 1; i < ehdr->e_shnum; i++) {
		if (shdr[i].sh_type != SHT_SYMTAB)
			continue;
		if (shdr[i].sh_offset == 0)
			continue;
		ksymtab = (uintptr_t)((uint8_t *)ehdr + shdr[i].sh_offset);
		ksymtab_size = shdr[i].sh_size;
		j = shdr[i].sh_link;
		if (shdr[j].sh_offset == 0)
			continue; /* Can this happen? */
		kstrtab = (uintptr_t)((uint8_t *)ehdr + shdr[j].sh_offset);
		break;
	}

	if ((ksymtab == 0 || kstrtab == 0) && isxen())
		xc_printf(
    "Unable to load ELF symtab: could not find symtab or strtab\n");
}
#endif

static void
fixup_console(caddr_t kmdp)
{
	struct xen_platform_op op = {
		.cmd = XENPF_get_dom0_console,
	};
	xenpf_dom0_console_t *console = &op.u.dom0_console;
	union {
		struct efi_fb efi;
		struct vbe_fb vbe;
	} *fb = NULL;
	int size;

	size = HYPERVISOR_platform_op(&op);
	if (size < 0) {
		xc_printf("Failed to get dom0 video console info: %d\n", size);
		return;
	}

	switch (console->video_type) {
	case XEN_VGATYPE_VESA_LFB:
		fb = (__typeof__ (fb))preload_search_info(kmdp,
		    MODINFO_METADATA | MODINFOMD_VBE_FB);

		if (fb == NULL) {
			xc_printf("No VBE FB in kernel metadata\n");
			return;
		}

		_Static_assert(offsetof(struct vbe_fb, fb_bpp) ==
		    offsetof(struct efi_fb, fb_mask_reserved) +
		    sizeof(fb->efi.fb_mask_reserved),
		    "Bad structure overlay\n");
		fb->vbe.fb_bpp = console->u.vesa_lfb.bits_per_pixel;
		/* FALLTHROUGH */
	case XEN_VGATYPE_EFI_LFB:
		if (fb == NULL) {
			fb = (__typeof__ (fb))preload_search_info(kmdp,
			    MODINFO_METADATA | MODINFOMD_EFI_FB);
			if (fb == NULL) {
				xc_printf("No EFI FB in kernel metadata\n");
				return;
			}
		}

		fb->efi.fb_addr = console->u.vesa_lfb.lfb_base;
		if (size >
		    offsetof(xenpf_dom0_console_t, u.vesa_lfb.ext_lfb_base))
			fb->efi.fb_addr |=
			    (uint64_t)console->u.vesa_lfb.ext_lfb_base << 32;
		fb->efi.fb_size = console->u.vesa_lfb.lfb_size << 16;
		fb->efi.fb_height = console->u.vesa_lfb.height;
		fb->efi.fb_width = console->u.vesa_lfb.width;
		fb->efi.fb_stride = (console->u.vesa_lfb.bytes_per_line << 3) /
		    console->u.vesa_lfb.bits_per_pixel;
#define FBMASK(c) \
    ((~0u << console->u.vesa_lfb.c ## _pos) & \
    (~0u >> (32 - console->u.vesa_lfb.c ## _pos - \
    console->u.vesa_lfb.c ## _size)))
		fb->efi.fb_mask_red = FBMASK(red);
		fb->efi.fb_mask_green = FBMASK(green);
		fb->efi.fb_mask_blue = FBMASK(blue);
		fb->efi.fb_mask_reserved = FBMASK(rsvd);
#undef FBMASK
		break;

	default:
		xc_printf("Video console type unsupported\n");
		return;
	}
}

static caddr_t
xen_pvh_parse_preload_data(uint64_t modulep)
{
	caddr_t kmdp;
	vm_ooffset_t off;
	vm_paddr_t metadata;
	char *envp;
	char acpi_rsdp[19];

	TSENTER();
	if (start_info->modlist_paddr != 0) {
		struct hvm_modlist_entry *mod;
		const char *cmdline;

		mod = (struct hvm_modlist_entry *)
		    (start_info->modlist_paddr + KERNBASE);
		cmdline = mod[0].cmdline_paddr ?
		    (const char *)(mod[0].cmdline_paddr + KERNBASE) : NULL;

		if (strcmp(cmdline, "header") == 0) {
			struct xen_header *header;

			header = (struct xen_header *)(mod[0].paddr + KERNBASE);

			if ((header->flags & XENHEADER_HAS_MODULEP_OFFSET) !=
			    XENHEADER_HAS_MODULEP_OFFSET) {
				xc_printf("Unable to load module metadata\n");
				HYPERVISOR_shutdown(SHUTDOWN_crash);
			}

			preload_metadata = (caddr_t)(mod[0].paddr +
			    header->modulep_offset + KERNBASE);

			kmdp = preload_search_by_type("elf kernel");
			if (kmdp == NULL)
				kmdp = preload_search_by_type("elf64 kernel");
			if (kmdp == NULL) {
				xc_printf("Unable to find kernel\n");
				HYPERVISOR_shutdown(SHUTDOWN_crash);
			}

			/*
			 * Xen has relocated the metadata and the modules, so
			 * we need to recalculate it's position. This is done
			 * by saving the original modulep address and then
			 * calculating the offset from the real modulep
			 * position.
			 */
			metadata = MD_FETCH(kmdp, MODINFOMD_MODULEP,
			    vm_paddr_t);
			off = mod[0].paddr + header->modulep_offset - metadata +
			    KERNBASE;
		} else {
			preload_metadata = (caddr_t)(mod[0].paddr + KERNBASE);

			kmdp = preload_search_by_type("elf kernel");
			if (kmdp == NULL)
				kmdp = preload_search_by_type("elf64 kernel");
			if (kmdp == NULL) {
				xc_printf("Unable to find kernel\n");
				HYPERVISOR_shutdown(SHUTDOWN_crash);
			}

			metadata = MD_FETCH(kmdp, MODINFOMD_MODULEP, vm_paddr_t);
			off = mod[0].paddr + KERNBASE - metadata;
		}

		preload_bootstrap_relocate(off);

		boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
		envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
		if (envp != NULL)
			envp += off;
		xen_pvh_set_env(envp, reject_option);

		if (MD_FETCH(kmdp, MODINFOMD_EFI_MAP, void *) != NULL)
		    strlcpy(bootmethod, "UEFI", sizeof(bootmethod));
		else
		    strlcpy(bootmethod, "BIOS", sizeof(bootmethod));

		fixup_console(kmdp);
	} else {
		/* Parse the extra boot information given by Xen */
		if (start_info->cmdline_paddr != 0)
			boot_parse_cmdline_delim(
			    (char *)(start_info->cmdline_paddr + KERNBASE),
			    ", \t\n");
		kmdp = NULL;
		strlcpy(bootmethod, "PVH", sizeof(bootmethod));
	}

	boothowto |= boot_env_to_howto();

	snprintf(acpi_rsdp, sizeof(acpi_rsdp), "%#" PRIx64,
	    start_info->rsdp_paddr);
	kern_setenv("acpi.rsdp", acpi_rsdp);

#ifdef DDB
	xen_pvh_parse_symtab();
#endif
	TSEXIT();
	return (kmdp);
}

static void
pvh_parse_memmap_start_info(caddr_t kmdp, vm_paddr_t *physmap,
    int *physmap_idx)
{
	const struct hvm_memmap_table_entry * entries;
	size_t nentries;
	size_t i;

	/* Extract from HVM start_info. */
	entries = (struct hvm_memmap_table_entry *)(start_info->memmap_paddr + KERNBASE);
	nentries = start_info->memmap_entries;

	/* Convert into E820 format and handle one by one. */
	for (i = 0; i < nentries; i++) {
		struct bios_smap entry;

		entry.base = entries[i].addr;
		entry.length = entries[i].size;

		/*
		 * Luckily for us, the XEN_HVM_MEMMAP_TYPE_* values exactly
		 * match the SMAP_TYPE_* values so we don't need to translate
		 * anything here.
		 */
		entry.type = entries[i].type;

		bios_add_smap_entries(&entry, 1, physmap, physmap_idx);
	}
}

static void
xen_pvh_parse_memmap(caddr_t kmdp, vm_paddr_t *physmap, int *physmap_idx)
{
	struct xen_memory_map memmap;
	u_int32_t size;
	int rc;

	/* We should only reach here if we're running under Xen. */
	KASSERT(isxen(), ("xen_pvh_parse_memmap reached when !Xen"));

	/* Fetch the E820 map from Xen */
	memmap.nr_entries = MAX_E820_ENTRIES;
	set_xen_guest_handle(memmap.buffer, xen_smap);
	rc = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if (rc) {
		xc_printf("ERROR: unable to fetch Xen E820 memory map: %d\n",
		    rc);
		HYPERVISOR_shutdown(SHUTDOWN_crash);
	}

	size = memmap.nr_entries * sizeof(xen_smap[0]);

	bios_add_smap_entries(xen_smap, size, physmap, physmap_idx);
}

static void
pvh_parse_memmap(caddr_t kmdp, vm_paddr_t *physmap, int *physmap_idx)
{

	/*
	 * If version >= 1 and memmap_paddr != 0, use the memory map provided
	 * in the start_info structure; if not, we're running under legacy
	 * Xen and need to use the Xen hypercall.
	 */
	if ((start_info->version >= 1) && (start_info->memmap_paddr != 0))
		pvh_parse_memmap_start_info(kmdp, physmap, physmap_idx);
	else
		xen_pvh_parse_memmap(kmdp, physmap, physmap_idx);
}
