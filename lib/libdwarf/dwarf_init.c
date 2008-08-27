/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
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
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <string.h>
#include "_libdwarf.h"

static const char *debug_snames[DWARF_DEBUG_SNAMES] = {
	".debug_abbrev",
	".debug_aranges",
	".debug_frame",
	".debug_info",
	".debug_line",
	".debug_pubnames",
	".eh_frame",
	".debug_macinfo",
	".debug_str",
	".debug_loc",
	".debug_pubtypes",
	".debug_ranges",
	".debug_static_func",
	".debug_static_vars",
	".debug_types",
	".debug_weaknames",
	".symtab",
	".strtab"
};

static uint64_t (*dwarf_read) (Elf_Data **, uint64_t *, int);
static void (*dwarf_write) (Elf_Data **, uint64_t *, uint64_t, int);

static uint64_t
dwarf_read_lsb(Elf_Data **dp, uint64_t *offsetp, int bytes_to_read)
{
	uint64_t ret = 0;

	uint8_t *src = (uint8_t *) (*dp)->d_buf + *offsetp;

	switch (bytes_to_read) {
	case 8:
		ret |= ((uint64_t) src[4]) << 32 | ((uint64_t) src[5]) << 40;
		ret |= ((uint64_t) src[6]) << 48 | ((uint64_t) src[7]) << 56;
	case 4:
		ret |= ((uint64_t) src[2]) << 16 | ((uint64_t) src[3]) << 24;
	case 2:
		ret |= ((uint64_t) src[1]) << 8;
	case 1:
		ret |= src[0];
		break;
	default:
		return 0;
		break;
	}

	*offsetp += bytes_to_read;

	return ret;
}

static uint64_t
dwarf_read_msb(Elf_Data **dp, uint64_t *offsetp, int bytes_to_read)
{
	uint64_t ret = 0;

	uint8_t *src = (uint8_t *) (*dp)->d_buf + *offsetp;

	switch (bytes_to_read) {
	case 1:
		ret = src[0];
		break;
	case 2:
		ret = src[1] | ((uint64_t) src[0]) << 8;
		break;
	case 4:
		ret = src[3] | ((uint64_t) src[2]) << 8;
		ret |= ((uint64_t) src[1]) << 16 | ((uint64_t) src[0]) << 24;
		break;
	case 8:
		ret = src[7] | ((uint64_t) src[6]) << 8;
		ret |= ((uint64_t) src[5]) << 16 | ((uint64_t) src[4]) << 24;
		ret |= ((uint64_t) src[3]) << 32 | ((uint64_t) src[2]) << 40;
		ret |= ((uint64_t) src[1]) << 48 | ((uint64_t) src[0]) << 56;
		break;
	default:
		return 0;
		break;
	}

	*offsetp += bytes_to_read;

	return ret;
}

static void
dwarf_write_lsb(Elf_Data **dp, uint64_t *offsetp, uint64_t value, int bytes_to_write)
{
	uint8_t *dst = (uint8_t *) (*dp)->d_buf + *offsetp;

	switch (bytes_to_write) {
	case 8:
		dst[7] = (value >> 56) & 0xff;
		dst[6] = (value >> 48) & 0xff;
		dst[5] = (value >> 40) & 0xff;
		dst[4] = (value >> 32) & 0xff;
	case 4:
		dst[3] = (value >> 24) & 0xff;
		dst[2] = (value >> 16) & 0xff;
	case 2:
		dst[1] = (value >> 8) & 0xff;
	case 1:
		dst[0] = value & 0xff;
		break;
	default:
		return;
		break;
	}

	*offsetp += bytes_to_write;
}

static void
dwarf_write_msb(Elf_Data **dp, uint64_t *offsetp, uint64_t value, int bytes_to_write)
{
	uint8_t *dst = (uint8_t *) (*dp)->d_buf + *offsetp;

	switch (bytes_to_write) {
	case 8:
		dst[7] = value & 0xff;
		dst[6] = (value >> 8) & 0xff;
		dst[5] = (value >> 16) & 0xff;
		dst[4] = (value >> 24) & 0xff;
		value >>= 32;
	case 4:
		dst[3] = value & 0xff;
		dst[2] = (value >> 8) & 0xff;
		value >>= 16;
	case 2:
		dst[1] = value & 0xff;
		value >>= 8;
	case 1:
		dst[0] = value & 0xff;
		break;
	default:
		return;
		break;
	}

	*offsetp += bytes_to_write;
}

