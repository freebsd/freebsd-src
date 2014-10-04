/*-
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

#include "_libproc.h"

#ifdef NO_CTF
typedef struct ctf_file ctf_file_t;
#endif

#ifndef NO_CXA_DEMANGLE
extern char *__cxa_demangle(const char *, char *, size_t *, int *);
#endif /* NO_CXA_DEMANGLE */

static void	proc_rdl2prmap(rd_loadobj_t *, prmap_t *);

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

static void
proc_rdl2prmap(rd_loadobj_t *rdl, prmap_t *map)
{
	map->pr_vaddr = rdl->rdl_saddr;
	map->pr_size = rdl->rdl_eaddr - rdl->rdl_saddr;
	map->pr_offset = rdl->rdl_offset;
	map->pr_mflags = 0;
	if (rdl->rdl_prot & RD_RDL_R)
		map->pr_mflags |= MA_READ;
	if (rdl->rdl_prot & RD_RDL_W)
		map->pr_mflags |= MA_WRITE;
	if (rdl->rdl_prot & RD_RDL_X)
		map->pr_mflags |= MA_EXEC;
	strlcpy(map->pr_mapname, rdl->rdl_path,
	    sizeof(map->pr_mapname));
}

char *
proc_objname(struct proc_handle *p, uintptr_t addr, char *objname,
    size_t objnamesz)
{
	size_t i;
	rd_loadobj_t *rdl;

	for (i = 0; i < p->nobjs; i++) {
		rdl = &p->rdobjs[i];
		if (addr >= rdl->rdl_saddr && addr < rdl->rdl_eaddr) {
			strlcpy(objname, rdl->rdl_path, objnamesz);
			return (objname);
		}
	}
	return (NULL);
}

prmap_t *
proc_obj2map(struct proc_handle *p, const char *objname)
{
	size_t i;
	prmap_t *map;
	rd_loadobj_t *rdl;
	char path[MAXPATHLEN];

	rdl = NULL;
	for (i = 0; i < p->nobjs; i++) {
		basename_r(p->rdobjs[i].rdl_path, path);
		if (strcmp(path, objname) == 0) {
			rdl = &p->rdobjs[i];
			break;
		}
	}
	if (rdl == NULL) {
		if (strcmp(objname, "a.out") == 0 && p->rdexec != NULL)
			rdl = p->rdexec;
		else
			return (NULL);
	}

	if ((map = malloc(sizeof(*map))) == NULL)
		return (NULL);
	proc_rdl2prmap(rdl, map);
	return (map);
}

int
proc_iter_objs(struct proc_handle *p, proc_map_f *func, void *cd)
{
	size_t i;
	rd_loadobj_t *rdl;
	prmap_t map;
	char path[MAXPATHLEN];
	char last[MAXPATHLEN];

	if (p->nobjs == 0)
		return (-1);
	memset(last, 0, sizeof(last));
	for (i = 0; i < p->nobjs; i++) {
		rdl = &p->rdobjs[i];
		proc_rdl2prmap(rdl, &map);
		basename_r(rdl->rdl_path, path);
		/*
		 * We shouldn't call the callback twice with the same object.
		 * To do that we are assuming the fact that if there are
		 * repeated object names (i.e. different mappings for the
		 * same object) they occur next to each other.
		 */
		if (strcmp(path, last) == 0)
			continue;
		(*func)(cd, &map, path);
		strlcpy(last, path, sizeof(last));
	}

	return (0);
}

prmap_t *
proc_addr2map(struct proc_handle *p, uintptr_t addr)
{
	size_t i;
	int cnt, lastvn = 0;
	prmap_t *map;
	rd_loadobj_t *rdl;
	struct kinfo_vmentry *kves, *kve;

	/*
	 * If we don't have a cache of listed objects, we need to query
	 * it ourselves.
	 */
	if (p->nobjs == 0) {
		if ((kves = kinfo_getvmmap(p->pid, &cnt)) == NULL)
			return (NULL);
		for (i = 0; i < (size_t)cnt; i++) {
			kve = kves + i;
			if (kve->kve_type == KVME_TYPE_VNODE)
				lastvn = i;
			if (addr >= kve->kve_start && addr < kve->kve_end) {
				if ((map = malloc(sizeof(*map))) == NULL) {
					free(kves);
					return (NULL);
				}
				map->pr_vaddr = kve->kve_start;
				map->pr_size = kve->kve_end - kve->kve_start;
				map->pr_offset = kve->kve_offset;
				map->pr_mflags = 0;
				if (kve->kve_protection & KVME_PROT_READ)
					map->pr_mflags |= MA_READ;
				if (kve->kve_protection & KVME_PROT_WRITE)
					map->pr_mflags |= MA_WRITE;
				if (kve->kve_protection & KVME_PROT_EXEC)
					map->pr_mflags |= MA_EXEC;
				if (kve->kve_flags & KVME_FLAG_COW)
					map->pr_mflags |= MA_COW;
				if (kve->kve_flags & KVME_FLAG_NEEDS_COPY)
					map->pr_mflags |= MA_NEEDS_COPY;
				if (kve->kve_flags & KVME_FLAG_NOCOREDUMP)
					map->pr_mflags |= MA_NOCOREDUMP;
				strlcpy(map->pr_mapname, kves[lastvn].kve_path,
				    sizeof(map->pr_mapname));
				free(kves);
				return (map);
			}
		}
		free(kves);
		return (NULL);
	}

	for (i = 0; i < p->nobjs; i++) {
		rdl = &p->rdobjs[i];
		if (addr >= rdl->rdl_saddr && addr < rdl->rdl_eaddr) {
			if ((map = malloc(sizeof(*map))) == NULL)
				return (NULL);
			proc_rdl2prmap(rdl, map);
			return (map);
		}
	}
	return (NULL);
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
	Elf_Scn *scn, *dynsymscn = NULL, *symtabscn = NULL;
	prmap_t *map;
	const char *s;
	uintptr_t off;
	u_long symtabstridx = 0, dynsymstridx = 0;
	int fd, error = -1;

	if ((map = proc_addr2map(p, addr)) == NULL)
		return (-1);
	if ((fd = open(map->pr_mapname, O_RDONLY, 0)) < 0) {
		DPRINTF("ERROR: open %s failed", map->pr_mapname);
		goto err0;
	}
	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_begin() failed: %s", elf_errmsg(-1));
		goto err1;
	}
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err2;
	}

	/*
	 * Find the index of the STRTAB and SYMTAB sections to locate
	 * symbol names.
	 */
	scn = NULL;
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

	off = ehdr.e_type == ET_EXEC ? 0 : map->pr_vaddr;

	/*
	 * First look up the symbol in the dynsymtab, and fall back to the
	 * symtab if the lookup fails.
	 */
	error = lookup_addr(e, dynsymscn, dynsymstridx, off, addr, &s, symcopy);
	if (error == 0)
		goto out;

	error = lookup_addr(e, symtabscn, symtabstridx, off, addr, &s, symcopy);
	if (error == 0)
		goto out;

