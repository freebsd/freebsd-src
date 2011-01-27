/*-
 * Copyright (c) 2007 Sandvine Incorporated
 * Copyright (c) 1998 John D. Polstra
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

#include <sys/param.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/queue.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <machine/elf.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libutil.h>

#include "extern.h"

/*
 * Code for generating ELF core dumps.
 */

typedef void (*segment_callback)(vm_map_entry_t, void *);

/* Closure for cb_put_phdr(). */
struct phdr_closure {
	Elf_Phdr *phdr;		/* Program header to fill in */
	Elf_Off offset;		/* Offset of segment in core file */
};

/* Closure for cb_size_segment(). */
struct sseg_closure {
	int count;		/* Count of writable segments. */
	size_t size;		/* Total size of all writable segments. */
};

static void cb_put_phdr(vm_map_entry_t, void *);
static void cb_size_segment(vm_map_entry_t, void *);
static void each_writable_segment(vm_map_entry_t, segment_callback,
    void *closure);
static void elf_detach(void);	/* atexit() handler. */
static void elf_puthdr(pid_t, vm_map_entry_t, void *, size_t *, int numsegs);
static void elf_putnote(void *dst, size_t *off, const char *name, int type,
    const void *desc, size_t descsz);
static void freemap(vm_map_entry_t);
static vm_map_entry_t readmap(pid_t);

static pid_t g_pid;		/* Pid being dumped, global for elf_detach */

static int
elf_ident(int efd, pid_t pid __unused, char *binfile __unused)
{
	Elf_Ehdr hdr;
	int cnt;

	cnt = read(efd, &hdr, sizeof(hdr));
	if (cnt != sizeof(hdr))
		return (0);
	if (IS_ELF(hdr))
		return (1);
	return (0);
}

static void
elf_detach(void)
{

	if (g_pid != 0)
		ptrace(PT_DETACH, g_pid, (caddr_t)1, 0);
}

/*
 * Write an ELF coredump for the given pid to the given fd.
 */
static void
elf_coredump(int efd __unused, int fd, pid_t pid)
{
	vm_map_entry_t map;
	struct sseg_closure seginfo;
	void *hdr;
	size_t hdrsize;
	Elf_Phdr *php;
	int i;

	/* Attach to process to dump. */
	g_pid = pid;
	if (atexit(elf_detach) != 0)
		err(1, "atexit");
	errno = 0;
	ptrace(PT_ATTACH, pid, NULL, 0);
	if (errno)
		err(1, "PT_ATTACH");
	if (waitpid(pid, NULL, 0) == -1)
		err(1, "waitpid");

	/* Get the program's memory map. */
	map = readmap(pid);

	/* Size the program segments. */
	seginfo.count = 0;
	seginfo.size = 0;
	each_writable_segment(map, cb_size_segment, &seginfo);

	/*
	 * Calculate the size of the core file header area by making
	 * a dry run of generating it.  Nothing is written, but the
	 * size is calculated.
	 */
	hdrsize = 0;
	elf_puthdr(pid, map, NULL, &hdrsize, seginfo.count);

	/*
	 * Allocate memory for building the header, fill it up,
	 * and write it out.
	 */
	if ((hdr = calloc(1, hdrsize)) == NULL)
		errx(1, "out of memory");

	/* Fill in the header. */
	hdrsize = 0;
	elf_puthdr(pid, map, hdr, &hdrsize, seginfo.count);

	/* Write it to the core file. */
	if (write(fd, hdr, hdrsize) == -1)
		err(1, "write");

	/* Write the contents of all of the writable segments. */
	php = (Elf_Phdr *)((char *)hdr + sizeof(Elf_Ehdr)) + 1;
	for (i = 0;  i < seginfo.count;  i++) {
		struct ptrace_io_desc iorequest;
		uintmax_t nleft = php->p_filesz;

		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (caddr_t)php->p_vaddr;
		while (nleft > 0) {
			char buf[8*1024];
			size_t nwant;
			ssize_t ngot;

			if (nleft > sizeof(buf))
				nwant = sizeof buf;
			else
				nwant = nleft;
			iorequest.piod_addr = buf;
			iorequest.piod_len = nwant;
			ptrace(PT_IO, pid, (caddr_t)&iorequest, 0);
			ngot = iorequest.piod_len;
			if ((size_t)ngot < nwant)
				errx(1, "short read wanted %d, got %d",
				    nwant, ngot);
			ngot = write(fd, buf, nwant);
			if (ngot == -1)
				err(1, "write of segment %d failed", i);
			if ((size_t)ngot != nwant)
				errx(1, "short write");
			nleft -= nwant;
			iorequest.piod_offs += ngot;
		}
		php++;
	}
	free(hdr);
	freemap(map);
}