static int64_t
dwarf_read_sleb128(Elf_Data **dp, uint64_t *offsetp)
{
	int64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = (uint8_t *) (*dp)->d_buf + *offsetp;

	do {
		b = *src++;

		ret |= ((b & 0x7f) << shift);

		(*offsetp)++;

		shift += 7;
	} while ((b & 0x80) != 0);

	if (shift < 32 && (b & 0x40) != 0)
		ret |= (-1 << shift);

	return ret;
}

static uint64_t
dwarf_read_uleb128(Elf_Data **dp, uint64_t *offsetp)
{
	uint64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = (uint8_t *) (*dp)->d_buf + *offsetp;

	do {
		b = *src++;

		ret |= ((b & 0x7f) << shift);

		(*offsetp)++;

		shift += 7;
	} while ((b & 0x80) != 0);

	return ret;
}

static const char *
dwarf_read_string(Elf_Data **dp, uint64_t *offsetp)
{
	char *ret;

	char *src = (char *) (*dp)->d_buf + *offsetp;

	ret = src;

	while (*src != '\0' && *offsetp < (*dp)->d_size) {
		src++;
		(*offsetp)++;
	}

	if (*src == '\0' && *offsetp < (*dp)->d_size)
		(*offsetp)++;

	return ret;
}

static uint8_t *
dwarf_read_block(Elf_Data **dp, uint64_t *offsetp, uint64_t length)
{
	uint8_t *ret;

	uint8_t *src = (char *) (*dp)->d_buf + *offsetp;

	ret = src;

	(*offsetp) += length;

	return ret;
}

static int
dwarf_apply_relocations(Dwarf_Debug dbg, Elf_Data *reld, int secindx)
{
	Elf_Data *d;
	GElf_Rela rela;
	int indx = 0;
	int ret = DWARF_E_NONE;
	uint64_t offset;

	/* Point to the data to be relocated: */
	d = dbg->dbg_s[secindx].s_data;

	/* Enter a loop to process each relocation addend: */
	while (gelf_getrela(reld, indx++, &rela) != NULL) {
		GElf_Sym sym;
		Elf64_Xword symindx = ELF64_R_SYM(rela.r_info);

		if (gelf_getsym(dbg->dbg_s[DWARF_symtab].s_data, symindx, &sym) == NULL) {
			printf("Couldn't find symbol index %lu for relocation\n",(u_long) symindx);
			continue;
		}

		offset = rela.r_offset;

		dwarf_write(&d, &offset, rela.r_addend, dbg->dbg_offsize);
	}

	return ret;
}

static int
dwarf_relocate(Dwarf_Debug dbg, Dwarf_Error *error)
{
	Elf_Scn *scn = NULL;
	GElf_Shdr shdr;
	int i;
	int ret = DWARF_E_NONE;

	/* Look for sections which relocate the debug sections. */
	while ((scn = elf_nextscn(dbg->dbg_elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) == NULL) {
			DWARF_SET_ELF_ERROR(error, elf_errno());
			return DWARF_E_ELF;
		}

		if (shdr.sh_type != SHT_RELA || shdr.sh_size == 0)
			continue;

		for (i = 0; i < DWARF_DEBUG_SNAMES; i++) {
			if (dbg->dbg_s[i].s_shnum == shdr.sh_info &&
			    dbg->dbg_s[DWARF_symtab].s_shnum == shdr.sh_link) {
				Elf_Data *rd;

				/* Get the relocation data. */
				if ((rd = elf_getdata(scn, NULL)) == NULL) {
					DWARF_SET_ELF_ERROR(error, elf_errno());
					return DWARF_E_ELF;
				}

				/* Apply the relocations. */
				dwarf_apply_relocations(dbg, rd, i);
				break;
			}
		}
	}

	return ret;
}

