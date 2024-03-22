/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Bojan Novković <bnovkov@freebsd.org>
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <ddb/ddb.h>
#include <ddb/db_ctf.h>

static const ctf_header_t *
db_ctf_fetch_cth(linker_ctf_t *lc)
{
	return (const ctf_header_t *)lc->ctftab;
}

/*
 * Tries to look up the ELF symbol -> CTF type identifier mapping by scanning
 * the CTF object section.
 */
static uint32_t
sym_to_objtoff(linker_ctf_t *lc, const Elf_Sym *sym, const Elf_Sym *symtab,
    const Elf_Sym *symtab_end)
{
	const ctf_header_t *hp = db_ctf_fetch_cth(lc);
	uint32_t objtoff = hp->cth_objtoff;
	const size_t idwidth = 4;

	/* Ignore non-object symbols */
	if (ELF_ST_TYPE(sym->st_info) != STT_OBJECT) {
		return (DB_CTF_INVALID_OFF);
	}
	/* Sanity check */
	if (!(sym >= symtab && sym <= symtab_end)) {
		return (DB_CTF_INVALID_OFF);
	}

	for (const Elf_Sym *symp = symtab; symp < symtab_end; symp++) {
		/* Make sure we do not go beyond the objtoff section */
		if (objtoff >= hp->cth_funcoff) {
			objtoff = DB_CTF_INVALID_OFF;
			break;
		}
		if (symp->st_name == 0 || symp->st_shndx == SHN_UNDEF) {
			continue;
		}
		if (symp->st_shndx == SHN_ABS && symp->st_value == 0) {
			continue;
		}

		/* Skip non-object symbols */
		if (ELF_ST_TYPE(symp->st_info) != STT_OBJECT) {
			continue;
		}
		if (symp == sym) {
			break;
		}
		objtoff += idwidth;
	}

	return (objtoff);
}

/*
 * Returns the size of CTF type 't'.
 */
static u_int
db_ctf_type_size(struct ctf_type_v3 *t)
{
	u_int vlen, kind, ssize;
	u_int type_struct_size, kind_size;

	vlen = CTF_V3_INFO_VLEN(t->ctt_info);
	kind = CTF_V3_INFO_KIND(t->ctt_info);
	ssize = ((t->ctt_size == CTF_V3_LSIZE_SENT) ? CTF_TYPE_LSIZE(t) :
						      t->ctt_size);
	type_struct_size = ((t->ctt_size == CTF_V3_LSIZE_SENT) ?
		sizeof(struct ctf_type_v3) :
		sizeof(struct ctf_stype_v3));

	switch (kind) {
	case CTF_K_INTEGER:
	case CTF_K_FLOAT:
		kind_size = sizeof(uint32_t);
		break;
	case CTF_K_ARRAY:
		kind_size = sizeof(struct ctf_array_v3);
		break;
	case CTF_K_UNION:
	case CTF_K_STRUCT:
		kind_size = vlen *
		    ((ssize < CTF_V3_LSTRUCT_THRESH) ?
			    sizeof(struct ctf_member_v3) :
			    sizeof(struct ctf_lmember_v3));
		break;
	case CTF_K_ENUM:
		kind_size = vlen * sizeof(struct ctf_enum);
		break;
	case CTF_K_FUNCTION:
		kind_size = vlen * sizeof(uint32_t);
		break;
	case CTF_K_UNKNOWN:
	case CTF_K_FORWARD:
	case CTF_K_POINTER:
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
		kind_size = 0;
		break;
	default:
		db_printf("Error: invalid CTF type kind encountered\n");
		return (-1);
	}

	return (type_struct_size + kind_size);
}

/*
 * Looks up type name 'name' in the CTF string table and returns the
 * corresponding CTF type struct, if any.
 */
struct ctf_type_v3 *
db_ctf_typename_to_type(linker_ctf_t *lc, const char *name)
{
	const ctf_header_t *hp = db_ctf_fetch_cth(lc);
	char *start, *cur, *end;
	uint32_t stroff = hp->cth_stroff;
	uint32_t typeoff = hp->cth_typeoff;
	uint32_t name_stroff;
	const uint8_t *ctfstart = (const uint8_t *)hp + sizeof(ctf_header_t);

	u_int skiplen;

	/* Scan ctf strtab for typename. */
	start = cur = __DECONST(char *, hp) + sizeof(ctf_header_t) +
	    hp->cth_stroff;
	end = cur + hp->cth_strlen;
	while (cur < end) {
		if (strcmp(cur, name) == 0)
			break;
		cur += strlen(cur) + 1;
	}
	if (cur >= end)
		return (NULL);
	name_stroff = (uint32_t)(cur - start);

	/* Scan for type containing the found stroff. */
	while (typeoff < stroff) {
		struct ctf_type_v3 *t =
		    (struct ctf_type_v3 *)(__DECONST(uint8_t *, ctfstart) +
			typeoff);
		/* We found the type struct */
		if (t->ctt_name == name_stroff) {
			break;
		}
		if ((skiplen = db_ctf_type_size(t)) == -1) {
			return (NULL);
		}
		typeoff += skiplen;
	}
	if (typeoff < stroff) {
		return (struct ctf_type_v3 *)(__DECONST(uint8_t *, ctfstart) +
		    typeoff);
	} else { /* A type struct was not found */
		return (NULL);
	}
}

