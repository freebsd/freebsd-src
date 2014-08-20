/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004-2008 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2004-2009 John Tytgat <joty@netsurf-browser.org>
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

#include <assert.h>
#include <errno.h>
#include <fpu_control.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <features.h>
#include <unixlib/local.h>
#include <curl/curl.h>
#include "oslib/font.h"
#include "oslib/help.h"
#include "oslib/hourglass.h"
#include "oslib/inetsuite.h"
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/osmodule.h"
#include "oslib/osspriteop.h"
#include "oslib/pdriver.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "oslib/uri.h"
#include "rufl.h"

#include "utils/config.h"
#include "utils/filename.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "content/fetchers/resource.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "desktop/save_complete.h"
#include "desktop/treeview.h"
#include "render/font.h"

#include "riscos/content-handlers/artworks.h"
#include "riscos/bitmap.h"
#include "riscos/buffer.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/content-handlers/draw.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/gui/url_bar.h"
#include "riscos/help.h"
#include "riscos/hotlist.h"
#include "riscos/iconbar.h"
#include "riscos/menus.h"
#include "riscos/message.h"
#include "riscos/mouse.h"
#include "riscos/print.h"
#include "riscos/query.h"
#include "riscos/save.h"
#include "riscos/sslcert.h"
#include "riscos/content-handlers/sprite.h"
#include "riscos/textselection.h"
#include "riscos/theme.h"
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/uri.h"
#include "riscos/url_protocol.h"
#include "riscos/url_complete.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "riscos/window.h"
#include "riscos/ucstables.h"


#ifndef FILETYPE_ACORN_URI
#define FILETYPE_ACORN_URI 0xf91
#endif
#ifndef FILETYPE_ANT_URL
#define FILETYPE_ANT_URL 0xb28
#endif
#ifndef FILETYPE_IEURL
#define FILETYPE_IEURL 0x1ba
#endif
#ifndef FILETYPE_HTML
#define FILETYPE_HTML 0xfaf
#endif
#ifndef FILETYPE_JNG
#define FILETYPE_JNG 0xf78
#endif
#ifndef FILETYPE_CSS
#define FILETYPE_CSS 0xf79
#endif
#ifndef FILETYPE_MNG
#define FILETYPE_MNG 0xf83
#endif
#ifndef FILETYPE_GIF
#define FILETYPE_GIF 0x695
#endif
#ifndef FILETYPE_BMP
#define FILETYPE_BMP 0x69c
#endif
#ifndef FILETYPE_ICO
#define FILETYPE_ICO 0x132
#endif
#ifndef FILETYPE_PNG
#define FILETYPE_PNG 0xb60
#endif
#ifndef FILETYPE_JPEG
#define FILETYPE_JPEG 0xc85
#endif
#ifndef FILETYPE_ARTWORKS
#define FILETYPE_ARTWORKS 0xd94
#endif
#ifndef FILETYPE_SVG
#define FILETYPE_SVG 0xaad
#endif

extern bool ro_plot_patterned_lines;

int os_version = 0;

const char * const __dynamic_da_name = "NetSurf";	/**< For UnixLib. */
int __dynamic_da_max_size = 128 * 1024 * 1024;	/**< For UnixLib. */
int __feature_imagefs_is_file = 1;		/**< For UnixLib. */
/* default filename handling */
int __riscosify_control = __RISCOSIFY_NO_SUFFIX |
			__RISCOSIFY_NO_REVERSE_SUFFIX;
#ifndef __ELF__
extern int __dynamic_num;
#endif

const char * NETSURF_DIR;

static const char *task_name = "NetSurf";
#define CHOICES_PREFIX "<Choices$Write>.WWW.NetSurf."

ro_gui_drag_type gui_current_drag_type;
wimp_t task_handle;	/**< RISC OS wimp task handle. */
static clock_t gui_last_poll;	/**< Time of last wimp_poll. */
osspriteop_area *gui_sprites;	   /**< Sprite area containing pointer and hotlist sprites */

/** Accepted wimp user messages. */
static ns_wimp_message_list task_messages = {
	message_HELP_REQUEST,
	{
		message_DATA_SAVE,
		message_DATA_SAVE_ACK,
		message_DATA_LOAD,
		message_DATA_LOAD_ACK,
		message_DATA_OPEN,
		message_PRE_QUIT,
		message_SAVE_DESKTOP,
		message_MENU_WARNING,
		message_MENUS_DELETED,
		message_WINDOW_INFO,
		message_CLAIM_ENTITY,
		message_DATA_REQUEST,
		message_DRAGGING,
		message_DRAG_CLAIM,
		message_MODE_CHANGE,
		message_PALETTE_CHANGE,
		message_FONT_CHANGED,
		message_URI_PROCESS,
		message_URI_RETURN_RESULT,
		message_INET_SUITE_OPEN_URL,
#ifdef WITH_PLUGIN
		message_PLUG_IN_OPENING,
		message_PLUG_IN_CLOSED,
		message_PLUG_IN_RESHAPE_REQUEST,
		message_PLUG_IN_FOCUS,
		message_PLUG_IN_URL_ACCESS,
		message_PLUG_IN_STATUS,
		message_PLUG_IN_BUSY,
		message_PLUG_IN_STREAM_NEW,
		message_PLUG_IN_STREAM_WRITE,
		message_PLUG_IN_STREAM_WRITTEN,
		message_PLUG_IN_STREAM_DESTROY,
		message_PLUG_IN_OPEN,
		message_PLUG_IN_CLOSE,
		message_PLUG_IN_RESHAPE,
		message_PLUG_IN_STREAM_AS_FILE,
		message_PLUG_IN_NOTIFY,
		message_PLUG_IN_ABORT,
		message_PLUG_IN_ACTION,
		/* message_PLUG_IN_INFORMED, (not provided by oslib) */
#endif
		message_PRINT_SAVE,
		message_PRINT_ERROR,
		message_PRINT_TYPE_ODD,
		message_HOTLIST_ADD_URL,
		message_HOTLIST_CHANGED,
		0
	}
};

static struct
{
	int	width;  /* in OS units */
	int	height;
} screen_info;

static void ro_gui_create_dirs(void);
static void ro_gui_create_dir(char *path);
static void ro_gui_choose_language(void);
static void ro_gui_signal(int sig);
static void ro_gui_cleanup(void);
static void ro_gui_handle_event(wimp_event_no event, wimp_block *block);
static void ro_gui_close_window_request(wimp_close *close);
static void ro_gui_check_resolvers(void);
static void ro_gui_keypress(wimp_key *key);
static void ro_gui_user_message(wimp_event_no event, wimp_message *message);
static void ro_msg_dataload(wimp_message *block);
static char *ro_gui_uri_file_parse(const char *file_name, char **uri_title);
static bool ro_gui_uri_file_parse_line(FILE *fp, char *b);
static char *ro_gui_url_file_parse(const char *file_name);
static char *ro_gui_ieurl_file_parse(const char *file_name);
static void ro_msg_terminate_filename(wimp_full_message_data_xfer *message);
static void ro_msg_datasave(wimp_message *message);
static void ro_msg_datasave_ack(wimp_message *message);
static void ro_msg_dataopen(wimp_message *message);
static void ro_gui_get_screen_properties(void);
static void ro_msg_prequit(wimp_message *message);
static void ro_msg_save_desktop(wimp_message *message);
static void ro_msg_window_info(wimp_message *message);
static void ro_gui_view_source_bounce(wimp_message *message);

