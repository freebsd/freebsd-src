/*
 * Copyright 2005,2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include <exec/lists.h>
#include <exec/nodes.h>

#include "amiga/filetype.h"
#include "amiga/font.h"
#include "amiga/object.h"

struct MinList *NewObjList(void)
{

	struct MinList *objlist;

	objlist = (struct MinList *)AllocVecTagList(sizeof(struct MinList), NULL);

	NewMinList(objlist);

	return(objlist);

}

struct nsObject *AddObject(struct MinList *objlist, ULONG otype)
{
	struct nsObject *dtzo;

	dtzo = (struct nsObject *)AllocVecTags(sizeof(struct nsObject), AVT_ClearWithValue, 0, TAG_DONE);

	AddTail((struct List *)objlist,(struct Node *)dtzo);

	dtzo->Type = otype;

	return(dtzo);
}

void DelObjectInternal(struct nsObject *dtzo, BOOL free_obj)
{
	Remove((struct Node *)dtzo);
	if(dtzo->Type == AMINS_FONT) ami_font_close(dtzo->objstruct);
	if(dtzo->Type == AMINS_MIME) ami_mime_entry_free(dtzo->objstruct);
	if(dtzo->objstruct && free_obj) FreeVec(dtzo->objstruct);
	if(dtzo->dtz_Node.ln_Name) free(dtzo->dtz_Node.ln_Name);
	FreeVec(dtzo);
	dtzo = NULL;
}

void DelObject(struct nsObject *dtzo)
{
	DelObjectInternal(dtzo, TRUE);
}

void DelObjectNoFree(struct nsObject *dtzo)
{
	DelObjectInternal(dtzo, FALSE);
}

void FreeObjList(struct MinList *objlist)
{
	struct nsObject *node;
	struct nsObject *nnode;

	if(IsMinListEmpty(objlist)) return;
	node = (struct nsObject *)GetHead((struct List *)objlist);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		DelObject(node);
	}while(node=nnode);

	FreeVec(objlist);
}