static int
dwarf_init_attr(Dwarf_Debug dbg, Elf_Data **dp, uint64_t *offsetp,
    Dwarf_CU cu, Dwarf_Die die, Dwarf_Attribute at, uint64_t form,
    Dwarf_Error *error)
{
	int ret = DWARF_E_NONE;
	struct _Dwarf_AttrValue avref;

	memset(&avref, 0, sizeof(avref));
	avref.av_attrib	= at->at_attrib;
	avref.av_form	= at->at_form;

	switch (form) {
	case DW_FORM_addr:
		avref.u[0].u64 = dwarf_read(dp, offsetp, cu->cu_pointer_size);
		break;
	case DW_FORM_block:
		avref.u[0].u64 = dwarf_read_uleb128(dp, offsetp);
		avref.u[1].u8p = dwarf_read_block(dp, offsetp, avref.u[0].u64);
		break;
	case DW_FORM_block1:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 1);
		avref.u[1].u8p = dwarf_read_block(dp, offsetp, avref.u[0].u64);
		break;
	case DW_FORM_block2:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 2);
		avref.u[1].u8p = dwarf_read_block(dp, offsetp, avref.u[0].u64);
		break;
	case DW_FORM_block4:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 4);
		avref.u[1].u8p = dwarf_read_block(dp, offsetp, avref.u[0].u64);
		break;
	case DW_FORM_data1:
	case DW_FORM_flag:
	case DW_FORM_ref1:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 1);
		break;
	case DW_FORM_data2:
	case DW_FORM_ref2:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 2);
		break;
	case DW_FORM_data4:
	case DW_FORM_ref4:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 4);
		break;
	case DW_FORM_data8:
	case DW_FORM_ref8:
		avref.u[0].u64 = dwarf_read(dp, offsetp, 8);
		break;
	case DW_FORM_indirect:
		form = dwarf_read_uleb128(dp, offsetp);
		return dwarf_init_attr(dbg, dp, offsetp, cu, die, at, form, error);
	case DW_FORM_ref_addr:
		if (cu->cu_version == 2)
			avref.u[0].u64 = dwarf_read(dp, offsetp, cu->cu_pointer_size);
		else if (cu->cu_version == 3)
			avref.u[0].u64 = dwarf_read(dp, offsetp, dbg->dbg_offsize);
		break;
	case DW_FORM_ref_udata:
	case DW_FORM_udata:
		avref.u[0].u64 = dwarf_read_uleb128(dp, offsetp);
		break;
	case DW_FORM_sdata:
		avref.u[0].s64 = dwarf_read_sleb128(dp, offsetp);
		break;
	case DW_FORM_string:
		avref.u[0].s = dwarf_read_string(dp, offsetp);
		break;
	case DW_FORM_strp:
		avref.u[0].u64 = dwarf_read(dp, offsetp, dbg->dbg_offsize);
		avref.u[1].s = elf_strptr(dbg->dbg_elf,
		    dbg->dbg_s[DWARF_debug_str].s_shnum, avref.u[0].u64);
		break;
	default:
		DWARF_SET_ERROR(error, DWARF_E_NOT_IMPLEMENTED);
		ret = DWARF_E_NOT_IMPLEMENTED;
		break;
	}

	if (ret == DWARF_E_NONE)
		ret = dwarf_attrval_add(die, &avref, NULL, error);

	return ret;
}

static int
dwarf_init_abbrev(Dwarf_Debug dbg, Dwarf_CU cu, Dwarf_Error *error)
{
	Dwarf_Abbrev a;
	Elf_Data *d;
	int ret = DWARF_E_NONE;
	uint64_t attr;
	uint64_t entry;
	uint64_t form;
	uint64_t offset;
	uint64_t tag;
	u_int8_t children;

	d = dbg->dbg_s[DWARF_debug_abbrev].s_data;

	offset = cu->cu_abbrev_offset;

	while (offset < d->d_size) {

		entry = dwarf_read_uleb128(&d, &offset);

		/* Check if this is the end of the data: */
		if (entry == 0)
			break;

		tag = dwarf_read_uleb128(&d, &offset);

		children = dwarf_read(&d, &offset, 1);

		if ((ret = dwarf_abbrev_add(cu, entry, tag, children, &a, error)) != DWARF_E_NONE)
			break;

		do {
			attr = dwarf_read_uleb128(&d, &offset);
			form = dwarf_read_uleb128(&d, &offset);

			if (attr != 0)
				if ((ret = dwarf_attr_add(a, attr, form, NULL, error)) != DWARF_E_NONE)
					return ret;
		} while (attr != 0);
	}

	return ret;
}

