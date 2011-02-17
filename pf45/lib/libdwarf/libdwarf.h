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

#ifndef	_LIBDWARF_H_
#define	_LIBDWARF_H_

#include <libelf.h>

typedef int		Dwarf_Bool;
typedef off_t		Dwarf_Off;
typedef uint64_t	Dwarf_Unsigned;
typedef uint16_t	Dwarf_Half;
typedef uint8_t		Dwarf_Small;
typedef int64_t		Dwarf_Signed;
typedef uint64_t	Dwarf_Addr;
typedef void		*Dwarf_Ptr;

/* Forward definitions. */
typedef struct _Dwarf_Abbrev	*Dwarf_Abbrev;
typedef struct _Dwarf_Arange	*Dwarf_Arange;
typedef struct _Dwarf_Attribute	*Dwarf_Attribute;
typedef struct _Dwarf_AttrValue	*Dwarf_AttrValue;
typedef struct _Dwarf_CU	*Dwarf_CU;
typedef struct _Dwarf_Cie	*Dwarf_Cie;
typedef struct _Dwarf_Debug	*Dwarf_Debug;
typedef struct _Dwarf_Die	*Dwarf_Die;
typedef struct _Dwarf_Fde	*Dwarf_Fde;
typedef struct _Dwarf_Func	*Dwarf_Func;
typedef struct _Dwarf_Global	*Dwarf_Global;
typedef struct _Dwarf_Line	*Dwarf_Line;
typedef struct _Dwarf_Type	*Dwarf_Type;
typedef struct _Dwarf_Var	*Dwarf_Var;
typedef struct _Dwarf_Weak	*Dwarf_Weak;

typedef struct {
        Dwarf_Small	lr_atom;
        Dwarf_Unsigned	lr_number;
	Dwarf_Unsigned	lr_number2;
	Dwarf_Unsigned	lr_offset;
} Dwarf_Loc;

typedef struct {
	Dwarf_Addr      ld_lopc;
	Dwarf_Addr      ld_hipc;
	Dwarf_Half      ld_cents;
	Dwarf_Loc	*ld_s;
} Dwarf_Locdesc;

/*
 * Error numbers which are specific to this implementation.
 */
enum {
	DWARF_E_NONE,			/* No error. */
	DWARF_E_ERROR,			/* An error! */
	DWARF_E_NO_ENTRY,		/* No entry. */
	DWARF_E_ARGUMENT,		/* Invalid argument. */
	DWARF_E_DEBUG_INFO,		/* Debug info NULL. */
	DWARF_E_MEMORY,			/* Insufficient memory. */
	DWARF_E_ELF,			/* ELF error. */
	DWARF_E_INVALID_CU,		/* Invalid compilation unit data. */
	DWARF_E_CU_VERSION,		/* Wrong CU version. */
	DWARF_E_MISSING_ABBREV,		/* Abbrev not found. */
	DWARF_E_NOT_IMPLEMENTED,	/* Not implemented. */
	DWARF_E_CU_CURRENT,		/* No current compilation unit. */
	DWARF_E_BAD_FORM,		/* Wrong form type for attribute value. */
	DWARF_E_INVALID_EXPR,		/* Invalid DWARF expression. */
	DWARF_E_NUM			/* Max error number. */
};

typedef struct _Dwarf_Error {
	int		err_error;	/* DWARF error. */
	int		elf_error;	/* ELF error. */
	const char	*err_func;	/* Function name where error occurred. */
	int		err_line;	/* Line number where error occurred. */
	char		err_msg[1024];	/* Formatted error message. */
} Dwarf_Error;

/*
 * Return values which have to be compatible with other
 * implementations of libdwarf.
 */
#define DW_DLV_NO_ENTRY		DWARF_E_NO_ENTRY
#define DW_DLV_OK		DWARF_E_NONE
#define DW_DLE_DEBUG_INFO_NULL	DWARF_E_DEBUG_INFO

#define DW_DLC_READ        	0	/* read only access */

