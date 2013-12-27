/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#ifdef SW_WATCHDOG
#include <sys/watchdog.h>
#endif
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <machine/elf.h>
#include <machine/md_var.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

/*
 * Don't touch the first SIZEOF_METADATA bytes on the dump device. This
 * is to protect us from metadata and to protect metadata from us.
 */
#define	SIZEOF_METADATA		(64*1024)

#define	MD_ALIGN(x)	(((off_t)(x) + EFI_PAGE_MASK) & ~EFI_PAGE_MASK)
#define	DEV_ALIGN(x)	(((off_t)(x) + (DEV_BSIZE-1)) & ~(DEV_BSIZE-1))

static int minidump = 0;
TUNABLE_INT("debug.minidump", &minidump);
SYSCTL_INT(_debug, OID_AUTO, minidump, CTLFLAG_RW, &minidump, 0,
    "Enable mini crash dumps");

static struct kerneldumpheader kdh;
static off_t dumplo, fileofs;

/* Handle buffered writes. */
static char buffer[DEV_BSIZE];
static size_t fragsz;

static int
buf_write(struct dumperinfo *di, char *ptr, size_t sz)
{
	size_t len;
	int error;

	while (sz) {
		len = DEV_BSIZE - fragsz;
		if (len > sz)
			len = sz;
		bcopy(ptr, buffer + fragsz, len);
		fragsz += len;
		ptr += len;
		sz -= len;
		if (fragsz == DEV_BSIZE) {
			error = dump_write(di, buffer, 0, dumplo,
			    DEV_BSIZE);
			if (error)
				return (error);
			dumplo += DEV_BSIZE;
			fragsz = 0;
		}
	}

	return (0);
}

static int
buf_flush(struct dumperinfo *di)
{
	int error;

	if (fragsz == 0)
		return (0);

	error = dump_write(di, buffer, 0, dumplo, DEV_BSIZE);
	dumplo += DEV_BSIZE;
	fragsz = 0;
	return (error);
}

/*
 * Physical dump support
 */

typedef int phys_callback_t(struct efi_md*, int, void*);

static int
phys_cb_dumpdata(struct efi_md *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	vm_offset_t pa;
	uint64_t pgs;
	size_t counter, sz;
	int c, error, twiddle;

	error = 0;	/* catch case in which mdp->md_pages is 0 */
	counter = 0;	/* Update twiddle every 16MB */
	twiddle = 0;
	pgs = mdp->md_pages;
	pa = IA64_PHYS_TO_RR7(mdp->md_phys);

	printf("  chunk %d: %ld pages ", seqnr, (long)pgs);

	while (pgs) {
		sz = (pgs > (DFLTPHYS >> EFI_PAGE_SHIFT))
		    ? DFLTPHYS : pgs << EFI_PAGE_SHIFT;
		counter += sz;
		if (counter >> 24) {
			printf("%c\b", "|/-\\"[twiddle++ & 3]);
			counter &= (1<<24) - 1;
		}
#ifdef SW_WATCHDOG
		wdog_kern_pat(WD_LASTVAL);
#endif
		error = dump_write(di, (void*)pa, 0, dumplo, sz);
		if (error)
			break;
		dumplo += sz;
		pgs -= sz >> EFI_PAGE_SHIFT;
		pa += sz;

		/* Check for user abort. */
		c = cncheckc();
		if (c == 0x03)
			return (ECANCELED);
		if (c != -1)
			printf("(CTRL-C to abort)  ");
	}
	printf("... %s\n", (error) ? "fail" : "ok");
	return (error);
}

