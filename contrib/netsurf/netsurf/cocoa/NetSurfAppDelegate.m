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

#import "cocoa/NetSurfAppDelegate.h"
#import "cocoa/SearchWindowController.h"
#import "cocoa/PreferencesWindowController.h"
#import "cocoa/HistoryWindowController.h"

#import "desktop/browser.h"
#import "utils/nsoption.h"
#import "utils/messages.h"
#import "utils/utils.h"

@interface NetSurfAppDelegate ()

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;

@end


@implementation NetSurfAppDelegate

@synthesize history;
@synthesize search;
@synthesize preferences;

- (void) newDocument: (id) sender;
{
	nsurl *url;
	nserror error;

        if (nsoption_charp(homepage_url) != NULL) {
                error = nsurl_create(nsoption_charp(homepage_url), &url);
	} else {
                error = nsurl_create(NETSURF_HOMEPAGE, &url);
	}

	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

- (void) openDocument: (id) sender;
{
	nsurl *u;
	nserror error;

	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	[openPanel setAllowsMultipleSelection: YES];
	if ([openPanel runModalForTypes: nil] == NSOKButton) {
		for (NSURL *url in [openPanel URLs]) {
                        error = nsurl_create([[url absoluteString] UTF8String], &u);
                        if (error == NSERROR_OK) {
                                error = browser_window_create(BW_CREATE_HISTORY,
					      u,
					      NULL,
					      NULL,
					      NULL);
                                nsurl_unref(u);
                        }
                        if (error != NSERROR_OK) {
                                warn_user(messages_get_errorcode(error), 0);
                        }
		}
	}
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	nsurl *url;
	nserror error;
        NSString *urlAsString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];

	error = nsurl_create([urlAsString UTF8String], &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

- (IBAction) showSearchWindow: (id) sender;
{
	if (search == nil) {
		[self setSearch: [[[SearchWindowController alloc] init] autorelease]];
	}
	[[search window] makeKeyAndOrderFront: self];
}

- (IBAction) searchForward: (id) sender;
{
	[search search: SearchForward];
}

- (IBAction) searchBackward: (id) sender;
{
	[search search: SearchBackward];
}

- (BOOL) validateMenuItem: (id) item;
{
	SEL action = [item action];
	
	if (action == @selector( searchForward: )) {
		return [search canGoForward];
	} else if (action == @selector( searchBackward: )) {
		return [search canGoBack];
	}
	
	return YES;
}

- (IBAction) showPreferences: (id) sender;
{
	if (preferences == nil) {
		[self setPreferences: [[[PreferencesWindowController alloc] init] autorelease]];
	}
	[preferences showWindow: sender];
}

- (IBAction) showGlobalHistory: (id) sender;
{
	if (history == nil) {
		[self setHistory: [[[HistoryWindowController alloc] init] autorelease]];
	}
	[history showWindow: sender];
}

// Application delegate methods

- (BOOL) applicationOpenUntitledFile: (NSApplication *)sender;
{
	[self newDocument: self];
	return YES;
}

-(void)applicationWillFinishLaunching:(NSNotification *)aNotification 
{
    NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:self 
                           andSelector:@selector(handleGetURLEvent:withReplyEvent:)
                         forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
 	nsurl *url;
	nserror error;
        NSURL *urltxt = [NSURL fileURLWithPath: filename];

	error = nsurl_create([[urltxt absoluteString] UTF8String], &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

        return YES;
}


@end
