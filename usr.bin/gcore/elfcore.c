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
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <machine/elf.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <assert.h>
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

typedef void* (*notefunc_t)(void *, size_t *);

static void cb_put_phdr(vm_map_entry_t, void *);
static void cb_size_segment(vm_map_entry_t, void *);
static void each_writable_segment(vm_map_entry_t, segment_callback,
    void *closure);
static void elf_detach(void);	/* atexit() handler. */
static void *elf_note_fpregset(void *, size_t *);
static void *elf_note_prpsinfo(void *, size_t *);
static void *elf_note_prstatus(void *, size_t *);
static void *elf_note_thrmisc(void *, size_t *);
static void *elf_note_procstat_auxv(void *, size_t *);
static void *elf_note_procstat_files(void *, size_t *);
static void *elf_note_procstat_groups(void *, size_t *);
static void *elf_note_procstat_osrel(void *, size_t *);
static void *elf_note_procstat_proc(void *, size_t *);
static void *elf_note_procstat_psstrings(void *, size_t *);
static void *elf_note_procstat_rlimit(void *, size_t *);
static void *elf_note_procstat_umask(void *, size_t *);
static void *elf_note_procstat_vmmap(void *, size_t *);
static void elf_puthdr(pid_t, vm_map_entry_t, void *, size_t, size_t, size_t,
    int);
