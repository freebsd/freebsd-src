/*
 * Copyright 2008-2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>

#include <diskfont/diskfonttag.h>
#include <datatypes/textclass.h>
#include <datatypes/pictureclass.h>

#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"

#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/drag.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/iff_cset.h"
#include "amiga/iff_dr2d.h"
#include "amiga/menu.h"
#include "amiga/utf8.h"

#define ID_UTF8  MAKE_ID('U','T','F','8')

struct IFFHandle *iffh = NULL;

static LONG ami_clipboard_iffp_do_nothing(struct Hook *hook, void *object, LONG *cmd)
{
	return 0;
}

static void ami_clipboard_iffp_clear_stopchunk(struct IFFHandle *iffh, ULONG iff_type, ULONG iff_chunk)
{
	static struct Hook entry_hook;

	entry_hook.h_Entry = (void *)ami_clipboard_iffp_do_nothing;
	entry_hook.h_Data = 0;

	EntryHandler(iffh, iff_type, iff_chunk, IFFSLI_TOP, &entry_hook, NULL);
}

struct IFFHandle *ami_clipboard_init_internal(int unit)
{
	struct IFFHandle *iffhandle = NULL;

	if(iffhandle = AllocIFF())
	{
		if(iffhandle->iff_Stream = (ULONG)OpenClipboard(unit))
		{
			InitIFFasClip(iffhandle);
		}
	}

	return iffhandle;
}

void ami_clipboard_init(void)
{
	iffh = ami_clipboard_init_internal(0);
}

void ami_clipboard_free_internal(struct IFFHandle *iffhandle)
{
	if(iffhandle == NULL) return;
	if(iffhandle->iff_Stream) CloseClipboard((struct ClipboardHandle *)iffhandle->iff_Stream);
	FreeIFF(iffhandle);
}

void ami_clipboard_free(void)
{
	ami_clipboard_free_internal(iffh);
}

void gui_start_selection(struct gui_window *g)
{
	if(!g) return;
	if(!g->shared->win) return;
	if(nsoption_bool(kiosk_mode) == true) return;

	OnMenu(g->shared->win, AMI_MENU_CLEAR);
	OnMenu(g->shared->win, AMI_MENU_COPY);

	if (browser_window_get_editor_flags(g->shared->bw) & BW_EDITOR_CAN_CUT)
		OnMenu(g->shared->win, AMI_MENU_CUT);
}

char *ami_clipboard_cat_collection(struct CollectionItem *ci, LONG codeset, size_t *text_length)
{
	struct CollectionItem *ci_new = NULL, *ci_next, *ci_curr = ci;
	size_t len = 0;
	char *text = NULL, *p;

	/* Scan the collected chunks to find out the total size.
	 * If they are not in UTF-8, convert the chunks first and create a new CollectionItem list.
	 */
	do {
		switch(codeset) {
			case 106:
				len += ci_curr->ci_Size;
			break;
			
			case 0:
				if(ci_new) {
					ci_next->ci_Next = AllocVecTags(sizeof(struct CollectionItem), AVT_ClearWithValue, 0, TAG_DONE);
					ci_next = ci_next->ci_Next;
				} else {
					ci_new = AllocVecTags(sizeof(struct CollectionItem), AVT_ClearWithValue, 0, TAG_DONE);
					ci_next = ci_new;
				}
				
				utf8_from_local_encoding(ci_curr->ci_Data, ci_curr->ci_Size, (char **)&ci_next->ci_Data);
				ci_next->ci_Size = strlen(ci_next->ci_Data);
				len += ci_next->ci_Size;
			break;

			default:
				if(ci_new) {
					ci_next->ci_Next = AllocVecTags(sizeof(struct CollectionItem), AVT_ClearWithValue, 0, TAG_DONE);
					ci_next = ci_next->ci_Next;
				} else {
					ci_new = AllocVecTags(sizeof(struct CollectionItem), AVT_ClearWithValue, 0, TAG_DONE);
					ci_next = ci_new;
				}
				
				utf8_from_enc(ci_curr->ci_Data,
						(const char *)ObtainCharsetInfo(DFCS_NUMBER,
										codeset, DFCS_MIMENAME),
					      ci_curr->ci_Size, (char **)&ci_next->ci_Data, NULL);
				ci_next->ci_Size = strlen(ci_next->ci_Data);
				len += ci_next->ci_Size;
			break;
		}
	} while (ci_curr = ci_curr->ci_Next);

	text = malloc(len);

	if(text == NULL) return NULL;

	/* p points to the end of the buffer. This is because the chunks are
	 * in the list in reverse order. */
	p = text + len;

	if(ci_new) {
		ci_curr = ci_new;
	} else {
		ci_curr = ci;
	}

	do {
		p -= ci_curr->ci_Size;
		memcpy(p, ci_curr->ci_Data, ci_curr->ci_Size);
		ci_next = ci_curr->ci_Next;
		
		if(ci_new) {
			free(ci_curr->ci_Data);
			FreeVec(ci_curr);
		}
	} while (ci_curr = ci_next);

	*text_length = len;
	return text;
}

