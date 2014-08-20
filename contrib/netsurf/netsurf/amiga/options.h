/*
 * Copyright 2008 - 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_OPTIONS_H
#define AMIGA_OPTIONS_H

/* currently nothing here */

#endif



NSOPTION_STRING(url_file, NULL)
NSOPTION_STRING(hotlist_file, NULL)
NSOPTION_STRING(pubscreen_name, NULL)
NSOPTION_STRING(screen_modeid, NULL)
NSOPTION_INTEGER(screen_compositing, -1)
NSOPTION_INTEGER(screen_ydpi, 85)
NSOPTION_INTEGER(cache_bitmaps, 0)
NSOPTION_STRING(theme, "PROGDIR:Resources/Themes/Default")
NSOPTION_BOOL(clipboard_write_utf8, false)
NSOPTION_BOOL(context_menu, true)
NSOPTION_BOOL(truecolour_mouse_pointers, false)
NSOPTION_BOOL(os_mouse_pointers, true)
NSOPTION_BOOL(use_openurl_lib, false)
NSOPTION_BOOL(new_tab_is_active, false)
NSOPTION_BOOL(new_tab_last, false)
NSOPTION_BOOL(tab_close_warn, true)
NSOPTION_BOOL(tab_always_show, false)
NSOPTION_BOOL(kiosk_mode, false)
NSOPTION_STRING(search_engines_file, "PROGDIR:Resources/SearchEngines")
NSOPTION_STRING(arexx_dir, "PROGDIR:Rexx")
NSOPTION_STRING(arexx_startup, "Startup.nsrx")
NSOPTION_STRING(arexx_shutdown, "Shutdown.nsrx")
NSOPTION_STRING(download_dir, NULL)
NSOPTION_BOOL(download_notify, true)
NSOPTION_BOOL(faster_scroll, true)
NSOPTION_BOOL(scale_quality, false)
NSOPTION_INTEGER(dither_quality, 1)
NSOPTION_INTEGER(mask_alpha, 50)
NSOPTION_BOOL(ask_overwrite, true)
NSOPTION_INTEGER(printer_unit, 0)
NSOPTION_INTEGER(print_scale, 100)
NSOPTION_BOOL(startup_no_window, false)
NSOPTION_BOOL(close_no_quit, false)
NSOPTION_BOOL(hide_docky_icon, false)
NSOPTION_STRING(font_unicode, NULL)
NSOPTION_STRING(font_unicode_file, NULL)
NSOPTION_BOOL(font_unicode_only, false)
NSOPTION_BOOL(font_antialiasing, true)
NSOPTION_BOOL(drag_save_icons, true)
NSOPTION_INTEGER(hotlist_window_xpos, 0)
NSOPTION_INTEGER(hotlist_window_ypos, 0)
NSOPTION_INTEGER(hotlist_window_xsize, 0)
NSOPTION_INTEGER(hotlist_window_ysize, 0)
NSOPTION_INTEGER(history_window_xpos, 0)
NSOPTION_INTEGER(history_window_ypos, 0)
NSOPTION_INTEGER(history_window_xsize, 0)
NSOPTION_INTEGER(history_window_ysize, 0)
NSOPTION_INTEGER(cookies_window_xpos, 0)
NSOPTION_INTEGER(cookies_window_ypos, 0)
NSOPTION_INTEGER(cookies_window_xsize, 0)
NSOPTION_INTEGER(cookies_window_ysize, 0)
NSOPTION_INTEGER(web_search_width, 0)
NSOPTION_INTEGER(cairo_renderer, 0)
NSOPTION_BOOL(direct_render, false)
NSOPTION_BOOL(window_simple_refresh, false)
NSOPTION_BOOL(resize_with_contents, false)
NSOPTION_INTEGER(reformat_delay, 0)
NSOPTION_INTEGER(redraw_tile_size_x, 0)
NSOPTION_INTEGER(redraw_tile_size_y, 0)
NSOPTION_INTEGER(monitor_aspect_x, 0)
NSOPTION_INTEGER(monitor_aspect_y, 0)
NSOPTION_BOOL(accept_lang_locale, true)
NSOPTION_INTEGER(menu_refresh, 0)


