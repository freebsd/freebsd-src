//
//  PSMProgressIndicator.m
//  PSMTabBarControl
//
//  Created by John Pannell on 2/23/06.
//  Copyright 2006 Positive Spin Media. All rights reserved.
//

#import "PSMProgressIndicator.h"
#import "PSMTabBarControl.h"

@interface PSMTabBarControl (PSMProgressIndicatorExtensions)

- (void)update;

@end

@implementation PSMProgressIndicator

- (id) initWithFrame: (NSRect)frameRect;
{
	if ((self = [super initWithFrame: frameRect]) == nil) return nil;
	[self setControlSize: NSSmallControlSize];
	return self;
}

// overrides to make tab bar control re-layout things if status changes
- (void)setHidden:(BOOL)flag {
	[super setHidden:flag];
	[(PSMTabBarControl *)[self superview] update];
}

- (void)stopAnimation:(id)sender {
	[NSObject cancelPreviousPerformRequestsWithTarget:self
	 selector:@selector(startAnimation:)
	 object:nil];
	[super stopAnimation:sender];
}

@end
