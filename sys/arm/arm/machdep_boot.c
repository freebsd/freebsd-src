/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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

#include "opt_platform.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/linker.h>
#include <sys/physmem.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#if defined(LINUX_BOOT_ABI)
#include <sys/boot.h>
#endif

#include <machine/atags.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/vmparam.h>	/* For KERNVIRTADDR */

#ifdef FDT
#include <contrib/libfdt/libfdt.h>
#include <dev/fdt/fdt_common.h>
#endif

#ifdef EFI
#include <sys/efi.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

#ifdef DEBUG
#define	debugf(fmt, args...) printf(fmt, ##args)
#else
#define	debugf(fmt, args...)
#endif

#ifdef LINUX_BOOT_ABI
static char static_kenv[4096];
#endif

extern int *end;

static uint32_t board_revision;
/* hex representation of uint64_t */
static char board_serial[32];
static char *loader_envp;

#if defined(LINUX_BOOT_ABI)
#define LBABI_MAX_BANKS	10
#define CMDLINE_GUARD "FreeBSD:"
static uint32_t board_id;
static struct arm_lbabi_tag *atag_list;
static char linux_command_line[LBABI_MAX_COMMAND_LINE + 1];
static char atags[LBABI_MAX_COMMAND_LINE * 2];
#endif /* defined(LINUX_BOOT_ABI) */

