/*
 * Copyright 2008-2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/* NetSurf core includes */
#include "content/urldb.h"
#include "css/utils.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/hotlist.h"
#include "desktop/mouse.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "desktop/save_complete.h"
#include "desktop/scrollbar.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "image/ico.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "content/fetchers/resource.h"

/* NetSurf Amiga platform includes */
#include "amiga/arexx.h"
#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/context_menu.h"
#include "amiga/cookies.h"
#include "amiga/datatypes.h"
#include "amiga/download.h"
#include "amiga/drag.h"
#include "amiga/file.h"
#include "amiga/filetype.h"
#include "amiga/font.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "amiga/help.h"
#include "amiga/history.h"
#include "amiga/history_local.h"
#include "amiga/hotlist.h"
#include "amiga/icon.h"
#include "amiga/launch.h"
#include "amiga/login.h"
#include "amiga/menu.h"
#include "amiga/misc.h"
#include "amiga/plotters.h"
#include "amiga/plugin_hack.h"
#include "amiga/print.h"
#include "amiga/schedule.h"
#include "amiga/search.h"
#include "amiga/theme.h"
#include "amiga/tree.h"
#include "amiga/utf8.h"
#include "amiga/sslcert.h"

/* Custom StringView class */
#include "amiga/stringview/stringview.h"
#include "amiga/stringview/urlhistory.h"

/* AmigaOS libraries */
#ifdef __amigaos4__
#include <proto/application.h>
#endif
#include <proto/asl.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/locale.h>
#include <proto/Picasso96API.h>
#include <proto/popupmenu.h>
#include <proto/utility.h>
#include <proto/wb.h>

/* Other OS includes */
#include <datatypes/textclass.h>
#include <devices/inputevent.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <libraries/application.h>
#include <libraries/keymap.h>
#endif
#include <libraries/gadtools.h>
#include <intuition/icclass.h>
#include <graphics/rpattr.h>
#include <workbench/workbench.h>
#include <intuition/gui.h>
#include <intuition/screens.h>

/* ReAction libraries */
#include <proto/bevel.h>
#include <proto/bitmap.h>
#include <proto/button.h>
#include <proto/clicktab.h>
#include <proto/layout.h>
#include <proto/space.h>
#include <proto/speedbar.h>
#include <proto/string.h>
#include <proto/window.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/clicktab.h>
#include <gadgets/layout.h>
#include <gadgets/scroller.h>
#include <gadgets/space.h>
#include <gadgets/speedbar.h>
#include <gadgets/string.h>
#include <images/bevel.h>
#include <images/bitmap.h>

#include <reaction/reaction_macros.h>

/* newlib includes */
#include <math.h>
#include <string.h>

#define AMINS_SCROLLERPEN NUMDRIPENS

#define NSA_KBD_SCROLL_PX 10

/* Extra mouse button defines to match those in intuition/intuition.h */
#define SIDEDOWN  (IECODE_4TH_BUTTON)
#define SIDEUP    (IECODE_4TH_BUTTON | IECODE_UP_PREFIX)
#define EXTRADOWN (IECODE_5TH_BUTTON)
#define EXTRAUP   (IECODE_5TH_BUTTON | IECODE_UP_PREFIX)

extern struct gui_utf8_table *amiga_utf8_table;

struct ami_gui_tb_userdata {
	struct List *sblist;
	struct gui_window_2 *gw;
	int items;
};

struct MsgPort *appport;
struct Library  *KeymapBase = NULL;
struct KeymapIFace *IKeymap = NULL;
struct Library *ApplicationBase=NULL;
struct ApplicationIFace *IApplication=NULL;

Class *urlStringClass;

BOOL locked_screen = FALSE;
BOOL screen_closed = FALSE;
ULONG screen_signal = -1;

struct MsgPort *applibport = NULL;
ULONG applibsig = 0;
BOOL refresh_search_ico = FALSE;
BOOL refresh_favicon = FALSE;
BOOL refresh_throbber = FALSE;
struct Hook newprefs_hook;

STRPTR temp_homepage_url = NULL;
bool cli_force = false;

static char *current_user;
static char *current_user_dir;

static const __attribute__((used)) char *stack_cookie = "\0$STACK:131072\0";

const char * const versvn;
const char * const verdate;

void ami_update_buttons(struct gui_window_2 *);
void ami_scroller_hook(struct Hook *,Object *,struct IntuiMessage *);
void ami_switch_tab(struct gui_window_2 *gwin,bool redraw);
void ami_change_tab(struct gui_window_2 *gwin, int direction);
void ami_get_hscroll_pos(struct gui_window_2 *gwin, ULONG *xs);
void ami_get_vscroll_pos(struct gui_window_2 *gwin, ULONG *ys);
ULONG ami_set_border_gadget_balance(struct gui_window_2 *gwin);
ULONG ami_get_border_gadget_balance(struct gui_window_2 *gwin, ULONG *size1, ULONG *size2);
void ami_try_quit(void);
void ami_quit_netsurf_delayed(void);
Object *ami_gui_splash_open(void);
void ami_gui_splash_close(Object *win_obj);
static uint32 ami_set_search_ico_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg);
static uint32 ami_set_favicon_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg);
static uint32 ami_set_throbber_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg);
bool ami_gui_map_filename(char **remapped, const char *path, const char *file,
	const char *map);
static void ami_gui_window_update_box_deferred(struct gui_window *g, bool draw);
static void ami_do_redraw(struct gui_window_2 *g);
static void ami_schedule_redraw_remove(struct gui_window_2 *gwin);

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy);
static void gui_window_set_scroll(struct gui_window *g, int sx, int sy);
static void gui_window_remove_caret(struct gui_window *g);
static void gui_set_search_ico(hlcache_handle *ico);
static void gui_window_place_caret(struct gui_window *g, int x, int y, int height, const struct rect *clip);



