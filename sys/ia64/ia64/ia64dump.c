/*
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <machine/elf.h>
#include <machine/md_var.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

#define	MD_ALIGN(x)	(((off_t)(x) + EFI_PAGE_MASK) & ~EFI_PAGE_MASK)

typedef int callback_t(EFI_MEMORY_DESCRIPTOR*, int, void*);

static struct kerneldumpheader kdh;
static off_t dumplo, fileofs;

/* Handle buffered writes. */
static char buffer[DEV_BSIZE];
static size_t fragsz;

/* XXX should be MI */
static void
mkdumpheader(struct kerneldumpheader *kdh, uint32_t archver, uint64_t dumplen,
    uint32_t blksz)
{

	bzero(kdh, sizeof(*kdh));
	strncpy(kdh->magic, KERNELDUMPMAGIC, sizeof(kdh->magic));
	strncpy(kdh->architecture, MACHINE_ARCH, sizeof(kdh->architecture));
	kdh->version = htod32(KERNELDUMPVERSION);
	kdh->architectureversion = htod32(archver);
	kdh->dumplength = htod64(dumplen);
	kdh->dumptime = htod64(time_second);
	kdh->blocksize = htod32(blksz);
	strncpy(kdh->hostname, hostname, sizeof(kdh->hostname));
	strncpy(kdh->versionstring, version, sizeof(kdh->versionstring));
	if (panicstr != NULL)
		strncpy(kdh->panicstring, panicstr, sizeof(kdh->panicstring));
	kdh->parity = kerneldump_parity(kdh);
}

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
			error = di->dumper(di->priv, buffer, NULL, dumplo,
			    DEV_BSIZE);
			if (error)
				return error;
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

	error = di->dumper(di->priv, buffer, NULL, dumplo, DEV_BSIZE);
	dumplo += DEV_BSIZE;
	return (error);
}

static int
cb_dumpdata(EFI_MEMORY_DESCRIPTOR *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	vm_offset_t pa;
	uint64_t pgs;
	size_t sz;
	int error, twiddle;

	error = 0;	/* catch case in which mdp->NumberOfPages is 0 */
	twiddle = 0;
	pgs = mdp->NumberOfPages;
	pa = IA64_PHYS_TO_RR7(mdp->PhysicalStart);

	printf("  region %d: %ld pages ", seqnr, (long)pgs);

	while (pgs) {
		sz = (pgs > (DFLTPHYS >> EFI_PAGE_SHIFT))
		    ? DFLTPHYS : pgs << EFI_PAGE_SHIFT;
		printf("%c\b", "|/-\\"[twiddle++ & 3]);
		error = di->dumper(di->priv, (void*)pa, NULL, dumplo, sz);
		if (error)
			break;
		dumplo += sz;
		pgs -= sz >> EFI_PAGE_SHIFT;
		pa += sz;
	}
	printf("... %s\n", (error) ? "fail" : "ok");
	return (error);
}

static int
cb_dumphdr(EFI_MEMORY_DESCRIPTOR *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	Elf64_Phdr phdr;
	int error;

	bzero(&phdr, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R;			/* XXX */
	phdr.p_offset = fileofs;
	phdr.p_vaddr = mdp->VirtualStart;	/* XXX probably bogus. */
	phdr.p_paddr = mdp->PhysicalStart;
	phdr.p_filesz = mdp->NumberOfPages << EFI_PAGE_SHIFT;
	phdr.p_memsz = mdp->NumberOfPages << EFI_PAGE_SHIFT;
	phdr.p_align = EFI_PAGE_SIZE;

	error = buf_write(di->priv, (char*)&phdr, sizeof(phdr));
	fileofs += phdr.p_filesz;
	return (error);
}

static int
cb_size(EFI_MEMORY_DESCRIPTOR *mdp, int seqnr, void *arg)
{
	uint64_t *sz = (uint64_t*)arg;

	*sz += (uint64_t)mdp->NumberOfPages << EFI_PAGE_SHIFT;
	return (0);
}

static int
foreach_region(callback_t cb, void *arg)
{
	EFI_MEMORY_DESCRIPTOR *mdp;
	int error, i, mdcount, seqnr;

	mdp = (EFI_MEMORY_DESCRIPTOR *)IA64_PHYS_TO_RR7(bootinfo.bi_memmap);
	mdcount = bootinfo.bi_memmap_size / bootinfo.bi_memdesc_size;

	if (mdp == NULL || mdcount == 0)
		return (0);

	for (i = 0, seqnr = 0; i < mdcount; i++) {
		if (mdp->Type == EfiConventionalMemory) {
			error = (*cb)(mdp, seqnr++, arg);
			if (error)
				return (-error);
		}
		mdp = NextMemoryDescriptor(mdp, bootinfo.bi_memdesc_size);
	}

	return (seqnr);
}

void
dumpsys(struct dumperinfo *di)
{
	Elf64_Ehdr ehdr;
	uint64_t dumpsize;
	size_t hdrsz;
	int error;

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
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_flags = EF_IA_64_ABSOLUTE;		/* XXX misuse? */
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf64_Phdr);
	ehdr.e_shentsize = sizeof(Elf64_Shdr);

	/* Calculate dump size. */
	dumpsize = 0L;
	ehdr.e_phnum = foreach_region(cb_size, &dumpsize);
	hdrsz = ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;
	fileofs = MD_ALIGN(hdrsz);
	dumpsize += fileofs;

	/* Determine dump offset on device. */
	dumplo = di->mediaoffset + di->mediasize - dumpsize;
	dumplo -= sizeof(kdh) * 2;

	mkdumpheader(&kdh, KERNELDUMP_IA64_VERSION, dumpsize, di->blocksize);

	printf("Dumping %llu MB (%d regions)\n", dumpsize >> 20, ehdr.e_phnum);

	/* Dump leader */
	error = di->dumper(di->priv, &kdh, NULL, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

	/* Dump ELF header */
	error = buf_write(di, (char*)&ehdr, sizeof(ehdr));
	if (error)
		goto fail;

	/* Dump program headers */
	error = foreach_region(cb_dumphdr, di);
	if (error < 0)
		goto fail;
	buf_flush(di);
	dumplo += MD_ALIGN(hdrsz);

	/* Dump region data (updates dumplo) */
	error = foreach_region(cb_dumpdata, di);
	if (error < 0)
		goto fail;

	/* Dump trailer */
	error = di->dumper(di->priv, &kdh, NULL, dumplo, sizeof(kdh));
	if (error)
		goto fail;

	/* Signal completion, signoff and exit stage left. */
	di->dumper(di->priv, NULL, NULL, 0, 0);
	printf("\nDump complete\n");
	return;

 fail:
	if (error < 0)
		error = -error;
	/* XXX It should look more like VMS :-) */
	printf("** DUMP FAILED (ERROR %d) **\n", error);
}
