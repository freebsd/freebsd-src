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

#import "cocoa/LocalHistoryController.h"

#import "cocoa/BrowserView.h"
#import "cocoa/HistoryView.h"
#import "cocoa/ArrowWindow.h"

@implementation LocalHistoryController

@synthesize browser;
@synthesize history;

- initWithBrowser: (BrowserView *) bw;
{
	if ((self = [super initWithWindowNibName: @"LocalHistoryPanel"]) == nil) return nil;
	
	browser = bw;
	
	return self;
}

- (void) attachToView: (NSView *) view;
{
	NSDisableScreenUpdates();
	
	ArrowWindow *box = (ArrowWindow *)[self window];

	[box setContentSize: [history size]];
	[box setArrowPosition: 50];
	[history updateHistory];
	[box attachToView: view];
	
	NSRect frame = [box frame];
	NSRect screenFrame = [[box screen] visibleFrame];
	
	const CGFloat arrowSize = [box arrowSize];
	frame.origin.x += arrowSize;
	frame.origin.y += arrowSize;
	frame.size.width -= 2 * arrowSize;
	frame.size.height -= 2 * arrowSize;
	
	if (NSMinY( frame ) < NSMinY( screenFrame )) {
		const CGFloat delta = NSMinY( screenFrame ) - NSMinY( frame );
		frame.size.height -= delta;
		frame.origin.y += delta;
	}
	
	CGFloat arrowPositionChange = 50;
	if (NSMaxX( frame ) > NSMaxX( screenFrame )) {
		const CGFloat delta = NSMaxX( frame ) - NSMaxX( screenFrame );
		arrowPositionChange += delta;
		frame.origin.x -= delta;
	}
	
	if (NSMinX( frame ) < NSMinX( screenFrame )) {
		const CGFloat delta = NSMinX( screenFrame ) - NSMinX( frame );
		arrowPositionChange -= delta;
		frame.origin.x  += delta;
		frame.size.width -= delta;
	}

	frame.origin.x -= arrowSize;
	frame.origin.y -= arrowSize;
	frame.size.width += 2 * arrowSize;
	frame.size.height += 2 * arrowSize;
	
	[box setArrowPosition: arrowPositionChange];
	[box setFrame: frame display: YES];
	
	NSEnableScreenUpdates();
}

- (void) detach;
{
	[(ArrowWindow *)[self window] detach];
}

- (void) windowDidLoad;
{
	[history setBrowser: browser];
}

- (void) redraw;
{
	[history setNeedsDisplay: YES];
}

- (void) keyDown: (NSEvent *)theEvent;
{
	unichar key = [[theEvent characters] characterAtIndex: 0];
	switch (key) {
		case 27:
			[browser setHistoryVisible: NO];
			break;
			
		default:
			NSBeep();
			break;
	};
}

@end
