/*-
 * Copyright (c) 2011,2012 Kai Wang
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
 * $Id: ld_arch.h 3281 2015-12-11 21:39:23Z kaiwang27 $
 */

#define	MAX_ARCH_NAME_LEN	64
#define	MAX_TARGET_NAME_LEN	128

struct ld_input_section;
struct ld_reloc_entry;

struct ld_arch {
	char name[MAX_ARCH_NAME_LEN + 1];
	char *script;
	const char *interp;
	uint64_t (*get_max_page_size)(struct ld *);
	uint64_t (*get_common_page_size)(struct ld *);
	void (*scan_reloc)(struct ld *, struct ld_input_section *,
	    struct ld_reloc_entry *);
	void (*adjust_reloc)(struct ld *, struct ld_input_section *,
	    struct ld_reloc_entry *, struct ld_symbol *, uint8_t *);
	void (*process_reloc)(struct ld *, struct ld_input_section *,
	    struct ld_reloc_entry *, struct ld_symbol *, uint8_t *);
	void (*finalize_reloc)(struct ld *, struct ld_input_section *,
	    struct ld_reloc_entry *);
	void (*finalize_got_and_plt)(struct ld *);
	void (*merge_flags)(struct ld *, unsigned flags);
	int (*is_absolute_reloc)(uint64_t);
	int (*is_relative_reloc)(uint64_t);
	unsigned char reloc_is_64bit;
	unsigned char reloc_is_rela;
	size_t reloc_entsize;
	unsigned flags;			/* processor-specific flags */
	UT_hash_handle hh;
	struct ld_arch *alias;
};

void	ld_arch_init(struct ld *);
int	ld_arch_equal(struct ld_arch *, struct ld_arch *);
struct ld_arch *ld_arch_find(struct ld *, char *);
struct ld_arch *ld_arch_guess_arch_name(struct ld *, int, int);
void	ld_arch_set(struct ld *, char *);
void	ld_arch_set_from_target(struct ld *);
void	ld_arch_verify(struct ld *, const char *, int, int, unsigned);