/* accessors for default options - user option is updated if it is set as per default */
#define nsoption_default_set_int(OPTION, VALUE)				\
	if (nsoptions_default[NSOPTION_##OPTION].value.i == nsoptions[NSOPTION_##OPTION].value.i)	\
		nsoptions[NSOPTION_##OPTION].value.i = VALUE;	\
	nsoptions_default[NSOPTION_##OPTION].value.i = VALUE

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */

static char *filename_from_path(char *path)
{
	return strdup(FilePart(path));
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */

static bool path_add_part(char *path, int length, const char *newpart)
{
	if(AddPart(path, newpart, length)) return true;
		else return false;
}

STRPTR ami_locale_langs(void)
{
	struct Locale *locale;
	STRPTR acceptlangs = NULL;
	char *remapped;

	if(locale = OpenLocale(NULL))
	{
		for(int i = 0; i < 10; i++)
		{
			if(locale->loc_PrefLanguages[i])
			{
				if(ami_gui_map_filename(&remapped, "PROGDIR:Resources",
					locale->loc_PrefLanguages[i], "LangNames"))
				{
					if(acceptlangs)
					{
						STRPTR acceptlangs2 = acceptlangs;
						acceptlangs = ASPrintf("%s, %s",acceptlangs2, remapped);
						FreeVec(acceptlangs2);
						acceptlangs2 = NULL;
					}
					else
					{
						acceptlangs = ASPrintf("%s", remapped);
					}
				}
			}
			else
			{
				continue;
			}
		}
		CloseLocale(locale);
	}
	return acceptlangs;
}

bool ami_gui_map_filename(char **remapped, const char *path, const char *file, const char *map)
{
	BPTR fh = 0;
	char mapfile[1024];
	char buffer[1024];
	char *realfname;
	bool found = false;

	strcpy(mapfile, path);
	path_add_part(mapfile, 1024, map);

	if(fh = FOpen(mapfile, MODE_OLDFILE, 0))
	{
		while(FGets(fh, buffer, 1024) != 0)
		{
			if((buffer[0] == '#') ||
				(buffer[0] == '\n') ||
				(buffer[0] == '\0')) continue;

			if(realfname = strchr(buffer, ':'))
			{
				if(strncmp(buffer, file, strlen(file)) == 0)
				{
					if(realfname[strlen(realfname)-1] == '\n')
						realfname[strlen(realfname)-1] = '\0';
					*remapped = strdup(realfname + 1);
					found = true;
					break;
				}
			}
		}
		FClose(fh);
	}

	if(found == false) *remapped = strdup(file);
		else LOG(("Remapped %s to %s in path %s using %s", file, *remapped, path, map));

	return found;
}

bool ami_gui_check_resource(char *fullpath, const char *file)
{
	bool found = false;
	char *remapped;
	BPTR lock = 0;

	ami_gui_map_filename(&remapped, fullpath, file, "Resource.map");
	path_add_part(fullpath, 1024, remapped);

	LOG(("Checking for %s", fullpath));
	
	if(lock = Lock(fullpath, ACCESS_READ))
	{
		UnLock(lock);
		found = true;
	}

	if(found) LOG(("Found %s", fullpath));
	free(remapped);

	return found;
}

bool ami_locate_resource(char *fullpath, const char *file)
{
	struct Locale *locale;
	int i;
	BPTR lock = 0;
	bool found = false;
	char *remapped;

	/* Check NetSurf user data area first */

	strcpy(fullpath, current_user_dir);
	found = ami_gui_check_resource(fullpath, file);
	if(found) return true;

	/* Check current theme directory */
	if(nsoption_charp(theme))
	{
		strcpy(fullpath, nsoption_charp(theme));
		found = ami_gui_check_resource(fullpath, file);
		if(found) return true;
	}

	/* If not found, start on the user's preferred languages */

	locale = OpenLocale(NULL);

	for(i=0;i<10;i++)
	{
		strcpy(fullpath,"PROGDIR:Resources/");

		if(locale->loc_PrefLanguages[i])
		{
			ami_gui_map_filename(&remapped, "PROGDIR:Resources",
				locale->loc_PrefLanguages[i], "LangNames");
			path_add_part(fullpath, 1024, remapped);

			found = ami_gui_check_resource(fullpath, file);
		}
		else
		{
			continue;
		}

		if(found) break;
	}

	if(!found)
	{
		/* If not found yet, check in PROGDIR:Resources/en,
		 * might not be in user's preferred languages */

		strcpy(fullpath, "PROGDIR:Resources/en/");
		found = ami_gui_check_resource(fullpath, file);
	}

	CloseLocale(locale);

	if(!found)
	{
		/* Lastly check directly in PROGDIR:Resources */

		strcpy(fullpath, "PROGDIR:Resources/");
		found = ami_gui_check_resource(fullpath, file);
	}

	return found;
}

void ami_open_resources(void)
{
	/* Allocate ports/ASL and open libraries and devices */

	if(KeymapBase = OpenLibrary("keymap.library",37))
	{
		IKeymap = (struct KeymapIFace *)GetInterface(KeymapBase,"main",1,NULL);
	}

	if(ApplicationBase = OpenLibrary("application.library", 53)) {
		IApplication = (struct ApplicationIFace *)GetInterface(ApplicationBase, "application", 2, NULL);
	}

	urlStringClass = MakeStringClass();

    if(!(appport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack,FALSE,
							TAG_DONE))) die(messages_get("NoMemory"));

    if(!(sport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack,FALSE,
							TAG_DONE))) die(messages_get("NoMemory"));

	ami_file_req_init();
}

static UWORD ami_system_colour_scrollbar_fgpen(struct DrawInfo *drinfo)
{
	LONG scrollerfillpen = FALSE;

	GetGUIAttrs(NULL, drinfo, GUIA_PropKnobColor, &scrollerfillpen, TAG_DONE);

	if(scrollerfillpen) return FILLPEN;
		else return FOREGROUNDPEN;
}

/**
 * set option from pen
 */
static nserror
colour_option_from_pen(UWORD pen,
			   enum nsoption_e option,
			   struct Screen *screen,
			   colour def_colour)
{
	ULONG colr[3];
	struct DrawInfo *drinfo;

	if((option < NSOPTION_SYS_COLOUR_START) ||
	   (option > NSOPTION_SYS_COLOUR_END) ||
	   (nsoptions[option].type != OPTION_COLOUR)) {
		return NSERROR_BAD_PARAMETER;
	}

	if(screen != NULL) {
		drinfo = GetScreenDrawInfo(screen);
		if(drinfo != NULL) {

			if(pen == AMINS_SCROLLERPEN) pen = ami_system_colour_scrollbar_fgpen(drinfo);

			/* Get the colour of the pen being used for "pen" */
			GetRGB32(screen->ViewPort.ColorMap, drinfo->dri_Pens[pen], 1, (ULONG *)&colr);

			/* convert it to a color */
			def_colour = ((colr[0] & 0xff000000) >> 24) |
				((colr[1] & 0xff000000) >> 16) |
				((colr[2] & 0xff000000) >> 8);

			FreeScreenDrawInfo(screen, drinfo);
		}
	}

	if (nsoptions_default[option].value.c == nsoptions[option].value.c)
		nsoptions[option].value.c = def_colour;
	nsoptions_default[option].value.c = def_colour;

	return NSERROR_OK;
}

static void ami_set_screen_defaults(struct Screen *screen)
{
	nsoption_default_set_int(window_x, 0);
	nsoption_default_set_int(window_y, screen->BarHeight + 1);
	nsoption_default_set_int(window_width, screen->Width);
	nsoption_default_set_int(window_height, screen->Height - screen->BarHeight - 1);

	nsoption_default_set_int(redraw_tile_size_x, screen->Width);
	nsoption_default_set_int(redraw_tile_size_y, screen->Height);

	/* set system colours for amiga ui */
	colour_option_from_pen(FILLPEN, NSOPTION_sys_colour_ActiveBorder, screen, 0x00000000);
	colour_option_from_pen(FILLPEN, NSOPTION_sys_colour_ActiveCaption, screen, 0x00dddddd);
	colour_option_from_pen(BACKGROUNDPEN, NSOPTION_sys_colour_AppWorkspace, screen, 0x00eeeeee);
	colour_option_from_pen(BACKGROUNDPEN, NSOPTION_sys_colour_Background, screen, 0x00aa0000);
	colour_option_from_pen(FOREGROUNDPEN, NSOPTION_sys_colour_ButtonFace, screen, 0x00aaaaaa);
	colour_option_from_pen(FORESHINEPEN, NSOPTION_sys_colour_ButtonHighlight, screen, 0x00cccccc);
	colour_option_from_pen(FORESHADOWPEN, NSOPTION_sys_colour_ButtonShadow, screen, 0x00bbbbbb);
	colour_option_from_pen(TEXTPEN, NSOPTION_sys_colour_ButtonText, screen, 0x00000000);
	colour_option_from_pen(FILLTEXTPEN, NSOPTION_sys_colour_CaptionText, screen, 0x00000000);
	colour_option_from_pen(DISABLEDTEXTPEN, NSOPTION_sys_colour_GrayText, screen, 0x00777777);
	colour_option_from_pen(SELECTPEN, NSOPTION_sys_colour_Highlight, screen, 0x00ee0000);
	colour_option_from_pen(SELECTTEXTPEN, NSOPTION_sys_colour_HighlightText, screen, 0x00000000);
	colour_option_from_pen(INACTIVEFILLPEN, NSOPTION_sys_colour_InactiveBorder, screen, 0x00000000);
	colour_option_from_pen(INACTIVEFILLPEN, NSOPTION_sys_colour_InactiveCaption, screen, 0x00ffffff);
	colour_option_from_pen(INACTIVEFILLTEXTPEN, NSOPTION_sys_colour_InactiveCaptionText, screen, 0x00cccccc);
	colour_option_from_pen(BACKGROUNDPEN, NSOPTION_sys_colour_InfoBackground, screen, 0x00aaaaaa);/* This is wrong, HelpHint backgrounds are pale yellow but doesn't seem to be a DrawInfo pen defined for it. */
	colour_option_from_pen(TEXTPEN, NSOPTION_sys_colour_InfoText, screen, 0x00000000);
	colour_option_from_pen(MENUBACKGROUNDPEN, NSOPTION_sys_colour_Menu, screen, 0x00aaaaaa);
	colour_option_from_pen(MENUTEXTPEN, NSOPTION_sys_colour_MenuText, screen, 0x00000000);
	colour_option_from_pen(AMINS_SCROLLERPEN, NSOPTION_sys_colour_Scrollbar, screen, 0x00aaaaaa);
	colour_option_from_pen(FORESHADOWPEN, NSOPTION_sys_colour_ThreeDDarkShadow, screen, 0x00555555);
	colour_option_from_pen(FOREGROUNDPEN, NSOPTION_sys_colour_ThreeDFace, screen, 0x00dddddd);
	colour_option_from_pen(FORESHINEPEN, NSOPTION_sys_colour_ThreeDHighlight, screen, 0x00aaaaaa);
	colour_option_from_pen(HALFSHINEPEN, NSOPTION_sys_colour_ThreeDLightShadow, screen, 0x00999999);
	colour_option_from_pen(HALFSHADOWPEN, NSOPTION_sys_colour_ThreeDShadow, screen, 0x00777777);
	colour_option_from_pen(BACKGROUNDPEN, NSOPTION_sys_colour_Window, screen, 0x00aaaaaa);
	colour_option_from_pen(INACTIVEFILLPEN, NSOPTION_sys_colour_WindowFrame, screen, 0x00000000);
	colour_option_from_pen(TEXTPEN, NSOPTION_sys_colour_WindowText, screen, 0x00000000);
}


/**
 * Set option defaults for amiga frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror ami_set_options(struct nsoption_s *defaults)
{
	STRPTR tempacceptlangs;
	char temp[1024];

	/* The following line disables the popupmenu.class select menu
	** This will become a user option when/if popupmenu.class is
	** updated to show more items than can fit in one column vertically
	*/

	nsoption_set_bool(core_select_menu, true);

	#ifndef NS_AMIGA_CAIRO
	/* Ensure we get some output when Cairo not available */
	nsoption_set_int(cairo_renderer, 0);
	#endif

	if((!nsoption_charp(accept_language)) || 
	   (nsoption_charp(accept_language)[0] == '\0') ||
	   (nsoption_bool(accept_lang_locale) == true))
	{
		if(tempacceptlangs = ami_locale_langs())
		{
			nsoption_set_charp(accept_language,
					   (char *)strdup(tempacceptlangs));
			FreeVec(tempacceptlangs);
		}
	}

	sprintf(temp, "%s/Cookies", current_user_dir);
	nsoption_setnull_charp(cookie_file, 
			       (char *)strdup(temp));

	sprintf(temp, "%s/Hotlist", current_user_dir);
	nsoption_setnull_charp(hotlist_file, 
			       (char *)strdup(temp));

	sprintf(temp, "%s/URLdb", current_user_dir);
	nsoption_setnull_charp(url_file,
			       (char *)strdup(temp));

	nsoption_setnull_charp(ca_bundle,
			       (char *)strdup("PROGDIR:Resources/ca-bundle"));

	
	search_engines_file_location = nsoption_charp(search_engines_file);

	sprintf(temp, "%s/FontGlyphCache", current_user_dir);
	nsoption_setnull_charp(font_unicode_file,
			       (char *)strdup(temp));

	/* font defaults */
	nsoption_setnull_charp(font_sans, (char *)strdup("DejaVu Sans"));
	nsoption_setnull_charp(font_serif, (char *)strdup("DejaVu Serif"));
	nsoption_setnull_charp(font_mono, (char *)strdup("DejaVu Sans Mono"));
	nsoption_setnull_charp(font_cursive, (char *)strdup("DejaVu Sans"));
	nsoption_setnull_charp(font_fantasy, (char *)strdup("DejaVu Serif"));

	if (nsoption_charp(font_unicode) == NULL)
	{
		BPTR lock = 0;
		/* Search for some likely candidates */

		if(lock = Lock("FONTS:Code2000.font", ACCESS_READ))
		{
			UnLock(lock);
			nsoption_set_charp(font_unicode, 
					   (char *)strdup("Code2000"));
		}
		else if(lock = Lock("FONTS:Bitstream Cyberbit.font", ACCESS_READ))
		{
			UnLock(lock);
			nsoption_set_charp(font_unicode,
					   (char *)strdup("Bitstream Cyberbit"));
		}
	}

	if(popupmenu_lib_ok == FALSE)
		nsoption_set_bool(context_menu, false);

#ifndef __amigaos4__
	nsoption_set_bool(download_notify, false);
	nsoption_set_bool(font_antialiasing, false);
	nsoption_set_bool(truecolour_mouse_pointers, false);
#endif

	return NSERROR_OK;
}

void ami_amiupdate(void)
{
	/* Create AppPath location for AmiUpdate use */

	BPTR lock = 0;

	if(((lock = Lock("ENVARC:AppPaths",SHARED_LOCK)) == 0))
	{
		lock = CreateDir("ENVARC:AppPaths");
	}
	
	UnLock(lock);

	if(lock = Lock("PROGDIR:", ACCESS_READ))
	{
		char filename[1024];
		BPTR amiupdatefh;

		DevNameFromLock(lock,(STRPTR)&filename,1024L,DN_FULLPATH);

		amiupdatefh = FOpen("ENVARC:AppPaths/NetSurf",MODE_NEWFILE,0);
		FPuts(amiupdatefh,(CONST_STRPTR)&filename);
		FClose(amiupdatefh);
		UnLock(lock);
	}
}

static nsurl *gui_get_resource_url(const char *path)
{
	char buf[1024];
	char path2[1024];
	char *raw;
	nsurl *url = NULL;

	if(ami_locate_resource(buf, path) == false)
	{
		if((strncmp(path + strlen(path) - SLEN(".htm"), ".htm", SLEN(".htm")) == 0) ||
			(strncmp(path + strlen(path) - SLEN(".html"), ".html", SLEN(".html")) == 0))
		{
			/* Try with RISC OS HTML filetype, might work */
			strcpy(path2, path);
			strcat(path2, ",faf");

			if(ami_locate_resource(buf, path2) == false)
			{
				return NULL;
			}
		}
		else return NULL;
	}

	raw = path_to_url(buf);
	if (raw != NULL) {
		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
}

void gui_init(int argc, char** argv)
{
	BPTR lock = 0;

	ami_open_resources(); /* alloc ports/asl reqs, open libraries/devices */
	ami_clipboard_init();
	ami_openurl_open();

	win_destroyed = false;
	nsscreentitle = ASPrintf("NetSurf %s",netsurf_version);

	ami_font_setdevicedpi(0); /* for early font requests, eg treeview init */

	ami_amiupdate(); /* set env-vars for AmiUpdate */
	ami_init_fonts();

	ami_context_menu_init();

	window_list = NewObjList();

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	save_complete_init();
	ami_theme_init();
	ami_init_mouse_pointers();
}

static void ami_gui_newprefs_hook(struct Hook *hook, APTR window, APTR reserved)
{
	ami_set_screen_defaults(scrn);
}

void ami_openscreen(void)
{
	ULONG id = 0;
	ULONG compositing;

	if (nsoption_int(screen_compositing) == -1)
		compositing = ~0UL;
	else compositing = nsoption_int(screen_compositing);

	if (nsoption_charp(pubscreen_name) == NULL)
	{
		if((nsoption_charp(screen_modeid)) && 
		   (strncmp(nsoption_charp(screen_modeid), "0x", 2) == 0))
		{
			id = strtoul(nsoption_charp(screen_modeid), NULL, 0);
		}
		else
		{
			struct ScreenModeRequester *screenmodereq = NULL;

			if(screenmodereq = AllocAslRequest(ASL_ScreenModeRequest,NULL))
			{
				if(AslRequestTags(screenmodereq,
						ASLSM_MinDepth, 0,
						ASLSM_MaxDepth, 32,
						TAG_DONE))
				{
					char *modeid = malloc(20);
					id = screenmodereq->sm_DisplayID;
					sprintf(modeid, "0x%lx", id);
					nsoption_set_charp(screen_modeid, modeid);
					nsoption_write(current_user_options, NULL, NULL);
				}
				FreeAslRequest(screenmodereq);
			}
		}

		if(screen_signal == -1) screen_signal = AllocSignal(-1);
		LOG(("Screen signal %d", screen_signal));
		scrn = OpenScreenTags(NULL,
					SA_DisplayID, id,
					SA_Title, nsscreentitle,
					SA_Type, PUBLICSCREEN,
					SA_PubName, "NetSurf",
					SA_PubSig, screen_signal,
					SA_PubTask, FindTask(0),
					SA_LikeWorkbench, TRUE,
					SA_Compositing, compositing,
					TAG_DONE);

		if(scrn)
		{
			PubScreenStatus(scrn,0);
		}
		else
		{
			FreeSignal(screen_signal);
			screen_signal = -1;

			if(scrn = LockPubScreen("NetSurf"))
			{
				locked_screen = TRUE;
			}
			else
			{
				nsoption_set_charp(pubscreen_name,
						   strdup("Workbench"));
			}
		}
	}

	if (nsoption_charp(pubscreen_name) != NULL)
	{
		scrn = LockPubScreen(nsoption_charp(pubscreen_name));

		if(scrn == NULL)
		{
			scrn = LockPubScreen("Workbench");
		}
		locked_screen = TRUE;
	}

	ami_font_setdevicedpi(id);
	ami_set_screen_defaults(scrn);
	ami_help_new_screen(scrn);
}

void ami_openscreenfirst(void)
{
	ami_openscreen();
	if(!browserglob.bm) ami_init_layers(&browserglob, 0, 0);
	ami_theme_throbber_setup();
}

static void ami_gui_commandline(int *argc, char **argv)
{
	int new_argc = 0;
	struct RDArgs *args;
	STRPTR template = "NSOPTS/M,URL/K,FORCE/S";
	long rarray[] = {0,0,0};
	enum
	{
		A_NSOPTS, /* ignored */
		A_URL,
		A_FORCE
	};

	if(*argc == 0) return; // argc==0 is started from wb

	if(args = ReadArgs(template, rarray, NULL)) {
		if(rarray[A_URL]) {
			LOG(("URL %s specified on command line", rarray[A_URL]));
			temp_homepage_url = ami_to_utf8_easy((char *)rarray[A_URL]);
		}

		if(rarray[A_FORCE]) {
			LOG(("FORCE specified on command line"));
			cli_force = true;
		}

		if(rarray[A_NSOPTS]) {
		/* The NSOPTS/M parameter specified in the ReadArgs template is
		 * special. The /M means it collects all arguments that can't
		 * be assigned to any other parameter, and stores them in an
		 * array.  We collect these and pass them as a fake argc/argv
		 * to nsoption_commandline().
		 * This trickery is necessary because if ReadArgs() is called
		 * first, nsoption_commandline() can no longer parse (fetch?)
		 * the arguments.  If nsoption_commandline() is called first,
		 * then ReadArgs cannot fetch the arguments.
		 */
			char **p = (char **)rarray[A_NSOPTS];
			do {
				LOG(("Arg [%d] assigned to NSOPTS/M by ReadArgs: %s", new_argc, *p));
				new_argc++;
				p++;
			} while(*p != NULL);

			nsoption_commandline(&new_argc, (char **)rarray[A_NSOPTS], NULL);
		}

		FreeArgs(args);
	} else {
		LOG(("ReadArgs failed to parse command line"));
	}
}


static void gui_init2(int argc, char** argv)
{
	struct Screen *screen;
	BOOL notalreadyrunning;
	nsurl *url;
	nserror error;
	struct browser_window *bw = NULL;

	notalreadyrunning = ami_arexx_init();

	/* Treeview init code ends up calling a font function which needs this */
	glob = &browserglob;

	/* ...and this ensures the treeview at least gets the WB colour palette to work with */
	if(scrn == NULL) {
		if(screen = LockPubScreen("Workbench")) {
			ami_set_screen_defaults(screen);
			UnlockPubScreen(NULL, screen);
		}
	} else {
		ami_set_screen_defaults(scrn);
	}
	/**/

	ami_hotlist_initialise(nsoption_charp(hotlist_file));
	ami_cookies_initialise();
	ami_global_history_initialise();

	search_web_provider_details(nsoption_int(search_provider));

	if (notalreadyrunning && 
	    (nsoption_bool(startup_no_window) == false))
		ami_openscreenfirst();

	if(temp_homepage_url && notalreadyrunning) {
		error = nsurl_create(temp_homepage_url, &url);
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
					url,
					NULL,
					NULL,
					&bw);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
		free(temp_homepage_url);
	}

	if(cli_force == true) {
		notalreadyrunning = TRUE;
	}

	if(argc == 0) { // WB
		struct WBStartup *WBenchMsg = (struct WBStartup *)argv;
		struct WBArg *wbarg;
		int first=0,i=0;
		char fullpath[1024];

		for(i=0,wbarg=WBenchMsg->sm_ArgList;i<WBenchMsg->sm_NumArgs;i++,wbarg++)
		{
			if(i==0) continue;
			if((wbarg->wa_Lock)&&(*wbarg->wa_Name))
			{
				DevNameFromLock(wbarg->wa_Lock,fullpath,1024,DN_FULLPATH);
				AddPart(fullpath,wbarg->wa_Name,1024);

				if(!temp_homepage_url) temp_homepage_url = path_to_url(fullpath);

				if(notalreadyrunning)
				{
					error = nsurl_create(temp_homepage_url, &url);

					if (error == NSERROR_OK) {
						if(!first)
						{
							error = browser_window_create(BW_CREATE_HISTORY,
										      url,
										      NULL,
										      NULL,
										      &bw);

							first=1;
						}
						else
						{
							error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
										      url,
										      NULL,
										      bw,
										      &bw);

						}
						nsurl_unref(url);

					}
					if (error != NSERROR_OK) {
						warn_user(messages_get_errorcode(error), 0);
					}
					free(temp_homepage_url);
					temp_homepage_url = NULL;
				}
			}
		}
	}

	nsoption_setnull_charp(homepage_url, (char *)strdup(NETSURF_HOMEPAGE));

	if(!notalreadyrunning)
	{
		STRPTR sendcmd = NULL;

		if(temp_homepage_url)
		{
			sendcmd = ASPrintf("OPEN \"%s\" NEW",temp_homepage_url);
			free(temp_homepage_url);
		}
		else
		{
			sendcmd = ASPrintf("OPEN \"%s\" NEW",nsoption_charp(homepage_url));
		}
		IDoMethod(arexx_obj,AM_EXECUTE,sendcmd,"NETSURF",NULL,NULL,NULL,NULL);
		FreeVec(sendcmd);

		netsurf_quit=true;
		return;
	}

	if(IApplication)
	{
		if(argc == 0)
		{
			ULONG noicon = TAG_IGNORE;

			if (nsoption_bool(hide_docky_icon)) 
				noicon = REGAPP_NoIcon;

			ami_appid = RegisterApplication(messages_get("NetSurf"),
				REGAPP_URLIdentifier, "netsurf-browser.org",
				REGAPP_WBStartup, (struct WBStartup *)argv,
				noicon, TRUE,
				REGAPP_HasPrefsWindow, TRUE,
				REGAPP_CanCreateNewDocs, TRUE,
				REGAPP_UniqueApplication, TRUE,
				REGAPP_Description, messages_get("NetSurfDesc"),
				TAG_DONE);
		}
		else
		{
/* TODO: Specify icon when run from Shell */
			ami_appid = RegisterApplication(messages_get("NetSurf"),
				REGAPP_URLIdentifier, "netsurf-browser.org",
				REGAPP_FileName, argv[0],
				REGAPP_NoIcon, TRUE,
				REGAPP_HasPrefsWindow, TRUE,
				REGAPP_CanCreateNewDocs, TRUE,
				REGAPP_UniqueApplication, TRUE,
				REGAPP_Description, messages_get("NetSurfDesc"),
				TAG_DONE);
		}

		GetApplicationAttrs(ami_appid, APPATTR_Port, (ULONG)&applibport, TAG_DONE);
		if(applibport) applibsig = (1L << applibport->mp_SigBit);
	}

	if(!bw && (nsoption_bool(startup_no_window) == false)) {
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
}


void ami_gui_history(struct gui_window_2 *gwin, bool back)
{
	if(back == true)
	{
		if(browser_window_back_available(gwin->bw))
			browser_window_history_back(gwin->bw, false);
	}
	else
	{
		if(browser_window_forward_available(gwin->bw))
			browser_window_history_forward(gwin->bw, false);
	}

	ami_update_buttons(gwin);
}

int ami_key_to_nskey(ULONG keycode, struct InputEvent *ie)
{
	int nskey = 0, chars;
	UBYTE buffer[20];
	char *utf8 = NULL;

	if(keycode >= IECODE_UP_PREFIX) return 0;

	switch(keycode)
	{
		case RAWKEY_CRSRUP:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_PAGE_UP;
			}
			else if(ie->ie_Qualifier & IEQUALIFIER_RALT)
			{
				nskey = KEY_TEXT_START;
			}
			else nskey = KEY_UP;
		break;
		case RAWKEY_CRSRDOWN:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_PAGE_DOWN;
			}
			else if(ie->ie_Qualifier & IEQUALIFIER_RALT)
			{
				nskey = KEY_TEXT_END;
			}
			else nskey = KEY_DOWN;
		break;
		case RAWKEY_CRSRLEFT:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_LINE_START;
			}
			else if(ie->ie_Qualifier & IEQUALIFIER_RALT)
			{
				nskey = KEY_WORD_LEFT;
			}
			else nskey = KEY_LEFT;
		break;
		case RAWKEY_CRSRRIGHT:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_LINE_END;
			}
			else if(ie->ie_Qualifier & IEQUALIFIER_RALT)
			{
				nskey = KEY_WORD_RIGHT;
			}
			else nskey = KEY_RIGHT;
		break;
		case RAWKEY_ESC:
			nskey = KEY_ESCAPE;
		break;
		case RAWKEY_PAGEUP:
			nskey = KEY_PAGE_UP;
		break;
		case RAWKEY_PAGEDOWN:
			nskey = KEY_PAGE_DOWN;
		break;
		case RAWKEY_HOME:
			nskey = KEY_TEXT_START;
		break;
		case RAWKEY_END:
			nskey = KEY_TEXT_END;
		break;
		case RAWKEY_BACKSPACE:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_DELETE_LINE_START;
			}
			else nskey = KEY_DELETE_LEFT;
		break;
		case RAWKEY_DEL:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_DELETE_LINE_END;
			}
			else nskey = KEY_DELETE_RIGHT;
		break;
		case RAWKEY_TAB:
			if(ie->ie_Qualifier & IEQUALIFIER_RSHIFT)
			{
				nskey = KEY_SHIFT_TAB;
			}
			else nskey = KEY_TAB;
		break;
		case RAWKEY_F5:
		case RAWKEY_HELP:
			// don't translate
			nskey = keycode;
		break;
		default:
			if((chars = MapRawKey(ie,buffer,20,NULL)) > 0)
			{
				utf8_from_local_encoding(buffer, chars, &utf8);
				nskey = utf8_to_ucs4(utf8, utf8_char_byte_length(utf8));
			}
		break;
	}
	return nskey;
}

void ami_update_quals(struct gui_window_2 *gwin)
{
	uint32 quals = 0;

	GetAttr(WINDOW_Qualifier,gwin->objects[OID_MAIN],(uint32 *)&quals);

	gwin->key_state = 0;

	if((quals & IEQUALIFIER_LSHIFT) || (quals & IEQUALIFIER_RSHIFT)) 
	{
		gwin->key_state |= BROWSER_MOUSE_MOD_1;
	}

	if(quals & IEQUALIFIER_CONTROL) 
	{
		gwin->key_state |= BROWSER_MOUSE_MOD_2;
	}

	if((quals & IEQUALIFIER_LALT) || (quals & IEQUALIFIER_RALT)) 
	{
		gwin->key_state |= BROWSER_MOUSE_MOD_3;
	}
}

bool ami_spacebox_to_ns_coords(struct gui_window_2 *gwin, int *x, int *y,
	int space_x, int space_y)
{
	int ns_x = space_x;
	int ns_y = space_y;

	ns_x /= gwin->bw->scale;
	ns_y /= gwin->bw->scale;

	ns_x += gwin->bw->window->scrollx;
	ns_y += gwin->bw->window->scrolly;

	*x = ns_x;
	*y = ns_y;

	return true;	
}

bool ami_mouse_to_ns_coords(struct gui_window_2 *gwin, int *x, int *y,
	int mouse_x, int mouse_y)
{
	int ns_x, ns_y;
	struct IBox *bbox;

	if(mouse_x == -1) mouse_x = gwin->win->MouseX;
	if(mouse_y == -1) mouse_y = gwin->win->MouseY;

	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER],
			(ULONG *)&bbox);

	ns_x = (ULONG)(mouse_x - bbox->Left);
	ns_y = (ULONG)(mouse_y - bbox->Top);

	if((ns_x < 0) || (ns_x > bbox->Width) || (ns_y < 0) || (ns_y > bbox->Height))
		return false;

	return ami_spacebox_to_ns_coords(gwin, x, y, ns_x, ns_y);
}

void ami_gui_scroll_internal(struct gui_window_2 *gwin, int xs, int ys)
{
	struct IBox *bbox;
	int x, y;

	GetAttr(SPACE_AreaBox,
		(Object *)gwin->objects[GID_BROWSER],
		(ULONG *)&bbox);

	if(ami_mouse_to_ns_coords(gwin, &x, &y, -1, -1) == true)
	{
		if(browser_window_scroll_at_point(gwin->bw, x, y,
			xs, ys) == false)
		{
			gui_window_get_scroll(gwin->bw->window,
				&gwin->bw->window->scrollx,
				&gwin->bw->window->scrolly);

			switch(xs)
			{
				case SCROLL_PAGE_UP:
					xs = gwin->bw->window->scrollx - bbox->Width;
				break;

				case SCROLL_PAGE_DOWN:
					xs = gwin->bw->window->scrollx + bbox->Width;
				break;

				case SCROLL_TOP:
					xs = 0;
				break;

				case SCROLL_BOTTOM:
					xs = content_get_width(gwin->bw->current_content);
				break;

				default:
					xs += gwin->bw->window->scrollx;
				break;
			}

			switch(ys)
			{
				case SCROLL_PAGE_UP:
					ys = gwin->bw->window->scrolly - bbox->Height;
				break;

				case SCROLL_PAGE_DOWN:
					ys = gwin->bw->window->scrolly + bbox->Height;
				break;

				case SCROLL_TOP:
					ys = 0;
				break;

				case SCROLL_BOTTOM:
					ys = content_get_height(gwin->bw->current_content);
				break;

				default:
					ys += gwin->bw->window->scrolly;
				break;
			}

			gui_window_set_scroll(gwin->bw->window, xs, ys);
		}
	}
}

struct IBox *ami_ns_rect_to_ibox(struct gui_window_2 *gwin, const struct rect *rect)
{
	struct IBox *bbox, *ibox;

