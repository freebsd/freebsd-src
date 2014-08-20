//
//  PSMTabDragWindowController.m
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/18/07.
//  Copyright 2007 Kent Sutherland. All rights reserved.
//

#import "PSMTabDragWindowController.h"
#import "PSMTabDragWindow.h"
#import "PSMTabDragView.h"

@implementation PSMTabDragWindowController

- (id)initWithImage:(NSImage *)image styleMask:(NSUInteger)styleMask tearOffStyle:(PSMTabBarTearOffStyle)tearOffStyle {
	PSMTabDragWindow *window = [PSMTabDragWindow dragWindowWithImage:image styleMask:styleMask];
	if((self = [super initWithWindow:window])) {
		_view = [[window dragView] retain];
		_tearOffStyle = tearOffStyle;

		if(tearOffStyle == PSMTabBarTearOffMiniwindow) {
			[window setBackgroundColor:[NSColor clearColor]];
			[window setHasShadow:YES];
		}

		[window setAlphaValue:kPSMTabDragWindowAlpha];
	}
	return self;
}

- (void)dealloc {
	if(_timer) {
		[_timer invalidate];
	}

	if(_animation) {
		[_animation release];
	}

	[_view release];
	[super dealloc];
}

- (NSImage *)image {
	return [_view image];
}

- (NSImage *)alternateImage {
	return [_view alternateImage];
}

- (void)setAlternateImage:(NSImage *)image {
	[_view setAlternateImage:image];
}

- (BOOL)isAnimating {
	return _animation != nil;
}

- (void)switchImages {
	if(_tearOffStyle != PSMTabBarTearOffMiniwindow || ![_view alternateImage]) {
		return;
	}

	CGFloat progress = 0;
	_showingAlternate = !_showingAlternate;

	if(_animation) {
		//An animation already exists, get the current progress
		progress = 1.0f - [_animation currentProgress];
		[_animation stopAnimation];
		[_animation release];
	}

	//begin animating
	_animation = [[NSAnimation alloc] initWithDuration:0.25 animationCurve:NSAnimationEaseInOut];
	[_animation setAnimationBlockingMode:NSAnimationNonblocking];
	[_animation setCurrentProgress:progress];
	[_animation startAnimation];

	_originalWindowFrame = [[self window] frame];

	if(_timer) {
		[_timer invalidate];
	}
	_timer = [NSTimer scheduledTimerWithTimeInterval:1.0f / 30.0f target:self selector:@selector(animateTimer:) userInfo:nil repeats:YES];
}

- (void)animateTimer:(NSTimer *)timer {
	NSRect frame = _originalWindowFrame;
	NSImage *currentImage = _showingAlternate ?[_view alternateImage] :[_view image];
	NSSize size = [currentImage size];
	NSPoint mousePoint = [NSEvent mouseLocation];
	CGFloat animationValue = [_animation currentValue];

	frame.size.width = _originalWindowFrame.size.width + (size.width - _originalWindowFrame.size.width) * animationValue;
	frame.size.height = _originalWindowFrame.size.height + (size.height - _originalWindowFrame.size.height) * animationValue;
	frame.origin.x = mousePoint.x - (frame.size.width / 2);
	frame.origin.y = mousePoint.y - (frame.size.height / 2);

	[_view setFadeValue:_showingAlternate ? 1.0f - animationValue : animationValue];
	[[self window] setFrame:frame display:YES];

	if(![_animation isAnimating]) {
		[_animation release], _animation = nil;
		[timer invalidate];
		_timer = nil;
	}
}

@end
