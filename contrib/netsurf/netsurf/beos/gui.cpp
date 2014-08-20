/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#define __STDBOOL_H__	1
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>

#include <Alert.h>
#include <Application.h>
#include <BeBuild.h>
#include <FindDirectory.h>
#include <Mime.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <String.h>

extern "C" {

#include "content/content.h"
#include "content/content_protected.h"
#include "content/fetch.h"
#include "content/fetchers/curl.h"
#include "content/fetchers/resource.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser_private.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"

#include "render/form.h"
#include "utils/filename.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"
}

#include "beos/gui.h"

#include "beos/gui_options.h"
//#include "beos/completion.h"
#include "beos/window.h"
#include "beos/throbber.h"
#include "beos/filetype.h"
#include "beos/download.h"
#include "beos/schedule.h"
#include "beos/fetch_rsrc.h"
#include "beos/scaffolding.h"


static void *myrealloc(void *ptr, size_t len, void *pw);

/* Where to search for shared resources.  Must have trailing / */
#define RESPATH "/boot/apps/netsurf/res/"

//TODO: use resources
// enable using resources instead of files
#define USE_RESOURCES 1

bool replicated = false; /**< if we are running as a replicant */

char *options_file_location;
char *glade_file_location;

struct gui_window *search_current_window = 0;

BWindow *wndAbout;
BWindow *wndWarning;
//GladeXML *gladeWindows;
BWindow *wndTooltip;
//beosLabel *labelTooltip;
BFilePanel *wndOpenFile;

//static beosWidget *select_menu;
static struct browser_window *select_menu_bw;
static struct form_control *select_menu_control;

static thread_id sBAppThreadID;

static BMessage *gFirstRefsReceived = NULL;

static int sEventPipe[2];

// #pragma mark - class NSBrowserFrameView


NSBrowserApplication::NSBrowserApplication()
	: BApplication("application/x-vnd.NetSurf")
{
}


NSBrowserApplication::~NSBrowserApplication()
{
}


void
NSBrowserApplication::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_REFS_RECEIVED:
		case B_UI_SETTINGS_CHANGED:
		// messages for top-level
		// we'll just send them to the first window
		case 'back':
		case 'forw':
		case 'stop':
		case 'relo':
		case 'home':
		case 'urlc':
		case 'urle':
		case 'menu':
		// NetPositive messages
		case B_NETPOSITIVE_OPEN_URL:
		case B_NETPOSITIVE_BACK:
		case B_NETPOSITIVE_FORWARD:
		case B_NETPOSITIVE_HOME:
		case B_NETPOSITIVE_RELOAD:
		case B_NETPOSITIVE_STOP:
		case B_NETPOSITIVE_DOWN:
		case B_NETPOSITIVE_UP:
			//DetachCurrentMessage();
			//nsbeos_pipe_message(message, this, fGuiWindow);
			break;
		default:
			BApplication::MessageReceived(message);
	}
}


void
NSBrowserApplication::ArgvReceived(int32 argc, char **argv)
{
	NSBrowserWindow *win = nsbeos_find_last_window();
	if (!win) {
		return;
	}
	win->Unlock();
	BMessage *message = DetachCurrentMessage();
	nsbeos_pipe_message_top(message, win, win->Scaffolding());
}


void
NSBrowserApplication::RefsReceived(BMessage *message)
{
	DetachCurrentMessage();
	NSBrowserWindow *win = nsbeos_find_last_window();
	if (!win) {
		gFirstRefsReceived = message;
		return;
	}
	win->Unlock();
	nsbeos_pipe_message_top(message, win, win->Scaffolding());
}


void
NSBrowserApplication::AboutRequested()
{
	nsbeos_pipe_message(new BMessage(B_ABOUT_REQUESTED), NULL, NULL);
}


bool
NSBrowserApplication::QuitRequested()
{
	// let it notice it
	nsbeos_pipe_message(new BMessage(B_QUIT_REQUESTED), NULL, NULL);
	// we'll let the main thread Quit() ourselves when it's done.
	return false;
}


