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

#import "cocoa/BookmarksController.h"
#import "cocoa/Tree.h"
#import "cocoa/TreeView.h"
#import "cocoa/NetsurfApp.h"
#import "cocoa/BrowserViewController.h"
#import "cocoa/gui.h"

#import "desktop/browser_private.h"
#import "desktop/hotlist.h"
#import "desktop/tree.h"
#import "utils/messages.h"
#import "utils/utils.h"
#import "content/hlcache.h"

@interface BookmarksController ()
- (void) noteAppWillTerminate: (NSNotification *) note;
- (void) save;
@end

@implementation BookmarksController

@synthesize defaultMenu;
@synthesize view;

static const char *cocoa_hotlist_path( void )
{
	NSString *path = [[NSUserDefaults standardUserDefaults] stringForKey: kHotlistFileOption];
	return [path UTF8String];
}

- init;
{
	if ((self = [super initWithWindowNibName: @"BookmarksWindow"]) == nil) return nil;
	tree_hotlist_path = cocoa_hotlist_path();
	tree = [[Tree alloc] initWithFlags: TREE_HOTLIST];
	nodeForMenu = NSCreateMapTable( NSNonOwnedPointerMapKeyCallBacks, NSNonOwnedPointerMapValueCallBacks, 0 );
	
	[[NSNotificationCenter defaultCenter] addObserver:self 
											 selector:@selector( noteAppWillTerminate: ) 
												 name:NSApplicationWillTerminateNotification
											   object:NSApp];

	return self;
}

- (void) noteAppWillTerminate: (NSNotification *) note;
{
	[self save];
}

- (void) save;
{
	hotlist_export( cocoa_hotlist_path(), NULL );
}

- (void) dealloc;
{
	[self setView: nil];
	NSFreeMapTable( nodeForMenu );
	[tree release];
	
	[[NSNotificationCenter defaultCenter] removeObserver: self];
	
	[super dealloc];
}

- (void) menuNeedsUpdate: (NSMenu *)menu
{
#if 0
	for (NSMenuItem *item in [menu itemArray]) {
		if ([item hasSubmenu]) NSMapRemove( nodeForMenu, [item submenu] );
		[menu removeItem: item];
	}

	bool hasSeparator = true;
	struct node *node = (struct node *)NSMapGet( nodeForMenu, menu );
	if (node == NULL) {
		for (NSMenuItem *item in [defaultMenu itemArray]) {
			[menu addItem: [[item copy] autorelease]];
		}
		hasSeparator = false;
	}
	
	for (struct node *child = tree_node_get_child( node ); 
		 child != NULL; 
		 child = tree_node_get_next( child )) {
		
		if (tree_node_is_deleted( child )) continue;
		
		if (!hasSeparator) {
			[menu addItem: [NSMenuItem separatorItem]];
			hasSeparator = true;
		}
		
		NSString *title = [NSString stringWithUTF8String: tree_url_node_get_title( child )];
		
		NSMenuItem *item = [menu addItemWithTitle: title action: NULL keyEquivalent: @""];
		if (tree_node_is_folder( child )) {
			NSMenu *subMenu = [[[NSMenu alloc] initWithTitle: title] autorelease];
			NSMapInsert( nodeForMenu, subMenu, child );
			[subMenu setDelegate: self];
			[menu setSubmenu: subMenu forItem: item];
		} else {
			[item setRepresentedObject: [NSString stringWithUTF8String: tree_url_node_get_url( child )]];
			[item setTarget: self];
			[item setAction: @selector( openBookmarkURL: )];
		}
	}
#endif
}

- (IBAction) openBookmarkURL: (id) sender;
{
	const char *urltxt = [[sender representedObject] UTF8String];
	NSParameterAssert( urltxt != NULL );

	nsurl *url;
	nserror error;

	error = nsurl_create(urltxt, &url);
	if (error == NSERROR_OK) {
                BrowserViewController *tab = [(NetSurfApp *)NSApp frontTab];
                if (tab != nil) {
                        error = browser_window_navigate([tab browser],
                                                url,
                                                NULL,
                                                BW_NAVIGATE_HISTORY,
                                                NULL,
                                                NULL,
                                                NULL);
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
}

- (IBAction) addBookmark: (id) sender;
{
	struct browser_window *bw = [[(NetSurfApp *)NSApp frontTab] browser];
	if (bw && bw->current_content) {
		hotlist_add_url( hlcache_handle_get_url( bw->current_content ) );
	}
}

- (BOOL) validateUserInterfaceItem: (id) item;
{
	SEL action = [item action];
	
	if (action == @selector( addBookmark: )) {
		return [(NetSurfApp *)NSApp frontTab] != nil;
	}
	
	return YES;
}

- (void) windowDidLoad;
{
	hotlist_expand(false);
	hotlist_contract(true);
	
	[view setTree: tree];
}


+ (void) initialize;
{
	[[NSUserDefaults standardUserDefaults] registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys:
															  cocoa_get_user_path( @"Bookmarks.html" ), kHotlistFileOption,
															  nil]];
}

- (IBAction) editSelected: (id) sender;
{
	hotlist_edit_selection();
}

- (IBAction) deleteSelected: (id) sender;
{
	hotlist_keypress(KEY_DELETE_LEFT);
}

- (IBAction) addFolder: (id) sender;
{
	hotlist_add_folder(NULL, false, 0);
}

@end
