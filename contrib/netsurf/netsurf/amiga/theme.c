/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <string.h>

#include <proto/clicktab.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>

#include <gadgets/clicktab.h>
#include <gadgets/space.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#endif
#include <intuition/pointerclass.h>
#include <workbench/icon.h>

#include "amiga/bitmap.h"
#include "amiga/drag.h"
#include "desktop/browser_private.h"
#include "utils/nsoption.h"
#include "amiga/theme.h"
#include "desktop/searchweb.h"
#include "utils/messages.h"
#include "utils/utils.h"

struct BitMap *throbber = NULL;
struct bitmap *throbber_nsbm = NULL;
ULONG throbber_frames,throbber_update_interval;
static Object *mouseptrobj[AMI_LASTPOINTER+1];
static struct BitMap *mouseptrbm[AMI_LASTPOINTER+1];

char *ptrs[AMI_LASTPOINTER+1] = {
	"ptr_default",
	"ptr_point",
	"ptr_caret",
	"ptr_menu",
	"ptr_up",
	"ptr_down",
	"ptr_left",
	"ptr_right",
	"ptr_rightup",
	"ptr_leftdown",
	"ptr_leftup",
	"ptr_rightdown",
	"ptr_cross",
	"ptr_move",
	"ptr_wait",
	"ptr_help",
	"ptr_nodrop",
	"ptr_notallowed",
	"ptr_progress",
	"ptr_blank",
	"ptr_drag"};

char *ptrs32[AMI_LASTPOINTER+1] = {
	"ptr32_default",
	"ptr32_point",
	"ptr32_caret",
	"ptr32_menu",
	"ptr32_up",
	"ptr32_down",
	"ptr32_left",
	"ptr32_right",
	"ptr32_rightup",
	"ptr32_leftdown",
	"ptr32_leftup",
	"ptr32_rightdown",
	"ptr32_cross",
	"ptr32_move",
	"ptr32_wait",
	"ptr32_help",
	"ptr32_nodrop",
	"ptr32_notallowed",
	"ptr32_progress",
	"ptr32_blank",
	"ptr32_drag"};

/* Mapping from NetSurf to AmigaOS mouse pointers */
int osmouseptr[AMI_LASTPOINTER+1] = {
	POINTERTYPE_NORMAL, 
	POINTERTYPE_LINK,
	POINTERTYPE_TEXT,
	POINTERTYPE_CONTEXTMENU,
	POINTERTYPE_NORTHRESIZE,
	POINTERTYPE_SOUTHRESIZE,
	POINTERTYPE_WESTRESIZE,
	POINTERTYPE_EASTRESIZE,
	POINTERTYPE_NORTHEASTRESIZE,
	POINTERTYPE_SOUTHWESTRESIZE,
	POINTERTYPE_NORTHWESTRESIZE,
	POINTERTYPE_SOUTHEASTRESIZE,
	POINTERTYPE_CROSS,
	POINTERTYPE_HAND,
	POINTERTYPE_BUSY,
	POINTERTYPE_HELP,
	POINTERTYPE_NODROP,
	POINTERTYPE_NOTALLOWED,
	POINTERTYPE_PROGRESS,
	POINTERTYPE_NONE,
	POINTERTYPE_DRAGANDDROP};


void ami_theme_init(void)
{
	char themefile[1024];
	char searchico[1024];
	BPTR lock = 0;

	strcpy(themefile,nsoption_charp(theme));
	AddPart(themefile,"Theme",100);

	lock = Lock(themefile,ACCESS_READ);

	if(!lock)
	{
		warn_user("ThemeApplyErr",nsoption_charp(theme));
		strcpy(themefile,"PROGDIR:Resources/Themes/Default/Theme");
		nsoption_set_charp(theme, (char *)strdup("PROGDIR:Resources/Themes/Default"));
	}
	else
	{
		UnLock(lock);
	}

	lock = Lock(themefile,ACCESS_READ);
	if(lock)
	{
		UnLock(lock);
		messages_load(themefile);
	}

	ami_get_theme_filename(searchico, "theme_search", false);
	search_default_ico_location = (char *)strdup(searchico);
}

void ami_theme_throbber_setup(void)
{
	char throbberfile[1024];
	struct bitmap *bm;

	ami_get_theme_filename(throbberfile,"theme_throbber",false);
	throbber_frames=atoi(messages_get("theme_throbber_frames"));
	throbber_update_interval = atoi(messages_get("theme_throbber_delay"));
	if(throbber_update_interval == 0) throbber_update_interval = 100;

	bm = ami_bitmap_from_datatype(throbberfile);
	throbber = ami_bitmap_get_native(bm, bm->width, bm->height, NULL);
				
	throbber_width = bm->width / throbber_frames;
	throbber_height = bm->height;
	throbber_nsbm = bm;
}

void ami_theme_throbber_free(void)
{
	bitmap_destroy(throbber_nsbm);
	throbber = NULL;
}

