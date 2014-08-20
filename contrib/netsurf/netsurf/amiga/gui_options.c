/*
 * Copyright 2009 - 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdbool.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#ifdef __amigaos4__
#include <proto/application.h>
#endif
#include <libraries/gadtools.h>
#include <exec/types.h>
#include <intuition/classusr.h>
#include <graphics/gfxbase.h>

#include "amiga/object.h"
#include "amiga/font.h"
#include "amiga/gui.h"
#include "amiga/gui_options.h"
#include "amiga/help.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"
#include "utils/messages.h"
#include "desktop/browser_private.h"
#include "utils/nsoption.h"
#include "desktop/searchweb.h"

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <proto/clicktab.h>
#include <proto/label.h>
#include <proto/string.h>
#include <proto/checkbox.h>
#include <proto/radiobutton.h>
#include <proto/getscreenmode.h>
#include <proto/getfile.h>
#include <proto/chooser.h>
#include <proto/integer.h>
#include <proto/getfont.h>
#include <classes/window.h>
#include <images/label.h>
#include <gadgets/button.h>
#include <gadgets/clicktab.h>
#include <gadgets/string.h>
#include <gadgets/checkbox.h>
#include <gadgets/radiobutton.h>
#include <gadgets/getscreenmode.h>
#include <gadgets/getfile.h>
#include <gadgets/chooser.h>
#include <gadgets/integer.h>
#include <gadgets/getfont.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

enum
{
	GID_OPTS_MAIN = GID_MAIN,
	GID_OPTS_HOMEPAGE,
	GID_OPTS_HOMEPAGE_DEFAULT,
	GID_OPTS_HOMEPAGE_CURRENT,
	GID_OPTS_HOMEPAGE_BLANK,
	GID_OPTS_HIDEADS,
	GID_OPTS_CONTENTLANG,
	GID_OPTS_FROMLOCALE,
	GID_OPTS_HISTORY,
	GID_OPTS_JAVASCRIPT,
	GID_OPTS_REFERRAL,
	GID_OPTS_DONOTTRACK,
	GID_OPTS_FASTSCROLL,
	GID_OPTS_SCREEN,
	GID_OPTS_SCREENMODE,
	GID_OPTS_SCREENNAME,
	GID_OPTS_WIN_SIMPLE,
	GID_OPTS_THEME,
	GID_OPTS_PTRTRUE,
	GID_OPTS_PTROS,
	GID_OPTS_PROXY,
	GID_OPTS_PROXY_HOST,
	GID_OPTS_PROXY_PORT,
	GID_OPTS_PROXY_USER,
	GID_OPTS_PROXY_PASS,
	GID_OPTS_PROXY_BYPASS,
	GID_OPTS_FETCHMAX,
	GID_OPTS_FETCHHOST,
	GID_OPTS_FETCHCACHE,
	GID_OPTS_NATIVEBM,
	GID_OPTS_SCALEQ,
	GID_OPTS_DITHERQ,
	GID_OPTS_ANIMSPEED,
	GID_OPTS_ANIMDISABLE,
	GID_OPTS_DPI_Y,
	GID_OPTS_FONT_SANS,
	GID_OPTS_FONT_SERIF,
	GID_OPTS_FONT_MONO,
	GID_OPTS_FONT_CURSIVE,
	GID_OPTS_FONT_FANTASY,
	GID_OPTS_FONT_DEFAULT,
	GID_OPTS_FONT_SIZE,
	GID_OPTS_FONT_MINSIZE,
	GID_OPTS_FONT_ANTIALIASING,
	GID_OPTS_CACHE_MEM,
	GID_OPTS_CACHE_DISC,
	GID_OPTS_OVERWRITE,
	GID_OPTS_NOTIFY,
	GID_OPTS_DLDIR,
	GID_OPTS_TAB_ACTIVE,
	GID_OPTS_TAB_2,
	GID_OPTS_TAB_LAST,
	GID_OPTS_TAB_ALWAYS,
	GID_OPTS_TAB_CLOSE,
	GID_OPTS_SEARCH_PROV,
	GID_OPTS_CLIPBOARD,
	GID_OPTS_CONTEXTMENU,
	GID_OPTS_STARTUP_NO_WIN,
	GID_OPTS_CLOSE_NO_QUIT,
	GID_OPTS_DOCKY,
	GID_OPTS_MARGIN_TOP,
	GID_OPTS_MARGIN_LEFT,
	GID_OPTS_MARGIN_BOTTOM,
	GID_OPTS_MARGIN_RIGHT,
	GID_OPTS_EXPORT_SCALE,
	GID_OPTS_EXPORT_NOIMAGES,
	GID_OPTS_EXPORT_NOBKG,
	GID_OPTS_EXPORT_LOOSEN,
	GID_OPTS_EXPORT_COMPRESS,
	GID_OPTS_EXPORT_PASSWORD,
	GID_OPTS_SAVE,
	GID_OPTS_USE,
	GID_OPTS_CANCEL,
	GID_OPTS_LAST
};

enum
{
	GRP_OPTS_HOMEPAGE = GID_OPTS_LAST,
	GRP_OPTS_CONTENTBLOCKING,
	GRP_OPTS_CONTENTLANGUAGE,
	GRP_OPTS_HISTORY,
	GRP_OPTS_SCRIPTING,
	GRP_OPTS_PRIVACY,
	GRP_OPTS_MISC,
	GRP_OPTS_SCREEN,
	GRP_OPTS_WINDOW,
	GRP_OPTS_THEME,
	GRP_OPTS_MOUSE,
	GRP_OPTS_PROXY,
	GRP_OPTS_FETCHING,
	GRP_OPTS_IMAGES,
	GRP_OPTS_ANIMS,
	GRP_OPTS_DPI,
	GRP_OPTS_FONTFACES,
	GRP_OPTS_FONTSIZE,
	GRP_OPTS_MEMCACHE,
	GRP_OPTS_DISCCACHE,
	GRP_OPTS_DOWNLOADS,
	GRP_OPTS_TABS,
	GRP_OPTS_SEARCH,
	GRP_OPTS_CLIPBOARD,
	GRP_OPTS_BEHAVIOUR,
	GRP_OPTS_MARGINS,
	GRP_OPTS_SCALING,
	GRP_OPTS_APPEARANCE,
	GRP_OPTS_ADVANCED,
	GRP_OPTS_LAST
};

enum
{
	LAB_OPTS_WINTITLE = GRP_OPTS_LAST,
	LAB_OPTS_RESTART,
	LAB_OPTS_DAYS,
	LAB_OPTS_SECS,
	LAB_OPTS_PT,
	LAB_OPTS_MB,
	LAB_OPTS_MM,
	LAB_OPTS_DPI,
	LAB_OPTS_LAST
};

#define OPTS_LAST LAB_OPTS_LAST
#define OPTS_MAX_TABS 10
#define OPTS_MAX_SCREEN 4
#define OPTS_MAX_PROXY 5
#define OPTS_MAX_NATIVEBM 4
#define OPTS_MAX_DITHER 4

struct ami_gui_opts_window {
	struct nsObject *node;
	struct Window *win;
	Object *objects[GID_OPTS_LAST];
};

static struct ami_gui_opts_window *gow = NULL;

CONST_STRPTR tabs[OPTS_MAX_TABS];
static STRPTR screenopts[OPTS_MAX_SCREEN];
CONST_STRPTR proxyopts[OPTS_MAX_PROXY];
CONST_STRPTR nativebmopts[OPTS_MAX_NATIVEBM];
CONST_STRPTR ditheropts[OPTS_MAX_DITHER];
CONST_STRPTR fontopts[6];
CONST_STRPTR gadlab[OPTS_LAST];
STRPTR *websearch_list;

STRPTR *ami_gui_opts_websearch(void);
void ami_gui_opts_websearch_free(STRPTR *websearchlist);

void ami_gui_opts_setup(void)
{
	tabs[0] = (char *)ami_utf8_easy((char *)messages_get("con_general"));
	tabs[1] = (char *)ami_utf8_easy((char *)messages_get("Display"));
	tabs[2] = (char *)ami_utf8_easy((char *)messages_get("con_connect"));
	tabs[3] = (char *)ami_utf8_easy((char *)messages_get("con_rendering"));
	tabs[4] = (char *)ami_utf8_easy((char *)messages_get("con_fonts"));
	tabs[5] = (char *)ami_utf8_easy((char *)messages_get("con_cache"));
	tabs[6] = (char *)ami_utf8_easy((char *)messages_get("Tabs"));
	tabs[7] = (char *)ami_utf8_easy((char *)messages_get("con_advanced"));
#ifdef WITH_PDF_EXPORT
	tabs[8] = (char *)ami_utf8_easy((char *)messages_get("Export"));
	tabs[9] = NULL;
#else
	tabs[8] = NULL;
#endif

	screenopts[0] = (char *)ami_utf8_easy((char *)messages_get("ScreenOwn"));
	screenopts[1] = (char *)ami_utf8_easy((char *)messages_get("ScreenWB"));
	screenopts[2] = (char *)ami_utf8_easy((char *)messages_get("ScreenPublic"));
	screenopts[3] = NULL;

	proxyopts[0] = (char *)ami_utf8_easy((char *)messages_get("ProxyNone"));
	proxyopts[1] = (char *)ami_utf8_easy((char *)messages_get("ProxyNoAuth"));
	proxyopts[2] = (char *)ami_utf8_easy((char *)messages_get("ProxyBasic"));
	proxyopts[3] = (char *)ami_utf8_easy((char *)messages_get("ProxyNTLM"));
	proxyopts[4] = NULL;

	nativebmopts[0] = (char *)ami_utf8_easy((char *)messages_get("None"));
	nativebmopts[1] = (char *)ami_utf8_easy((char *)messages_get("Scaled"));
	nativebmopts[2] = (char *)ami_utf8_easy((char *)messages_get("All"));
	nativebmopts[3] = NULL;

	ditheropts[0] = (char *)ami_utf8_easy((char *)messages_get("Low"));
	ditheropts[1] = (char *)ami_utf8_easy((char *)messages_get("Medium"));
	ditheropts[2] = (char *)ami_utf8_easy((char *)messages_get("High"));
	ditheropts[3] = NULL;
	
	websearch_list = ami_gui_opts_websearch();

	gadlab[GID_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("HomePageURL"));
	gadlab[GID_OPTS_HOMEPAGE_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("HomePageDefault"));
	gadlab[GID_OPTS_HOMEPAGE_CURRENT] = (char *)ami_utf8_easy((char *)messages_get("HomePageCurrent"));
	gadlab[GID_OPTS_HOMEPAGE_BLANK] = (char *)ami_utf8_easy((char *)messages_get("HomePageBlank"));
	gadlab[GID_OPTS_HIDEADS] = (char *)ami_utf8_easy((char *)messages_get("BlockAds"));
	gadlab[GID_OPTS_FROMLOCALE] = (char *)ami_utf8_easy((char *)messages_get("LocaleLang"));
	gadlab[GID_OPTS_HISTORY] = (char *)ami_utf8_easy((char *)messages_get("HistoryAge"));
	gadlab[GID_OPTS_JAVASCRIPT] = (char *)ami_utf8_easy((char *)messages_get("EnableJS"));
	gadlab[GID_OPTS_REFERRAL] = (char *)ami_utf8_easy((char *)messages_get("SendReferer"));
	gadlab[GID_OPTS_DONOTTRACK] = (char *)ami_utf8_easy((char *)messages_get("DoNotTrack"));
	gadlab[GID_OPTS_FASTSCROLL] = (char *)ami_utf8_easy((char *)messages_get("FastScrolling"));
	gadlab[GID_OPTS_WIN_SIMPLE] = (char *)ami_utf8_easy((char *)messages_get("SimpleRefresh"));
	gadlab[GID_OPTS_PTRTRUE] = (char *)ami_utf8_easy((char *)messages_get("TrueColour"));
	gadlab[GID_OPTS_PTROS] = (char *)ami_utf8_easy((char *)messages_get("OSPointers"));
	gadlab[GID_OPTS_PROXY] = (char *)ami_utf8_easy((char *)messages_get("ProxyType"));
	gadlab[GID_OPTS_PROXY_HOST] = (char *)ami_utf8_easy((char *)messages_get("Host"));
	gadlab[GID_OPTS_PROXY_USER] = (char *)ami_utf8_easy((char *)messages_get("Username"));
	gadlab[GID_OPTS_PROXY_PASS] = (char *)ami_utf8_easy((char *)messages_get("Password"));
	gadlab[GID_OPTS_PROXY_BYPASS] = (char *)ami_utf8_easy((char *)messages_get("ProxyBypass"));
	gadlab[GID_OPTS_FETCHMAX] = (char *)ami_utf8_easy((char *)messages_get("FetchesMax"));
	gadlab[GID_OPTS_FETCHHOST] = (char *)ami_utf8_easy((char *)messages_get("FetchesHost"));
	gadlab[GID_OPTS_FETCHCACHE] = (char *)ami_utf8_easy((char *)messages_get("FetchesCached"));
	gadlab[GID_OPTS_NATIVEBM] = (char *)ami_utf8_easy((char *)messages_get("CacheNative"));
	gadlab[GID_OPTS_SCALEQ] = (char *)ami_utf8_easy((char *)messages_get("ScaleQuality"));
	gadlab[GID_OPTS_DITHERQ] = (char *)ami_utf8_easy((char *)messages_get("DitherQuality"));
	gadlab[GID_OPTS_ANIMSPEED] = (char *)ami_utf8_easy((char *)messages_get("AnimSpeedLimit"));
	gadlab[GID_OPTS_DPI_Y] = (char *)ami_utf8_easy((char *)messages_get("ResolutionY"));
	gadlab[GID_OPTS_ANIMDISABLE] = (char *)ami_utf8_easy((char *)messages_get("AnimDisable"));
	gadlab[GID_OPTS_FONT_SANS] = (char *)ami_utf8_easy((char *)messages_get("FontSans"));
	gadlab[GID_OPTS_FONT_SERIF] = (char *)ami_utf8_easy((char *)messages_get("FontSerif"));
	gadlab[GID_OPTS_FONT_MONO] = (char *)ami_utf8_easy((char *)messages_get("FontMono"));
	gadlab[GID_OPTS_FONT_CURSIVE] = (char *)ami_utf8_easy((char *)messages_get("FontCursive"));
	gadlab[GID_OPTS_FONT_FANTASY] = (char *)ami_utf8_easy((char *)messages_get("FontFantasy"));
	gadlab[GID_OPTS_FONT_DEFAULT] = (char *)ami_utf8_easy((char *)messages_get("Default"));
	gadlab[GID_OPTS_FONT_SIZE] = (char *)ami_utf8_easy((char *)messages_get("Default"));
	gadlab[GID_OPTS_FONT_MINSIZE] = (char *)ami_utf8_easy((char *)messages_get("Minimum"));
	gadlab[GID_OPTS_FONT_ANTIALIASING] = (char *)ami_utf8_easy((char *)messages_get("FontAntialiasing"));
	gadlab[GID_OPTS_CACHE_MEM] = (char *)ami_utf8_easy((char *)messages_get("Size"));
	gadlab[GID_OPTS_CACHE_DISC] = (char *)ami_utf8_easy((char *)messages_get("Duration"));
	gadlab[GID_OPTS_OVERWRITE] = (char *)ami_utf8_easy((char *)messages_get("ConfirmOverwrite"));
	gadlab[GID_OPTS_NOTIFY] = (char *)ami_utf8_easy((char *)messages_get("DownloadNotify"));
	gadlab[GID_OPTS_DLDIR] = (char *)ami_utf8_easy((char *)messages_get("DownloadDir"));
	gadlab[GID_OPTS_TAB_ACTIVE] = (char *)ami_utf8_easy((char *)messages_get("TabActive"));
	gadlab[GID_OPTS_TAB_2] = (char *)ami_utf8_easy((char *)messages_get("TabMiddle"));
	gadlab[GID_OPTS_TAB_LAST] = (char *)ami_utf8_easy((char *)messages_get("TabLast"));
	gadlab[GID_OPTS_TAB_ALWAYS] = (char *)ami_utf8_easy((char *)messages_get("TabAlways"));
	gadlab[GID_OPTS_TAB_CLOSE] = (char *)ami_utf8_easy((char *)messages_get("TabClose"));
	gadlab[GID_OPTS_SEARCH_PROV] = (char *)ami_utf8_easy((char *)messages_get("SearchProvider"));
	gadlab[GID_OPTS_CLIPBOARD] = (char *)ami_utf8_easy((char *)messages_get("ClipboardUTF8"));
	gadlab[GID_OPTS_CONTEXTMENU] = (char *)ami_utf8_easy((char *)messages_get("ContextMenu"));
	gadlab[GID_OPTS_STARTUP_NO_WIN] = (char *)ami_utf8_easy((char *)messages_get("OptionNoWindow"));
	gadlab[GID_OPTS_CLOSE_NO_QUIT] = (char *)ami_utf8_easy((char *)messages_get("OptionNoQuit"));
	gadlab[GID_OPTS_DOCKY] = (char *)ami_utf8_easy((char *)messages_get("OptionDocky"));
	gadlab[GID_OPTS_MARGIN_TOP] = (char *)ami_utf8_easy((char *)messages_get("Top"));
	gadlab[GID_OPTS_MARGIN_LEFT] = (char *)ami_utf8_easy((char *)messages_get("Left"));
	gadlab[GID_OPTS_MARGIN_RIGHT] = (char *)ami_utf8_easy((char *)messages_get("Right"));
	gadlab[GID_OPTS_MARGIN_BOTTOM] = (char *)ami_utf8_easy((char *)messages_get("Bottom"));
	gadlab[GID_OPTS_EXPORT_SCALE] = (char *)ami_utf8_easy((char *)messages_get("Scale"));
	gadlab[GID_OPTS_EXPORT_NOIMAGES] = (char *)ami_utf8_easy((char *)messages_get("SuppressImages"));
	gadlab[GID_OPTS_EXPORT_NOBKG] = (char *)ami_utf8_easy((char *)messages_get("RemoveBackground"));
	gadlab[GID_OPTS_EXPORT_LOOSEN] = (char *)ami_utf8_easy((char *)messages_get("FitPage"));
	gadlab[GID_OPTS_EXPORT_COMPRESS] = (char *)ami_utf8_easy((char *)messages_get("CompressPDF"));
	gadlab[GID_OPTS_EXPORT_PASSWORD] = (char *)ami_utf8_easy((char *)messages_get("SetPassword"));
	gadlab[GID_OPTS_SAVE] = (char *)ami_utf8_easy((char *)messages_get("SelSave"));
	gadlab[GID_OPTS_USE] = (char *)ami_utf8_easy((char *)messages_get("Use"));
	gadlab[GID_OPTS_CANCEL] = (char *)ami_utf8_easy((char *)messages_get("Cancel"));

	gadlab[LAB_OPTS_WINTITLE] = (char *)ami_utf8_easy((char *)messages_get("Preferences"));
	gadlab[LAB_OPTS_RESTART] = (char *)ami_utf8_easy((char *)messages_get("NeedRestart"));
	gadlab[LAB_OPTS_DAYS] = (char *)ami_utf8_easy((char *)messages_get("Days"));
	gadlab[LAB_OPTS_SECS] = (char *)ami_utf8_easy((char *)messages_get("AnimSpeedFrames"));
	gadlab[LAB_OPTS_PT] = (char *)ami_utf8_easy((char *)messages_get("Pt"));
	gadlab[LAB_OPTS_MM] = (char *)ami_utf8_easy((char *)messages_get("MM"));
	gadlab[LAB_OPTS_MB] = (char *)ami_utf8_easy((char *)messages_get("MBytes"));
	gadlab[LAB_OPTS_DPI] = (char *)ami_utf8_easy((char *)messages_get("DPI"));

	gadlab[GRP_OPTS_HOMEPAGE] = (char *)ami_utf8_easy((char *)messages_get("Home"));
	gadlab[GRP_OPTS_CONTENTBLOCKING] = (char *)ami_utf8_easy((char *)messages_get("ContentBlocking"));
	gadlab[GRP_OPTS_CONTENTLANGUAGE] = (char *)ami_utf8_easy((char *)messages_get("ContentLanguage"));
	gadlab[GRP_OPTS_HISTORY] = (char *)ami_utf8_easy((char *)messages_get("History"));
	gadlab[GRP_OPTS_SCRIPTING] = (char *)ami_utf8_easy((char *)messages_get("Scripting"));
	gadlab[GRP_OPTS_MISC] = (char *)ami_utf8_easy((char *)messages_get("Miscellaneous"));
	gadlab[GRP_OPTS_SCREEN] = (char *)ami_utf8_easy((char *)messages_get("Screen"));
	gadlab[GRP_OPTS_WINDOW] = (char *)ami_utf8_easy((char *)messages_get("Window"));
	gadlab[GRP_OPTS_THEME] = (char *)ami_utf8_easy((char *)messages_get("Theme"));
	gadlab[GRP_OPTS_MOUSE] = (char *)ami_utf8_easy((char *)messages_get("MousePointers"));
	gadlab[GRP_OPTS_PROXY] = (char *)ami_utf8_easy((char *)messages_get("Proxy"));
	gadlab[GRP_OPTS_FETCHING] = (char *)ami_utf8_easy((char *)messages_get("Fetching"));
	gadlab[GRP_OPTS_IMAGES] = (char *)ami_utf8_easy((char *)messages_get("Images"));
	gadlab[GRP_OPTS_ANIMS] = (char *)ami_utf8_easy((char *)messages_get("Animations"));
	gadlab[GRP_OPTS_DPI] = (char *)ami_utf8_easy((char *)messages_get("Resolution"));
	gadlab[GRP_OPTS_FONTFACES] = (char *)ami_utf8_easy((char *)messages_get("FontFamilies"));
	gadlab[GRP_OPTS_FONTSIZE] = (char *)ami_utf8_easy((char *)messages_get("FontSize"));
	gadlab[GRP_OPTS_MEMCACHE] = (char *)ami_utf8_easy((char *)messages_get("CacheMemory"));
	gadlab[GRP_OPTS_DISCCACHE] = (char *)ami_utf8_easy((char *)messages_get("CacheDisc"));
	gadlab[GRP_OPTS_DOWNLOADS] = (char *)ami_utf8_easy((char *)messages_get("Downloads"));
	gadlab[GRP_OPTS_TABS] = (char *)ami_utf8_easy((char *)messages_get("TabbedBrowsing"));
	gadlab[GRP_OPTS_SEARCH] = (char *)ami_utf8_easy((char *)messages_get("SearchWeb"));
	gadlab[GRP_OPTS_CLIPBOARD] = (char *)ami_utf8_easy((char *)messages_get("Clipboard"));
	gadlab[GRP_OPTS_PRIVACY] = (char *)ami_utf8_easy((char *)messages_get("Privacy"));
	gadlab[GRP_OPTS_BEHAVIOUR] = (char *)ami_utf8_easy((char *)messages_get("Behaviour"));
	gadlab[GRP_OPTS_MARGINS] = (char *)ami_utf8_easy((char *)messages_get("Margins"));
	gadlab[GRP_OPTS_SCALING] = (char *)ami_utf8_easy((char *)messages_get("Scaling"));
	gadlab[GRP_OPTS_APPEARANCE] = (char *)ami_utf8_easy((char *)messages_get("Appearance"));
	gadlab[GRP_OPTS_ADVANCED] = (char *)ami_utf8_easy((char *)messages_get("con_advanced"));

	fontopts[0] = gadlab[GID_OPTS_FONT_SANS];
	fontopts[1] = gadlab[GID_OPTS_FONT_SERIF];
	fontopts[2] = gadlab[GID_OPTS_FONT_MONO];
	fontopts[3] = gadlab[GID_OPTS_FONT_CURSIVE];
	fontopts[4] = gadlab[GID_OPTS_FONT_FANTASY];
	fontopts[5] = NULL;
}

void ami_gui_opts_free(void)
{
	int i;

	for(i = 0; i < OPTS_LAST; i++)
		if(gadlab[i]) free((APTR)gadlab[i]);

	for(i = 0; i < OPTS_MAX_TABS; i++)
		if(tabs[i]) free((APTR)tabs[i]);

	for(i = 0; i < OPTS_MAX_SCREEN; i++)
		if(screenopts[i]) free((APTR)screenopts[i]);

	for(i = 0; i < OPTS_MAX_PROXY; i++)
		if(proxyopts[i]) free((APTR)proxyopts[i]);

	for(i = 0; i < OPTS_MAX_NATIVEBM; i++)
		if(nativebmopts[i]) free((APTR)nativebmopts[i]);

	ami_gui_opts_websearch_free(websearch_list);
}

void ami_gui_opts_open(void)
{
	uint16 screenoptsselected;
	ULONG screenmodeid = 0;
	ULONG proxytype = 0;
	BOOL screenmodedisabled = FALSE, screennamedisabled = FALSE;
	BOOL proxyhostdisabled = TRUE, proxyauthdisabled = TRUE, proxybypassdisabled = FALSE;
	BOOL disableanims, animspeeddisabled = FALSE, acceptlangdisabled = FALSE;
	BOOL scaleselected = nsoption_bool(scale_quality), scaledisabled = FALSE;
	BOOL ditherdisable = TRUE;
	BOOL download_notify_disabled = FALSE;
	BOOL ptr_disable = FALSE;
	char animspeed[10];
	char *homepage_url_lc = ami_utf8_easy(nsoption_charp(homepage_url));

	struct TextAttr fontsans, fontserif, fontmono, fontcursive, fontfantasy;

	if(gow && gow->win)
	{
		WindowToFront(gow->win);
		ActivateWindow(gow->win);
		return;
	}

	if(LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 53, 42)) ptr_disable = TRUE;
	
	if(nsoption_charp(pubscreen_name))
	{
		if(strcmp(nsoption_charp(pubscreen_name),"Workbench") == 0)
		{
			screenoptsselected = 1;
			screennamedisabled = TRUE;
			screenmodedisabled = TRUE;
		}
		else
		{
			screenoptsselected = 2;
			screenmodedisabled = TRUE;
		}
	}
	else
	{
		screenoptsselected = 0;
		screennamedisabled = TRUE;
	}

	if((nsoption_charp(screen_modeid)) && 
	   (strncmp(nsoption_charp(screen_modeid),"0x",2) == 0))
	{
		screenmodeid = strtoul(nsoption_charp(screen_modeid),NULL,0);
	}

	if(ami_plot_screen_is_palettemapped() == true)
		ditherdisable = FALSE;

	if(nsoption_bool(http_proxy) == true)
	{
		proxytype = nsoption_int(http_proxy_auth) + 1;
		switch(nsoption_int(http_proxy_auth))
		{
			case OPTION_HTTP_PROXY_AUTH_BASIC:
			case OPTION_HTTP_PROXY_AUTH_NTLM:
				proxyauthdisabled = FALSE;
			case OPTION_HTTP_PROXY_AUTH_NONE:
				proxyhostdisabled = FALSE;
			break;
		}
	} else {
		proxybypassdisabled = TRUE;
	}

	sprintf(animspeed,"%.2f",(float)(nsoption_int(minimum_gif_delay)/100.0));

	if(nsoption_bool(animate_images))
	{
		disableanims = FALSE;
		animspeeddisabled = FALSE;
	}
	else
	{
		disableanims = TRUE;
		animspeeddisabled = TRUE;
	}

	if(nsoption_bool(accept_lang_locale))
		acceptlangdisabled = TRUE;
	else
		acceptlangdisabled = FALSE;

	if(GfxBase->LibNode.lib_Version < 53)
	{
		scaledisabled = TRUE;
		scaleselected = FALSE;
	}

	if(ApplicationBase->lib_Version < 53)
	{
		download_notify_disabled = TRUE;
		nsoption_set_bool(download_notify, FALSE);
	}

	fontsans.ta_Name = ASPrintf("%s.font", nsoption_charp(font_sans));
	fontserif.ta_Name = ASPrintf("%s.font", nsoption_charp(font_serif));
	fontmono.ta_Name = ASPrintf("%s.font", nsoption_charp(font_mono));
	fontcursive.ta_Name = ASPrintf("%s.font", nsoption_charp(font_cursive));
	fontfantasy.ta_Name = ASPrintf("%s.font", nsoption_charp(font_fantasy));

	fontsans.ta_Style = 0;
	fontserif.ta_Style = 0;
	fontmono.ta_Style = 0;
	fontcursive.ta_Style = 0;
	fontfantasy.ta_Style = 0;

	fontsans.ta_YSize = 0;
	fontserif.ta_YSize = 0;
	fontmono.ta_YSize = 0;
	fontcursive.ta_YSize = 0;
	fontfantasy.ta_YSize = 0;

	fontsans.ta_Flags = 0;
	fontserif.ta_Flags = 0;
	fontmono.ta_Flags = 0;
	fontcursive.ta_Flags = 0;
	fontfantasy.ta_Flags = 0;

	if(!gow)
	{
		ami_gui_opts_setup();

		gow = AllocVecTags(sizeof(struct ami_gui_opts_window), AVT_ClearWithValue, 0, TAG_DONE);

		gow->objects[OID_MAIN] = WindowObject,
			WA_ScreenTitle,nsscreentitle,
			WA_Title, gadlab[LAB_OPTS_WINTITLE],
			WA_Activate, TRUE,
			WA_DepthGadget, TRUE,
			WA_DragBar, TRUE,
			WA_CloseGadget, TRUE,
			WA_SizeGadget, FALSE,
			WA_PubScreen,scrn,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,gow,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_Position, WPOS_CENTERSCREEN,
			WA_IDCMP, IDCMP_GADGETUP | IDCMP_CLOSEWINDOW,
			WINDOW_ParentGroup, gow->objects[GID_OPTS_MAIN] = VGroupObject,
				LAYOUT_AddChild, ClickTabObject,
					GA_RelVerify, TRUE,
					GA_Text, tabs,
					CLICKTAB_PageGroup, PageObject,
						/*
						** General
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_HOMEPAGE],
									LAYOUT_AddChild, gow->objects[GID_OPTS_HOMEPAGE] = StringObject,
										GA_ID, GID_OPTS_HOMEPAGE,
										GA_RelVerify, TRUE,
										STRINGA_TextVal, homepage_url_lc,
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_HOMEPAGE],
									LabelEnd,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_AddChild, gow->objects[GID_OPTS_HOMEPAGE_DEFAULT] = ButtonObject,
											GA_ID,GID_OPTS_HOMEPAGE_DEFAULT,
											GA_Text,gadlab[GID_OPTS_HOMEPAGE_DEFAULT],
											GA_RelVerify,TRUE,
										ButtonEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_HOMEPAGE_CURRENT] = ButtonObject,
											GA_ID,GID_OPTS_HOMEPAGE_CURRENT,
											GA_Text,gadlab[GID_OPTS_HOMEPAGE_CURRENT],
											GA_RelVerify,TRUE,
										ButtonEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_HOMEPAGE_BLANK] = ButtonObject,
											GA_ID,GID_OPTS_HOMEPAGE_BLANK,
											GA_Text,gadlab[GID_OPTS_HOMEPAGE_BLANK],
											GA_RelVerify,TRUE,
										ButtonEnd,
									LayoutEnd,
								LayoutEnd, //homepage
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,HGroupObject,
									LAYOUT_AddChild, VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_CONTENTBLOCKING],
		                				LAYOUT_AddChild, gow->objects[GID_OPTS_HIDEADS] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_HIDEADS,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_HIDEADS],
         	           						GA_Selected, nsoption_bool(block_advertisements),
            	    					CheckBoxEnd,
									LayoutEnd, // content blocking
									LAYOUT_AddChild,VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_CONTENTLANGUAGE],
										LAYOUT_AddChild, gow->objects[GID_OPTS_CONTENTLANG] = StringObject,
											GA_ID, GID_OPTS_CONTENTLANG,
											GA_RelVerify, TRUE,
											GA_Disabled, acceptlangdisabled,
											STRINGA_TextVal, nsoption_charp(accept_language),
											STRINGA_BufferPos,0,
										StringEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_FROMLOCALE] = CheckBoxObject,
											GA_ID, GID_OPTS_FROMLOCALE,
											GA_Text, gadlab[GID_OPTS_FROMLOCALE],
											GA_RelVerify, TRUE,
											GA_Selected, nsoption_bool(accept_lang_locale),
										ButtonEnd,
									//	CHILD_WeightedWidth, 0,
									LayoutEnd, // content language
								LayoutEnd, // content
								LAYOUT_AddChild, HGroupObject,
									LAYOUT_AddChild, VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_HISTORY],
										LAYOUT_AddChild, HGroupObject,
											LAYOUT_LabelColumn, PLACETEXT_RIGHT,
											LAYOUT_AddChild, gow->objects[GID_OPTS_HISTORY] = IntegerObject,
												GA_ID, GID_OPTS_HISTORY,
												GA_RelVerify, TRUE,
												INTEGER_Number, nsoption_int(expire_url),
												INTEGER_Minimum, 0,
												INTEGER_Maximum, 366,
												INTEGER_Arrows, TRUE,
											IntegerEnd,
											CHILD_WeightedWidth, 0,
											CHILD_Label, LabelObject,
												LABEL_Text, gadlab[LAB_OPTS_DAYS],
											LabelEnd,
										LayoutEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[GID_OPTS_HISTORY],
										LabelEnd,
									LayoutEnd, // history
#if defined(WITH_JS) || defined(WITH_MOZJS)
									LAYOUT_AddChild, VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_SCRIPTING],
		                				LAYOUT_AddChild, gow->objects[GID_OPTS_JAVASCRIPT] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_JAVASCRIPT,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_JAVASCRIPT],
         	           						GA_Selected, nsoption_bool(enable_javascript),
            	    					CheckBoxEnd,
									LayoutEnd, // scripting
#endif
								LayoutEnd,
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_PRIVACY],
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_REFERRAL] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_REFERRAL,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_REFERRAL],
         	           					GA_Selected, nsoption_bool(send_referer),
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_DONOTTRACK] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_DONOTTRACK,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_DONOTTRACK],
         	           					GA_Selected, nsoption_bool(do_not_track),
            	    				CheckBoxEnd,
								LayoutEnd, // misc
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // pageadd
						/*
						** Display
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_SCREEN],
									LAYOUT_AddChild, HGroupObject,
			                			LAYOUT_AddChild, gow->objects[GID_OPTS_SCREEN] = RadioButtonObject,
    	  	              					GA_ID, GID_OPTS_SCREEN,
        	 	           					GA_RelVerify, TRUE,
         		           					GA_Text, screenopts,
  					      		            RADIOBUTTON_Selected, screenoptsselected,
            	    					RadioButtonEnd,
										CHILD_WeightedWidth,0,
										LAYOUT_AddChild,VGroupObject,
			                				LAYOUT_AddChild, gow->objects[GID_OPTS_SCREENMODE] = GetScreenModeObject,
    	  	              						GA_ID, GID_OPTS_SCREENMODE,
        	 	           						GA_RelVerify, TRUE,
												GA_Disabled,screenmodedisabled,
												GETSCREENMODE_DisplayID,screenmodeid,
												GETSCREENMODE_MinDepth, 0,
												GETSCREENMODE_MaxDepth, 32,
											GetScreenModeEnd,
											LAYOUT_AddChild, gow->objects[GID_OPTS_SCREENNAME] = StringObject,
												GA_ID, GID_OPTS_SCREENNAME,
												GA_RelVerify, TRUE,
												GA_Disabled,screennamedisabled,
												STRINGA_TextVal, nsoption_charp(pubscreen_name),
												STRINGA_BufferPos,0,
											StringEnd,
										LayoutEnd,
										CHILD_WeightedHeight,0,
									LayoutEnd,
								LayoutEnd, // screen
								CHILD_WeightedHeight,0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_WINDOW],
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_WIN_SIMPLE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_WIN_SIMPLE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_WIN_SIMPLE],
         	           					GA_Selected, nsoption_bool(window_simple_refresh),
            	    				CheckBoxEnd,
								LayoutEnd, // window
								CHILD_WeightedHeight,0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_THEME],
									LAYOUT_AddChild, gow->objects[GID_OPTS_THEME] = GetFileObject,
										GA_ID, GID_OPTS_THEME,
										GA_RelVerify, TRUE,
										GETFILE_Drawer, nsoption_charp(theme),
										GETFILE_DrawersOnly, TRUE,
										GETFILE_ReadOnly, TRUE,
										GETFILE_FullFileExpand, FALSE,
									GetFileEnd,
								LayoutEnd, // theme
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_MOUSE],
#ifdef __amigaos4__
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_PTRTRUE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_PTRTRUE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_PTRTRUE],
         	           					GA_Selected, nsoption_bool(truecolour_mouse_pointers),
										GA_Disabled, ptr_disable,
            	    				CheckBoxEnd,
#endif
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_PTROS] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_PTROS,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_PTROS],
         	           					GA_Selected, nsoption_bool(os_mouse_pointers),
										GA_Disabled, ptr_disable,
            	    				CheckBoxEnd,
								LayoutEnd, // mouse
								CHILD_WeightedHeight,0,
			                	LAYOUT_AddImage, LabelObject,
    	     	           			LABEL_Text, gadlab[LAB_OPTS_RESTART],
	            	    		LabelEnd,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // pageadd
						/*
						** Network
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_PROXY],
									LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY] = ChooserObject,
										GA_ID, GID_OPTS_PROXY,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, proxyopts,
										CHOOSER_Selected, proxytype,
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY],
									LabelEnd,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY_HOST] = StringObject,
											GA_ID, GID_OPTS_PROXY_HOST,
											GA_RelVerify, TRUE,
											GA_Disabled, proxyhostdisabled,
											STRINGA_TextVal, nsoption_charp(http_proxy_host),
											STRINGA_BufferPos,0,
										StringEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY_PORT] = IntegerObject,
											GA_ID, GID_OPTS_PROXY_PORT,
											GA_RelVerify, TRUE,
											GA_Disabled, proxyhostdisabled,
											INTEGER_Number, nsoption_charp(http_proxy_port),
											INTEGER_Minimum, 1,
											INTEGER_Maximum, 65535,
											INTEGER_Arrows, FALSE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, ":",
										LabelEnd,
									LayoutEnd, //host:port group
									CHILD_WeightedHeight, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_HOST],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY_USER] = StringObject,
										GA_ID, GID_OPTS_PROXY_USER,
										GA_RelVerify, TRUE,
										GA_Disabled, proxyauthdisabled,
										STRINGA_TextVal, nsoption_charp(http_proxy_auth_user),
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_USER],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY_PASS] = StringObject,
										GA_ID, GID_OPTS_PROXY_PASS,
										GA_RelVerify, TRUE,
										GA_Disabled, proxyauthdisabled,
										STRINGA_TextVal, nsoption_charp(http_proxy_auth_pass),
										STRINGA_BufferPos,0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_PASS],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_PROXY_BYPASS] = StringObject,
										GA_ID, GID_OPTS_PROXY_BYPASS,
										GA_RelVerify, TRUE,
										GA_Disabled, proxybypassdisabled,
										STRINGA_TextVal, nsoption_charp(http_proxy_noproxy),
										STRINGA_BufferPos, 0,
									StringEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_PROXY_BYPASS],
									LabelEnd,
								LayoutEnd, // proxy
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_FETCHING],
									LAYOUT_AddChild, gow->objects[GID_OPTS_FETCHMAX] = IntegerObject,
										GA_ID, GID_OPTS_FETCHMAX,
										GA_RelVerify, TRUE,
										INTEGER_Number, nsoption_int(max_fetchers),
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHMAX],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FETCHHOST] = IntegerObject,
										GA_ID, GID_OPTS_FETCHHOST,
										GA_RelVerify, TRUE,
										INTEGER_Number, nsoption_int(max_fetchers_per_host),
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHHOST],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FETCHCACHE] = IntegerObject,
										GA_ID, GID_OPTS_FETCHCACHE,
										GA_RelVerify, TRUE,
										INTEGER_Number, nsoption_int(max_cached_fetch_handles),
										INTEGER_Minimum, 1,
										INTEGER_Maximum, 99,
										INTEGER_Arrows, TRUE,
									IntegerEnd,
									CHILD_WeightedWidth, 0,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FETCHCACHE],
									LabelEnd,
								LayoutEnd,
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Rendering
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_IMAGES],
									LAYOUT_AddChild, gow->objects[GID_OPTS_NATIVEBM] = ChooserObject,
										GA_ID, GID_OPTS_NATIVEBM,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, nativebmopts,
										CHOOSER_Selected, nsoption_int(cache_bitmaps),
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_NATIVEBM],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_DITHERQ] = ChooserObject,
										GA_ID, GID_OPTS_DITHERQ,
										GA_RelVerify, TRUE,
										GA_Disabled, ditherdisable,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, ditheropts,
										CHOOSER_Selected, nsoption_int(dither_quality),
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_DITHERQ],
									LabelEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_SCALEQ] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_SCALEQ,
										GA_Disabled, scaledisabled,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_SCALEQ],
  				      		            GA_Selected, scaleselected,
            	    				CheckBoxEnd,
								LayoutEnd, // images
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_ANIMS],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_ANIMSPEED] = StringObject,
											GA_ID, GID_OPTS_ANIMSPEED,
											GA_RelVerify, TRUE,
											GA_Disabled, animspeeddisabled,
											STRINGA_HookType, SHK_FLOAT,
											STRINGA_TextVal, animspeed,
											STRINGA_BufferPos,0,
										StringEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_SECS],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_ANIMSPEED],
									LabelEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_ANIMDISABLE] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_ANIMDISABLE,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_ANIMDISABLE],
  				      		            GA_Selected, disableanims,
            	    				CheckBoxEnd,
								LayoutEnd, //animations
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_DPI],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_DPI_Y] = IntegerObject,
											GA_ID, GID_OPTS_DPI_Y,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(screen_ydpi),
											INTEGER_Minimum, 60,
											INTEGER_Maximum, 150,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_DPI],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_DPI_Y],
									LabelEnd,
								LayoutEnd, //animations
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Fonts
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP,
									LAYOUT_Label, gadlab[GRP_OPTS_FONTFACES],
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_SANS] = GetFontObject,
										GA_ID, GID_OPTS_FONT_SANS,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontsans,
										GETFONT_OTagOnly, TRUE,
										GETFONT_ScalableOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SANS],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_SERIF] = GetFontObject,
										GA_ID, GID_OPTS_FONT_SERIF,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontserif,
										GETFONT_OTagOnly, TRUE,
										GETFONT_ScalableOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_SERIF],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_MONO] = GetFontObject,
										GA_ID, GID_OPTS_FONT_MONO,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontmono,
										GETFONT_OTagOnly, TRUE,
										GETFONT_ScalableOnly, TRUE,
										GETFONT_FixedWidthOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_MONO],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_CURSIVE] = GetFontObject,
										GA_ID, GID_OPTS_FONT_CURSIVE,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontcursive,
										GETFONT_OTagOnly, TRUE,
										GETFONT_ScalableOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_CURSIVE],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_FANTASY] = GetFontObject,
										GA_ID, GID_OPTS_FONT_FANTASY,
										GA_RelVerify, TRUE,
										GETFONT_TextAttr, &fontfantasy,
										GETFONT_OTagOnly, TRUE,
										GETFONT_ScalableOnly, TRUE,
									GetFontEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_FANTASY],
									LabelEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_DEFAULT] = ChooserObject,
										GA_ID, GID_OPTS_FONT_DEFAULT,
										GA_RelVerify, TRUE,
										CHOOSER_PopUp, TRUE,
										CHOOSER_LabelArray, fontopts,
										CHOOSER_Selected, nsoption_int(font_default) - PLOT_FONT_FAMILY_SANS_SERIF,
									ChooserEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_FONT_DEFAULT],
									LabelEnd,
								LayoutEnd, // font faces
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, HGroupObject,
									LAYOUT_AddChild,VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_FONTSIZE],
										LAYOUT_AddChild, HGroupObject,
											LAYOUT_LabelColumn, PLACETEXT_RIGHT,
											LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_SIZE] = IntegerObject,
												GA_ID, GID_OPTS_FONT_SIZE,
												GA_RelVerify, TRUE,
												INTEGER_Number, nsoption_int(font_size) / 10,
												INTEGER_Minimum, 1,
												INTEGER_Maximum, 99,
												INTEGER_Arrows, TRUE,
											IntegerEnd,
											CHILD_WeightedWidth, 0,
											CHILD_Label, LabelObject,
												LABEL_Text, gadlab[LAB_OPTS_PT],
											LabelEnd,
										LayoutEnd,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[GID_OPTS_FONT_SIZE],
										LabelEnd,
										LAYOUT_AddChild, HGroupObject,
											LAYOUT_LabelColumn, PLACETEXT_RIGHT,
											LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_MINSIZE] = IntegerObject,
												GA_ID, GID_OPTS_FONT_MINSIZE,
												GA_RelVerify, TRUE,
												INTEGER_Number, nsoption_int(font_min_size) / 10,
												INTEGER_Minimum, 1,
												INTEGER_Maximum, 99,
												INTEGER_Arrows, TRUE,
											IntegerEnd,
											CHILD_WeightedWidth, 0,
											CHILD_Label, LabelObject,
												LABEL_Text, gadlab[LAB_OPTS_PT],
											LabelEnd,
										LayoutEnd,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[GID_OPTS_FONT_MINSIZE],
										LabelEnd,
									LayoutEnd,
#ifdef __amigaos4__
									LAYOUT_AddChild,VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_MISC],
										LAYOUT_AddChild, gow->objects[GID_OPTS_FONT_ANTIALIASING] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_FONT_ANTIALIASING,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_FONT_ANTIALIASING],
         	           						GA_Selected, nsoption_bool(font_antialiasing),
            	    					CheckBoxEnd,
									LayoutEnd,
#endif
								LayoutEnd,
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Cache
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_MEMCACHE],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_CACHE_MEM] = IntegerObject,
											GA_ID, GID_OPTS_CACHE_MEM,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(memory_cache_size) / 1048576,
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 2048,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MB],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_CACHE_MEM],
									LabelEnd,
								LayoutEnd, // memory cache
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_DISCCACHE],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_CACHE_DISC] = IntegerObject,
											GA_ID, GID_OPTS_CACHE_DISC,
											GA_RelVerify, TRUE,
											GA_Disabled, TRUE,
											INTEGER_Number, nsoption_int(disc_cache_age),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 366,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_DAYS],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_CACHE_DISC],
									LabelEnd,
								LayoutEnd, // disc cache
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Tabs
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,HGroupObject,
									LAYOUT_AddChild,VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_TABS],
										LAYOUT_AddChild, gow->objects[GID_OPTS_TAB_ACTIVE] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_TAB_ACTIVE,
         	        	   					GA_RelVerify, TRUE,
         	     	      					GA_Text, gadlab[GID_OPTS_TAB_ACTIVE],
         	     	      					GA_Selected, !nsoption_bool(new_tab_is_active),
            	    					CheckBoxEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_TAB_LAST] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_TAB_LAST,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_TAB_LAST],
         	           						GA_Selected, nsoption_bool(new_tab_last),
            	    					CheckBoxEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_TAB_2] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_TAB_2,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_TAB_2],
         	           						GA_Selected, nsoption_bool(button_2_tab),
            	    					CheckBoxEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_TAB_ALWAYS] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_TAB_ALWAYS,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_TAB_ALWAYS],
         	           						GA_Selected, nsoption_bool(tab_always_show),
            	    					CheckBoxEnd,
										LAYOUT_AddChild, gow->objects[GID_OPTS_TAB_CLOSE] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_TAB_CLOSE,
         	           						GA_RelVerify, TRUE,
         	           						GA_Text, gadlab[GID_OPTS_TAB_CLOSE],
         	           						GA_Selected, nsoption_bool(tab_close_warn),
            	    					CheckBoxEnd,
									LayoutEnd, // tabbed browsing
								LayoutEnd,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Advanced
						*/
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_DOWNLOADS],
									LAYOUT_AddChild, HGroupObject,
		                				LAYOUT_AddChild, gow->objects[GID_OPTS_OVERWRITE] = CheckBoxObject,
      	              						GA_ID, GID_OPTS_OVERWRITE,
         	           						GA_RelVerify, TRUE,
											GA_Disabled, FALSE,
         	           						GA_Text, gadlab[GID_OPTS_OVERWRITE],
         	           						GA_Selected, nsoption_bool(ask_overwrite),
    	        	    				CheckBoxEnd,