SYSCTL_NODE(_hw, OID_AUTO, board, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Board attributes");
SYSCTL_UINT(_hw_board, OID_AUTO, revision, CTLFLAG_RD,
    &board_revision, 0, "Board revision");
SYSCTL_STRING(_hw_board, OID_AUTO, serial, CTLFLAG_RD,
    board_serial, 0, "Board serial");

int vfp_exists;
SYSCTL_INT(_hw, HW_FLOATINGPT, floatingpoint, CTLFLAG_RD,
    &vfp_exists, 0, "Floating point support enabled");

void
board_set_serial(uint64_t serial)
{

	snprintf(board_serial, sizeof(board_serial)-1,
		    "%016jx", serial);
}

void
board_set_revision(uint32_t revision)
{

	board_revision = revision;
}

static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

void
arm_print_kenv(void)
{
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (loader_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" loader_envp = 0x%08x\n", (uint32_t)loader_envp);

	for (cp = loader_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

#if defined(LINUX_BOOT_ABI)

/* Convert the U-Boot command line into FreeBSD kenv and boot options. */
static void
cmdline_set_env(char *cmdline, const char *guard)
{
	size_t guard_len;

	/* Skip leading spaces. */
	while (isspace(*cmdline))
		cmdline++;

	/* Test and remove guard. */
	if (guard != NULL && guard[0] != '\0') {
		guard_len  =  strlen(guard);
		if (strncasecmp(cmdline, guard, guard_len) != 0)
			return;
		cmdline += guard_len;
	}

	boothowto |= boot_parse_cmdline(cmdline);
}

/*
 * Called for armv6 and newer.
 */
void arm_parse_fdt_bootargs(void)
{

#ifdef FDT
	if (loader_envp == NULL && fdt_get_chosen_bootargs(linux_command_line,
	    LBABI_MAX_COMMAND_LINE) == 0) {
		init_static_kenv(static_kenv, sizeof(static_kenv));
		cmdline_set_env(linux_command_line, CMDLINE_GUARD);
	}
#endif
}

/*
 * Called for armv[45].
 */
static vm_offset_t
linux_parse_boot_param(struct arm_boot_params *abp)
{
	struct arm_lbabi_tag *walker;
	uint32_t revision;
	uint64_t serial;
	int size;
	vm_offset_t lastaddr;
#ifdef FDT
	struct fdt_header *dtb_ptr;
	uint32_t dtb_size;
#endif

	/*
	 * Linux boot ABI: r0 = 0, r1 is the board type (!= 0) and r2
	 * is atags or dtb pointer.  If all of these aren't satisfied,
	 * then punt. Unfortunately, it looks like DT enabled kernels
	 * doesn't uses board type and U-Boot delivers 0 in r1 for them.
	 */
	if (abp->abp_r0 != 0 || abp->abp_r2 == 0)
		return (0);
#ifdef FDT
	/* Test if r2 point to valid DTB. */
	dtb_ptr = (struct fdt_header *)abp->abp_r2;
	if (fdt_check_header(dtb_ptr) == 0) {
		dtb_size = fdt_totalsize(dtb_ptr);
		return (fake_preload_metadata(abp, dtb_ptr, dtb_size));
	}
#endif

	board_id = abp->abp_r1;
	walker = (struct arm_lbabi_tag *)abp->abp_r2;

	if (ATAG_TAG(walker) != ATAG_CORE)
		return 0;

	atag_list = walker;
	while (ATAG_TAG(walker) != ATAG_NONE) {
		switch (ATAG_TAG(walker)) {
		case ATAG_CORE:
			break;
		case ATAG_MEM:
			physmem_hardware_region(walker->u.tag_mem.start,
			    walker->u.tag_mem.size);
			break;
		case ATAG_INITRD2:
			break;
		case ATAG_SERIAL:
			serial = walker->u.tag_sn.high;
			serial <<= 32;
			serial |= walker->u.tag_sn.low;
			board_set_serial(serial);
			break;
		case ATAG_REVISION:
			revision = walker->u.tag_rev.rev;
			board_set_revision(revision);
			break;
		case ATAG_CMDLINE:
			size = ATAG_SIZE(walker) -
			    sizeof(struct arm_lbabi_header);
			size = min(size, LBABI_MAX_COMMAND_LINE);
			strncpy(linux_command_line, walker->u.tag_cmd.command,
			    size);
			linux_command_line[size] = '\0';
			break;
		default:
			break;
		}
		walker = ATAG_NEXT(walker);
	}

	/* Save a copy for later */
	bcopy(atag_list, atags,
	    (char *)walker - (char *)atag_list + ATAG_SIZE(walker));

	lastaddr = fake_preload_metadata(abp, NULL, 0);
	init_static_kenv(static_kenv, sizeof(static_kenv));
	cmdline_set_env(linux_command_line, CMDLINE_GUARD);
	return lastaddr;
}
#endif

#if defined(FREEBSD_BOOT_LOADER)
static vm_offset_t
freebsd_parse_boot_param(struct arm_boot_params *abp)
{
	vm_offset_t lastaddr = 0;
	void *mdp;
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	/*
	 * Mask metadata pointer: it is supposed to be on page boundary. If
	 * the first argument (mdp) doesn't point to a valid address the
	 * bootloader must have passed us something else than the metadata
	 * ptr, so we give up.  Also give up if we cannot find metadta section
	 * the loader creates that we get all this data out of.
	 */

	if ((mdp = (void *)(abp->abp_r0 & ~PAGE_MASK)) == NULL)
		return 0;
	preload_metadata = mdp;

	/* Initialize preload_kmdp */
	preload_initkmdp(false);
	if (preload_kmdp == NULL)
		return 0;

	boothowto = MD_FETCH(preload_kmdp, MODINFOMD_HOWTO, int);
	loader_envp = MD_FETCH(preload_kmdp, MODINFOMD_ENVP, char *);
	init_static_kenv(loader_envp, 0);
	lastaddr = MD_FETCH(preload_kmdp, MODINFOMD_KERNEND, vm_offset_t);
#ifdef DDB
	ksym_start = MD_FETCH(preload_kmdp, MODINFOMD_SSYM, uintptr_t);
	ksym_end = MD_FETCH(preload_kmdp, MODINFOMD_ESYM, uintptr_t);
	db_fetch_ksymtab(ksym_start, ksym_end, 0);
#endif
	return lastaddr;
}
#endif

vm_offset_t
default_parse_boot_param(struct arm_boot_params *abp)
{
	vm_offset_t lastaddr;

#if defined(LINUX_BOOT_ABI)
	if ((lastaddr = linux_parse_boot_param(abp)) != 0)
		return lastaddr;
#endif
#if defined(FREEBSD_BOOT_LOADER)
	if ((lastaddr = freebsd_parse_boot_param(abp)) != 0)
		return lastaddr;
#endif
	/* Fall back to hardcoded metadata. */
	lastaddr = fake_preload_metadata(abp, NULL, 0);

	return lastaddr;
}

/*
 * Stub version of the boot parameter parsing routine.  We are
 * called early in initarm, before even VM has been initialized.
 * This routine needs to preserve any data that the boot loader
 * has passed in before the kernel starts to grow past the end
 * of the BSS, traditionally the place boot-loaders put this data.
 *
 * Since this is called so early, things that depend on the vm system
 * being setup (including access to some SoC's serial ports), about
 * all that can be done in this routine is to copy the arguments.
 *
 * This is the default boot parameter parsing routine.  Individual
 * kernels/boards can override this weak function with one of their
 * own.  We just fake metadata...
 */
__weak_reference(default_parse_boot_param, parse_boot_param);

/*
 * Fake up a boot descriptor table
 */
vm_offset_t
fake_preload_metadata(struct arm_boot_params *abp __unused, void *dtb_ptr,
    size_t dtb_size)
{
	vm_offset_t lastaddr;
	int i = 0;
	static uint32_t fake_preload[35];

	lastaddr = (vm_offset_t)&end;

	fake_preload[i++] = MODINFO_NAME;
	fake_preload[i++] = strlen("kernel") + 1;
	strcpy((char *)&fake_preload[i++], "kernel");
	i += 1;
	fake_preload[i++] = MODINFO_TYPE;
	fake_preload[i++] = strlen(preload_kerntype) + 1;
	strcpy((char *)&fake_preload[i], preload_kerntype);
	i += howmany(fake_preload[i - 1], sizeof(uint32_t));
	fake_preload[i++] = MODINFO_ADDR;
	fake_preload[i++] = sizeof(vm_offset_t);
	fake_preload[i++] = KERNVIRTADDR;
	fake_preload[i++] = MODINFO_SIZE;
	fake_preload[i++] = sizeof(uint32_t);
	fake_preload[i++] = (uint32_t)&end - KERNVIRTADDR;
	if (dtb_ptr != NULL) {
		/* Copy DTB to KVA space and insert it into module chain. */
		lastaddr = roundup(lastaddr, sizeof(int));
		fake_preload[i++] = MODINFO_METADATA | MODINFOMD_DTBP;
		fake_preload[i++] = sizeof(uint32_t);
		fake_preload[i++] = (uint32_t)lastaddr;
		memmove((void *)lastaddr, dtb_ptr, dtb_size);
		lastaddr += dtb_size;
		lastaddr = roundup(lastaddr, sizeof(int));
	}
	fake_preload[i++] = 0;
	fake_preload[i] = 0;
	preload_metadata = (void *)fake_preload;

	/* Initialize preload_kmdp */
	preload_initkmdp(true);

	init_static_kenv(NULL, 0);

	return (lastaddr);
}