void ami_get_theme_filename(char *filename, char *themestring, bool protocol)
{
	if(protocol)
		strcpy(filename,"file:///");
	else
		strcpy(filename,"");

	if(messages_get(themestring)[0] == '*')
	{
		strncat(filename,messages_get(themestring)+1,100);
	}
	else
	{
		strcat(filename, nsoption_charp(theme));
		AddPart(filename, messages_get(themestring), 100);
	}
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	ami_set_pointer(g->shared, shape, true);
}

void ami_set_pointer(struct gui_window_2 *gwin, gui_pointer_shape shape, bool update)
{
	if(gwin->mouse_pointer == shape) return;
	ami_update_pointer(gwin->win, shape);
	if(update == true) gwin->mouse_pointer = shape;
}

/* reset the mouse pointer back to what NetSurf last set it as */
void ami_reset_pointer(struct gui_window_2 *gwin)
{
	ami_update_pointer(gwin->win, gwin->mouse_pointer);
}

void ami_update_pointer(struct Window *win, gui_pointer_shape shape)
{
	if(drag_save_data) return;

	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 53, 42)) {
		BOOL ptr_delay = FALSE;
		if(shape == GUI_POINTER_WAIT) ptr_delay = TRUE;

		SetWindowPointer(win,
					WA_PointerType, osmouseptr[shape],
					WA_PointerDelay, ptr_delay,
					TAG_DONE);					
	} else {
		if(nsoption_bool(os_mouse_pointers))
		{
			switch(shape)
			{
				case GUI_POINTER_DEFAULT:
					SetWindowPointer(win, TAG_DONE);
				break;

				case GUI_POINTER_WAIT:
					SetWindowPointer(win,
						WA_BusyPointer, TRUE,
						WA_PointerDelay, TRUE,
						TAG_DONE);
				break;

				default:
					if(mouseptrobj[shape]) {
						SetWindowPointer(win, WA_Pointer, mouseptrobj[shape], TAG_DONE);
					} else {
						SetWindowPointer(win, TAG_DONE);
					}
				break;
			}
		}
		else
		{
			if(mouseptrobj[shape])
			{
				SetWindowPointer(win, WA_Pointer, mouseptrobj[shape], TAG_DONE);
			}
			else
			{
				if(shape ==	GUI_POINTER_WAIT)
				{
					SetWindowPointer(win,
						WA_BusyPointer, TRUE,
						WA_PointerDelay, TRUE,
						TAG_DONE);
				}
				else
				{
					SetWindowPointer(win, TAG_DONE);
				}
			}
		}
	}
}

void ami_init_mouse_pointers(void)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 53, 42)) return;

	int i;
	struct RastPort mouseptr;
	struct DiskObject *dobj;
	uint32 format = IDFMT_BITMAPPED;
	int32 mousexpt=0,mouseypt=0;

	InitRastPort(&mouseptr);

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		BPTR ptrfile;
		mouseptrbm[i] = NULL;
		mouseptrobj[i] = NULL;
		char ptrfname[1024];

#ifdef __amigaos4__
		if(nsoption_bool(truecolour_mouse_pointers))
		{
			ami_get_theme_filename((char *)&ptrfname,ptrs32[i], false);
			if(dobj = GetIconTags(ptrfname,ICONGETA_UseFriendBitMap,TRUE,TAG_DONE))
			{
				if(IconControl(dobj, ICONCTRLA_GetImageDataFormat, &format, TAG_DONE))
				{
					if(IDFMT_DIRECTMAPPED == format)
					{
						int32 width = 0, height = 0;
						uint8* data = 0;
						IconControl(dobj,
							ICONCTRLA_GetWidth, &width,
							ICONCTRLA_GetHeight, &height,
							ICONCTRLA_GetImageData1, &data,
							TAG_DONE);

						if (width > 0 && width <= 64 && height > 0 && height <= 64 && data)
						{
							STRPTR tooltype;

							if(tooltype = FindToolType(dobj->do_ToolTypes, "XOFFSET"))
								mousexpt = atoi(tooltype);

							if(tooltype = FindToolType(dobj->do_ToolTypes, "YOFFSET"))
								mouseypt = atoi(tooltype);

							if (mousexpt < 0 || mousexpt >= width)
								mousexpt = 0;
							if (mouseypt < 0 || mouseypt >= height)
								mouseypt = 0;

							static uint8 dummyPlane[64 * 64 / 8];
                   			static struct BitMap dummyBitMap = { 64 / 8, 64, 0, 2, 0, { dummyPlane, dummyPlane, 0, 0, 0, 0, 0, 0 }, };

							mouseptrobj[i] = NewObject(NULL, "pointerclass",
												POINTERA_BitMap, &dummyBitMap,
												POINTERA_XOffset, -mousexpt,
												POINTERA_YOffset, -mouseypt,
												POINTERA_WordWidth, (width + 15) / 16,
												POINTERA_XResolution, POINTERXRESN_SCREENRES,
												POINTERA_YResolution, POINTERYRESN_SCREENRESASPECT,
												POINTERA_ImageData, data,
												POINTERA_Width, width,
												POINTERA_Height, height,
												TAG_DONE);
						}
					}
				}
			}
		}