// #pragma mark - implementation



/* realpath fallback on R5 */
#if !defined(__HAIKU__) && !defined(B_BEOS_VERSION_DANO)
extern "C" char *realpath(const char *f, char *buf);
char *realpath(const char *f, char *buf)
{
	BPath path(f, NULL, true);
	if (path.InitCheck() < 0) {
		strncpy(buf, f, MAXPATHLEN);
		return NULL;
	}
	//printf("RP: '%s'\n", path.Path());
	strncpy(buf, path.Path(), MAXPATHLEN);
	return buf;
}
#endif

/* finds the NetSurf binary image ID and path
 * 
 */
image_id nsbeos_find_app_path(char *path)
{
	image_info info;
	int32 cookie = 0;
	while (get_next_image_info(0, &cookie, &info) == B_OK) {
//fprintf(stderr, "%p <> %p, %p\n", (char *)&find_app_resources, (char *)info.text, (char *)info.text + info.text_size);
		if (((char *)&nsbeos_find_app_path >= (char *)info.text)
		 && ((char *)&nsbeos_find_app_path < (char *)info.text + info.text_size)) {
//fprintf(stderr, "match\n");
			if (path) {
				memset(path, 0, B_PATH_NAME_LENGTH);
				strncpy(path, info.name, B_PATH_NAME_LENGTH-1);
			}
			return info.id;
		}
	}
	return B_ERROR;
}

/**
 * Locate a shared resource file by searching known places in order.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 * Search order is: ~/config/settings/NetSurf/, ~/.netsurf/, $NETSURFRES/
 * (where NETSURFRES is an environment variable), and finally the path
 * specified by the #define at the top of this file.
 */