#ifdef __amigaos4__
			                			LAYOUT_AddChild, gow->objects[GID_OPTS_NOTIFY] = CheckBoxObject,
      	    	          					GA_ID, GID_OPTS_NOTIFY,
         	    	       					GA_RelVerify, TRUE,
											GA_Disabled, download_notify_disabled,
         	           						GA_Text, gadlab[GID_OPTS_NOTIFY],
         	           						GA_Selected, nsoption_bool(download_notify),
										CheckBoxEnd,
#endif
									LayoutEnd,
									LAYOUT_AddChild, gow->objects[GID_OPTS_DLDIR] = GetFileObject,
										GA_ID, GID_OPTS_DLDIR,
										GA_RelVerify, TRUE,
										GETFILE_Drawer, nsoption_charp(download_dir),
										GETFILE_DrawersOnly, TRUE,
										GETFILE_ReadOnly, TRUE,
										GETFILE_FullFileExpand, FALSE,
									GetFileEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_DLDIR],
									LabelEnd,
								LayoutEnd, // downloads
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,HGroupObject,

									LAYOUT_AddChild, VGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_BEHAVIOUR],
			                			LAYOUT_AddChild, gow->objects[GID_OPTS_STARTUP_NO_WIN] = CheckBoxObject,
    	  	              					GA_ID, GID_OPTS_STARTUP_NO_WIN,
        	 	           					GA_RelVerify, TRUE,
											GA_Text, gadlab[GID_OPTS_STARTUP_NO_WIN],
        	 	           					GA_Selected, nsoption_bool(startup_no_window),
            		    				CheckBoxEnd,
		        	        			LAYOUT_AddChild, gow->objects[GID_OPTS_CLOSE_NO_QUIT] = CheckBoxObject,
      		              					GA_ID, GID_OPTS_CLOSE_NO_QUIT,
											GA_RelVerify, TRUE,
											GA_Text, gadlab[GID_OPTS_CLOSE_NO_QUIT],
											GA_Selected, nsoption_bool(close_no_quit),
	        	        				CheckBoxEnd,