static void elf_putnote(int, notefunc_t, void *, struct sbuf *);
static void elf_putnotes(pid_t, struct sbuf *, size_t *);
static void freemap(vm_map_entry_t);
static vm_map_entry_t readmap(pid_t);
static void *procstat_sysctl(void *, int, size_t, size_t *sizep);

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
	struct sbuf *sb;
	void *hdr;
	size_t hdrsize, notesz, segoff;
	ssize_t n, old_len;
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
	 * Build the header and the notes using sbuf and write to the file.
	 */
	sb = sbuf_new_auto();
	hdrsize = sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * (1 + seginfo.count);
	/* Start header + notes section. */
	sbuf_start_section(sb, NULL);
	/* Make empty header subsection. */
	sbuf_start_section(sb, &old_len);
	sbuf_putc(sb, 0);
	sbuf_end_section(sb, old_len, hdrsize, 0);
	/* Put notes. */
	elf_putnotes(pid, sb, &notesz);
	/* Align up to a page boundary for the program segments. */
	sbuf_end_section(sb, -1, PAGE_SIZE, 0);
	if (sbuf_finish(sb) != 0)
		err(1, "sbuf_finish");
	hdr = sbuf_data(sb);
	segoff = sbuf_len(sb);
	/* Fill in the header. */
	elf_puthdr(pid, map, hdr, hdrsize, notesz, segoff, seginfo.count);

	n = write(fd, hdr, segoff);
	if (n == -1)
		err(1, "write");
	if (n < segoff)
              errx(1, "short write");

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
				errx(1, "short read wanted %zu, got %zd",
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
	sbuf_delete(sb);
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
elf_putnotes(pid_t pid, struct sbuf *sb, size_t *sizep)
{
	lwpid_t *tids;
	size_t threads, old_len;
	ssize_t size;
	int i;

	errno = 0;
	threads = ptrace(PT_GETNUMLWPS, pid, NULL, 0);
	if (errno)
		err(1, "PT_GETNUMLWPS");
	tids = malloc(threads * sizeof(*tids));
	if (tids == NULL)
		errx(1, "out of memory");
	errno = 0;
	ptrace(PT_GETLWPLIST, pid, (void *)tids, threads);
	if (errno)
		err(1, "PT_GETLWPLIST");

	sbuf_start_section(sb, &old_len);
	elf_putnote(NT_PRPSINFO, elf_note_prpsinfo, &pid, sb);

	for (i = 0; i < threads; ++i) {
		elf_putnote(NT_PRSTATUS, elf_note_prstatus, tids + i, sb);
		elf_putnote(NT_FPREGSET, elf_note_fpregset, tids + i, sb);
		elf_putnote(NT_THRMISC, elf_note_thrmisc, tids + i, sb);
	}

	elf_putnote(NT_PROCSTAT_PROC, elf_note_procstat_proc, &pid, sb);
	elf_putnote(NT_PROCSTAT_FILES, elf_note_procstat_files, &pid, sb);
	elf_putnote(NT_PROCSTAT_VMMAP, elf_note_procstat_vmmap, &pid, sb);
	elf_putnote(NT_PROCSTAT_GROUPS, elf_note_procstat_groups, &pid, sb);
	elf_putnote(NT_PROCSTAT_UMASK, elf_note_procstat_umask, &pid, sb);
	elf_putnote(NT_PROCSTAT_RLIMIT, elf_note_procstat_rlimit, &pid, sb);
	elf_putnote(NT_PROCSTAT_OSREL, elf_note_procstat_osrel, &pid, sb);
	elf_putnote(NT_PROCSTAT_PSSTRINGS, elf_note_procstat_psstrings, &pid,
	    sb);
	elf_putnote(NT_PROCSTAT_AUXV, elf_note_procstat_auxv, &pid, sb);

	size = sbuf_end_section(sb, old_len, 1, 0);
	if (size == -1)
		err(1, "sbuf_end_section");
	free(tids);
	*sizep = size;
}

/*
 * Emit one note section to sbuf.
 */
static void
elf_putnote(int type, notefunc_t notefunc, void *arg, struct sbuf *sb)
{
	Elf_Note note;
	size_t descsz;
	ssize_t old_len;
	void *desc;

	desc = notefunc(arg, &descsz);
	note.n_namesz = 8; /* strlen("FreeBSD") + 1 */
	note.n_descsz = descsz;
	note.n_type = type;

	sbuf_bcat(sb, &note, sizeof(note));
	sbuf_start_section(sb, &old_len);
	sbuf_bcat(sb, "FreeBSD", note.n_namesz);
	sbuf_end_section(sb, old_len, sizeof(Elf32_Size), 0);
	if (descsz == 0)
		return;
	sbuf_start_section(sb, &old_len);
	sbuf_bcat(sb, desc, descsz);
	sbuf_end_section(sb, old_len, sizeof(Elf32_Size), 0);
	free(desc);
}

/*
 * Generate the ELF coredump header.
 */
static void
elf_puthdr(pid_t pid, vm_map_entry_t map, void *hdr, size_t hdrsize,
    size_t notesz, size_t segoff, int numsegs)
{
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	struct phdr_closure phc;

	ehdr = (Elf_Ehdr *)hdr;
	phdr = (Elf_Phdr *)((char *)hdr + sizeof(Elf_Ehdr));

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
	ehdr->e_phoff = sizeof(Elf_Ehdr);
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

	/* The note segement. */
	phdr->p_type = PT_NOTE;
	phdr->p_offset = hdrsize;
	phdr->p_vaddr = 0;
	phdr->p_paddr = 0;
	phdr->p_filesz = notesz;
	phdr->p_memsz = 0;
	phdr->p_flags = PF_R;
	phdr->p_align = sizeof(Elf32_Size);
	phdr++;

	/* All the writable segments from the program. */
	phc.phdr = phdr;
	phc.offset = segoff;
	each_writable_segment(map, cb_put_phdr, &phc);
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

/*
 * Miscellaneous note out functions.
 */

static void *
elf_note_prpsinfo(void *arg, size_t *sizep)
{
	pid_t pid;
	prpsinfo_t *psinfo;
	struct kinfo_proc kip;
	size_t len;
	int name[4];

	pid = *(pid_t *)arg;
	psinfo = calloc(1, sizeof(*psinfo));
	if (psinfo == NULL)
		errx(1, "out of memory");
	psinfo->pr_version = PRPSINFO_VERSION;
	psinfo->pr_psinfosz = sizeof(prpsinfo_t);

	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_PID;
	name[3] = pid;
	len = sizeof(kip);
	if (sysctl(name, 4, &kip, &len, NULL, 0) == -1)
		err(1, "kern.proc.pid.%u", pid);
	if (kip.ki_pid != pid)
		err(1, "kern.proc.pid.%u", pid);
	strncpy(psinfo->pr_fname, kip.ki_comm, MAXCOMLEN);
	strncpy(psinfo->pr_psargs, psinfo->pr_fname, PRARGSZ);

	*sizep = sizeof(*psinfo);
	return (psinfo);
}

static void *
elf_note_prstatus(void *arg, size_t *sizep)
{
	lwpid_t tid;
	prstatus_t *status;

	tid = *(lwpid_t *)arg;
	status = calloc(1, sizeof(*status));
	if (status == NULL)
		errx(1, "out of memory");
	status->pr_version = PRSTATUS_VERSION;
	status->pr_statussz = sizeof(prstatus_t);
	status->pr_gregsetsz = sizeof(gregset_t);
	status->pr_fpregsetsz = sizeof(fpregset_t);
	status->pr_osreldate = __FreeBSD_version;
	status->pr_pid = tid;
	ptrace(PT_GETREGS, tid, (void *)&status->pr_reg, 0);

	*sizep = sizeof(*status);
	return (status);
}

static void *
elf_note_fpregset(void *arg, size_t *sizep)
{
	lwpid_t tid;
	prfpregset_t *fpregset;

	tid = *(lwpid_t *)arg;
	fpregset = calloc(1, sizeof(*fpregset));
	if (fpregset == NULL)
		errx(1, "out of memory");
	ptrace(PT_GETFPREGS, tid, (void *)fpregset, 0);

	*sizep = sizeof(*fpregset);
	return (fpregset);
}

static void *
elf_note_thrmisc(void *arg, size_t *sizep)
{
	lwpid_t tid;
	struct ptrace_lwpinfo lwpinfo;
	thrmisc_t *thrmisc;

	tid = *(lwpid_t *)arg;
	thrmisc = calloc(1, sizeof(*thrmisc));
	if (thrmisc == NULL)
		errx(1, "out of memory");
	ptrace(PT_LWPINFO, tid, (void *)&lwpinfo,
	    sizeof(lwpinfo));
	memset(&thrmisc->_pad, 0, sizeof(thrmisc->_pad));
	strcpy(thrmisc->pr_tname, lwpinfo.pl_tdname);

	*sizep = sizeof(*thrmisc);
	return (thrmisc);
}

static void *
procstat_sysctl(void *arg, int what, size_t structsz, size_t *sizep)
{
	size_t len, oldlen;
	pid_t pid;
	int name[4], structsize;
	void *buf, *p;

	pid = *(pid_t *)arg;
	structsize = structsz;
	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = what;
	name[3] = pid;
	len = 0;
	if (sysctl(name, 4, NULL, &len, NULL, 0) == -1)
		err(1, "kern.proc.%d.%u", what, pid);
	buf = calloc(1, sizeof(structsize) + len * 4 / 3);
	if (buf == NULL)
		errx(1, "out of memory");
	bcopy(&structsize, buf, sizeof(structsize));
	p = (char *)buf + sizeof(structsize);
	if (sysctl(name, 4, p, &len, NULL, 0) == -1)
		err(1, "kern.proc.%d.%u", what, pid);

	*sizep = sizeof(structsize) + len;
	return (buf);
}

static void *
elf_note_procstat_proc(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_PID | KERN_PROC_INC_THREAD,
	    sizeof(struct kinfo_proc), sizep));
}

