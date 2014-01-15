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
 *
 * $Id: ld_symver.h 2882 2013-01-09 22:47:04Z kaiwang27 $
 */

struct ld_symver_vna {
	char *sna_name;
	uint32_t sna_hash;
	uint16_t sna_flags;
	uint16_t sna_other;
	uint32_t sna_nameindex;
	STAILQ_ENTRY(ld_symver_vna) sna_next;
};

STAILQ_HEAD(ld_symver_vna_head, ld_symver_vna);

struct ld_symver_verneed {
	char *svn_file;
	uint16_t svn_version;
	uint16_t svn_cnt;
	uint32_t svn_fileindex;
	struct ld_symver_vna_head svn_aux;
	STAILQ_ENTRY(ld_symver_verneed) svn_next;
};

STAILQ_HEAD(ld_symver_verneed_head, ld_symver_verneed);

struct ld_symver_vda {
	char *sda_name;
	uint32_t sda_nameindex;
	STAILQ_ENTRY(ld_symver_vda) sda_next;
};

STAILQ_HEAD(ld_symver_vda_head, ld_symver_vda);

struct ld_symver_verdef {
	uint16_t svd_version;
	uint16_t svd_flags;
	uint16_t svd_ndx;
	uint16_t svd_ndx_output;
	uint16_t svd_cnt;
	uint32_t svd_hash;
	uint64_t svd_ref;
	struct ld_symver_vda_head svd_aux;
	STAILQ_ENTRY(ld_symver_verdef) svd_next;
};

STAILQ_HEAD(ld_symver_verdef_head, ld_symver_verdef);

void	ld_symver_load_symbol_version_info(struct ld *, struct ld_input *,
    Elf *, Elf_Scn *, Elf_Scn *, Elf_Scn *);
void	ld_symver_create_verdef_section(struct ld *);
void	ld_symver_create_verneed_section(struct ld *);
void	ld_symver_create_versym_section(struct ld *);
void	ld_symver_add_verdef_refcnt(struct ld *, struct ld_symbol *);
uint16_t ld_symver_search_version_script(struct ld *, struct ld_symbol *);