static char *find_resource(char *buf, const char *filename, const char *def)
{
	const char *cdir = NULL;
	status_t err;
	BPath path;
	char t[PATH_MAX];

	err = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append("NetSurf");
	if (err >= B_OK)
		cdir = path.Path();
	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/");
		strcat(t, filename);
		realpath(t, buf);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	cdir = getenv("HOME");
	if (cdir != NULL) {
		strcpy(t, cdir);
		strcat(t, "/.netsurf/");
		strcat(t, filename);
		realpath(t, buf);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	cdir = getenv("NETSURFRES");

	if (cdir != NULL) {
		realpath(cdir, buf);
		strcat(buf, "/");
		strcat(buf, filename);
		if (access(buf, R_OK) == 0)
			return buf;
	}

	strcpy(t, RESPATH);
	strcat(t, filename);
	realpath(t, buf);
	if (access(buf, R_OK) == 0)
		return buf;

	if (def[0] == '%') {
		snprintf(t, PATH_MAX, "%s%s", path.Path(), def + 1);
		realpath(t, buf);
	} else if (def[0] == '~') {
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		realpath(t, buf);
	} else {
		realpath(def, buf);
	}

	return buf;
}

/**
 * Check that ~/.netsurf/ exists, and if it doesn't, create it.
 */
static void check_homedir(void)
{
	status_t err;

	BPath path;
	err = find_directory(B_USER_SETTINGS_DIRECTORY, &path, true);

	if (err < B_OK) {
		/* we really can't continue without a home directory. */
		LOG(("Can't find user settings directory - nowhere to store state!"));
		die("NetSurf needs to find the user settings directory in order to run.\n");
	}

	path.Append("NetSurf");
	err = create_directory(path.Path(), 0644); 
	if (err < B_OK) {
		LOG(("Unable to create %s", path.Path()));
		die("NetSurf could not create its settings directory.\n");
	}
}

static int32 bapp_thread(void *arg)
{
	be_app->Lock();
	be_app->Run();
	return 0;
}

static nsurl *gui_get_resource_url(const char *path)
{
	nsurl *url = NULL;
	BString u("rsrc:///");

	/* default.css -> beosdefault.css */
	if (strcmp(path, "default.css") == 0)
		path = "beosdefault.css";

	/* favicon.ico -> favicon.png */
	if (strcmp(path, "favicon.ico") == 0)
		path = "favicon.png";

	u << path;
	LOG(("(%s) -> '%s'\n", path, u.String()));
	nsurl_create(u.String(), &url);
	return url;
}



#if !defined(__HAIKU__) && !defined(B_BEOS_VERSION_DANO)
/* more ui_colors, R5 only had a few defined... */
#define B_PANEL_TEXT_COLOR ((color_which)10)
#define B_DOCUMENT_BACKGROUND_COLOR ((color_which)11)
#define B_DOCUMENT_TEXT_COLOR ((color_which)12)
#define B_CONTROL_BACKGROUND_COLOR ((color_which)13)
#define B_CONTROL_TEXT_COLOR ((color_which)14)
#define B_CONTROL_BORDER_COLOR ((color_which)15)
#define B_CONTROL_HIGHLIGHT_COLOR ((color_which)16)
#define B_NAVIGATION_BASE_COLOR ((color_which)4)
#define B_NAVIGATION_PULSE_COLOR ((color_which)17)
#define B_SHINE_COLOR ((color_which)18)
#define B_SHADOW_COLOR ((color_which)19)
#define B_MENU_SELECTED_BORDER_COLOR ((color_which)9)
#define B_TOOL_TIP_BACKGROUND_COLOR ((color_which)20)
#define B_TOOL_TIP_TEXT_COLOR ((color_which)21)
#define B_SUCCESS_COLOR ((color_which)100)
#define B_FAILURE_COLOR ((color_which)101)
#define B_MENU_SELECTED_BACKGROUND_COLOR B_MENU_SELECTION_BACKGROUND_COLOR
#define B_RANDOM_COLOR ((color_which)0x80000000)
#define B_MICHELANGELO_FAVORITE_COLOR ((color_which)0x80000001)
#define B_DSANDLER_FAVORITE_SKY_COLOR ((color_which)0x80000002)
#define B_DSANDLER_FAVORITE_INK_COLOR ((color_which)0x80000003)
#define B_DSANDLER_FAVORITE_SHOES_COLOR ((color_which)0x80000004)
#define B_DAVE_BROWN_FAVORITE_COLOR ((color_which)0x80000005)
#endif
#if defined(B_BEOS_VERSION_DANO)
#define B_TOOL_TIP_BACKGROUND_COLOR B_TOOLTIP_BACKGROUND_COLOR
#define B_TOOL_TIP_TEXT_COLOR B_TOOLTIP_TEXT_COLOR
#define
#endif
#define NOCOL ((color_which)0)

/**
 * set option from pen
 */
static nserror
set_colour_from_ui(struct nsoption_s *opts,
                   color_which ui,
                   enum nsoption_e option,
                   colour def_colour)
{
	if (ui != NOCOL) {
		rgb_color c;
		if (ui == B_DESKTOP_COLOR) {
			BScreen s;
			c = s.DesktopColor();
		} else {
			c = ui_color(ui);
		}

		def_colour = ((((uint32_t)c.blue << 16) & 0xff0000) |
					  ((c.green << 8) & 0x00ff00) |
					  ((c.red) & 0x0000ff));
	}

	opts[option].value.c = def_colour;

	return NSERROR_OK;
}

/**
 * Set option defaults for framebuffer frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	/* set system colours for beos ui */
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ActiveBorder, 0x00000000);
	set_colour_from_ui(defaults, B_WINDOW_TAB_COLOR, NSOPTION_sys_colour_ActiveCaption, 0x00dddddd);
	set_colour_from_ui(defaults, B_PANEL_BACKGROUND_COLOR, NSOPTION_sys_colour_AppWorkspace, 0x00eeeeee);
	set_colour_from_ui(defaults, B_DESKTOP_COLOR, NSOPTION_sys_colour_Background, 0x00aa0000);
	set_colour_from_ui(defaults, B_CONTROL_BACKGROUND_COLOR, NSOPTION_sys_colour_ButtonFace, 0x00aaaaaa);
	set_colour_from_ui(defaults, B_CONTROL_HIGHLIGHT_COLOR, NSOPTION_sys_colour_ButtonHighlight, 0x00cccccc);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ButtonShadow, 0x00bbbbbb);
	set_colour_from_ui(defaults, B_CONTROL_TEXT_COLOR, NSOPTION_sys_colour_ButtonText, 0x00000000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_CaptionText, 0x00000000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_GrayText, 0x00777777);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_Highlight, 0x00ee0000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_HighlightText, 0x00000000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_InactiveBorder, 0x00000000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_InactiveCaption, 0x00ffffff);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_InactiveCaptionText, 0x00cccccc);
	set_colour_from_ui(defaults, B_TOOL_TIP_BACKGROUND_COLOR, NSOPTION_sys_colour_InfoBackground, 0x00aaaaaa);
	set_colour_from_ui(defaults, B_TOOL_TIP_TEXT_COLOR, NSOPTION_sys_colour_InfoText, 0x00000000);
	set_colour_from_ui(defaults, B_MENU_BACKGROUND_COLOR, NSOPTION_sys_colour_Menu, 0x00aaaaaa);
	set_colour_from_ui(defaults, B_MENU_ITEM_TEXT_COLOR, NSOPTION_sys_colour_MenuText, 0x00000000);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_Scrollbar, 0x00aaaaaa);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ThreeDDarkShadow, 0x00555555);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ThreeDFace, 0x00dddddd);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ThreeDHighlight, 0x00aaaaaa);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ThreeDLightShadow, 0x00999999);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_ThreeDShadow, 0x00777777);
	set_colour_from_ui(defaults, B_DOCUMENT_BACKGROUND_COLOR, NSOPTION_sys_colour_Window, 0x00aaaaaa);
	set_colour_from_ui(defaults, NOCOL, NSOPTION_sys_colour_WindowFrame, 0x00000000);
	set_colour_from_ui(defaults, B_DOCUMENT_TEXT_COLOR, NSOPTION_sys_colour_WindowText, 0x00000000);

	return NSERROR_OK;
}