void gui_get_clipboard(char **buffer, size_t *length)
{
	struct CollectionItem *ci = NULL;
	struct StoredProperty *sp = NULL;
	ULONG rlen=0,error;
	struct CSet *cset;

	if(OpenIFF(iffh,IFFF_READ)) return;
	
	if(CollectionChunk(iffh,ID_FTXT,ID_CHRS)) return;
	if(PropChunk(iffh,ID_FTXT,ID_CSET)) return;
	if(CollectionChunk(iffh,ID_FTXT,ID_UTF8)) return;
	if(StopOnExit(iffh, ID_FTXT, ID_FORM)) return;
	
	error = ParseIFF(iffh,IFFPARSE_SCAN);

	if(ci = FindCollection(iffh, ID_FTXT, ID_UTF8)) {
		*buffer = ami_clipboard_cat_collection(ci, 106, length);
	} else if(ci = FindCollection(iffh, ID_FTXT, ID_CHRS)) {
		LONG codeset = 0;
		if(sp = FindProp(iffh, ID_FTXT, ID_CSET)) {
			cset = (struct CSet *)sp->sp_Data;
			codeset = cset->CodeSet;
		}
		*buffer = ami_clipboard_cat_collection(ci, codeset, length);
	}

	CloseIFF(iffh);
}

static void gui_set_clipboard(const char *buffer, size_t length,
	nsclipboard_styles styles[], int n_styles)
{
	char *text;
	struct CSet cset = {0};

	if(buffer == NULL) return;

	if(!(OpenIFF(iffh, IFFF_WRITE)))
	{
		if(!(PushChunk(iffh, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)))
		{
			if(nsoption_bool(clipboard_write_utf8))
			{
				if(!(PushChunk(iffh, 0, ID_CSET, 32)))
				{
					cset.CodeSet = 106; // UTF-8
					WriteChunkBytes(iffh, &cset, 32);
					PopChunk(iffh);
				}
			}
		}
		else
		{
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh, 0, ID_CHRS, IFFSIZE_UNKNOWN))) {
			if(nsoption_bool(clipboard_write_utf8)) {
				WriteChunkBytes(iffh, buffer, length);
			} else {
				if(utf8_to_local_encoding(buffer, length, &text) == NSERROR_OK) {
					char *p;

					p = text;

					while(*p != '\0') {
						if(*p == 0xa0) *p = 0x20;
						p++;
					}
					WriteChunkBytes(iffh, text, strlen(text));
					ami_utf8_free(text);
				}
			}

			PopChunk(iffh);
		} else {
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh, 0, ID_UTF8, IFFSIZE_UNKNOWN))) {
			WriteChunkBytes(iffh, buffer, length);
			PopChunk(iffh);
		} else {
			PopChunk(iffh);
		}
		CloseIFF(iffh);
	}
}

void ami_drag_selection(struct gui_window *g)
{
	int x;
	int y;
	char *utf8text;
	char *sel;
	struct IFFHandle *old_iffh = iffh;
	struct gui_window_2 *gwin = ami_window_at_pointer(AMINS_WINDOW);
	
	/* NB: 'gwin' is at the drop point, 'g' is where the selection was dragged from.
	 * These may be different if the selection has been dragged between windows. */
	
	if(!gwin)
	{
		DisplayBeep(scrn);
		return;
	}

	x = gwin->win->MouseX;
	y = gwin->win->MouseY;

	if(ami_text_box_at_point(gwin, (ULONG *)&x, (ULONG *)&y))
	{
		iffh = ami_clipboard_init_internal(1);

		browser_window_key_press(g->shared->bw, KEY_COPY_SELECTION);
		browser_window_mouse_click(gwin->bw, BROWSER_MOUSE_PRESS_1, x, y);
		browser_window_key_press(gwin->bw, KEY_PASTE);

		ami_clipboard_free_internal(iffh);
		iffh = old_iffh;
	}
	else
	{
		x = gwin->win->MouseX;
		y = gwin->win->MouseY;

		if(ami_gadget_hit(gwin->objects[GID_URL], x, y))
		{
			if(sel = browser_window_get_selection(g->shared->bw))
			{
				utf8text = ami_utf8_easy(sel);
				RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_URL],
					gwin->win, NULL, STRINGA_TextVal, utf8text, TAG_DONE);
				free(sel);
				ami_utf8_free(utf8text);
			}
		}
		else if(ami_gadget_hit(gwin->objects[GID_SEARCHSTRING], x, y))
		{
			if(sel = browser_window_get_selection(g->shared->bw))
			{
				utf8text = ami_utf8_easy(sel);
				RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_SEARCHSTRING],
					gwin->win, NULL, STRINGA_TextVal, utf8text, TAG_DONE);
				free(sel);
				ami_utf8_free(utf8text);
			}
		}
		else
		{
			DisplayBeep(scrn);
		}
	}
}

bool ami_easy_clipboard(char *text)
{
	gui_set_clipboard(text, strlen(text), NULL, 0);
	return true;
}

bool ami_easy_clipboard_bitmap(struct bitmap *bitmap)
{
	Object *dto = NULL;

	if(dto = ami_datatype_object_from_bitmap(bitmap))
	{
		DoDTMethod(dto,NULL,NULL,DTM_COPY,NULL);
		DisposeDTObject(dto);
	}
}

#ifdef WITH_NS_SVG
bool ami_easy_clipboard_svg(struct hlcache_handle *c)
{
	const char *source_data;
	ULONG source_size;

	if(ami_mime_compare(c, "svg") == false) return false;
	if((source_data = content_get_source_data(c, &source_size)) == NULL) return false;

	if(!(OpenIFF(iffh,IFFF_WRITE)))
	{
		ami_svg_to_dr2d(iffh, source_data, source_size, nsurl_access(hlcache_handle_get_url(c)));
		CloseIFF(iffh);
	}

	return true;
}
#endif

static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *amiga_clipboard_table = &clipboard_table;
