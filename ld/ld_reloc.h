/*-
 * Copyright (c) 2012,2013 Kai Wang
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
 * $Id: ld_reloc.h 2898 2013-01-15 23:05:59Z kaiwang27 $
 */

struct ld_symbol;
struct ld_input_section;
struct ld_output_section;

struct ld_reloc_entry {
	struct ld_input_section *lre_tis; /* input section to apply to */
	struct ld_symbol *lre_sym;	/* reloc symbol */
	uint64_t lre_type;		/* reloc type */
	uint64_t lre_offset;		/* reloc offset */
	uint64_t lre_addend;		/* reloc addend */
	STAILQ_ENTRY(ld_reloc_entry) lre_next; /* next reloc */
};

STAILQ_HEAD(ld_reloc_entry_head, ld_reloc_entry);

enum ld_tls_relax {
	TLS_RELAX_NONE,
	TLS_RELAX_INIT_EXEC,
	TLS_RELAX_LOCAL_EXEC
};

void	ld_reloc_create_entry(struct ld *, const char *,
    struct ld_input_section *, uint64_t, struct ld_symbol *, uint64_t,
    int64_t);
void	ld_reloc_deferred_scan(struct ld *);
void	ld_reloc_finalize(struct ld *, struct ld_output_section *);
void	ld_reloc_finalize_dynamic(struct ld *, struct ld_output *,
    struct ld_output_section *);
void	ld_reloc_gc_sections(struct ld *);
void	ld_reloc_join(struct ld *, struct ld_output_section *,
    struct ld_input_section *);
void	ld_reloc_sort(struct ld *, struct ld_output_section *);
void	ld_reloc_load(struct ld *);
void	ld_reloc_process_input_section(struct ld *, struct ld_input_section *,
    void *);
int	ld_reloc_require_plt(struct ld *, struct ld_reloc_entry *);
int	ld_reloc_require_copy_reloc(struct ld *, struct ld_reloc_entry *);
int	ld_reloc_require_dynamic_reloc(struct ld *, struct ld_reloc_entry *);
int	ld_reloc_require_glob_dat(struct ld *, struct ld_reloc_entry *);
int	ld_reloc_relative_relax(struct ld *, struct ld_reloc_entry *);
void	*ld_reloc_serialize(struct ld *, struct ld_output_section *, size_t *);