	ibox = AllocVecTagList(sizeof(struct IBox), NULL);
	if(ibox == NULL) return NULL;

	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER], (ULONG *)&bbox);

	ibox->Left = gwin->win->MouseX + (rect->x0 * gwin->bw->scale);
	ibox->Top = gwin->win->MouseY + (rect->y0 * gwin->bw->scale);

	ibox->Width = (rect->x1 - rect->x0) * gwin->bw->scale;
	ibox->Height = (rect->y1 - rect->y0) * gwin->bw->scale;

	if(ibox->Left < bbox->Left) ibox->Left = bbox->Left;
	if(ibox->Top < bbox->Top) ibox->Top = bbox->Top;

	if((ibox->Left > (bbox->Left + bbox->Width)) ||
		(ibox->Top > (bbox->Top + bbox->Height)) ||
		(ibox->Width < 0) || (ibox->Height < 0))
	{
		FreeVec(ibox);
		return NULL;
	}

	return ibox;	
}

void ami_gui_trap_mouse(struct gui_window_2 *gwin)
{
	switch(gwin->drag_op)
	{
		case GDRAGGING_NONE:
		case GDRAGGING_SCROLLBAR:
		case GDRAGGING_OTHER:
		break;

		default:
			if(gwin->ptr_lock)
			{
				SetWindowAttrs(gwin->win, WA_GrabFocus, 10,
					WA_MouseLimits, gwin->ptr_lock, TAG_DONE);
			}
		break;
	}
}

void ami_gui_menu_update_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			ami_menu_update_checked(gwin);
		}
	} while(node = nnode);
}

/**
 * function to add retrieved favicon to gui
 */
static void gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
	struct BitMap *bm = NULL;
	struct IBox *bbox;
	ULONG cur_tab = 0;
	struct bitmap *icon_bitmap;

	if(nsoption_bool(kiosk_mode) == true) return;
	if(!g) return;

	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
						g->shared->objects[GID_TABS],
						(ULONG *)&cur_tab);

	if ((icon != NULL) && ((icon_bitmap = content_get_bitmap(icon)) != NULL))
	{
		bm = ami_bitmap_get_native(icon_bitmap, 16, 16,
					g->shared->win->RPort->BitMap);
	}

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_ICON], (ULONG *)&bbox);

		RefreshGList((struct Gadget *)g->shared->objects[GID_ICON],
					g->shared->win, NULL, 1);

		if(bm)
		{
			ULONG tag, tag_data, minterm;

			if(ami_plot_screen_is_palettemapped() == false) {
				tag = BLITA_UseSrcAlpha;
				tag_data = !icon_bitmap->opaque;
				minterm = 0xc0;
			} else {
				tag = BLITA_MaskPlane;
				tag_data = (ULONG)ami_bitmap_get_mask(icon_bitmap, 16, 16, bm);
				minterm = (ABC|ABNC|ANBC);
			}

			BltBitMapTags(BLITA_SrcX, 0,
						BLITA_SrcY, 0,
						BLITA_DestX, bbox->Left,
						BLITA_DestY, bbox->Top,
						BLITA_Width, 16,
						BLITA_Height, 16,
						BLITA_Source, bm,
						BLITA_Dest, g->shared->win->RPort,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_RASTPORT,
						BLITA_Minterm, minterm,
						tag, tag_data,
						TAG_DONE);
		}
	}

	g->favicon = icon;
}

void ami_handle_msg(void)
{
	ULONG class,result,storage = 0,x,y,xs,ys,width=800,height=600;
	uint16 code,quals;
	struct IBox *bbox;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin = NULL;
	struct InputEvent *ie;
	struct Node *tabnode;
	int nskey;
	struct browser_window *closedbw;
	struct timeval curtime;
	static int drag_x_move = 0, drag_y_move = 0;
	char *utf8 = NULL;
	nsurl *url;

	if(IsMinListEmpty(window_list))
	{
		/* no windows in list, so NetSurf should not be running */
		ami_try_quit();
		return;
	}

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do
	{
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		gwin = node->objstruct;

		if(node->Type == AMINS_TVWINDOW) {
			if(ami_tree_event((struct treeview_window *)gwin)) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_FINDWINDOW) {
			if(ami_search_event()) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_HISTORYWINDOW) {
			if(ami_history_event((struct history_window *)gwin)) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_PRINTWINDOW) {
			if(ami_print_event((struct ami_print_window *)gwin)) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_GUIOPTSWINDOW) {
			if(ami_gui_opts_event()) {
				/* last window possibly closed, so exit with conditions ;) */
				if(scrn) ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_DLWINDOW) {
			if(ami_download_window_event((struct gui_download_window *)gwin)) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		} else if(node->Type == AMINS_LOGINWINDOW) {
			if(ami_401login_event((struct gui_login_window *)gwin)) {
				ami_try_quit();
				break;
			} else {
				node = nnode;
				continue;
			}
		}

		while((result = RA_HandleInput(gwin->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
		{
//printf("%ld: %ld (switch)\n",code, result & WMHI_CLASSMASK);
	        switch(result & WMHI_CLASSMASK) // class
   		   	{
				case WMHI_MOUSEMOVE:
					ami_gui_trap_mouse(gwin); /* re-assert mouse area */

					drag_x_move = 0;
					drag_y_move = 0;

					GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER],
							(ULONG *)&bbox);

					x = (ULONG)((gwin->win->MouseX - bbox->Left) / gwin->bw->scale);
					y = (ULONG)((gwin->win->MouseY - bbox->Top) / gwin->bw->scale);

					ami_get_hscroll_pos(gwin, (ULONG *)&xs);
					ami_get_vscroll_pos(gwin, (ULONG *)&ys);

					x += xs;
					y += ys;

					width=bbox->Width;
					height=bbox->Height;

					if(gwin->mouse_state & BROWSER_MOUSE_DRAG_ON)
					{
						ami_drag_icon_move();

						if(ami_autoscroll == TRUE) {
							if((gwin->win->MouseX < bbox->Left) &&
								((gwin->win->MouseX - bbox->Left) > -AMI_DRAG_THRESHOLD))
								drag_x_move = gwin->win->MouseX - bbox->Left;
							if((gwin->win->MouseX > (bbox->Left + bbox->Width)) &&
								((gwin->win->MouseX - (bbox->Left + bbox->Width)) < AMI_DRAG_THRESHOLD))
								drag_x_move = gwin->win->MouseX - (bbox->Left + bbox->Width);
							if((gwin->win->MouseY < bbox->Top) &&
								((gwin->win->MouseY - bbox->Top) > -AMI_DRAG_THRESHOLD))
								drag_y_move = gwin->win->MouseY - bbox->Top;
							if((gwin->win->MouseY > (bbox->Top + bbox->Height)) &&
								((gwin->win->MouseY - (bbox->Top + bbox->Height)) < AMI_DRAG_THRESHOLD))
								drag_y_move = gwin->win->MouseY - (bbox->Top + bbox->Height);
						}
					}

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						ami_update_quals(gwin);
						ami_context_menu_mouse_trap(gwin, TRUE);

						if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
						{
							browser_window_mouse_track(gwin->bw,BROWSER_MOUSE_DRAG_1 | gwin->key_state,x,y);
							gwin->mouse_state = BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
						}
						else if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
						{
							browser_window_mouse_track(gwin->bw,BROWSER_MOUSE_DRAG_2 | gwin->key_state,x,y);
							gwin->mouse_state = BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
						}
						else
						{
							browser_window_mouse_track(gwin->bw,gwin->mouse_state | gwin->key_state,x,y);
						}
					}
					else
					{
						ami_context_menu_mouse_trap(gwin, FALSE);

						if(!gwin->mouse_state) ami_set_pointer(gwin, GUI_POINTER_DEFAULT, true);
					}
				break;

				case WMHI_MOUSEBUTTONS:
					GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER],
								(ULONG *)&bbox);

					x = (ULONG)((gwin->win->MouseX - bbox->Left) / gwin->bw->scale);
					y = (ULONG)((gwin->win->MouseY - bbox->Top) / gwin->bw->scale);

					ami_get_hscroll_pos(gwin, (ULONG *)&xs);
					ami_get_vscroll_pos(gwin, (ULONG *)&ys);

					x += xs;
					y += ys;

					width=bbox->Width;
					height=bbox->Height;

					ami_update_quals(gwin);

					if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
					{
						//code = code>>16;
						switch(code)
						{
							case SELECTDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_1 | gwin->key_state,x,y);
								gwin->mouse_state=BROWSER_MOUSE_PRESS_1;
							break;
							case MIDDLEDOWN:
								browser_window_mouse_click(gwin->bw,BROWSER_MOUSE_PRESS_2 | gwin->key_state,x,y);
								gwin->mouse_state=BROWSER_MOUSE_PRESS_2;
							break;
						}
					}

					if(x<xs) x=xs;
					if(y<ys) y=ys;
					if(x>=width+xs) x=width+xs-1;
					if(y>=height+ys) y=height+ys-1;

					switch(code)
					{
						case MENUDOWN:
							ami_context_menu_show(gwin,x,y);
						break;

						case SELECTUP:
							if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
							{
								CurrentTime(&curtime.tv_sec, &curtime.tv_usec);

								gwin->mouse_state = BROWSER_MOUSE_CLICK_1;

								if(gwin->lastclick.tv_sec)
								{
									if(DoubleClick(gwin->lastclick.tv_sec,
												gwin->lastclick.tv_usec,
												curtime.tv_sec, curtime.tv_usec)) {
										if(gwin->prev_mouse_state & BROWSER_MOUSE_DOUBLE_CLICK) {
											gwin->mouse_state |= BROWSER_MOUSE_TRIPLE_CLICK;
										} else {
											gwin->mouse_state |= BROWSER_MOUSE_DOUBLE_CLICK;
										}
									}
								}

								browser_window_mouse_click(gwin->bw,
									gwin->mouse_state | gwin->key_state,x,y);

								if(gwin->mouse_state & BROWSER_MOUSE_TRIPLE_CLICK)
								{
									gwin->lastclick.tv_sec = 0;
									gwin->lastclick.tv_usec = 0;
								}
								else
								{
									gwin->lastclick.tv_sec = curtime.tv_sec;
									gwin->lastclick.tv_usec = curtime.tv_usec;
								}
							}
							else
							{
								browser_window_mouse_track(gwin->bw, 0, x, y);
							}
							gwin->prev_mouse_state = gwin->mouse_state;
							gwin->mouse_state=0;
						break;

						case MIDDLEUP:
							if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
							{
								CurrentTime(&curtime.tv_sec, &curtime.tv_usec);

								gwin->mouse_state = BROWSER_MOUSE_CLICK_2;

								if(gwin->lastclick.tv_sec)
								{
									if(DoubleClick(gwin->lastclick.tv_sec,
												gwin->lastclick.tv_usec,
												curtime.tv_sec, curtime.tv_usec)) {
										if(gwin->prev_mouse_state & BROWSER_MOUSE_DOUBLE_CLICK) {
											gwin->mouse_state |= BROWSER_MOUSE_TRIPLE_CLICK;
										} else {
											gwin->mouse_state |= BROWSER_MOUSE_DOUBLE_CLICK;
										}
									}
								}

								browser_window_mouse_click(gwin->bw,
									gwin->mouse_state | gwin->key_state,x,y);

								if(gwin->mouse_state & BROWSER_MOUSE_TRIPLE_CLICK)
								{
									gwin->lastclick.tv_sec = 0;
									gwin->lastclick.tv_usec = 0;
								}
								else
								{
									gwin->lastclick.tv_sec = curtime.tv_sec;
									gwin->lastclick.tv_usec = curtime.tv_usec;
								}
							}
							else
							{
								browser_window_mouse_track(gwin->bw, 0, x, y);
							}
							gwin->prev_mouse_state = gwin->mouse_state;
							gwin->mouse_state=0;
						break;

						case SIDEUP:
							ami_gui_history(gwin, true);
						break;

						case EXTRAUP:
							ami_gui_history(gwin, false);
						break;
					}

					if(drag_save_data && !gwin->mouse_state)
						ami_drag_save(gwin->win);
				break;

				case WMHI_GADGETUP:
					switch(result & WMHI_GADGETMASK)
					{
						case GID_TABS:
							if(gwin->objects[GID_TABS] == NULL) break;
							GetAttrs(gwin->objects[GID_TABS],
								CLICKTAB_NodeClosed, &tabnode, TAG_DONE);
							if(tabnode)
							{
								GetClickTabNodeAttrs(tabnode,
									TNA_UserData, &closedbw,
									TAG_DONE);

								browser_window_destroy(closedbw);
							}
							else
							{
								ami_switch_tab(gwin, true);
							}
						break;

						case GID_CLOSETAB:
							browser_window_destroy(gwin->bw);
						break;

						case GID_ADDTAB:
							ami_gui_new_blank_tab(gwin);
						break;

						case GID_URL:
							GetAttr(STRINGA_TextVal,
								(Object *)gwin->objects[GID_URL],
								(ULONG *)&storage);
							if(utf8 = ami_to_utf8_easy((const char *)storage)) {
								if(search_is_url((char *)utf8) == false)
								{
									utf8 = search_web_from_term(utf8);
								}

								if (nsurl_create((char *)utf8, &url) != NSERROR_OK) {
									warn_user("NoMemory", 0);
								} else {
									browser_window_navigate(gwin->bw,
											url,
											NULL,
											BW_NAVIGATE_HISTORY,
											NULL,
											NULL,
											NULL);
									nsurl_unref(url);
								}
								ami_utf8_free(utf8);
							} else {
								warn_user("NoMemory", 0);
							}
						break;

						case GID_TOOLBARLAYOUT:
							/* Need fixing: never gets here */
							search_web_retrieve_ico(false);
						break;

						case GID_SEARCHSTRING:
							GetAttr(STRINGA_TextVal,
								(Object *)gwin->objects[GID_SEARCHSTRING],
								(ULONG *)&storage);
							if(utf8 = ami_to_utf8_easy((const char *)storage)) {
								storage = (ULONG)search_web_from_term(utf8);
								ami_utf8_free(utf8);

								if (nsurl_create((char *)storage, &url) != NSERROR_OK) {
									warn_user("NoMemory", 0);
								} else {
									browser_window_navigate(gwin->bw,
											url,
											NULL,
											BW_NAVIGATE_HISTORY,
											NULL,
											NULL,
											NULL);
									nsurl_unref(url);
								}
							} else {
								warn_user("NoMemory", 0);
							}
						break;

						case GID_HOME:
							{
								if (nsurl_create(nsoption_charp(homepage_url), &url) != NSERROR_OK) {
									warn_user("NoMemory", 0);
								} else {
									browser_window_navigate(gwin->bw,
											url,
											NULL,
											BW_NAVIGATE_HISTORY,
											NULL,
											NULL,
											NULL);
									nsurl_unref(url);
								}
							}
						break;

						case GID_STOP:
							if(browser_window_stop_available(gwin->bw))
								browser_window_stop(gwin->bw);
						break;

						case GID_RELOAD:
							ami_update_quals(gwin);

							if(browser_window_reload_available(gwin->bw))
							{
								if(gwin->key_state & BROWSER_MOUSE_MOD_1)
								{
									browser_window_reload(gwin->bw,true);
								}
								else
								{
									browser_window_reload(gwin->bw,false);
								}
							}
						break;

						case GID_BACK:
							ami_gui_history(gwin, true);
						break;

						case GID_FORWARD:
							ami_gui_history(gwin, false);
						break;

						case GID_FAVE:
							GetAttr(STRINGA_TextVal,
								(Object *)gwin->objects[GID_URL],
								(ULONG *)&storage);
							if(nsurl_create((const char *)storage, &url) == NSERROR_OK) {
								if(hotlist_has_url(url)) {
									hotlist_remove_url(url);
								} else {
									hotlist_add_url(url);
								}
								nsurl_unref(url);
							}
							ami_gui_update_hotlist_button(gwin);
						break;

						case GID_HOTLIST:
						default:
//							printf("GADGET: %ld\n",(result & WMHI_GADGETMASK));
						break;
					}
				break;

				case WMHI_RAWKEY:
					ami_update_quals(gwin);
				
					storage = result & WMHI_GADGETMASK;
					if(storage >= IECODE_UP_PREFIX) break;

					GetAttr(WINDOW_InputEvent,gwin->objects[OID_MAIN],(ULONG *)&ie);

					nskey = ami_key_to_nskey(storage, ie);

					if(ie->ie_Qualifier & IEQUALIFIER_RCOMMAND)
					{
/* We are duplicating the menu shortcuts here, as if RMBTRAP is active
 * (ie. when context menus are enabled and the mouse is over the browser
 * rendering area), Intuition also does not catch the menu shortcut
 * key presses.  Context menus need to be changed to use MENUVERIFY not RMBTRAP */
						switch(nskey)
						{
							case 'n':
								if ((nsoption_bool(kiosk_mode) == false)) {
									nsurl *urlns;
									nserror error;

									error = nsurl_create(nsoption_charp(homepage_url), &urlns);
									if (error == NSERROR_OK) {
										error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
													      urlns,
													      NULL,
													      gwin->bw,
													      NULL);
										nsurl_unref(urlns);
									}
									if (error != NSERROR_OK) {
										warn_user(messages_get_errorcode(error), 0);
									}

								}
							break;

							case 't':
								if((nsoption_bool(kiosk_mode) == false)) {
									nsurl *urlns;
									nserror error;

									error = nsurl_create(nsoption_charp(homepage_url), &urlns);
									if (error == NSERROR_OK) {
										error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY |
													      BW_CREATE_TAB,
													      urlns,
													      NULL,
													      gwin->bw,
													      NULL);
										nsurl_unref(urlns);
									}
									if (error != NSERROR_OK) {
										warn_user(messages_get_errorcode(error), 0);
									}

								}
							break;

							case 'k':
								if((nsoption_bool(kiosk_mode) == false))
									browser_window_destroy(gwin->bw);
							break;

							case 'o':
								ami_file_open(gwin);
							break;

							case 's':
								ami_file_save_req(AMINS_SAVE_SOURCE, gwin,
									gwin->bw->current_content);
							break;

							case 'p':
								ami_print_ui(gwin->bw->current_content);
							break;

							case 'q':
								if((nsoption_bool(kiosk_mode) == false))
									ami_quit_netsurf();
							break;

							case 'a':
								browser_window_key_press(gwin->bw, KEY_SELECT_ALL);
							break;

							case 'x':
								browser_window_key_press(gwin->bw, KEY_CUT_SELECTION);
							break;

							case 'c':
								browser_window_key_press(gwin->bw, KEY_COPY_SELECTION);
								browser_window_key_press(gwin->bw, KEY_CLEAR_SELECTION);
							break;

							case 'v':
								browser_window_key_press(gwin->bw, KEY_PASTE);
							break;

							case 'z':
								browser_window_key_press(gwin->bw, KEY_UNDO);
							break;

							case 'y':
								browser_window_key_press(gwin->bw, KEY_REDO);
							break;

							case 'f':
								ami_search_open(gwin->bw->window);
							break;

							case 'h':
								if((nsoption_bool(kiosk_mode) == false))
									ami_tree_open(hotlist_window, AMI_TREE_HOTLIST);
							break;

							case '-':
								if(browser_window_get_scale(gwin->bw) > 0.1)
									browser_window_set_scale(gwin->bw,
										browser_window_get_scale(gwin->bw) - 0.1, false);
							break;
							
							case '=':
								browser_window_set_scale(gwin->bw, 1.0, false);
							break;

							case '+':
								browser_window_set_scale(gwin->bw,
									browser_window_get_scale(gwin->bw) + 0.1, false);
							break;
							
							/* The following aren't available from the menu at the moment */

							case 'r': // reload
								if(browser_window_reload_available(gwin->bw))
									browser_window_reload(gwin->bw,false);
							break;

							case 'u': // open url
								if((nsoption_bool(kiosk_mode) == false))
									ActivateLayoutGadget((struct Gadget *)gwin->objects[GID_MAIN],
										gwin->win, NULL, (uint32)gwin->objects[GID_URL]);
							break;
						}
					}
					else
					{
						if(!browser_window_key_press(gwin->bw, nskey))
						{
							switch(nskey)
							{
								case KEY_UP:
									ami_gui_scroll_internal(gwin, 0, -NSA_KBD_SCROLL_PX);
								break;

								case KEY_DOWN:
									ami_gui_scroll_internal(gwin, 0, +NSA_KBD_SCROLL_PX);
								break;

								case KEY_LEFT:
									ami_gui_scroll_internal(gwin, -NSA_KBD_SCROLL_PX, 0);
								break;

								case KEY_RIGHT:
									ami_gui_scroll_internal(gwin, +NSA_KBD_SCROLL_PX, 0);
								break;

								case KEY_PAGE_UP:
									ami_gui_scroll_internal(gwin, 0, SCROLL_PAGE_UP);
								break;

								case KEY_PAGE_DOWN:
									ami_gui_scroll_internal(gwin, 0, SCROLL_PAGE_DOWN);
								break;

								case KEY_LINE_START: // page left
									ami_gui_scroll_internal(gwin, SCROLL_PAGE_UP, 0);
								break;

								case KEY_LINE_END: // page right
									ami_gui_scroll_internal(gwin, SCROLL_PAGE_DOWN, 0);
								break;

								case KEY_TEXT_START: // home
									ami_gui_scroll_internal(gwin, SCROLL_TOP, SCROLL_TOP);
								break;

								case KEY_TEXT_END: // end
									ami_gui_scroll_internal(gwin, SCROLL_BOTTOM, SCROLL_BOTTOM);
								break;

								case KEY_WORD_RIGHT: // alt+right
									ami_change_tab(gwin, 1);
								break;

								case KEY_WORD_LEFT: // alt+left
									ami_change_tab(gwin, -1);
								break;

								case KEY_DELETE_LEFT: // backspace
									ami_gui_history(gwin, true);
								break;

								case RAWKEY_F5: // reload
									if(browser_window_reload_available(gwin->bw))
										browser_window_reload(gwin->bw,false);
								break;
								
								case RAWKEY_HELP: // help
									ami_help_open(AMI_HELP_GUI, scrn);
								break;
							}
						}
					}
				break;

				case WMHI_NEWSIZE:
					switch(node->Type)
					{
						struct Node *tab = NULL, *ntab = NULL;
						struct browser_window *bw = NULL;

						case AMINS_WINDOW:
							ami_set_border_gadget_balance(gwin);
							ami_update_throbber(gwin,true);

							if(gwin->tabs)
							{
								tab = GetHead(&gwin->tab_list);

								do
								{
									ntab=GetSucc(tab);
									GetClickTabNodeAttrs(tab,
										TNA_UserData, &bw,
										TAG_DONE);
									bw->reformat_pending = true;
								} while(tab=ntab);
							}

							gwin->bw->reformat_pending = true;
							ami_schedule_redraw(gwin, true);
						break;
					}
				break;

				case WMHI_CLOSEWINDOW:
					ami_close_all_tabs(gwin);
		        break;

				case WMHI_ICONIFY:
				{
					struct bitmap *bm;

					bm = urldb_get_thumbnail(hlcache_handle_get_url(gwin->bw->current_content));
					if(!bm) bm = content_get_bitmap(gwin->bw->current_content);
					gwin->dobj = amiga_icon_from_bitmap(bm);
					amiga_icon_superimpose_favicon_internal(gwin->bw->window->favicon,
						gwin->dobj);
					HideWindow(gwin->win);
					gwin->appicon = AddAppIcon((ULONG)gwin->objects[OID_MAIN],
										(ULONG)gwin, gwin->win->Title, appport,
										0, gwin->dobj, NULL);

					curbw = NULL;
				}
				break;

				case WMHI_INACTIVE:
					gwin->bw->window->c_h_temp = gwin->bw->window->c_h;
					gui_window_remove_caret(gwin->bw->window);
				break;

				case WMHI_ACTIVE:
					if(gwin->bw) curbw = gwin->bw;
					if(gwin->bw->window->c_h_temp)
						gwin->bw->window->c_h = gwin->bw->window->c_h_temp;
				break;

				case WMHI_INTUITICK:
				break;

	   	     	default:
					//printf("class: %ld\n",(result & WMHI_CLASSMASK));
   	       		break;
			}

			if(win_destroyed)
			{
					/* we can't be sure what state our window_list is in, so let's
					jump out of the function and start again */

				win_destroyed = false;
				return;
			}

			if(drag_x_move || drag_y_move)
			{
				gui_window_get_scroll(gwin->bw->window,
					&gwin->bw->window->scrollx, &gwin->bw->window->scrolly);

				gui_window_set_scroll(gwin->bw->window,
					gwin->bw->window->scrollx + drag_x_move,
					gwin->bw->window->scrolly + drag_y_move);
			}