/*
 * Wrapper used by the kernel linker CTF routines.
 * Currently used to implement lookup of CTF types accross all loaded kernel
 * modules.
 */
bool
db_ctf_lookup_typename(linker_ctf_t *lc, const char *typename)
{
	return (db_ctf_typename_to_type(lc, typename) != NULL);
}

/*
 * Returns the type corresponding to the 'typeid' parameter from the CTF type
 * section.
 */
struct ctf_type_v3 *
db_ctf_typeid_to_type(db_ctf_sym_data_t sd, uint32_t typeid)
{
	const ctf_header_t *hp = db_ctf_fetch_cth(&sd->lc);
	const uint8_t *ctfstart = (const uint8_t *)hp + sizeof(ctf_header_t);
	uint32_t typeoff = hp->cth_typeoff;
	uint32_t stroff = hp->cth_stroff;
	/* CTF typeids start at 0x1 */
	size_t cur_typeid = 1;
	u_int skiplen;

	/* Find corresponding type */
	while (typeoff < stroff) {
		struct ctf_type_v3 *t =
		    (struct ctf_type_v3 *)(__DECONST(uint8_t *, ctfstart) +
			typeoff);

		/* We found the type struct */
		if (cur_typeid == typeid) {
			break;
		}
		cur_typeid++;
		if ((skiplen = db_ctf_type_size(t)) == -1) {
			return (NULL);
		}
		typeoff += skiplen;
	}
	if (typeoff < stroff) {
		return (struct ctf_type_v3 *)(__DECONST(uint8_t *, ctfstart) +
		    typeoff);
	} else { /* A type struct was not found */
		return (NULL);
	}
}

const char *
db_ctf_stroff_to_str(db_ctf_sym_data_t sd, uint32_t off)
{
	const ctf_header_t *hp = db_ctf_fetch_cth(&sd->lc);
	uint32_t stroff = hp->cth_stroff + off;
	const char *ret;

	if (stroff >= (hp->cth_stroff + hp->cth_strlen)) {
		return ("invalid");
	}
	ret = ((const char *)hp + sizeof(ctf_header_t)) + stroff;
	if (*ret == '\0') {
		return (NULL);
	}

	return (ret);
}

/*
 * Tries to find the type of the symbol specified in 'sd->sym'.
 */
struct ctf_type_v3 *
db_ctf_sym_to_type(db_ctf_sym_data_t sd)
{
	uint32_t objtoff, typeid;
	const Elf_Sym *symtab, *symtab_end;

	if (sd->sym == NULL) {
		return (NULL);
	}
	symtab = sd->lc.symtab;
	symtab_end = symtab + sd->lc.nsym;

	objtoff = sym_to_objtoff(&sd->lc, sd->sym, symtab, symtab_end);
	/* Sanity check - should not happen */
	if (objtoff == DB_CTF_INVALID_OFF) {
		db_printf("Could not find CTF object offset.\n");
		return (NULL);
	}

	typeid = *(
	    const uint32_t *)(sd->lc.ctftab + sizeof(ctf_header_t) + objtoff);

	return (db_ctf_typeid_to_type(sd, typeid));
}

/*
 * Scans the kernel file and all loaded module for symbol 'name'.
 */
int
db_ctf_find_symbol(const char *name, db_ctf_sym_data_t sd)
{
	int error;
	c_linker_sym_t lsym = NULL;

	error = linker_ctf_lookup_sym_ddb(name, &lsym, &sd->lc);
	if (error != 0) {
		db_printf(
		    "failed to look up symbol and CTF info for %s: error %d\n",
		    name, error);
		return (error);
	}
	sd->sym = __DECONST(Elf_Sym *, lsym);

	return (0);
}

/*
 * Scans the kernel file and all loaded module for type specified by 'typename'.
 */
struct ctf_type_v3 *
db_ctf_find_typename(db_ctf_sym_data_t sd, const char *typename)
{
	if (linker_ctf_lookup_typename_ddb(&sd->lc, typename) != 0) {
		return (NULL);
	}
	return (db_ctf_typename_to_type(&sd->lc, typename));
}
