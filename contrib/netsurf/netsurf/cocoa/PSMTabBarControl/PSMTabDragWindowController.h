//
//  PSMTabDragWindowController.h
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/18/07.
//  Copyright 2007 Kent Sutherland. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PSMTabBarControl.h"

#define kPSMTabDragWindowAlpha 0.75
#define kPSMTabDragAlphaInterval 0.15

@class PSMTabDragView;

@interface PSMTabDragWindowController : NSWindowController {
	PSMTabBarTearOffStyle				_tearOffStyle;
	PSMTabDragView						*_view;
	NSAnimation							*_animation;
	NSTimer								*_timer;

	BOOL									_showingAlternate;
	NSRect									_originalWindowFrame;
}
- (id)initWithImage:(NSImage *)image styleMask:(NSUInteger) styleMask tearOffStyle:(PSMTabBarTearOffStyle)tearOffStyle;

- (NSImage *)image;
- (NSImage *)alternateImage;
- (void)setAlternateImage:(NSImage *)image;
- (BOOL)isAnimating;
- (void)switchImages;
@end