/*
 * A callback for each_writable_segment() to write out the segment's
 * program header entry.
 */
static void
cb_put_phdr(vm_map_entry_t entry, void *closure)
{
	struct phdr_closure *phc = (struct phdr_closure *)closure;
	Elf_Phdr *phdr = phc->phdr;

	phc->offset = round_page(phc->offset);

	phdr->p_type = PT_LOAD;
	phdr->p_offset = phc->offset;
	phdr->p_vaddr = entry->start;
	phdr->p_paddr = 0;
	phdr->p_filesz = phdr->p_memsz = entry->end - entry->start;
	phdr->p_align = PAGE_SIZE;
	phdr->p_flags = 0;
	if (entry->protection & VM_PROT_READ)
		phdr->p_flags |= PF_R;
	if (entry->protection & VM_PROT_WRITE)
		phdr->p_flags |= PF_W;
	if (entry->protection & VM_PROT_EXECUTE)
		phdr->p_flags |= PF_X;

	phc->offset += phdr->p_filesz;
	phc->phdr++;
}

/*
 * A callback for each_writable_segment() to gather information about
 * the number of segments and their total size.
 */
static void
cb_size_segment(vm_map_entry_t entry, void *closure)
{
	struct sseg_closure *ssc = (struct sseg_closure *)closure;

	ssc->count++;
	ssc->size += entry->end - entry->start;
}

/*
 * For each segment in the given memory map, call the given function
 * with a pointer to the map entry and some arbitrary caller-supplied
 * data.
 */
static void
each_writable_segment(vm_map_entry_t map, segment_callback func, void *closure)
{
	vm_map_entry_t entry;

	for (entry = map;  entry != NULL;  entry = entry->next)
		(*func)(entry, closure);
}

static void
elf_getstatus(pid_t pid, prpsinfo_t *psinfo)
{
	struct kinfo_proc kobj;
	int name[4];
	size_t len;

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_PID;
	name[3] = pid;

	len = sizeof(kobj);
	if (sysctl(name, 4, &kobj, &len, NULL, 0) == -1)
		err(1, "error accessing kern.proc.pid.%u sysctl", pid);
	if (kobj.ki_pid != pid)
		err(1, "error accessing kern.proc.pid.%u sysctl datas", pid);
	strncpy(psinfo->pr_fname, kobj.ki_comm, MAXCOMLEN);
	strncpy(psinfo->pr_psargs, psinfo->pr_fname, PRARGSZ);
}

/*
 * Generate the ELF coredump header into the buffer at "dst".  "dst" may
 * be NULL, in which case the header is sized but not actually generated.
 */
