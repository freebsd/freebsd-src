/*-
 * Copyright (c) 2013 Mikolaj Golub <trociny@FreeBSD.org>
 * Copyright (c) 2017 Dell EMC
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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
#include <sys/elf.h>
#include <sys/exec.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core.h"

#define PROCSTAT_CORE_MAGIC	0x012DADB8
struct procstat_core
{
	int		pc_magic;
	int		pc_fd;
	Elf		*pc_elf;
	GElf_Ehdr	pc_ehdr;
	GElf_Phdr	pc_phdr;
};

/*
 * XXXss The following is dependent on how capabilities are stored
 * in the core image (and memory) and in the ELF Notes by the
 * kernel. This is subject to changes in the CHERI architecture spec.
 * The procstat code, however, needs to know the offset to the cursor
 * value.
 */
struct _cap128 {
	uint64_t cursor;
	uint64_t base;
} __packed;
typedef struct _cap128 cap128_t;

struct _cap256 {
	uint64_t tps;		/* type, permission, and sealed bits */
	uint64_t cursor;
	uint64_t base;
	uint64_t length;
} __packed;
typedef struct _cap256 cap256_t;

/* XXXss Should be moved to elf.h? */
typedef struct {
	long	a_type;
	union {
		long		a_val;			/* Integer value */
		cap128_t	a_ptr __aligned(16);	/* Address. */
	} a_un;
} ElfCheriABI128_Auxinfo;

/* XXXss Should be moved to elf.h? */
typedef struct {
	long	a_type;
	union {
		long		a_val;			/* Integer value */
		cap256_t	a_ptr __aligned(32);	/* Address. */
	} a_un;
} ElfCheriABI256_Auxinfo;

struct _cap128_ps_strings {
	cap128_t	ps_argvstr __aligned(16);
	int		ps_nargvstr;
	cap128_t	ps_envstr __aligned(16);
	int		ps_nenvstr;
	cap128_t	ps_sbclasses __aligned(16);
	size_t		ps_sbclasseslen;
	cap128_t	ps_sbmethods __aligned(16);
	size_t		ps_sbmethodslen;
	cap128_t	ps_sbobjects __aligned(16);
	size_t		ps_sbobjectslen;
};
typedef struct _cap128_ps_strings cap128_ps_strings_t;

struct _cap256_ps_strings {
	cap256_t	ps_argvstr __aligned(32);
	int		ps_nargvstr;
	cap256_t	ps_envstr __aligned(32);
	int		ps_nenvstr;
	cap256_t	ps_sbclasses __aligned(32);
	size_t		ps_sbclasseslen;
	cap256_t	ps_sbmethods __aligned(32);
	size_t		ps_sbmethodslen;
	cap256_t	ps_sbobjects __aligned(32);
	size_t		ps_sbobjectslen;
};
typedef struct _cap256_ps_strings cap256_ps_strings_t;

static struct psc_type_info {
	unsigned int	n_type;
	int		structsize;
} psc_type_info[PSC_TYPE_MAX] = {
	{ .n_type  = NT_PROCSTAT_PROC, .structsize = sizeof(struct kinfo_proc) },
	{ .n_type = NT_PROCSTAT_FILES, .structsize = sizeof(struct kinfo_file) },
	{ .n_type = NT_PROCSTAT_VMMAP, .structsize = sizeof(struct kinfo_vmentry) },
	{ .n_type = NT_PROCSTAT_GROUPS, .structsize = sizeof(gid_t) },
	{ .n_type = NT_PROCSTAT_UMASK, .structsize = sizeof(u_short) },
	{ .n_type = NT_PROCSTAT_RLIMIT, .structsize = sizeof(struct rlimit) * RLIM_NLIMITS },
	{ .n_type = NT_PROCSTAT_OSREL, .structsize = sizeof(int) },
	{ .n_type = NT_PROCSTAT_PSSTRINGS, .structsize = sizeof(vm_offset_t) },
	{ .n_type = NT_PROCSTAT_PSSTRINGS, .structsize = sizeof(vm_offset_t) },
	{ .n_type = NT_PROCSTAT_PSSTRINGS, .structsize = sizeof(vm_offset_t) },
	{ .n_type = NT_PROCSTAT_AUXV, .structsize = sizeof(Elf_Auxinfo) },
	{ .n_type = NT_PTLWPINFO, .structsize = sizeof(struct ptrace_lwpinfo) },
};

