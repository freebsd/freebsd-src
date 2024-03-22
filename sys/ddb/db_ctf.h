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

#ifndef _DDB_DB_CTF_H_
#define _DDB_DB_CTF_H_

#include <sys/types.h>
#include <sys/ctf.h>
#include <sys/linker.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

#define DB_CTF_INVALID_OFF 0xffffffff

struct db_ctf_sym_data {
	linker_ctf_t lc;
	Elf_Sym *sym;
};

typedef struct db_ctf_sym_data *db_ctf_sym_data_t;

/*
 * Routines for finding symbols and CTF info accross all loaded linker files.
 */
int db_ctf_find_symbol(const char *name, db_ctf_sym_data_t sd);
struct ctf_type_v3 *db_ctf_find_typename(db_ctf_sym_data_t sd,
    const char *typename);
bool db_ctf_lookup_typename(linker_ctf_t *lc, const char *typename);

/*
 * Routines for working with CTF data.
 */
struct ctf_type_v3 *db_ctf_sym_to_type(db_ctf_sym_data_t sd);
const char *db_ctf_stroff_to_str(db_ctf_sym_data_t sd, uint32_t off);
struct ctf_type_v3 *db_ctf_typename_to_type(linker_ctf_t *lc, const char *name);
struct ctf_type_v3 *db_ctf_typeid_to_type(db_ctf_sym_data_t sd,
    uint32_t typeid);

#endif /* !_DDB_DB_CTF_H_ */