#ifdef __amigaos4__
		                				LAYOUT_AddChild, gow->objects[GID_OPTS_DOCKY] = CheckBoxObject,
											GA_ID, GID_OPTS_DOCKY,
        	 	           					GA_RelVerify, TRUE,
         		           					GA_Text, gadlab[GID_OPTS_DOCKY],
         		           					GA_Selected, !nsoption_bool(hide_docky_icon),
	            		    			CheckBoxEnd,
#endif
									LayoutEnd, // behaviour
									CHILD_WeightedHeight, 0,

								LayoutEnd, // hgroup
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, HGroupObject,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_CLIPBOARD],
			                			LAYOUT_AddChild, gow->objects[GID_OPTS_CLIPBOARD] = CheckBoxObject,
      		              					GA_ID, GID_OPTS_CLIPBOARD,
         		           					GA_RelVerify, TRUE,
         	    	       					GA_Text, gadlab[GID_OPTS_CLIPBOARD],
         	    	       					GA_Selected, nsoption_bool(clipboard_write_utf8),
            	    					CheckBoxEnd,
									LayoutEnd, // clipboard
									CHILD_WeightedHeight, 0,
									LAYOUT_AddChild,HGroupObject,
										LAYOUT_SpaceOuter, TRUE,
										LAYOUT_BevelStyle, BVS_GROUP, 
										LAYOUT_Label, gadlab[GRP_OPTS_SEARCH],
										LAYOUT_AddChild, gow->objects[GID_OPTS_SEARCH_PROV] = ChooserObject,
											GA_ID, GID_OPTS_SEARCH_PROV,
											GA_RelVerify, TRUE,
											CHOOSER_PopUp, TRUE,
											CHOOSER_LabelArray, websearch_list,
											CHOOSER_Selected, nsoption_int(search_provider),
											CHOOSER_MaxLabels, 40,
										ChooserEnd,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[GID_OPTS_SEARCH_PROV],
										LabelEnd,
									LayoutEnd, // search
									CHILD_WeightedHeight, 0,
								LayoutEnd, // hgroup
								CHILD_WeightedHeight, 0,

								LAYOUT_AddChild,HGroupObject,
									LAYOUT_BevelStyle, BVS_GROUP,
									LAYOUT_Label, gadlab[GRP_OPTS_MISC],
									LAYOUT_SpaceOuter, TRUE,
