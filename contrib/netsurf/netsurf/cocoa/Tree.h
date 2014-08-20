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

#import "desktop/tree.h"

@class Tree;

@protocol TreeDelegate

- (void) tree: (Tree *)tree requestedRedrawInRect: (NSRect) rect;
- (void) tree: (Tree *)tree resized: (NSSize) size;
- (void) tree: (Tree *)tree scrollPoint: (NSPoint) point;
- (NSSize) treeWindowSize: (Tree *)tree;

@end


@interface Tree : NSObject {
	id <TreeDelegate> delegate;
	struct tree *tree;
}

@property (readwrite, assign, nonatomic) id <TreeDelegate> delegate;

- initWithFlags: (unsigned int) flags;

- (struct tree *) tree;

@end


@interface Tree (ViewInterface)

- (void) drawRect: (NSRect) rect inView: (NSView *) view;
- (void) mouseAction: (browser_mouse_state)state atPoint: (NSPoint)point;
- (void) mouseDragEnd: (browser_mouse_state)state fromPoint: (NSPoint)p0 toPoint: (NSPoint) p1;
- (void) keyPress: (uint32_t) key;

@end