//	ReplyMsg((struct Message *)message);
		}

		if(node->Type == AMINS_WINDOW)
		{
			/* Catch any reformats tagged by the core - only used by scale? */
			if(gwin->bw->reformat_pending) {
				ami_schedule_redraw(gwin, true);
			}
			
			if(gwin->bw->window->throbbing)
				ami_update_throbber(gwin,false);
		}
	} while(node = nnode);

	if(refresh_search_ico)
	{
		gui_set_search_ico(NULL);
		refresh_search_ico = FALSE;
	}

	if(refresh_favicon)
	{
		gui_window_set_icon(gwin->bw->window, gwin->bw->window->favicon);
		refresh_favicon = FALSE;
	}

	if(refresh_throbber)
	{
		ami_update_throbber(gwin, true);
		refresh_throbber = FALSE;
	}
	
	if(ami_menu_window_close)
	{
		if(ami_menu_window_close == (void *)AMI_MENU_WINDOW_CLOSE_ALL)
			ami_quit_netsurf();
		else
			ami_close_all_tabs(ami_menu_window_close);
			
		ami_menu_window_close = NULL;
	}
	
	if(ami_menu_check_toggled) {
		ami_gui_menu_update_all();
		ami_menu_check_toggled = false;
	}
}

void ami_gui_appicon_remove(struct gui_window_2 *gwin)
{
	if(gwin->appicon)
	{
		RemoveAppIcon(gwin->appicon);
		amiga_icon_free(gwin->dobj);
		gwin->appicon = NULL;
	}
}

void ami_handle_appmsg(void)
{
	struct AppMessage *appmsg;
	struct gui_window_2 *gwin;
	int x, y;
	struct WBArg *appwinargs;
	STRPTR filename;
	char *urlfilename;
	int i = 0;

	while(appmsg=(struct AppMessage *)GetMsg(appport))
	{
		gwin = (struct gui_window_2 *)appmsg->am_UserData;

		if(appmsg->am_Type == AMTYPE_APPICON)
		{
			ami_gui_appicon_remove(gwin);
			ShowWindow(gwin->win, WINDOW_FRONTMOST);
			ActivateWindow(gwin->win);
		}
		else if(appmsg->am_Type == AMTYPE_APPWINDOW)
		{
			for(i = 0; i < appmsg->am_NumArgs; ++i)
			{
				if(appwinargs = &appmsg->am_ArgList[i])
				{
					if(filename = AllocVecTagList(1024, NULL))
					{
						if(appwinargs->wa_Lock)
						{
							NameFromLock(appwinargs->wa_Lock, filename, 1024);
						}

						AddPart(filename, appwinargs->wa_Name, 1024);

						if(ami_mouse_to_ns_coords(gwin, &x, &y,
							appmsg->am_MouseX, appmsg->am_MouseY) == false)
						{
							nsurl *url;
							urlfilename = path_to_url(filename);

							if (nsurl_create(urlfilename, &url) != NSERROR_OK) {
								warn_user("NoMemory", 0);
							}
							else
							{
								if(i == 0)
								{
									browser_window_navigate(gwin->bw,
										url,
										NULL,
										BW_NAVIGATE_HISTORY,
										NULL,
										NULL,
										NULL);

									ActivateWindow(gwin->win);
								}
								else
								{
									browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY |
											      BW_CREATE_TAB,
											      url,
											      NULL,
											      gwin->bw,
											      NULL);
								}
								nsurl_unref(url);
							}

							free(urlfilename);
						}
						else
						{
							if(browser_window_drop_file_at_point(gwin->bw, x, y, filename) == false)
							{
								nsurl *url;
								urlfilename = path_to_url(filename);

								if (nsurl_create(urlfilename, &url) != NSERROR_OK) {
									warn_user("NoMemory", 0);
								}
								else
								{

									if(i == 0)
									{
										browser_window_navigate(gwin->bw,
											url,
											NULL,
											BW_NAVIGATE_HISTORY,
											NULL,
											NULL,
											NULL);

										ActivateWindow(gwin->win);
									}
									else
									{
										browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY |
												      BW_CREATE_TAB,
												      url,
												      NULL,
												      gwin->bw,
												      NULL);
										
									}
									nsurl_unref(url);
								}
								free(urlfilename);
							}
						}
						FreeVec(filename);
					}
				}
			}
		}
		ReplyMsg((struct Message *)appmsg);
	}
}

void ami_handle_applib(void)
{
	struct ApplicationMsg *applibmsg;
	struct browser_window *bw;
	nsurl *url;
	nserror error;

	if(!applibport) return;

	while((applibmsg=(struct ApplicationMsg *)GetMsg(applibport)))
	{
		switch (applibmsg->type)
		{
			case APPLIBMT_NewBlankDoc:
			{

				error = nsurl_create(nsoption_charp(homepage_url), &url);
				if (error == NSERROR_OK) {
					error = browser_window_create(BW_CREATE_HISTORY,
								      url,
								      NULL,
								      NULL,
								      &bw);
					nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					warn_user(messages_get_errorcode(error), 0);
				}
			}
			break;

			case APPLIBMT_OpenDoc:
			{
				struct ApplicationOpenPrintDocMsg *applibopdmsg =
					(struct ApplicationOpenPrintDocMsg *)applibmsg;
				char *tempurl;

				tempurl = path_to_url(applibopdmsg->fileName);

				error = nsurl_create(tempurl, &url);
				if (error == NSERROR_OK) {
					error = browser_window_create(BW_CREATE_HISTORY,
								      url,
								      NULL,
								      NULL,
								      &bw);
					nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					warn_user(messages_get_errorcode(error), 0);
				}
				free(tempurl);
			}
			break;

			case APPLIBMT_ToFront:
				if(curbw)
				{
					ScreenToFront(scrn);
					WindowToFront(curbw->window->shared->win);
					ActivateWindow(curbw->window->shared->win);
				}
			break;

			case APPLIBMT_OpenPrefs:
				ScreenToFront(scrn);
				ami_gui_opts_open();
			break;

			case APPLIBMT_Quit:
			case APPLIBMT_ForceQuit:
				ami_quit_netsurf();
			break;

			case APPLIBMT_CustomMsg:
			{
				struct ApplicationCustomMsg *applibcustmsg =
					(struct ApplicationCustomMsg *)applibmsg;
				LOG(("Ringhio BackMsg received: %s", applibcustmsg->customMsg));
				OpenWorkbenchObjectA(applibcustmsg->customMsg, NULL);
			}
			break;
		}
		ReplyMsg((struct Message *)applibmsg);
	}
}

void ami_get_msg(void)
{
	ULONG winsignal = 1L << sport->mp_SigBit;
	ULONG appsig = 1L << appport->mp_SigBit;
	ULONG schedulesig = 1L << msgport->mp_SigBit;
	ULONG ctrlcsig = SIGBREAKF_CTRL_C;
	ULONG signal;
	struct TimerRequest *timermsg = NULL;
	struct MsgPort *printmsgport = ami_print_get_msgport();
	ULONG printsig = 0;
	ULONG helpsignal = ami_help_signal();
	if(printmsgport) printsig = 1L << printmsgport->mp_SigBit;
	ULONG signalmask = winsignal | appsig | schedulesig | rxsig | printsig | applibsig | ctrlcsig | helpsignal;

	signal = Wait(signalmask);

	if(signal & winsignal)
		ami_handle_msg();

	if(signal & appsig)
		ami_handle_appmsg();

	if(signal & rxsig)
		ami_arexx_handle();

	if(signal & applibsig)
		ami_handle_applib();

	if(signal & printsig) {
		while(GetMsg(printmsgport));  //ReplyMsg
		ami_print_cont();
	}

	if(signal & schedulesig) {
		if(timermsg = (struct TimerRequest *)GetMsg(msgport)) {
			ReplyMsg((struct Message *)timermsg);
			schedule_run(FALSE);
		}
	}

	if(signal & helpsignal)
		ami_help_process();

	if(signal & ctrlcsig)
		ami_quit_netsurf_delayed();
}

static void ami_gui_fetch_callback(void *p)
{
	/* This doesn't need to do anything - the scheduled event will
	 * send a message to trigger Wait() to return, thereby causing
	 * the event function to return, and NetSurf to call
	 * hlcache_poll() as part of the usual fetch/event loop.
	 */
}

static void gui_poll(bool active)
{
	if(active) ami_schedule(0, ami_gui_fetch_callback, NULL);
	ami_get_msg();
}

void ami_change_tab(struct gui_window_2 *gwin, int direction)
{
	struct Node *tab_node = gwin->bw->window->tab_node;
	struct Node *ptab;
	ULONG ptabnum = 0;

	if(gwin->tabs <= 1) return;

	if(direction > 0)
	{
		ptab = GetSucc(tab_node);
	}
	else
	{
		ptab = GetPred(tab_node);
	}

	if(!ptab) return;

	GetClickTabNodeAttrs(ptab, TNA_Number, (ULONG *)&ptabnum, TAG_DONE);

	RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABS], gwin->win, NULL,
						CLICKTAB_Current, ptabnum,
						TAG_DONE);

	ami_switch_tab(gwin, true);
}

void ami_switch_tab(struct gui_window_2 *gwin,bool redraw)
{
	struct Node *tabnode;
	struct IBox *bbox;

	/* Clear the last new tab list */
	gwin->bw->window->last_new_tab = NULL;

	if(gwin->tabs == 0) return;

	gui_window_get_scroll(gwin->bw->window,
		&gwin->bw->window->scrollx, &gwin->bw->window->scrolly);

	GetAttr(CLICKTAB_CurrentNode, (Object *)gwin->objects[GID_TABS],
				(ULONG *)&tabnode);
	GetClickTabNodeAttrs(tabnode,
						TNA_UserData, &gwin->bw,
						TAG_DONE);
	curbw = gwin->bw;
	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER], (ULONG *)&bbox);

	if(!gwin->bw->current_content)
	{
		RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_URL],
			gwin->win, NULL, STRINGA_TextVal, "", TAG_DONE);

		p96RectFill(gwin->win->RPort, bbox->Left, bbox->Top,
			bbox->Width+bbox->Left, bbox->Height+bbox->Top, 0xffffffff);
		return;
	}

	ami_plot_release_pens(&gwin->shared_pens);
	ami_update_buttons(gwin);
	ami_menu_update_disabled(gwin->bw->window, gwin->bw->current_content);

	if(redraw)
	{
		gui_window_set_icon(gwin->bw->window, gwin->bw->window->favicon);

		p96RectFill(gwin->win->RPort, bbox->Left, bbox->Top,
			bbox->Width+bbox->Left, bbox->Height+bbox->Top, 0xffffffff);

		browser_window_update(gwin->bw, false);

		gui_window_set_scroll(gwin->bw->window,
			gwin->bw->window->scrollx, gwin->bw->window->scrolly);
		gwin->redraw_scroll = false;

		browser_window_refresh_url_bar(gwin->bw);
			
		ami_gui_update_hotlist_button(gwin);
	}
}

void ami_quit_netsurf(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(!IsMinListEmpty(window_list)) {
		node = (struct nsObject *)GetHead((struct List *)window_list);

		do {
			nnode=(struct nsObject *)GetSucc((struct Node *)node);
			gwin = node->objstruct;

			switch(node->Type) {
				case AMINS_TVWINDOW:
					ami_tree_close((struct treeview_window *)gwin);
				break;

				case AMINS_WINDOW:
					/* This also closes windows that are attached to the
					 * gui_window, such as local history and find. */
					ShowWindow(gwin->win, WINDOW_BACKMOST);
					ami_close_all_tabs(gwin);
				break;

				case AMINS_GUIOPTSWINDOW:
					ami_gui_opts_close();
				break;

				case AMINS_DLWINDOW:
					ami_download_window_abort((struct gui_download_window *)gwin);
				break;
			}
		} while(node = nnode);

		win_destroyed = true;
	}

	if(IsMinListEmpty(window_list)) {
		/* last window closed, so exit */
		netsurf_quit = true;
	}
}

void ami_quit_netsurf_delayed(void)
{
	int res = -1;
#ifdef __amigaos4__
	char *utf8text = ami_utf8_easy(messages_get("TCPIPShutdown"));
	char *utf8gadgets = ami_utf8_easy(messages_get("AbortShutdown"));

	DisplayBeep(NULL);
	
	res = TimedDosRequesterTags(TDR_ImageType, TDRIMAGE_INFO,
		TDR_TitleString, messages_get("NetSurf"),
		TDR_FormatString, utf8text,
		TDR_GadgetString, utf8gadgets,
		TDR_Timeout, 5,
		TDR_Inactive, TRUE,
		TAG_DONE);
	
	free(utf8text);
	free(utf8gadgets);
#endif
	if(res == -1) { /* Requester timed out */
		nsoption_set_bool(tab_close_warn, false);
		ami_quit_netsurf();
	}
}

void ami_gui_close_screen(struct Screen *scrn, BOOL locked_screen, BOOL donotwait)
{
	if(scrn == NULL) return;
	if(CloseScreen(scrn) == TRUE) {
		if(locked_screen == FALSE) {
			FreeSignal(screen_signal);
			screen_signal = -1;
			scrn = NULL;
		}
		return;
	}
	if(locked_screen == TRUE) return;
	if(donotwait == TRUE) return;

	/* If this is our own screen, wait for visitor windows to close */
	if(screen_signal != -1) {
		ULONG scrnsig = 1 << screen_signal;
		LOG(("Waiting for visitor windows to close... (signal)"));
		Wait(scrnsig);
	}

	while (CloseScreen(scrn) == FALSE) {
		LOG(("Waiting for visitor windows to close... (polling)"));
		Delay(50);
	}

	FreeSignal(screen_signal);
	screen_signal = -1;
	scrn = NULL;
}

void ami_try_quit(void)
{
	if(!IsMinListEmpty(window_list)) return;

	if(nsoption_bool(close_no_quit) == false)
	{
		netsurf_quit = true;
		return;
	}
	else
	{
		ami_gui_close_screen(scrn, locked_screen, TRUE);
	}
}