#ifdef __amigaos4__
	        	        			LAYOUT_AddChild, gow->objects[GID_OPTS_CONTEXTMENU] = CheckBoxObject,
   	    	          					GA_ID, GID_OPTS_CONTEXTMENU,
       	 	           					GA_RelVerify, TRUE,
   	     	           					GA_Text, gadlab[GID_OPTS_CONTEXTMENU],
   	     	           					GA_Selected, nsoption_bool(context_menu),
								GA_Disabled, !popupmenu_lib_ok,
           	    					CheckBoxEnd,
#endif
			               			LAYOUT_AddChild, gow->objects[GID_OPTS_FASTSCROLL] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_FASTSCROLL,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_FASTSCROLL],
         	           					GA_Selected, nsoption_bool(faster_scroll),
            	    				CheckBoxEnd,
								LayoutEnd, // context menus
								CHILD_WeightedHeight, 0,

							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
						/*
						** Export
						*/
#ifdef WITH_PDF_EXPORT
						PAGE_Add, LayoutObject,
							LAYOUT_AddChild,VGroupObject,
								LAYOUT_AddChild, HGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_MARGINS],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_MARGIN_TOP] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_TOP,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(margin_top),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_TOP],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_MARGIN_LEFT] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_LEFT,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(margin_left),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_LEFT],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_MARGIN_BOTTOM] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_BOTTOM,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(margin_bottom),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_BOTTOM],
									LabelEnd,
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_MARGIN_RIGHT] = IntegerObject,
											GA_ID, GID_OPTS_MARGIN_RIGHT,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(margin_right),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 99,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, gadlab[LAB_OPTS_MM],
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_MARGIN_RIGHT],
									LabelEnd,
								LayoutEnd, // margins
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild, VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_SCALING],
									LAYOUT_AddChild, HGroupObject,
										LAYOUT_LabelColumn, PLACETEXT_RIGHT,
										LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_SCALE] = IntegerObject,
											GA_ID, GID_OPTS_EXPORT_SCALE,
											GA_RelVerify, TRUE,
											INTEGER_Number, nsoption_int(export_scale),
											INTEGER_Minimum, 0,
											INTEGER_Maximum, 100,
											INTEGER_Arrows, TRUE,
										IntegerEnd,
										CHILD_WeightedWidth, 0,
										CHILD_Label, LabelObject,
											LABEL_Text, "%",
										LabelEnd,
									LayoutEnd,
									CHILD_Label, LabelObject,
										LABEL_Text, gadlab[GID_OPTS_EXPORT_SCALE],
									LabelEnd,
								LayoutEnd, // scaling
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_APPEARANCE],
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_NOIMAGES] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_NOIMAGES,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_NOIMAGES],
         	           					GA_Selected, nsoption_bool(suppress_images),
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_NOBKG] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_NOBKG,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_NOBKG],
         	           					GA_Selected, nsoption_bool(remove_backgrounds),
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_LOOSEN] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_LOOSEN,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_LOOSEN],
         	           					GA_Selected, nsoption_bool(enable_loosening),
            	    				CheckBoxEnd,
								LayoutEnd, // appearance
								CHILD_WeightedHeight, 0,
								LAYOUT_AddChild,VGroupObject,
									LAYOUT_SpaceOuter, TRUE,
									LAYOUT_BevelStyle, BVS_GROUP, 
									LAYOUT_Label, gadlab[GRP_OPTS_ADVANCED],
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_COMPRESS] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_COMPRESS,
         	           					GA_RelVerify, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_COMPRESS],
         	           					GA_Selected, nsoption_bool(enable_PDF_compression),
            	    				CheckBoxEnd,
		                			LAYOUT_AddChild, gow->objects[GID_OPTS_EXPORT_PASSWORD] = CheckBoxObject,
      	              					GA_ID, GID_OPTS_EXPORT_PASSWORD,
         	           					GA_RelVerify, TRUE,
										GA_Disabled, TRUE,
         	           					GA_Text, gadlab[GID_OPTS_EXPORT_PASSWORD],
         	           					GA_Selected, nsoption_bool(enable_PDF_password),
            	    				CheckBoxEnd,
								LayoutEnd, // export
								CHILD_WeightedHeight, 0,
							LayoutEnd, // page vgroup
							CHILD_WeightedHeight, 0,
						PageEnd, // page object
