/*-
 * Copyright (c) 2009 Kai Wang
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

#include "_libdwarf.h"

ELFTC_VCSID("$Id: dwarf_loclist.c 2074 2011-10-27 03:34:33Z jkoshy $");

int
dwarf_loclist_n(Dwarf_Attribute at, Dwarf_Locdesc ***llbuf,
    Dwarf_Signed *listlen, Dwarf_Error *error)
{
	Dwarf_Loclist ll;
	Dwarf_Debug dbg;
	int ret;

	dbg = at != NULL ? at->at_die->die_dbg : NULL;

	if (at == NULL || llbuf == NULL || listlen == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	switch (at->at_attrib) {
	case DW_AT_location:
	case DW_AT_string_length:
	case DW_AT_return_addr:
	case DW_AT_data_member_location:
	case DW_AT_frame_base:
	case DW_AT_segment:
	case DW_AT_static_link:
	case DW_AT_use_location:
	case DW_AT_vtable_elem_location:
		switch (at->at_form) {
		case DW_FORM_data4:
		case DW_FORM_data8:
			ret = _dwarf_loclist_find(at->at_die->die_dbg,
			    at->at_die->die_cu, at->u[0].u64, &ll, error);
			if (ret == DW_DLE_NO_ENTRY) {
				DWARF_SET_ERROR(dbg, error, ret);
				return (DW_DLV_NO_ENTRY);
			}
			if (ret != DW_DLE_NONE)
				return (DW_DLV_ERROR);
			*llbuf = ll->ll_ldlist;
			*listlen = ll->ll_ldlen;
			return (DW_DLV_OK);
		case DW_FORM_block:
		case DW_FORM_block1:
		case DW_FORM_block2:
		case DW_FORM_block4:
			if (at->at_ld == NULL) {
				ret = _dwarf_loc_add(at->at_die, at, error);
				if (ret != DW_DLE_NONE)
					return (DW_DLV_ERROR);
			}
			*llbuf = &at->at_ld;
			*listlen = 1;
			return (DW_DLV_OK);
		default:
			/* Malformed Attr? */
			DWARF_SET_ERROR(dbg, error, DW_DLE_ATTR_FORM_BAD);
			return (DW_DLV_NO_ENTRY);
		}
	default:
		/* Wrong attr supplied. */
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}
}

int
dwarf_loclist(Dwarf_Attribute at, Dwarf_Locdesc **llbuf,
    Dwarf_Signed *listlen, Dwarf_Error *error)
{
	Dwarf_Loclist ll;
	Dwarf_Debug dbg;
	int ret;

	dbg = at != NULL ? at->at_die->die_dbg : NULL;

	if (at == NULL || llbuf == NULL || listlen == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	switch (at->at_attrib) {
	case DW_AT_location:
	case DW_AT_string_length:
	case DW_AT_return_addr:
	case DW_AT_data_member_location:
	case DW_AT_frame_base:
	case DW_AT_segment:
	case DW_AT_static_link:
	case DW_AT_use_location:
	case DW_AT_vtable_elem_location:
		switch (at->at_form) {
		case DW_FORM_data4:
		case DW_FORM_data8:
			ret = _dwarf_loclist_find(at->at_die->die_dbg,
			    at->at_die->die_cu, at->u[0].u64, &ll, error);
			if (ret == DW_DLE_NO_ENTRY) {
				DWARF_SET_ERROR(dbg, error, DW_DLV_NO_ENTRY);
				return (DW_DLV_NO_ENTRY);
			}
			if (ret != DW_DLE_NONE)
				return (DW_DLV_ERROR);
			*llbuf = ll->ll_ldlist[0];
			*listlen = 1;
			return (DW_DLV_OK);
		case DW_FORM_block:
		case DW_FORM_block1:
		case DW_FORM_block2:
		case DW_FORM_block4:
			if (at->at_ld == NULL) {
				ret = _dwarf_loc_add(at->at_die, at, error);
				if (ret != DW_DLE_NONE)
					return (DW_DLV_ERROR);
			}
			*llbuf = at->at_ld;
			*listlen = 1;
			return (DW_DLV_OK);
		default:
			DWARF_SET_ERROR(dbg, error, DW_DLE_ATTR_FORM_BAD);
			return (DW_DLV_ERROR);
		}
	default:
		/* Wrong attr supplied. */
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}
}

int
dwarf_get_loclist_entry(Dwarf_Debug dbg, Dwarf_Unsigned offset,
    Dwarf_Addr *hipc, Dwarf_Addr *lopc, Dwarf_Ptr *data,
    Dwarf_Unsigned *entry_len, Dwarf_Unsigned *next_entry,
    Dwarf_Error *error)
{
	Dwarf_Loclist ll, next_ll;
	Dwarf_Locdesc *ld;
	Dwarf_Section *ds;
	int i, ret;

	if (dbg == NULL || hipc == NULL || lopc == NULL || data == NULL ||
	    entry_len == NULL || next_entry == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	ret = _dwarf_loclist_find(dbg, STAILQ_FIRST(&dbg->dbg_cu), offset, &ll,
	    error);
	if (ret == DW_DLE_NO_ENTRY) {
		DWARF_SET_ERROR(dbg, error, DW_DLV_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	} else if (ret != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	*hipc = *lopc = 0;
	for (i = 0; i < ll->ll_ldlen; i++) {
		ld = ll->ll_ldlist[i];
		if (i == 0) {
			*hipc = ld->ld_hipc;
			*lopc = ld->ld_lopc;
		} else {
			if (ld->ld_lopc < *lopc)
				*lopc = ld->ld_lopc;
			if (ld->ld_hipc > *hipc)
				*hipc = ld->ld_hipc;
		}
	}

	ds = _dwarf_find_section(dbg, ".debug_loc");
	assert(ds != NULL);
	*data = (uint8_t *) ds->ds_data + ll->ll_offset;
	*entry_len = ll->ll_length;

	next_ll = TAILQ_NEXT(ll, ll_next);
	if (next_ll != NULL)
		*next_entry = next_ll->ll_offset;
	else
		*next_entry = ds->ds_size;

	return (DW_DLV_OK);
}

int
dwarf_loclist_from_expr(Dwarf_Debug dbg, Dwarf_Ptr bytes_in,
    Dwarf_Unsigned bytes_len, Dwarf_Locdesc **llbuf, Dwarf_Signed *listlen,
    Dwarf_Error *error)
{
	Dwarf_Locdesc *ld;
	int ret;

	if (dbg == NULL || bytes_in == NULL || bytes_len == 0 ||
	    llbuf == NULL || listlen == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	ret = _dwarf_loc_fill_locexpr(dbg, &ld, bytes_in, bytes_len,
	    dbg->dbg_pointer_size, error);
	if (ret != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	*llbuf = ld;
	*listlen = 1;

	return (DW_DLV_OK);
}

int
dwarf_loclist_from_expr_a(Dwarf_Debug dbg, Dwarf_Ptr bytes_in,
    Dwarf_Unsigned bytes_len, Dwarf_Half addr_size, Dwarf_Locdesc **llbuf,
    Dwarf_Signed *listlen, Dwarf_Error *error)
{
	Dwarf_Locdesc *ld;
	int ret;

	if (dbg == NULL || bytes_in == NULL || bytes_len == 0 ||
	    llbuf == NULL || listlen == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (addr_size != 4 && addr_size != 8) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	ret = _dwarf_loc_fill_locexpr(dbg, &ld, bytes_in, bytes_len, addr_size,
	    error);
	if (ret != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	*llbuf = ld;
	*listlen = 1;

	return (DW_DLV_OK);
}
