//
//  PSMTabDragWindow.h
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/1/06.
//  Copyright 2006 Kent Sutherland. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class PSMTabDragView;

@interface PSMTabDragWindow : NSWindow {
	PSMTabDragView					*_dragView;
}
+ (PSMTabDragWindow *)dragWindowWithImage:(NSImage *)image styleMask:(NSUInteger)styleMask;

- (id)initWithImage:(NSImage *)image styleMask:(NSUInteger)styleMask;
- (PSMTabDragView *)dragView;
@end
