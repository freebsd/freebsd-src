/*-
 * Copyright (c) 2010-2013 Kai Wang
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

#include "ld.h"
#include "ld_input.h"
#include "ld_layout.h"
#include "ld_output.h"
#include "ld_script.h"
#include "ld_symbols.h"
#include "ld_symver.h"
#include "ld_strtab.h"

ELFTC_VCSID("$Id: ld_symver.c 2917 2013-02-16 07:16:02Z kaiwang27 $");

/*
 * Symbol versioning sections are the same for 32bit and 64bit
 * ELF objects.
 */
#define Elf_Verdef	Elf32_Verdef
#define	Elf_Verdaux	Elf32_Verdaux
#define	Elf_Verneed	Elf32_Verneed
#define	Elf_Vernaux	Elf32_Vernaux

static void _add_version_name(struct ld *ld, struct ld_input *li, int ndx,
    const char *name);
static struct ld_symver_vda *_alloc_vda(struct ld *ld, const char *name,
    struct ld_symver_verdef *svd);
static struct ld_symver_vna *_alloc_vna(struct ld *ld, const char *name,
    struct ld_symver_verneed *svn);
static struct ld_symver_verdef *_alloc_verdef(struct ld *ld,
    struct ld_symver_verdef_head *head);
static struct ld_symver_verneed *_alloc_verneed(struct ld *ld,
    struct ld_input *li, struct ld_symver_verneed_head *head);
static struct ld_symver_verdef *_load_verdef(struct ld *ld,
    struct ld_input *li, Elf_Verdef *vd);
static void _load_verdef_section(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *verdef);
static void _load_verneed_section(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *verneed);

