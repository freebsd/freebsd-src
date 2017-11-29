/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2006 Marcel Moolenaar
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

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>
#include <sys/boot.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "libi386.h"
#include <machine/bootinfo.h>

static const char howto_switches[] = "aCdrgDmphsv";
static int howto_masks[] = {
	RB_ASKNAME, RB_CDROM, RB_KDB, RB_DFLTROOT, RB_GDB, RB_MULTIPLE,
	RB_MUTE, RB_PAUSE, RB_SERIAL, RB_SINGLE, RB_VERBOSE
};

int
bi_getboothowto(char *kargs)
{
	const char *sw;
	char *opts;
	int howto, i;

	howto = 0;

	/* Get the boot options from the environment first. */
	for (i = 0; howto_names[i].ev != NULL; i++) {
		if (getenv(howto_names[i].ev) != NULL)
			howto |= howto_names[i].mask;
	}

	/* Parse kargs */
	if (kargs == NULL)
		return (howto);

	opts = strchr(kargs, '-');
	while (opts != NULL) {
		while (*(++opts) != '\0') {
			sw = strchr(howto_switches, *opts);
			if (sw == NULL)
				break;
			howto |= howto_masks[sw - howto_switches];
		}
		opts = strchr(opts, '-');
	}

	return (howto);
}

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
vm_offset_t
bi_copyenv(vm_offset_t start)
{
	struct env_var *ep;
	vm_offset_t addr, last;
	size_t len;

	addr = last = start;

	/* Traverse the environment. */
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		len = strlen(ep->ev_name);
		if (i386_copyin(ep->ev_name, addr, len) != len)
			break;
		addr += len;
		if (i386_copyin("=", addr, 1) != 1)
			break;
		addr++;
		if (ep->ev_value != NULL) {
			len = strlen(ep->ev_value);
			if (i386_copyin(ep->ev_value, addr, len) != len)
				break;
			addr += len;
		}
		if (i386_copyin("", addr, 1) != 1)
			break;
		last = ++addr;
	}

	if (i386_copyin("", last++, 1) != 1)
		last = start;
	return(last);
}

/*
 * Copy module-related data into the load area, where it can be
 * used as a directory for loaded modules.
 *
 * Module data is presented in a self-describing format.  Each datum
 * is preceded by a 32-bit identifier and a 32-bit size field.
 *
 * Currently, the following data are saved:
 *
 * MOD_NAME	(variable)		module name (string)
 * MOD_TYPE	(variable)		module type (string)
 * MOD_ARGS	(variable)		module parameters (string)
 * MOD_ADDR	sizeof(vm_offset_t)	module load address
 * MOD_SIZE	sizeof(size_t)		module size
 * MOD_METADATA	(variable)		type-specific metadata
 */
#define COPY32(v, a) {				\
    u_int32_t	x = (v);			\
    i386_copyin(&x, a, sizeof(x));		\
    a += sizeof(x);				\
}

#define MOD_STR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(strlen(s) + 1, a);			\
    i386_copyin(s, a, strlen(s) + 1);		\
    a += roundup(strlen(s) + 1, sizeof(u_int64_t));\
}

#define MOD_NAME(a, s)	MOD_STR(MODINFO_NAME, a, s)
#define MOD_TYPE(a, s)	MOD_STR(MODINFO_TYPE, a, s)
#define MOD_ARGS(a, s)	MOD_STR(MODINFO_ARGS, a, s)

#define MOD_VAR(t, a, s) {			\
    COPY32(t, a);				\
    COPY32(sizeof(s), a);			\
    i386_copyin(&s, a, sizeof(s));		\
    a += roundup(sizeof(s), sizeof(u_int64_t));	\
}

#define MOD_ADDR(a, s)	MOD_VAR(MODINFO_ADDR, a, s)
#define MOD_SIZE(a, s)	MOD_VAR(MODINFO_SIZE, a, s)