static bool	core_offset(struct procstat_core *core, off_t offset);
static bool	core_read(struct procstat_core *core, void *buf, size_t len);
static ssize_t	core_read_mem(struct procstat_core *core, void *buf,
    size_t len, vm_offset_t addr, bool readall);
static void	*get_args(struct procstat_core *core, vm_offset_t psstrings,
    enum psc_type type, void *buf, size_t *lenp);
static void	*get_auxv(struct procstat_core *core, void *buf, size_t *lenp);

struct procstat_core *
procstat_core_open(const char *filename)
{
	struct procstat_core *core;
	Elf *e;
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;
	size_t nph;
	int fd, i;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		warnx("ELF library too old");
		return (NULL);
	}
	fd = open(filename, O_RDONLY, 0);
	if (fd == -1) {
		warn("open(%s)", filename);
		return (NULL);
	}
	e = elf_begin(fd, ELF_C_READ, NULL);
	if (e == NULL) {
		warnx("elf_begin: %s", elf_errmsg(-1));
		goto fail;
	}
	if (elf_kind(e) != ELF_K_ELF) {
		warnx("%s is not an ELF object", filename);
		goto fail;
	}
	if (gelf_getehdr(e, &ehdr) == NULL) {
		warnx("gelf_getehdr: %s", elf_errmsg(-1));
		goto fail;
	}
	if (ehdr.e_type != ET_CORE) {
		warnx("%s is not a CORE file", filename);
		goto fail;
	}
	if (elf_getphnum(e, &nph) == 0) {
		warnx("program headers not found");
		goto fail;
	}
	for (i = 0; i < ehdr.e_phnum; i++) {
		if (gelf_getphdr(e, i, &phdr) != &phdr) {
			warnx("gelf_getphdr: %s", elf_errmsg(-1));
			goto fail;
		}
		if (phdr.p_type == PT_NOTE)
			break;
	}
	if (i == ehdr.e_phnum) {
		warnx("NOTE program header not found");
		goto fail;
	}
	core = malloc(sizeof(struct procstat_core));
	if (core == NULL) {
		warn("malloc(%zu)", sizeof(struct procstat_core));
		goto fail;
	}
	core->pc_magic = PROCSTAT_CORE_MAGIC;
	core->pc_fd = fd;
	core->pc_elf = e;
	core->pc_ehdr = ehdr;
	core->pc_phdr = phdr;

	return (core);
fail:
	if (e != NULL)
		elf_end(e);
	close(fd);

	return (NULL);
}

void
procstat_core_close(struct procstat_core *core)
{

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	elf_end(core->pc_elf);
	close(core->pc_fd);
	free(core);
}

