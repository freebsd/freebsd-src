/*
 *  $Id: dlg_internals.h,v 1.3 2019/08/08 21:29:41 tom Exp $
 *
 *  dlg_internals.h -- runtime binding support for dialog
 *
 *  Copyright 2019 Thomas E.  Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 */

#ifndef DLG_INTERNALS_H_included
#define DLG_INTERNALS_H_included 1

#include <dialog.h>

#define TableSize(name) (sizeof(name)/sizeof((name)[0]))

/* *INDENT-OFF* */
#define resizeit(name, NAME) \
		name = ((NAME >= old_##NAME) \
			? (NAME - (old_##NAME - old_##name)) \
			: old_##name)

#define AddLastKey() \
	if (dialog_vars.last_key) { \
	    if (dlg_need_separator()) \
		dlg_add_separator(); \
	    dlg_add_last_key(-1); \
	}


/* *INDENT-ON* */

#endif /* DLG_INTERNALS_H_included */
