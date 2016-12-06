/*-
 * Copyright (c) 2016 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2008 John Birrell (jb@freebsd.org)
 * All rights reserved.
 *
 * Portions of this software were developed by Rui Paulo under sponsorship
 * from the FreeBSD Foundation.
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

#include <sys/types.h>
#ifndef NO_CTF
#include <sys/ctf.h>
#include <sys/ctf_api.h>
#endif
#include <sys/user.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef NO_CTF
#include <libctf.h>
#endif
#include <libutil.h>

#include "crc32.h"
#include "_libproc.h"

#define	PATH_DEBUG_DIR	"/usr/lib/debug"

#ifdef NO_CTF
typedef struct ctf_file ctf_file_t;
#endif

#ifndef NO_CXA_DEMANGLE
extern char *__cxa_demangle(const char *, char *, size_t *, int *);
#endif /* NO_CXA_DEMANGLE */

static int
crc32_file(int fd, uint32_t *crc)
{
	uint8_t buf[PAGE_SIZE], *p;
	size_t n;

	*crc = ~0;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		p = &buf[0];
		while (n-- > 0)
			*crc = crc32_tab[(*crc ^ *p++) & 0xff] ^ (*crc >> 8);
	}
	*crc = ~*crc;
	return (n);
}

static void
demangle(const char *symbol, char *buf, size_t len)
{
#ifndef NO_CXA_DEMANGLE
	char *dembuf;

	if (symbol[0] == '_' && symbol[1] == 'Z' && symbol[2]) {
		dembuf = __cxa_demangle(symbol, NULL, NULL, NULL);
		if (!dembuf)
			goto fail;
		strlcpy(buf, dembuf, len);
		free(dembuf);
		return;
	}
fail:
#endif /* NO_CXA_DEMANGLE */
	strlcpy(buf, symbol, len);
}

static int
open_debug_file(char *path, const char *debugfile, uint32_t crc)
{
	size_t n;
	uint32_t compcrc;
	int fd;

	fd = -1;
	if ((n = strlcat(path, "/", PATH_MAX)) >= PATH_MAX)
		return (fd);
	if (strlcat(path, debugfile, PATH_MAX) >= PATH_MAX)
		goto out;
	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
		goto out;
	if (crc32_file(fd, &compcrc) != 0 || crc != compcrc) {
		DPRINTFX("ERROR: CRC32 mismatch for %s", path);
		(void)close(fd);
		fd = -1;
	}
out:
	path[n] = '\0';
	return (fd);
}

/*
 * Obtain an ELF descriptor for the specified mapped object. If a GNU debuglink
 * section is present, a descriptor for the corresponding debug file is
 * returned.
 */
