/*-
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
#include <machine/elf.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
static void elf_corehdr(int fd, pid_t, vm_map_entry_t, int numsegs,
    void *hdr, size_t hdrsize);
static void elf_puthdr(vm_map_entry_t, void *, size_t *,
    const prstatus_t *, const prfpregset_t *, const prpsinfo_t *, int numsegs);
static void elf_putnote(void *dst, size_t *off, const char *name, int type,
    const void *desc, size_t descsz);
static void freemap(vm_map_entry_t);
static void readhdrinfo(pid_t, prstatus_t *, prfpregset_t *, prpsinfo_t *);
static vm_map_entry_t readmap(pid_t);

/*
 * Write an ELF coredump for the given pid to the given fd.
 */
void
elf_coredump(int fd, pid_t pid)
{
	vm_map_entry_t map;
	struct sseg_closure seginfo;
	void *hdr;
	size_t hdrsize;
	char memname[64];
	int memfd;
	Elf_Phdr *php;
	int i;

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
	elf_puthdr(map, (void *)NULL, &hdrsize,
	    (const prstatus_t *)NULL, (const prfpregset_t *)NULL,
	    (const prpsinfo_t *)NULL, seginfo.count);

	/*
	 * Allocate memory for building the header, fill it up,
	 * and write it out.
	 */
	hdr = malloc(hdrsize);
	if ((hdr = malloc(hdrsize)) == NULL)
		errx(1, "out of memory");
	elf_corehdr(fd, pid, map, seginfo.count, hdr, hdrsize);

	/* Write the contents of all of the writable segments. */
	snprintf(memname, sizeof memname, "/proc/%d/mem", pid);
	if ((memfd = open(memname, O_RDONLY)) == -1)
		err(1, "cannot open %s", memname);

	php = (Elf_Phdr *)((char *)hdr + sizeof(Elf_Ehdr)) + 1;
	for (i = 0;  i < seginfo.count;  i++) {
		uintmax_t nleft = php->p_filesz;

		lseek(memfd, (off_t)php->p_vaddr, SEEK_SET);
		while (nleft > 0) {
			char buf[8*1024];
			size_t nwant;
			ssize_t ngot;

			if (nleft > sizeof(buf))
				nwant = sizeof buf;
			else
				nwant = nleft;
			ngot = read(memfd, buf, nwant);
			if (ngot == -1)
				err(1, "read from %s", memname);
			if ((size_t)ngot < nwant)
				errx(1, "short read from %s:"
				    " wanted %d, got %d", memname,
				    nwant, ngot);
			ngot = write(fd, buf, nwant);
			if (ngot == -1)
				err(1, "write of segment %d failed", i);
			if ((size_t)ngot != nwant)
				errx(1, "short write");
			nleft -= nwant;
		}
		php++;
	}
	close(memfd);
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

/*
 * Write the core file header to the file, including padding up to
 * the page boundary.
 */
static void
elf_corehdr(int fd, pid_t pid, vm_map_entry_t map, int numsegs, void *hdr,
    size_t hdrsize)
{
	size_t off;
	prstatus_t status;
	prfpregset_t fpregset;
	prpsinfo_t psinfo;

	/* Gather the information for the header. */
	readhdrinfo(pid, &status, &fpregset, &psinfo);

	/* Fill in the header. */
	memset(hdr, 0, hdrsize);
	off = 0;
	elf_puthdr(map, hdr, &off, &status, &fpregset, &psinfo, numsegs);

	/* Write it to the core file. */
	if (write(fd, hdr, hdrsize) == -1)
		err(1, "write");
}

/*
 * Generate the ELF coredump header into the buffer at "dst".  "dst" may
 * be NULL, in which case the header is sized but not actually generated.
 */
static void
elf_puthdr(vm_map_entry_t map, void *dst, size_t *off, const prstatus_t *status,
    const prfpregset_t *fpregset, const prpsinfo_t *psinfo, int numsegs)
{
	size_t ehoff;
	size_t phoff;
	size_t noteoff;
	size_t notesz;

	ehoff = *off;
	*off += sizeof(Elf_Ehdr);

	phoff = *off;
	*off += (numsegs + 1) * sizeof(Elf_Phdr);

	noteoff = *off;
	elf_putnote(dst, off, "FreeBSD", NT_PRSTATUS, status,
	    sizeof *status);
	elf_putnote(dst, off, "FreeBSD", NT_FPREGSET, fpregset,
	    sizeof *fpregset);
	elf_putnote(dst, off, "FreeBSD", NT_PRPSINFO, psinfo,
	    sizeof *psinfo);
	notesz = *off - noteoff;

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
 * Read the process information necessary to fill in the core file's header.
 */
static void
readhdrinfo(pid_t pid, prstatus_t *status, prfpregset_t *fpregset,
    prpsinfo_t *psinfo)
{
	char name[64];
	char line[256];
	int fd;
	int i;
	int n;

	memset(status, 0, sizeof *status);
	status->pr_version = PRSTATUS_VERSION;
	status->pr_statussz = sizeof(prstatus_t);
	status->pr_gregsetsz = sizeof(gregset_t);
	status->pr_fpregsetsz = sizeof(fpregset_t);
	status->pr_osreldate = __FreeBSD_version;
	status->pr_pid = pid;

	memset(fpregset, 0, sizeof *fpregset);

	memset(psinfo, 0, sizeof *psinfo);
	psinfo->pr_version = PRPSINFO_VERSION;
	psinfo->pr_psinfosz = sizeof(prpsinfo_t);

	/* Read the general registers. */
	snprintf(name, sizeof name, "/proc/%d/regs", pid);
	if ((fd = open(name, O_RDONLY)) == -1)
		err(1, "cannot open %s", name);
	if ((n = read(fd, &status->pr_reg, sizeof status->pr_reg)) == -1)
		err(1, "read error from %s", name);
	if ((size_t)n < sizeof(status->pr_reg))
		errx(1, "short read from %s: wanted %u, got %d", name,
		    sizeof status->pr_reg, n);
	close(fd);

	/* Read the floating point registers. */
	snprintf(name, sizeof name, "/proc/%d/fpregs", pid);
	if ((fd = open(name, O_RDONLY)) == -1)
		err(1, "cannot open %s", name);
	if ((n = read(fd, fpregset, sizeof *fpregset)) == -1)
		err(1, "read error from %s", name);
	if ((size_t)n < sizeof(*fpregset))
		errx(1, "short read from %s: wanted %u, got %d", name,
		    sizeof *fpregset, n);
	close(fd);

	/* Read and parse the process status. */
	snprintf(name, sizeof name, "/proc/%d/status", pid);
	if ((fd = open(name, O_RDONLY)) == -1)
		err(1, "cannot open %s", name);
	if ((n = read(fd, line, sizeof line - 1)) == -1)
		err(1, "read error from %s", name);
	if (n > MAXCOMLEN)
		n = MAXCOMLEN;
	for (i = 0;  i < n && line[i] != ' ';  i++)
		psinfo->pr_fname[i] = line[i];
	strncpy(psinfo->pr_psargs, psinfo->pr_fname, PRARGSZ);
	close(fd);
}

/*
 * Read the process's memory map using procfs, and return a list of
 * VM map entries.  Only the non-device read/writable segments are
 * returned.  The map entries in the list aren't fully filled in; only
 * the items we need are present.
 */
static vm_map_entry_t
readmap(pid_t pid)
{
	char mapname[64];
	int mapfd;
	ssize_t mapsize;
	size_t bufsize;
	char *mapbuf;
	int pos;
	vm_map_entry_t map;
	vm_map_entry_t *linkp;

	snprintf(mapname, sizeof mapname, "/proc/%d/map", pid);
	if ((mapfd = open(mapname, O_RDONLY)) == -1)
		err(1, "cannot open %s", mapname);

	/*
	 * Procfs requires (for consistency) that the entire memory map
	 * be read with a single read() call.  Start with a reasonably sized
	 * buffer, and double it until it is big enough.
	 */
	bufsize = 8 * 1024;
	mapbuf = NULL;
	for ( ; ; ) {
		if ((mapbuf = realloc(mapbuf, bufsize + 1)) == NULL)
			errx(1, "out of memory");
		mapsize = read(mapfd, mapbuf, bufsize);
		if (mapsize != -1 || errno != EFBIG)
			break;
		bufsize *= 2;
		/* This lseek shouldn't be necessary, but it is. */
		lseek(mapfd, (off_t)0, SEEK_SET);
	}
	if (mapsize == -1)
		err(1, "read error from %s", mapname);
	if (mapsize == 0)
		errx(1, "empty map file %s", mapname);
	mapbuf[mapsize] = 0;
	close(mapfd);

	pos = 0;
	map = NULL;
	linkp = &map;
	while (pos < mapsize) {
		vm_map_entry_t ent;
		vm_offset_t start;
		vm_offset_t end;
		char prot[4];
		char type[16];
		int n;
		int len;

		len = 0;
		n = sscanf(mapbuf + pos, "%x %x %*d %*d %*x %3[-rwx]"
		    " %*d %*d %*x %*s %*s %16s%*[\n]%n",
		    &start, &end, prot, type, &len);
		if (n != 4)
			errx(1, "ill-formed line in %s", mapname);
		pos += len;

		/* Ignore segments of the wrong kind, and unwritable ones */
		if (strncmp(prot, "rw", 2) != 0 ||
		    (strcmp(type, "default") != 0 &&
		    strcmp(type, "vnode") != 0 &&
		    strcmp(type, "swap") != 0))
			continue;

		if ((ent = (vm_map_entry_t)calloc(1, sizeof *ent)) == NULL)
			errx(1, "out of memory");
		ent->start = start;
		ent->end = end;
		ent->protection = VM_PROT_READ | VM_PROT_WRITE;
		if (prot[2] == 'x')
		    ent->protection |= VM_PROT_EXECUTE;

		*linkp = ent;
		linkp = &ent->next;
	}
	free(mapbuf);
	return map;
}
