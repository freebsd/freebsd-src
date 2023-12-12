/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#ifndef _EF_H_
#define _EF_H_

#define	EFT_KLD		1
#define	EFT_KERNEL	2

#define EF_RELOC_REL	1
#define EF_RELOC_RELA	2

#define EF_GET_TYPE(ef) \
    (ef)->ef_ops->get_type((ef)->ef_ef)
#define EF_CLOSE(ef) \
    (ef)->ef_ops->close((ef)->ef_ef)
#define EF_READ(ef, offset, len, dest) \
    (ef)->ef_ops->read((ef)->ef_ef, offset, len, dest)
#define EF_READ_ENTRY(ef, offset, len, ptr) \
    (ef)->ef_ops->read_entry((ef)->ef_ef, offset, len, ptr)
#define EF_SEG_READ(ef, offset, len, dest) \
    (ef)->ef_ops->seg_read((ef)->ef_ef, offset, len, dest)
#define EF_SEG_READ_REL(ef, offset, len, dest) \
    (ef)->ef_ops->seg_read_rel((ef)->ef_ef, offset, len, dest)
#define EF_SEG_READ_STRING(ef, offset, len, dest) \
    (ef)->ef_ops->seg_read_string((ef)->ef_ef, offset, len, dest)
#define EF_SEG_READ_ENTRY(ef, offset, len, ptr) \
    (ef)->ef_ops->seg_read_entry((ef)->kf_ef, offset, len, ptr)
#define EF_SEG_READ_ENTRY_REL(ef, offset, len, ptr) \
    (ef)->ef_ops->seg_read_entry_rel((ef)->ef_ef, offset, len, ptr)
#define EF_SYMADDR(ef, symidx) \
    (ef)->ef_ops->symaddr((ef)->ef_ef, symidx)
#define EF_LOOKUP_SET(ef, name, startp, stopp, countp) \
    (ef)->ef_ops->lookup_set((ef)->ef_ef, name, startp, stopp, countp)
#define EF_LOOKUP_SYMBOL(ef, name, sym) \
    (ef)->ef_ops->lookup_symbol((ef)->ef_ef, name, sym)

/* XXX, should have a different name. */
typedef struct ef_file *elf_file_t;

struct elf_file_ops {
	int (*get_type)(elf_file_t ef);
	int (*close)(elf_file_t ef);
	int (*read)(elf_file_t ef, Elf_Off offset, size_t len, void* dest);
	int (*read_entry)(elf_file_t ef, Elf_Off offset, size_t len,
	    void **ptr);
	int (*seg_read)(elf_file_t ef, Elf_Off offset, size_t len, void *dest);
	int (*seg_read_rel)(elf_file_t ef, Elf_Off offset, size_t len,
	    void *dest);
	int (*seg_read_string)(elf_file_t, Elf_Off offset, size_t len,
	    char *dest);
	int (*seg_read_entry)(elf_file_t ef, Elf_Off offset, size_t len,
	    void**ptr);
	int (*seg_read_entry_rel)(elf_file_t ef, Elf_Off offset, size_t len,
	    void**ptr);
	Elf_Addr (*symaddr)(elf_file_t ef, Elf_Size symidx);
	int (*lookup_set)(elf_file_t ef, const char *name, long *startp,
	    long *stopp, long *countp);
	int (*lookup_symbol)(elf_file_t ef, const char* name, Elf_Sym** sym);
};

struct elf_file {
	elf_file_t ef_ef;
	struct elf_file_ops *ef_ops;
};

__BEGIN_DECLS
int ef_open(const char *filename, struct elf_file *ef, int verbose);
int ef_obj_open(const char *filename, struct elf_file *ef, int verbose);
int ef_reloc(struct elf_file *ef, const void *reldata, int reltype,
    Elf_Off relbase, Elf_Off dataoff, size_t len, void *dest);
__END_DECLS

#endif /* _EF_H_*/