void
ld_symver_load_symbol_version_info(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *versym, Elf_Scn *verneed, Elf_Scn *verdef)
{
	Elf_Data *d_vs;
	int elferr;

	if (versym == NULL)
		return;

	(void) elf_errno();
	if ((d_vs = elf_getdata(versym, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_fatal(ld, "%s: elf_getdata failed: %s", li->li_name,
			    elf_errmsg(elferr));
		return;
	}
	if (d_vs->d_size == 0)
		return;

	if ((li->li_versym = malloc(d_vs->d_size)) == NULL)
		ld_fatal_std(ld, "malloc");
	memcpy(li->li_versym, d_vs->d_buf, d_vs->d_size);
	li->li_versym_sz = d_vs->d_size / sizeof(uint16_t);

	_add_version_name(ld, li, 0, "*local*");
	_add_version_name(ld, li, 1, "*global*");

	if (verneed != NULL)
		_load_verneed_section(ld, li, e, verneed);

	if (verdef != NULL)
		_load_verdef_section(ld, li, e, verdef);
}

void
ld_symver_create_verneed_section(struct ld *ld)
{
	struct ld_input *li;
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_data_buffer *odb;
	struct ld_symver_verdef *svd;
	struct ld_symver_verneed *svn;
	struct ld_symver_vda *sda;
	struct ld_symver_vna *sna;
	char verneed_name[] = ".gnu.version_r";
	Elf_Verneed *vn;
	Elf_Vernaux *vna;
	uint8_t *buf, *buf2, *end;
	size_t sz;

	lo = ld->ld_output;
	assert(lo != NULL);
	assert(lo->lo_dynstr != NULL);

	/*
	 * Create .gnu.version_r section.
	 */
	HASH_FIND_STR(lo->lo_ostbl, verneed_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, verneed_name,
		    SHF_ALLOC);
	os->os_type = SHT_GNU_verneed;
	os->os_flags = SHF_ALLOC;
	os->os_entsize = 0;
	if (lo->lo_ec == ELFCLASS32)
		os->os_align = 4;
	else
		os->os_align = 8;

	if ((os->os_link = strdup(".dynstr")) == NULL)
		ld_fatal_std(ld, "strdup");

	lo->lo_verneed = os;

	/*
	 * Build Verneed/Vernaux structures.
	 */
	sz = 0;
	STAILQ_FOREACH(li, &ld->ld_lilist, li_next) {
		if (li->li_type != LIT_DSO || li->li_dso_refcnt == 0 ||
		    li->li_verdef == NULL)
			continue;

		svn = NULL;
		STAILQ_FOREACH(svd, li->li_verdef, svd_next) {
			if (svd->svd_flags & VER_FLG_BASE)
				continue;

			/* Skip version definition that is never ref'ed. */
			if (svd->svd_ref == 0)
				continue;

			/* Invalid Verdef? */
			if ((sda = STAILQ_FIRST(&svd->svd_aux)) == NULL)
				continue;

			if (lo->lo_vnlist == NULL) {
				lo->lo_vnlist = calloc(1,
				    sizeof(*lo->lo_vnlist));
				if (lo->lo_vnlist == NULL)
					ld_fatal_std(ld, "calloc");
				STAILQ_INIT(lo->lo_vnlist);
			}

			/* Allocate Verneed entry. */
			if (svn == NULL) {
				svn = _alloc_verneed(ld, li, lo->lo_vnlist);
				svn->svn_version = VER_NEED_CURRENT;
				svn->svn_cnt = 0;
				svn->svn_fileindex =
				    ld_strtab_insert_no_suffix(ld,
					ld->ld_dynstr, svn->svn_file);
				sz += sizeof(Elf_Verneed);
				lo->lo_verneed_num++;
			}

			/* Allocate Vernaux entry. */
			sna = _alloc_vna(ld, sda->sda_name, svn);
			sna->sna_other = lo->lo_version_index++;
			sna->sna_nameindex = ld_strtab_insert_no_suffix(ld,
			    ld->ld_dynstr, sna->sna_name);
			/* TODO: flags? VER_FLG_WEAK */
			svn->svn_cnt++;

			sz += sizeof(Elf_Vernaux);

			/*
			 * Store the index in Verdef structure, so later we can
			 * quickly find the version index for a dynamic symbol,
			 * when we build the .gnu.version section.
			 */
			svd->svd_ndx_output = sna->sna_other;
		}
	}

	if (lo->lo_verneed_num == 0)
		return;

	/* Store the number of verneed entries in the sh_info field. */
	os->os_info_val = lo->lo_verneed_num;

	/*
	 * Write Verneed/Vernaux structures.
	 */
	if ((buf = malloc(sz)) == NULL)
		ld_fatal_std(ld, "malloc");

	if ((odb = calloc(1, sizeof(*odb))) == NULL)
		ld_fatal_std(ld, "calloc");

	odb->odb_buf = buf;
	odb->odb_size = sz;
	odb->odb_align = os->os_align;
	odb->odb_type = ELF_T_VNEED; /* enable libelf translation */

	end = buf + sz;
	vn = NULL;
	STAILQ_FOREACH(svn, lo->lo_vnlist, svn_next){
		vn = (Elf_Verneed *) (uintptr_t) buf;
		vn->vn_version = VER_NEED_CURRENT;
		vn->vn_cnt = svn->svn_cnt;
		vn->vn_file = svn->svn_fileindex;
		vn->vn_aux = sizeof(Elf_Verneed);
		vn->vn_next = sizeof(Elf_Verneed) +
		    svn->svn_cnt * sizeof(Elf_Vernaux);

		/*
		 * Write Vernaux entries.
		 */
		buf2 = buf + sizeof(Elf_Verneed);
		vna = NULL;
		STAILQ_FOREACH(sna, &svn->svn_aux, sna_next) {
			vna = (Elf_Vernaux *) (uintptr_t) buf2;
			vna->vna_hash = sna->sna_hash;
			vna->vna_flags = 0; /* TODO: VER_FLG_WEAK? */
			vna->vna_other = sna->sna_other;
			vna->vna_name = sna->sna_nameindex;
			vna->vna_next = sizeof(Elf_Vernaux);
			buf2 += sizeof(Elf_Vernaux);
		}

		/* Set last Vernaux entry's vna_next to 0. */
		if (vna != NULL)
			vna->vna_next = 0;

		buf += vn->vn_next;
	}

	/* Set last Verneed entry's vn_next to 0 */
	if (vn != NULL)
		vn->vn_next = 0;

	assert(buf == end);

	(void) ld_output_create_section_element(ld, os, OET_DATA_BUFFER,
	    odb, NULL);
}

void
ld_symver_create_verdef_section(struct ld *ld)
{
	struct ld_script *lds;
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_data_buffer *odb;
	struct ld_script_version_node *ldvn;
	char verdef_name[] = ".gnu.version_d";
	Elf_Verdef *vd;
	Elf_Verdaux *vda;
	uint8_t *buf, *end;
	char *soname;
	size_t sz;

	lo = ld->ld_output;
	assert(lo != NULL);
	assert(lo->lo_dynstr != NULL);

	lds = ld->ld_scp;
	if (STAILQ_EMPTY(&lds->lds_vn))
		return;

	/*
	 * Create .gnu.version_d section.
	 */
	HASH_FIND_STR(lo->lo_ostbl, verdef_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, verdef_name,
		    SHF_ALLOC);
	os->os_type = SHT_GNU_verdef;
	os->os_flags = SHF_ALLOC;
	os->os_entsize = 0;
	if (lo->lo_ec == ELFCLASS32)
		os->os_align = 4;
	else
		os->os_align = 8;

	if ((os->os_link = strdup(".dynstr")) == NULL)
		ld_fatal_std(ld, "strdup");

	lo->lo_verdef = os;

	/*
	 * Calculate verdef section size: .gnu.version_d section consists
	 * of one file version entry and several symbol version definition
	 * entries (with corresponding) auxiliary entries.
	 */
	lo->lo_verdef_num = 1;
	sz = sizeof(Elf_Verdef) + sizeof(Elf_Verdaux);
	STAILQ_FOREACH(ldvn, &lds->lds_vn, ldvn_next) {
		sz += sizeof(Elf_Verdef) + sizeof(Elf_Verdaux);
		if (ldvn->ldvn_dep != NULL)
			sz += sizeof(Elf_Verdaux);
		lo->lo_verdef_num++;
	}

	/* Store the number of verdef entries in the sh_info field. */
	os->os_info_val = lo->lo_verdef_num;

	/* Allocate buffer for Verdef/Verdaux entries. */
	if ((buf = malloc(sz)) == NULL)
		ld_fatal_std(ld, "malloc");

	end = buf + sz;

	if ((odb = calloc(1, sizeof(*odb))) == NULL)
		ld_fatal_std(ld, "calloc");

	odb->odb_buf = buf;
	odb->odb_size = sz;
	odb->odb_align = os->os_align;
	odb->odb_type = ELF_T_VDEF; /* enable libelf translation */

	/*
	 * Set file version name to `soname' if it is provided,
	 * otherwise set version name to output file name.
	 */
	if (ld->ld_soname != NULL)
		soname = ld->ld_soname;
	else {
		if ((soname = strrchr(ld->ld_output_file, '/')) == NULL)
			soname = ld->ld_output_file;
		else
			soname++;
	}

	/* Write file version entry. */
	vd = (Elf_Verdef *) (uintptr_t) buf;
	vd->vd_version = VER_DEF_CURRENT;
	vd->vd_flags |= VER_FLG_BASE;
	vd->vd_ndx = 1;
	vd->vd_cnt = 1;
	vd->vd_hash = elf_hash(soname);
	vd->vd_aux = sizeof(Elf_Verdef);
	vd->vd_next = sizeof(Elf_Verdef) + sizeof(Elf_Verdaux);
	buf += sizeof(Elf_Verdef);

	/* Write file version auxiliary entry. */
	vda = (Elf_Verdaux *) (uintptr_t) buf;
	vda->vda_name = ld_strtab_insert_no_suffix(ld, ld->ld_dynstr,
	    soname);
	vda->vda_next = 0;
	buf += sizeof(Elf_Verdaux);
	
	/* Write symbol version definition entries. */
	STAILQ_FOREACH(ldvn, &lds->lds_vn, ldvn_next) {
		vd = (Elf_Verdef *) (uintptr_t) buf;
		vd->vd_version = VER_DEF_CURRENT;
		vd->vd_flags = 0;
		vd->vd_ndx = lo->lo_version_index++;
		vd->vd_cnt = (ldvn->ldvn_dep == NULL) ? 1 : 2;
		vd->vd_hash = elf_hash(ldvn->ldvn_name);
		vd->vd_aux = sizeof(Elf_Verdef);
		if (STAILQ_NEXT(ldvn, ldvn_next) == NULL)
			vd->vd_next = 0;
		else
			vd->vd_next = sizeof(Elf_Verdef) + 
			    ((ldvn->ldvn_dep == NULL) ? 1 : 2) *
				sizeof(Elf_Verdaux);
		buf += sizeof(Elf_Verdef);

		/* Write version name auxiliary entry. */
		vda = (Elf_Verdaux *) (uintptr_t) buf;
		vda->vda_name = ld_strtab_insert_no_suffix(ld, ld->ld_dynstr,
		    ldvn->ldvn_name);
		vda->vda_next = ldvn->ldvn_dep == NULL ? 0 :
		    sizeof(Elf_Verdaux);
		buf += sizeof(Elf_Verdaux);

		if (ldvn->ldvn_dep == NULL)
			continue;
		
		/* Write version dependency auxiliary entry. */
		vda = (Elf_Verdaux *) (uintptr_t) buf;
		vda->vda_name = ld_strtab_insert_no_suffix(ld, ld->ld_dynstr,
		    ldvn->ldvn_dep);
		vda->vda_next = 0;
		buf += sizeof(Elf_Verdaux);
	}

	assert(buf == end);

	(void) ld_output_create_section_element(ld, os, OET_DATA_BUFFER,
	    odb, NULL);
}

void
ld_symver_create_versym_section(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_data_buffer *odb;
	struct ld_symbol *lsb;
	char versym_name[] = ".gnu.version";
	uint16_t *buf;
	size_t sz;
	int i;

	lo = ld->ld_output;
	assert(lo != NULL);
	assert(lo->lo_dynsym != NULL);
	assert(ld->ld_dynsym != NULL);

	/*
	 * Create .gnu.version section.
	 */
	HASH_FIND_STR(lo->lo_ostbl, versym_name, os);
	if (os == NULL)
		os = ld_layout_insert_output_section(ld, versym_name,
		    SHF_ALLOC);
	os->os_type = SHT_GNU_versym;
	os->os_flags = SHF_ALLOC;
	os->os_entsize = 2;
	os->os_align = 2;

	if ((os->os_link = strdup(".dynsym")) == NULL)
		ld_fatal_std(ld, "strdup");

	lo->lo_versym = os;

	/*
	 * Write versym table.
	 */
	sz = ld->ld_dynsym->sy_size * sizeof(*buf);
	if ((buf = malloc(sz)) == NULL)
		ld_fatal_std(ld, "malloc");

	buf[0] = 0;		/* special index 0 symbol */
	i = 1;
	STAILQ_FOREACH(lsb, ld->ld_dyn_symbols, lsb_dyn) {
		/*
		 * Assign version index according to the following rules:
		 *
		 * 1. If the symbol is local, the version is *local*.
		 *
		 * 2. If the symbol is defined in shared libraries and there
		 *    exists a version definition for this symbol, use the
		 *    version defined by the shared library.
		 *
		 * 3. If the symbol is defined in regular objects and the
		 *    linker creates a shared library, use the version
		 *    defined in the version script, if provided.
		 *
		 * 4. Otherwise, the version is *global*.
		 */
		if (lsb->lsb_bind == STB_LOCAL)
			buf[i] = 0; /* Version is *local* */
		else if (lsb->lsb_vd != NULL)
			buf[i] = lsb->lsb_vd->svd_ndx_output;
		else if (ld->ld_dso && ld_symbols_in_regular(lsb))
			buf[i] = ld_symver_search_version_script(ld, lsb);
		else {
			buf[i] = 1; /* Version is *global* */
		}
		i++;
	}
	assert((size_t) i == ld->ld_dynsym->sy_size);

	if ((odb = calloc(1, sizeof(*odb))) == NULL)
		ld_fatal_std(ld, "calloc");

	odb->odb_buf = (void *) buf;
	odb->odb_size = sz;
	odb->odb_align = os->os_align;
	odb->odb_type = ELF_T_HALF; /* enable libelf translation */

	(void) ld_output_create_section_element(ld, os, OET_DATA_BUFFER,
	    odb, NULL);
}

void
ld_symver_add_verdef_refcnt(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_symbol_defver *dv;
	struct ld_symver_verdef *svd;
	struct ld_symver_vda *sda;
	struct ld_input *li;
	const char *ver;

	li = lsb->lsb_input;
	assert(li != NULL);

	if (li->li_verdef == NULL)
		return;

	if (lsb->lsb_ver != NULL)
		ver = lsb->lsb_ver;
	else {
		HASH_FIND_STR(ld->ld_defver, lsb->lsb_name, dv);
		if (dv == NULL || dv->dv_ver == NULL)
			return;
		ver = dv->dv_ver;
	}

	STAILQ_FOREACH(svd, li->li_verdef, svd_next) {
		if (svd->svd_flags & VER_FLG_BASE)
			continue;

		/* Invalid Verdef? */
		if ((sda = STAILQ_FIRST(&svd->svd_aux)) == NULL)
			continue;

		if (!strcmp(ver, sda->sda_name))
			break;
	}

	if (svd != NULL) {
		svd->svd_ref++;
		lsb->lsb_vd = svd;
	}
}

static void
_add_version_name(struct ld *ld, struct ld_input *li, int ndx,
    const char *name)
{
	int i;

	assert(name != NULL);

	if (ndx <= 1)
		return;

	if (li->li_vername == NULL) {
		li->li_vername_sz = 10;
		li->li_vername = calloc(li->li_vername_sz,
		    sizeof(*li->li_vername));
		if (li->li_vername == NULL)
			ld_fatal_std(ld, "calloc");
	}

	if ((size_t) ndx >= li->li_vername_sz) {
		li->li_vername = realloc(li->li_vername,
		    sizeof(*li->li_vername) * li->li_vername_sz * 2);
		if (li->li_vername == NULL)
			ld_fatal_std(ld, "realloc");
		for (i = li->li_vername_sz; (size_t) i < li->li_vername_sz * 2;
		     i++)
			li->li_vername[i] = NULL;
		li->li_vername_sz *= 2;
	}

	if (li->li_vername[ndx] == NULL) {
		li->li_vername[ndx] = strdup(name);
		if (li->li_vername[ndx] == NULL)
			ld_fatal_std(ld, "strdup");
	}
}

static struct ld_symver_vna *
_alloc_vna(struct ld *ld, const char *name, struct ld_symver_verneed *svn)
{
	struct ld_symver_vna *sna;

	assert(name != NULL);

	if ((sna = calloc(1, sizeof(*sna))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((sna->sna_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	sna->sna_hash = (uint32_t) elf_hash(sna->sna_name);

	if (svn != NULL)
		STAILQ_INSERT_TAIL(&svn->svn_aux, sna, sna_next);

	return (sna);
}

static struct ld_symver_vda *
_alloc_vda(struct ld *ld, const char *name, struct ld_symver_verdef *svd)
{
	struct ld_symver_vda *sda;

	if ((sda = calloc(1, sizeof(*sda))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((sda->sda_name = strdup(name)) == NULL)
		ld_fatal_std(ld, "strdup");

	if (svd != NULL)
		STAILQ_INSERT_TAIL(&svd->svd_aux, sda, sda_next);

	return (sda);
}

static struct ld_symver_verneed *
_alloc_verneed(struct ld *ld, struct ld_input *li,
    struct ld_symver_verneed_head *head)
{
	struct ld_symver_verneed *svn;
	const char *bn;

	if ((svn = calloc(1, sizeof(*svn))) == NULL)
		ld_fatal_std(ld, "calloc");

	if (li->li_soname != NULL)
		bn = li->li_soname;
	else {
		if ((bn = strrchr(li->li_name, '/')) == NULL)
			bn = li->li_name;
		else
			bn++;
	}

	if ((svn->svn_file = strdup(bn)) == NULL)
		ld_fatal_std(ld, "strdup");

	STAILQ_INIT(&svn->svn_aux);

	if (head != NULL)
		STAILQ_INSERT_TAIL(head, svn, svn_next);

	return (svn);
}

static struct ld_symver_verdef *
_alloc_verdef(struct ld *ld, struct ld_symver_verdef_head *head)
{
	struct ld_symver_verdef *svd;

	if ((svd = calloc(1, sizeof(*svd))) == NULL)
		ld_fatal_std(ld, "calloc");

	STAILQ_INIT(&svd->svd_aux);

	if (head != NULL)
		STAILQ_INSERT_TAIL(head, svd, svd_next);

	return (svd);
}

static void
_load_verneed_section(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *verneed)
{
	Elf_Data *d_vn;
	Elf_Verneed *vn;
	Elf_Vernaux *vna;
	GElf_Shdr sh_vn;
	uint8_t *buf, *end, *buf2;
	char *name;
	int elferr, i;

	if (gelf_getshdr(verneed, &sh_vn) != &sh_vn)
		ld_fatal(ld, "%s: gelf_getshdr failed: %s", li->li_name,
		    elf_errmsg(-1));

	(void) elf_errno();
	if ((d_vn = elf_getdata(verneed, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_fatal(ld, "%s: elf_getdata failed: %s", li->li_name,
			    elf_errmsg(elferr));
		return;
	}
	if (d_vn->d_size == 0)
		return;

	buf = d_vn->d_buf;
	end = buf + d_vn->d_size;
	while (buf + sizeof(Elf_Verneed) <= end) {
		vn = (Elf_Verneed *) (uintptr_t) buf;
		buf2 = buf + vn->vn_aux;
		i = 0;
		while (buf2 + sizeof(Elf_Vernaux) <= end && i < vn->vn_cnt) {
			vna = (Elf32_Vernaux *) (uintptr_t) buf2;
			name = elf_strptr(e, sh_vn.sh_link,
			    vna->vna_name);
			if (name != NULL)
				_add_version_name(ld, li, (int) vna->vna_other,
				    name);
			buf2 += vna->vna_next;
			i++;
		}
		if (vn->vn_next == 0)
			break;
		buf += vn->vn_next;
	}
}

static void
_load_verdef_section(struct ld *ld, struct ld_input *li, Elf *e,
    Elf_Scn *verdef)
{
	struct ld_symver_verdef *svd;
	Elf_Data *d_vd;
	Elf_Verdef *vd;
	Elf_Verdaux *vda;
	GElf_Shdr sh_vd;
	uint8_t *buf, *end, *buf2;
	char *name;
	int elferr, i;

	if (gelf_getshdr(verdef, &sh_vd) != &sh_vd)
		ld_fatal(ld, "%s: gelf_getshdr failed: %s", li->li_name,
		    elf_errmsg(-1));

	(void) elf_errno();
	if ((d_vd = elf_getdata(verdef, NULL)) == NULL) {
		elferr = elf_errno();
		if (elferr != 0)
			ld_fatal(ld, "%s: elf_getdata failed: %s", li->li_name,
			    elf_errmsg(elferr));
		return;
	}
	if (d_vd->d_size == 0)
		return;

	buf = d_vd->d_buf;
	end = buf + d_vd->d_size;
	while (buf + sizeof(Elf_Verdef) <= end) {
		vd = (Elf_Verdef *) (uintptr_t) buf;
		svd = _load_verdef(ld, li, vd);
		buf2 = buf + vd->vd_aux;
		i = 0;
		while (buf2 + sizeof(Elf_Verdaux) <= end && i < vd->vd_cnt) {
			vda = (Elf_Verdaux *) (uintptr_t) buf2;
			name = elf_strptr(e, sh_vd.sh_link, vda->vda_name);
			if (name != NULL) {
				_add_version_name(ld, li, (int) vd->vd_ndx,
				    name);
				(void) _alloc_vda(ld, name, svd);
			}
			if (vda->vda_next == 0)
				break;
			buf2 += vda->vda_next;
			i++;
		}
		if (vd->vd_next == 0)
			break;
		buf += vd->vd_next;
	}
}

static struct ld_symver_verdef *
_load_verdef(struct ld *ld, struct ld_input *li, Elf_Verdef *vd)
{
	struct ld_symver_verdef *svd;

	if (li->li_verdef == NULL) {
		if ((li->li_verdef = calloc(1, sizeof(*li->li_verdef))) ==
		    NULL)
			ld_fatal_std(ld, "calloc");
		STAILQ_INIT(li->li_verdef);
	}

	svd = _alloc_verdef(ld, li->li_verdef);
	svd->svd_version = vd->vd_version;
	svd->svd_flags = vd->vd_flags;
	svd->svd_ndx = vd->vd_ndx;
	svd->svd_cnt = vd->vd_cnt;
	svd->svd_hash = vd->vd_hash;

	return (svd);
}

uint16_t
ld_symver_search_version_script(struct ld *ld, struct ld_symbol *lsb)
{
	struct ld_script *lds;
	struct ld_script_version_node *ldvn;
	struct ld_script_version_entry *ldve, *ldve_g;
	uint16_t ndx, ret_ndx, ret_ndx_g;

	/* If the symbol version index was known, return it directly. */
	if (lsb->lsb_vndx_known)
		return (lsb->lsb_vndx);

	/* The symbol version index will be known after searching. */
	lsb->lsb_vndx_known = 1;

	lds = ld->ld_scp;

	/* If there isn't a version script, the default version is *global* */
	if (STAILQ_EMPTY(&lds->lds_vn)) {
		lsb->lsb_vndx = 1;
		return (1);
	}

	/* Search for a match in the version patterns. */
	ndx = 2;
	ldve_g = NULL;
	ret_ndx_g = 0;
	STAILQ_FOREACH(ldvn, &lds->lds_vn, ldvn_next) {
		STAILQ_FOREACH(ldve, ldvn->ldvn_e, ldve_next) {
			assert(ldve->ldve_sym != NULL);
			if (fnmatch(ldve->ldve_sym, lsb->lsb_name, 0) == 0) {
				if (ldve->ldve_local)
					ret_ndx = 0;
				else if (ldvn->ldvn_name != NULL)
					ret_ndx = ndx;
				else
					ret_ndx = 1;

				/*
				 * If the version name is a globbing pattern,
				 * we only consider it is a match when there
				 * doesn't exist a exact match.
				 */
				if (ldve->ldve_glob) {
					if (ldve_g == NULL) {
						ldve_g = ldve;
						ret_ndx_g = ret_ndx;
					}
				} else {
					lsb->lsb_vndx = ret_ndx;
					return (ret_ndx);
				}
			}
		}
		if (ldvn->ldvn_name != NULL)
			ndx++;
	}

	/* There is no exact match, check if there is a globbing match. */
	if (ldve_g != NULL) {
		lsb->lsb_vndx = ret_ndx_g;
		return (ret_ndx_g);
	}

	/*
	 * Symbol doesn't match any version definition, set version
	 * to *global*.
	 */
	lsb->lsb_vndx = 1;
	return (1);
}