void *
procstat_core_get(struct procstat_core *core, enum psc_type type, void *buf,
    size_t *lenp)
{
	Elf_Note nhdr;
	off_t offset, eoffset;
	vm_offset_t psstrings;
	void *freebuf;
	size_t len, curlen;
	int cstructsize;
	char nbuf[8];

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	if (type >= PSC_TYPE_MAX) {
		warnx("unknown core stat type: %d", type);
		return (NULL);
	}

	offset = core->pc_phdr.p_offset;
	eoffset = offset + core->pc_phdr.p_filesz;
	curlen = 0;

	while (offset < eoffset) {
		if (!core_offset(core, offset))
			return (NULL);
		if (!core_read(core, &nhdr, sizeof(nhdr)))
			return (NULL);

		offset += sizeof(nhdr) +
		    roundup2(nhdr.n_namesz, sizeof(Elf32_Size)) +
		    roundup2(nhdr.n_descsz, sizeof(Elf32_Size));

		if (nhdr.n_namesz == 0 && nhdr.n_descsz == 0)
			break;
		if (nhdr.n_type != psc_type_info[type].n_type)
			continue;
		if (nhdr.n_namesz != 8)
			continue;
		if (!core_read(core, nbuf, sizeof(nbuf)))
			return (NULL);
		if (strcmp(nbuf, "FreeBSD") != 0)
			continue;
		if (nhdr.n_descsz < sizeof(cstructsize)) {
			warnx("corrupted core file");
			return (NULL);
		}
		if (!core_read(core, &cstructsize, sizeof(cstructsize)))
			return (NULL);
		if (cstructsize != psc_type_info[type].structsize) {
			warnx("version mismatch");
			return (NULL);
		}
		len = nhdr.n_descsz - sizeof(cstructsize);
		if (len == 0)
			return (NULL);
		if (buf != NULL) {
			len = MIN(len, *lenp);
			freebuf = NULL;
		} else {
			freebuf = buf = malloc(len);
			if (buf == NULL) {
				warn("malloc(%zu)", len);
				return (NULL);
			}
		}
		if (!core_read(core, (char *)buf + curlen, len)) {
			free(freebuf);
			return (NULL);
		}
		if (type == PSC_TYPE_AUXV) {
			if ((buf = get_auxv(core, buf, &len)) == NULL)
				return (NULL);
		}
		if (type == PSC_TYPE_ARGV || type == PSC_TYPE_ENVV) {
			if (len < sizeof(psstrings)) {
				free(freebuf);
				return (NULL);
			}
			psstrings = *(vm_offset_t *)buf;
			if (freebuf == NULL)
				len = *lenp;
			else
				buf = NULL;
			free(freebuf);
			buf = get_args(core, psstrings, type, buf, &len);
		} else if (type == PSC_TYPE_PTLWPINFO) {
			*lenp -= len;
			curlen += len;
			continue;
		}
		*lenp = len;
		return (buf);
        }

	if (curlen != 0) {
		*lenp = curlen;
		return (buf);
	}

	return (NULL);
}

static bool
core_offset(struct procstat_core *core, off_t offset)
{

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	if (lseek(core->pc_fd, offset, SEEK_SET) == -1) {
		warn("core: lseek(%jd)", (intmax_t)offset);
		return (false);
	}
	return (true);
}

static bool
core_read(struct procstat_core *core, void *buf, size_t len)
{
	ssize_t n;

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	n = read(core->pc_fd, buf, len);
	if (n == -1) {
		warn("core: read");
		return (false);
	}
	if (n < (ssize_t)len) {
		warnx("core: short read");
		return (false);
	}
	return (true);
}

static ssize_t
core_read_mem(struct procstat_core *core, void *buf, size_t len,
    vm_offset_t addr, bool readall)
{
	GElf_Phdr phdr;
	off_t offset;
	int i;

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	for (i = 0; i < core->pc_ehdr.e_phnum; i++) {
		if (gelf_getphdr(core->pc_elf, i, &phdr) != &phdr) {
			warnx("gelf_getphdr: %s", elf_errmsg(-1));
			return (-1);
		}
		if (phdr.p_type != PT_LOAD)
			continue;
		if (addr < phdr.p_vaddr || addr > phdr.p_vaddr + phdr.p_memsz)
			continue;
		offset = phdr.p_offset + (addr - phdr.p_vaddr);
		if ((phdr.p_vaddr + phdr.p_memsz) - addr < len) {
			if (readall) {
				warnx("format error: "
				    "attempt to read out of segment");
				return (-1);
			}
			len = (phdr.p_vaddr + phdr.p_memsz) - addr;
		}
		if (!core_offset(core, offset))
			return (-1);
		if (!core_read(core, buf, len))
			return (-1);
		return (len);
	}
	warnx("format error: address %ju not found", (uintmax_t)addr);
	return (-1);
}

static inline vm_offset_t
core_read_ps_strings(struct procstat_core *core, vm_offset_t psstrings,
    enum psc_type type, u_int *nstr, size_t *size)
{
	vm_offset_t argaddr, envaddr;
	u_int nargstr, nenvstr;

	assert(type == PSC_TYPE_ARGV || type == PSC_TYPE_ENVV);

