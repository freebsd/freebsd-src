/*
 * Copyright 2009 Rene W. Olsen <ac@rebels.com>
 * Copyright 2009 Stephen Fellner <sf.amiga@gmail.com>
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

#include "amiga/os3support.h"

#include <exec/semaphores.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- */

struct myStringClassData
{
	struct SignalSemaphore  Semaphore;
	uint32					WinXPos;
	uint32					WinYPos;
	uint32					WinWidth;
	uint32					WinHeight;
	struct Window *			Window;
	Object *				WindowObject;
	Object *				ListviewObject;
	struct List				ListviewHeader;
	uint32					ListviewCount;
	uint32					ListviewSelected;
	struct List *			SearchHeader;
	STRPTR					SearchBuffer;
};

#define STRINGVIEW_Header	0x50000001

/* protos */

Class *	MakeStringClass(		void );
void FreeStringClass(Class *);

/* The End */

#ifdef __cplusplus
}
#endif
