/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#import <Cocoa/Cocoa.h>

extern struct gui_window_table *cocoa_window_table;
extern struct gui_clipboard_table *cocoa_clipboard_table;
extern struct gui_browser_table *cocoa_browser_table;

extern NSString * const kCookiesFileOption;
extern NSString * const kURLsFileOption;
extern NSString * const kHotlistFileOption;
extern NSString * const kHomepageURLOption;
extern NSString * const kOptionsFileOption;
extern NSString * const kAlwaysCancelDownload;
extern NSString * const kAlwaysCloseMultipleTabs;

void cocoa_autorelease( void );