void nsbeos_update_system_ui_colors(void)
{
	set_defaults(nsoptions);
}

/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
        /* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}

static BPath get_messages_path()
{
	BPath p("/boot/apps/netsurf/res");
	// TODO: use Haiku's BLocale stuff
	BString lang(getenv("LC_MESSAGES"));
	lang.Truncate(2);
	BDirectory d(p.Path());
	if (!d.Contains(lang.String(), B_DIRECTORY_NODE))
		lang = "en";
	p.Append(lang.String());
	p.Append("Messages");
	return p;
}


static void gui_init(int argc, char** argv)
{
	const char *addr;
	nsurl *url;
	nserror error;
	char buf[PATH_MAX];

	if (pipe(sEventPipe) < 0)
		return;
	if (!replicated) {
		sBAppThreadID = spawn_thread(bapp_thread, "BApplication(NetSurf)", B_NORMAL_PRIORITY, (void *)find_thread(NULL));
		if (sBAppThreadID < B_OK)
			return; /* #### handle errors */
		if (resume_thread(sBAppThreadID) < B_OK)
			return;
	}

	nsbeos_update_system_ui_colors();

	fetch_rsrc_register();

	check_homedir();

	// make sure the cache dir exists
	create_directory(TEMP_FILENAME_PREFIX, 0700);

	//nsbeos_completion_init();


	/* This is an ugly hack to just get the new-style throbber going.
	 * It, along with the PNG throbber loader, need making more generic.
	 */
	{
#define STROF(n) #n
#define FIND_THROB(n) filenames[(n)] = \
				"throbber/throbber" STROF(n) ".png";
		char *filenames[9];
		FIND_THROB(0);
		FIND_THROB(1);
		FIND_THROB(2);
		FIND_THROB(3);
		FIND_THROB(4);
		FIND_THROB(5);
		FIND_THROB(6);
		FIND_THROB(7);
		FIND_THROB(8);
		nsbeos_throbber_initialise_from_png(9,
			filenames[0], filenames[1], filenames[2], filenames[3],
			filenames[4], filenames[5], filenames[6], filenames[7], 
			filenames[8]);
#undef FIND_THROB
#undef STROF
	}

	if (nsbeos_throbber == NULL)
		die("Unable to load throbber image.\n");

	find_resource(buf, "Choices", "%/Choices");
	LOG(("Using '%s' as Preferences file", buf));
	options_file_location = strdup(buf);
	nsoption_read(buf, NULL);


	/* check what the font settings are, setting them to a default font
	 * if they're not set - stops Pango whinging
	 */
