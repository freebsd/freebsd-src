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

@class PSMTabBarControl;
@class BrowserViewController;
@class URLFieldCell;

@interface BrowserWindowController : NSWindowController {
	PSMTabBarControl *tabBar;
	NSTabView *tabView;
	URLFieldCell *urlField;
	NSObjectController *activeBrowserController;
	NSSegmentedControl *navigationControl;
	NSButton *historyButton;
	BrowserViewController *activeBrowser;
	NSMenu *historyBackMenu;
	NSMenu *historyForwardMenu;
}

@property (readwrite, assign, nonatomic) IBOutlet PSMTabBarControl *tabBar;
@property (readwrite, assign, nonatomic) IBOutlet NSTabView *tabView;
@property (readwrite, assign, nonatomic) IBOutlet URLFieldCell *urlField;
@property (readwrite, assign, nonatomic) IBOutlet NSObjectController *activeBrowserController;
@property (readwrite, assign, nonatomic) IBOutlet NSSegmentedControl *navigationControl;
@property (readwrite, assign, nonatomic) IBOutlet NSButton *historyButton;
@property (readwrite, assign, nonatomic) IBOutlet NSMenu *historyBackMenu;
@property (readwrite, assign, nonatomic) IBOutlet NSMenu *historyForwardMenu;

@property (readwrite, assign, nonatomic) BrowserViewController *activeBrowser;

@property (readwrite, assign, nonatomic) BOOL canGoBack;
@property (readwrite, assign, nonatomic) BOOL canGoForward;

- (IBAction) newTab: (id) sender;
- (IBAction) closeCurrentTab: (id) sender;

- (void) addTab: (BrowserViewController *)browser;
- (void) removeTab: (BrowserViewController *)browser;

@end