static void gui_quit(void)
{
	ami_theme_throbber_free();

	urldb_save(nsoption_charp(url_file));
	urldb_save_cookies(nsoption_charp(cookie_file));
	ami_hotlist_free(nsoption_charp(hotlist_file));
	ami_cookies_free();
	ami_global_history_free();

	if(IApplication && ami_appid)
		UnregisterApplication(ami_appid, NULL);

	ami_arexx_cleanup();

	ami_free_layers(&browserglob);

	ami_close_fonts();
	ami_help_free();
	
	LOG(("Closing screen"));
	ami_gui_close_screen(scrn, locked_screen, FALSE);
	FreeVec(nsscreentitle);

	LOG(("Freeing menu items"));
	ami_context_menu_free();
	ami_menu_free_glyphs();

	LOG(("Freeing mouse pointers"));
	ami_mouse_pointers_free();
	LOG(("Freeing clipboard"));
	ami_clipboard_free();

	FreeSysObject(ASOT_PORT,appport);
	FreeSysObject(ASOT_PORT,sport);

	ami_file_req_free();
	ami_openurl_close();
	FreeStringClass(urlStringClass);

	if(IApplication) DropInterface((struct Interface *)IApplication);
	if(ApplicationBase) CloseLibrary(ApplicationBase);

	if(IKeymap) DropInterface((struct Interface *)IKeymap);
	if(KeymapBase) CloseLibrary(KeymapBase);

	LOG(("Freeing scheduler"));
	ami_schedule_free();
	ami_schedule_close_timer();

	FreeObjList(window_list);

	FreeVec(current_user_options);
	FreeVec(current_user_dir);
	FreeVec(current_user);
}

void ami_gui_update_hotlist_button(struct gui_window_2 *gwin)
{
	char *url;
	nsurl *nsurl;

	GetAttr(STRINGA_TextVal,
		(Object *)gwin->objects[GID_URL],
		(ULONG *)&url);

	if(nsurl_create(url, &nsurl) == NSERROR_OK) {
		if(hotlist_has_url(nsurl)) {
			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_FAVE], gwin->win, NULL,
				BUTTON_RenderImage, gwin->objects[GID_FAVE_RMV], TAG_DONE);
		} else {
			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_FAVE], gwin->win, NULL,
				BUTTON_RenderImage, gwin->objects[GID_FAVE_ADD], TAG_DONE);
		}
		
		nsurl_unref(nsurl);
	}
}

void ami_update_buttons(struct gui_window_2 *gwin)
{
	BOOL back=FALSE,forward=TRUE,tabclose=FALSE,stop=FALSE,reload=FALSE;
	uint32 storage = 0;

	if(!browser_window_back_available(gwin->bw))
		back=TRUE;

	if(browser_window_forward_available(gwin->bw))
		forward=FALSE;

	if(!browser_window_stop_available(gwin->bw))
		stop=TRUE;

	if(!browser_window_reload_available(gwin->bw))
		reload=TRUE;

	if(nsoption_bool(kiosk_mode) == false)
	{
		if(gwin->tabs <= 1)
		{
			tabclose=TRUE;
			OffMenu(gwin->win,AMI_MENU_CLOSETAB);
		}
		else
		{
			OnMenu(gwin->win,AMI_MENU_CLOSETAB);
		}
	}

	GetAttr(GA_Disabled, gwin->objects[GID_BACK], &storage);
	if(storage != back)
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_BACK],
			gwin->win, NULL, GA_Disabled, back, TAG_DONE);

	GetAttr(GA_Disabled, gwin->objects[GID_FORWARD], &storage);
	if(storage != forward)
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_FORWARD],
			gwin->win, NULL, GA_Disabled, forward, TAG_DONE);

	GetAttr(GA_Disabled, gwin->objects[GID_RELOAD], &storage);
	if(storage != reload)
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_RELOAD],
			gwin->win, NULL, GA_Disabled, reload, TAG_DONE);

	GetAttr(GA_Disabled, gwin->objects[GID_STOP], &storage);
	if(storage != stop)
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_STOP],
			gwin->win, NULL, GA_Disabled, stop, TAG_DONE);

	if((gwin->tabs) && (ClickTabBase->lib_Version < 53))
	{
		GetAttr(GA_Disabled, gwin->objects[GID_CLOSETAB], &storage);
		if(storage != tabclose)
			SetGadgetAttrs((struct Gadget *)gwin->objects[GID_CLOSETAB],
				gwin->win, NULL, GA_Disabled, tabclose, TAG_DONE);
	}
}

static bool ami_gui_hotlist_add(void *userdata, int level, int item, const char *title, nsurl *url, bool is_folder)
{
	struct ami_gui_tb_userdata *tb_userdata = (struct ami_gui_tb_userdata *)userdata;
	struct Node *speed_button_node;

	if(level != 1) return false;
	if(item > AMI_GUI_TOOLBAR_MAX) return false;
	if(is_folder == true) return false;
	
	tb_userdata->gw->hotlist_toolbar_lab[item] = ami_utf8_easy(title);

	speed_button_node = AllocSpeedButtonNode(item,
					SBNA_Text, tb_userdata->gw->hotlist_toolbar_lab[item],
					SBNA_UserData, (void *)url,
					TAG_DONE);
			
	AddTail(tb_userdata->sblist, speed_button_node);

	tb_userdata->items++;
	return true;
}

int ami_gui_hotlist_scan(struct tree *tree, struct List *speed_button_list, struct gui_window_2 *gwin)
{
	struct ami_gui_tb_userdata userdata;
	userdata.gw = gwin;
	userdata.sblist = speed_button_list;
	userdata.items = 0;

	ami_hotlist_scan((void *)&userdata, 0, messages_get("HotlistToolbar"), ami_gui_hotlist_add);
	return userdata.items;
}

void ami_gui_hotlist_toolbar_add(struct gui_window_2 *gwin)
{
	struct TagItem attrs[2];

	attrs[0].ti_Tag = CHILD_MinWidth;
	attrs[0].ti_Data = 0;
	attrs[1].ti_Tag = TAG_DONE;
	attrs[1].ti_Data = 0;

	NewList(&gwin->hotlist_toolbar_list);

	if(ami_gui_hotlist_scan(ami_tree_get_tree(hotlist_window), &gwin->hotlist_toolbar_list, gwin) > 0) {
		gwin->objects[GID_HOTLIST] =
				SpeedBarObject,
					GA_ID, GID_HOTLIST,
					GA_RelVerify, TRUE,
					ICA_TARGET, ICTARGET_IDCMP,
					SPEEDBAR_BevelStyle, BVS_NONE,
					SPEEDBAR_Buttons, &gwin->hotlist_toolbar_list,
				SpeedBarEnd;
				
		gwin->objects[GID_HOTLISTSEPBAR] =
				BevelObject,
					BEVEL_Style, BVS_SBAR_VERT,
				BevelEnd;

		IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_ADDCHILD,
				gwin->win, gwin->objects[GID_HOTLIST], attrs);

		IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_ADDIMAGE,
				gwin->win, gwin->objects[GID_HOTLISTSEPBAR], NULL);

		FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

		RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
				gwin->win, NULL, TRUE);
		
		if(gwin->bw) {
			gwin->bw->reformat_pending = true;
			ami_schedule_redraw(gwin, true);
		}
	}
}

void ami_gui_hotlist_toolbar_free(struct gui_window_2 *gwin, struct List *speed_button_list)
{
	int i;
	struct Node *node;
	struct Node *nnode;

	if(IsListEmpty(speed_button_list)) return;
	node = GetHead(speed_button_list);

	do {
		nnode = GetSucc(node);
		Remove(node);
		FreeSpeedButtonNode(node);
	} while(node = nnode);
		
	for(i = 0; i < AMI_GUI_TOOLBAR_MAX; i++) {
		if(gwin->hotlist_toolbar_lab[i]) {
			free(gwin->hotlist_toolbar_lab[i]);
			gwin->hotlist_toolbar_lab[i] = NULL;
		}
	}
}

void ami_gui_hotlist_toolbar_remove(struct gui_window_2 *gwin)
{
	IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_HOTLIST]);

	IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_HOTLISTSEPBAR]);

	FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
			gwin->win, NULL, TRUE);

	gwin->bw->reformat_pending = true;
	ami_schedule_redraw(gwin, true);
}

void ami_gui_hotlist_toolbar_update(struct gui_window_2 *gwin)
{
	if(IsListEmpty(&gwin->hotlist_toolbar_list)) {
		ami_gui_hotlist_toolbar_add(gwin);
		return;
	}

	/* Below should be SetAttr according to Autodocs */
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLIST],
						gwin->win, NULL,
						SPEEDBAR_Buttons, ~0,
						TAG_DONE);

	ami_gui_hotlist_toolbar_free(gwin, &gwin->hotlist_toolbar_list);

	if(ami_gui_hotlist_scan(ami_tree_get_tree(hotlist_window), &gwin->hotlist_toolbar_list, gwin) > 0) {
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLIST],
						gwin->win, NULL,
						SPEEDBAR_Buttons, &gwin->hotlist_toolbar_list,
						TAG_DONE);
	} else {
		ami_gui_hotlist_toolbar_remove(gwin);
	}
}

void ami_gui_hotlist_toolbar_update_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			ami_gui_hotlist_toolbar_update(gwin);
		}
	} while(node = nnode);
}

void ami_toggletabbar(struct gui_window_2 *gwin, bool show)
{
	if(ClickTabBase->lib_Version < 53) return;

	if(show)
	{
		struct TagItem attrs[3];

		attrs[0].ti_Tag = CHILD_WeightedWidth;
		attrs[0].ti_Data = 0;
		attrs[1].ti_Tag = CHILD_WeightedHeight;
		attrs[1].ti_Data = 0;
		attrs[2].ti_Tag = TAG_DONE;
		attrs[2].ti_Data = 0;

		gwin->objects[GID_TABS] = ClickTabObject,
					GA_ID, GID_TABS,
					GA_RelVerify, TRUE,
					GA_Underscore, 13, // disable kb shortcuts
					CLICKTAB_Labels, &gwin->tab_list,
					CLICKTAB_LabelTruncate, TRUE,
					CLICKTAB_CloseImage, gwin->objects[GID_CLOSETAB_BM],
					CLICKTAB_FlagImage, gwin->objects[GID_TABS_FLAG],
					ClickTabEnd;

		gwin->objects[GID_ADDTAB] = ButtonObject,
					GA_ID, GID_ADDTAB,
					GA_RelVerify, TRUE,
					GA_HintInfo, gwin->helphints[GID_ADDTAB],
					GA_Text, "+",
					BUTTON_RenderImage, gwin->objects[GID_ADDTAB_BM],
					ButtonEnd;

		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_ADDCHILD,
				gwin->win, gwin->objects[GID_TABS], NULL);

		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_ADDCHILD,
				gwin->win, gwin->objects[GID_ADDTAB], attrs);
	}
	else
	{
		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_REMOVECHILD,
				gwin->win, gwin->objects[GID_TABS]);

		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_REMOVECHILD,
				gwin->win, gwin->objects[GID_ADDTAB]);

		/* NB: We are NULLing these, but not disposing them as
		 * that causes an Intuition deadlock (TODO) */
		gwin->objects[GID_TABS] = NULL;
		gwin->objects[GID_ADDTAB] = NULL;
	}

	FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
			gwin->win, NULL, TRUE);

	if(gwin->bw) {
		gwin->bw->reformat_pending = true;
		ami_schedule_redraw(gwin, true);
	}
}

void ami_gui_tabs_toggle_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			if(gwin->tabs == 1) {
				if(nsoption_bool(tab_always_show) == true) {
					ami_toggletabbar(gwin, true);
				} else {
					ami_toggletabbar(gwin, false);
				}
			}
		}
	} while(node = nnode);
}

nserror ami_gui_new_blank_tab(struct gui_window_2 *gwin)
{
	nsurl *url;
	nserror error;
	struct browser_window *bw = NULL;

	error = nsurl_create(nsoption_charp(homepage_url), &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY |
					      BW_CREATE_TAB,
					      url,
					      NULL,
					      gwin->bw,
					      &bw);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
		return error;
	}

	return NSERROR_OK;
}

