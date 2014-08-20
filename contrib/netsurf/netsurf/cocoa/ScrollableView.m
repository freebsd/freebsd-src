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

#import "cocoa/ScrollableView.h"

@interface ScrollableView ()

- (void) frameChangeNotification: (NSNotification *) note;

@end

@implementation ScrollableView
@synthesize minimumSize;

- (void) setMinimumSize: (NSSize)newSize;
{
	minimumSize = newSize;
	[self adjustFrame];
}

- (void) adjustFrame;
{
	NSSize frameSize = [[self superview] frame].size;
	[self setFrameSize: NSMakeSize( MAX( minimumSize.width, frameSize.width ),
								   MAX( minimumSize.height, frameSize.height ) )];
}

- (void) frameChangeNotification: (NSNotification *) note;
{
	[self adjustFrame];
}

- (void) viewDidMoveToSuperview;
{
	if (observedSuperview) {
		[[NSNotificationCenter defaultCenter] removeObserver: self 
														name: NSViewFrameDidChangeNotification 
													  object: observedSuperview];
		observedSuperview = nil;
	}
	
	NSView *newSuperView = [self superview];
	
	if (nil != newSuperView) {
		observedSuperview = newSuperView;
		[[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(frameChangeNotification:)
													 name: NSViewFrameDidChangeNotification
												   object: observedSuperview];
		[observedSuperview setPostsFrameChangedNotifications: YES];
	}
}

@end
