/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_OBJECT_H
#define AMIGA_OBJECT_H

#include <exec/lists.h>

enum
{
	AMINS_UNKNOWN,
	AMINS_CALLBACK,
	AMINS_WINDOW,
	AMINS_DLWINDOW,
	AMINS_LOGINWINDOW,
	AMINS_TVWINDOW,
	AMINS_FINDWINDOW,
	AMINS_HISTORYWINDOW,
	AMINS_GUIOPTSWINDOW,
	AMINS_PRINTWINDOW,
	AMINS_FONT,
	AMINS_MIME,
	AMINS_RECT
};

struct nsObject
{
	struct Node dtz_Node;
	ULONG Type;
	void *objstruct;
	ULONG objstruct_size;
};

struct MinList *NewObjList(void);
struct nsObject *AddObject(struct MinList *objlist, ULONG otype);
void DelObject(struct nsObject *dtzo);
void DelObjectNoFree(struct nsObject *dtzo);
void FreeObjList(struct MinList *objlist);

#endif