/* Function prototype definitions. */
__BEGIN_DECLS
Dwarf_Abbrev	dwarf_abbrev_find(Dwarf_CU, uint64_t);
Dwarf_AttrValue dwarf_attrval_find(Dwarf_Die, Dwarf_Half);
Dwarf_Die	dwarf_die_find(Dwarf_Die, Dwarf_Unsigned);
const char	*dwarf_errmsg(Dwarf_Error *);
const char	*get_sht_desc(uint32_t);
const char	*get_attr_desc(uint32_t);
const char	*get_form_desc(uint32_t);
const char	*get_tag_desc(uint32_t);
int		dwarf_abbrev_add(Dwarf_CU, uint64_t, uint64_t, uint8_t, Dwarf_Abbrev *, Dwarf_Error *);
int		dwarf_attr(Dwarf_Die, Dwarf_Half, Dwarf_Attribute *, Dwarf_Error *);
int		dwarf_attr_add(Dwarf_Abbrev, uint64_t, uint64_t, Dwarf_Attribute *, Dwarf_Error *);
int		dwarf_attrval(Dwarf_Die, Dwarf_Half, Dwarf_AttrValue *, Dwarf_Error *);
int		dwarf_attrval_add(Dwarf_Die, Dwarf_AttrValue, Dwarf_AttrValue *, Dwarf_Error *);
int		dwarf_attrval_flag(Dwarf_Die, uint64_t, Dwarf_Bool *, Dwarf_Error *);
int		dwarf_attrval_signed(Dwarf_Die, uint64_t, Dwarf_Signed *, Dwarf_Error *);
int		dwarf_attrval_string(Dwarf_Die, uint64_t, const char **, Dwarf_Error *);
int		dwarf_attrval_unsigned(Dwarf_Die, uint64_t, Dwarf_Unsigned *, Dwarf_Error *);
int		dwarf_child(Dwarf_Die, Dwarf_Die *, Dwarf_Error *);
int		dwarf_die_add(Dwarf_CU, int, uint64_t, uint64_t, Dwarf_Abbrev, Dwarf_Die *, Dwarf_Error *);
int		dwarf_dieoffset(Dwarf_Die, Dwarf_Off *, Dwarf_Error *);
int		dwarf_elf_init(Elf *, int, Dwarf_Debug *, Dwarf_Error *);
int		dwarf_errno(Dwarf_Error *);
int		dwarf_finish(Dwarf_Debug *, Dwarf_Error *);
int		dwarf_locdesc(Dwarf_Die, uint64_t, Dwarf_Locdesc **, Dwarf_Signed *, Dwarf_Error *);
int		dwarf_locdesc_free(Dwarf_Locdesc *, Dwarf_Error *);
int		dwarf_init(int, int, Dwarf_Debug *, Dwarf_Error *);
int		dwarf_next_cu_header(Dwarf_Debug, Dwarf_Unsigned *, Dwarf_Half *,
		    Dwarf_Unsigned *, Dwarf_Half *, Dwarf_Unsigned *, Dwarf_Error *);
int		dwarf_op_num(uint8_t, uint8_t *, int);
int		dwarf_siblingof(Dwarf_Debug, Dwarf_Die, Dwarf_Die *, Dwarf_Error *);
int		dwarf_tag(Dwarf_Die, Dwarf_Half *, Dwarf_Error *);
int		dwarf_whatform(Dwarf_Attribute, Dwarf_Half *, Dwarf_Error *);
void		dwarf_dealloc(Dwarf_Debug, Dwarf_Ptr, Dwarf_Unsigned);
void		dwarf_dump(Dwarf_Debug);
void		dwarf_dump_abbrev(Dwarf_Debug);
void		dwarf_dump_av(Dwarf_Die, Dwarf_AttrValue);
void		dwarf_dump_dbgstr(Dwarf_Debug);
void		dwarf_dump_die(Dwarf_Die);
void		dwarf_dump_die_at_offset(Dwarf_Debug, Dwarf_Off);
void		dwarf_dump_info(Dwarf_Debug);
void		dwarf_dump_shstrtab(Dwarf_Debug);
void		dwarf_dump_strtab(Dwarf_Debug);
void		dwarf_dump_symtab(Dwarf_Debug);
void		dwarf_dump_raw(Dwarf_Debug);
void		dwarf_dump_tree(Dwarf_Debug);
__END_DECLS

#endif /* !_LIBDWARF_H_ */
