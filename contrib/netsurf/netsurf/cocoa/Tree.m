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

#import "cocoa/Tree.h"
#import "cocoa/coordinates.h"
#import "cocoa/font.h"
#import "cocoa/plotter.h"

#import "desktop/tree.h"

@implementation Tree

@synthesize delegate;

static void tree_redraw_request( int x, int y, int w, int h, void *data );
static void tree_resized( struct tree *tree, int w, int h, void *data );
static void tree_scroll_visible( int y, int height, void *data );
static void tree_get_window_dimensions( int *width, int *height, void *data );

static const struct treeview_table cocoa_tree_callbacks = {
	.redraw_request = tree_redraw_request,
	.resized = tree_resized,
	.scroll_visible = tree_scroll_visible,
	.get_window_dimensions = tree_get_window_dimensions
};

- initWithFlags: (unsigned int)flags;
{
	if ((self = [super init]) == nil) return nil;
	
	tree = tree_create( flags, &cocoa_tree_callbacks, self );
	if (tree == NULL) {
		[self release];
		return nil;
	}
	
	return self;
}


- (void) dealloc;
{
	tree_delete( tree );
	[super dealloc];
}

- (struct tree *) tree;
{
	return tree;
}

- (void) setRedrawing: (BOOL) newRedrawing;
{
}


+ (void) initialize;
{
}

//MARK: -
//MARK: Callbacks

static void tree_redraw_request( int x, int y, int w, int h, void *data )
{
	id <TreeDelegate> delegate = ((Tree *)data)->delegate;
	[delegate tree: (Tree *)data requestedRedrawInRect: cocoa_rect_wh( x, y, w, h )];
}

static void tree_resized( struct tree *tree, int w, int h, void *data )
{
	id <TreeDelegate> delegate = ((Tree *)data)->delegate;
	[delegate tree: (Tree *)data resized: cocoa_size( w, h )];
}

static void tree_scroll_visible( int y, int height, void *data )
{
	id <TreeDelegate> delegate = ((Tree *)data)->delegate;
	[delegate tree: (Tree *)data scrollPoint: cocoa_point( 0, y )];
}

static void tree_get_window_dimensions( int *width, int *height, void *data )
{
	id <TreeDelegate> delegate = ((Tree *)data)->delegate;
	if (delegate == nil) return;
	
	NSSize size = [delegate treeWindowSize: (Tree *)data];

	if (width != NULL) *width = cocoa_pt_to_px( size.width );
	if (height != NULL) *height = cocoa_pt_to_px( size.height );
}

@end

@implementation Tree (ViewInterface)

- (void) drawRect: (NSRect) rect inView: (NSView *) view;
{
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &cocoa_plotters
	};

	tree_draw( tree, 0, 0, cocoa_pt_to_px( NSMinX( rect ) ), cocoa_pt_to_px( NSMinY( rect )), 
			  cocoa_pt_to_px( NSWidth( rect ) ), cocoa_pt_to_px( NSHeight( rect ) ), &ctx );
}

- (void) mouseAction: (browser_mouse_state)state atPoint: (NSPoint)point;
{
	tree_mouse_action( tree, state, cocoa_pt_to_px( point.x ), cocoa_pt_to_px( point.y ) );
}

- (void) mouseDragEnd: (browser_mouse_state)state fromPoint: (NSPoint)p0 toPoint: (NSPoint) p1;
{
	tree_drag_end( tree, state, 
				  cocoa_pt_to_px( p0.x ), cocoa_pt_to_px( p0.y ), 
				  cocoa_pt_to_px( p1.x ), cocoa_pt_to_px( p1.y ) );
}

- (void) keyPress: (uint32_t) key;
{
	tree_keypress( tree, key );
}

@end