static void
elf_puthdr(pid_t pid, vm_map_entry_t map, void *dst, size_t *off, int numsegs)
{
	struct {
		prstatus_t status;
		prfpregset_t fpregset;
		prpsinfo_t psinfo;
	} *tempdata;
	size_t ehoff;
	size_t phoff;
	size_t noteoff;
	size_t notesz;
	size_t threads;
	lwpid_t *tids;
	int i;

	prstatus_t *status;
	prfpregset_t *fpregset;
	prpsinfo_t *psinfo;

	ehoff = *off;
	*off += sizeof(Elf_Ehdr);

	phoff = *off;
	*off += (numsegs + 1) * sizeof(Elf_Phdr);

	noteoff = *off;

	if (dst != NULL) {
		if ((tempdata = calloc(1, sizeof(*tempdata))) == NULL)
			errx(1, "out of memory");
		status = &tempdata->status;
		fpregset = &tempdata->fpregset;
		psinfo = &tempdata->psinfo;
	} else {
		tempdata = NULL;
		status = NULL;
		fpregset = NULL;
		psinfo = NULL;
	}

	errno = 0;
	threads = ptrace(PT_GETNUMLWPS, pid, NULL, 0);
	if (errno)
		err(1, "PT_GETNUMLWPS");

	if (dst != NULL) {
		psinfo->pr_version = PRPSINFO_VERSION;
		psinfo->pr_psinfosz = sizeof(prpsinfo_t);
		elf_getstatus(pid, psinfo);

	}
	elf_putnote(dst, off, "FreeBSD", NT_PRPSINFO, psinfo,
	    sizeof *psinfo);

	if (dst != NULL) {
		tids = malloc(threads * sizeof(*tids));
		if (tids == NULL)
			errx(1, "out of memory");
		errno = 0;
		ptrace(PT_GETLWPLIST, pid, (void *)tids, threads);
		if (errno)
			err(1, "PT_GETLWPLIST");
	}
	for (i = 0; i < threads; ++i) {
		if (dst != NULL) {
			status->pr_version = PRSTATUS_VERSION;
			status->pr_statussz = sizeof(prstatus_t);
			status->pr_gregsetsz = sizeof(gregset_t);
			status->pr_fpregsetsz = sizeof(fpregset_t);
			status->pr_osreldate = __FreeBSD_version;
			status->pr_pid = tids[i];

			ptrace(PT_GETREGS, tids[i], (void *)&status->pr_reg, 0);
			ptrace(PT_GETFPREGS, tids[i], (void *)fpregset, 0);
		}
		elf_putnote(dst, off, "FreeBSD", NT_PRSTATUS, status,
		    sizeof *status);
		elf_putnote(dst, off, "FreeBSD", NT_FPREGSET, fpregset,
		    sizeof *fpregset);
	}

	notesz = *off - noteoff;

	if (dst != NULL) {
		free(tids);
		free(tempdata);
	}

	/* Align up to a page boundary for the program segments. */
	*off = round_page(*off);

	if (dst != NULL) {
		Elf_Ehdr *ehdr;
		Elf_Phdr *phdr;
		struct phdr_closure phc;

		/*
		 * Fill in the ELF header.
		 */
		ehdr = (Elf_Ehdr *)((char *)dst + ehoff);
		ehdr->e_ident[EI_MAG0] = ELFMAG0;
		ehdr->e_ident[EI_MAG1] = ELFMAG1;
		ehdr->e_ident[EI_MAG2] = ELFMAG2;
		ehdr->e_ident[EI_MAG3] = ELFMAG3;
		ehdr->e_ident[EI_CLASS] = ELF_CLASS;
		ehdr->e_ident[EI_DATA] = ELF_DATA;
		ehdr->e_ident[EI_VERSION] = EV_CURRENT;
		ehdr->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
		ehdr->e_ident[EI_ABIVERSION] = 0;
		ehdr->e_ident[EI_PAD] = 0;
		ehdr->e_type = ET_CORE;
		ehdr->e_machine = ELF_ARCH;
		ehdr->e_version = EV_CURRENT;
		ehdr->e_entry = 0;
		ehdr->e_phoff = phoff;
		ehdr->e_flags = 0;
		ehdr->e_ehsize = sizeof(Elf_Ehdr);
		ehdr->e_phentsize = sizeof(Elf_Phdr);
		ehdr->e_phnum = numsegs + 1;
		ehdr->e_shentsize = sizeof(Elf_Shdr);
		ehdr->e_shnum = 0;
		ehdr->e_shstrndx = SHN_UNDEF;

		/*
		 * Fill in the program header entries.
		 */
		phdr = (Elf_Phdr *)((char *)dst + phoff);

		/* The note segment. */
		phdr->p_type = PT_NOTE;
		phdr->p_offset = noteoff;
		phdr->p_vaddr = 0;
		phdr->p_paddr = 0;
		phdr->p_filesz = notesz;
		phdr->p_memsz = 0;
		phdr->p_flags = 0;
		phdr->p_align = 0;
		phdr++;

		/* All the writable segments from the program. */
		phc.phdr = phdr;
		phc.offset = *off;
		each_writable_segment(map, cb_put_phdr, &phc);
	}
}