static int
dwarf_init_info(Dwarf_Debug dbg, Dwarf_Error *error)
{
	Dwarf_CU cu;
	Elf_Data *d = NULL;
	Elf_Scn *scn;
	int i;
	int level = 0;
	int relocated = 0;
	int ret = DWARF_E_NONE;
	uint64_t length;
	uint64_t next_offset;
	uint64_t offset = 0;

	scn = dbg->dbg_s[DWARF_debug_info].s_scn;

	d = dbg->dbg_s[DWARF_debug_info].s_data;

	while (offset < d->d_size) {
		/* Allocate memory for the first compilation unit. */
		if ((cu = calloc(sizeof(struct _Dwarf_CU), 1)) == NULL) {
			DWARF_SET_ERROR(error, DWARF_E_MEMORY);
			return DWARF_E_MEMORY;
		}

		/* Save the offet to this compilation unit: */
		cu->cu_offset = offset;

		length = dwarf_read(&d, &offset, 4);
		if (length == 0xffffffff) {
			length = dwarf_read(&d, &offset, 8);
			dbg->dbg_offsize = 8;
		} else
			dbg->dbg_offsize = 4;

		/*
		 * Check if there is enough ELF data for this CU.
		 * This assumes that libelf gives us the entire
		 * section in one Elf_Data object.
		 */
		if (length > d->d_size - offset) {
			free(cu);
			DWARF_SET_ERROR(error, DWARF_E_INVALID_CU);
			return DWARF_E_INVALID_CU;
		}

		/* Relocate the DWARF sections if necessary: */
		if (!relocated) {
			if ((ret = dwarf_relocate(dbg, error)) != DWARF_E_NONE)
				return ret;
			relocated = 1;
		}

		/* Compute the offset to the next compilation unit: */
		next_offset = offset + length;

		/* Initialise the compilation unit. */
		cu->cu_length 		= length;
		cu->cu_header_length	= (dbg->dbg_offsize == 4) ? 4 : 12;
		cu->cu_version		= dwarf_read(&d, &offset, 2);
		cu->cu_abbrev_offset	= dwarf_read(&d, &offset, dbg->dbg_offsize);
		cu->cu_pointer_size	= dwarf_read(&d, &offset, 1);
		cu->cu_next_offset	= next_offset;

		/* Initialise the list of abbrevs. */
		STAILQ_INIT(&cu->cu_abbrev);

		/* Initialise the list of dies. */
		STAILQ_INIT(&cu->cu_die);

		/* Initialise the hash table of dies. */
		for (i = 0; i < DWARF_DIE_HASH_SIZE; i++)
			STAILQ_INIT(&cu->cu_die_hash[i]);

		/* Add the compilation unit to the list. */
		STAILQ_INSERT_TAIL(&dbg->dbg_cu, cu, cu_next);

		if (cu->cu_version != 2 && cu->cu_version != 3) {
			DWARF_SET_ERROR(error, DWARF_E_CU_VERSION);
			ret = DWARF_E_CU_VERSION;
			break;
		}

		/* Parse the .debug_abbrev info for this CU: */
		if ((ret = dwarf_init_abbrev(dbg, cu, error)) != DWARF_E_NONE)
			break;

		level = 0;

		while (offset < next_offset && offset < d->d_size) {
			Dwarf_Abbrev a;
			Dwarf_Attribute at;
			Dwarf_Die die;
			uint64_t abnum;
			uint64_t die_offset = offset;;

			abnum = dwarf_read_uleb128(&d, &offset);

			if (abnum == 0) {
				level--;
				continue;
			}

			if ((a = dwarf_abbrev_find(cu, abnum)) == NULL) {
				DWARF_SET_ERROR(error, DWARF_E_MISSING_ABBREV);
				return DWARF_E_MISSING_ABBREV;
			}

			if ((ret = dwarf_die_add(cu, level, die_offset,
			    abnum, a, &die, error)) != DWARF_E_NONE)
				return ret;

			STAILQ_FOREACH(at, &a->a_attrib, at_next) {
				if ((ret = dwarf_init_attr(dbg, &d, &offset,
				    cu, die, at, at->at_form, error)) != DWARF_E_NONE)
					return ret;
			}

			if (a->a_children == DW_CHILDREN_yes)
				level++;
		}

		offset = next_offset;
	}

	return ret;
}