static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *g = NULL;
	ULONG curx=nsoption_int(window_x),cury=nsoption_int(window_y),curw=nsoption_int(window_width),curh=nsoption_int(window_height);
	char nav_west[100],nav_west_s[100],nav_west_g[100];
	char nav_east[100],nav_east_s[100],nav_east_g[100];
	char stop[100],stop_s[100],stop_g[100];
	char reload[100],reload_s[100],reload_g[100];
	char home[100],home_s[100],home_g[100];
	char closetab[100],closetab_s[100],closetab_g[100];
	char addtab[100],addtab_s[100],addtab_g[100];
	char fave[100], unfave[100];
	char tabthrobber[100];
	ULONG refresh_mode = WA_SmartRefresh;
	ULONG idcmp_sizeverify = IDCMP_SIZEVERIFY;

	if (!scrn) ami_openscreenfirst();

	if (nsoption_bool(kiosk_mode)) flags &= ~GW_CREATE_TAB;
	if (nsoption_bool(resize_with_contents)) idcmp_sizeverify = 0;
	bw->scale = 1.0;

	if(existing)
	{
		curx=existing->shared->win->LeftEdge;
		cury=existing->shared->win->TopEdge;
		curw=existing->shared->win->Width;
		curh=existing->shared->win->Height;
	}

	g = AllocVecTags(sizeof(struct gui_window), AVT_ClearWithValue, 0, TAG_DONE);

	if(!g)
	{
		warn_user("NoMemory","");
		return NULL;
	}

	NewList(&g->dllist);
	g->deferred_rects = NewObjList();

	if((flags & GW_CREATE_TAB) && existing)
	{
		g->shared = existing->shared;
		g->tab = g->shared->next_tab;

		if((g->shared->tabs == 1) && (nsoption_bool(tab_always_show) == false))
			ami_toggletabbar(g->shared, true);

		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
						g->shared->win, NULL,
						CLICKTAB_Labels, ~0,
						TAG_DONE);

		g->tab_node = AllocClickTabNode(TNA_Text,messages_get("NetSurf"),
								TNA_Number,g->tab,
								TNA_UserData,bw,
								TNA_CloseGadget, TRUE,
								TAG_DONE);

		if(nsoption_bool(new_tab_last))
		{
			AddTail(&g->shared->tab_list, g->tab_node);
		}
		else
		{
			struct Node *insert_after = existing->tab_node;

			if(existing->last_new_tab)
				insert_after = existing->last_new_tab;
			Insert(&g->shared->tab_list, g->tab_node, insert_after);
			existing->last_new_tab = g->tab_node;
		}

		RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
							g->shared->win, NULL,
							CLICKTAB_Labels, &g->shared->tab_list,
							TAG_DONE);

		if(nsoption_bool(new_tab_is_active))
		{
			RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],g->shared->win,NULL,
							CLICKTAB_Current,g->tab,
							TAG_DONE);
		}

		if(ClickTabBase->lib_Version < 53)
			RethinkLayout((struct Gadget *)g->shared->objects[GID_TABLAYOUT],g->shared->win,NULL,TRUE);

		g->shared->tabs++;
		g->shared->next_tab++;

		if(nsoption_bool(new_tab_is_active)) ami_switch_tab(g->shared,false);

		ami_update_buttons(g->shared);

		return g;
	}

	g->shared = AllocVecTags(sizeof(struct gui_window_2), AVT_ClearWithValue, 0, TAG_DONE);

	if(!g->shared)
	{
		warn_user("NoMemory","");
		return NULL;
	}

	NewMinList(&g->shared->shared_pens);
	
	g->shared->scrollerhook.h_Entry = (void *)ami_scroller_hook;
	g->shared->scrollerhook.h_Data = g->shared;

	g->shared->search_ico_hook.h_Entry = (void *)ami_set_search_ico_render_hook;
	g->shared->search_ico_hook.h_Data = g->shared;

	g->shared->favicon_hook.h_Entry = (void *)ami_set_favicon_render_hook;
	g->shared->favicon_hook.h_Data = g->shared;

	g->shared->throbber_hook.h_Entry = (void *)ami_set_throbber_render_hook;
	g->shared->throbber_hook.h_Data = g->shared;

	newprefs_hook.h_Entry = (void *)ami_gui_newprefs_hook;
	newprefs_hook.h_Data = 0;

	if(nsoption_bool(window_simple_refresh) == true) {
		refresh_mode = WA_SimpleRefresh;
	} else {
		refresh_mode = WA_SmartRefresh;
	}

	if(!nsoption_bool(kiosk_mode))
	{
		ULONG addtabclosegadget = TAG_IGNORE;
		ULONG iconifygadget = FALSE;

		if (nsoption_charp(pubscreen_name) && 
		    (locked_screen == TRUE) &&
		    (strcmp(nsoption_charp(pubscreen_name), "Workbench") == 0))
				iconifygadget = TRUE;
		ami_create_menu(g->shared);

		NewList(&g->shared->tab_list);
		g->tab_node = AllocClickTabNode(TNA_Text,messages_get("NetSurf"),
											TNA_Number, 0,
											TNA_UserData, bw,
											TNA_CloseGadget, TRUE,
											TAG_DONE);
		AddTail(&g->shared->tab_list,g->tab_node);

		g->shared->tabs=1;
		g->shared->next_tab=1;

		g->shared->svbuffer = AllocVecTags(2000, AVT_ClearWithValue, 0, TAG_DONE);

		g->shared->helphints[GID_BACK] =
			translate_escape_chars(messages_get("HelpToolbarBack"));
		g->shared->helphints[GID_FORWARD] =
			translate_escape_chars(messages_get("HelpToolbarForward"));
		g->shared->helphints[GID_STOP] =
			translate_escape_chars(messages_get("HelpToolbarStop"));
		g->shared->helphints[GID_RELOAD] =
			translate_escape_chars(messages_get("HelpToolbarReload"));
		g->shared->helphints[GID_HOME] =
			translate_escape_chars(messages_get("HelpToolbarHome"));
		g->shared->helphints[GID_URL] =
			translate_escape_chars(messages_get("HelpToolbarURL"));
		g->shared->helphints[GID_SEARCHSTRING] =
			translate_escape_chars(messages_get("HelpToolbarWebSearch"));
		g->shared->helphints[GID_ADDTAB] =
			translate_escape_chars(messages_get("HelpToolbarAddTab"));

		ami_get_theme_filename(nav_west, "theme_nav_west", false);
		ami_get_theme_filename(nav_west_s, "theme_nav_west_s", false);
		ami_get_theme_filename(nav_west_g, "theme_nav_west_g", false);
		ami_get_theme_filename(nav_east, "theme_nav_east", false);
		ami_get_theme_filename(nav_east_s, "theme_nav_east_s", false);
		ami_get_theme_filename(nav_east_g, "theme_nav_east_g", false);
		ami_get_theme_filename(stop, "theme_stop", false);
		ami_get_theme_filename(stop_s, "theme_stop_s", false);
		ami_get_theme_filename(stop_g, "theme_stop_g", false);
		ami_get_theme_filename(reload, "theme_reload", false);
		ami_get_theme_filename(reload_s, "theme_reload_s", false);
		ami_get_theme_filename(reload_g, "theme_reload_g", false);
		ami_get_theme_filename(home, "theme_home", false);
		ami_get_theme_filename(home_s, "theme_home_s", false);
		ami_get_theme_filename(home_g, "theme_home_g", false);
		ami_get_theme_filename(closetab, "theme_closetab", false);
		ami_get_theme_filename(closetab_s, "theme_closetab_s", false);
		ami_get_theme_filename(closetab_g, "theme_closetab_g", false);
		ami_get_theme_filename(addtab, "theme_addtab", false);
		ami_get_theme_filename(addtab_s, "theme_addtab_s", false);
		ami_get_theme_filename(addtab_g, "theme_addtab_g", false);
		ami_get_theme_filename(tabthrobber, "theme_tab_loading", false);
		ami_get_theme_filename(fave, "theme_fave", false);
		ami_get_theme_filename(unfave, "theme_unfave", false);

		g->shared->objects[GID_FAVE_ADD] = BitMapObject,
					BITMAP_SourceFile, fave,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_FAVE_RMV] = BitMapObject,
					BITMAP_SourceFile, unfave,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_ADDTAB_BM] = BitMapObject,
					BITMAP_SourceFile, addtab,
					BITMAP_SelectSourceFile, addtab_s,
					BITMAP_DisabledSourceFile, addtab_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_CLOSETAB_BM] = BitMapObject,
					BITMAP_SourceFile, closetab,
					BITMAP_SelectSourceFile, closetab_s,
					BITMAP_DisabledSourceFile, closetab_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		if(ClickTabBase->lib_Version < 53)
		{
			addtabclosegadget = LAYOUT_AddChild;
			g->shared->objects[GID_CLOSETAB] = ButtonObject,
					GA_ID, GID_CLOSETAB,
					GA_RelVerify, TRUE,
					BUTTON_RenderImage, g->shared->objects[GID_CLOSETAB_BM],
					ButtonEnd;

			g->shared->objects[GID_TABS] = ClickTabObject,
					GA_ID,GID_TABS,
					GA_RelVerify,TRUE,
					GA_Underscore,13, // disable kb shortcuts
					CLICKTAB_Labels,&g->shared->tab_list,
					CLICKTAB_LabelTruncate,TRUE,
					ClickTabEnd;

			g->shared->objects[GID_ADDTAB] = ButtonObject,
					GA_ID, GID_ADDTAB,
					GA_RelVerify, TRUE,
					GA_Text, "+",
					BUTTON_RenderImage, g->shared->objects[GID_ADDTAB_BM],
					ButtonEnd;
		}
		else
		{
			g->shared->objects[GID_TABS_FLAG] = BitMapObject,
					BITMAP_SourceFile, tabthrobber,
					BITMAP_Screen,scrn,
					BITMAP_Masking,TRUE,
					BitMapEnd;
		}

		g->shared->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle,nsscreentitle,
       	   	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
       	   	WA_CloseGadget, TRUE,
           	WA_SizeGadget, TRUE,
			WA_Top,cury,
			WA_Left,curx,
			WA_Width,curw,
			WA_Height,curh,
			WA_PubScreen,scrn,
			WA_ReportMouse,TRUE,
			refresh_mode, TRUE,
			WA_SizeBBottom, TRUE,
       	   	WA_IDCMP, IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
						IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
						IDCMP_RAWKEY | idcmp_sizeverify |
						IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
						IDCMP_REFRESHWINDOW |
						IDCMP_ACTIVEWINDOW | IDCMP_EXTENDEDMOUSE,
			WINDOW_IconifyGadget, iconifygadget,
			WINDOW_NewMenu, g->shared->menu,
			WINDOW_MenuUserData, WGUD_HOOK,
			WINDOW_VertProp, 1,
			WINDOW_NewPrefsHook, &newprefs_hook,
			WINDOW_IDCMPHook, &g->shared->scrollerhook,
			WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_REFRESHWINDOW |
						IDCMP_EXTENDEDMOUSE | IDCMP_SIZEVERIFY,
			WINDOW_SharedPort, sport,
			WINDOW_BuiltInScroll, TRUE,
			WINDOW_GadgetHelp, TRUE,
			WINDOW_UserData, g->shared,
           	WINDOW_ParentGroup, g->shared->objects[GID_MAIN] = VGroupObject,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, g->shared->objects[GID_TOOLBARLAYOUT] = HGroupObject,
					LAYOUT_VertAlignment, LALIGN_CENTER,
					LAYOUT_AddChild, g->shared->objects[GID_BACK] = ButtonObject,
						GA_ID,GID_BACK,
						GA_RelVerify,TRUE,
						GA_Disabled,TRUE,
						GA_HintInfo, g->shared->helphints[GID_BACK],
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,nav_west,
							BITMAP_SelectSourceFile,nav_west_s,
							BITMAP_DisabledSourceFile,nav_west_g,
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, g->shared->objects[GID_FORWARD] = ButtonObject,
						GA_ID,GID_FORWARD,
						GA_RelVerify,TRUE,
						GA_Disabled,TRUE,
						GA_HintInfo, g->shared->helphints[GID_FORWARD],
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,nav_east,
							BITMAP_SelectSourceFile,nav_east_s,
							BITMAP_DisabledSourceFile,nav_east_g,
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, g->shared->objects[GID_STOP] = ButtonObject,
						GA_ID,GID_STOP,
						GA_RelVerify,TRUE,
						GA_HintInfo, g->shared->helphints[GID_STOP],
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,stop,
							BITMAP_SelectSourceFile,stop_s,
							BITMAP_DisabledSourceFile,stop_g,
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, g->shared->objects[GID_RELOAD] = ButtonObject,
						GA_ID,GID_RELOAD,
						GA_RelVerify,TRUE,
						GA_HintInfo, g->shared->helphints[GID_RELOAD],
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,reload,
							BITMAP_SelectSourceFile,reload_s,
							BITMAP_DisabledSourceFile,reload_g,
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, g->shared->objects[GID_HOME] = ButtonObject,
						GA_ID,GID_HOME,
						GA_RelVerify,TRUE,
						GA_HintInfo, g->shared->helphints[GID_HOME],
						BUTTON_RenderImage,BitMapObject,
							BITMAP_SourceFile,home,
							BITMAP_SelectSourceFile,home_s,
							BITMAP_DisabledSourceFile,home_g,
							BITMAP_Screen,scrn,
							BITMAP_Masking,TRUE,
						BitMapEnd,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, HGroupObject, // FavIcon, URL bar and hotlist star
						LAYOUT_VertAlignment, LALIGN_CENTER,
						LAYOUT_AddChild, g->shared->objects[GID_ICON] = SpaceObject,
							GA_ID, GID_ICON,
							SPACE_MinWidth, 16,
							SPACE_MinHeight, 16,
							SPACE_Transparent, FALSE,
						//	SPACE_RenderHook, &g->shared->favicon_hook,
						SpaceEnd,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						LAYOUT_AddChild, g->shared->objects[GID_URL] =
							NewObject(urlStringClass, NULL,
									STRINGA_MaxChars, 2000,
									GA_ID, GID_URL,
									GA_RelVerify, TRUE,
									GA_HintInfo, g->shared->helphints[GID_URL],
									GA_TabCycle, TRUE,
									STRINGA_Buffer, g->shared->svbuffer,
									STRINGVIEW_Header, URLHistory_GetList(),
							TAG_DONE),

						LAYOUT_AddChild, g->shared->objects[GID_FAVE] = ButtonObject,
							GA_ID, GID_FAVE,
							GA_RelVerify, TRUE,
						//	GA_HintInfo, g->shared->helphints[GID_FAVE],
							BUTTON_RenderImage, g->shared->objects[GID_FAVE_ADD],
						ButtonEnd,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
					LayoutEnd,
				//	GA_ID, GID_TOOLBARLAYOUT,
				//	GA_RelVerify, TRUE,
				//	LAYOUT_RelVerify, TRUE,
					LAYOUT_WeightBar, TRUE,
					LAYOUT_AddChild, HGroupObject,
						LAYOUT_VertAlignment, LALIGN_CENTER,
						LAYOUT_AddChild, g->shared->objects[GID_SEARCH_ICON] = SpaceObject,
							GA_ID, GID_SEARCH_ICON,
							SPACE_MinWidth, 16,
							SPACE_MinHeight, 16,
							SPACE_Transparent, FALSE,
							SPACE_RenderHook, &g->shared->search_ico_hook,
						SpaceEnd,
						CHILD_WeightedWidth,0,
						CHILD_WeightedHeight,0,
						LAYOUT_AddChild, g->shared->objects[GID_SEARCHSTRING] =StringObject,
							GA_ID,GID_SEARCHSTRING,
                 					STRINGA_TextVal, NULL,
							GA_RelVerify,TRUE,
							GA_HintInfo, g->shared->helphints[GID_SEARCHSTRING],
						StringEnd,
					LayoutEnd,
					CHILD_WeightedWidth, nsoption_int(web_search_width),
					LAYOUT_AddChild, g->shared->objects[GID_THROBBER] = SpaceObject,
						GA_ID,GID_THROBBER,
						SPACE_MinWidth,throbber_width,
						SPACE_MinHeight,throbber_height,
						SPACE_Transparent,TRUE,
					//	SPACE_RenderHook, &g->shared->throbber_hook,
					SpaceEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
				LayoutEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddImage, BevelObject,
					BEVEL_Style, BVS_SBAR_VERT,
				BevelEnd,
				CHILD_WeightedHeight, 0,
				LAYOUT_AddChild, g->shared->objects[GID_HOTLISTLAYOUT] = VGroupObject,
					LAYOUT_SpaceInner, FALSE,
				LayoutEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, g->shared->objects[GID_TABLAYOUT] = HGroupObject,
					LAYOUT_SpaceInner,FALSE,
					addtabclosegadget, g->shared->objects[GID_CLOSETAB],
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,

					addtabclosegadget, g->shared->objects[GID_TABS],
					CHILD_CacheDomain,FALSE,

					addtabclosegadget, g->shared->objects[GID_ADDTAB],
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
				LayoutEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, g->shared->objects[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
					SPACE_Transparent,TRUE,
				SpaceEnd,
			EndGroup,
		EndWindow;
	}
	else
	{
		/* borderless kiosk mode window */
		g->tab = 0;
		g->shared->tabs = 0;
		g->tab_node = NULL;

		g->shared->objects[OID_MAIN] = WindowObject,
       	    WA_ScreenTitle,nsscreentitle,
           	WA_Activate, TRUE,
           	WA_DepthGadget, FALSE,
       	   	WA_DragBar, FALSE,
           	WA_CloseGadget, FALSE,
			WA_Borderless,TRUE,
			WA_RMBTrap,TRUE,
			WA_Top,0,
			WA_Left,0,
			WA_Width, scrn->Width,
			WA_Height, scrn->Height,
           	WA_SizeGadget, FALSE,
			WA_PubScreen, scrn,
			WA_ReportMouse, TRUE,
			refresh_mode, TRUE,
       	   	WA_IDCMP, IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
					IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
					IDCMP_RAWKEY | IDCMP_REFRESHWINDOW |
					IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
					IDCMP_EXTENDEDMOUSE,
			WINDOW_HorizProp,1,
			WINDOW_VertProp,1,
			WINDOW_IDCMPHook,&g->shared->scrollerhook,
			WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE |
					IDCMP_EXTENDEDMOUSE | IDCMP_REFRESHWINDOW,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,g->shared,
			WINDOW_BuiltInScroll,TRUE,
           	WINDOW_ParentGroup, g->shared->objects[GID_MAIN] = VGroupObject,
               	LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, g->shared->objects[GID_BROWSER] = SpaceObject,
					GA_ID,GID_BROWSER,
					SPACE_Transparent,TRUE,
				SpaceEnd,
			EndGroup,
		EndWindow;
	}

	g->shared->win = (struct Window *)RA_OpenWindow(g->shared->objects[OID_MAIN]);

	if(!g->shared->win)
	{
		warn_user("NoMemory","");
		FreeVec(g->shared);
		FreeVec(g);
		return NULL;
	}

	GetAttr(WINDOW_VertObject, g->shared->objects[OID_MAIN],
			(ULONG *)&g->shared->objects[OID_VSCROLL]);

	RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_VSCROLL],
			g->shared->win, NULL,
			GA_ID, OID_VSCROLL,
			ICA_TARGET, ICTARGET_IDCMP,
			TAG_DONE);

	if(nsoption_bool(kiosk_mode) == false)
	{
		ULONG sz, size1, size2;
		struct DrawInfo *dri = GetScreenDrawInfo(scrn);
		
		sz = ami_get_border_gadget_balance(g->shared,
				(ULONG *)&size1, (ULONG *)&size2);

		g->shared->objects[GID_HSCROLL] = NewObject(
				NULL,
				"scrollergclass",
				GA_ID, GID_HSCROLL,
				PGA_Freedom, FREEHORIZ,
				GA_RelRight, 1 - size2 - sz,
				GA_Width, size2,
				GA_BottomBorder, TRUE,
				GA_Immediate, TRUE,
				ICA_TARGET, ICTARGET_IDCMP,
				GA_DrawInfo, dri,
				TAG_DONE);

		GetAttr(GA_Height, (Object *)g->shared->objects[GID_HSCROLL],
				(ULONG *)&sz);

		g->shared->objects[GID_STATUS] = NewObject(
				NULL,
				"frbuttonclass",
				GA_ID, GID_STATUS,
				GA_Left, scrn->WBorLeft + 2,
				GA_RelBottom, -((2 + sz + scrn->WBorBottom - scrn->RastPort.TxHeight)/2),
				GA_Width, size1,
				GA_DrawInfo, dri,
				GA_BottomBorder, TRUE,
				GA_ReadOnly, TRUE,
				GA_Image, (struct Image *)NewObject(
					NULL,
					"gaugeiclass",
					IA_Top, 2 - (scrn->RastPort.TxHeight),
					IA_Left, -4,
					IA_Height, 4 + scrn->RastPort.TxHeight, 
					IA_InBorder, TRUE,
					IA_Label, NULL,
					IA_Screen, scrn,
					GAUGEIA_Level, 0,
					TAG_DONE),
				GA_Next, g->shared->objects[GID_HSCROLL],
				TAG_DONE);

		AddGList(g->shared->win, (struct Gadget *)g->shared->objects[GID_STATUS],
				(UWORD)~0, -1, NULL);

	/* Apparently you can't set GA_Width on creation time for frbuttonclass */

		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_STATUS],
			g->shared->win, NULL,
			GA_Width, size1,
			TAG_DONE);

		RefreshGadgets((APTR)g->shared->objects[GID_STATUS],
				g->shared->win, NULL);
				
		FreeScreenDrawInfo(scrn, dri);
				
		ami_gui_hotlist_toolbar_add(g->shared); /* is this the right place for this? */
		if(nsoption_bool(tab_always_show)) ami_toggletabbar(g->shared, true);
	}
	else
	{
		GetAttr(WINDOW_HorizObject, g->shared->objects[OID_MAIN],
				(ULONG *)&g->shared->objects[OID_HSCROLL]);

		RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_HSCROLL],
				g->shared->win, NULL,
				GA_ID, OID_HSCROLL,
				ICA_TARGET, ICTARGET_IDCMP,
				TAG_DONE);
	}

	g->shared->rmbtrapped = FALSE;
	g->shared->bw = bw;
	curbw = bw;

	g->shared->appwin = AddAppWindowA((ULONG)g->shared->objects[OID_MAIN],
							(ULONG)g->shared, g->shared->win, appport, NULL);

	g->shared->node = AddObject(window_list,AMINS_WINDOW);
	g->shared->node->objstruct = g->shared;

	glob = &browserglob;

	if(locked_screen) UnlockPubScreen(NULL,scrn);
	search_web_retrieve_ico(false);

	ScreenToFront(scrn);

	return g;
}

ULONG ami_set_border_gadget_balance(struct gui_window_2 *gwin)
{
	/* Reset gadget widths according to new calculation */
	ULONG size1, size2, sz;

	sz = ami_get_border_gadget_balance(gwin, &size1, &size2);

	RefreshSetGadgetAttrs((struct Gadget *)(APTR)gwin->objects[GID_HSCROLL],
			gwin->win, NULL,
			GA_RelRight, - size2 - sz,
			GA_Width, size2,
			TAG_DONE);

	RefreshSetGadgetAttrs((struct Gadget *)(APTR)gwin->objects[GID_STATUS],
			gwin->win, NULL,
			GA_Width, size1,
			TAG_DONE);

	RefreshWindowFrame(gwin->win);
}

ULONG ami_get_border_gadget_balance(struct gui_window_2 *gwin, ULONG *size1, ULONG *size2)
{
	/* Get the sizes that border gadget 1 (status) and 2 (hscroller) need to be.
	** Returns the width of the vertical scroller (right-hand window border) as
	** a convenience.
	*/

	ULONG sz;
	ULONG available_width;
	float gad1percent;

	GetAttr(GA_Width, gwin->objects[OID_VSCROLL], (ULONG *)&sz);

	available_width = gwin->win->Width - scrn->WBorLeft - sz;

	gad1percent = nsoption_int(toolbar_status_size) / 10000.0;

	*size1 = (ULONG)(available_width * gad1percent);
	*size2 = (ULONG)(available_width * (1 - gad1percent));

	return sz;
}

void ami_close_all_tabs(struct gui_window_2 *gwin)
{
	struct Node *tab;
	struct Node *ntab;
	
	if((gwin->tabs > 1) && (nsoption_bool(tab_close_warn) == true)) {
		char *req_body = ami_utf8_easy(messages_get("MultiTabClose"));
		int32 res = ami_warn_user_multi(req_body, "Yes", "No", gwin->win);
		free(req_body);
		
		if(res == 0) return;
	}
	
	if(gwin->tabs)
	{
		tab = GetHead(&gwin->tab_list);

		do
		{
			ntab=GetSucc(tab);
			GetClickTabNodeAttrs(tab,
								TNA_UserData,&gwin->bw,
								TAG_DONE);
			browser_window_destroy(gwin->bw);
		} while(tab=ntab);
	}
	else
	{
			browser_window_destroy(gwin->bw);
	}
}

static void gui_window_destroy(struct gui_window *g)
{
	struct Node *ptab;
	ULONG ptabnum = 0;
	int gid;

	if(!g) return;

	if(g->shared->searchwin && (g->shared->searchwin->gwin == g))
	{
		ami_search_close();
		win_destroyed = true;
	}

	if(g->hw)
	{
		ami_history_close(g->hw);
		win_destroyed = true;
	}

	ami_free_download_list(&g->dllist);
	FreeObjList(g->deferred_rects);

	curbw = NULL;

	if(g->shared->tabs > 1)
	{
		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,~0,
						TAG_DONE);

		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
				(ULONG *)&ptabnum);

		if(ptabnum == g->tab)
		{
			ptab = GetSucc(g->tab_node);
			if(!ptab) ptab = GetPred(g->tab_node);

			GetClickTabNodeAttrs(ptab,TNA_Number,(ULONG *)&ptabnum,TAG_DONE);
		}

		Remove(g->tab_node);
		FreeClickTabNode(g->tab_node);
		RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],g->shared->win,NULL,
						CLICKTAB_Labels,&g->shared->tab_list,
						CLICKTAB_Current,ptabnum,
						TAG_DONE);

		if(ClickTabBase->lib_Version < 53)
			RethinkLayout((struct Gadget *)g->shared->objects[GID_TABLAYOUT],g->shared->win,NULL,TRUE);

		g->shared->tabs--;
		ami_switch_tab(g->shared,true);

		if((g->shared->tabs == 1) && (nsoption_bool(tab_always_show) == false))
			ami_toggletabbar(g->shared, false);

		ami_utf8_free(g->tabtitle);

		FreeVec(g);
		return;
	}

	ami_plot_release_pens(&g->shared->shared_pens);
	ami_schedule_redraw_remove(g->shared);

	DisposeObject(g->shared->objects[OID_MAIN]);
	ami_gui_appicon_remove(g->shared);
	if(g->shared->appwin) RemoveAppWindow(g->shared->appwin);

	ami_gui_hotlist_toolbar_free(g->shared, &g->shared->hotlist_toolbar_list);

	/* These aren't freed by the above.
	 * TODO: nav_west etc need freeing too? */
	DisposeObject(g->shared->objects[GID_ADDTAB_BM]);
	DisposeObject(g->shared->objects[GID_CLOSETAB_BM]);
	DisposeObject(g->shared->objects[GID_TABS_FLAG]);
	DisposeObject(g->shared->objects[GID_FAVE_ADD]);
	DisposeObject(g->shared->objects[GID_FAVE_RMV]);

	ami_free_menulabs(g->shared);
	free(g->shared->wintitle);
	ami_utf8_free(g->shared->status);
	FreeVec(g->shared->svbuffer);

	for(gid = 0; gid < GID_LAST; gid++)
		free(g->shared->helphints[gid]);

	DelObject(g->shared->node);
	if(g->tab_node)
	{
		Remove(g->tab_node);
		FreeClickTabNode(g->tab_node);
	}
	FreeVec(g); // g->shared should be freed by DelObject()

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		ami_try_quit();
	}

	win_destroyed = true;
}