static nsurl *gui_get_resource_url(const char *path)
{
	static const char base_url[] = "file:///NetSurf:/Resources/";
	size_t path_len, length;
	char *raw;
	nsurl *url = NULL;

	/* Map paths first */
	if (strcmp(path, "adblock.css") == 0) {
		path = "AdBlock";

	} else if (strcmp(path, "default.css") == 0) {
		path = "CSS";

	} else if (strcmp(path, "quirks.css") == 0) {
		path = "Quirks";

	} else if (strcmp(path, "favicon.ico") == 0) {
		path = "Icons/content.png";

	} else if (strcmp(path, "user.css") == 0) {
		/* Special case; this file comes from Choices: */
		nsurl_create("file:///Choices:WWW/NetSurf/User", &url);
		return url;
	}

	path_len = strlen(path);

	/* Find max URL length */
	length = SLEN(base_url) + SLEN("xx/") + path_len + 1;

	raw = malloc(length);
	if (raw != NULL) {
		/* Insert base URL */
		char *ptr = memcpy(raw, base_url, SLEN(base_url));
		ptr += SLEN(base_url);

		/* Add language directory to URL, for translated files */
		/* TODO: handle non-en langauages
		 *       handle non-html translated files */
		if (path_len > SLEN(".html") &&
				strncmp(path + path_len - SLEN(".html"),
					".html", SLEN(".html")) == 0) {
			memcpy(ptr, "en/", SLEN("en/"));
			ptr += SLEN("en/");
		}

		/* Add filename to URL */
		memcpy(ptr, path, path_len);
		ptr += path_len;

		/* Terminate string */
		*ptr = '\0';

		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
}

/**
 * set option from wimp
 */
static nserror
set_colour_from_wimp(struct nsoption_s *opts,
                   wimp_colour wimp,
                   enum nsoption_e option,
                   colour def_colour)
{
	os_error *error;
	os_PALETTE(20) palette;

	error = xwimp_read_true_palette((os_palette *) &palette);
	if (error != NULL) {
		LOG(("xwimp_read_palette: 0x%x: %s",
		     error->errnum, error->errmess));
	} else {
		/* entries are in B0G0R0LL */
		def_colour = palette.entries[wimp] >> 8;
	}

	opts[option].value.c = def_colour;

	return NSERROR_OK;
}

/**
 * Set option defaults for riscos frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 *
 * @TODO -- The wimp_COLOUR_... values here map the colour definitions
 *          to parts of the RISC OS desktop palette.  In places this
 *          is fairly arbitrary, and could probably do with
 *          re-checking.
 *
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	/* Set defaults for absent option strings */
	nsoption_setnull_charp(ca_bundle, strdup("NetSurf:Resources.ca-bundle"));
	nsoption_setnull_charp(cookie_file, strdup("NetSurf:Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup(CHOICES_PREFIX "Cookies"));

	if (nsoption_charp(ca_bundle) == NULL ||
	    nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		LOG(("Failed initialising string options"));
		return NSERROR_BAD_PARAMETER;
	}

	/* set default system colours for riscos ui */
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_ActiveBorder, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_CREAM, NSOPTION_sys_colour_ActiveCaption, 0x00dddddd);
	set_colour_from_wimp(defaults, wimp_COLOUR_VERY_LIGHT_GREY, NSOPTION_sys_colour_AppWorkspace, 0x00eeeeee);
	set_colour_from_wimp(defaults, wimp_COLOUR_VERY_LIGHT_GREY, NSOPTION_sys_colour_Background, 0x00aa0000);/* \TODO -- Check */
	set_colour_from_wimp(defaults, wimp_COLOUR_VERY_LIGHT_GREY, NSOPTION_sys_colour_ButtonFace, 0x00aaaaaa);
	set_colour_from_wimp(defaults, wimp_COLOUR_DARK_GREY, NSOPTION_sys_colour_ButtonHighlight, 0x00cccccc);/* \TODO -- Check */
	set_colour_from_wimp(defaults, wimp_COLOUR_MID_DARK_GREY, NSOPTION_sys_colour_ButtonShadow, 0x00bbbbbb);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_ButtonText, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_CaptionText, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_MID_LIGHT_GREY, NSOPTION_sys_colour_GrayText, 0x00777777);/* \TODO -- Check */
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_Highlight, 0x00ee0000);
	set_colour_from_wimp(defaults, wimp_COLOUR_WHITE, NSOPTION_sys_colour_HighlightText, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_InactiveBorder, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_LIGHT_GREY, NSOPTION_sys_colour_InactiveCaption, 0x00ffffff);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_InactiveCaptionText, 0x00cccccc);
	set_colour_from_wimp(defaults, wimp_COLOUR_CREAM, NSOPTION_sys_colour_InfoBackground, 0x00aaaaaa);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_InfoText, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_WHITE, NSOPTION_sys_colour_Menu, 0x00aaaaaa);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_MenuText, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_LIGHT_GREY, NSOPTION_sys_colour_Scrollbar, 0x00aaaaaa);/* \TODO -- Check */
	set_colour_from_wimp(defaults, wimp_COLOUR_MID_DARK_GREY, NSOPTION_sys_colour_ThreeDDarkShadow, 0x00555555);
	set_colour_from_wimp(defaults, wimp_COLOUR_VERY_LIGHT_GREY, NSOPTION_sys_colour_ThreeDFace, 0x00dddddd);
	set_colour_from_wimp(defaults, wimp_COLOUR_WHITE, NSOPTION_sys_colour_ThreeDHighlight, 0x00aaaaaa);
	set_colour_from_wimp(defaults, wimp_COLOUR_WHITE, NSOPTION_sys_colour_ThreeDLightShadow, 0x00999999);
	set_colour_from_wimp(defaults, wimp_COLOUR_MID_DARK_GREY, NSOPTION_sys_colour_ThreeDShadow, 0x00777777);
	set_colour_from_wimp(defaults, wimp_COLOUR_VERY_LIGHT_GREY, NSOPTION_sys_colour_Window, 0x00aaaaaa);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_WindowFrame, 0x00000000);
	set_colour_from_wimp(defaults, wimp_COLOUR_BLACK, NSOPTION_sys_colour_WindowText, 0x00000000);

	return NSERROR_OK;
}

/**
 * Initialise the gui (RISC OS specific part).
 */