#define MOD_METADATA(a, mm) {			\
    COPY32(MODINFO_METADATA | mm->md_type, a);	\
    COPY32(mm->md_size, a);			\
    i386_copyin(mm->md_data, a, mm->md_size);	\
    a += roundup(mm->md_size, sizeof(u_int64_t));\
}

#define MOD_END(a) {				\
    COPY32(MODINFO_END, a);			\
    COPY32(0, a);				\
}

vm_offset_t
bi_copymodules(vm_offset_t addr)
{
	struct preloaded_file *fp;
	struct file_metadata *md;

	/* Start with the first module on the list, should be the kernel. */
	for (fp = file_findfile(NULL, NULL); fp != NULL; fp = fp->f_next) {
		/* The name field must come first. */
		MOD_NAME(addr, fp->f_name);
		MOD_TYPE(addr, fp->f_type);
		if (fp->f_args)
			MOD_ARGS(addr, fp->f_args);
		MOD_ADDR(addr, fp->f_addr);
		MOD_SIZE(addr, fp->f_size);
		for (md = fp->f_metadata; md != NULL; md = md->md_next) {
			if (!(md->md_type & MODINFOMD_NOCOPY))
				MOD_METADATA(addr, md);
		}
	}
	MOD_END(addr);
	return(addr);
}

/*
 * Load the information expected by the kernel.
 *
 * - The kernel environment is copied into kernel space.
 * - Module metadata are formatted and placed in kernel space.
 */
int
bi_load(struct preloaded_file *fp, uint64_t *bi_addr)
{
	struct bootinfo bi;
	struct preloaded_file *xp;
	struct file_metadata *md;
	struct devdesc *rootdev;
	char *rootdevname;
	vm_offset_t addr, ssym, esym;

	bzero(&bi, sizeof(struct bootinfo));
	bi.bi_version = 1;
//	bi.bi_boothowto = bi_getboothowto(fp->f_args);

	/*
	 * Allow the environment variable 'rootdev' to override the supplied
	 * device. This should perhaps go to MI code and/or have $rootdev
	 * tested/set by MI code before launching the kernel.
	 */
	rootdevname = getenv("rootdev");
	i386_getdev((void**)&rootdev, rootdevname, NULL);
	if (rootdev != NULL) {
		/* Try reading /etc/fstab to select the root device. */
		getrootmount(i386_fmtdev(rootdev));
		free(rootdev);
	}

	md = file_findmetadata(fp, MODINFOMD_SSYM);
	ssym = (md != NULL) ? *((vm_offset_t *)&(md->md_data)) : 0;
	md = file_findmetadata(fp, MODINFOMD_ESYM);
	esym = (md != NULL) ? *((vm_offset_t *)&(md->md_data)) : 0;
	if (ssym != 0 && esym != 0) {
		bi.bi_symtab = ssym;
		bi.bi_esymtab = esym;
	}

	/* Find the last module in the chain. */
	addr = 0;
	for (xp = file_findfile(NULL, NULL); xp != NULL; xp = xp->f_next) {
		if (addr < (xp->f_addr + xp->f_size))
			addr = xp->f_addr + xp->f_size;
	}

	addr = (addr + 15) & ~15;

	/* Copy module list and metadata. */
	bi.bi_modulep = addr;
	addr = bi_copymodules(addr);
	if (addr <= bi.bi_modulep) {
		addr = bi.bi_modulep;
		bi.bi_modulep = 0;
	}

	addr = (addr + 15) & ~15;

	/* Copy our environment. */
	bi.bi_envp = addr;
	addr = bi_copyenv(addr);
	if (addr <= bi.bi_envp) {
		addr = bi.bi_envp;
		bi.bi_envp = 0;
	}

	addr = (addr + PAGE_MASK) & ~PAGE_MASK;
	bi.bi_kernend = addr;

	return (ldr_bootinfo(&bi, bi_addr));
}
