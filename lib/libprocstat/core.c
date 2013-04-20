/*-
 * Copyright (c) 2013 Mikolaj Golub <trociny@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/elf.h>
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

static bool	core_offset(struct procstat_core *core, off_t offset);
static bool	core_read(struct procstat_core *core, void *buf, size_t len);

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
	void *freebuf;
	size_t len;
	u_int32_t n_type;
	int cstructsize, structsize;
	char nbuf[8];

	assert(core->pc_magic == PROCSTAT_CORE_MAGIC);

	switch(type) {
	case PSC_TYPE_PROC:
		n_type = NT_PROCSTAT_PROC;
		structsize = sizeof(struct kinfo_proc);
		break;
	case PSC_TYPE_FILES:
		n_type = NT_PROCSTAT_FILES;
		structsize = sizeof(struct kinfo_file);
		break;
	case PSC_TYPE_VMMAP:
		n_type = NT_PROCSTAT_VMMAP;
		structsize = sizeof(struct kinfo_vmentry);
		break;
	case PSC_TYPE_GROUPS:
		n_type = NT_PROCSTAT_GROUPS;
		structsize = sizeof(gid_t);
		break;
	case PSC_TYPE_UMASK:
		n_type = NT_PROCSTAT_UMASK;
		structsize = sizeof(u_short);
		break;
	default:
		warnx("unknown core stat type: %d", type);
		return (NULL);
	}

	offset = core->pc_phdr.p_offset;
	eoffset = offset + core->pc_phdr.p_filesz;

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
		if (nhdr.n_type != n_type)
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
		if (cstructsize != structsize) {
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
		if (!core_read(core, buf, len)) {
			free(freebuf);
			return (NULL);
		}
		*lenp = len;
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
