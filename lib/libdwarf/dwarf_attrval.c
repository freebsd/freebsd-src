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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "_libdwarf.h"

Dwarf_AttrValue
dwarf_attrval_find(Dwarf_Die die, Dwarf_Half attr)
{
	Dwarf_AttrValue av;

	STAILQ_FOREACH(av, &die->die_attrval, av_next) {
		if (av->av_attrib == attr)
			break;
	}

	return av;
}

int
dwarf_attrval_add(Dwarf_Die die, Dwarf_AttrValue avref, Dwarf_AttrValue *avp, Dwarf_Error *error)
{
	Dwarf_AttrValue av;
	int ret = DWARF_E_NONE;

	if ((av = malloc(sizeof(struct _Dwarf_AttrValue))) == NULL) {
		DWARF_SET_ERROR(error, DWARF_E_MEMORY);
		return DWARF_E_MEMORY;
	}

	memcpy(av, avref, sizeof(struct _Dwarf_AttrValue));

	/* Add the attribute value to the list in the die. */
	STAILQ_INSERT_TAIL(&die->die_attrval, av, av_next);

	/* Save a pointer to the attribute name if this is one. */
	if (av->av_attrib == DW_AT_name)
		switch (av->av_form) {
		case DW_FORM_strp:
			die->die_name = av->u[1].s;
			break;
		case DW_FORM_string:
			die->die_name = av->u[0].s;
			break;
		default:
			break;
		}

	if (avp != NULL)
		*avp = av;

	return ret;
}

int
dwarf_attrval_flag(Dwarf_Die die, uint64_t attr, Dwarf_Bool *valp, Dwarf_Error *err)
{
	Dwarf_AttrValue av;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || valp == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	*valp = 0;

	if ((av = dwarf_attrval_find(die, attr)) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else {
		switch (av->av_form) {
		case DW_FORM_flag:
		case DW_FORM_flag_present:
			*valp = (Dwarf_Bool) av->u[0].u64;
			break;
		default:
			printf("%s(%d): av->av_form '%s' (0x%lx) not handled\n",
			    __func__,__LINE__,get_form_desc(av->av_form),
			    (u_long) av->av_form);
			DWARF_SET_ERROR(err, DWARF_E_BAD_FORM);
			ret = DWARF_E_BAD_FORM;
		}
	}

	return ret;
}

int
dwarf_attrval_string(Dwarf_Die die, uint64_t attr, const char **strp, Dwarf_Error *err)
{
	Dwarf_AttrValue av;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || strp == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	*strp = NULL;

	if (attr == DW_AT_name)
		*strp = die->die_name;
	else if ((av = dwarf_attrval_find(die, attr)) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else {
		switch (av->av_form) {
		case DW_FORM_strp:
			*strp = av->u[1].s;
			break;
		case DW_FORM_string:
			*strp = av->u[0].s;
			break;
		default:
			printf("%s(%d): av->av_form '%s' (0x%lx) not handled\n",
			    __func__,__LINE__,get_form_desc(av->av_form),
			    (u_long) av->av_form);
			DWARF_SET_ERROR(err, DWARF_E_BAD_FORM);
			ret = DWARF_E_BAD_FORM;
		}
	}

	return ret;
}

int
dwarf_attrval_signed(Dwarf_Die die, uint64_t attr, Dwarf_Signed *valp, Dwarf_Error *err)
{
	Dwarf_AttrValue av;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || valp == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	*valp = 0;

	if ((av = dwarf_attrval_find(die, attr)) == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else {
		switch (av->av_form) {
		case DW_FORM_data1:
		case DW_FORM_sdata:
			*valp = av->u[0].s64;
			break;
		default:
			printf("%s(%d): av->av_form '%s' (0x%lx) not handled\n",
			    __func__,__LINE__,get_form_desc(av->av_form),
			    (u_long) av->av_form);
			DWARF_SET_ERROR(err, DWARF_E_BAD_FORM);
			ret = DWARF_E_BAD_FORM;
		}
	}

	return ret;
}

int
dwarf_attrval_unsigned(Dwarf_Die die, uint64_t attr, Dwarf_Unsigned *valp, Dwarf_Error *err)
{
	Dwarf_AttrValue av;
	int ret = DWARF_E_NONE;

	if (err == NULL)
		return DWARF_E_ERROR;

	if (die == NULL || valp == NULL) {
		DWARF_SET_ERROR(err, DWARF_E_ARGUMENT);
		return DWARF_E_ARGUMENT;
	}

	*valp = 0;

	if ((av = dwarf_attrval_find(die, attr)) == NULL && attr != DW_AT_type) {
		DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
		ret = DWARF_E_NO_ENTRY;
	} else if (av == NULL && (av = dwarf_attrval_find(die,
	    DW_AT_abstract_origin)) != NULL) {
		Dwarf_Die die1;
		Dwarf_Unsigned val;

		switch (av->av_form) {
		case DW_FORM_data1:
		case DW_FORM_data2:
		case DW_FORM_data4:
		case DW_FORM_data8:
		case DW_FORM_ref1:
		case DW_FORM_ref2:
		case DW_FORM_ref4:
		case DW_FORM_ref8:
		case DW_FORM_ref_udata:
			val = av->u[0].u64;

			if ((die1 = dwarf_die_find(die, val)) == NULL ||
			    (av = dwarf_attrval_find(die1, attr)) == NULL) {
				DWARF_SET_ERROR(err, DWARF_E_NO_ENTRY);
				ret = DWARF_E_NO_ENTRY;
			}
			break;
		default:
			printf("%s(%d): av->av_form '%s' (0x%lx) not handled\n",
			    __func__,__LINE__,get_form_desc(av->av_form),
			    (u_long) av->av_form);
			DWARF_SET_ERROR(err, DWARF_E_BAD_FORM);
			ret = DWARF_E_BAD_FORM;
		}
	}

	if (ret == DWARF_E_NONE) {
		switch (av->av_form) {
		case DW_FORM_data1:
		case DW_FORM_data2:
		case DW_FORM_data4:
		case DW_FORM_data8:
		case DW_FORM_ref1:
		case DW_FORM_ref2:
		case DW_FORM_ref4:
		case DW_FORM_ref8:
		case DW_FORM_ref_udata:
			*valp = av->u[0].u64;
			break;
		default:
			printf("%s(%d): av->av_form '%s' (0x%lx) not handled\n",
			    __func__,__LINE__,get_form_desc(av->av_form),
			    (u_long) av->av_form);
			DWARF_SET_ERROR(err, DWARF_E_BAD_FORM);
			ret = DWARF_E_BAD_FORM;
		}
	}

	return ret;
}