#endif

		if(!mouseptrobj[i])
		{
			ami_get_theme_filename(ptrfname,ptrs[i], false);
			if(ptrfile = Open(ptrfname,MODE_OLDFILE))
			{
				int mx,my;
				UBYTE *pprefsbuf = AllocVecTagList(1061, NULL);
				Read(ptrfile,pprefsbuf,1061);

				mouseptrbm[i]=AllocVecTagList(sizeof(struct BitMap), NULL);
				InitBitMap(mouseptrbm[i],2,32,32);
				mouseptrbm[i]->Planes[0] = AllocRaster(32,32);
				mouseptrbm[i]->Planes[1] = AllocRaster(32,32);
				mouseptr.BitMap = mouseptrbm[i];

				for(my=0;my<32;my++)
				{
					for(mx=0;mx<32;mx++)
					{
						SetAPen(&mouseptr,pprefsbuf[(my*(33))+mx]-'0');
						WritePixel(&mouseptr,mx,my);
					}
				}

				mousexpt = ((pprefsbuf[1056]-'0')*10)+(pprefsbuf[1057]-'0');
				mouseypt = ((pprefsbuf[1059]-'0')*10)+(pprefsbuf[1060]-'0');

				mouseptrobj[i] = NewObject(NULL,"pointerclass",
					POINTERA_BitMap,mouseptrbm[i],
					POINTERA_WordWidth,2,
					POINTERA_XOffset,-mousexpt,
					POINTERA_YOffset,-mouseypt,
					POINTERA_XResolution,POINTERXRESN_SCREENRES,
					POINTERA_YResolution,POINTERYRESN_SCREENRESASPECT,
					TAG_DONE);

				FreeVec(pprefsbuf);
				Close(ptrfile);
			}

		}

	} // for
}

void ami_mouse_pointers_free(void)
{
	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 53, 42)) return;

	int i;

	for(i=0;i<=AMI_LASTPOINTER;i++)
	{
		if(mouseptrbm[i])
		{
			FreeRaster(mouseptrbm[i]->Planes[0],32,32);
			FreeRaster(mouseptrbm[i]->Planes[1],32,32);
			FreeVec(mouseptrbm[i]);
		}
	}
}

void gui_window_start_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;
	if(nsoption_bool(kiosk_mode)) return;

	if(g->tab_node && (g->shared->tabs > 1))
	{
		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
				(ULONG *)&cur_tab);
		SetClickTabNodeAttrs(g->tab_node, TNA_Flagged, TRUE, TAG_DONE);
		RefreshGadgets((APTR)g->shared->objects[GID_TABS],
			g->shared->win, NULL);
	}

	g->throbbing = true;

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_THROBBER],
				(ULONG *)&bbox);

		if(g->shared->throbber_frame == 0) g->shared->throbber_frame=1;

		BltBitMapRastPort(throbber,throbber_width,0,g->shared->win->RPort,bbox->Left,bbox->Top,throbber_width,throbber_height,0x0C0);
	}
}

void gui_window_stop_throbber(struct gui_window *g)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;
	if(nsoption_bool(kiosk_mode)) return;

	if(g->tab_node && (g->shared->tabs > 1))
	{
		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
			(ULONG *)&cur_tab);
		SetClickTabNodeAttrs(g->tab_node, TNA_Flagged, FALSE, TAG_DONE);
		RefreshGadgets((APTR)g->shared->objects[GID_TABS],
			g->shared->win, NULL);
	}

	g->throbbing = false;

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_THROBBER],
				(ULONG *)&bbox);

		BltBitMapRastPort(throbber, 0, 0, g->shared->win->RPort, bbox->Left,
			bbox->Top, throbber_width, throbber_height, 0x0C0);
	}
//	g->shared->throbber_frame = 0;
}

void ami_update_throbber(struct gui_window_2 *g, bool redraw)
{
	struct IBox *bbox;
	int frame;

	if(!g) return;
	if(!g->objects[GID_THROBBER]) return;

	if(g->bw->window->throbbing == false)
	{
		frame = 0;
		g->throbber_frame = 1;
	}
	else
	{
		frame = g->throbber_frame;

		if(!redraw)
		{
			if(g->throbber_update_count < throbber_update_interval)
			{
				g->throbber_update_count++;
				return;
			}

			g->throbber_update_count = 0;

			g->throbber_frame++;
			if(g->throbber_frame > (throbber_frames-1))
				g->throbber_frame=1;

		}
	}

	GetAttr(SPACE_AreaBox,(Object *)g->objects[GID_THROBBER],(ULONG *)&bbox);

/*
	EraseRect(g->win->RPort,bbox->Left,bbox->Top,
		bbox->Left+throbber_width,bbox->Top+throbber_height);
*/

	BltBitMapTags(BLITA_SrcX, throbber_width * frame,
					BLITA_SrcY,0,
					BLITA_DestX,bbox->Left,
					BLITA_DestY,bbox->Top,
					BLITA_Width,throbber_width,
					BLITA_Height,throbber_height,
					BLITA_Source,throbber,
					BLITA_Dest,g->win->RPort,
					BLITA_SrcType,BLITT_BITMAP,
					BLITA_DestType,BLITT_RASTPORT,
//					BLITA_UseSrcAlpha,TRUE,
					TAG_DONE);
}