#endif
					End, // pagegroup
				ClickTabEnd,
                LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, gow->objects[GID_OPTS_SAVE] = ButtonObject,
						GA_ID,GID_OPTS_SAVE,
						GA_Text,gadlab[GID_OPTS_SAVE],
						GA_RelVerify,TRUE,
					ButtonEnd,
					LAYOUT_AddChild, gow->objects[GID_OPTS_USE] = ButtonObject,
						GA_ID,GID_OPTS_USE,
						GA_Text,gadlab[GID_OPTS_USE],
						GA_RelVerify,TRUE,
					ButtonEnd,
					LAYOUT_AddChild, gow->objects[GID_OPTS_CANCEL] = ButtonObject,
						GA_ID,GID_OPTS_CANCEL,
						GA_Text,gadlab[GID_OPTS_CANCEL],
						GA_RelVerify,TRUE,
					ButtonEnd,
				EndGroup, // save/use/cancel
			EndGroup, // main
		EndWindow;

		gow->win = (struct Window *)RA_OpenWindow(gow->objects[OID_MAIN]);
		gow->node = AddObject(window_list,AMINS_GUIOPTSWINDOW);
		gow->node->objstruct = gow;
	}
	ami_utf8_free(homepage_url_lc);
}

void ami_gui_opts_use(bool save)
{
	ULONG data, id = 0;
	float animspeed;
	struct TextAttr *tattr;
	char *dot;
	bool rescan_fonts = false;
	bool old_tab_always_show;

	ami_update_pointer(gow->win, GUI_POINTER_WAIT);

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_HOMEPAGE],(ULONG *)&data);
	nsoption_set_charp(homepage_url, (char *)ami_to_utf8_easy((char *)data));

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_CONTENTLANG],(ULONG *)&data);
	nsoption_set_charp(accept_language, (char *)strdup((char *)data));

	GetAttr(GA_Selected, gow->objects[GID_OPTS_FROMLOCALE],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(accept_lang_locale, true);
	} else {
		nsoption_set_bool(accept_lang_locale, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_HIDEADS],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(block_advertisements, true);
	} else {
		nsoption_set_bool(block_advertisements, false);
	}

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_HISTORY],(ULONG *)&nsoption_int(expire_url));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_REFERRAL],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(send_referer, true);
	} else {
		nsoption_set_bool(send_referer, false);
	}