out:
	demangle(s, name, namesz);
err2:
	elf_end(e);
err1:
	close(fd);
err0:
	free(map);
	return (error);
}

prmap_t *
proc_name2map(struct proc_handle *p, const char *name)
{
	size_t i;
	int cnt;
	prmap_t *map = NULL;
	char tmppath[MAXPATHLEN];
	struct kinfo_vmentry *kves, *kve;
	rd_loadobj_t *rdl;

	/*
	 * If we haven't iterated over the list of loaded objects,
	 * librtld_db isn't yet initialized and it's very likely
	 * that librtld_db called us. We need to do the heavy
	 * lifting here to find the symbol librtld_db is looking for.
	 */
	if (p->nobjs == 0) {
		if ((kves = kinfo_getvmmap(proc_getpid(p), &cnt)) == NULL)
			return (NULL);
		for (i = 0; i < (size_t)cnt; i++) {
			kve = kves + i;
			basename_r(kve->kve_path, tmppath);
			if (strcmp(tmppath, name) == 0) {
				map = proc_addr2map(p, kve->kve_start);
				break;
			}
		}
		free(kves);
	} else
		for (i = 0; i < p->nobjs; i++) {
			rdl = &p->rdobjs[i];
			basename_r(rdl->rdl_path, tmppath);
			if (strcmp(tmppath, name) == 0) {
				if ((map = malloc(sizeof(*map))) == NULL)
					return (NULL);
				proc_rdl2prmap(rdl, map);
				break;
			}
		}

	if (map == NULL && strcmp(name, "a.out") == 0 && p->rdexec != NULL)
		map = proc_addr2map(p, p->rdexec->rdl_saddr);

	return (map);
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
	Elf *e;
	Elf_Scn *scn, *dynsymscn = NULL, *symtabscn = NULL;
	GElf_Shdr shdr;
	GElf_Ehdr ehdr;
	prmap_t *map;
	uintptr_t off;
	u_long symtabstridx = 0, dynsymstridx = 0;
	int fd, error = -1;

	if ((map = proc_name2map(p, object)) == NULL) {
		DPRINTFX("ERROR: couldn't find object %s", object);
		goto err0;
	}
	if ((fd = open(map->pr_mapname, O_RDONLY, 0)) < 0) {
		DPRINTF("ERROR: open %s failed", map->pr_mapname);
		goto err0;
	}
	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_begin() failed: %s", elf_errmsg(-1));
		goto err1;
	}
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err2;
	}
	/*
	 * Find the index of the STRTAB and SYMTAB sections to locate
	 * symbol names.
	 */
	scn = NULL;
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
	off = ehdr.e_type == ET_EXEC ? 0 : map->pr_vaddr;
	symcopy->st_value += off;

err2:
	elf_end(e);
err1:
	close(fd);
err0:
	free(map);

	return (error);
}

ctf_file_t *
proc_name2ctf(struct proc_handle *p, const char *name)
{
#ifndef NO_CTF
	prmap_t *map;
	int error;

	if ((map = proc_name2map(p, name)) == NULL)
		return (NULL);

	return (ctf_open(map->pr_mapname, &error));
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
	Elf *e;
	int i, fd;
	prmap_t *map;
	Elf_Scn *scn, *foundscn = NULL;
	Elf_Data *data;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	GElf_Sym sym;
	unsigned long stridx = -1;
	char *s;
	int error = -1;

	if ((map = proc_name2map(p, object)) == NULL)
		return (-1);
	if ((fd = open(map->pr_mapname, O_RDONLY)) < 0) {
		DPRINTF("ERROR: open %s failed", map->pr_mapname);
		goto err0;
	}
	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		DPRINTFX("ERROR: elf_begin() failed: %s", elf_errmsg(-1));
		goto err1;
	}
	if (gelf_getehdr(e, &ehdr) == NULL) {
		DPRINTFX("ERROR: gelf_getehdr() failed: %s", elf_errmsg(-1));
		goto err2;
	}
	/*
	 * Find the section we are looking for.
	 */
	scn = NULL;
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
		goto err2;
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
			sym.st_value += map->pr_vaddr;
		(*func)(cd, &sym, s);
	}
	error = 0;
err2:
	elf_end(e);
err1:
	close(fd);
err0:
	free(map);
	return (error);
}
