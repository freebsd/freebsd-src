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

struct browser_window;
@class BrowserView;
@class BrowserWindowController;

@interface BrowserViewController : NSViewController {
	struct browser_window *browser;
	NSString *url;
	BrowserView *browserView;
	BrowserWindowController *windowController;
	NSString *title;
	NSString *status;
	BOOL isProcessing;
	NSImage *favicon;
	BOOL canGoBack;
	BOOL canGoForward;
}

@property (readwrite, assign, nonatomic) struct browser_window *browser;
@property (readwrite, copy, nonatomic) NSString *url;
@property (readwrite, assign, nonatomic) IBOutlet BrowserView *browserView;
@property (readwrite, retain, nonatomic) BrowserWindowController *windowController;
@property (readwrite, copy, nonatomic) NSString *title;
@property (readwrite, copy, nonatomic) NSString *status;
@property (readwrite, assign, nonatomic) BOOL isProcessing;
@property (readwrite, copy, nonatomic) NSImage *favicon;
@property (readwrite, assign, nonatomic) BOOL canGoBack;
@property (readwrite, assign, nonatomic) BOOL canGoForward;

- initWithBrowser: (struct browser_window *) bw;

- (void) contentUpdated;
- (void) updateBackForward;

- (IBAction) navigate: (id) sender;

- (IBAction) backForwardSelected: (id) sender;

- (IBAction) goHome: (id) sender;

- (IBAction) goBack: (id) sender;
- (IBAction) goForward: (id) sender;
- (IBAction) reloadPage: (id) sender;
- (IBAction) stopLoading: (id) sender;

- (IBAction) zoomIn: (id) sender;
- (IBAction) zoomOut: (id) sender;
- (IBAction) zoomOriginal: (id) sender;

- (IBAction) viewSource: (id) sender;

- (void) buildBackMenu: (NSMenu *)menu;
- (void) buildForwardMenu: (NSMenu *)menu;

@end