#define SETFONTDEFAULT(OPTION,y) if (nsoption_charp(OPTION) == NULL) nsoption_set_charp(OPTION, strdup((y)))

	//XXX: use be_plain_font & friends, when we can check if font is serif or not.
/*
	font_family family;
	font_style style;
	be_plain_font->GetFamilyAndStyle(&family, &style);
	SETFONTDEFAULT(font_sans, family);
	SETFONTDEFAULT(font_serif, family);
	SETFONTDEFAULT(font_mono, family);
	SETFONTDEFAULT(font_cursive, family);
	SETFONTDEFAULT(font_fantasy, family);
*/
#ifdef __HAIKU__
	SETFONTDEFAULT(font_sans, "DejaVu Sans");
	SETFONTDEFAULT(font_serif, "DejaVu Serif");
	SETFONTDEFAULT(font_mono, "DejaVu Mono");
	SETFONTDEFAULT(font_cursive, "DejaVu Sans");
	SETFONTDEFAULT(font_fantasy, "DejaVu Sans");
#else
	SETFONTDEFAULT(font_sans, "Bitstream Vera Sans");
	SETFONTDEFAULT(font_serif, "Bitstream Vera Serif");
	SETFONTDEFAULT(font_mono, "Bitstream Vera Sans Mono");
	SETFONTDEFAULT(font_cursive, "Bitstream Vera Serif");
	SETFONTDEFAULT(font_fantasy, "Bitstream Vera Serif");
#endif

	nsbeos_options_init();

	if (nsoption_charp(cookie_file) == NULL) {
		find_resource(buf, "Cookies", "%/Cookies");
		LOG(("Using '%s' as Cookies file", buf));
		nsoption_set_charp(cookie_file, strdup(buf));
	}
	if (nsoption_charp(cookie_jar) == NULL) {
		find_resource(buf, "Cookies", "%/Cookies");
		LOG(("Using '%s' as Cookie Jar file", buf));
		nsoption_set_charp(cookie_jar, strdup(buf));
	}
	if ((nsoption_charp(cookie_file) == NULL) || 
	    (nsoption_charp(cookie_jar) == NULL))
		die("Failed initialising cookie options");

	if (nsoption_charp(url_file) == NULL) {
		find_resource(buf, "URLs", "%/URLs");
		LOG(("Using '%s' as URL file", buf));
		nsoption_set_charp(url_file, strdup(buf));
	}

        if (nsoption_charp(ca_path) == NULL) {
                find_resource(buf, "certs", "/etc/ssl/certs");
                LOG(("Using '%s' as certificate path", buf));
                nsoption_set_charp(ca_path, strdup(buf));
        }

	//find_resource(buf, "mime.types", "/etc/mime.types");
	beos_fetch_filetype_init();

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	//nsbeos_download_initialise();

	if (!replicated)
		be_app->Unlock();

	if (argc > 1) {
		addr = argv[1];
	} else if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	/* create an initial browser window */
	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(
			BW_CREATE_HISTORY,
			url,
			NULL,
			NULL,
			NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	if (gFirstRefsReceived) {
		// resend the refs we got before having a window to send them to
		be_app_messenger.SendMessage(gFirstRefsReceived);
		delete gFirstRefsReceived;
		gFirstRefsReceived = NULL;
	}

}