/*
 * Emit one note section to "dst", or just size it if "dst" is NULL.
 */
static void
elf_putnote(void *dst, size_t *off, const char *name, int type,
    const void *desc, size_t descsz)
{
	Elf_Note note;

	note.n_namesz = strlen(name) + 1;
	note.n_descsz = descsz;
	note.n_type = type;
	if (dst != NULL)
		bcopy(&note, (char *)dst + *off, sizeof note);
	*off += sizeof note;
	if (dst != NULL)
		bcopy(name, (char *)dst + *off, note.n_namesz);
	*off += roundup2(note.n_namesz, sizeof(Elf_Size));
	if (dst != NULL)
		bcopy(desc, (char *)dst + *off, note.n_descsz);
	*off += roundup2(note.n_descsz, sizeof(Elf_Size));
}

/*
 * Free the memory map.
 */
static void
freemap(vm_map_entry_t map)
{

	while (map != NULL) {
		vm_map_entry_t next = map->next;
		free(map);
		map = next;
	}
}

/*
 * Read the process's memory map using kinfo_getvmmap(), and return a list of
 * VM map entries.  Only the non-device read/writable segments are
 * returned.  The map entries in the list aren't fully filled in; only
 * the items we need are present.
 */
static vm_map_entry_t
readmap(pid_t pid)
{
	vm_map_entry_t ent, *linkp, map;
	struct kinfo_vmentry *vmentl, *kve;
	int i, nitems;

	vmentl = kinfo_getvmmap(pid, &nitems);
	if (vmentl == NULL)
		err(1, "cannot retrieve mappings for %u process", pid);

	map = NULL;
	linkp = &map;
	for (i = 0; i < nitems; i++) {
		kve = &vmentl[i];

		/*
		 * Ignore 'malformed' segments or ones representing memory
		 * mapping with MAP_NOCORE on.
		 * If the 'full' support is disabled, just dump the most
		 * meaningful data segments.
		 */
		if ((kve->kve_protection & KVME_PROT_READ) == 0 ||
		    (kve->kve_flags & KVME_FLAG_NOCOREDUMP) != 0 ||
		    kve->kve_type == KVME_TYPE_DEAD ||
		    kve->kve_type == KVME_TYPE_UNKNOWN ||
		    ((pflags & PFLAGS_FULL) == 0 &&
		    kve->kve_type != KVME_TYPE_DEFAULT &&
		    kve->kve_type != KVME_TYPE_VNODE &&
		    kve->kve_type != KVME_TYPE_SWAP))
			continue;

		ent = calloc(1, sizeof(*ent));
		if (ent == NULL)
			errx(1, "out of memory");
		ent->start = (vm_offset_t)kve->kve_start;
		ent->end = (vm_offset_t)kve->kve_end;
		ent->protection = VM_PROT_READ | VM_PROT_WRITE;
		if ((kve->kve_protection & KVME_PROT_EXEC) != 0)
			ent->protection |= VM_PROT_EXECUTE;

		*linkp = ent;
		linkp = &ent->next;
	}
	free(vmentl);
	return (map);
}

struct dumpers elfdump = { elf_ident, elf_coredump };
TEXT_SET(dumpset, elfdump);
