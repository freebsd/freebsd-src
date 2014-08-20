/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * Option available on all platforms
 *
 * Non-platform specific options can be added by editing this file 
 *
 * Platform specific options should be added in the platform options.h.
 *
 * This header is specificaly intented to be included multiple times
 *   with different macro definitions so there is no guard
 */

#ifndef _NETSURF_DESKTOP_OPTIONS_H_
#define _NETSURF_DESKTOP_OPTIONS_H_

#include "desktop/plot_style.h"

/* defines for system colour table */
#define NSOPTION_SYS_COLOUR_START NSOPTION_sys_colour_ActiveBorder
#define NSOPTION_SYS_COLOUR_END NSOPTION_sys_colour_WindowText

#endif

/** An HTTP proxy should be used. */
NSOPTION_BOOL(http_proxy, false) 

/** Hostname of proxy. */
NSOPTION_STRING(http_proxy_host, NULL) 

/** Proxy port. */
NSOPTION_INTEGER(http_proxy_port, 8080) 

/** Proxy authentication method. */
NSOPTION_INTEGER(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE) 

/** Proxy authentication user name */
NSOPTION_STRING(http_proxy_auth_user, NULL)

/** Proxy authentication password */
NSOPTION_STRING(http_proxy_auth_pass, NULL)

/** Proxy omission list */
NSOPTION_STRING(http_proxy_noproxy, "localhost")

/** Default font size / 0.1pt. */
NSOPTION_INTEGER(font_size, 128)

/** Minimum font size. */
NSOPTION_INTEGER(font_min_size, 85)

/** Default sans serif font */
NSOPTION_STRING(font_sans, NULL)
/** Default serif font */
NSOPTION_STRING(font_serif, NULL)

/** Default monospace font */
NSOPTION_STRING(font_mono, NULL)

/** Default cursive font */
NSOPTION_STRING(font_cursive, NULL)

/** Default fantasy font */
NSOPTION_STRING(font_fantasy, NULL)

/** Accept-Language header. */
NSOPTION_STRING(accept_language, NULL)

/** Accept-Charset header. */
NSOPTION_STRING(accept_charset, NULL)

/** Preferred maximum size of memory cache / bytes. */
NSOPTION_INTEGER(memory_cache_size, 12 * 1024 * 1024)

/** Preferred expiry size of disc cache / bytes. */
NSOPTION_INTEGER(disc_cache_size, 1024 * 1024 * 1024)

/** Preferred expiry age of disc cache / days. */
NSOPTION_INTEGER(disc_cache_age, 28)

/** Whether to block advertisements */
NSOPTION_BOOL(block_advertisements, false)

/** Disable website tracking, see	                
 * http://www.w3.org/Submission/2011/SUBM-web-tracking-protection-20110224/#dnt-uas */
NSOPTION_BOOL(do_not_track, false)

/** Minimum GIF animation delay */
NSOPTION_INTEGER(minimum_gif_delay, 10)

/** Whether to send the referer HTTP header */
NSOPTION_BOOL(send_referer, true)

/** Whether to fetch foreground images */
NSOPTION_BOOL(foreground_images, true)

/** Whether to fetch background images */
NSOPTION_BOOL(background_images, true)

/** Whether to animate images */
NSOPTION_BOOL(animate_images, true)

/** Whether to execute javascript */
NSOPTION_BOOL(enable_javascript, false)

/** Maximum time (in seconds) to wait for a script to run */
NSOPTION_INTEGER(script_timeout, 10)

/** How many days to retain URL data for */
NSOPTION_INTEGER(expire_url, 28)

/** Default font family */
NSOPTION_INTEGER(font_default, PLOT_FONT_FAMILY_SANS_SERIF)

/** ca-bundle location */
NSOPTION_STRING(ca_bundle, NULL)

/** ca-path location */
NSOPTION_STRING(ca_path, NULL)

/** Cookie file location */
NSOPTION_STRING(cookie_file, NULL)

/** Cookie jar location */
NSOPTION_STRING(cookie_jar, NULL)

/** Home page location */
NSOPTION_STRING(homepage_url, NULL)

/** search web from url bar */
NSOPTION_BOOL(search_url_bar, false)

/** default web search provider */
NSOPTION_INTEGER(search_provider, 0)

/** URL completion in url bar */
NSOPTION_BOOL(url_suggestion, true)

/** default x position of new windows */
NSOPTION_INTEGER(window_x, 0)

/** default y position of new windows */
NSOPTION_INTEGER(window_y, 0)

/** default width of new windows */
NSOPTION_INTEGER(window_width, 0)

/** default height of new windows */
NSOPTION_INTEGER(window_height, 0)

/** width of screen when above options were saved */
NSOPTION_INTEGER(window_screen_width, 0)

/** height of screen when above options were saved */
NSOPTION_INTEGER(window_screen_height, 0)

/** default size of status bar vs. h scroll bar */
NSOPTION_INTEGER(toolbar_status_size, 6667)

/** default window scale */
NSOPTION_INTEGER(scale, 100)

/* Whether to reflow web pages while objects are fetching */
NSOPTION_BOOL(incremental_reflow, true)