static int
dwarf_elf_read(Dwarf_Debug dbg, Dwarf_Error *error)
{
	GElf_Shdr shdr;
	Elf_Scn *scn = NULL;
	char *sname;
	int i;
	int ret = DWARF_E_NONE;

	/* Get a copy of the ELF header. */
	if (gelf_getehdr(dbg->dbg_elf, &dbg->dbg_ehdr) == NULL) {
		DWARF_SET_ELF_ERROR(error, elf_errno());
		return DWARF_E_ELF;
	}

	/* Check the ELF data format: */
	switch (dbg->dbg_ehdr.e_ident[EI_DATA]) {
	case ELFDATA2MSB:
		dwarf_read = dwarf_read_msb;
		dwarf_write = dwarf_write_msb;
		break;

	case ELFDATA2LSB:
	case ELFDATANONE:
	default:
		dwarf_read = dwarf_read_lsb;
		dwarf_write = dwarf_write_lsb;
		break;
	}

	/* Get the section index to the string table. */
	if (elf_getshstrndx(dbg->dbg_elf, &dbg->dbg_stnum) == 0) {
		DWARF_SET_ELF_ERROR(error, elf_errno());
		return DWARF_E_ELF;
	}

	/* Look for the debug sections. */
	while ((scn = elf_nextscn(dbg->dbg_elf, scn)) != NULL) {
		/* Get a copy of the section header: */
		if (gelf_getshdr(scn, &shdr) == NULL) {
			DWARF_SET_ELF_ERROR(error, elf_errno());
			return DWARF_E_ELF;
		}

		/* Get a pointer to the section name: */
		if ((sname = elf_strptr(dbg->dbg_elf, dbg->dbg_stnum, shdr.sh_name)) == NULL) {
			DWARF_SET_ELF_ERROR(error, elf_errno());
			return DWARF_E_ELF;
		}

		/*
		 * Look up the section name to check if it's
		 * one we need for DWARF.
		 */
		for (i = 0; i < DWARF_DEBUG_SNAMES; i++) {
			if (strcmp(sname, debug_snames[i]) == 0) {
				dbg->dbg_s[i].s_sname = sname;
				dbg->dbg_s[i].s_shnum = elf_ndxscn(scn);
				dbg->dbg_s[i].s_scn = scn;
				memcpy(&dbg->dbg_s[i].s_shdr, &shdr, sizeof(shdr));
				if ((dbg->dbg_s[i].s_data = elf_getdata(scn, NULL)) == NULL) {
					DWARF_SET_ELF_ERROR(error, elf_errno());
					return DWARF_E_ELF;
				}
				break;
			}
		}
	}

	/* Check if any of the required sections are missing: */
	if (dbg->dbg_s[DWARF_debug_abbrev].s_scn == NULL ||
	    dbg->dbg_s[DWARF_debug_info].s_scn == NULL) {
		/* Missing debug information. */
		DWARF_SET_ERROR(error, DWARF_E_DEBUG_INFO);
		return DWARF_E_DEBUG_INFO;
	}

	/* Initialise the compilation-units: */
	ret = dwarf_init_info(dbg, error);

	return ret;
}

int
dwarf_elf_init(Elf *elf, int mode, Dwarf_Debug *ret_dbg, Dwarf_Error *error)
{
	Dwarf_Debug dbg;
	int ret = DWARF_E_NONE;

	if (error == NULL)
		/* Can only return a generic error. */
		return DWARF_E_ERROR;

	if (elf == NULL || ret_dbg == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_ARGUMENT);
		ret = DWARF_E_ARGUMENT;
	} else if ((dbg = calloc(sizeof(struct _Dwarf_Debug), 1)) == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_MEMORY);
		ret = DWARF_E_MEMORY;
	} else {
		dbg->dbg_elf		= elf;
		dbg->dbg_elf_close 	= 0;
		dbg->dbg_mode		= mode;

		STAILQ_INIT(&dbg->dbg_cu);

		*ret_dbg = dbg;

		/* Read the ELF sections. */
		ret = dwarf_elf_read(dbg, error);
	}

	return ret;
}

int
dwarf_init(int fd, int mode, Dwarf_Debug *ret_dbg, Dwarf_Error *error)
{
	Dwarf_Error lerror;
	Elf *elf;
	Elf_Cmd	c;
	int ret;

	if (error == NULL)
		/* Can only return a generic error. */
		return DWARF_E_ERROR;

	if (fd < 0 || ret_dbg == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_ARGUMENT);
		return DWARF_E_ERROR;
	}

	/* Translate the DWARF mode to ELF mode. */
	switch (mode) {
	default:
	case DW_DLC_READ:
		c = ELF_C_READ;
		break;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		DWARF_SET_ELF_ERROR(error, elf_errno());
		return DWARF_E_ERROR;
	}

	if ((elf = elf_begin(fd, c, NULL)) == NULL) {
		DWARF_SET_ELF_ERROR(error, elf_errno());
		return DWARF_E_ERROR;
	}

	ret = dwarf_elf_init(elf, mode, ret_dbg, error);

	if (*ret_dbg != NULL)
		/* Remember to close the ELF file. */
		(*ret_dbg)->dbg_elf_close = 1;

	if (ret != DWARF_E_NONE) {
		if (*ret_dbg != NULL) {
			dwarf_finish(ret_dbg, &lerror);
		} else
			elf_end(elf);
	}

	return ret;
}
