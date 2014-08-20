//
//  PSMTabDragWindow.m
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/1/06.
//  Copyright 2006 Kent Sutherland. All rights reserved.
//

#import "PSMTabDragWindow.h"
#import "PSMTabDragView.h"

@implementation PSMTabDragWindow

+ (PSMTabDragWindow *)dragWindowWithImage:(NSImage *)image styleMask:(NSUInteger)styleMask {
	return [[[PSMTabDragWindow alloc] initWithImage:image styleMask:styleMask] autorelease];
}

- (id)initWithImage:(NSImage *)image styleMask:(NSUInteger)styleMask {
	NSSize size = [image size];

	if((self = [super initWithContentRect:NSMakeRect(0, 0, size.width, size.height) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO])) {
		_dragView = [[[PSMTabDragView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)] autorelease];
		[self setContentView:_dragView];
		[self setLevel:NSStatusWindowLevel];
		[self setIgnoresMouseEvents:YES];
		[self setOpaque:NO];

		[_dragView setImage:image];

		//Set the size of the window to be the exact size of the drag image
		NSRect windowFrame = [self frame];
		windowFrame.origin.y += windowFrame.size.height - size.height;
		windowFrame.size = size;

		if(styleMask | NSBorderlessWindowMask) {
			windowFrame.size.height += 22;
		}

		[self setFrame:windowFrame display:YES];
	}
	return self;
}

- (PSMTabDragView *)dragView {
	return _dragView;
}

@end