/* Minimum time (in cs) between HTML reflows while objects are fetching */
NSOPTION_UINT(min_reflow_period, DEFAULT_REFLOW_PERIOD)

/* use core selection menu */
NSOPTION_BOOL(core_select_menu, false)

/******** Fetcher options ********/

/** Maximum simultaneous active fetchers */
NSOPTION_INTEGER(max_fetchers, 24)

/** Maximum simultaneous active fetchers per host.
 * (<=option_max_fetchers else it makes no sense) Note that rfc2616
 * section 8.1.4 says that there should be no more than two keepalive
 * connections per host. None of the main browsers follow this as it
 * slows page fetches down considerably.  See
 * https://bugzilla.mozilla.org/show_bug.cgi?id=423377#c4
 */
NSOPTION_INTEGER(max_fetchers_per_host, 5)

/** Maximum number of inactive fetchers cached.  The total number of
 * handles netsurf will therefore have open is this plus
 * option_max_fetchers.
 */
NSOPTION_INTEGER(max_cached_fetch_handles, 6)

/** Suppress debug output from cURL. */
NSOPTION_BOOL(suppress_curl_debug, true)

/** Whether to allow target="_blank" */
NSOPTION_BOOL(target_blank, true)

/** Whether second mouse button opens in new tab */
NSOPTION_BOOL(button_2_tab, true)

/******** PDF / Print options ********/

/** top margin of exported page */
NSOPTION_INTEGER(margin_top, DEFAULT_MARGIN_TOP_MM)

/** bottom margin of exported page */
NSOPTION_INTEGER(margin_bottom, DEFAULT_MARGIN_BOTTOM_MM)

/** left margin of exported page */
NSOPTION_INTEGER(margin_left, DEFAULT_MARGIN_LEFT_MM)

/** right margin of exported page */
NSOPTION_INTEGER(margin_right, DEFAULT_MARGIN_RIGHT_MM)

/** scale of exported content */
NSOPTION_INTEGER(export_scale, DEFAULT_EXPORT_SCALE * 100)

/** suppressing images in printed content */
NSOPTION_BOOL(suppress_images, false)

/** turning off all backgrounds for printed content */
NSOPTION_BOOL(remove_backgrounds, false)

/** turning on content loosening for printed content */
NSOPTION_BOOL(enable_loosening, true)

/** compression of PDF documents */
NSOPTION_BOOL(enable_PDF_compression, true)

/** setting a password and encoding PDF documents */
NSOPTION_BOOL(enable_PDF_password, false)

/******** System colours ********/
NSOPTION_COLOUR(sys_colour_ActiveBorder, 0x00d3d3d3)
NSOPTION_COLOUR(sys_colour_ActiveCaption, 0x00f1f1f1)
NSOPTION_COLOUR(sys_colour_AppWorkspace, 0x00f1f1f1)
NSOPTION_COLOUR(sys_colour_Background, 0x006e6e6e)
NSOPTION_COLOUR(sys_colour_ButtonFace, 0x00f9f9f9)
NSOPTION_COLOUR(sys_colour_ButtonHighlight, 0x00ffffff)
NSOPTION_COLOUR(sys_colour_ButtonShadow, 0x00aeaeae)
NSOPTION_COLOUR(sys_colour_ButtonText, 0x004c4c4c)
NSOPTION_COLOUR(sys_colour_CaptionText, 0x004c4c4c)
NSOPTION_COLOUR(sys_colour_GrayText, 0x00505050)
NSOPTION_COLOUR(sys_colour_Highlight, 0x00c00800)
NSOPTION_COLOUR(sys_colour_HighlightText, 0x00ffffff)
NSOPTION_COLOUR(sys_colour_InactiveBorder, 0x00f1f1f1)
NSOPTION_COLOUR(sys_colour_InactiveCaption, 0x00e6e6e6)
NSOPTION_COLOUR(sys_colour_InactiveCaptionText, 0x00a6a6a6)
NSOPTION_COLOUR(sys_colour_InfoBackground, 0x008fdfef)
NSOPTION_COLOUR(sys_colour_InfoText, 0x00000000)
NSOPTION_COLOUR(sys_colour_Menu, 0x00f1f1f1)
NSOPTION_COLOUR(sys_colour_MenuText, 0x004e4e4e)
NSOPTION_COLOUR(sys_colour_Scrollbar, 0x00cccccc)
NSOPTION_COLOUR(sys_colour_ThreeDDarkShadow, 0x00aeaeae)
NSOPTION_COLOUR(sys_colour_ThreeDFace, 0x00f9f9f9)
NSOPTION_COLOUR(sys_colour_ThreeDHighlight, 0x00ffffff)
NSOPTION_COLOUR(sys_colour_ThreeDLightShadow, 0x00ffffff)
NSOPTION_COLOUR(sys_colour_ThreeDShadow, 0x00d5d5d5)
NSOPTION_COLOUR(sys_colour_Window, 0x00f1f1f1)
NSOPTION_COLOUR(sys_colour_WindowFrame, 0x004e4e4e)
NSOPTION_COLOUR(sys_colour_WindowText, 0x00000000)
