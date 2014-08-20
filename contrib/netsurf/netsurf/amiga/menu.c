/*
 * Copyright 2008-9,2013 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/dos.h>
#include <proto/asl.h>
#include <proto/exec.h>
#include <proto/gadtools.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#ifdef __amigaos4__
#include <dos/anchorpath.h>
#endif

#include <libraries/gadtools.h>

#include <classes/window.h>
#include <proto/label.h>
#include <images/label.h>
#include <proto/bitmap.h>
#include <images/bitmap.h>
#include <proto/glyph.h>
#include <images/glyph.h>

#include <reaction/reaction_macros.h>

#include "amiga/arexx.h"
#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/cookies.h"
#include "amiga/file.h"
#include "amiga/filetype.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "amiga/history.h"
#include "amiga/history_local.h"
#include "amiga/hotlist.h"
#include "amiga/menu.h"
#include "utils/nsoption.h"
#include "amiga/print.h"
#include "amiga/search.h"
#include "amiga/theme.h"
#include "amiga/tree.h"
#include "amiga/utf8.h"
#include "amiga/schedule.h"
#include "desktop/hotlist.h"
#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/textinput.h"
#include "utils/messages.h"


enum {
	NSA_GLYPH_SUBMENU,
	NSA_GLYPH_AMIGAKEY,
	NSA_GLYPH_CHECKMARK,
	NSA_GLYPH_MX,
	NSA_GLYPH_MAX
};

BOOL menualreadyinit;
const char * const netsurf_version;
const char * const verdate;
Object *menu_glyph[NSA_GLYPH_MAX];
int menu_glyph_width[NSA_GLYPH_MAX];
bool menu_glyphs_loaded = false;

static nserror ami_menu_scan(struct tree *tree, struct gui_window_2 *gwin);
void ami_menu_arexx_scan(struct gui_window_2 *gwin);

/* Functions for menu selections */
static void ami_menu_item_project_newwin(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_newtab(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_open(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_save(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_closetab(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_closewin(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_print(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_about(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_project_quit(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_cut(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_copy(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_paste(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_selectall(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_clearsel(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_undo(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_edit_redo(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_find(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_localhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_globalhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_cookies(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_foreimg(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_backimg(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_enablejs(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_decrease(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_normal(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_scale_increase(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_browser_redraw(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_add(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_show(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_hotlist_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_edit(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_snapshot(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_settings_save(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_arexx_execute(struct Hook *hook, APTR window, struct IntuiMessage *msg);
static void ami_menu_item_arexx_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg);


void ami_free_menulabs(struct gui_window_2 *gwin)
{
	int i;

	for(i=0;i<=AMI_MENU_AREXX_MAX;i++)
	{
		if(gwin->menulab[i] && (gwin->menulab[i] != NM_BARLABEL))
		{
			if(gwin->menutype[i] & MENU_IMAGE)
			{
				DisposeObject(gwin->menuobj[i]);
			}

			ami_utf8_free(gwin->menulab[i]);

			if(i >= AMI_MENU_AREXX)
			{
				if(gwin->menu_hook[i].h_Data) free(gwin->menu_hook[i].h_Data);
			}
		}

		gwin->menulab[i] = NULL;
		gwin->menuobj[i] = NULL;
		gwin->menukey[i] = 0;
	}

	FreeVec(gwin->menutype);
	FreeVec(gwin->menu);

	gwin->menutype = NULL;
	gwin->menu = NULL;
}

static void ami_menu_alloc_item(struct gui_window_2 *gwin, int num, UBYTE type,
			const char *label, char key, char *icon, void *func, void *hookdata)
{
	char menu_icon[1024];

	gwin->menutype[num] = type;

	if((label == NM_BARLABEL) || (strcmp(label, "--") == 0)) {
		gwin->menulab[num] = NM_BARLABEL;
	} else {
		if((num >= AMI_MENU_HOTLIST) && (num <= AMI_MENU_HOTLIST_MAX)) {
			gwin->menulab[num] = ami_utf8_easy(label);
		} else if((num >= AMI_MENU_AREXX) && (num <= AMI_MENU_AREXX_MAX)) {
			gwin->menulab[num] = strdup(label);		
		} else {
			gwin->menulab[num] = ami_utf8_easy(messages_get(label));
		}
	}
	
	gwin->menuicon[num] = NULL;
	if(key) gwin->menukey[num] = key;
	if(func) gwin->menu_hook[num].h_Entry = (HOOKFUNC)func;
	if(hookdata) gwin->menu_hook[num].h_Data = hookdata;

	if(icon) {
		if(ami_locate_resource(menu_icon, icon) == true)
			gwin->menuicon[num] = (char *)strdup(menu_icon);
	}
}

void ami_init_menulabs(struct gui_window_2 *gwin)
{
	int i;

	gwin->menutype = AllocVecTags(AMI_MENU_AREXX_MAX + 1, AVT_ClearWithValue, 0, TAG_DONE);

	for(i=0;i <= AMI_MENU_AREXX_MAX;i++)
	{
		gwin->menutype[i] = NM_IGNORE;
		gwin->menulab[i] = NULL;
		gwin->menuobj[i] = NULL;
	}

	ami_menu_alloc_item(gwin, M_PROJECT, NM_TITLE, "Project",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_NEWWIN,   NM_ITEM, "NewWindowNS", 'N', NULL,
			ami_menu_item_project_newwin, NULL);
	ami_menu_alloc_item(gwin, M_NEWTAB,   NM_ITEM, "NewTab",      'T', NULL,
			ami_menu_item_project_newtab, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_OPEN,     NM_ITEM, "OpenFile",    'O', NULL,
			ami_menu_item_project_open, NULL);
	ami_menu_alloc_item(gwin, M_SAVEAS,   NM_ITEM, "SaveAsNS",      0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SAVESRC,   NM_SUB, "Source",      'S', NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_SOURCE);
	ami_menu_alloc_item(gwin, M_SAVETXT,   NM_SUB, "TextNS",        0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_TEXT);
	ami_menu_alloc_item(gwin, M_SAVECOMP,  NM_SUB, "SaveCompNS",    0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_COMPLETE);
#ifdef WITH_PDF_EXPORT
	ami_menu_alloc_item(gwin, M_SAVEPDF,   NM_SUB, "PDFNS",         0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_PDF);
#endif
	ami_menu_alloc_item(gwin, M_SAVEIFF,   NM_SUB, "IFF",           0, NULL,
			ami_menu_item_project_save, (void *)AMINS_SAVE_IFF);
	ami_menu_alloc_item(gwin, M_BAR_P2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_PRINT,    NM_ITEM, "PrintNS",     'P', NULL,
			ami_menu_item_project_print, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P3,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_CLOSETAB, NM_ITEM, "CloseTab",    'K', NULL,
			ami_menu_item_project_closetab, NULL);
	ami_menu_alloc_item(gwin, M_CLOSEWIN, NM_ITEM, "CloseWindow",   0, NULL,
			ami_menu_item_project_closewin, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P4,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);				
	ami_menu_alloc_item(gwin, M_ABOUT,    NM_ITEM, "About",       '?', NULL,
			ami_menu_item_project_about, NULL);
	ami_menu_alloc_item(gwin, M_BAR_P5,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);				
	ami_menu_alloc_item(gwin, M_QUIT,     NM_ITEM, "Quit",        'Q', NULL,
			ami_menu_item_project_quit, NULL);

	ami_menu_alloc_item(gwin, M_EDIT,    NM_TITLE, "Edit",          0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_CUT,      NM_ITEM, "CutNS",       'X', NULL,
			ami_menu_item_edit_cut, NULL);
	ami_menu_alloc_item(gwin, M_COPY,     NM_ITEM, "CopyNS",      'C', NULL,
			ami_menu_item_edit_copy, NULL);
	ami_menu_alloc_item(gwin, M_PASTE,    NM_ITEM, "PasteNS",     'V', NULL,
			ami_menu_item_edit_paste, NULL);
	ami_menu_alloc_item(gwin, M_BAR_E1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SELALL,   NM_ITEM, "SelectAllNS", 'A', NULL,
			ami_menu_item_edit_selectall, NULL);
	ami_menu_alloc_item(gwin, M_CLEAR,    NM_ITEM, "ClearNS",       0, NULL,
			ami_menu_item_edit_clearsel, NULL);
	ami_menu_alloc_item(gwin, M_BAR_E2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_UNDO,     NM_ITEM, "Undo",        'Z', NULL,
			ami_menu_item_edit_undo, NULL);
	ami_menu_alloc_item(gwin, M_REDO,     NM_ITEM, "Redo",        'Y', NULL,
			ami_menu_item_edit_redo, NULL);

	ami_menu_alloc_item(gwin, M_BROWSER, NM_TITLE, "Browser",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_FIND,     NM_ITEM, "FindTextNS",   'F', NULL,
			ami_menu_item_browser_find, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_HISTLOCL, NM_ITEM, "HistLocalNS",   0, NULL,
			ami_menu_item_browser_localhistory, NULL);
	ami_menu_alloc_item(gwin, M_HISTGLBL, NM_ITEM, "HistGlobalNS",  0, NULL,
			ami_menu_item_browser_globalhistory, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B2,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_COOKIES,  NM_ITEM, "ShowCookiesNS",   0, NULL,
			ami_menu_item_browser_cookies, NULL);
	ami_menu_alloc_item(gwin, M_BAR_B3,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SCALE,    NM_ITEM, "ScaleNS",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SCALEDEC,  NM_SUB, "ScaleDec",    '-', NULL,
			ami_menu_item_browser_scale_decrease, NULL);
	ami_menu_alloc_item(gwin, M_SCALENRM,  NM_SUB, "ScaleNorm",   '=', NULL,
			ami_menu_item_browser_scale_normal, NULL);
	ami_menu_alloc_item(gwin, M_SCALEDEC,  NM_SUB, "ScaleDec",    '-', NULL,
			ami_menu_item_browser_scale_decrease, NULL);
	ami_menu_alloc_item(gwin, M_SCALEINC,  NM_SUB, "ScaleInc",    '+', NULL,
			ami_menu_item_browser_scale_increase, NULL);
	ami_menu_alloc_item(gwin, M_IMAGES,   NM_ITEM, "Images",        0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_IMGFORE,   NM_SUB, "ForeImg",       0, NULL,
			ami_menu_item_browser_foreimg, NULL);
	ami_menu_alloc_item(gwin, M_IMGBACK,   NM_SUB, "BackImg",       0, NULL,
			ami_menu_item_browser_backimg, NULL);
#if defined(WITH_JS) || defined(WITH_MOZJS)
	ami_menu_alloc_item(gwin, M_JS,       NM_ITEM, "EnableJS",      0, NULL,
			ami_menu_item_browser_enablejs, NULL);
#endif
	ami_menu_alloc_item(gwin, M_BAR_B4,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_REDRAW,   NM_ITEM, "Redraw",        0, NULL,
			ami_menu_item_browser_redraw, NULL);

	ami_menu_alloc_item(gwin, M_HOTLIST, NM_TITLE, "Hotlist",       0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_HLADD,    NM_ITEM, "HotlistAdd",  'B', NULL,
			ami_menu_item_hotlist_add, NULL);
	ami_menu_alloc_item(gwin, M_HLSHOW,   NM_ITEM,"HotlistShowNS",'H', NULL,
			ami_menu_item_hotlist_show, NULL);
	ami_menu_alloc_item(gwin, M_BAR_H1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);

	ami_menu_alloc_item(gwin, M_PREFS,   NM_TITLE, "Settings",      0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_PREDIT,   NM_ITEM, "SettingsEdit",  0, NULL,
			ami_menu_item_settings_edit, NULL);
	ami_menu_alloc_item(gwin, M_BAR_S1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_SNAPSHOT, NM_ITEM, "SnapshotWindow",0, NULL,
			ami_menu_item_settings_snapshot, NULL);
	ami_menu_alloc_item(gwin, M_PRSAVE,   NM_ITEM, "SettingsSave",  0, NULL,
			ami_menu_item_settings_save, NULL);

	ami_menu_alloc_item(gwin, M_AREXX,   NM_TITLE, "ARexx",         0, NULL, NULL, NULL);
	ami_menu_alloc_item(gwin, M_AREXXEX,  NM_ITEM, "ARexxExecute",'E', NULL,
			ami_menu_item_arexx_execute, NULL);
	ami_menu_alloc_item(gwin, M_BAR_A1,   NM_ITEM, NM_BARLABEL,     0, NULL, NULL, NULL);
	gwin->menutype[AMI_MENU_AREXX_MAX] = NM_END;
}

/* Menu refresh for hotlist */
void ami_menu_refresh(struct gui_window_2 *gwin)
{
	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_NewMenu, NULL,
			TAG_DONE);

	ami_free_menulabs(gwin);
	ami_create_menu(gwin);

	SetAttrs(gwin->objects[OID_MAIN],
			WINDOW_NewMenu, gwin->menu,
			TAG_DONE);
}

static void ami_menu_load_glyphs(struct DrawInfo *dri)
{
	for(int i = 0; i < NSA_GLYPH_MAX; i++)
		menu_glyph[i] = NULL;

	menu_glyph[NSA_GLYPH_SUBMENU] = NewObject(NULL, "sysiclass",
										SYSIA_Which, MENUSUB,
										SYSIA_DrawInfo, dri,
									TAG_DONE);
	menu_glyph[NSA_GLYPH_AMIGAKEY] = NewObject(NULL, "sysiclass",
										SYSIA_Which, AMIGAKEY,
										SYSIA_DrawInfo, dri,
									TAG_DONE);
	GetAttr(IA_Width, menu_glyph[NSA_GLYPH_SUBMENU],
		(ULONG *)&menu_glyph_width[NSA_GLYPH_SUBMENU]);
	GetAttr(IA_Width, menu_glyph[NSA_GLYPH_AMIGAKEY],
		(ULONG *)&menu_glyph_width[NSA_GLYPH_AMIGAKEY]);
	
	menu_glyphs_loaded = true;
}

void ami_menu_free_glyphs(void)
{
	int i;
	if(menu_glyphs_loaded == false) return;
	
	for(i = 0; i < NSA_GLYPH_MAX; i++) {
		if(menu_glyph[i]) DisposeObject(menu_glyph[i]);
		menu_glyph[i] = NULL;
	};
	
	menu_glyphs_loaded = false;
}

static struct gui_window_2 *ami_menu_layout(struct gui_window_2 *gwin)
{
	int i, j;
	int txtlen = 0;
	struct RastPort *rp = &scrn->RastPort;
	struct DrawInfo *dri = GetScreenDrawInfo(scrn);
	
	if(menu_glyphs_loaded == false)
		ami_menu_load_glyphs(dri);

	for(i=0; i <= AMI_MENU_AREXX_MAX; i++)
	{
		if(gwin->menutype[i] == NM_TITLE) {
			j = i + 1;
			txtlen = 0;
			int item_size = 0;
			do {
				if(gwin->menulab[j] != NM_BARLABEL) {
					if(gwin->menutype[j] == NM_ITEM) {
						item_size = TextLength(rp, gwin->menulab[j], strlen(gwin->menulab[j]));
						if(gwin->menukey[j]) {
							item_size += TextLength(rp, &gwin->menukey[j], 1);
							item_size += menu_glyph_width[NSA_GLYPH_AMIGAKEY];
							/**TODO: take account of the size of other imagery too
							 */
						}
						
						if(item_size > txtlen) {
							txtlen = item_size;
						}
					}
				}
				j++;
			} while((gwin->menutype[j] != NM_TITLE) && (gwin->menutype[j] != 0));
		}

		if(LIB_IS_AT_LEAST((struct Library *)GadToolsBase, 53, 6)) {
			/* GadTools 53.6+ only. For now we will only create the menu
				using label.image if there's a bitmap associated with the item. */
			if((gwin->menuicon[i] != NULL) && (gwin->menulab[i] != NM_BARLABEL)) {
				int icon_width = 0;
				Object *submenuarrow = NULL;
				Object *icon = 	BitMapObject,
						BITMAP_Screen, scrn,
						BITMAP_SourceFile, gwin->menuicon[i],
						BITMAP_Masking, TRUE,
					BitMapEnd;
				GetAttr(IA_Width, icon, (ULONG *)&icon_width);
				
				if((gwin->menutype[i] == NM_ITEM) && (gwin->menutype[i+1] == NM_SUB)) {
					submenuarrow = NewObject(NULL, "sysiclass",
										SYSIA_Which, MENUSUB,
										SYSIA_DrawInfo, dri,
										IA_Left, txtlen - TextLength(rp, gwin->menulab[i], strlen(gwin->menulab[i])) -
													menu_glyph_width[NSA_GLYPH_SUBMENU] - icon_width,
									TAG_DONE);
				}

				/**TODO: Checkmark/MX images and keyboard shortcuts
				 */
				
				gwin->menuobj[i] = LabelObject,
					LABEL_DrawInfo, dri,
					LABEL_DisposeImage, TRUE,
					LABEL_Image, icon,
					LABEL_Text, gwin->menulab[i],
					LABEL_DisposeImage, TRUE,
					LABEL_Image, submenuarrow,
				LabelEnd;

				if(gwin->menuobj[i]) gwin->menutype[i] |= MENU_IMAGE;
			}
		}

		gwin->menu[i].nm_Type = gwin->menutype[i];
		
		if(gwin->menuobj[i])
			gwin->menu[i].nm_Label = (void *)gwin->menuobj[i];
		else
			gwin->menu[i].nm_Label = gwin->menulab[i];

		if(gwin->menukey[i]) gwin->menu[i].nm_CommKey = &gwin->menukey[i];
		gwin->menu[i].nm_Flags = 0;
		if(gwin->menu_hook[i].h_Entry) gwin->menu[i].nm_UserData = &gwin->menu_hook[i];
		
		if(gwin->menuicon[i]) {
			free(gwin->menuicon[i]);
			gwin->menuicon[i] = NULL;
		}
	}
	
	FreeScreenDrawInfo(scrn, dri);
	
	return gwin;
}

struct NewMenu *ami_create_menu(struct gui_window_2 *gwin)
{
	gwin->menu = AllocVecTags(sizeof(struct NewMenu) * (AMI_MENU_AREXX_MAX + 1),
					AVT_ClearWithValue, 0, TAG_DONE);
	ami_init_menulabs(gwin);
	ami_menu_scan(ami_tree_get_tree(hotlist_window), gwin);
	ami_menu_arexx_scan(gwin);
	gwin = ami_menu_layout(gwin);

#if defined(WITH_JS) || defined(WITH_MOZJS)
	gwin->menu[M_JS].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(enable_javascript) == true)
		gwin->menu[M_JS].nm_Flags |= CHECKED;
#endif

	gwin->menu[M_PRINT].nm_Flags = NM_ITEMDISABLED;

	gwin->menu[M_IMGFORE].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(foreground_images) == true)
		gwin->menu[M_IMGFORE].nm_Flags |= CHECKED;
	gwin->menu[M_IMGBACK].nm_Flags = CHECKIT | MENUTOGGLE;
	if(nsoption_bool(background_images) == true)
		gwin->menu[M_IMGBACK].nm_Flags |= CHECKED;

	/* Set up scheduler to refresh the hotlist menu */
	if(nsoption_int(menu_refresh) > 0)
	{
		ami_schedule(nsoption_int(menu_refresh) * 10,
			     (void *)ami_menu_refresh,
			     gwin);
	}

	return(gwin->menu);
}

void ami_menu_arexx_scan(struct gui_window_2 *gwin)
{
	int item = AMI_MENU_AREXX;
	BPTR lock = 0;
	UBYTE *buffer;
	struct ExAllControl *ctrl;
	char matchpatt[16];
	LONG cont;
	struct ExAllData *ead;
	char *menu_lab;

	if(lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK))
	{
		if(buffer = AllocVecTagList(1024, NULL))
		{
			if(ctrl = AllocDosObject(DOS_EXALLCONTROL,NULL))
			{
				ctrl->eac_LastKey = 0;

				if(ParsePatternNoCase("#?.nsrx",(char *)&matchpatt,16) != -1)
				{
					ctrl->eac_MatchString = (char *)&matchpatt;
				}

				do
				{
					cont = ExAll(lock,(struct ExAllData *)buffer,1024,ED_COMMENT,ctrl);
					if((!cont) && (IoErr() != ERROR_NO_MORE_ENTRIES)) break;
					if(!ctrl->eac_Entries) continue;

					for(ead = (struct ExAllData *)buffer; ead; ead = ead->ed_Next)
					{
						if(item >= AMI_MENU_AREXX_MAX) continue;
						if(EAD_IS_FILE(ead))
						{
							gwin->menu[item].nm_Type = NM_ITEM;
							if(ead->ed_Comment[0] != '\0')
								menu_lab = ead->ed_Comment;
							else
								menu_lab = ead->ed_Name;

							ami_menu_alloc_item(gwin, item, NM_ITEM, menu_lab, 0, NULL,
								ami_menu_item_arexx_entries, (void *)strdup(ead->ed_Name));

							item++;
						}
					}
				}while(cont);
				FreeDosObject(DOS_EXALLCONTROL,ctrl);
			}
			FreeVec(buffer);
		}
		UnLock(lock);
	}

	gwin->menu[item].nm_Type = NM_END;
	gwin->menu[item].nm_Label = NULL;
}

static bool ami_menu_hotlist_add(void *userdata, int level, int item, const char *title, nsurl *url, bool is_folder)
{
	UBYTE type;
	char *icon;
	struct gui_window_2 *gw = (struct gui_window_2 *)userdata;
	
	if(item >= AMI_MENU_HOTLIST_MAX) return false;
	
	switch(level) {
		case 1:
			type = NM_ITEM;
		break;
		case 2:
			type = NM_SUB;
		break;
		default:
			/* entries not at level 1 or 2 are not able to be added */
			return false;
		break;
	}

	if(is_folder == true) {
		icon = "icons/directory.png";
	} else {
		icon = "icons/content.png";
	}

	ami_menu_alloc_item(gw, item, type, title,
		0, icon, ami_menu_item_hotlist_entries, (void *)url);
	if((is_folder == true) && (type == NM_SUB))
		gw->menu[item].nm_Flags = NM_ITEMDISABLED;

	return true;
}

static nserror ami_menu_scan(struct tree *tree, struct gui_window_2 *gwin)
{
	return ami_hotlist_scan((void *)gwin, AMI_MENU_HOTLIST, messages_get("HotlistMenu"), ami_menu_hotlist_add);
}

void ami_menu_update_checked(struct gui_window_2 *gwin)
{
	struct Menu *menustrip;

	GetAttr(WINDOW_MenuStrip, gwin->objects[OID_MAIN], (ULONG *)&menustrip);
	if(!menustrip) return;
#if defined(WITH_JS) || defined(WITH_MOZJS)
	if(nsoption_bool(enable_javascript) == true) {
		if((ItemAddress(menustrip, AMI_MENU_JS)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_JS)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_JS)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_JS)->Flags ^= CHECKED;
	}
#endif
	if(nsoption_bool(foreground_images) == true) {
		if((ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_FOREIMG)->Flags ^= CHECKED;
	}

	if(nsoption_bool(background_images) == true) {
		if((ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags & CHECKED) == 0)
			ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags ^= CHECKED;
	} else {
		if(ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags & CHECKED)
			ItemAddress(menustrip, AMI_MENU_BACKIMG)->Flags ^= CHECKED;
	}

	ResetMenuStrip(gwin->win, menustrip);
}

void ami_menu_update_disabled(struct gui_window *g, hlcache_handle *c)
{
	struct Window *win = g->shared->win;

	if(nsoption_bool(kiosk_mode) == true) return;

	if(content_get_type(c) <= CONTENT_CSS)
	{
		OnMenu(win,AMI_MENU_SAVEAS_TEXT);
		OnMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OnMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
		if(browser_window_get_editor_flags(g->shared->bw) & BW_EDITOR_CAN_COPY)
		{
			OnMenu(win,AMI_MENU_COPY);
			OnMenu(win,AMI_MENU_CLEAR);
		} else {
			OffMenu(win,AMI_MENU_COPY);
			OffMenu(win,AMI_MENU_CLEAR);	
		}

		if(browser_window_get_editor_flags(g->shared->bw) & BW_EDITOR_CAN_CUT)
			OnMenu(win,AMI_MENU_CUT);
		else
			OffMenu(win,AMI_MENU_CUT);		
		
		if(browser_window_get_editor_flags(g->shared->bw) & BW_EDITOR_CAN_PASTE)
			OnMenu(win,AMI_MENU_PASTE);
		else
			OffMenu(win,AMI_MENU_PASTE);

		OnMenu(win,AMI_MENU_SELECTALL);
		OnMenu(win,AMI_MENU_FIND);
		OffMenu(win,AMI_MENU_SAVEAS_IFF);
	}
	else
	{
		OffMenu(win,AMI_MENU_CUT);
		OffMenu(win,AMI_MENU_PASTE);
		OffMenu(win,AMI_MENU_CLEAR);

		OffMenu(win,AMI_MENU_SAVEAS_TEXT);
		OffMenu(win,AMI_MENU_SAVEAS_COMPLETE);
#ifdef WITH_PDF_EXPORT
		OffMenu(win,AMI_MENU_SAVEAS_PDF);
#endif
		OffMenu(win,AMI_MENU_SELECTALL);
		OffMenu(win,AMI_MENU_FIND);

#ifdef WITH_NS_SVG
		if(content_get_bitmap(c) || (ami_mime_compare(c, "svg") == true))
#else
		if(content_get_bitmap(c))
#endif
		{
			OnMenu(win,AMI_MENU_COPY);
			OnMenu(win,AMI_MENU_SAVEAS_IFF);
		}
		else
		{
			OffMenu(win,AMI_MENU_COPY);
			OffMenu(win,AMI_MENU_SAVEAS_IFF);
		}
	}
}

/*
 * The below functions are called automatically by window.class when menu items are selected.
 */

static void ami_menu_item_project_newwin(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsurl *url;
	nserror error;

	error = nsurl_create(nsoption_charp(homepage_url), &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

static void ami_menu_item_project_newtab(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	nserror error;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);
	error = ami_gui_new_blank_tab(gwin);
}

static void ami_menu_item_project_open(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_open(gwin);
}

static void ami_menu_item_project_save(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	ULONG type = (ULONG)hook->h_Data;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_file_save_req(type, gwin, gwin->bw->current_content);
}

static void ami_menu_item_project_closetab(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_destroy(gwin->bw);
}

static void ami_menu_item_project_closewin(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_menu_window_close = gwin;
}

static void ami_menu_item_project_print(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);
	ami_print_ui(gwin->bw->current_content);
	ami_reset_pointer(gwin);
}

static void ami_menu_item_project_about(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	char *temp, *temp2;
	int sel;
	nsurl *url = NULL;
	nserror error;

	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	temp = ASPrintf("%s|%s|%s", messages_get("OK"),
								messages_get("HelpCredits"),
								messages_get("HelpLicence"));

	temp2 = ami_utf8_easy(temp);
	FreeVec(temp);

	sel = TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_INFO,
				TDR_TitleString, messages_get("NetSurf"),
				TDR_Window, gwin->win,
				TDR_GadgetString, temp2,
#ifndef NDEBUG
				TDR_FormatString,"NetSurf %s\n%s\nBuild date %s\n\nhttp://www.netsurf-browser.org",
#else
				TDR_FormatString,"NetSurf %s\n%s\n\nhttp://www.netsurf-browser.org",
#endif
				TDR_Arg1,netsurf_version,
#ifdef NS_AMIGA_CAIRO
				TDR_Arg2,"Cairo (OS4.1+) SObjs build",
#else
				TDR_Arg2,"graphics.library static build",
#endif
				TDR_Arg3,verdate,
				TAG_DONE);

	free(temp2);

	if(sel == 2) {
		error = nsurl_create("about:credits", &url);
	} else if(sel == 0) {
		error = nsurl_create("about:licence", &url);
	}

	if(url) {
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
							  url,
							  NULL,
							  NULL,
							  NULL);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
	}

	ami_reset_pointer(gwin);
}

static void ami_menu_item_project_quit(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_menu_window_close = AMI_MENU_WINDOW_CLOSE_ALL;
}

static void ami_menu_item_edit_cut(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
}

static void ami_menu_item_edit_copy(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct bitmap *bm;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(content_get_type(gwin->bw->current_content) <= CONTENT_CSS)
	{
		browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
		browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
	}
	else if(bm = content_get_bitmap(gwin->bw->current_content))
	{
		bm->url = (char *)nsurl_access(hlcache_handle_get_url(gwin->bw->current_content));
		bm->title = (char *)content_get_title(gwin->bw->current_content);
		ami_easy_clipboard_bitmap(bm);
	}
#ifdef WITH_NS_SVG
	else if(ami_mime_compare(gwin->bw->current_content, "svg") == true)
	{
		ami_easy_clipboard_svg(gwin->bw->current_content);
	}
#endif
}

static void ami_menu_item_edit_paste(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_PASTE);
}

static void ami_menu_item_edit_selectall(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
	gui_start_selection(gwin->bw->window);
}

static void ami_menu_item_edit_clearsel(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
}

static void ami_menu_item_edit_undo(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_UNDO);
}

static void ami_menu_item_edit_redo(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_key_press(gwin->bw, KEY_REDO);
}

static void ami_menu_item_browser_find(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_search_open(gwin->bw->window);
}

static void ami_menu_item_browser_localhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(gwin->bw && gwin->bw->history)
		ami_history_open(gwin->bw, gwin->bw->history);
}

static void ami_menu_item_browser_globalhistory(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(global_history_window,AMI_TREE_HISTORY);
}

static void ami_menu_item_browser_cookies(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(cookies_window,AMI_TREE_COOKIES);
}

static void ami_menu_item_browser_foreimg(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(foreground_images, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_backimg(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(background_images, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_enablejs(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct Menu *menustrip;
	bool checked = false;

	GetAttr(WINDOW_MenuStrip, (Object *)window, (ULONG *)&menustrip);
	if(ItemAddress(menustrip, msg->Code)->Flags & CHECKED) checked = true;
	
	nsoption_set_bool(enable_javascript, checked);
	ami_menu_check_toggled = true;
}

static void ami_menu_item_browser_scale_decrease(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(browser_window_get_scale(gwin->bw) > 0.1)
		browser_window_set_scale(gwin->bw, browser_window_get_scale(gwin->bw) - 0.1, false);
}

static void ami_menu_item_browser_scale_normal(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_set_scale(gwin->bw, 1.0, false);
}

static void ami_menu_item_browser_scale_increase(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	browser_window_set_scale(gwin->bw, browser_window_get_scale(gwin->bw) + 0.1, false);
}

static void ami_menu_item_browser_redraw(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	ami_schedule_redraw(gwin, true);
	gwin->new_content = true;
}

static void ami_menu_item_hotlist_add(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct browser_window *bw;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	bw = gwin->bw;

	if (bw == NULL || bw->current_content == NULL ||
			nsurl_access(hlcache_handle_get_url(bw->current_content)) == NULL)
		return;

	hotlist_add_url(hlcache_handle_get_url(bw->current_content));
	ami_gui_update_hotlist_button(gwin);
}

static void ami_menu_item_hotlist_show(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
}

static void ami_menu_item_hotlist_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsurl *url = hook->h_Data;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(url == NULL) return;

	browser_window_navigate(gwin->bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
}

static void ami_menu_item_settings_edit(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	ami_gui_opts_open();
}

static void ami_menu_item_settings_snapshot(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	nsoption_set_int(window_x, gwin->win->LeftEdge);
	nsoption_set_int(window_y, gwin->win->TopEdge);
	nsoption_set_int(window_width, gwin->win->Width);
	nsoption_set_int(window_height, gwin->win->Height);
}

static void ami_menu_item_settings_save(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	nsoption_write(current_user_options, NULL, NULL);
}

static void ami_menu_item_arexx_execute(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(AslRequestTags(filereq,
						ASLFR_Window, gwin->win,
						ASLFR_SleepWindow, TRUE,
						ASLFR_TitleText, messages_get("NetSurf"),
						ASLFR_Screen, scrn,
						ASLFR_DoSaveMode, FALSE,
						ASLFR_InitialDrawer, nsoption_charp(arexx_dir),
						ASLFR_InitialPattern, "#?.nsrx",
						TAG_DONE))
	{
		if(temp = AllocVecTagList(1024, NULL))
		{
			strlcpy(temp, filereq->fr_Drawer, 1024);
			AddPart(temp, filereq->fr_File, 1024);
			ami_arexx_execute(temp);
			FreeVec(temp);
		}
	}
}

static void ami_menu_item_arexx_entries(struct Hook *hook, APTR window, struct IntuiMessage *msg)
{
	char *script = hook->h_Data;
	char *temp;
	struct gui_window_2 *gwin;
	GetAttr(WINDOW_UserData, (Object *)window, (ULONG *)&gwin);

	if(script)
	{
		if(temp = AllocVecTagList(1024, NULL))
		{
			BPTR lock;
			if(lock = Lock(nsoption_charp(arexx_dir), SHARED_LOCK)) {
				DevNameFromLock(lock, temp, 1024, DN_FULLPATH);
				AddPart(temp, script, 1024);
				ami_arexx_execute(temp);
				FreeVec(temp);
				UnLock(lock);
			}
		}
	}
}