void nsbeos_pipe_message(BMessage *message, BView *_this, struct gui_window *gui)
{
	if (message == NULL) {
		fprintf(stderr, "%s(NULL)!\n", __FUNCTION__);
		return;
	}
	if (_this)
		message->AddPointer("View", _this);
	if (gui)
		message->AddPointer("gui_window", gui);
	int len = write(sEventPipe[1], &message, sizeof(void *));
	//LOG(("nsbeos_pipe_message: %d written", len));
	//printf("nsbeos_pipe_message: %d written\n", len);
}


void nsbeos_pipe_message_top(BMessage *message, BWindow *_this, struct beos_scaffolding *scaffold)
{
	if (message == NULL) {
		fprintf(stderr, "%s(NULL)!\n", __FUNCTION__);
		return;
	}
	if (_this)
		message->AddPointer("Window", _this);
	if (scaffold)
		message->AddPointer("scaffolding", scaffold);
	int len = write(sEventPipe[1], &message, sizeof(void *));
	//LOG(("nsbeos_pipe_message: %d written", len));
	//printf("nsbeos_pipe_message: %d written\n", len);
}


static void gui_poll(bool active)
{
	CURLMcode code;
	fd_set read_fd_set, write_fd_set, exc_fd_set;
	int max_fd = 0;
	struct timeval timeout;
	unsigned int fd_count = 0;
	bool block = true;
	bigtime_t next_schedule = 0;

	// handle early deadlines
	schedule_run();

	FD_ZERO(&read_fd_set);
	FD_ZERO(&write_fd_set);
	FD_ZERO(&exc_fd_set);

	if (active) {
		code = curl_multi_fdset(fetch_curl_multi,
				&read_fd_set,
				&write_fd_set,
				&exc_fd_set,
				&max_fd);
		assert(code == CURLM_OK);
	}

	// our own event pipe
	FD_SET(sEventPipe[0], &read_fd_set);
	max_fd = MAX(max_fd, sEventPipe[0] + 1);

	// If there are pending events elsewhere, we should not be blocking
	if (!browser_reformat_pending) {
		if (earliest_callback_timeout != B_INFINITE_TIMEOUT) {
			next_schedule = earliest_callback_timeout - system_time();
			block = false;
		}

		// we're quite late already...
		if (next_schedule < 0)
			next_schedule = 0;

	} else //we're not allowed to sleep, there is other activity going on.
		block = false;

	/*
	LOG(("gui_poll: browser_reformat_pending:%d earliest_callback_timeout:%Ld"
		" next_schedule:%Ld block:%d ", browser_reformat_pending,
		earliest_callback_timeout, next_schedule, block));
	*/

	timeout.tv_sec = (long)(next_schedule / 1000000LL);
	timeout.tv_usec = (long)(next_schedule % 1000000LL);

	//LOG(("gui_poll: select(%d, ..., %Ldus", max_fd, next_schedule));
	fd_count = select(max_fd, &read_fd_set, &write_fd_set, &exc_fd_set, 
		block ? NULL : &timeout);
	//LOG(("select: %d\n", fd_count));

	if (fd_count > 0 && FD_ISSET(sEventPipe[0], &read_fd_set)) {
		BMessage *message;
		int len = read(sEventPipe[0], &message, sizeof(void *));
		//LOG(("gui_poll: BMessage ? %d read", len));
		if (len == sizeof(void *)) {
			//LOG(("gui_poll: BMessage.what %-4.4s\n", &(message->what)));
			nsbeos_dispatch_event(message);
		}
	}

	schedule_run();

	if (browser_reformat_pending)
		nsbeos_window_process_reformats();
}


static void gui_quit(void)
{
	urldb_save_cookies(nsoption_charp(cookie_jar));
	urldb_save(nsoption_charp(url_file));
	//options_save_tree(hotlist,nsoption_charp(hotlist_file),messages_get("TreeHotlist"));

	free(nsoption_charp(cookie_file));
	free(nsoption_charp(cookie_jar));
	beos_fetch_filetype_fin();
	fetch_rsrc_unregister();
}

