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

#import "cocoa/BrowserWindowController.h"

#import "cocoa/BrowserViewController.h"
#import "cocoa/PSMTabBarControl/PSMTabBarControl.h"
#import "cocoa/PSMTabBarControl/PSMRolloverButton.h"
#import "cocoa/URLFieldCell.h"
#import "cocoa/gui.h"
#import "cocoa/NetsurfApp.h"

#import "desktop/browser.h"
#import "utils/nsoption.h"
#import "utils/messages.h"
#import "utils/utils.h"

@interface BrowserWindowController ()

- (void) canCloseAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end


@implementation BrowserWindowController

@synthesize tabBar;
@synthesize tabView;
@synthesize urlField;
@synthesize navigationControl;
@synthesize historyButton;
@synthesize historyBackMenu;
@synthesize historyForwardMenu;

@synthesize activeBrowser;
@synthesize activeBrowserController;

- (id) init;
{
	if (nil == (self = [super initWithWindowNibName: @"BrowserWindow"])) return nil;
	
	return self;
}

- (void) dealloc;
{
	[self setTabBar: nil];
	[self setTabView: nil];
	[self setUrlField: nil];
	[self setNavigationControl: nil];
	
	[super dealloc];
}

- (void) awakeFromNib;
{
	[tabBar setShowAddTabButton: YES];
	[tabBar setTearOffStyle: PSMTabBarTearOffMiniwindow];
	[tabBar setCanCloseOnlyTab: YES];
	[tabBar setHideForSingleTab: YES];
	
	NSButton *b = [tabBar addTabButton];
	[b setTarget: self];
	[b setAction: @selector(newTab:)];
	
	[urlField setRefreshAction: @selector(reloadPage:)];
	[urlField bind: @"favicon" toObject: activeBrowserController withKeyPath: @"selection.favicon"  options: nil];
	
	[self bind: @"canGoBack" 
	  toObject: activeBrowserController withKeyPath: @"selection.canGoBack" 
	   options: nil];
	[self bind: @"canGoForward" 
	  toObject: activeBrowserController withKeyPath: @"selection.canGoForward" 
	   options: nil];
	
	[navigationControl setMenu: historyBackMenu forSegment: 0];
	[navigationControl setMenu: historyForwardMenu forSegment: 1];
}

- (void) addTab: (BrowserViewController *)browser;
{
	NSTabViewItem *item = [[[NSTabViewItem alloc] initWithIdentifier: browser] autorelease];
	
	[item setView: [browser view]];
	[item bind: @"label" toObject: browser withKeyPath: @"title" options: nil];
	
	[tabView addTabViewItem: item];
	[browser setWindowController: self];
	
	[tabView selectTabViewItem: item];
}

- (void) removeTab: (BrowserViewController *)browser;
{
	NSUInteger itemIndex = [tabView indexOfTabViewItemWithIdentifier: browser];
	if (itemIndex != NSNotFound) {
		NSTabViewItem *item = [tabView tabViewItemAtIndex: itemIndex];
		[tabView removeTabViewItem: item];
		[browser setWindowController: nil];
	}
}

- (BOOL) windowShouldClose: (NSWindow *) window;
{
	if ([tabView numberOfTabViewItems] <= 1) return YES;
	if ([[NSUserDefaults standardUserDefaults] boolForKey: kAlwaysCloseMultipleTabs]) return YES;
	
	NSAlert *ask = [NSAlert alertWithMessageText: NSLocalizedString( @"Do you really want to close this window?", nil )
								   defaultButton: NSLocalizedString( @"Yes", @"'Yes' button" )
								 alternateButton: NSLocalizedString( @"No" , @"'No' button" )
									 otherButton:nil 
					   informativeTextWithFormat: NSLocalizedString( @"There are %d tabs open, do you want to close them all?", nil ),
							[tabView numberOfTabViewItems]];
	[ask setShowsSuppressionButton:YES];
	
	[ask beginSheetModalForWindow: window modalDelegate:self didEndSelector:@selector(canCloseAlertDidEnd:returnCode:contextInfo:) 
					  contextInfo: NULL];

	return NO;
}