static int
open_object(struct map_info *mapping)
{
	char path[PATH_MAX];
	GElf_Shdr shdr;
	Elf *e, *e2;
	Elf_Data *data;
	Elf_Scn *scn;
	struct file_info *file;
	prmap_t *map;
	const char *debugfile, *scnname;
	size_t ndx;
	uint32_t crc;
	int fd, fd2;

	if (mapping->map.pr_mapname[0] == '\0')
		return (-1); /* anonymous object */
	if (mapping->file->elf != NULL)
		return (0); /* already loaded */

	file = mapping->file;
	map = &mapping->map;
	if ((fd = open(map->pr_mapname, O_RDONLY | O_CLOEXEC)) < 0) {
		DPRINTF("ERROR: open %s failed", map->pr_mapname);
		return (-1);
	}
	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_begin() failed: %s", elf_errmsg(-1));
		goto err;
	}

	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			DPRINTFX("ERROR: gelf_getshdr failed: %s",
			    elf_errmsg(-1));
			goto err;
		}
		if (shdr.sh_type != SHT_PROGBITS)
			continue;
		if (elf_getshdrstrndx(e, &ndx) != 0) {
			DPRINTFX("ERROR: elf_getshdrstrndx failed: %s",
			    elf_errmsg(-1));
			goto err;
		}
		if ((scnname = elf_strptr(e, ndx, shdr.sh_name)) == NULL)
			continue;

		if (strcmp(scnname, ".gnu_debuglink") == 0)
			break;
	}
	if (scn == NULL)
		goto internal;

	if ((data = elf_getdata(scn, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_getdata failed: %s", elf_errmsg(-1));
		goto err;
	}

	/*
	 * The data contains a null-terminated file name followed by a 4-byte
	 * CRC.
	 */
	if (data->d_size < sizeof(crc) + 1) {
		DPRINTFX("ERROR: debuglink section is too small (%zd bytes)",
		    data->d_size);
		goto internal;
	}
	if (strnlen(data->d_buf, data->d_size) >= data->d_size - sizeof(crc)) {
		DPRINTFX("ERROR: no null-terminator in gnu_debuglink section");
		goto internal;
	}

	debugfile = data->d_buf;
	memcpy(&crc, (char *)data->d_buf + data->d_size - sizeof(crc),
	    sizeof(crc));

	/*
	 * Search for the debug file using the algorithm described in the gdb
	 * documentation:
	 * - look in the directory containing the object,
	 * - look in the subdirectory ".debug" of the directory containing the
	 *   object,
	 * - look in the global debug directories (currently /usr/lib/debug).
	 */
	(void)strlcpy(path, map->pr_mapname, sizeof(path));
	(void)dirname(path);

	if ((fd2 = open_debug_file(path, debugfile, crc)) >= 0)
		goto external;

	if (strlcat(path, "/.debug", sizeof(path)) < sizeof(path) &&
	    (fd2 = open_debug_file(path, debugfile, crc)) >= 0)
		goto external;

	(void)snprintf(path, sizeof(path), PATH_DEBUG_DIR);
	if (strlcat(path, map->pr_mapname, sizeof(path)) < sizeof(path)) {
		(void)dirname(path);
		if ((fd2 = open_debug_file(path, debugfile, crc)) >= 0)
			goto external;
	}

internal:
	/* We didn't find a debug file, just return the object's descriptor. */
	file->elf = e;
	file->fd = fd;
	return (0);

external:
	if ((e2 = elf_begin(fd2, ELF_C_READ, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_begin failed: %s", elf_errmsg(-1));
		(void)close(fd2);
		goto err;
	}
	(void)elf_end(e);
	(void)close(fd);
	file->elf = e2;
	file->fd = fd2;
	return (0);

err:
	if (e != NULL)
		(void)elf_end(e);
	(void)close(fd);
	return (-1);
}

char *
proc_objname(struct proc_handle *p, uintptr_t addr, char *objname,
    size_t objnamesz)
{
	prmap_t *map;
	size_t i;

	for (i = 0; i < p->nmappings; i++) {
		map = &p->mappings[i].map;
		if (addr >= map->pr_vaddr &&
		    addr < map->pr_vaddr + map->pr_size) {
			strlcpy(objname, map->pr_mapname, objnamesz);
			return (objname);
		}
	}
	return (NULL);
}

/*
 * Currently just returns the first mapping of the named object, effectively
 * what Plmid_to_map(p, PR_LMID_EVERY, objname) does in illumos libproc.
 */
prmap_t *
proc_obj2map(struct proc_handle *p, const char *objname)
{
	char path[PATH_MAX], *base;
	prmap_t *map;
	size_t i;

	map = NULL;
	for (i = 0; i < p->nmappings; i++) {
		strlcpy(path, p->mappings[i].map.pr_mapname, sizeof(path));
		base = basename(path);
		if (strcmp(base, objname) == 0) {
			map = &p->mappings[i].map;
			break;
		}
	}
	if (map == NULL && strcmp(objname, "a.out") == 0 && p->exec_map != NULL)
		map = p->exec_map;
	return (map);
}

int
proc_iter_objs(struct proc_handle *p, proc_map_f *func, void *cd)
{
	char last[MAXPATHLEN], path[MAXPATHLEN], *base;
	prmap_t *map;
	size_t i;
	int error;

	if (p->nmappings == 0)
		if (proc_rdagent(p) == NULL)
			return (-1);

	error = 0;
	memset(last, 0, sizeof(last));
	for (i = 0; i < p->nmappings; i++) {
		map = &p->mappings[i].map;
		strlcpy(path, map->pr_mapname, sizeof(path));
		base = basename(path);
		/*
		 * We shouldn't call the callback twice with the same object.
		 * To do that we are assuming the fact that if there are
		 * repeated object names (i.e. different mappings for the
		 * same object) they occur next to each other.
		 */
		if (strcmp(base, last) == 0)
			continue;
		if ((error = (*func)(cd, map, base)) != 0)
			break;
		strlcpy(last, path, sizeof(last));
	}
	return (error);
}

static struct map_info *
_proc_addr2map(struct proc_handle *p, uintptr_t addr)
{
	struct map_info *mapping;
	size_t i;

	if (p->nmappings == 0)
		if (proc_rdagent(p) == NULL)
			return (NULL);
	for (i = 0; i < p->nmappings; i++) {
		mapping = &p->mappings[i];
		if (addr >= mapping->map.pr_vaddr &&
		    addr < mapping->map.pr_vaddr + mapping->map.pr_size)
			return (mapping);
	}
	return (NULL);
}

prmap_t *
proc_addr2map(struct proc_handle *p, uintptr_t addr)
{

	return (&_proc_addr2map(p, addr)->map);
}

/*
 * Look up the symbol at addr, returning a copy of the symbol and its name.
 */
static int
lookup_addr(Elf *e, Elf_Scn *scn, u_long stridx, uintptr_t off, uintptr_t addr,
    const char **name, GElf_Sym *symcopy)
{
	GElf_Sym sym;
	Elf_Data *data;
	const char *s;
	uint64_t rsym;
	int i;

	if ((data = elf_getdata(scn, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_getdata() failed: %s", elf_errmsg(-1));
		return (1);
	}
	for (i = 0; gelf_getsym(data, i, &sym) != NULL; i++) {
		rsym = off + sym.st_value;
		if (addr >= rsym && addr < rsym + sym.st_size) {
			s = elf_strptr(e, stridx, sym.st_name);
			if (s != NULL) {
				*name = s;
				memcpy(symcopy, &sym, sizeof(*symcopy));
				/*
				 * DTrace expects the st_value to contain
				 * only the address relative to the start of
				 * the function.
				 */
				symcopy->st_value = rsym;
				return (0);
			}
		}
	}
	return (1);
}

int
proc_addr2sym(struct proc_handle *p, uintptr_t addr, char *name,
    size_t namesz, GElf_Sym *symcopy)
{
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf *e;
	Elf_Scn *scn, *dynsymscn, *symtabscn;
	struct map_info *mapping;
	const char *s;
	uintptr_t off;
	u_long symtabstridx, dynsymstridx;
	int error = -1;

	if ((mapping = _proc_addr2map(p, addr)) == NULL) {
		DPRINTFX("ERROR: proc_addr2map failed to resolve 0x%jx", addr);
		return (-1);
	}
	if (open_object(mapping) != 0) {
		DPRINTFX("ERROR: failed to open object %s",
		    mapping->map.pr_mapname);
		return (-1);
	}
	e = mapping->file->elf;
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err;
	}

	/*
	 * Find the index of the STRTAB and SYMTAB sections to locate
	 * symbol names.
	 */
	symtabstridx = dynsymstridx = 0;
	scn = dynsymscn = symtabscn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		switch (shdr.sh_type) {
		case SHT_SYMTAB:
			symtabscn = scn;
			symtabstridx = shdr.sh_link;
			break;
		case SHT_DYNSYM:
			dynsymscn = scn;
			dynsymstridx = shdr.sh_link;
			break;
		}
	}

	off = ehdr.e_type == ET_EXEC ? 0 : mapping->map.pr_vaddr;

	/*
	 * First look up the symbol in the dynsymtab, and fall back to the
	 * symtab if the lookup fails.
	 */
	error = lookup_addr(e, dynsymscn, dynsymstridx, off, addr, &s, symcopy);
	if (error == 0)
		goto out;

	error = lookup_addr(e, symtabscn, symtabstridx, off, addr, &s, symcopy);
	if (error != 0)
		goto err;

out:
	demangle(s, name, namesz);
err:
	return (error);
}

static struct map_info *
_proc_name2map(struct proc_handle *p, const char *name)
{
	char path[MAXPATHLEN], *base;
	struct map_info *mapping;
	size_t i;

	mapping = NULL;
	if (p->nmappings == 0)
		if (proc_rdagent(p) == NULL)
			return (NULL);
	for (i = 0; i < p->nmappings; i++) {
		mapping = &p->mappings[i];
		(void)strlcpy(path, mapping->map.pr_mapname, sizeof(path));
		base = basename(path);
		if (strcmp(base, name) == 0)
			break;
	}
	if (i == p->nmappings)
		mapping = NULL;
	if (mapping == NULL && strcmp(name, "a.out") == 0)
		mapping = _proc_addr2map(p, p->exec_map->pr_vaddr);
	return (mapping);
}

prmap_t *
proc_name2map(struct proc_handle *p, const char *name)
{

	return (&_proc_name2map(p, name)->map);
}

/*
 * Look up the symbol with the given name and return a copy of it.
 */
static int
lookup_name(Elf *e, Elf_Scn *scn, u_long stridx, const char *symbol,
    GElf_Sym *symcopy, prsyminfo_t *si)
{
	GElf_Sym sym;
	Elf_Data *data;
	char *s;
	int i;

	if ((data = elf_getdata(scn, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_getdata() failed: %s", elf_errmsg(-1));
		return (1);
	}
	for (i = 0; gelf_getsym(data, i, &sym) != NULL; i++) {
		s = elf_strptr(e, stridx, sym.st_name);
		if (s != NULL && strcmp(s, symbol) == 0) {
			memcpy(symcopy, &sym, sizeof(*symcopy));
			if (si != NULL)
				si->prs_id = i;
			return (0);
		}
	}
	return (1);
}

int
proc_name2sym(struct proc_handle *p, const char *object, const char *symbol,
    GElf_Sym *symcopy, prsyminfo_t *si)
{
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf *e;
	Elf_Scn *scn, *dynsymscn, *symtabscn;
	struct map_info *mapping;
	uintptr_t off;
	u_long symtabstridx, dynsymstridx;
	int error = -1;

	if ((mapping = _proc_name2map(p, object)) == NULL) {
		DPRINTFX("ERROR: proc_name2map failed to resolve %s", object);
		return (-1);
	}
	if (open_object(mapping) != 0) {
		DPRINTFX("ERROR: failed to open object %s",
		    mapping->map.pr_mapname);
		return (-1);
	}
	e = mapping->file->elf;
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err;
	}

	/*
	 * Find the index of the STRTAB and SYMTAB sections to locate
	 * symbol names.
	 */
	symtabstridx = dynsymstridx = 0;
	scn = dynsymscn = symtabscn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		switch (shdr.sh_type) {
		case SHT_SYMTAB:
			symtabscn = scn;
			symtabstridx = shdr.sh_link;
			break;
		case SHT_DYNSYM:
			dynsymscn = scn;
			dynsymstridx = shdr.sh_link;
			break;
		}
	}

	/*
	 * First look up the symbol in the dynsymtab, and fall back to the
	 * symtab if the lookup fails.
	 */
	error = lookup_name(e, dynsymscn, dynsymstridx, symbol, symcopy, si);
	if (error == 0)
		goto out;

	error = lookup_name(e, symtabscn, symtabstridx, symbol, symcopy, si);
	if (error == 0)
		goto out;

out:
	off = ehdr.e_type == ET_EXEC ? 0 : mapping->map.pr_vaddr;
	symcopy->st_value += off;

err:
	return (error);
}

ctf_file_t *
proc_name2ctf(struct proc_handle *p, const char *name)
{
#ifndef NO_CTF
	ctf_file_t *ctf;
	prmap_t *map;
	int error;

	if ((map = proc_name2map(p, name)) == NULL)
		return (NULL);

	ctf = ctf_open(map->pr_mapname, &error);
	return (ctf);
#else
	(void)p;
	(void)name;
	return (NULL);
#endif
}

int
proc_iter_symbyaddr(struct proc_handle *p, const char *object, int which,
    int mask, proc_sym_f *func, void *cd)
{
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	GElf_Sym sym;
	Elf *e;
	Elf_Scn *scn, *foundscn;
	Elf_Data *data;
	struct map_info *mapping;
	char *s;
	unsigned long stridx = -1;
	int error = -1, i;

	if ((mapping = _proc_name2map(p, object)) == NULL) {
		DPRINTFX("ERROR: proc_name2map failed to resolve %s", object);
		return (-1);
	}
	if (open_object(mapping) != 0) {
		DPRINTFX("ERROR: failed to open object %s",
		    mapping->map.pr_mapname);
		return (-1);
	}
	e = mapping->file->elf;
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err;
	}
	/*
	 * Find the section we are looking for.
	 */
	foundscn = scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
		if (which == PR_SYMTAB &&
		    shdr.sh_type == SHT_SYMTAB) {
			foundscn = scn;
			break;
		} else if (which == PR_DYNSYM &&
		    shdr.sh_type == SHT_DYNSYM) {
			foundscn = scn;
			break;
		}
	}
	if (!foundscn)
		return (-1);
	stridx = shdr.sh_link;
	if ((data = elf_getdata(foundscn, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_getdata() failed: %s", elf_errmsg(-1));
		goto err;
	}
	for (i = 0; gelf_getsym(data, i, &sym) != NULL; i++) {
		if (GELF_ST_BIND(sym.st_info) == STB_LOCAL &&
		    (mask & BIND_LOCAL) == 0)
			continue;
		if (GELF_ST_BIND(sym.st_info) == STB_GLOBAL &&
		    (mask & BIND_GLOBAL) == 0)
			continue;
		if (GELF_ST_BIND(sym.st_info) == STB_WEAK &&
		    (mask & BIND_WEAK) == 0)
			continue;
		if (GELF_ST_TYPE(sym.st_info) == STT_NOTYPE &&
		    (mask & TYPE_NOTYPE) == 0)
			continue;
		if (GELF_ST_TYPE(sym.st_info) == STT_OBJECT &&
		    (mask & TYPE_OBJECT) == 0)
			continue;
		if (GELF_ST_TYPE(sym.st_info) == STT_FUNC &&
		    (mask & TYPE_FUNC) == 0)
			continue;
		if (GELF_ST_TYPE(sym.st_info) == STT_SECTION &&
		    (mask & TYPE_SECTION) == 0)
			continue;
		if (GELF_ST_TYPE(sym.st_info) == STT_FILE &&
		    (mask & TYPE_FILE) == 0)
			continue;
		s = elf_strptr(e, stridx, sym.st_name);
		if (ehdr.e_type != ET_EXEC)
			sym.st_value += mapping->map.pr_vaddr;
		if ((error = (*func)(cd, &sym, s)) != 0)
			goto err;
	}
	error = 0;
err:
	return (error);
}