static char *url_to_path(const char *url)
{
	char *url_path = curl_unescape(url, 0);
	char *path;

	/* return the absolute path including leading / */
	path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN - 1));
	curl_free(url_path);

	return path;
}

/**
 * Send the source of a content to a text editor.
 */

void nsbeos_gui_view_source(struct hlcache_handle *content)
{
	char *temp_name;
	bool done = false;
	BPath path;
	status_t err;
	size_t size;
	const char *source = content_get_source_data(content, &size);

	if (!content || !source) {
		warn_user("MiscError", "No document source");
		return;
	}

	/* try to load local files directly. */
	temp_name = url_to_path(nsurl_access(hlcache_handle_get_url(content)));
	if (temp_name) {
		path.SetTo(temp_name);
		BEntry entry;
		if (entry.SetTo(path.Path()) >= B_OK 
			&& entry.Exists() && entry.IsFile())
			done = true;
	}
	if (!done) {
		/* We cannot release the requested filename until after it
		 * has finished being used. As we can't easily find out when
		 * this is, we simply don't bother releasing it and simply
		 * allow it to be re-used next time NetSurf is started. The
		 * memory overhead from doing this is under 1 byte per
		 * filename. */
		const char *filename = filename_request();
		if (!filename) {
			warn_user("NoMemory", 0);
			return;
		}
		path.SetTo(TEMP_FILENAME_PREFIX);
		path.Append(filename);
		BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE);
		err = file.InitCheck();
		if (err < B_OK) {
			warn_user("IOError", strerror(err));
			return;
		}
		err = file.Write(source, size);
		if (err < B_OK) {
			warn_user("IOError", strerror(err));
			return;
		}
		lwc_string *mime = content_get_mime_type(content);
		if (mime) {
			file.WriteAttr("BEOS:TYPE", B_MIME_STRING_TYPE, 0LL, 
				lwc_string_data(mime), lwc_string_length(mime) + 1);
			lwc_string_unref(mime);
		}
		
	}

	entry_ref ref;
	if (get_ref_for_path(path.Path(), &ref) < B_OK)
		return;

	BMessage m(B_REFS_RECEIVED);
	m.AddRef("refs", &ref);


	// apps to try
	const char *editorSigs[] = {
		"application/x-vnd.beunited.pe",
		"application/x-vnd.XEmacs",
		"application/x-vnd.Haiku-StyledEdit",
		"application/x-vnd.Be-STEE",
		"application/x-vnd.yT-STEE",
		NULL
	};
	int i;
	for (i = 0; editorSigs[i]; i++) {
		team_id team = -1;
		{
			BMessenger msgr(editorSigs[i], team);
			if (msgr.SendMessage(&m) >= B_OK)
				break;
		}
		
		err = be_roster->Launch(editorSigs[i], (BMessage *)&m, &team);
		if (err >= B_OK)
			break;
	}
}

/**
 * Broadcast an URL that we can't handle.
 */

static void gui_launch_url(const char *url)
{
	status_t status;
	// try to open it as an URI
	BString mimeType = "application/x-vnd.Be.URL.";
	BString arg(url);
	mimeType.Append(arg, arg.FindFirst(":"));

	// special case, text/x-email is used traditionally
	// use it instead
	if (arg.IFindFirst("mailto:") == 0)
		mimeType = "text/x-email";

	// the protocol should be alphanum
	// we just check if it's registered
	// if not there is likely no supporting app anyway
	if (!BMimeType::IsValid(mimeType.String()))
		return;
	char *args[2] = { (char *)url, NULL };
	status = be_roster->Launch(mimeType.String(), 1, args);
	if (status < B_OK)
		warn_user("Cannot launch url", strerror(status));
}


/**
 * Display a warning for a serious problem (eg memory exhaustion).
 *
 * \param  warning  message key for warning message
 * \param  detail   additional message, or 0
 */

