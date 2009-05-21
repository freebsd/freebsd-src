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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/elf.h>
#include <machine/md_var.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

/*
 * Don't touch the first SIZEOF_METADATA bytes on the dump device. This
 * is to protect us from metadata and to protect metadata from us.
 */
#define	SIZEOF_METADATA		(64*1024)

#define	MD_ALIGN(x)	(((off_t)(x) + PAGE_MASK) & ~PAGE_MASK)
#define	DEV_ALIGN(x)	(((off_t)(x) + (DEV_BSIZE-1)) & ~(DEV_BSIZE-1))

typedef int callback_t(struct pmap_md *, int, void *);

static struct kerneldumpheader kdh;
static off_t dumplo, fileofs;

/* Handle buffered writes. */
static char buffer[DEV_BSIZE];
static size_t fragsz;

int dumpsys_minidump = 1;
SYSCTL_INT(_debug, OID_AUTO, minidump, CTLFLAG_RD, &dumpsys_minidump, 0,
    "Kernel makes compressed crash dumps");

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
			error = di->dumper(di->priv, buffer, 0, dumplo,
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

	error = di->dumper(di->priv, buffer, 0, dumplo, DEV_BSIZE);
	dumplo += DEV_BSIZE;
	fragsz = 0;
	return (error);
}

static int
cb_dumpdata(struct pmap_md *md, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	vm_offset_t va;
	size_t counter, ofs, resid, sz;
	int c, error, twiddle;

	error = 0;
	counter = 0;	/* Update twiddle every 16MB */
	twiddle = 0;

	ofs = 0;	/* Logical offset within the chunk */
	resid = md->md_size;

	printf("  chunk %d: %lu bytes ", seqnr, (u_long)resid);

	while (resid) {
		sz = (resid > DFLTPHYS) ? DFLTPHYS : resid;
		va = pmap_dumpsys_map(md, ofs, &sz);
		counter += sz;
		if (counter >> 24) {
			printf("%c\b", "|/-\\"[twiddle++ & 3]);
			counter &= (1<<24) - 1;
		}
		error = di->dumper(di->priv, (void*)va, 0, dumplo, sz);
		pmap_dumpsys_unmap(md, ofs, va);
		if (error)
			break;
		dumplo += sz;
		resid -= sz;
		ofs += sz;

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
cb_dumphdr(struct pmap_md *md, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	Elf32_Phdr phdr;
	int error;

	bzero(&phdr, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R;			/* XXX */
	phdr.p_offset = fileofs;
	phdr.p_vaddr = md->md_vaddr;
	phdr.p_paddr = md->md_paddr;
	phdr.p_filesz = md->md_size;
	phdr.p_memsz = md->md_size;
	phdr.p_align = PAGE_SIZE;

	error = buf_write(di, (char*)&phdr, sizeof(phdr));
	fileofs += phdr.p_filesz;
	return (error);
}

static int
cb_size(struct pmap_md *md, int seqnr, void *arg)
{
	uint32_t *sz = (uint32_t*)arg;

	*sz += md->md_size;
	return (0);
}

static int
foreach_chunk(callback_t cb, void *arg)
{
	struct pmap_md *md;
	int error, seqnr;

	seqnr = 0;
	md = pmap_scan_md(NULL);
	while (md != NULL) {
		error = (*cb)(md, seqnr++, arg);
		if (error)
			return (-error);
		md = pmap_scan_md(md);
	}
	return (seqnr);
}

void
dumpsys(struct dumperinfo *di)
{
	Elf32_Ehdr ehdr;
	uint32_t dumpsize;
	off_t hdrgap;
	size_t hdrsz;
	int error;

	bzero(&ehdr, sizeof(ehdr));
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#if BYTE_ORDER == LITTLE_ENDIAN
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_STANDALONE;	/* XXX big picture? */
	ehdr.e_type = ET_CORE;
	ehdr.e_machine = EM_PPC;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf32_Phdr);
	ehdr.e_shentsize = sizeof(Elf32_Shdr);

	/* Calculate dump size. */
	dumpsize = 0L;
	ehdr.e_phnum = foreach_chunk(cb_size, &dumpsize);
	hdrsz = ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;
	fileofs = MD_ALIGN(hdrsz);
	dumpsize += fileofs;
	hdrgap = fileofs - DEV_ALIGN(hdrsz);

	/* For block devices, determine the dump offset on the device. */
	if (di->mediasize > 0) {
		if (di->mediasize <
		    SIZEOF_METADATA + dumpsize + sizeof(kdh) * 2) {
			error = ENOSPC;
			goto fail;
		}
		dumplo = di->mediaoffset + di->mediasize - dumpsize;
		dumplo -= sizeof(kdh) * 2;
	} else
		dumplo = 0;

	mkdumpheader(&kdh, KERNELDUMPMAGIC, KERNELDUMP_POWERPC_VERSION, dumpsize,
	    di->blocksize);

	printf("Dumping %u MB (%d chunks)\n", dumpsize >> 20,
	    ehdr.e_phnum);

	/* Dump leader */
	error = di->dumper(di->priv, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

	/* Dump ELF header */
	error = buf_write(di, (char*)&ehdr, sizeof(ehdr));
	if (error)
		goto fail;

	/* Dump program headers */
	error = foreach_chunk(cb_dumphdr, di);
	if (error < 0)
		goto fail;
	buf_flush(di);

	/*
	 * All headers are written using blocked I/O, so we know the
	 * current offset is (still) block aligned. Skip the alignement
	 * in the file to have the segment contents aligned at page
	 * boundary. We cannot use MD_ALIGN on dumplo, because we don't
	 * care and may very well be unaligned within the dump device.
	 */
	dumplo += hdrgap;

	/* Dump memory chunks (updates dumplo) */
	error = foreach_chunk(cb_dumpdata, di);
	if (error < 0)
		goto fail;

	/* Dump trailer */
	error = di->dumper(di->priv, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;

	/* Signal completion, signoff and exit stage left. */
	di->dumper(di->priv, NULL, 0, 0, 0);
	printf("\nDump complete\n");
	return;

 fail:
	if (error < 0)
		error = -error;

	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
}