static void gui_init(int argc, char** argv)
{
	struct {
		void (*sigabrt)(int);
		void (*sigfpe)(int);
		void (*sigill)(int);
		void (*sigint)(int);
		void (*sigsegv)(int);
		void (*sigterm)(int);
	} prev_sigs;
	char path[40];
	os_error *error;
	int length;
	char *nsdir_temp;
	byte *base;
	nserror err;

	/* re-enable all FPU exceptions/traps except inexact operations,
	 * which we're not interested in, and underflow which is incorrectly
	 * raised when converting an exact value of 0 from double-precision
	 * to single-precision on FPEmulator v4.09-4.11 (MVFD F0,#0:MVFS F0,F0)
	 * - UnixLib disables all FP exceptions by default */

	_FPU_SETCW(_FPU_IEEE & ~(_FPU_MASK_PM | _FPU_MASK_UM));

	xhourglass_start(1);

	/* read OS version for code that adapts to conform to the OS
	 * (remember that it's preferable to check for specific features
	 * being present) */
	xos_byte(osbyte_IN_KEY, 0, 0xff, &os_version, NULL);

	/* the first release version of the A9home OS is incapable of
	   plotting patterned lines (presumably a fault in the hw acceleration) */
	if (!xosmodule_lookup("VideoHWSMI", NULL, NULL, &base, NULL, NULL)) {
#if 0   // this fault still hasn't been fixed, so disable patterned lines for all versions until it has
		const char *help = (char*)base + ((int*)base)[5];
		while (*help > 9) help++;
		while (*help == 9) help++;
		if (!memcmp(help, "0.55", 4))
#endif
			ro_plot_patterned_lines = false;
	}

	/* Create our choices directories */
	ro_gui_create_dirs();

	/* Register exit and signal handlers */
	atexit(ro_gui_cleanup);
	prev_sigs.sigabrt = signal(SIGABRT, ro_gui_signal);
	prev_sigs.sigfpe = signal(SIGFPE, ro_gui_signal);
	prev_sigs.sigill = signal(SIGILL, ro_gui_signal);
	prev_sigs.sigint = signal(SIGINT, ro_gui_signal);
	prev_sigs.sigsegv = signal(SIGSEGV, ro_gui_signal);
	prev_sigs.sigterm = signal(SIGTERM, ro_gui_signal);

	if (prev_sigs.sigabrt == SIG_ERR || prev_sigs.sigfpe == SIG_ERR ||
			prev_sigs.sigill == SIG_ERR ||
			prev_sigs.sigint == SIG_ERR ||
			prev_sigs.sigsegv == SIG_ERR ||
			prev_sigs.sigterm == SIG_ERR)
		die("Failed registering signal handlers");

	/* Load in UI sprites */
	gui_sprites = ro_gui_load_sprite_file("NetSurf:Resources.Sprites");
	if (!gui_sprites)
		die("Unable to load Sprites.");

	/* Find NetSurf directory */
	nsdir_temp = getenv("NetSurf$Dir");
	if (!nsdir_temp)
		die("Failed to locate NetSurf directory");
	NETSURF_DIR = strdup(nsdir_temp);
	if (!NETSURF_DIR)
		die("Failed duplicating NetSurf directory string");

	/* Initialise filename allocator */
	filename_initialise();

	/* Initialise save complete functionality */
	save_complete_init();

	/* Load in visited URLs and Cookies */
	urldb_load(nsoption_charp(url_path));
	urldb_load_cookies(nsoption_charp(cookie_file));

	/* Initialise with the wimp */
	error = xwimp_initialise(wimp_VERSION_RO38, task_name,
			PTR_WIMP_MESSAGE_LIST(&task_messages), 0,
			&task_handle);
	if (error) {
		LOG(("xwimp_initialise: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}
	/* Register message handlers */
	ro_message_register_route(message_HELP_REQUEST,
			ro_gui_interactive_help_request);
	ro_message_register_route(message_DATA_OPEN,
			ro_msg_dataopen);
	ro_message_register_route(message_DATA_SAVE,
			ro_msg_datasave);
	ro_message_register_route(message_DATA_SAVE_ACK,
			ro_msg_datasave_ack);
	ro_message_register_route(message_PRE_QUIT,
			ro_msg_prequit);
	ro_message_register_route(message_SAVE_DESKTOP,
			ro_msg_save_desktop);
	ro_message_register_route(message_DRAGGING,
			ro_gui_selection_dragging);
	ro_message_register_route(message_DRAG_CLAIM,
			ro_gui_selection_drag_claim);
	ro_message_register_route(message_WINDOW_INFO,
			ro_msg_window_info);

	/* Initialise the font subsystem */
	nsfont_init();

	/* Initialise global information */
	ro_gui_get_screen_properties();
	ro_gui_wimp_get_desktop_font();

	/* Issue a *Desktop to poke AcornURI into life */
	if (getenv("NetSurf$Start_URI_Handler"))
		xwimp_start_task("Desktop", 0);

	/* Open the templates */
	if ((length = snprintf(path, sizeof(path),
			"NetSurf:Resources.%s.Templates",
			nsoption_charp(language))) < 0 || length >= (int)sizeof(path))
		die("Failed to locate Templates resource.");
	error = xwimp_open_template(path);
	if (error) {
		LOG(("xwimp_open_template failed: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}

	err = treeview_init(12);
	if (err != NSERROR_OK) {
		die("Failed to initialise treeview");
	}

	/* Initialise themes before dialogs */
	ro_gui_theme_initialise();
	/* Initialise dialog windows (must be after UI sprites are loaded) */
	ro_gui_dialog_init();
	/* Initialise download window */
	ro_gui_download_init();
	/* Initialise menus */
	ro_gui_menu_init();
	/* Initialise query windows */
	ro_gui_query_init();
	/* Initialise the history subsystem */
	ro_gui_history_init();
	/* Initialise toolbars */
	ro_toolbar_init();
	/* Initialise url bar module */
	ro_gui_url_bar_init();
	/* Initialise browser windows */
	ro_gui_window_initialise();

	/* Done with the templates file */
	wimp_close_template();

	/* Create Iconbar icon and menus */
	ro_gui_iconbar_initialise();

	/* Finally, check Inet$Resolvers for sanity */
	ro_gui_check_resolvers();
}

/**
 * Create intermediate directories for Choices and User Data files
 */
void ro_gui_create_dirs(void)
{
	char buf[256];
	char *path;

	/* Choices */
	path = getenv("NetSurf$ChoicesSave");
	if (!path)
		die("Failed to find NetSurf Choices save path");

	snprintf(buf, sizeof(buf), "%s", path);
	ro_gui_create_dir(buf);

	/* URL */
	snprintf(buf, sizeof(buf), "%s", nsoption_charp(url_save));
	ro_gui_create_dir(buf);

	/* Hotlist */
	snprintf(buf, sizeof(buf), "%s", nsoption_charp(hotlist_save));
	ro_gui_create_dir(buf);

	/* Recent */
	snprintf(buf, sizeof(buf), "%s", nsoption_charp(recent_save));
	ro_gui_create_dir(buf);

	/* Theme */
	snprintf(buf, sizeof(buf), "%s", nsoption_charp(theme_save));
	ro_gui_create_dir(buf);
	/* and the final directory part (as theme_save is a directory) */
	xosfile_create_dir(buf, 0);
}


/**
 * Create directory structure for a path
 *
 * Given a path of x.y.z directories x and x.y will be created
 *
 * \param path the directory path to create
 */
void ro_gui_create_dir(char *path)
{
  	char *cur = path;
	while ((cur = strchr(cur, '.'))) {
		*cur = '\0';
		xosfile_create_dir(path, 0);
		*cur++ = '.';
	}
}


/**
 * Choose the language to use.
 */

void ro_gui_choose_language(void)
{
	char path[40];

	/* if option_language exists and is valid, use that */
	if (nsoption_charp(language)) {
		if (2 < strlen(nsoption_charp(language)))
			nsoption_charp(language)[2] = 0;
		sprintf(path, "NetSurf:Resources.%s", nsoption_charp(language));
		if (is_dir(path)) {
			nsoption_setnull_charp(accept_language, 
					strdup(nsoption_charp(language)));
			return;
		}
		nsoption_set_charp(language, NULL);
	}

	nsoption_set_charp(language, strdup(ro_gui_default_language()));
	if (nsoption_charp(language) == NULL)
		die("Out of memory");
	nsoption_set_charp(accept_language, strdup(nsoption_charp(language)));
	if (nsoption_charp(accept_language) == NULL)
		die("Out of memory");
}


/**
 * Determine the default language to use.
 *
 * RISC OS has no standard way of determining which language the user prefers.
 * We have to guess from the 'Country' setting.
 */

const char *ro_gui_default_language(void)
{
	char path[40];
	const char *lang;
	int country;
	os_error *error;

	/* choose a language from the configured country number */
	error = xosbyte_read(osbyte_VAR_COUNTRY_NUMBER, &country);
	if (error) {
		LOG(("xosbyte_read failed: 0x%x: %s",
				error->errnum, error->errmess));
		country = 1;
	}
	switch (country) {
		case 7: /* Germany */
		case 30: /* Austria */
		case 35: /* Switzerland (70% German-speaking) */
			lang = "de";
			break;
		case 6: /* France */
		case 18: /* Canada2 (French Canada?) */
			lang = "fr";
			break;
		case 34: /* Netherlands */
			lang = "nl";
			break;
		default:
			lang = "en";
			break;
	}
	sprintf(path, "NetSurf:Resources.%s", lang);
	if (is_dir(path))
		return lang;
	return "en";
}


/**
 * Warn the user if Inet$Resolvers is not set.
 */

void ro_gui_check_resolvers(void)
{
	char *resolvers;
	resolvers = getenv("Inet$Resolvers");
	if (resolvers && resolvers[0]) {
		LOG(("Inet$Resolvers '%s'", resolvers));
	} else {
		LOG(("Inet$Resolvers not set or empty"));
		warn_user("Resolvers", 0);
	}
}

/**
 * Convert a RISC OS pathname to a file: URL.
 *
 * \param  path  RISC OS pathname
 * \return  URL, allocated on heap, or 0 on failure
 */

static char *path_to_url(const char *path)
{
	int spare;
	char *canonical_path; /* canonicalised RISC OS path */
	char *unix_path; /* unix path */
	char *escurl;
	os_error *error;
	url_func_result url_err;
	int urllen;
	char *url; /* resulting url */

        /* calculate the canonical risc os path */
	error = xosfscontrol_canonicalise_path(path, 0, 0, 0, 0, &spare);
	if (error) {
		LOG(("xosfscontrol_canonicalise_path failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PathToURL", error->errmess);
		return NULL;
	}

	canonical_path = malloc(1 - spare);
	if (canonical_path == NULL) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		free(canonical_path);
		return NULL;
	}

	error = xosfscontrol_canonicalise_path(path, canonical_path, 0, 0, 1 - spare, 0);
	if (error) {
		LOG(("xosfscontrol_canonicalise_path failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PathToURL", error->errmess);
		free(canonical_path);
		return NULL;
	}

	/* create a unix path from teh cananocal risc os one */
	unix_path = __unixify(canonical_path, __RISCOSIFY_NO_REVERSE_SUFFIX, NULL, 0, 0);

	if (unix_path == NULL) {
		LOG(("__unixify failed: %s", canonical_path));
		free(canonical_path);
		return NULL;
	}
	free(canonical_path);

        /* convert the unix path into a url */
	urllen = strlen(unix_path) + FILE_SCHEME_PREFIX_LEN + 1;
	url = malloc(urllen);
	if (url == NULL) {
		LOG(("Unable to allocate url"));
		free(unix_path);
		return NULL;
	}

	if (*unix_path == '/') {
		snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, unix_path + 1);
	} else {
		snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, unix_path);
	}
	free(unix_path);

	/* We don't want '/' to be escaped.  */
	url_err = url_escape(url, FILE_SCHEME_PREFIX_LEN, false, "/", &escurl);
	free(url); url = NULL;
	if (url_err != URL_FUNC_OK) {
		LOG(("url_escape failed: %s", url));
		return NULL;
	}

	return escurl;
}


/**
 * Convert a file: URL to a RISC OS pathname.
 *
 * \param  url  a file: URL
 * \return  RISC OS pathname, allocated on heap, or 0 on failure
 */

char *url_to_path(const char *url)
{
	char *path;
	char *filename;
	char *respath;
	url_func_result res; /* result from url routines */
	char *r;

	res = url_path(url, &path);
	if (res != URL_FUNC_OK) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	res = url_unescape(path, &respath);
	free(path);
	if (res != URL_FUNC_OK) {
		return NULL;
	}

	/* RISC OS path should not be more than 100 characters longer */
	filename = malloc(strlen(respath) + 100);
	if (!filename) {
		free(respath);
		warn_user("NoMemory", 0);
		return NULL;
	}

	r = __riscosify(respath, 0, __RISCOSIFY_NO_SUFFIX,
			filename, strlen(respath) + 100, 0);

	free(respath);
	if (r == 0) {
		free(filename);
		LOG(("__riscosify failed"));
		return NULL;
	}

	return filename;
}

/**
 * Last-minute gui init, after all other modules have initialised.
 */

static void gui_init2(int argc, char** argv)
{
	char *url = 0;
	bool open_window = nsoption_bool(open_browser_at_startup);

	/* Complete initialisation of the treeview modules. */

	/* certificate verification window */
	ro_gui_cert_postinitialise();

	/* hotlist window */
	ro_gui_hotlist_postinitialise();

	/* global history window */
	ro_gui_global_history_postinitialise();

	/* cookies window */
	ro_gui_cookies_postinitialise();


	/* parse command-line arguments */
	if (argc == 2) {
		LOG(("parameters: '%s'", argv[1]));
		/* this is needed for launching URI files */
		if (strcasecmp(argv[1], "-nowin") == 0)
			open_window = false;
	}
	else if (argc == 3) {
		LOG(("parameters: '%s' '%s'", argv[1], argv[2]));
		open_window = true;

		/* HTML files */
		if (strcasecmp(argv[1], "-html") == 0) {
			url = path_to_url(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* URL files */
		else if (strcasecmp(argv[1], "-urlf") == 0) {
			url = ro_gui_url_file_parse(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* ANT URL Load */
		else if (strcasecmp(argv[1], "-url") == 0) {
			url = strdup(argv[2]);
			if (!url) {
				LOG(("malloc failed"));
				die("Insufficient memory for URL");
			}
		}
		/* Unknown => exit here. */
		else {
			LOG(("Unknown parameters: '%s' '%s'",
				argv[1], argv[2]));
			return;
		}
	}
	/* get user's homepage (if configured) */
	else if (nsoption_charp(homepage_url) && nsoption_charp(homepage_url)[0]) {
		url = calloc(strlen(nsoption_charp(homepage_url)) + 5, sizeof(char));
		if (!url) {
			LOG(("malloc failed"));
			die("Insufficient memory for URL");
		}
		sprintf(url, "%s", nsoption_charp(homepage_url));
	}
	/* default homepage */
	else {
		url = strdup(NETSURF_HOMEPAGE);
		if (!url) {
			LOG(("malloc failed"));
			die("Insufficient memory for URL");
		}
	}

	if (open_window) {
		nsurl *urlns;
		nserror errorns;

		errorns = nsurl_create(url, &urlns);
		if (errorns == NSERROR_OK) {
			errorns = browser_window_create(BW_CREATE_HISTORY,
							urlns,
							NULL,
							NULL,
							NULL);
			nsurl_unref(urlns);
		}
		if (errorns != NSERROR_OK) {
			warn_user(messages_get_errorcode(errorns), 0);
		}
	}

	free(url);
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


/**
 * Close down the gui (RISC OS).
 */

static void gui_quit(void)
{
	urldb_save_cookies(nsoption_charp(cookie_jar));
	urldb_save(nsoption_charp(url_save));
	ro_gui_window_quit();
	ro_gui_global_history_destroy();
	ro_gui_hotlist_destroy();
	ro_gui_cookies_destroy();
	ro_gui_saveas_quit();
	ro_gui_url_bar_fini();
	rufl_quit();
	free(gui_sprites);
	xwimp_close_down(task_handle);
	xhourglass_off();
}


/**
 * Handles a signal
 */

void ro_gui_signal(int sig)
{
	static const os_error error = { 1, "NetSurf has detected a serious "
			"error and must exit. Please submit a bug report, "
			"attaching the browser log file." };
	os_colour old_sand, old_glass;

	ro_gui_cleanup();

	xhourglass_on();
	xhourglass_colours(0x0000ffff, 0x000000ff, &old_sand, &old_glass);
	nsoption_dump(stderr, NULL);
	/*rufl_dump_state();*/

#ifndef __ELF__
	/* save WimpSlot and DA to files if NetSurf$CoreDump exists */
	int used;
	xos_read_var_val_size("NetSurf$CoreDump", 0, 0, &used, 0, 0);
	if (used) {
		int curr_slot;
		xwimp_slot_size(-1, -1, &curr_slot, 0, 0);
		LOG(("saving WimpSlot, size 0x%x", curr_slot));
		xosfile_save("$.NetSurf_Slot", 0x8000, 0,
				(byte *) 0x8000,
				(byte *) 0x8000 + curr_slot);

		if (__dynamic_num != -1) {
			int size;
			byte *base_address;
			xosdynamicarea_read(__dynamic_num, &size,
					&base_address, 0, 0, 0, 0, 0);
			LOG(("saving DA %i, base %p, size 0x%x",
					__dynamic_num,
					base_address, size));
			xosfile_save("$.NetSurf_DA",
					(bits) base_address, 0,
					base_address,
					base_address + size);
		}
	}
#else
	/* Save WimpSlot and UnixLib managed DAs when UnixEnv$coredump
	 * defines a coredump directory.  */
	_kernel_oserror *err = __unixlib_write_coredump (NULL);
	if (err != NULL)
		LOG(("Coredump failed: %s", err->errmess));
#endif

	xhourglass_colours(old_sand, old_glass, 0, 0);
	xhourglass_off();

	__write_backtrace(sig);

	xwimp_report_error_by_category(&error,
			wimp_ERROR_BOX_GIVEN_CATEGORY |
			wimp_ERROR_BOX_CATEGORY_ERROR <<
				wimp_ERROR_BOX_CATEGORY_SHIFT,
			"NetSurf", "!netsurf",
			(osspriteop_area *) 1, "Quit", 0);
	xos_cli("Filer_Run <Wimp$ScrapDir>.WWW.NetSurf.Log");

	_Exit(sig);
}


/**
 * Ensures the gui exits cleanly.
 */

void ro_gui_cleanup(void)
{
	ro_gui_buffer_close();
	xhourglass_off();
	/* Uninstall NetSurf-specific fonts */
	xos_cli("FontRemove NetSurf:Resources.Fonts.");
}


/**
 * Poll the OS for events (RISC OS).
 *
 * \param active return as soon as possible
 */

static void riscos_poll(bool active)
{
	wimp_event_no event;
	wimp_block block;
	const wimp_poll_flags mask = wimp_MASK_LOSE | wimp_MASK_GAIN |
			wimp_SAVE_FP;
	os_t track_poll_offset;

	/* Poll wimp. */
	xhourglass_off();
	track_poll_offset = ro_mouse_poll_interval();
	if (active) {
		event = wimp_poll(mask, &block, 0);
	} else if (sched_active || (track_poll_offset > 0) ||
			browser_reformat_pending) {
		os_t t = os_read_monotonic_time();

		if (track_poll_offset > 0)
			t += track_poll_offset;
		else
			t += 10;

		if (sched_active && (sched_time - t) < 0)
			t = sched_time;

		event = wimp_poll_idle(mask, &block, t, 0);
	} else {
		event = wimp_poll(wimp_MASK_NULL | mask, &block, 0);
	}

	xhourglass_on();
	gui_last_poll = clock();
	ro_gui_handle_event(event, &block);

	/* Only run scheduled callbacks on a null poll
	 * We cannot do this in the null event handler, as that may be called
	 * from gui_multitask(). Scheduled callbacks must only be run from the
	 * top-level.
	 */
	if (event == wimp_NULL_REASON_CODE) {
		schedule_run();
	}

	ro_gui_window_update_boxes();

	if (browser_reformat_pending && event == wimp_NULL_REASON_CODE)
		ro_gui_window_process_reformats();
}


/**
 * Process a Wimp_Poll event.
 *
 * \param event wimp event number
 * \param block parameter block
 */

void ro_gui_handle_event(wimp_event_no event, wimp_block *block)
{
	switch (event) {
		case wimp_NULL_REASON_CODE:
			ro_gui_throb();
			ro_mouse_poll();
			break;

		case wimp_REDRAW_WINDOW_REQUEST:
			ro_gui_wimp_event_redraw_window(&block->redraw);
			break;

		case wimp_OPEN_WINDOW_REQUEST:
			ro_gui_open_window_request(&block->open);
			break;

		case wimp_CLOSE_WINDOW_REQUEST:
			ro_gui_close_window_request(&block->close);
			break;

		case wimp_POINTER_LEAVING_WINDOW:
			ro_mouse_pointer_leaving_window(&block->leaving);
			break;

		case wimp_POINTER_ENTERING_WINDOW:
			ro_gui_wimp_event_pointer_entering_window(&block->entering);
			break;

		case wimp_MOUSE_CLICK:
			ro_gui_wimp_event_mouse_click(&block->pointer);
			break;

		case wimp_USER_DRAG_BOX:
			ro_mouse_drag_end(&block->dragged);
			break;

		case wimp_KEY_PRESSED:
			ro_gui_keypress(&(block->key));
			break;

		case wimp_MENU_SELECTION:
			ro_gui_menu_selection(&(block->selection));
			break;

		/* Scroll requests fall back to a generic handler because we
		 * might get these events for any window from a scroll-wheel.
		 */

		case wimp_SCROLL_REQUEST:
			if (!ro_gui_wimp_event_scroll_window(&(block->scroll)))
				ro_gui_scroll(&(block->scroll));
			break;

		case wimp_USER_MESSAGE:
		case wimp_USER_MESSAGE_RECORDED:
		case wimp_USER_MESSAGE_ACKNOWLEDGE:
			ro_gui_user_message(event, &(block->message));
			break;
	}
}


/**
 * Handle Open_Window_Request events.
 */

void ro_gui_open_window_request(wimp_open *open)
{
	os_error *error;

	if (ro_gui_wimp_event_open_window(open))
		return;

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Handle Close_Window_Request events.
 */

void ro_gui_close_window_request(wimp_close *close)
{
	if (ro_gui_alt_pressed())
		ro_gui_window_close_all();
	else {
		if (ro_gui_wimp_event_close_window(close->w))
			return;
		ro_gui_dialog_close(close->w);
	}
}


/**
 * Handle Key_Pressed events.
 */

static void ro_gui_keypress_cb(void *pw)
{
	wimp_key *key = (wimp_key *) pw;

	if (ro_gui_wimp_event_keypress(key) == false) {
		os_error *error = xwimp_process_key(key->c);
		if (error) {
			LOG(("xwimp_process_key: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	free(key);
}

void ro_gui_keypress(wimp_key *key)
{
	if (key->c == wimp_KEY_ESCAPE &&
		(gui_current_drag_type == GUI_DRAG_SAVE ||
		 gui_current_drag_type == GUI_DRAG_DOWNLOAD_SAVE)) {

		/* Allow Escape key to be used for cancelling a drag save
			(easier than finding somewhere safe to abort the drag) */
		ro_gui_drag_box_cancel();
		gui_current_drag_type = GUI_DRAG_NONE;
	} else if (key->c == 22 /* Ctrl-V */) {
		wimp_key *copy;

		/* Must copy the keypress as it's on the stack */
		copy = malloc(sizeof(wimp_key));
		if (copy == NULL)
			return;
		memcpy(copy, key, sizeof(wimp_key));

		ro_gui_selection_prepare_paste(key->w, ro_gui_keypress_cb, copy);
	} else if (ro_gui_wimp_event_keypress(key) == false) {
		os_error *error = xwimp_process_key(key->c);
		if (error) {
			LOG(("xwimp_process_key: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle the three User_Message events.
 */
void ro_gui_user_message(wimp_event_no event, wimp_message *message)
{
	/* attempt automatic routing */
	if (ro_message_handle_message(event, message))
		return;

	switch (message->action) {
		case message_DATA_LOAD:
			ro_msg_terminate_filename((wimp_full_message_data_xfer*)message);

			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE) {
				if (ro_print_current_window)
					ro_print_dataload_bounce(message);
			} else if (ro_gui_selection_prepare_paste_dataload(
					(wimp_full_message_data_xfer *) message) == false) {
				ro_msg_dataload(message);
			}
			break;

		case message_DATA_LOAD_ACK:
			if (ro_print_current_window)
				ro_print_cleanup();
			break;

		case message_MENU_WARNING:
			ro_gui_menu_warning((wimp_message_menu_warning *)
					&message->data);
			break;

		case message_MENUS_DELETED:
			ro_gui_menu_message_deleted((wimp_message_menus_deleted *)
					&message->data);
			break;

		case message_CLAIM_ENTITY:
			ro_gui_selection_claim_entity((wimp_full_message_claim_entity*)message);
			break;

		case message_DATA_REQUEST:
			ro_gui_selection_data_request((wimp_full_message_data_request*)message);
			break;

		case message_MODE_CHANGE:
			ro_gui_get_screen_properties();
			rufl_invalidate_cache();
			break;

		case message_PALETTE_CHANGE:
			break;

		case message_FONT_CHANGED:
			ro_gui_wimp_get_desktop_font();
			break;

		case message_URI_PROCESS:
			if (event != wimp_USER_MESSAGE_ACKNOWLEDGE)
				ro_uri_message_received(message);
			break;
		case message_URI_RETURN_RESULT:
			ro_uri_bounce(message);
			break;
		case message_INET_SUITE_OPEN_URL:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE) {
				ro_url_bounce(message);
			}
			else {
				ro_url_message_received(message);
			}
			break;
#ifdef WITH_PLUGIN
		case message_PLUG_IN_OPENING:
			plugin_opening(message);
			break;
		case message_PLUG_IN_CLOSED:
			plugin_closed(message);
			break;
		case message_PLUG_IN_RESHAPE_REQUEST:
			plugin_reshape_request(message);
			break;
		case message_PLUG_IN_FOCUS:
			break;
		case message_PLUG_IN_URL_ACCESS:
			plugin_url_access(message);
			break;
		case message_PLUG_IN_STATUS:
			plugin_status(message);
			break;
		case message_PLUG_IN_BUSY:
			break;
		case message_PLUG_IN_STREAM_NEW:
			plugin_stream_new(message);
			break;
		case message_PLUG_IN_STREAM_WRITE:
			break;
		case message_PLUG_IN_STREAM_WRITTEN:
			plugin_stream_written(message);
			break;
		case message_PLUG_IN_STREAM_DESTROY:
			break;
		case message_PLUG_IN_OPEN:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				plugin_open_msg(message);
			break;
		case message_PLUG_IN_CLOSE:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				plugin_close_msg(message);
			break;
		case message_PLUG_IN_RESHAPE:
		case message_PLUG_IN_STREAM_AS_FILE:
		case message_PLUG_IN_NOTIFY:
		case message_PLUG_IN_ABORT:
		case message_PLUG_IN_ACTION:
			break;
#endif
		case message_PRINT_SAVE:
			if (event == wimp_USER_MESSAGE_ACKNOWLEDGE)
				ro_print_save_bounce(message);
			break;
		case message_PRINT_ERROR:
			ro_print_error(message);
			break;
		case message_PRINT_TYPE_ODD:
			ro_print_type_odd(message);
			break;
		case message_HOTLIST_CHANGED:
			ro_gui_hotlist_add_cleanup();
			break;
		case message_QUIT:
			netsurf_quit = true;
			break;
	}
}


/**
 * Ensure that the filename in a data transfer message is NUL terminated
 * (some applications, especially BASIC programs use CR)
 *
 * \param  message  message to be corrected
 */

void ro_msg_terminate_filename(wimp_full_message_data_xfer *message)
{
	const char *ep = (char*)message + message->size;
	char *p = message->file_name;

	if ((size_t)message->size >= sizeof(*message))
		ep = (char*)message + sizeof(*message) - 1;

	while (p < ep && *p >= ' ') p++;
	*p = '\0';
}


/**
 * Handle Message_DataLoad (file dragged in).
 */

void ro_msg_dataload(wimp_message *message)
{
	int file_type = message->data.data_xfer.file_type;
	int tree_file_type = file_type;
	char *urltxt = NULL;
	char *title = NULL;
	struct gui_window *g;
	os_error *oserror;
	nsurl *url;
	nserror error;

	g = ro_gui_window_lookup(message->data.data_xfer.w);
	if (g) {
		if (ro_gui_window_dataload(g, message))
			return;
	}
	else {
		g = ro_gui_toolbar_lookup(message->data.data_xfer.w);
		if (g && ro_gui_toolbar_dataload(g, message))
			return;
	}

	switch (file_type) {
		case FILETYPE_ACORN_URI:
			urltxt = ro_gui_uri_file_parse(message->data.data_xfer.file_name,
					&title);
			tree_file_type = 0xfaf;
			break;
		case FILETYPE_ANT_URL:
			urltxt = ro_gui_url_file_parse(message->data.data_xfer.file_name);
			tree_file_type = 0xfaf;
			break;
		case FILETYPE_IEURL:
			urltxt = ro_gui_ieurl_file_parse(message->data.data_xfer.file_name);
			tree_file_type = 0xfaf;
			break;

		case FILETYPE_HTML:
		case FILETYPE_JNG:
		case FILETYPE_CSS:
		case FILETYPE_MNG:
		case FILETYPE_GIF:
		case FILETYPE_BMP:
		case FILETYPE_ICO:
		case osfile_TYPE_DRAW:
		case FILETYPE_PNG:
		case FILETYPE_JPEG:
		case osfile_TYPE_SPRITE:
		case osfile_TYPE_TEXT:
		case FILETYPE_ARTWORKS:
		case FILETYPE_SVG:
			/* display the actual file */
			urltxt = path_to_url(message->data.data_xfer.file_name);
			break;

		default:
			return;
	}

	if (!urltxt)
		/* error has already been reported by one of the
		 * functions called above */
		return;


	error = nsurl_create(urltxt, &url);
	if (error == NSERROR_OK) {
		if (g) {
			error = browser_window_navigate(g->bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);

#ifdef DROPURLHOTLIST /** @todo This was commented out should it be removed? */
		} else if (ro_gui_hotlist_check_window(
				message->data.data_xfer.w)) {
			/* Drop URL into hotlist */
			ro_gui_hotlist_url_drop(message, urltxt);
#endif
		} else {
			error = browser_window_create(BW_CREATE_HISTORY,
					url,
					NULL,
					NULL,
					NULL);
		}
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	free(urltxt);

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	oserror = xwimp_send_message(wimp_USER_MESSAGE, message,
			message->sender);
	if (oserror) {
		LOG(("xwimp_send_message: 0x%x: %s",
				oserror->errnum, oserror->errmess));
		warn_user("WimpError", oserror->errmess);
		return;
	}

}


/**
 * Parse an Acorn URI file.
 *
 * \param  file_name  file to read
 * \param  uri_title  pointer to receive title data, or NULL for no data
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_uri_file_parse(const char *file_name, char **uri_title)
{
	/* See the "Acorn URI Handler Functional Specification" for the
	 * definition of the URI file format. */
	char line[400];
	char *url = NULL;
	FILE *fp;

	*uri_title = NULL;
	fp = fopen(file_name, "rb");
	if (!fp) {
		LOG(("fopen(\"%s\", \"rb\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	/* "URI" */
	if (!ro_gui_uri_file_parse_line(fp, line) || strcmp(line, "URI") != 0)
		goto uri_syntax_error;

	/* version */
	if (!ro_gui_uri_file_parse_line(fp, line) ||
			strspn(line, "0123456789") != strlen(line))
		goto uri_syntax_error;

	/* URI */
	if (!ro_gui_uri_file_parse_line(fp, line))
		goto uri_syntax_error;

	url = strdup(line);
	if (!url) {
		warn_user("NoMemory", 0);
		fclose(fp);
		return 0;
	}

	/* title */
	if (!ro_gui_uri_file_parse_line(fp, line))
		goto uri_free;
	if (uri_title && line[0] && ((line[0] != '*') || line[1])) {
		*uri_title = strdup(line);
		if (!*uri_title) /* non-fatal */
			warn_user("NoMemory", 0);
	}
	fclose(fp);

	return url;

uri_free:
	free(url);

uri_syntax_error:
	fclose(fp);
	warn_user("URIError", 0);
	return 0;
}


/**
 * Read a "line" from an Acorn URI file.
 *
 * \param  fp  file pointer to read from
 * \param  b   buffer for line, size 400 bytes
 * \return  true on success, false on EOF
 */

bool ro_gui_uri_file_parse_line(FILE *fp, char *b)
{
	int c;
	unsigned int i = 0;

	c = getc(fp);
	if (c == EOF)
		return false;

	/* skip comment lines */
	while (c == '#') {
		do { c = getc(fp); } while (c != EOF && 32 <= c);
		if (c == EOF)
			return false;
		do { c = getc(fp); } while (c != EOF && c < 32);
		if (c == EOF)
			return false;
	}

	/* read "line" */
	do {
		if (i == 399)
			return false;
		b[i++] = c;
		c = getc(fp);
	} while (c != EOF && 32 <= c);

	/* skip line ending control characters */
	while (c != EOF && c < 32)
		c = getc(fp);

	if (c != EOF)
		ungetc(c, fp);

	b[i] = 0;
	return true;
}


/**
 * Parse an ANT URL file.
 *
 * \param  file_name  file to read
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_url_file_parse(const char *file_name)
{
	char line[400];
	char *url;
	FILE *fp;

	fp = fopen(file_name, "r");
	if (!fp) {
		LOG(("fopen(\"%s\", \"r\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	if (!fgets(line, sizeof line, fp)) {
		if (ferror(fp)) {
			LOG(("fgets: %i: %s",
					errno, strerror(errno)));
			warn_user("LoadError", strerror(errno));
		} else
			warn_user("LoadError", messages_get("EmptyError"));
		fclose(fp);
		return 0;
	}

	fclose(fp);

	if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = '\0';

	url = strdup(line);
	if (!url) {
		warn_user("NoMemory", 0);
		return 0;
	}

	return url;
}


/**
 * Parse an IEURL file.
 *
 * \param  file_name  file to read
 * \return  URL from file, or 0 on error and error reported
 */

char *ro_gui_ieurl_file_parse(const char *file_name)
{
	char line[400];
	char *url = 0;
	FILE *fp;

	fp = fopen(file_name, "r");
	if (!fp) {
		LOG(("fopen(\"%s\", \"r\"): %i: %s",
				file_name, errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		return 0;
	}

	while (fgets(line, sizeof line, fp)) {
		if (strncmp(line, "URL=", 4) == 0) {
			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = '\0';
			url = strdup(line + 4);
			if (!url) {
				fclose(fp);
				warn_user("NoMemory", 0);
				return 0;
			}
			break;
		}
	}
	if (ferror(fp)) {
		LOG(("fgets: %i: %s",
				errno, strerror(errno)));
		warn_user("LoadError", strerror(errno));
		fclose(fp);
		return 0;
	}

	fclose(fp);

	if (!url)
		warn_user("URIError", 0);

	return url;
}


/**
 * Handle Message_DataSave
 */

void ro_msg_datasave(wimp_message *message)
{
	wimp_full_message_data_xfer *dataxfer = (wimp_full_message_data_xfer*)message;

	/* remove ghost caret if drag-and-drop protocol was used */
//	ro_gui_selection_drag_reset();

	ro_msg_terminate_filename(dataxfer);

	if (ro_gui_selection_prepare_paste_datasave(dataxfer))
		return;

	switch (dataxfer->file_type) {
		case FILETYPE_ACORN_URI:
		case FILETYPE_ANT_URL:
		case FILETYPE_IEURL:
		case FILETYPE_HTML:
		case FILETYPE_JNG:
		case FILETYPE_CSS:
		case FILETYPE_MNG:
		case FILETYPE_GIF:
		case FILETYPE_BMP:
		case FILETYPE_ICO:
		case osfile_TYPE_DRAW:
		case FILETYPE_PNG:
		case FILETYPE_JPEG:
		case osfile_TYPE_SPRITE:
		case osfile_TYPE_TEXT:
		case FILETYPE_ARTWORKS:
		case FILETYPE_SVG: {
			os_error *error;

			dataxfer->your_ref = dataxfer->my_ref;
			dataxfer->size = offsetof(wimp_full_message_data_xfer, file_name) + 16;
			dataxfer->action = message_DATA_SAVE_ACK;
			dataxfer->est_size = -1;
			memcpy(dataxfer->file_name, "<Wimp$Scrap>", 13);

			error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)dataxfer, message->sender);
			if (error) {
				LOG(("xwimp_send_message: 0x%x: %s", error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		break;
	}
}


/**
 * Handle Message_DataSaveAck.
 */

void ro_msg_datasave_ack(wimp_message *message)
{
	ro_msg_terminate_filename((wimp_full_message_data_xfer*)message);

	if (ro_print_ack(message))
		return;

	switch (gui_current_drag_type) {
		case GUI_DRAG_DOWNLOAD_SAVE:
			ro_gui_download_datasave_ack(message);
			break;

		case GUI_DRAG_SAVE:
			ro_gui_save_datasave_ack(message);
			gui_current_drag_type = GUI_DRAG_NONE;
			break;

		default:
			break;
	}
	
	gui_current_drag_type = GUI_DRAG_NONE;
}


/**
 * Handle Message_DataOpen (double-click on file in the Filer).
 */

void ro_msg_dataopen(wimp_message *message)
{
	int file_type = message->data.data_xfer.file_type;
	char *url = 0;
	size_t len;
	os_error *oserror;
	nsurl *urlns;
	nserror error;

	if (file_type == 0xb28)			/* ANT URL file */
		url = ro_gui_url_file_parse(message->data.data_xfer.file_name);
	else if (file_type == 0xfaf)		/* HTML file */
		url = path_to_url(message->data.data_xfer.file_name);
	else if (file_type == 0x1ba)		/* IEURL file */
		url = ro_gui_ieurl_file_parse(message->
				data.data_xfer.file_name);
	else if (file_type == 0x2000) {		/* application */
		len = strlen(message->data.data_xfer.file_name);
		if (len < 9 || strcmp(".!NetSurf",
				message->data.data_xfer.file_name + len - 9))
			return;
		if (nsoption_charp(homepage_url) &&
					nsoption_charp(homepage_url)[0]) {
			url = strdup(nsoption_charp(homepage_url));
		} else {
			url = strdup(NETSURF_HOMEPAGE);
		}
		if (!url)
			warn_user("NoMemory", 0);
	} else
		return;

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	oserror = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
	if (oserror) {
		LOG(("xwimp_send_message: 0x%x: %s",
				oserror->errnum, oserror->errmess));
		warn_user("WimpError", oserror->errmess);
		return;
	}

	if (!url)
		/* error has already been reported by one of the
		 * functions called above */
		return;

	error = nsurl_create(url, &urlns);
	free(url);
	if (error == NSERROR_OK) {
	/* create a new window with the file */
		error = browser_window_create(BW_CREATE_HISTORY,
					      urlns,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(urlns);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}


/**
 * Handle PreQuit message
 *
 * \param  message  PreQuit message from Wimp
 */

void ro_msg_prequit(wimp_message *message)
{
	if (!ro_gui_prequit()) {
		os_error *error;

		/* we're objecting to the close down */
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE,
						message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x:%s", error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle SaveDesktop message
 *
 * \param  message  SaveDesktop message from Wimp
 */

void ro_msg_save_desktop(wimp_message *message)
{
	os_error *error;

	error = xosgbpb_writew(message->data.save_desktopw.file,
				(const byte*)"Run ", 4, NULL);
	if (!error) {
		error = xosgbpb_writew(message->data.save_desktopw.file,
					(const byte*)NETSURF_DIR, strlen(NETSURF_DIR), NULL);
		if (!error)
			error = xos_bputw('\n', message->data.save_desktopw.file);
	}

	if (error) {
		LOG(("xosgbpb_writew/xos_bputw: 0x%x:%s", error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);

		/* we must cancel the save by acknowledging the message */
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE,
						message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x:%s", error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle WindowInfo message (part of the iconising protocol)
 *
 * \param  message  WindowInfo message from the Iconiser
 */

void ro_msg_window_info(wimp_message *message)
{
	wimp_full_message_window_info *wi;
	struct gui_window *g;

	/* allow the user to turn off thumbnail icons */
	if (!nsoption_bool(thumbnail_iconise))
		return;

	wi = (wimp_full_message_window_info*)message;
	g = ro_gui_window_lookup(wi->w);

	/* ic_<task name> will suffice for our other windows */
	if (g) {
		ro_gui_window_iconise(g, wi);
		ro_gui_dialog_close_persistent(wi->w);
	}
}




/**
 * Get screen properties following a mode change.
 */

void ro_gui_get_screen_properties(void)
{
	static const ns_os_vdu_var_list vars = {
		os_MODEVAR_XWIND_LIMIT,
		{
			os_MODEVAR_YWIND_LIMIT,
			os_MODEVAR_XEIG_FACTOR,
			os_MODEVAR_YEIG_FACTOR,
			os_VDUVAR_END_LIST
		}
	};
	os_error *error;
	int vals[4];

	error = xos_read_vdu_variables(PTR_OS_VDU_VAR_LIST(&vars), vals);
	if (error) {
		LOG(("xos_read_vdu_variables: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return;
	}
	screen_info.width  = (vals[0] + 1) << vals[2];
	screen_info.height = (vals[1] + 1) << vals[3];
}


/**
 * Find screen size in OS units.
 */

void ro_gui_screen_size(int *width, int *height)
{
	*width = screen_info.width;
	*height = screen_info.height;
}


/**
 * Send the source of a content to a text editor.
 */

void ro_gui_view_source(hlcache_handle *c)
{
	os_error *error;
	char full_name[256];
	char *temp_name, *r;
	wimp_full_message_data_xfer message;
	int objtype;
	bool done = false;

	const char *source_data;
	unsigned long source_size;

	if (!c) {
		warn_user("MiscError", "No document source");
		return;
	}

	source_data = content_get_source_data(c, &source_size);

	if (!source_data) {
		warn_user("MiscError", "No document source");
		return;
	}

	/* try to load local files directly. */
	temp_name = url_to_path(nsurl_access(hlcache_handle_get_url(c)));
	if (temp_name) {
		error = xosfile_read_no_path(temp_name, &objtype, 0, 0, 0, 0);
		if ((!error) && (objtype == osfile_IS_FILE)) {
			snprintf(message.file_name, 212, "%s", temp_name);
			message.file_name[211] = '\0';
			done = true;
		}
		free(temp_name);
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

		snprintf(full_name, 256, "%s/%s", TEMP_FILENAME_PREFIX,
				filename);
		full_name[255] = '\0';
		r = __riscosify(full_name, 0, __RISCOSIFY_NO_SUFFIX,
				message.file_name, 212, 0);
		if (r == 0) {
			LOG(("__riscosify failed"));
			return;
		}
		message.file_name[211] = '\0';

		error = xosfile_save_stamped(message.file_name,
				ro_content_filetype(c),
				(byte *) source_data,
				(byte *) source_data + source_size);
		if (error) {
			LOG(("xosfile_save_stamped failed: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			return;
		}
	}

	/* begin the DataOpen protocol */
	message.your_ref = 0;
	message.size = 44 + ((strlen(message.file_name) + 4) & (~3u));
	message.action = message_DATA_OPEN;
	message.w = 0;
	message.i = 0;
	message.pos.x = 0;
	message.pos.y = 0;
	message.est_size = 0;
	message.file_type = 0xfff;
	ro_message_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message*)&message, 0,
			ro_gui_view_source_bounce);
}


void ro_gui_view_source_bounce(wimp_message *message)
{
	char *filename;
	os_error *error;
	char command[256];

	/* run the file as text */
	filename = ((wimp_full_message_data_xfer *)message)->file_name;
	sprintf(command, "@RunType_FFF %s", filename);
	error = xwimp_start_task(command, 0);
	if (error) {
		LOG(("xwimp_start_task failed: 0x%x: %s",
					error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Send the debug dump of a content to a text editor.
 */

void ro_gui_dump_browser_window(struct browser_window *bw)
{
	os_error *error;

	/* open file for dump */
	FILE *stream = fopen("<Wimp$ScrapDir>.WWW.NetSurf.dump", "w");
	if (!stream) {
		LOG(("fopen: errno %i", errno));
		warn_user("SaveError", strerror(errno));
		return;
	}

	browser_window_debug_dump(bw, stream);

	fclose(stream);

	/* launch file in editor */
	error = xwimp_start_task("Filer_Run <Wimp$ScrapDir>.WWW.NetSurf.dump",
			0);
	if (error) {
		LOG(("xwimp_start_task failed: 0x%x: %s",
					error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Broadcast an URL that we can't handle.
 */

static void gui_launch_url(const char *url)
{
	/* Try ant broadcast first */
	ro_url_broadcast(url);
}


/**
 * Display a warning for a serious problem (eg memory exhaustion).
 *
 * \param  warning  message key for warning message
 * \param  detail   additional message, or 0
 */

void warn_user(const char *warning, const char *detail)
{
	LOG(("%s %s", warning, detail));

	if (dialog_warning) {
		char warn_buffer[300];
		snprintf(warn_buffer, sizeof warn_buffer, "%s %s",
				messages_get(warning),
				detail ? detail : "");
		warn_buffer[sizeof warn_buffer - 1] = 0;
		ro_gui_set_icon_string(dialog_warning, ICON_WARNING_MESSAGE,
				warn_buffer, true);
		xwimp_set_icon_state(dialog_warning, ICON_WARNING_HELP,
				wimp_ICON_DELETED, wimp_ICON_DELETED);
		ro_gui_dialog_open(dialog_warning);
		xos_bell();
	}
	else {
		/* probably haven't initialised (properly), use a
		   non-multitasking error box */
		os_error error;
		snprintf(error.errmess, sizeof error.errmess, "%s %s",
				messages_get(warning),
				detail ? detail : "");
		error.errmess[sizeof error.errmess - 1] = 0;
		xwimp_report_error_by_category(&error,
				wimp_ERROR_BOX_OK_ICON |
				wimp_ERROR_BOX_GIVEN_CATEGORY |
				wimp_ERROR_BOX_CATEGORY_ERROR <<
					wimp_ERROR_BOX_CATEGORY_SHIFT,
				"NetSurf", "!netsurf",
				(osspriteop_area *) 1, 0, 0);
	}
}


/**
 * Display an error and exit.
 *
 * Should only be used during initialisation.
 */

void die(const char * const error)
{
	os_error warn_error;

	LOG(("%s", error));

	warn_error.errnum = 1; /* \todo: reasonable ? */
	strncpy(warn_error.errmess, messages_get(error),
			sizeof(warn_error.errmess)-1);
	warn_error.errmess[sizeof(warn_error.errmess)-1] = '\0';
	xwimp_report_error_by_category(&warn_error,
			wimp_ERROR_BOX_OK_ICON |
			wimp_ERROR_BOX_GIVEN_CATEGORY |
			wimp_ERROR_BOX_CATEGORY_ERROR <<
				wimp_ERROR_BOX_CATEGORY_SHIFT,
			"NetSurf", "!netsurf",
			(osspriteop_area *) 1, 0, 0);
	exit(EXIT_FAILURE);
}


/**
 * Test whether it's okay to shutdown, prompting the user if not.
 *
 * \return true iff it's okay to shutdown immediately
 */

bool ro_gui_prequit(void)
{
	return ro_gui_download_prequit();
}

void PDF_Password(char **owner_pass, char **user_pass, char *path)
{
	/*TODO:this waits to be written, until then no PDF encryption*/
	*owner_pass = NULL;
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
	char *temp;
	int leaflen;

	temp = strrchr(path, '.');
	if (!temp)
		temp = path; /* already leafname */
	else
		temp += 1;

	leaflen = strlen(temp);

	leafname = malloc(leaflen + 1);
	if (!leafname) {
		LOG(("malloc failed"));
		return NULL;
	}
	memcpy(leafname, temp, leaflen + 1);

	/* and s/\//\./g */
	for (temp = leafname; *temp; temp++)
		if (*temp == '/')
			*temp = '.';

	return leafname;
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing platform-native format path + free space
 * \param length length of buffer "path"
 * \param newpart string containing unix-format path component to add to path
 * \return true on success
 */

static bool path_add_part(char *path, int length, const char *newpart)
{
	size_t path_len = strlen(path);

	/* Append directory separator, if there isn't one */
	if (path[path_len - 1] != '.') {
		strncat(path, ".", length);
		path_len += 1;
	}

	strncat(path, newpart, length);

	/* Newpart is either a directory name, or a file leafname
 	 * Either way, we must replace all dots with forward slashes */
	for (path = path + path_len; *path; path++) {
		if (*path == '.')
			*path = '/';
	}

	return true;
}


static struct gui_fetch_table riscos_fetch_table = {
	.filename_from_path = filename_from_path,
	.path_add_part = path_add_part,
	.filetype = fetch_filetype,
	.path_to_url = path_to_url,
	.url_to_path = url_to_path,

	.get_resource_url = gui_get_resource_url,
	.mimetype = fetch_mimetype,
};

static struct gui_browser_table riscos_browser_table = {
	.poll = riscos_poll,
	.schedule = riscos_schedule,

	.quit = gui_quit,
	.launch_url = gui_launch_url,
	.create_form_select_menu = gui_create_form_select_menu,
	.cert_verify = gui_cert_verify,
	.login = gui_401login_open,
};


/** Normal entry point from OS */
int main(int argc, char** argv)
{
	char path[40];
	int length;
	char logging_env[2];
	os_var_type type;
	int used = -1;  /* slightly better with older OSLib versions */
	os_error *error;
	nserror ret;
	struct gui_table riscos_gui_table = {
		.browser = &riscos_browser_table,
		.window = riscos_window_table,
		.clipboard = riscos_clipboard_table,
		.download = riscos_download_table,
		.fetch = &riscos_fetch_table,
		.utf8 = riscos_utf8_table,
		.search = riscos_search_table,
	};

	/* Consult NetSurf$Logging environment variable to decide if logging
	 * is required. */
	error = xos_read_var_val_size("NetSurf$Logging", 0, os_VARTYPE_STRING,
			&used, NULL, &type);
	if (error != NULL || type != os_VARTYPE_STRING || used != -2) {
		verbose_log = true;
	} else {
		error = xos_read_var_val("NetSurf$Logging", logging_env,
				sizeof(logging_env), 0, os_VARTYPE_STRING,
				&used, NULL, &type);
		if (error != NULL || logging_env[0] != '0') {
			verbose_log = true;
		} else {
			verbose_log = false;
		}
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
	nsoption_read("NetSurf:Choices", NULL);
	nsoption_commandline(&argc, argv, NULL);

	/* Choose the interface language to use */
	ro_gui_choose_language();

	/* select language-specific Messages */
	if (((length = snprintf(path,
				sizeof(path),
			       "NetSurf:Resources.%s.Messages",
				nsoption_charp(language))) < 0) || 
	    (length >= (int)sizeof(path))) {
		die("Failed to locate Messages resource.");
	}

	/* common initialisation */
	ret = netsurf_init(path, &riscos_gui_table);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	artworks_init();
	draw_init();
	sprite_init();

	/* Load some extra RISC OS specific Messages */
	messages_load("NetSurf:Resources.LangNames");

	gui_init(argc, argv);

	gui_init2(argc, argv);

	netsurf_main_loop();

	netsurf_exit();

	return 0;
}