static void gui_window_set_title(struct gui_window *g, const char *title)
{
	struct Node *node;
	ULONG cur_tab = 0;
	char *utf8title;

	if(!g) return;
	if(!title) return;

	utf8title = ami_utf8_easy((char *)title);

	if(g->tab_node && (g->shared->tabs > 1))
	{
		node = g->tab_node;

		if((g->tabtitle == NULL) || (strcmp(utf8title, g->tabtitle)))
		{
			SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
							g->shared->win, NULL,
							CLICKTAB_Labels, ~0,
							TAG_DONE);

			SetClickTabNodeAttrs(node, TNA_Text, utf8title,
									TNA_HintInfo, utf8title,
									TAG_DONE);

			RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
								g->shared->win, NULL,
								CLICKTAB_Labels, &g->shared->tab_list,
								TAG_DONE);

			if(g->tabtitle) ami_utf8_free(g->tabtitle);
			g->tabtitle = utf8title;

			if(ClickTabBase->lib_Version < 53)
				RethinkLayout((struct Gadget *)g->shared->objects[GID_TABLAYOUT],
					g->shared->win, NULL, TRUE);
		}

		GetAttr(CLICKTAB_Current, g->shared->objects[GID_TABS],
				(ULONG *)&cur_tab);
	}

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		if((g->shared->wintitle == NULL) || (strcmp(utf8title, g->shared->wintitle)))
		{
			if(g->shared->wintitle) free(g->shared->wintitle);
			g->shared->wintitle = strdup(utf8title);
			SetWindowTitles(g->shared->win, g->shared->wintitle, nsscreentitle);
		}
	}
}

static void ami_redraw_callback(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;

	if(gwin->redraw_required || gwin->bw->reformat_pending) {
		ami_do_redraw(gwin);
	}

	ami_gui_window_update_box_deferred(gwin->bw->window, true);

	if(gwin->bw->window->c_h)
	{
		gui_window_place_caret(gwin->bw->window, gwin->bw->window->c_x,
		gwin->bw->window->c_y, gwin->bw->window->c_h, NULL);
	}

	gwin->redraw_scheduled = false;
}

/**
 * Schedule a redraw of the browser window - Amiga-specific function
 *
 * \param  gwin         a struct gui_window_2
 * \param  full_redraw  set to true to schedule a full redraw,
                        should only be set to false when called from gui_window_update_box()
 */
void ami_schedule_redraw(struct gui_window_2 *gwin, bool full_redraw)
{
	int ms = 0;

	if(full_redraw) gwin->redraw_required = true;
	if(gwin->redraw_scheduled == true) return;

	if(gwin->bw->reformat_pending) ms = nsoption_int(reformat_delay) * 10;
	ami_schedule(ms, ami_redraw_callback, gwin);
	gwin->redraw_scheduled = true;
}

static void ami_schedule_redraw_remove(struct gui_window_2 *gwin)
{
	ami_schedule(-1, ami_redraw_callback, gwin);
}

static void ami_do_redraw_tiled(struct gui_window_2 *gwin, bool busy,
	int left, int top, int width, int height,
	int sx, int sy, struct IBox *bbox, struct redraw_context *ctx)
{
	int x, y;
	struct rect clip;
	int tile_x_scale = (int)(nsoption_int(redraw_tile_size_x) / gwin->bw->scale);
	int tile_y_scale = (int)(nsoption_int(redraw_tile_size_y) / gwin->bw->scale);
				
	browserglob.shared_pens = &gwin->shared_pens;
	
	if(top < 0) {
		height += top;
		top = 0;
	}

	if(left < 0) {
		width += left;
		left = 0;
	}

	if(top < sy) {
		height += (top - sy);
		top = sy;
	}
	if(left < sx) {
		width += (left - sx);
		left = sx;
	}

	if(((top - sy) + height) > bbox->Height)
		height = bbox->Height - (top - sy);

	if(((left - sx) + width) > bbox->Width)
		width = bbox->Width - (left - sx);

	if(width <= 0) return;
	if(height <= 0) return;

	if(busy) ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	for(y = top; y < (top + height); y += tile_y_scale) {
		clip.y0 = 0;
		clip.y1 = nsoption_int(redraw_tile_size_y);
		if(clip.y1 > height) clip.y1 = height;
		if((((y - sy) * gwin->bw->scale) + clip.y1) > bbox->Height)
			clip.y1 = bbox->Height - ((y - sy) * gwin->bw->scale);

		for(x = left; x < (left + width); x += tile_x_scale) {
			clip.x0 = 0;
			clip.x1 = nsoption_int(redraw_tile_size_x);
			if(clip.x1 > width) clip.x1 = width;
			if((((x - sx) * gwin->bw->scale) + clip.x1) > bbox->Width)
				clip.x1 = bbox->Width - ((x - sx) * gwin->bw->scale);

			if(browser_window_redraw(gwin->bw,
				clip.x0 - (int)x,
				clip.y0 - (int)y,
				&clip, ctx))
			{
				ami_clearclipreg(&browserglob);

				BltBitMapTags(BLITA_SrcType, BLITT_BITMAP, 
					BLITA_Source, browserglob.bm,
					BLITA_SrcX, 0,
					BLITA_SrcY, 0,
					BLITA_DestType, BLITT_RASTPORT, 
					BLITA_Dest, gwin->win->RPort,
					BLITA_DestX, bbox->Left + (int)((x - sx) * gwin->bw->scale),
					BLITA_DestY, bbox->Top + (int)((y - sy) * gwin->bw->scale),
					BLITA_Width, (int)(clip.x1),
					BLITA_Height, (int)(clip.y1),
					TAG_DONE);
			}
		}
	}
	
	if(busy) ami_reset_pointer(gwin);
}


/**
 * Redraw an area of the browser window - Amiga-specific function
 *
 * \param  g   a struct gui_window 
 * \param  bw  a struct browser_window
 * \param  x0  top-left co-ordinate (in document co-ordinates)
 * \param  y0  top-left co-ordinate (in document co-ordinates)
 * \param  x1  bottom-right co-ordinate (in document co-ordinates)
 * \param  y1  bottom-right co-ordinate (in document co-ordinates)
 */

static void ami_do_redraw_limits(struct gui_window *g, struct browser_window *bw, bool busy,
		int x0, int y0, int x1, int y1)
{
	ULONG xoffset,yoffset,width=800,height=600;
	ULONG htemp,vtemp;
	struct IBox *bbox;
	ULONG cur_tab = 0;
	ULONG sx, sy;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &amiplot
	};

	if(!g) return;
	if(browser_window_redraw_ready(bw) == false) return;

	sx = g->scrollx;
	sy = g->scrolly;

	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
				g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if(!((cur_tab == g->tab) || (g->shared->tabs <= 1)))
		return;

	GetAttr(SPACE_AreaBox, g->shared->objects[GID_BROWSER], (ULONG *)&bbox);

	ami_do_redraw_tiled(g->shared, busy, x0, y0,
		(x1 - x0) * bw->scale, (y1 - y0) * bw->scale, sx, sy, bbox, &ctx);
	return;
}

static void gui_window_redraw_window(struct gui_window *g)
{
	ULONG cur_tab = 0;

	if(!g) return;

	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
				g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
		ami_schedule_redraw(g->shared, true);
}

static void ami_gui_window_update_box_deferred(struct gui_window *g, bool draw)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct rect *rect;
	
	if(!g) return;
	if(IsMinListEmpty(g->deferred_rects)) return;

	if(draw == true) {
		ami_set_pointer(g->shared, GUI_POINTER_WAIT, false);
	} else {
		LOG(("Ignoring deferred box redraw queue"));
	}

	node = (struct nsObject *)GetHead((struct List *)g->deferred_rects);

	do {
		if(draw == true) {
			rect = (struct rect *)node->objstruct;
			ami_do_redraw_limits(g, g->shared->bw, false,
				rect->x0, rect->y0, rect->x1, rect->y1);
		}
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		DelObject(node);
	} while(node = nnode);

	if(draw == true) ami_reset_pointer(g->shared);
}

static bool ami_gui_window_update_box_deferred_check(struct MinList *deferred_rects,
				const struct rect *new_rect)
{
struct nsObject *node;
struct nsObject *nnode;
	struct rect *rect;
	
	if(IsMinListEmpty(deferred_rects)) return true;

	node = (struct nsObject *)GetHead((struct List *)deferred_rects);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		rect = (struct rect *)node->objstruct;
		
		if((rect->x0 <= new_rect->x0) &&
			(rect->y0 <= new_rect->y0) &&
			(rect->x1 >= new_rect->x1) &&
			(rect->y1 >= new_rect->y1)) {
			return false;
		}
		
		if ((new_rect->x0 <= rect->x0) &&
			(new_rect->y0 <= rect->y0) &&
			(new_rect->x1 >= rect->x1) &&
			(new_rect->y1 >= rect->y1)) {
			LOG(("Removing queued redraw that is a subset of new box redraw"));
			DelObject(node);
			/* Don't return - we might find more */
		}
	} while(node = nnode);

	return true;
}

static void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	struct nsObject *nsobj;
	struct rect *deferred_rect;
	if(!g) return;
	
	if(ami_gui_window_update_box_deferred_check(g->deferred_rects, rect)) {
		deferred_rect = AllocVecTagList(sizeof(struct rect), NULL);
		CopyMem(rect, deferred_rect, sizeof(struct rect));
		nsobj = AddObject(g->deferred_rects, AMINS_RECT);
		nsobj->objstruct = deferred_rect;
	} else {
		LOG(("Ignoring duplicate or subset of queued box redraw"));
	}
	ami_schedule_redraw(g->shared, false);
}

static void ami_do_redraw(struct gui_window_2 *gwin)
{
	struct Rectangle rect;
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600,x0=0,y0=0;
	struct IBox *bbox;
	ULONG oldh = gwin->oldh, oldv=gwin->oldv;
	struct RastPort *temprp;

	if(browser_window_redraw_ready(gwin->bw) == false) return;

	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER], (ULONG *)&bbox);
	ami_get_hscroll_pos(gwin, (ULONG *)&hcurrent);
	ami_get_vscroll_pos(gwin, (ULONG *)&vcurrent);

	gwin->bw->window->scrollx = hcurrent;
	gwin->bw->window->scrolly = vcurrent;

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

	if(gwin->bw->reformat_pending)
	{
		browser_window_reformat(gwin->bw, false, width, height);
		gwin->bw->reformat_pending = false;
		gwin->redraw_scroll = false;
	}

	if(gwin->redraw_scroll)
	{
		if((abs(vcurrent-oldv) > height) ||	(abs(hcurrent-oldh) > width))
			gwin->redraw_scroll = false;

 		if(gwin->new_content) gwin->redraw_scroll = false;
//		if(gwin->bw->scale != 1.0) gwin->redraw_scroll = false;
	}

	if(gwin->redraw_scroll)
	{
		struct rect rect;
		
		gwin->bw->window->c_h_temp = gwin->bw->window->c_h;
		gui_window_remove_caret(gwin->bw->window);

		ScrollWindowRaster(gwin->win, hcurrent - oldh, vcurrent - oldv,
				xoffset, yoffset, xoffset + width - 1, yoffset + height - 1);

		gwin->bw->window->c_h = gwin->bw->window->c_h_temp;

		if(vcurrent>oldv) /* Going down */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, height - (vcurrent - oldv) - 1);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, height + 1);
			gui_window_update_box(gwin->bw->window, &rect);
		}
		else if(vcurrent<oldv) /* Going up */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, oldv - vcurrent + 1);
			gui_window_update_box(gwin->bw->window, &rect);
		}

		if(hcurrent>oldh) /* Going right */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, width - (hcurrent - oldh), 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, height + 1);
			gui_window_update_box(gwin->bw->window, &rect);
		}
		else if(hcurrent<oldh) /* Going left */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, oldh - hcurrent + 1, height + 1);
			gui_window_update_box(gwin->bw->window, &rect);
		}
	}
	else
	{
		struct rect clip;
		struct redraw_context ctx = {
			.interactive = true,
			.background_images = true,
			.plot = &amiplot
		};

		glob = &browserglob;

		if(nsoption_bool(direct_render) == false)
		{
			ami_do_redraw_tiled(gwin, true, hcurrent, vcurrent, width, height, hcurrent, vcurrent, bbox, &ctx);
		}
		else
		{
			browserglob.shared_pens = &gwin->shared_pens;
			temprp = browserglob.rp;
 			browserglob.rp = gwin->win->RPort;
			clip.x0 = bbox->Left;
			clip.y0 = bbox->Top;
			clip.x1 = bbox->Left + bbox->Width;
			clip.y1 = bbox->Top + bbox->Height;

			ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

			if(browser_window_redraw(gwin->bw, clip.x0 - hcurrent, clip.y0 - vcurrent, &clip, &ctx))
			{
				ami_clearclipreg(&browserglob);
				browserglob.rp = temprp;
			}
			
			ami_reset_pointer(gwin);
		}
		/* Tell NetSurf not to bother with the next queued box redraw, as we've redrawn everything. */
		ami_gui_window_update_box_deferred(gwin->bw->window, false);
	}

	ami_update_buttons(gwin);

	gwin->oldh = hcurrent;
	gwin->oldv = vcurrent;

	gwin->redraw_scroll = false;
	gwin->redraw_required = false;
	gwin->new_content = false;
}

void ami_refresh_window(struct gui_window_2 *gwin)
{
	/* simplerefresh only */

	struct IBox *bbox;
	int x0, x1, y0, y1, sx, sy;
	struct RegionRectangle *regrect;

	sx = gwin->bw->window->scrollx;
	sy = gwin->bw->window->scrolly;

	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER], (ULONG *)&bbox); 
	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);
	
	BeginRefresh(gwin->win);

	x0 = ((gwin->win->RPort->Layer->DamageList->bounds.MinX - bbox->Left) /
			browser_window_get_scale(gwin->bw)) + sx - 1;
	x1 = ((gwin->win->RPort->Layer->DamageList->bounds.MaxX - bbox->Left) /
			browser_window_get_scale(gwin->bw)) + sx + 2;
	y0 = ((gwin->win->RPort->Layer->DamageList->bounds.MinY - bbox->Top) /
			browser_window_get_scale(gwin->bw)) + sy - 1;
	y1 = ((gwin->win->RPort->Layer->DamageList->bounds.MaxY - bbox->Top) /
			browser_window_get_scale(gwin->bw)) + sy + 2;

	regrect = gwin->win->RPort->Layer->DamageList->RegionRectangle;

	ami_do_redraw_limits(gwin->bw->window, gwin->bw, false, x0, y0, x1, y1);

	while(regrect)
	{
		x0 = ((regrect->bounds.MinX - bbox->Left) /
			browser_window_get_scale(gwin->bw)) + sx - 1;
		x1 = ((regrect->bounds.MaxX - bbox->Left) /
			browser_window_get_scale(gwin->bw)) + sx + 2;
		y0 = ((regrect->bounds.MinY - bbox->Top) /
			browser_window_get_scale(gwin->bw)) + sy - 1;
		y1 = ((regrect->bounds.MaxY - bbox->Top) /
			browser_window_get_scale(gwin->bw)) + sy + 2;

		regrect = regrect->Next;

		ami_do_redraw_limits(gwin->bw->window, gwin->bw, false, x0, y0, x1, y1);
	}

	EndRefresh(gwin->win, TRUE);
	
	ami_reset_pointer(gwin);
}

void ami_get_hscroll_pos(struct gui_window_2 *gwin, ULONG *xs)
{
	if(gwin->objects[GID_HSCROLL])
	{
		GetAttr(PGA_Top, (Object *)gwin->objects[GID_HSCROLL], xs);
	}
	else if(gwin->objects[OID_HSCROLL])
	{
		GetAttr(SCROLLER_Top, gwin->objects[OID_HSCROLL], xs);
	}

	*xs /= gwin->bw->scale;
}

void ami_get_vscroll_pos(struct gui_window_2 *gwin, ULONG *ys)
{
	GetAttr(SCROLLER_Top, gwin->objects[OID_VSCROLL], ys);
	*ys /= gwin->bw->scale;
}

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	ami_get_hscroll_pos(g->shared, (ULONG *)sx);
	ami_get_vscroll_pos(g->shared, (ULONG *)sy);
}

static void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	struct IBox *bbox;
	ULONG cur_tab = 0;

	if(!g) return;
	if(!g->shared->bw || !g->shared->bw->current_content) return;

	GetAttr(SPACE_AreaBox, g->shared->objects[GID_BROWSER], (ULONG *)&bbox);

	if(sx < 0) sx=0;
	if(sy < 0) sy=0;

	if(sx >= content_get_width(g->shared->bw->current_content) - bbox->Width)
		sx = content_get_width(g->shared->bw->current_content) - bbox->Width;
	if(sy >= (content_get_height(g->shared->bw->current_content) - bbox->Height))
		sy = content_get_height(g->shared->bw->current_content) - bbox->Height;

	if(content_get_width(g->shared->bw->current_content) <= bbox->Width) sx = 0;
	if(content_get_height(g->shared->bw->current_content) <= bbox->Height) sy = 0;

	if(g->tab_node && (g->shared->tabs > 1))
				GetAttr(CLICKTAB_Current,
					g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_VSCROLL],
			g->shared->win, NULL,
			SCROLLER_Top, (ULONG)(sy * g->shared->bw->scale),
			TAG_DONE);

		if(g->shared->objects[GID_HSCROLL])
		{
			RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_HSCROLL],
				g->shared->win, NULL,
				PGA_Top, (ULONG)(sx * g->shared->bw->scale),
				TAG_DONE);
		}
		else if(g->shared->objects[OID_HSCROLL])
		{
			RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_HSCROLL],
				g->shared->win, NULL,
				SCROLLER_Top, (ULONG)(sx * g->shared->bw->scale),
				TAG_DONE);
		}
		ami_schedule_redraw(g->shared, true);

		if(nsoption_bool(faster_scroll) == true) g->shared->redraw_scroll = true;
			else g->shared->redraw_scroll = false;

		g->scrollx = sx;
		g->scrolly = sy;

//		history_set_current_scroll(g->shared->bw->history,g->scrollx,g->scrolly);
	}
//	g->shared->new_content = false;
}

static void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
	struct IBox *bbox;
	if(!g) return;

	GetAttr(SPACE_AreaBox, g->shared->objects[GID_BROWSER], (ULONG *)&bbox);

	*width = bbox->Width;
	*height = bbox->Height;

	if(scaled)
	{
		*width /= g->shared->bw->scale;
		*height /= g->shared->bw->scale;
	}
}