	switch(core->pc_ehdr.e_machine) {
	case EM_MIPS_CHERI128:
		{
			cap128_ps_strings_t pss;

			if (core_read_mem(core, &pss, sizeof(pss), psstrings,
			    true) == -1)
				return ((vm_offset_t)0);
			nargstr = pss.ps_nargvstr;
			nenvstr = pss.ps_nenvstr;
			argaddr = (vm_offset_t)pss.ps_argvstr.cursor;
			envaddr = (vm_offset_t)pss.ps_envstr.cursor;
			*size = sizeof(cap128_t);
		}
		break;

	case EM_MIPS_CHERI:
		{
			cap256_ps_strings_t pss;

			if (core_read_mem(core, &pss, sizeof(pss), psstrings,
			    true) == -1)
				return ((vm_offset_t)0);
			nargstr = pss.ps_nargvstr;
			nenvstr = pss.ps_nenvstr;
			argaddr = (vm_offset_t)pss.ps_argvstr.cursor;
			envaddr = (vm_offset_t)pss.ps_envstr.cursor;
			*size = sizeof(cap256_t);
		}
		break;

	default:
		{
			struct ps_strings pss;

			if (core_read_mem(core, &pss, sizeof(pss), psstrings,
			    true) == -1)
				return ((vm_offset_t)0);
			nargstr = pss.ps_nargvstr;
			nenvstr = pss.ps_nenvstr;
			argaddr = (vm_offset_t)pss.ps_argvstr;
			envaddr = (vm_offset_t)pss.ps_envstr;
			*size = sizeof(char *);
		}
		break;
	}

	if (type == PSC_TYPE_ARGV) {
		*nstr = nargstr;
		return (argaddr);
	} else /* type == PSC_TYPE_ENVV */ {
		*nstr = nenvstr;
		return (envaddr);
	}
}

static inline vm_offset_t
core_image_off(struct procstat_core *core, char **ptr, int i)
{

	switch(core->pc_ehdr.e_machine) {
	case EM_MIPS_CHERI128:
		{
			cap128_t *cap = (cap128_t *)(ptr + (i * sizeof(cap128_t) / 8));

			return (cap->cursor);
		}

	case EM_MIPS_CHERI:
		{
			cap256_t *cap = (cap256_t *)(ptr + (i * sizeof(cap256_t) / 8));

			return (cap->cursor);
		}

	default:
		return ((vm_offset_t)ptr[i]);
	}
}

#define ARGS_CHUNK_SZ	256	/* Chunk size (bytes) for get_args operations. */

static void *
get_args(struct procstat_core *core, vm_offset_t psstrings, enum psc_type type,
     void *args, size_t *lenp)
{
	void *freeargs;
	vm_offset_t addr;
	char **argv, *p;
	size_t chunksz, done, len, nchr, size, psz;
	ssize_t n;
	u_int i, nstr;

	if (!(addr = core_read_ps_strings(core, psstrings, type, &nstr, &psz)))
		return (NULL);
	if (addr == 0 || nstr == 0)
		return (NULL);
	if (nstr > ARG_MAX) {
		warnx("format error");
		return (NULL);
	}
	size = nstr * psz;
	argv = malloc(size);
	if (argv == NULL) {
		warn("malloc(%zu)", size);
		return (NULL);
	}
	done = 0;
	freeargs = NULL;
	if (core_read_mem(core, argv, size, addr, true) == -1)
		goto fail;
	if (args != NULL) {
		nchr = MIN(ARG_MAX, *lenp);
	} else {
		nchr = ARG_MAX;
		freeargs = args = malloc(nchr);
		if (args == NULL) {
			warn("malloc(%zu)", nchr);
			goto fail;
		}
	}
	p = args;
	for (i = 0; ; i++) {
		if (i == nstr)
			goto done;
		/*
		 * The program may have scribbled into its argv array, e.g. to
		 * remove some arguments.  If that has happened, break out
		 * before trying to read from NULL.
		 */
		if (core_image_off(core, argv, i) == 0UL)
			goto done;
		for (addr = core_image_off(core, argv, i); ; addr += chunksz) {
			chunksz = MIN(ARGS_CHUNK_SZ, nchr - 1 - done);
			if (chunksz <= 0)
				goto done;
			n = core_read_mem(core, p, chunksz, addr, false);
			if (n == -1)
				goto fail;
			len = strnlen(p, chunksz);
			p += len;
			done += len;
			if (len != chunksz)
				break;
		}
		*p++ = '\0';
		done++;
	}
fail:
	free(freeargs);
	args = NULL;
done:
	*lenp = done;
	free(argv);
	return (args);
}

