/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Bojan Novković <bnovkov@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/linker.h>

#include <machine/stdarg.h>

#include <ddb/ddb.h>
#include <ddb/db_ctf.h>
#include <ddb/db_lex.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>

#define DB_PPRINT_DEFAULT_DEPTH 1

static void db_pprint_type(db_addr_t addr, struct ctf_type_v3 *type,
    u_int depth);

static u_int max_depth = DB_PPRINT_DEFAULT_DEPTH;
static struct db_ctf_sym_data sym_data;

/*
 * Pretty-prints a CTF_INT type.
 */
static inline void
db_pprint_int(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{
	uint32_t data;
	size_t type_struct_size;

	type_struct_size = (type->ctt_size == CTF_V3_LSIZE_SENT) ?
		sizeof(struct ctf_type_v3) :
		sizeof(struct ctf_stype_v3);

	data = db_get_value((db_expr_t)type + type_struct_size,
		sizeof(uint32_t), 0);
	u_int bits = CTF_INT_BITS(data);
	boolean_t sign = !!(CTF_INT_ENCODING(data) & CTF_INT_SIGNED);

	if (db_pager_quit) {
		return;
	}
	if (bits > 64) {
		db_printf("Invalid size '%d' found for integer type\n", bits);
		return;
	}
	db_printf("0x%lx",
	    db_get_value(addr, (bits / 8) ? (bits / 8) : 1, sign));
}

/*
 * Pretty-prints a struct. Nested structs are pretty-printed up 'depth' nested
 * levels.
 */
static inline void
db_pprint_struct(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{
	size_t type_struct_size;
	size_t struct_size;
	struct ctf_type_v3 *mtype;
	const char *mname;
	db_addr_t maddr;
	u_int vlen;

	type_struct_size = (type->ctt_size == CTF_V3_LSIZE_SENT) ?
		sizeof(struct ctf_type_v3) :
		sizeof(struct ctf_stype_v3);
	struct_size = ((type->ctt_size == CTF_V3_LSIZE_SENT) ?
		CTF_TYPE_LSIZE(type) :
		type->ctt_size);
	vlen = CTF_V3_INFO_VLEN(type->ctt_info);

	if (db_pager_quit) {
		return;
	}
	if (depth > max_depth) {
		db_printf("{ ... }");
		return;
	}
	db_printf("{\n");

	if (struct_size < CTF_V3_LSTRUCT_THRESH) {
		struct ctf_member_v3 *mp, *endp;

		mp = (struct ctf_member_v3 *)((db_addr_t)type +
		    type_struct_size);
		endp = mp + vlen;
		for (; mp < endp; mp++) {
			if (db_pager_quit) {
				return;
			}
			mtype = db_ctf_typeid_to_type(&sym_data, mp->ctm_type);
			maddr = addr + mp->ctm_offset;
			mname = db_ctf_stroff_to_str(&sym_data, mp->ctm_name);
			db_indent = depth;
			if (mname != NULL) {
				db_iprintf("%s = ", mname);
			} else {
				db_iprintf("");
			}

			db_pprint_type(maddr, mtype, depth + 1);
			db_printf(",\n");
		}
	} else {
		struct ctf_lmember_v3 *mp, *endp;

		mp = (struct ctf_lmember_v3 *)((db_addr_t)type +
		    type_struct_size);
		endp = mp + vlen;
		for (; mp < endp; mp++) {
			if (db_pager_quit) {
				return;
			}
			mtype = db_ctf_typeid_to_type(&sym_data, mp->ctlm_type);
			maddr = addr + CTF_LMEM_OFFSET(mp);
			mname = db_ctf_stroff_to_str(&sym_data, mp->ctlm_name);
			db_indent = depth;
			if (mname != NULL) {
				db_iprintf("%s = ", mname);
			} else {
				db_iprintf("");
			}

			db_pprint_type(maddr, mtype, depth + 1);
			db_printf(",");
		}
	}
	db_indent = depth - 1;
	db_iprintf("}");
}

/*
 * Pretty-prints an array. Each array member is printed out in a separate line
 * indented with 'depth' spaces.
 */
static inline void
db_pprint_arr(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{
	struct ctf_type_v3 *elem_type;
	struct ctf_array_v3 *arr;
	db_addr_t elem_addr, end;
	size_t type_struct_size;
	size_t elem_size;

	type_struct_size = (type->ctt_size == CTF_V3_LSIZE_SENT) ?
		sizeof(struct ctf_type_v3) :
		sizeof(struct ctf_stype_v3);
	arr = (struct ctf_array_v3 *)((db_addr_t)type + type_struct_size);
	elem_type = db_ctf_typeid_to_type(&sym_data, arr->cta_contents);
	elem_size = ((elem_type->ctt_size == CTF_V3_LSIZE_SENT) ?
		CTF_TYPE_LSIZE(elem_type) :
		elem_type->ctt_size);
	elem_addr = addr;
	end = addr + (arr->cta_nelems * elem_size);

	db_indent = depth;
	db_printf("[\n");
	/* Loop through and print individual elements. */
	for (; elem_addr < end; elem_addr += elem_size) {
		if (db_pager_quit) {
			return;
		}
		db_iprintf("");
		db_pprint_type(elem_addr, elem_type, depth);
		if ((elem_addr + elem_size) < end) {
			db_printf(",\n");
		}
	}
	db_printf("\n");
	db_indent = depth - 1;
	db_iprintf("]");
}

/*
 * Pretty-prints an enum value. Also prints out symbolic name of value, if any.
 */
static inline void
db_pprint_enum(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{
	struct ctf_enum *ep, *endp;
	size_t type_struct_size;
	const char *valname;
	db_expr_t val;
	u_int vlen;

	type_struct_size = (type->ctt_size == CTF_V3_LSIZE_SENT) ?
		sizeof(struct ctf_type_v3) :
		sizeof(struct ctf_stype_v3);
	vlen = CTF_V3_INFO_VLEN(type->ctt_info);
	val = db_get_value(addr, sizeof(int), 0);

	if (db_pager_quit) {
		return;
	}
	ep = (struct ctf_enum *)((db_addr_t)type + type_struct_size);
	endp = ep + vlen;
	for (; ep < endp; ep++) {
		if (val == ep->cte_value) {
			valname = db_ctf_stroff_to_str(&sym_data, ep->cte_name);
			if (valname != NULL)
				db_printf("%s (0x%lx)", valname, val);
			else
				db_printf("(0x%lx)", val);
			break;
		}
	}
}

/*
 * Pretty-prints a pointer. If the 'depth' parameter is less than the
 * 'max_depth' global var, the pointer is "dereference", i.e. the contents of
 * the memory it points to are also printed. The value of the pointer is printed
 * otherwise.
 */
static inline void
db_pprint_ptr(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{
	struct ctf_type_v3 *ref_type;
	const char *qual = "";
	const char *name;
	db_addr_t val;
	u_int kind;

	ref_type = db_ctf_typeid_to_type(&sym_data, type->ctt_type);
	kind = CTF_V3_INFO_KIND(ref_type->ctt_info);
	switch (kind) {
	case CTF_K_STRUCT:
		qual = "struct ";
		break;
	case CTF_K_VOLATILE:
		qual = "volatile ";
		break;
	case CTF_K_CONST:
		qual = "const ";
		break;
	default:
		break;
	}

	val = db_get_value(addr, sizeof(db_addr_t), false);
	if (depth < max_depth) {
		/* Print contents of memory pointed to by this pointer. */
		db_pprint_type(addr, ref_type, depth + 1);
	} else {
		name = db_ctf_stroff_to_str(&sym_data, ref_type->ctt_name);
		db_indent = depth;
		if (name != NULL)
			db_printf("(%s%s *) 0x%lx", qual, name, val);
		else
			db_printf("0x%lx", val);
	}
}

/*
 * Pretty-print dispatching function.
 */
static void
db_pprint_type(db_addr_t addr, struct ctf_type_v3 *type, u_int depth)
{

	if (db_pager_quit) {
		return;
	}
	if (type == NULL) {
		db_printf("unknown type");
		return;
	}

	switch (CTF_V3_INFO_KIND(type->ctt_info)) {
	case CTF_K_INTEGER:
		db_pprint_int(addr, type, depth);
		break;
	case CTF_K_UNION:
	case CTF_K_STRUCT:
		db_pprint_struct(addr, type, depth);
		break;
	case CTF_K_FUNCTION:
	case CTF_K_FLOAT:
		db_indent = depth;
		db_iprintf("0x%lx", addr);
		break;
	case CTF_K_POINTER:
		db_pprint_ptr(addr, type, depth);
		break;
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_RESTRICT:
	case CTF_K_CONST: {
		struct ctf_type_v3 *ref_type = db_ctf_typeid_to_type(&sym_data,
		    type->ctt_type);
		db_pprint_type(addr, ref_type, depth);
		break;
	}
	case CTF_K_ENUM:
		db_pprint_enum(addr, type, depth);
		break;
	case CTF_K_ARRAY:
		db_pprint_arr(addr, type, depth);
		break;
	case CTF_K_UNKNOWN:
	case CTF_K_FORWARD:
	default:
		break;
	}
}

/*
 * Symbol pretty-printing command.
 * Syntax: pprint [/d depth] <sym_name>
 */
static void
db_pprint_symbol_cmd(const char *name)
{
	db_addr_t addr;
	int db_indent_old;
	const char *type_name = NULL;
	struct ctf_type_v3 *type = NULL;

	if (db_pager_quit) {
		return;
	}
	/* Clear symbol and CTF info */
	memset(&sym_data, 0, sizeof(struct db_ctf_sym_data));
	if (db_ctf_find_symbol(name, &sym_data) != 0) {
		db_error("Symbol not found\n");
	}
	if (ELF_ST_TYPE(sym_data.sym->st_info) != STT_OBJECT) {
		db_error("Symbol is not a variable\n");
	}
	addr = sym_data.sym->st_value;
	type = db_ctf_sym_to_type(&sym_data);
	if (type == NULL) {
		db_error("Can't find CTF type info\n");
	}
	type_name = db_ctf_stroff_to_str(&sym_data, type->ctt_name);
	if (type_name != NULL)
		db_printf("%s ", type_name);
	db_printf("%s = ", name);

	db_indent_old = db_indent;
	db_pprint_type(addr, type, 0);
	db_indent = db_indent_old;
}

/*
 * Command for pretty-printing arbitrary addresses.
 * Syntax: pprint [/d depth] struct <struct_name> <addr>
 */
static void
db_pprint_struct_cmd(db_expr_t addr, const char *struct_name)
{
	int db_indent_old;
	struct ctf_type_v3 *type = NULL;

	type = db_ctf_find_typename(&sym_data, struct_name);
	if (type == NULL) {
		db_error("Can't find CTF type info\n");
		return;
	}

	db_printf("struct %s ", struct_name);
	db_printf("%p = ", (void *)addr);

	db_indent_old = db_indent;
	db_pprint_type(addr, type, 0);
	db_indent = db_indent_old;
}

/*
 * Pretty print an address or a symbol.
 */
void
db_pprint_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	int t = 0;
	const char *name;

	/* Set default depth */
	max_depth = DB_PPRINT_DEFAULT_DEPTH;
	/* Parse print modifiers */
	t = db_read_token();
	if (t == tSLASH) {
		t = db_read_token();
		if (t != tIDENT) {
			db_error("Invalid flag passed\n");
		}
		/* Parse desired depth level */
		if (strcmp(db_tok_string, "d") == 0) {
			t = db_read_token();
			if (t != tNUMBER) {
				db_error("Invalid depth provided\n");
			}
			max_depth = db_tok_number;
		} else {
			db_error("Invalid flag passed\n");
		}
		/* Fetch next token */
		t = db_read_token();
	}
	/* Parse subcomannd */
	if (t == tIDENT) {
		if (strcmp(db_tok_string, "struct") == 0) {
			t = db_read_token();

			if (t != tIDENT) {
				db_error("Invalid struct type name provided\n");
			}
			name = db_tok_string;

			if (db_expression(&addr) == 0) {
				db_error("Address not provided\n");
			}
			db_pprint_struct_cmd(addr, name);
		} else {
			name = db_tok_string;
			db_pprint_symbol_cmd(name);
		}
	} else {
		db_error("Invalid subcommand\n");
	}
	db_skip_to_eol();
}