static int
phys_cb_dumphdr(struct efi_md *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	Elf64_Phdr phdr;
	int error;

	bzero(&phdr, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R;			/* XXX */
	phdr.p_offset = fileofs;
	phdr.p_vaddr = (uintptr_t)mdp->md_virt;	/* XXX probably bogus. */
	phdr.p_paddr = mdp->md_phys;
	phdr.p_filesz = mdp->md_pages << EFI_PAGE_SHIFT;
	phdr.p_memsz = mdp->md_pages << EFI_PAGE_SHIFT;
	phdr.p_align = EFI_PAGE_SIZE;

	error = buf_write(di, (char*)&phdr, sizeof(phdr));
	fileofs += phdr.p_filesz;
	return (error);
}

static int
phys_cb_size(struct efi_md *mdp, int seqnr, void *arg)
{
	uint64_t *sz = (uint64_t*)arg;

	*sz += (uint64_t)mdp->md_pages << EFI_PAGE_SHIFT;
	return (0);
}

static int
phys_foreach(phys_callback_t cb, void *arg)
{
	struct efi_md *mdp;
	int error, seqnr;

	seqnr = 0;
	mdp = efi_md_first();
	while (mdp != NULL) {
		if (mdp->md_type == EFI_MD_TYPE_FREE ||
		    mdp->md_type == EFI_MD_TYPE_DATA ||
		    mdp->md_type == EFI_MD_TYPE_CODE ||
		    mdp->md_type == EFI_MD_TYPE_BS_DATA ||
		    mdp->md_type == EFI_MD_TYPE_BS_CODE) {
			error = (*cb)(mdp, seqnr++, arg);
			if (error)
				return (-error);
		}
		mdp = efi_md_next(mdp);
	}
	return (seqnr);
}

/*
 * Virtual dump (aka minidump) support
 */

static int
virt_size(uint64_t *dumpsize)
{

	return (0);
}

static int
virt_dumphdrs(struct dumperinfo *di)
{

	return (-ENOSYS);
}

static int
virt_dumpdata(struct dumperinfo *di)
{

	return (-ENOSYS);
}

void
dumpsys(struct dumperinfo *di)
{
	Elf64_Ehdr ehdr;
	uint64_t dumpsize;
	off_t hdrgap;
	size_t hdrsz;
	int error, status;

	bzero(&ehdr, sizeof(ehdr));
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
#if BYTE_ORDER == LITTLE_ENDIAN
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_STANDALONE;	/* XXX big picture? */
	ehdr.e_type = ET_CORE;
	ehdr.e_machine = EM_IA_64;
	ehdr.e_entry = (minidump) ? (uintptr_t)bootinfo :
	    ia64_tpa((uintptr_t)bootinfo);
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_flags = (minidump) ? 0 : EF_IA_64_ABSOLUTE; /* XXX misuse? */
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf64_Phdr);
	ehdr.e_shentsize = sizeof(Elf64_Shdr);

	/* Calculate dump size. */
	dumpsize = 0L;
	status = (minidump) ? virt_size(&dumpsize) :
	    phys_foreach(phys_cb_size, &dumpsize);
	if (status < 0) {
		error = -status;
		goto fail;
	}
	ehdr.e_phnum = status;
	hdrsz = ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;
	fileofs = (minidump) ? round_page(hdrsz) : MD_ALIGN(hdrsz);
	dumpsize += fileofs;
	hdrgap = fileofs - DEV_ALIGN(hdrsz);

	/* Determine dump offset on device. */
	if (di->mediasize < SIZEOF_METADATA + dumpsize + sizeof(kdh) * 2) {
		error = ENOSPC;
		goto fail;
	}
	dumplo = di->mediaoffset + di->mediasize - dumpsize;
	dumplo -= sizeof(kdh) * 2;

	mkdumpheader(&kdh, KERNELDUMPMAGIC, KERNELDUMP_IA64_VERSION, dumpsize, di->blocksize);

	printf("Dumping %llu MB (%d chunks)\n", (long long)dumpsize >> 20,
	    ehdr.e_phnum);

	/* Dump leader */
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

	/* Dump ELF header */
	error = buf_write(di, (char*)&ehdr, sizeof(ehdr));
	if (error)
		goto fail;

	/* Dump program headers */
	status = (minidump) ? virt_dumphdrs(di) :
	    phys_foreach(phys_cb_dumphdr, di);
	if (status < 0) {
		error = -status;
		goto fail;
	}
	buf_flush(di);

	/*
	 * All headers are written using blocked I/O, so we know the
	 * current offset is (still) block aligned. Skip the alignment
	 * in the file to have the segment contents aligned at page
	 * boundary. For physical dumps, it's the EFI page size (= 4K).
	 * For minidumps it's the kernel's page size (= 8K).
	 */
	dumplo += hdrgap;

	/* Dump memory chunks (updates dumplo) */
	status = (minidump) ? virt_dumpdata(di) :
	    phys_foreach(phys_cb_dumpdata, di);
	if (status < 0) {
		error = -status;
		goto fail;
	}

	/* Dump trailer */
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;

	/* Signal completion, signoff and exit stage left. */
	dump_write(di, NULL, 0, 0, 0);
	printf("\nDump complete\n");
	return;

 fail:
	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
}