static void *
elf_note_procstat_files(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_FILEDESC,
	    sizeof(struct kinfo_file), sizep));
}

static void *
elf_note_procstat_vmmap(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_VMMAP,
	    sizeof(struct kinfo_vmentry), sizep));
}

static void *
elf_note_procstat_groups(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_GROUPS,
	    (int)sizeof(gid_t), sizep));
}

static void *
elf_note_procstat_umask(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_UMASK, sizeof(u_short), sizep));
}

static void *
elf_note_procstat_osrel(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_OSREL, sizeof(int), sizep));
}

static void *
elf_note_procstat_psstrings(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_PS_STRINGS,
	    sizeof(vm_offset_t), sizep));
}

static void *
elf_note_procstat_auxv(void *arg, size_t *sizep)
{

	return (procstat_sysctl(arg, KERN_PROC_AUXV,
	    sizeof(Elf_Auxinfo), sizep));
}

static void *
elf_note_procstat_rlimit(void *arg, size_t *sizep)
{
	pid_t pid;
	size_t len;
	int i, name[5], structsize;
	void *buf, *p;

	pid = *(pid_t *)arg;
	structsize = sizeof(struct rlimit) * RLIM_NLIMITS;
	buf = calloc(1, sizeof(structsize) + structsize);
	if (buf == NULL)
		errx(1, "out of memory");
	bcopy(&structsize, buf, sizeof(structsize));
	p = (char *)buf + sizeof(structsize);
	name[0] = CTL_KERN;
	name[1] = KERN_PROC;
	name[2] = KERN_PROC_RLIMIT;
	name[3] = pid;
	len = sizeof(struct rlimit);
	for (i = 0; i < RLIM_NLIMITS; i++) {
		name[4] = i;
		if (sysctl(name, 5, p, &len, NULL, 0) == -1)
			err(1, "kern.proc.rlimit.%u", pid);
		if (len != sizeof(struct rlimit))
			errx(1, "kern.proc.rlimit.%u: short read", pid);
		p += len;
	}

	*sizep = sizeof(structsize) + structsize;
	return (buf);
}

struct dumpers elfdump = { elf_ident, elf_coredump };
TEXT_SET(dumpset, elfdump);