void warn_user(const char *warning, const char *detail)
{
	LOG(("warn_user: %s (%s)", warning, detail));
	BAlert *alert;
	BString text(warning);
	if (detail)
		text << ":\n" << detail;

	alert = new BAlert("NetSurf Warning", text.String(), "Debug", "Ok", NULL, 
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	if (alert->Go() < 1)
		debugger("warn_user");
}

void die(const char * const error)
{
	fprintf(stderr, "%s", error);
	BAlert *alert;
	BString text("Cannot continue:\n");
	text << error;

	alert = new BAlert("NetSurf Error", text.String(), "Debug", "Ok", NULL, 
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	if (alert->Go() < 1)
		debugger("die");

	exit(EXIT_FAILURE);
}

static void nsbeos_create_ssl_verify_window(struct browser_window *bw,
		hlcache_handle *c, const struct ssl_cert_info *certs,
		unsigned long num)
{
	CALLED();
}

static char *path_to_url(const char *path)
{
	int urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	char *url = (char *)malloc(urllen);

	if (url == NULL) {
		return NULL;
	}

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	} 

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}


static void *myrealloc(void *ptr, size_t len, void *pw)
{
	if (len == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, len);
}

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */

static char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
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
	if(path[strlen(path) - 1] != '/')
		strncat(path, "/", length);

	strncat(path, newpart, length);

	return true;
}

static struct gui_clipboard_table beos_clipboard_table = {
	gui_get_clipboard,
	gui_set_clipboard,
};

static struct gui_fetch_table beos_fetch_table = {
	filename_from_path,
        path_add_part,
        fetch_filetype,
        path_to_url,
        url_to_path,
        gui_get_resource_url,
        NULL //fetch_mimetype
};

static struct gui_browser_table beos_browser_table = {
	gui_poll,
        beos_schedule,
	gui_quit,
	NULL, //set_search_ico
	gui_launch_url,
	NULL, //create_form_select_menu
	NULL, //cert_verify
	gui_401login_open
};


/** Normal entry point from OS */
int main(int argc, char** argv)
{
	nserror ret;
	BPath options;
	struct gui_table beos_gui_table = {
		&beos_browser_table,
		beos_window_table,
		beos_download_table,
		&beos_clipboard_table,
                &beos_fetch_table
	};

	if (find_directory(B_USER_SETTINGS_DIRECTORY, &options, true) == B_OK) {
		options.Append("x-vnd.NetSurf");
	}

	if (!replicated) {
		// create the Application object before trying to use messages
		// so we can open an alert in case of error.
		new NSBrowserApplication;
	}

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	nsoption_read(options.Path(), NULL);
	nsoption_commandline(&argc, argv, NULL);

	/* common initialisation */
	BPath messages = get_messages_path();
	ret = netsurf_init(messages.Path(), &beos_gui_table);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	gui_init(argc, argv);

	netsurf_main_loop();

	netsurf_exit();

	return 0;
}

/** called when replicated from NSBaseView::Instantiate() */
int gui_init_replicant(int argc, char** argv)
{
	nserror ret;
	BPath options;
	struct gui_table beos_gui_table = {
		&beos_browser_table,
		beos_window_table,
		beos_download_table,
		&beos_clipboard_table,
                &beos_fetch_table
	};

	if (find_directory(B_USER_SETTINGS_DIRECTORY, &options, true) == B_OK) {
		options.Append("x-vnd.NetSurf");
	}

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	// FIXME: use options as readonly for replicants
	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		// FIXME: must not die when in replicant!
		die("Options failed to initialise");
	}
	nsoption_read(options.Path(), NULL);
	nsoption_commandline(&argc, argv, NULL);

	/* common initialisation */
	BPath messages = get_messages_path();
	ret = netsurf_init(messages.Path(), &beos_gui_table);
	if (ret != NSERROR_OK) {
		// FIXME: must not die when in replicant!
		die("NetSurf failed to initialise");
	}

	gui_init(argc, argv);

	return 0;
}