#if defined(WITH_JS) || defined(WITH_MOZJS)
	GetAttr(GA_Selected,gow->objects[GID_OPTS_JAVASCRIPT],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(enable_javascript, true);
	} else {
		nsoption_set_bool(enable_javascript, false);
	}
#endif
	
	GetAttr(GA_Selected,gow->objects[GID_OPTS_DONOTTRACK],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(do_not_track, true);
	} else {
		nsoption_set_bool(do_not_track, false);
	}
	
	GetAttr(GA_Selected,gow->objects[GID_OPTS_FASTSCROLL],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(faster_scroll, true);
	} else {
		nsoption_set_bool(faster_scroll, false);
	}

	GetAttr(RADIOBUTTON_Selected,gow->objects[GID_OPTS_SCREEN],(ULONG *)&data);
	switch(data)
	{
		case 0:
			nsoption_set_charp(pubscreen_name, strdup("\0"));
			break;

		case 1:
			nsoption_set_charp(pubscreen_name, (char *)strdup("Workbench"));
			break;

		case 2:
			GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_SCREENNAME],(ULONG *)&data);
			nsoption_set_charp(pubscreen_name, (char *)strdup((char *)data));
			break;
	}

	GetAttr(GETSCREENMODE_DisplayID, gow->objects[GID_OPTS_SCREENMODE], (ULONG *)&id);
	if(id)
	{
		char *modeid = malloc(20);
		sprintf(modeid,"0x%lx", id);
		nsoption_set_charp(screen_modeid, modeid);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_WIN_SIMPLE],(ULONG *)&data);
	if ((data == TRUE) && (nsoption_bool(window_simple_refresh) == false)) {
		nsoption_set_bool(window_simple_refresh, true);
		nsoption_set_int(screen_compositing, 0);
	} else if ((data == FALSE) && (nsoption_bool(window_simple_refresh) == true)) {
		nsoption_set_bool(window_simple_refresh, false);
		nsoption_set_int(screen_compositing, -1);
	}
	
	GetAttr(GETFILE_Drawer,gow->objects[GID_OPTS_THEME],(ULONG *)&data);
	nsoption_set_charp(theme, (char *)strdup((char *)data));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_PTRTRUE],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(truecolour_mouse_pointers, true);
	} else {
		nsoption_set_bool(truecolour_mouse_pointers, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_PTROS],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(os_mouse_pointers, true);
	} else {
		nsoption_set_bool(os_mouse_pointers, false);
	}

	GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_PROXY],(ULONG *)&data);
	if(data)
	{
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, data - 1);
	}
	else
	{
		nsoption_set_bool(http_proxy, false);
	}

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_PROXY_HOST],(ULONG *)&data);
	nsoption_set_charp(http_proxy_host, (char *)strdup((char *)data));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_PROXY_PORT],(ULONG *)&nsoption_int(http_proxy_port));

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_PROXY_USER],(ULONG *)&data);
	nsoption_set_charp(http_proxy_auth_user, (char *)strdup((char *)data));

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_PROXY_PASS],(ULONG *)&data);
	nsoption_set_charp(http_proxy_auth_pass, (char *)strdup((char *)data));

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_PROXY_BYPASS],(ULONG *)&data);
	nsoption_set_charp(http_proxy_noproxy, (char *)strdup((char *)data));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_FETCHMAX],(ULONG *)&nsoption_int(max_fetchers));
	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_FETCHHOST],(ULONG *)&nsoption_int(max_fetchers_per_host));
	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_FETCHCACHE],(ULONG *)&nsoption_int(max_cached_fetch_handles));

	GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_NATIVEBM],(ULONG *)&nsoption_int(cache_bitmaps));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_SCALEQ],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(scale_quality, true);
	} else {
		nsoption_set_bool(scale_quality, false);
	}

	GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_DITHERQ],(ULONG *)&nsoption_int(dither_quality));

	GetAttr(STRINGA_TextVal,gow->objects[GID_OPTS_ANIMSPEED],(ULONG *)&data);
	animspeed = strtof((char *)data, NULL);
	nsoption_set_int(minimum_gif_delay, (int)(animspeed * 100));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_ANIMDISABLE],(ULONG *)&data);
	if(data) { 
		nsoption_set_bool(animate_images, false);
	} else { 
		nsoption_set_bool(animate_images, true);
	}

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_DPI_Y],(ULONG *)&nsoption_int(screen_ydpi));
	ami_font_setdevicedpi(id); // id set above

	GetAttr(GETFONT_TextAttr,gow->objects[GID_OPTS_FONT_SANS],(ULONG *)&data);
	tattr = (struct TextAttr *)data;

	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	nsoption_set_charp(font_sans, (char *)strdup((char *)tattr->ta_Name));

	GetAttr(GETFONT_TextAttr,gow->objects[GID_OPTS_FONT_SERIF],(ULONG *)&data);
	tattr = (struct TextAttr *)data;

	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	nsoption_set_charp(font_serif, (char *)strdup((char *)tattr->ta_Name));

	GetAttr(GETFONT_TextAttr,gow->objects[GID_OPTS_FONT_MONO],(ULONG *)&data);
	tattr = (struct TextAttr *)data;

	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	nsoption_set_charp(font_mono, (char *)strdup((char *)tattr->ta_Name));

	GetAttr(GETFONT_TextAttr,gow->objects[GID_OPTS_FONT_CURSIVE],(ULONG *)&data);
	tattr = (struct TextAttr *)data;

	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	nsoption_set_charp(font_cursive, (char *)strdup((char *)tattr->ta_Name));

	GetAttr(GETFONT_TextAttr,gow->objects[GID_OPTS_FONT_FANTASY],(ULONG *)&data);
	tattr = (struct TextAttr *)data;

	if(dot = strrchr(tattr->ta_Name,'.')) *dot = '\0';
	nsoption_set_charp(font_fantasy, (char *)strdup((char *)tattr->ta_Name));

	GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_FONT_DEFAULT],(ULONG *)&nsoption_int(font_default));
	nsoption_set_int(font_default, nsoption_int(font_default) + PLOT_FONT_FAMILY_SANS_SERIF);

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_FONT_SIZE],(ULONG *)&nsoption_int(font_size));
	nsoption_set_int(font_size, nsoption_int(font_size) * 10);

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_FONT_MINSIZE],(ULONG *)&nsoption_int(font_min_size));
	nsoption_set_int(font_min_size, nsoption_int(font_min_size) * 10);

	GetAttr(GA_Selected, gow->objects[GID_OPTS_FONT_ANTIALIASING], (ULONG *)&data);
	if(data) { 
		nsoption_set_bool(font_antialiasing, true);
	} else { 
		nsoption_set_bool(font_antialiasing, false);
	}
	
	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_CACHE_MEM],(ULONG *)&nsoption_int(memory_cache_size));
	nsoption_set_int(memory_cache_size, nsoption_int(memory_cache_size) * 1048576);

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_CACHE_DISC],(ULONG *)&nsoption_int(disc_cache_age));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_OVERWRITE],(ULONG *)&data);
	if (data) { 
		nsoption_set_bool(ask_overwrite, true);
	} else {
		nsoption_set_bool(ask_overwrite, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_NOTIFY],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(download_notify, true);
	} else {
		nsoption_set_bool(download_notify, false);
	}

	GetAttr(GETFILE_Drawer,gow->objects[GID_OPTS_DLDIR],(ULONG *)&data);
	nsoption_set_charp(download_dir, (char *)strdup((char *)data));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_TAB_ACTIVE],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(new_tab_is_active, false);
	} else {
		nsoption_set_bool(new_tab_is_active, true);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_TAB_LAST],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(new_tab_last, true);
	} else {
		nsoption_set_bool(new_tab_last, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_TAB_2],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(button_2_tab, true);
	} else {
		nsoption_set_bool(button_2_tab, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_TAB_CLOSE],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(tab_close_warn, true);
	} else {
		nsoption_set_bool(tab_close_warn, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_TAB_ALWAYS],(ULONG *)&data);
	old_tab_always_show = nsoption_bool(tab_always_show);
	
	if (data) {
		nsoption_set_bool(tab_always_show, true);
	} else {
		nsoption_set_bool(tab_always_show, false);
	}

	if(old_tab_always_show != nsoption_bool(tab_always_show))
		ami_gui_tabs_toggle_all();
	
	GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_SEARCH_PROV],(ULONG *)&nsoption_int(search_provider));
	search_web_provider_details(nsoption_int(search_provider));
	search_web_retrieve_ico(false);

	GetAttr(GA_Selected,gow->objects[GID_OPTS_CLIPBOARD],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(clipboard_write_utf8, true);
	} else {
		nsoption_set_bool(clipboard_write_utf8, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_CONTEXTMENU],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(context_menu, true);
	} else {
		nsoption_set_bool(context_menu, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_STARTUP_NO_WIN],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(startup_no_window, true);
	} else {
		nsoption_set_bool(startup_no_window, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_CLOSE_NO_QUIT],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(close_no_quit, true);
	} else { 
		nsoption_set_bool(close_no_quit, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_DOCKY],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(hide_docky_icon, false);
	} else {
		nsoption_set_bool(hide_docky_icon, true);
	}

