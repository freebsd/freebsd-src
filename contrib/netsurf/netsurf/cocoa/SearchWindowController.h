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

@class BrowserViewController;

typedef enum {
	SearchBackward,
	SearchForward
} SearchDirection;

@interface SearchWindowController : NSWindowController {
	BOOL caseSensitive;
	BOOL selectAll;
	BOOL canGoBack;
	BOOL canGoForward;
	NSString *searchString;
	BrowserViewController *browser;
}

@property (readwrite, assign, nonatomic) BOOL caseSensitive;
@property (readwrite, assign, nonatomic) BOOL selectAll;
@property (readwrite, assign, nonatomic) BOOL canGoBack;
@property (readwrite, assign, nonatomic) BOOL canGoForward;
@property (readwrite, copy, nonatomic) NSString *searchString;
@property (readwrite, assign, nonatomic) BrowserViewController *browser;

- (IBAction) searchNext: (id) sender;
- (IBAction) searchPrevious: (id) sender;

- (IBAction) searchStringDidChange: (id) sender;

- (void) search: (SearchDirection)direction;

struct gui_search_table *cocoa_search_table;

@end