static void gui_window_update_extent(struct gui_window *g)
{
	struct IBox *bbox, zbox;
	ULONG cur_tab = 0;

	if(!g) return;
	if(!g->shared->bw->current_content) return;
	/*
	zbox.Top = ~0;
	zbox.Left = ~0;
	zbox.Width = (WORD)(content_get_width(g->shared->bw->current_content) * g->shared->bw->scale);
	zbox.Height = (WORD)(content_get_height(g->shared->bw->current_content) * g->shared->bw->scale);
	*/
	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
				g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		GetAttr(SPACE_AreaBox, g->shared->objects[GID_BROWSER],
				(ULONG *)&bbox);

		RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_VSCROLL],g->shared->win,NULL,
			SCROLLER_Total, (ULONG)(content_get_height(g->shared->bw->current_content) * g->shared->bw->scale),
			SCROLLER_Visible, bbox->Height,
			TAG_DONE);

		if(g->shared->objects[GID_HSCROLL])
		{
			RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_HSCROLL],
				g->shared->win, NULL,
				PGA_Total, (ULONG)(content_get_width(g->shared->bw->current_content) * g->shared->bw->scale),
				PGA_Visible, bbox->Width,
				TAG_DONE);
		}
		else if(g->shared->objects[OID_HSCROLL])
		{
			RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[OID_HSCROLL],
				g->shared->win, NULL,
				SCROLLER_Total, (ULONG)(content_get_width(g->shared->bw->current_content) * g->shared->bw->scale),
				SCROLLER_Visible, bbox->Width,
				TAG_DONE);
		}
		//SetWindowAttr(g->shared->win, WA_Zoom, &zbox, sizeof(ULONG));
	}
	g->shared->new_content = true;
}

static void gui_window_set_status(struct gui_window *g, const char *text)
{
	ULONG cur_tab = 0;
	char *utf8text;
	ULONG size;
	UWORD chars;
	struct TextExtent textex;

	if(!g) return;
	if(!text) return;
	if(!g->shared->objects[GID_STATUS]) return;

	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
			g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		utf8text = ami_utf8_easy((char *)text);
		if(utf8text == NULL) return;

		GetAttr(GA_Width, g->shared->objects[GID_STATUS], (ULONG *)&size);
		chars = TextFit(&scrn->RastPort, utf8text, strlen(utf8text),
					&textex, NULL, 1, size - 4, scrn->RastPort.TxHeight);

		utf8text[chars] = 0;

		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_STATUS],
			g->shared->win, NULL,
			GA_Text, utf8text,
			TAG_DONE);

		RefreshGList((struct Gadget *)g->shared->objects[GID_STATUS],
				g->shared->win, NULL, 1);

		if(g->shared->status) ami_utf8_free(g->shared->status);
		g->shared->status = utf8text;
	}
}

static void gui_window_set_url(struct gui_window *g, const char *url)
{
	ULONG cur_tab = 0;

	if(!g) return;
	if(!url) return;

	if(g->tab_node && (g->shared->tabs > 1)) GetAttr(CLICKTAB_Current,
				g->shared->objects[GID_TABS], (ULONG *)&cur_tab);

	if((cur_tab == g->tab) || (g->shared->tabs <= 1))
	{
		RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_URL],
			g->shared->win, NULL, STRINGA_TextVal, url, TAG_DONE);
	}

	ami_update_buttons(g->shared);
}


static uint32 ami_set_favicon_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg)
{
	refresh_favicon = TRUE;
	return 0;
}

/**
 * set gui display of a retrieved favicon representing the search
 * provider
 * \param ico may be NULL for local calls; then access current cache from
 * search_web_ico()
 */
static void gui_set_search_ico(hlcache_handle *ico)
{
	struct BitMap *bm = NULL;
	struct IBox *bbox;
	ULONG cur_tab = 0;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;
	struct bitmap *ico_bitmap;

	if(IsMinListEmpty(window_list))	return;
	if(nsoption_bool(kiosk_mode) == true) return;

	if (ico == NULL) ico = search_web_ico();
	ico_bitmap = content_get_bitmap(ico);
	if ((ico != NULL) && (ico_bitmap != NULL))
		bm = ami_bitmap_get_native(ico_bitmap, 16, 16, NULL);

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			GetAttr(SPACE_AreaBox, gwin->objects[GID_SEARCH_ICON], (ULONG *)&bbox);

			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_SEARCH_ICON],
				gwin->win, NULL,
				GA_HintInfo, search_web_provider_name(),
				TAG_DONE);

			EraseRect(gwin->win->RPort, bbox->Left, bbox->Top,
				bbox->Left+16, bbox->Top+16);

			if(bm)
			{
				ULONG tag, tag_data, minterm;
				
				if(ami_plot_screen_is_palettemapped() == false) {
					tag = BLITA_UseSrcAlpha;
					tag_data = !ico_bitmap->opaque;
					minterm = 0xc0;
				} else {
					tag = BLITA_MaskPlane;
					tag_data = (ULONG)ami_bitmap_get_mask(ico_bitmap, 16, 16, bm);
					minterm = (ABC|ABNC|ANBC);
				}

				BltBitMapTags(BLITA_SrcX, 0,
							BLITA_SrcY, 0,
							BLITA_DestX, bbox->Left,
							BLITA_DestY, bbox->Top,
							BLITA_Width, 16,
							BLITA_Height, 16,
							BLITA_Source, bm,
							BLITA_Dest, gwin->win->RPort,
							BLITA_SrcType, BLITT_BITMAP,
							BLITA_DestType, BLITT_RASTPORT,
							BLITA_Minterm, minterm,
							tag, tag_data,
							TAG_DONE);
			}
		}
	} while(node = nnode);
}

static uint32 ami_set_search_ico_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg)
{
	refresh_search_ico = TRUE;
	return 0;
}

static uint32 ami_set_throbber_render_hook(struct Hook *hook, APTR space,
	struct gpRender *msg)
{
	refresh_throbber = TRUE;
	return 0;
}

static void gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
	struct IBox *bbox;
	ULONG xs,ys;

	if(!g) return;

	gui_window_remove_caret(g);

	GetAttr(SPACE_AreaBox,g->shared->objects[GID_BROWSER],(ULONG *)&bbox);
	xs = g->scrollx;
	ys = g->scrolly;

	SetAPen(g->shared->win->RPort,3);

	if((y-ys+height) > (bbox->Height)) height = bbox->Height-y+ys;

	if(((x-xs) <= 0) || ((x-xs+2) >= (bbox->Width)) || ((y-ys) <= 0) || ((y-ys) >= (bbox->Height))) return;

	g->c_w = 2;

	SetDrMd(g->shared->win->RPort,COMPLEMENT);

	RectFill(g->shared->win->RPort, x + bbox->Left - xs, y + bbox->Top - ys,
		x + bbox->Left + g->c_w - xs, y+bbox->Top + height - ys);

	SetDrMd(g->shared->win->RPort,JAM1);

	g->c_x = x;
	g->c_y = y;
	g->c_h = height;

	if((nsoption_bool(kiosk_mode) == false))
		OnMenu(g->shared->win, AMI_MENU_PASTE);
}

static void gui_window_remove_caret(struct gui_window *g)
{
	if(!g) return;
	if(g->c_h == 0) return;

	if((nsoption_bool(kiosk_mode) == false))
		OffMenu(g->shared->win, AMI_MENU_PASTE);

	ami_do_redraw_limits(g, g->shared->bw, false, g->c_x, g->c_y,
		g->c_x + g->c_w + 1, g->c_y + g->c_h + 1);

	g->c_h = 0;
}

static void gui_window_new_content(struct gui_window *g)
{
	hlcache_handle *c;

	if(g && g->shared && g->shared->bw && g->shared->bw->current_content)
		c = g->shared->bw->current_content;
	else return;

	ami_clearclipreg(&browserglob);
	g->shared->new_content = true;
	g->scrollx = 0;
	g->scrolly = 0;
	g->shared->oldh = 0;
	g->shared->oldv = 0;
	g->favicon = NULL;
	ami_plot_release_pens(&g->shared->shared_pens);
	ami_menu_update_disabled(g, c);
	ami_gui_update_hotlist_button(g->shared);
}

static bool gui_window_drag_start(struct gui_window *g, gui_drag_type type,
		const struct rect *rect)
{
	g->shared->drag_op = type;
	if(rect) g->shared->ptr_lock = ami_ns_rect_to_ibox(g->shared, rect);

	if(type == GDRAGGING_NONE)
	{
		SetWindowAttrs(g->shared->win, WA_GrabFocus, 0,
			WA_MouseLimits, NULL, TAG_DONE);

		if(g->shared->ptr_lock)
		{
			FreeVec(g->shared->ptr_lock);
			g->shared->ptr_lock = NULL;
		}
	}

	return true;
}

void ami_scroller_hook(struct Hook *hook,Object *object,struct IntuiMessage *msg) 
{
	ULONG gid;
	struct gui_window_2 *gwin = hook->h_Data;
	struct IntuiWheelData *wheel;
	struct Node *node = NULL;
	nsurl *url;

	switch(msg->Class)
	{
		case IDCMP_IDCMPUPDATE:
			gid = GetTagData( GA_ID, 0, msg->IAddress );

			switch( gid ) 
			{
				case GID_HSCROLL:
 				case OID_HSCROLL: 
 				case OID_VSCROLL:
					if(nsoption_bool(faster_scroll) == true) gwin->redraw_scroll = true;
						else gwin->redraw_scroll = false;

					ami_schedule_redraw(gwin, true);
 				break;
				
				case GID_HOTLIST:
					if(node = (struct Node *)GetTagData(SPEEDBAR_SelectedNode, 0, msg->IAddress)) {
						GetSpeedButtonNodeAttrs(node, SBNA_UserData, (ULONG *)&url, TAG_DONE);

						if(gwin->key_state & BROWSER_MOUSE_MOD_2) {
							browser_window_create(BW_CREATE_TAB,
										      url,
										      NULL,
										      gwin->bw,
										      NULL);
						} else {
							browser_window_navigate(gwin->bw,
									url,
									NULL,
									BW_NAVIGATE_HISTORY,
									NULL,
									NULL,
									NULL);

						}
					}
				break;
			} 
		break;

		case IDCMP_EXTENDEDMOUSE:
			if(msg->Code == IMSGCODE_INTUIWHEELDATA)
			{
				wheel = (struct IntuiWheelData *)msg->IAddress;

				ami_gui_scroll_internal(gwin, wheel->WheelX * 50, wheel->WheelY * 50);
			}
		break;

		case IDCMP_SIZEVERIFY:
		break;

		case IDCMP_REFRESHWINDOW:
			ami_refresh_window(gwin);
		break;

		default:
			printf("UNHANDLED EVENT %ld\n",msg->Class);
		break;
	}
//	ReplyMsg((struct Message *)msg);
} 

/* return the text box at posn x,y in window coordinates
   x,y are updated to be document co-ordinates */

bool ami_text_box_at_point(struct gui_window_2 *gwin, ULONG *x, ULONG *y)
{
	struct IBox *bbox;
	ULONG xs, ys, width, height;
	struct contextual_content data;

	GetAttr(SPACE_AreaBox, (Object *)gwin->objects[GID_BROWSER],
				(ULONG *)&bbox);

	ami_get_hscroll_pos(gwin, (ULONG *)&xs);
	*x = *x - (bbox->Left) +xs;

	ami_get_vscroll_pos(gwin, (ULONG *)&ys);
	*y = *y - (bbox->Top) + ys;

	width=bbox->Width;
	height=bbox->Height;

	browser_window_get_contextual_content(gwin->bw, *x, *y, &data);

	if (data.form_features == CTX_FORM_TEXT)
		return true;

	return false;
}

BOOL ami_gadget_hit(Object *obj, int x, int y)
{
	int top, left, width, height;

	GetAttrs(obj,
		GA_Left, &left,
		GA_Top, &top,
		GA_Width, &width,
		GA_Height, &height,
		TAG_DONE);

	if((x >= left) && (x <= (left + width)) && (y >= top) && (y <= (top + height)))
		return TRUE;
	else return FALSE;
}

Object *ami_gui_splash_open(void)
{
	Object *win_obj, *bm_obj;
	struct Window *win;
	struct Screen *wbscreen = LockPubScreen("Workbench");
	uint32 top = 0, left = 0;
	STRPTR ver_string;
	struct TextAttr tattr;
	struct TextFont *tfont;

	win_obj = WindowObject,
				WA_Borderless, TRUE,
				WA_ToolBox, TRUE,
				WA_BusyPointer, TRUE,
				WINDOW_Position, WPOS_CENTERSCREEN,
				WINDOW_LockWidth, TRUE,
				WINDOW_LockHeight, TRUE,
				WINDOW_ParentGroup, LayoutObject,
					LAYOUT_AddImage, bm_obj = BitMapObject,
						BITMAP_SourceFile, "PROGDIR:Resources/splash.png",
						BITMAP_Screen, wbscreen,
					BitMapEnd,
				LayoutEnd,
			EndWindow;

	win = RA_OpenWindow(win_obj);

	GetAttrs(bm_obj, IA_Top, &top,
				IA_Left, &left,
				TAG_DONE);

	SetRPAttrs(win->RPort, RPTAG_APenColor, 0x003F6DFE, TAG_DONE);
	SetDrMd(win->RPort, JAM1);

	tattr.ta_Name = "DejaVu Serif Italic.font";
	tattr.ta_YSize = 24;
	tattr.ta_Style = 0;
	tattr.ta_Flags = 0;

	if(tfont = ami_font_open_disk_font(&tattr))
	{
		SetFont(win->RPort, tfont);
	}
	else
	{
		tattr.ta_Name = "DejaVu Serif Oblique.font";
		if(tfont = ami_font_open_disk_font(&tattr))
			SetFont(win->RPort, tfont);
	}

	Move(win->RPort, left + 5, top + 25);
	Text(win->RPort, "Initialising...", strlen("Initialising..."));

	if(tfont) ami_font_close_disk_font(tfont);

	tattr.ta_Name = "DejaVu Sans.font";
	tattr.ta_YSize = 16;
	tattr.ta_Style = 0;
	tattr.ta_Flags = 0;

	if(tfont = ami_font_open_disk_font(&tattr))
		SetFont(win->RPort, tfont);

	ver_string = ASPrintf("%s", netsurf_version);

	Move(win->RPort, left + 185, top + 220);
	Text(win->RPort, ver_string, strlen(ver_string));

	if(ver_string) FreeVec(ver_string);
	if(tfont) ami_font_close_disk_font(tfont);

	UnlockPubScreen(NULL, wbscreen);

	return win_obj;
}

void ami_gui_splash_close(Object *win_obj)
{
	if(win_obj) DisposeObject(win_obj);
}

static void gui_file_gadget_open(struct gui_window *g, hlcache_handle *hl, 
	struct form_control *gadget)
{
	LOG(("File open dialog rquest for %p/%p", g, gadget));

	if(AslRequestTags(filereq,
			ASLFR_Window, g->shared->win,
			ASLFR_SleepWindow, TRUE,
			ASLFR_TitleText, messages_get("NetSurf"),
			ASLFR_Screen, scrn,
			ASLFR_DoSaveMode, FALSE,
			TAG_DONE)) {
		char fname[1024];
		strlcpy(fname, filereq->fr_Drawer, 1024);
		AddPart(fname, filereq->fr_File, 1024);
		browser_window_set_gadget_filename(g->shared->bw, gadget, fname);
	}
}

static struct gui_window_table amiga_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.redraw = gui_window_redraw_window,
	.update = gui_window_update_box,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.update_extent = gui_window_update_extent,

	.set_icon = gui_window_set_icon,
	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.place_caret = gui_window_place_caret,
	.remove_caret = gui_window_remove_caret,
	.drag_start = gui_window_drag_start,
	.new_content = gui_window_new_content,
	.file_gadget_open = gui_file_gadget_open,
	.drag_save_object = gui_drag_save_object,
	.drag_save_selection =gui_drag_save_selection,
	.start_selection = gui_start_selection,

	/* from theme */
	.set_pointer = gui_window_set_pointer,
	.start_throbber = gui_window_start_throbber,
	.stop_throbber = gui_window_stop_throbber,

	/* from download */
	.save_link = gui_window_save_link,
};

static struct gui_fetch_table amiga_fetch_table = {
	.filename_from_path = filename_from_path,
	.path_add_part = path_add_part,
	.filetype = fetch_filetype,
	.path_to_url = path_to_url,
	.url_to_path = url_to_path,

	.get_resource_url = gui_get_resource_url,
};

static struct gui_browser_table amiga_browser_table = {
	.poll = gui_poll,
	.schedule = ami_schedule,

	.quit = gui_quit,
	.set_search_ico = gui_set_search_ico,
	.launch_url = gui_launch_url,
	.create_form_select_menu = gui_create_form_select_menu,
	.cert_verify = gui_cert_verify,
	.login = gui_401login_open,
};

/** Normal entry point from OS */
int main(int argc, char** argv)
{
	setbuf(stderr, NULL);
	char messages[100];
	char script[1024];
	char temp[1024];
	BPTR lock = 0;
	int32 user = 0;
	nserror ret;
	Object *splash_window = ami_gui_splash_open();
	struct gui_table amiga_gui_table = {
		.browser = &amiga_browser_table,
		.window = &amiga_window_table,
		.clipboard = amiga_clipboard_table,
		.download = amiga_download_table,
		.fetch = &amiga_fetch_table,
		.utf8 = amiga_utf8_table,
		.search = amiga_search_table,
	};

	/* Open popupmenu.library just to check the version.
	 * Versions older than 53.11 are dangerous, so we
	 * forcibly disable context menus if these are in use.
	 */
	popupmenu_lib_ok = FALSE;
	if(PopupMenuBase = OpenLibrary("popupmenu.library", 53)) {
		LOG(("popupmenu.library v%d.%d",
			PopupMenuBase->lib_Version, PopupMenuBase->lib_Revision));
		if(LIB_IS_AT_LEAST((struct Library *)PopupMenuBase, 53, 11))
			popupmenu_lib_ok = TRUE;
		CloseLibrary(PopupMenuBase);
	}

	user = GetVar("user", temp, 1024, GVF_GLOBAL_ONLY);
	current_user = ASPrintf("%s", (user == -1) ? "Default" : temp);
	current_user_dir = ASPrintf("PROGDIR:Users/%s", current_user);

	if(lock = CreateDirTree(current_user_dir))
		UnLock(lock);

	current_user_options = ASPrintf("%s/Choices", current_user_dir);

	ami_mime_init("PROGDIR:Resources/mimetypes");
	sprintf(temp, "%s/mimetypes.user", current_user_dir);
	ami_mime_init(temp);
	ami_schedule_open_timer();
	ami_schedule_create();

	amiga_plugin_hack_init();
	ret = amiga_datatypes_init();

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(NULL, &argc, argv);

	/* user options setup */
	ret = nsoption_init(ami_set_options, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	nsoption_read(current_user_options, NULL);
	ami_gui_commandline(&argc, argv); /* calls nsoption_commandline */

	if (ami_locate_resource(messages, "Messages") == false)
		die("Cannot open Messages file");
	ret = netsurf_init(messages, &amiga_gui_table);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	ret = amiga_icon_init();

	gui_init(argc, argv);
	gui_init2(argc, argv);

	ami_gui_splash_close(splash_window);

	strlcpy(script, nsoption_charp(arexx_dir), 1024);
	AddPart(script, nsoption_charp(arexx_startup), 1024);
	ami_arexx_execute(script);

	netsurf_main_loop();

	strlcpy(script, nsoption_charp(arexx_dir), 1024);
	AddPart(script, nsoption_charp(arexx_shutdown), 1024);
	ami_arexx_execute(script);

	netsurf_exit();

	ami_mime_free();

	return 0;
}