#ifdef WITH_PDF_EXPORT
	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_MARGIN_TOP],(ULONG *)&nsoption_int(margin_top));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_MARGIN_LEFT],(ULONG *)&nsoption_int(margin_left));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_MARGIN_BOTTOM],(ULONG *)&nsoption_int(margin_bottom));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_MARGIN_RIGHT],(ULONG *)&nsoption_int(margin_right));

	GetAttr(INTEGER_Number,gow->objects[GID_OPTS_EXPORT_SCALE],(ULONG *)&nsoption_int(export_scale));

	GetAttr(GA_Selected,gow->objects[GID_OPTS_EXPORT_NOIMAGES],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(suppress_images, true);
	} else {
		nsoption_set_bool(suppress_images, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_EXPORT_NOBKG],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(remove_backgrounds, true);
	} else {
		nsoption_set_bool(remove_backgrounds, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_EXPORT_LOOSEN],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(enable_loosening, true);
	} else {
		nsoption_set_bool(enable_loosening, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_EXPORT_COMPRESS],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(enable_PDF_compression, true);
	} else {
		nsoption_set_bool(enable_PDF_compression, false);
	}

	GetAttr(GA_Selected,gow->objects[GID_OPTS_EXPORT_PASSWORD],(ULONG *)&data);
	if (data) {
		nsoption_set_bool(enable_PDF_password, true);
	} else {
		nsoption_set_bool(enable_PDF_password, false);
	}
