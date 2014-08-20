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

#import "cocoa/TreeView.h"
#import "cocoa/Tree.h"

#import "desktop/plotters.h"
#import "desktop/textinput.h"

@interface TreeView () <TreeDelegate>
@end

@implementation TreeView

@synthesize tree;

- (void)drawRect:(NSRect)dirtyRect 
{
	[tree drawRect: dirtyRect inView: self];
}

- (BOOL) isFlipped;
{
	return YES;
}

- (BOOL) acceptsFirstResponder;
{
	return YES;
}

- (void) dealloc;
{
	[self setTree: nil];
	[super dealloc];
}

- (void) setTree: (Tree *)newTree;
{
	if (tree != newTree) {
		[tree setRedrawing: NO];
		[tree setDelegate: nil];
		[tree release];
		
		tree = [newTree retain];
		[tree setDelegate: self];
		[tree setRedrawing: YES];
		
		[self setNeedsDisplay: YES];
	}
}

//MARK: -
//MARK: Event handlers

- (void)mouseDown: (NSEvent *)event;
{
	isDragging = NO;
	dragStart = [self convertPoint: [event locationInWindow] fromView: nil];
	[tree mouseAction: BROWSER_MOUSE_PRESS_1 atPoint: dragStart];
}

#define squared(x) ((x)*(x))
#define MinDragDistance (5.0)

- (void) mouseDragged: (NSEvent *)event;
{
	const NSPoint point = [self convertPoint: [event locationInWindow] fromView: nil];
	
	if (!isDragging) {
		const CGFloat distance = squared( dragStart.x - point.x ) + squared( dragStart.y - point.y );
		if (distance >= squared( MinDragDistance)) isDragging = YES;
	}
}

- (void) mouseUp: (NSEvent *)event;
{
	const NSPoint point = [self convertPoint: [event locationInWindow] fromView: nil];

	browser_mouse_state modifierFlags = 0;
	
	if (isDragging) {
		isDragging = NO;
		[tree mouseDragEnd: modifierFlags fromPoint: dragStart toPoint: point];
	} else {
		modifierFlags |= BROWSER_MOUSE_CLICK_1;
		if ([event clickCount] == 2) modifierFlags |= BROWSER_MOUSE_DOUBLE_CLICK;
		[tree mouseAction: modifierFlags atPoint: point];
	}
}

//MARK: Keyboard events

- (void) keyDown: (NSEvent *)theEvent;
{
	[self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

- (void) insertText: (id)string;
{
	for (NSUInteger i = 0, length = [string length]; i < length; i++) {
		unichar ch = [string characterAtIndex: i];
		[tree keyPress: ch];
	}
}

- (void) moveLeft: (id)sender;
{
	[tree keyPress: KEY_LEFT];
}

- (void) moveRight: (id)sender;
{
	[tree keyPress: KEY_RIGHT];
}

- (void) moveUp: (id)sender;
{
	[tree keyPress: KEY_UP];
}

- (void) moveDown: (id)sender;
{
	[tree keyPress: KEY_DOWN];
}

- (void) deleteBackward: (id)sender;
{
	[tree keyPress: KEY_DELETE_LEFT];
}

- (void) deleteForward: (id)sender;
{
	[tree keyPress: KEY_DELETE_RIGHT];
}

- (void) cancelOperation: (id)sender;
{
	[tree keyPress: KEY_ESCAPE];
}

- (void) scrollPageUp: (id)sender;
{
	[tree keyPress: KEY_PAGE_UP];
}

- (void) scrollPageDown: (id)sender;
{
	[tree keyPress: KEY_PAGE_DOWN];
}

- (void) insertTab: (id)sender;
{
	[tree keyPress: KEY_TAB];
}

- (void) insertBacktab: (id)sender;
{
	[tree keyPress: KEY_SHIFT_TAB];
}

- (void) moveToBeginningOfLine: (id)sender;
{
	[tree keyPress: KEY_LINE_START];
}

- (void) moveToEndOfLine: (id)sender;
{
	[tree keyPress: KEY_LINE_END];
}

- (void) moveToBeginningOfDocument: (id)sender;
{
	[tree keyPress: KEY_TEXT_START];
}

- (void) moveToEndOfDocument: (id)sender;
{
	[tree keyPress: KEY_TEXT_END];
}

- (void) insertNewline: (id)sender;
{
	[tree keyPress: KEY_NL];
}

- (void) selectAll: (id)sender;
{
	[tree keyPress: KEY_SELECT_ALL];
}

- (void) copy: (id) sender;
{
	[tree keyPress: KEY_COPY_SELECTION];
}

- (void) cut: (id) sender;
{
	[tree keyPress: KEY_CUT_SELECTION];
}

- (void) paste: (id) sender;
{
	[tree keyPress: KEY_PASTE];
}

//MARK: -
//MARK: Tree delegate methods

- (void) tree: (Tree *)t requestedRedrawInRect: (NSRect) rect;
{
	[self setNeedsDisplayInRect: rect];
}

- (void) tree: (Tree *)t resized: (NSSize) size;
{
	[self setMinimumSize: size];
}

- (void) tree: (Tree *)t scrollPoint: (NSPoint) point;
{
	[self scrollPoint: point];
}

- (NSSize) treeWindowSize: (Tree *)t;
{
	return [self frame].size;
}

@end
