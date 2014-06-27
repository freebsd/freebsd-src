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
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kerneldump.h>
#ifdef SW_WATCHDOG
#include <sys/watchdog.h>
#endif
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/armreg.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

int do_minidump = 1;
SYSCTL_INT(_debug, OID_AUTO, minidump, CTLFLAG_RWTUN, &do_minidump, 0,
    "Enable mini crash dumps");

/*
 * Don't touch the first SIZEOF_METADATA bytes on the dump device. This
 * is to protect us from metadata and to protect metadata from us.
 */
#define	SIZEOF_METADATA		(64*1024)

#define	MD_ALIGN(x)	(((off_t)(x) + PAGE_MASK) & ~PAGE_MASK)
#define	DEV_ALIGN(x)	(((off_t)(x) + (DEV_BSIZE-1)) & ~(DEV_BSIZE-1))
extern struct pcb dumppcb;

struct md_pa {
	vm_paddr_t md_start;
	vm_paddr_t md_size;
};

typedef int callback_t(struct md_pa *, int, void *);

static struct kerneldumpheader kdh;
static off_t dumplo, fileofs;

/* Handle buffered writes. */
static char buffer[DEV_BSIZE];
static size_t fragsz;

/* XXX: I suppose 20 should be enough. */
static struct md_pa dump_map[20];

static void
md_pa_init(void)
{
	int n, idx;

	bzero(dump_map, sizeof(dump_map));
	for (n = 0; n < sizeof(dump_map) / sizeof(dump_map[0]); n++) {
		idx = n * 2;
		if (dump_avail[idx] == 0 && dump_avail[idx + 1] == 0)
			break;
		dump_map[n].md_start = dump_avail[idx];
		dump_map[n].md_size = dump_avail[idx + 1] - dump_avail[idx];
	}
}

static struct md_pa *
md_pa_first(void)
{

	return (&dump_map[0]);
}

static struct md_pa *
md_pa_next(struct md_pa *mdp)
{

	mdp++;
	if (mdp->md_size == 0)
		mdp = NULL;
	return (mdp);
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
			error = dump_write(di, buffer, 0, dumplo,
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

	error = dump_write(di, buffer, 0, dumplo, DEV_BSIZE);
	dumplo += DEV_BSIZE;
	fragsz = 0;
	return (error);
}

extern vm_offset_t kernel_l1kva;
extern char *pouet2;

static int
cb_dumpdata(struct md_pa *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	vm_paddr_t pa;
	uint32_t pgs;
	size_t counter, sz, chunk;
	int c, error;

	error = 0;	/* catch case in which chunk size is 0 */
	counter = 0;
	pgs = mdp->md_size / PAGE_SIZE;
	pa = mdp->md_start;

	printf("  chunk %d: %dMB (%d pages)", seqnr, pgs * PAGE_SIZE / (
	    1024*1024), pgs);

	/*
	 * Make sure we write coherent data.  Note that in the SMP case this
	 * only operates on the L1 cache of the current CPU, but all other CPUs
	 * have already been stopped, and their flush/invalidate was done as
	 * part of stopping.
	 */
	cpu_idcache_wbinv_all();
	cpu_l2cache_wbinv_all();
#ifdef __XSCALE__
	xscale_cache_clean_minidata();
#endif
	while (pgs) {
		chunk = pgs;
		if (chunk > MAXDUMPPGS)
			chunk = MAXDUMPPGS;
		sz = chunk << PAGE_SHIFT;
		counter += sz;
		if (counter >> 24) {
			printf(" %d", pgs * PAGE_SIZE);
			counter &= (1<<24) - 1;
		}
		if (pa == (pa & L1_ADDR_BITS)) {
			pmap_kenter_section(0, pa & L1_ADDR_BITS, 0);
			cpu_tlb_flushID_SE(0);
			cpu_cpwait();
		}
#ifdef SW_WATCHDOG
		wdog_kern_pat(WD_LASTVAL);
#endif
		error = dump_write(di,
		    (void *)(pa - (pa & L1_ADDR_BITS)),0, dumplo, sz);
		if (error)
			break;
		dumplo += sz;
		pgs -= chunk;
		pa += sz;

		/* Check for user abort. */
		c = cncheckc();
		if (c == 0x03)
			return (ECANCELED);
		if (c != -1)
			printf(" (CTRL-C to abort) ");
	}
	printf(" ... %s\n", (error) ? "fail" : "ok");
	return (error);
}

static int
cb_dumphdr(struct md_pa *mdp, int seqnr, void *arg)
{
	struct dumperinfo *di = (struct dumperinfo*)arg;
	Elf_Phdr phdr;
	uint64_t size;
	int error;

	size = mdp->md_size;
	bzero(&phdr, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R;			/* XXX */
	phdr.p_offset = fileofs;
	phdr.p_vaddr = mdp->md_start;
	phdr.p_paddr = mdp->md_start;
	phdr.p_filesz = size;
	phdr.p_memsz = size;
	phdr.p_align = PAGE_SIZE;

	error = buf_write(di, (char*)&phdr, sizeof(phdr));
	fileofs += phdr.p_filesz;
	return (error);
}

static int
cb_size(struct md_pa *mdp, int seqnr, void *arg)
{
	uint32_t *sz = (uint32_t*)arg;

	*sz += (uint32_t)mdp->md_size;
	return (0);
}

static int
foreach_chunk(callback_t cb, void *arg)
{
	struct md_pa *mdp;
	int error, seqnr;

	seqnr = 0;
	mdp = md_pa_first();
	while (mdp != NULL) {
		error = (*cb)(mdp, seqnr++, arg);
		if (error)
			return (-error);
		mdp = md_pa_next(mdp);
	}
	return (seqnr);
}

void
dumpsys(struct dumperinfo *di)
{
	Elf_Ehdr ehdr;
	uint32_t dumpsize;
	off_t hdrgap;
	size_t hdrsz;
	int error;

	if (do_minidump) {
		minidumpsys(di);
		return;
	}

	bzero(&ehdr, sizeof(ehdr));
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELF_CLASS;
#if BYTE_ORDER == LITTLE_ENDIAN
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_STANDALONE;	/* XXX big picture? */
	ehdr.e_type = ET_CORE;
	ehdr.e_machine = EM_ARM;
	ehdr.e_phoff = sizeof(ehdr);
	ehdr.e_flags = 0;
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf_Phdr);
	ehdr.e_shentsize = sizeof(Elf_Shdr);

	md_pa_init();

	/* Calculate dump size. */
	dumpsize = 0L;
	ehdr.e_phnum = foreach_chunk(cb_size, &dumpsize);
	hdrsz = ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;
	fileofs = MD_ALIGN(hdrsz);
	dumpsize += fileofs;
	hdrgap = fileofs - DEV_ALIGN(hdrsz);

	/* Determine dump offset on device. */
	if (di->mediasize < SIZEOF_METADATA + dumpsize + sizeof(kdh) * 2) {
		error = ENOSPC;
		goto fail;
	}
	dumplo = di->mediaoffset + di->mediasize - dumpsize;
	dumplo -= sizeof(kdh) * 2;

	mkdumpheader(&kdh, KERNELDUMPMAGIC, KERNELDUMP_ARM_VERSION, dumpsize, di->blocksize);

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
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;

	/* Signal completion, signoff and exit stage left. */
	dump_write(di, NULL, 0, 0, 0);
	printf("\nDump complete\n");
	return;

 fail:
	if (error < 0)
		error = -error;

	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else if (error == ENOSPC)
		printf("\nDump failed. Partition too small.\n");
	else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
}