- (void) canCloseAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;
{
	if (returnCode == NSOKButton) {
		[[NSUserDefaults standardUserDefaults] setBool: [[alert suppressionButton] state] == NSOnState 
												forKey: kAlwaysCloseMultipleTabs];
		[[self window] close];
	}
}

- (void) windowWillClose: (NSNotification *)notification;
{
	for (NSTabViewItem *tab in [tabView tabViewItems]) {
		[tabView removeTabViewItem: tab];
	}
}

- (IBAction) newTab: (id) sender;
{
	nsurl *url;
	nserror error;

        if (nsoption_charp(homepage_url) != NULL) {
                error = nsurl_create(nsoption_charp(homepage_url), &url);
	} else {
                error = nsurl_create(NETSURF_HOMEPAGE, &url);
	}
        if (error == NSERROR_OK) {
                error = browser_window_create(BW_CREATE_HISTORY |
                                              BW_CREATE_TAB,
                                              url,
                                              NULL,
                                              [activeBrowser browser],
                                              NULL);
                nsurl_unref(url);
        }
        if (error != NSERROR_OK) {
                warn_user(messages_get_errorcode(error), 0);
        }
}

- (IBAction) closeCurrentTab: (id) sender;
{
	[self removeTab: activeBrowser];
}

- (void) setActiveBrowser: (BrowserViewController *)newBrowser;
{
	activeBrowser = newBrowser;
	[self setNextResponder: activeBrowser];
}

- (void) setCanGoBack: (BOOL)can;
{
	[navigationControl setEnabled: can forSegment: 0];
}

- (BOOL) canGoBack;
{
	return [navigationControl isEnabledForSegment: 0];
}

- (void) setCanGoForward: (BOOL)can;
{
	[navigationControl setEnabled: can forSegment: 1];
}

- (BOOL) canGoForward;
{
	return [navigationControl isEnabledForSegment: 1];
}

- (void)windowDidBecomeMain: (NSNotification *)note;
{
	[(NetSurfApp *)NSApp setFrontTab: [[tabView selectedTabViewItem] identifier]];
}

- (void)menuNeedsUpdate:(NSMenu *)menu
{
	if (menu == historyBackMenu) {
		[activeBrowser buildBackMenu: menu];
	} else if (menu == historyForwardMenu) {
		[activeBrowser buildForwardMenu: menu];
	}
}

#pragma mark -
#pragma mark Tab bar delegate

- (void) tabView: (NSTabView *)tabView didSelectTabViewItem: (NSTabViewItem *)tabViewItem;
{
	[self setActiveBrowser: [tabViewItem identifier]];
	if ([[self window] isMainWindow]) {
		[(NetSurfApp *)NSApp setFrontTab: [tabViewItem identifier]];
	}
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDragTabViewItem:(NSTabViewItem *)tabViewItem fromTabBar:(PSMTabBarControl *)tabBarControl
{
    return [aTabView numberOfTabViewItems] > 1;
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDropTabViewItem:(NSTabViewItem *)tabViewItem inTabBar:(PSMTabBarControl *)tabBarControl
{
	[[tabViewItem identifier] setWindowController: self];
	return YES;
}

- (PSMTabBarControl *)tabView:(NSTabView *)aTabView newTabBarForDraggedTabViewItem:(NSTabViewItem *)tabViewItem atPoint:(NSPoint)point;
{
	BrowserWindowController *newWindow = [[[BrowserWindowController alloc] init] autorelease];
	[[tabViewItem identifier] setWindowController: newWindow];
	[[newWindow window] setFrameOrigin: point];
	return newWindow->tabBar;
}

- (void) tabView: (NSTabView *)aTabView didCloseTabViewItem: (NSTabViewItem *)tabViewItem;
{
	[tabViewItem unbind: @"label"];
	
	if (activeBrowser == [tabViewItem identifier]) {
		[self setActiveBrowser: nil];
		[(NetSurfApp *)NSApp setFrontTab: nil];
	}
	
	browser_window_destroy( [[tabViewItem identifier] browser] );
}

@end
