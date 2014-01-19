/*-
 * Copyright (c) 2009,2011 Kai Wang
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

ELFTC_VCSID("$Id: libdwarf_loclist.c 2972 2013-12-23 06:46:04Z kaiwang27 $");

static int
_dwarf_loclist_add_locdesc(Dwarf_Debug dbg, Dwarf_CU cu, Dwarf_Section *ds,
    uint64_t *off, Dwarf_Locdesc **ld, uint64_t *ldlen,
    Dwarf_Unsigned *total_len, Dwarf_Error *error)
{
	uint64_t start, end;
	int i, len, ret;

	if (total_len != NULL)
		*total_len = 0;

	for (i = 0; *off < ds->ds_size; i++) {
		start = dbg->read(ds->ds_data, off, cu->cu_pointer_size);
		end = dbg->read(ds->ds_data, off, cu->cu_pointer_size);
		if (ld != NULL) {
			ld[i]->ld_lopc = start;
			ld[i]->ld_hipc = end;
		}

		if (total_len != NULL)
			*total_len += 2 * cu->cu_pointer_size;

		/* Check if it is the end entry. */
		if (start == 0 && end ==0) {
			i++;
			break;
		}

		/* Check if it is base-select entry. */
		if ((cu->cu_pointer_size == 4 && start == ~0U) ||
		    (cu->cu_pointer_size == 8 && start == ~0ULL))
			continue;

		/* Otherwise it's normal entry. */
		len = dbg->read(ds->ds_data, off, 2);
		if (*off + len > ds->ds_size) {
			DWARF_SET_ERROR(dbg, error,
			    DW_DLE_DEBUG_LOC_SECTION_SHORT);
			return (DW_DLE_DEBUG_LOC_SECTION_SHORT);
		}

		if (total_len != NULL)
			*total_len += len;

		if (ld != NULL) {
			ret = _dwarf_loc_fill_locdesc(dbg, ld[i],
			    ds->ds_data + *off, len, cu->cu_pointer_size,
			    error);
			if (ret != DW_DLE_NONE)
				return (ret);
		}

		*off += len;
	}

	if (ldlen != NULL)
		*ldlen = i;

	return (DW_DLE_NONE);
}

int
_dwarf_loclist_find(Dwarf_Debug dbg, Dwarf_CU cu, uint64_t lloff,
    Dwarf_Loclist *ret_ll, Dwarf_Error *error)
{
	Dwarf_Loclist ll;
	int ret;

	assert(ret_ll != NULL);
	ret = DW_DLE_NONE;

	TAILQ_FOREACH(ll, &dbg->dbg_loclist, ll_next)
		if (ll->ll_offset == lloff)
			break;

	if (ll == NULL)
		ret = _dwarf_loclist_add(dbg, cu, lloff, ret_ll, error);
	else
		*ret_ll = ll;

	return (ret);
}

int
_dwarf_loclist_add(Dwarf_Debug dbg, Dwarf_CU cu, uint64_t lloff,
    Dwarf_Loclist *ret_ll, Dwarf_Error *error)
{
	Dwarf_Section *ds;
	Dwarf_Loclist ll, tll;
	uint64_t ldlen;
	int i, ret;

	ret = DW_DLE_NONE;

	if ((ds = _dwarf_find_section(dbg, ".debug_loc")) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLE_NO_ENTRY);
	}

	if (lloff >= ds->ds_size) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLE_NO_ENTRY);
	}

	if ((ll = malloc(sizeof(struct _Dwarf_Loclist))) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
		return (DW_DLE_MEMORY);
	}

	ll->ll_offset = lloff;

	/* Get the number of locdesc the first round. */
	ret = _dwarf_loclist_add_locdesc(dbg, cu, ds, &lloff, NULL, &ldlen,
	    NULL, error);
	if (ret != DW_DLE_NONE)
		goto fail_cleanup;

	/*
	 * Dwarf_Locdesc list memory is allocated in this way (one more level
	 * of indirect) to make the loclist API be compatible with SGI libdwarf.
	 */
	ll->ll_ldlen = ldlen;
	if (ldlen != 0) {
		if ((ll->ll_ldlist = calloc(ldlen, sizeof(Dwarf_Locdesc *))) ==
		    NULL) {
			DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
			ret = DW_DLE_MEMORY;
			goto fail_cleanup;
		}
		for (i = 0; (uint64_t) i < ldlen; i++) {
			if ((ll->ll_ldlist[i] =
			    calloc(1, sizeof(Dwarf_Locdesc))) == NULL) {
				DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
				ret = DW_DLE_MEMORY;
				goto fail_cleanup;
			}
		}
	} else
		ll->ll_ldlist = NULL;

	lloff = ll->ll_offset;

	/* Fill in locdesc. */
	ret = _dwarf_loclist_add_locdesc(dbg, cu, ds, &lloff, ll->ll_ldlist,
	    NULL, &ll->ll_length, error);
	if (ret != DW_DLE_NONE)
		goto fail_cleanup;

	/* Insert to the queue. Sort by offset. */
	TAILQ_FOREACH(tll, &dbg->dbg_loclist, ll_next)
		if (tll->ll_offset > ll->ll_offset) {
			TAILQ_INSERT_BEFORE(tll, ll, ll_next);
			break;
		}

	if (tll == NULL)
		TAILQ_INSERT_TAIL(&dbg->dbg_loclist, ll, ll_next);

	*ret_ll = ll;
	return (DW_DLE_NONE);

fail_cleanup:

	_dwarf_loclist_free(ll);

	return (ret);
}

void
_dwarf_loclist_free(Dwarf_Loclist ll)
{
	int i;

	if (ll == NULL)
		return;

	if (ll->ll_ldlist != NULL) {
		for (i = 0; i < ll->ll_ldlen; i++) {
			if (ll->ll_ldlist[i]->ld_s)
				free(ll->ll_ldlist[i]->ld_s);
			free(ll->ll_ldlist[i]);
		}
		free(ll->ll_ldlist);
	}
	free(ll);
}

void
_dwarf_loclist_cleanup(Dwarf_Debug dbg)
{
	Dwarf_Loclist ll, tll;

	assert(dbg != NULL && dbg->dbg_mode == DW_DLC_READ);

	TAILQ_FOREACH_SAFE(ll, &dbg->dbg_loclist, ll_next, tll) {
		TAILQ_REMOVE(&dbg->dbg_loclist, ll, ll_next);
		_dwarf_loclist_free(ll);
	}
}
