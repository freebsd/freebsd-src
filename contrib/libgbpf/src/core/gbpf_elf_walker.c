/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2017-2018 Yutaro Hayakawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>

#include <sys/ebpf_vm_isa.h>
#include <sys/ebpf_uapi.h>

#include <gbpf/core/gbpf_driver.h>
#include <gbpf/core/gbpf_elf_walker.h>

#ifdef DEBUG
#define D(_fmt, ...) fprintf(stderr, _fmt "\n", ##__VA_ARGS__)
#else
#define D(_fmt, ...) ;
#endif

#define PROG_SEC ".text"
#define MAP_SEC "maps"
#define RELOC_SEC ".rel" PROG_SEC

// private
struct elf_refs {
	Elf *elf;
	GElf_Ehdr *ehdr;
	Elf_Data *prog;
	Elf_Data *maps;
	Elf_Data *symbols;
	Elf_Data *relocations;
	// information to find program symbol
	int prog_sec_idx;
	GBPFElfWalker *walker;
	GBPFDriver *driver;
};

struct map_reloc
{
	const char *name;
	int map_desc;
};

static inline struct map_reloc *
find_map_entry(struct map_reloc *maps, uint16_t num_maps, char *name)
{
	for (uint16_t i = 0; i < num_maps; i++) {
		if (strcmp(maps[i].name, name) == 0) {
			return &maps[i];
		}
	}
	return NULL;
}

static int
resolve_relocations(struct elf_refs *refs)
{
	uint8_t *mapdata = refs->maps->d_buf;
	struct ebpf_inst *inst, *insts = refs->prog->d_buf;
	int map_desc;
	struct ebpf_map_def *map_def;
	char *symname;
	struct map_reloc discovered_maps[EBPF_PROG_MAX_ATTACHED_MAPS];
	struct map_reloc *reloc;
	uint16_t num_maps = 0;
	int ret = 0;
	GElf_Rel rel;
	GElf_Sym sym;

	for (int i = 0; gelf_getrel(refs->relocations, i, &rel); i++) {
		gelf_getsym(refs->symbols, GELF_R_SYM(rel.r_info), &sym);

		symname =
		    elf_strptr(refs->elf, refs->ehdr->e_shstrndx, sym.st_name);
		if (!symname) {
			return -1;
		}

		inst = insts + rel.r_offset / sizeof(struct ebpf_inst);

		if (inst->opcode == EBPF_OP_LDDW) {
			map_def =
			    (struct ebpf_map_def *)(mapdata + sym.st_value);

			D("Found map relocation entry. It's definition is\n"
			  "  Type: %d KeySize: %u ValueSize: %u MaxEntries: %u "
			  "Flags: %u",
			  map_def->type, map_def->key_size, map_def->value_size,
			  map_def->max_entries, map_def->flags);

			reloc =
			    find_map_entry(discovered_maps, num_maps, symname);
			if (!reloc) {
				int32_t type_id = map_def->type;
				if (type_id < 0) {
					return -1;
				}

				if (num_maps != EBPF_PROG_MAX_ATTACHED_MAPS) {
					map_desc = refs->driver->map_create(
					    refs->driver, type_id,
					    map_def->key_size,
					    map_def->value_size,
					    map_def->max_entries,
					    map_def->flags);
					if (map_desc < 0) {
						return -1;
					}

					if (refs->walker->on_map) {
						refs->walker->on_map(
						    refs->walker, symname,
						    map_desc, map_def);
					}

					reloc = &discovered_maps[num_maps];
					reloc->name = symname;
					reloc->map_desc = map_desc;
					num_maps++;
				} else {
					D("Too many maps");
					ret = -1;
					break;
				}
			}

			D("Map to %d", reloc->map_desc);
			inst->imm = reloc->map_desc;
			inst->src = EBPF_PSEUDO_MAP_DESC;

			continue;
		}

		D("Unknown type relocation entry. name: %s r_offset: %lu",
		  symname, rel.r_offset);
		ret = -1;
	}

	return ret;
}

static int
correct_required_section(struct elf_refs *refs, int idx)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;

	scn = elf_getscn(refs->elf, idx);
	if (!scn) {
		D("%s", elf_errmsg(elf_errno()));
		return -1;
	}

	if (gelf_getshdr(scn, &shdr) != &shdr) {
		return -1;
	}

	if (!refs->symbols && shdr.sh_type == SHT_SYMTAB) {
		refs->symbols = elf_getdata(scn, 0);
		return 0;
	}

	char *shname =
	    elf_strptr(refs->elf, refs->ehdr->e_shstrndx, shdr.sh_name);
	if (!shname) {
		return -1;
	}

	D("Found section name: %s", shname);

	if (!refs->prog && strcmp(shname, PROG_SEC) == 0) {
		refs->prog = elf_getdata(scn, 0);
		refs->prog_sec_idx = idx;
		return 0;
	}

	if (!refs->maps && strcmp(shname, MAP_SEC) == 0) {
		refs->maps = elf_getdata(scn, 0);
		return 0;
	}

	if (!refs->relocations && strcmp(shname, RELOC_SEC) == 0) {
		refs->relocations = elf_getdata(scn, 0);
		return 0;
	}

	return 0;
}

static int
find_prog_sym(struct elf_refs *refs)
{
	GElf_Sym sym;
	int ret = -1;
	for (int i = 0; gelf_getsym(refs->symbols, i, &sym); i++) {
		if (sym.st_shndx == refs->prog_sec_idx && GELF_ST_BIND(sym.st_info) == STB_GLOBAL) {
			if (refs->walker->on_prog) {
				const char *name = elf_strptr(
				    refs->elf, refs->ehdr->e_shstrndx,
				    sym.st_name);
				D("Found prog name: %s", name);
				refs->walker->on_prog(refs->walker, name,
					      (void*)((uint8_t *)refs->prog->d_buf + sym.st_value),
						      sym.st_size);
			}
			ret = 0;
		}
	}
	return ret;
}

int
gbpf_walk_elf(GBPFElfWalker *walker, GBPFDriver *driver, const char *fname)
{
	int error;
	struct elf_refs refs;

	memset(&refs, 0, sizeof(refs));
	refs.walker = walker;
	refs.driver = driver;

	walker->driver = driver;

	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		D("Invalid elf version");
		goto err0;
	}

	refs.elf = elf_begin(fd, ELF_C_READ, NULL);
	if (!refs.elf) {
		D("%s", elf_errmsg(elf_errno()));
		goto err0;
	}

	GElf_Ehdr ehdr;
	if (gelf_getehdr(refs.elf, &ehdr) != &ehdr) {
		D("%s", elf_errmsg(elf_errno()));
		goto err1;
	}
	refs.ehdr = &ehdr;

	for (int i = 1; i < ehdr.e_shnum; i++) {
		error = correct_required_section(&refs, i);
		if (error) {
			goto err1;
		}
	}

	if (!refs.prog) {
		D("Error: " PROG_SEC " missing");
		goto err1;
	}

	if (refs.relocations && refs.symbols) {
		error = resolve_relocations(&refs);
		if (error) {
			D("%s", elf_errmsg(elf_errno()));
			goto err1;
		}
	}

	error = find_prog_sym(&refs);
	if (error) {
		D("Couldn't find program symbol");
		goto err1;
	}

	elf_end(refs.elf);
	close(fd);

	return 0;

err1:
	elf_end(refs.elf);
err0:
	close(fd);
	return -1;
}