#endif

	if(rescan_fonts == true) {
		ami_font_finiscanner();
		ami_font_initscanner(true, false);
	}

	if(save == true) {
		nsoption_write(current_user_options, NULL, NULL);
		ami_font_savescanner(); /* just in case it has changed and been used only */
	}

	ami_menu_check_toggled = true;

	ami_update_pointer(gow->win, GUI_POINTER_DEFAULT);
}

void ami_gui_opts_close(void)
{
	DisposeObject(gow->objects[OID_MAIN]);
	DelObject(gow->node);
	gow = NULL;
	ami_gui_opts_free();
}

BOOL ami_gui_opts_event(void)
{
	/* return TRUE if window destroyed */
	ULONG result,data = 0;
	uint16 code;
	STRPTR text;

	while((result = RA_HandleInput(gow->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
   		{
			case WMHI_CLOSEWINDOW:
				ami_gui_opts_close();
				return TRUE;
			break;

			case WMHI_GADGETHELP:
				if((result & WMHI_GADGETMASK) == 0) {
					/* Pointer not over our window */
					ami_help_open(AMI_HELP_MAIN, scrn);
				} else {
					/* TODO: Make this sensitive to the tab the user is currently on */
					ami_help_open(AMI_HELP_PREFS, scrn);
				}
			break;
			
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case GID_OPTS_SAVE:
						ami_gui_opts_use(true);
						ami_gui_opts_close();
						return TRUE;
					break;

					case GID_OPTS_USE:
						ami_gui_opts_use(false);
						// fall through

					case GID_OPTS_CANCEL:
						ami_gui_opts_close();
						return TRUE;
					break;

					case GID_OPTS_HOMEPAGE_DEFAULT:
						RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_HOMEPAGE],
							gow->win,NULL,STRINGA_TextVal,NETSURF_HOMEPAGE,
							TAG_DONE);
					break;

					case GID_OPTS_HOMEPAGE_CURRENT:
						if(curbw) RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_HOMEPAGE],
							gow->win, NULL, STRINGA_TextVal,
							nsurl_access(hlcache_handle_get_url(curbw->current_content)), TAG_DONE);
					break;

					case GID_OPTS_HOMEPAGE_BLANK:
						if(curbw) RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_HOMEPAGE],
							gow->win, NULL, STRINGA_TextVal,
							"about:blank", TAG_DONE);
					break;

					case GID_OPTS_FROMLOCALE:
						RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_CONTENTLANG],
							gow->win, NULL, GA_Disabled, code, TAG_DONE);

						if(code && (text = ami_locale_langs()))
						{
							RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_CONTENTLANG],
								gow->win, NULL, STRINGA_TextVal, text, TAG_DONE);
							FreeVec(text);
						}
					break;

					case GID_OPTS_SCREEN:
						GetAttr(RADIOBUTTON_Selected,gow->objects[GID_OPTS_SCREEN],(ULONG *)&data);
						switch(data)
						{
							case 0:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;

							case 1:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;

							case 2:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENMODE],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_SCREENNAME],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
							break;
						}
					break;

					case GID_OPTS_SCREENMODE:
						IDoMethod(gow->objects[GID_OPTS_SCREENMODE],
						GSM_REQUEST,gow->win);
					break;

					case GID_OPTS_THEME:
						IDoMethod(gow->objects[GID_OPTS_THEME],
						GFILE_REQUEST,gow->win);
					break;

					case GID_OPTS_PROXY:
						GetAttr(CHOOSER_Selected,gow->objects[GID_OPTS_PROXY],(ULONG *)&data);
						switch(data)
						{
							case 0:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);

								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_BYPASS],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
							break;
							case 1:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);

								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, TRUE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_BYPASS],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
							break;

							case 2:
							case 3:
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_HOST],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PORT],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);

								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_USER],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_PASS],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
								RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_PROXY_BYPASS],
								gow->win,NULL, GA_Disabled, FALSE, TAG_DONE);
							break;
						}
					break;

					case GID_OPTS_ANIMDISABLE:
						RefreshSetGadgetAttrs((struct Gadget *)gow->objects[GID_OPTS_ANIMSPEED],
							gow->win,NULL, GA_Disabled, code, TAG_DONE);
					break;

					case GID_OPTS_FONT_SANS:
						IDoMethod(gow->objects[GID_OPTS_FONT_SANS],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_SERIF:
						IDoMethod(gow->objects[GID_OPTS_FONT_SERIF],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_MONO:
						IDoMethod(gow->objects[GID_OPTS_FONT_MONO],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_CURSIVE:
						IDoMethod(gow->objects[GID_OPTS_FONT_CURSIVE],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_FONT_FANTASY:
						IDoMethod(gow->objects[GID_OPTS_FONT_FANTASY],
						GFONT_REQUEST,gow->win);
					break;

					case GID_OPTS_DLDIR:
						IDoMethod(gow->objects[GID_OPTS_DLDIR],
						GFILE_REQUEST,gow->win);
					break;
				}
			break;
		}
	}
	return FALSE;
}

STRPTR *ami_gui_opts_websearch(void)
{
	char buf[300];
	ULONG ref = 0;
	STRPTR *websearchlist;

	websearchlist = AllocVecTagList(200, NULL); /* NB: Was not MEMF_PRIVATE */

	if (nsoption_charp(search_engines_file) == NULL) return websearchlist;

	FILE *f = fopen(nsoption_charp(search_engines_file), "r");
	if (f == NULL) return websearchlist;

	while (fgets(buf, sizeof(buf), f) != NULL) {
		if (buf[0] == '\0') continue;
		buf[strlen(buf)-1] = '\0';
		websearchlist[ref] = strdup(strtok(buf, "|"));
		ref++;
	}
	fclose(f);
	websearchlist[ref] = NULL;

	return websearchlist;
}

void ami_gui_opts_websearch_free(STRPTR *websearchlist)
{
	ULONG ref = 0;

	while (websearchlist[ref] != NULL) {
		free(websearchlist[ref]);
		ref++;
	}

	FreeVec(websearchlist);
}