static bool
is_auxv_ptr(long type)
{
	switch(type) {
	case AT_PHDR:
	case AT_BASE:
	case AT_ENTRY:
	case AT_EXECPATH:
	case AT_CANARY:
	case AT_PAGESIZES:
	case AT_TIMEKEEP:
		return (true);
	default:
		return (false);
	}
}

static void *
get_auxv(struct procstat_core *core, void *auxv, size_t *lenp)
{
	Elf_Auxinfo *buf;
	unsigned count, i;

	switch(core->pc_ehdr.e_machine) {
	case EM_MIPS_CHERI128:
		{
			ElfCheriABI128_Auxinfo *auxv_cheri =
			    (ElfCheriABI128_Auxinfo *)auxv;

			count = *lenp / sizeof(ElfCheriABI128_Auxinfo);
			*lenp = count * sizeof(Elf_Auxinfo);
			buf = (Elf_Auxinfo *)malloc(*lenp);
			if (buf == NULL) {
				free(auxv);
				*lenp = 0;
				return (NULL);
			}
			for (i = 0; i < count; i++) {
				buf[i].a_type = auxv_cheri[i].a_type;
				if (is_auxv_ptr(auxv_cheri[i].a_type))
					buf[i].a_un.a_ptr = (void *)(uintptr_t)
					    auxv_cheri[i].a_un.a_ptr.cursor;
				else
					buf[i].a_un.a_val =
					    auxv_cheri[i].a_un.a_val;
			}
			free(auxv);
			return ((void *)buf);
		}

	case EM_MIPS_CHERI:
		{
			ElfCheriABI256_Auxinfo *auxv_cheri =
			    (ElfCheriABI256_Auxinfo *)auxv;

			count = *lenp / sizeof(ElfCheriABI256_Auxinfo);
			*lenp = count * sizeof(Elf_Auxinfo);
			buf = (Elf_Auxinfo *)malloc(*lenp);
			if (buf == NULL) {
				free(auxv);
				*lenp = 0;
				return (NULL);
			}
			for (i = 0; i < count; i++) {
				buf[i].a_type = auxv_cheri[i].a_type;
				if (is_auxv_ptr(auxv_cheri[i].a_type))
					buf[i].a_un.a_ptr = (void *)(uintptr_t)
					    auxv_cheri[i].a_un.a_ptr.cursor;
				else
					buf[i].a_un.a_val =
					    auxv_cheri[i].a_un.a_val;
			}
			free(auxv);
			return ((void *)buf);
		}

	default:
		/* Nothing needs to be done so just pass the buffer back. */
		return (auxv);
	}
}

int
procstat_core_note_count(struct procstat_core *core, enum psc_type type)
{
	Elf_Note nhdr;
	off_t offset, eoffset;
	int cstructsize;
	char nbuf[8];
	int n;

	if (type >= PSC_TYPE_MAX) {
		warnx("unknown core stat type: %d", type);
		return (0);
	}

	offset = core->pc_phdr.p_offset;
	eoffset = offset + core->pc_phdr.p_filesz;

	for (n = 0; offset < eoffset; n++) {
		if (!core_offset(core, offset))
			return (0);
		if (!core_read(core, &nhdr, sizeof(nhdr)))
			return (0);

		offset += sizeof(nhdr) +
		    roundup2(nhdr.n_namesz, sizeof(Elf32_Size)) +
		    roundup2(nhdr.n_descsz, sizeof(Elf32_Size));

		if (nhdr.n_namesz == 0 && nhdr.n_descsz == 0)
			break;
		if (nhdr.n_type != psc_type_info[type].n_type)
			continue;
		if (nhdr.n_namesz != 8)
			continue;
		if (!core_read(core, nbuf, sizeof(nbuf)))
			return (0);
		if (strcmp(nbuf, "FreeBSD") != 0)
			continue;
		if (nhdr.n_descsz < sizeof(cstructsize)) {
			warnx("corrupted core file");
			return (0);
		}
		if (!core_read(core, &cstructsize, sizeof(cstructsize)))
			return (0);
		if (cstructsize != psc_type_info[type].structsize) {
			warnx("version mismatch");
			return (0);
		}
		if (nhdr.n_descsz - sizeof(cstructsize) == 0)
			return (0);
	}

	return (n);
}
