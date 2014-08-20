//
//  PSMTabDragView.h
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/17/07.
//  Copyright 2007 Kent Sutherland. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface PSMTabDragView : NSView {
	NSImage					*_image;
	NSImage					*_alternateImage;
	CGFloat					_alpha;
}
- (void)setFadeValue:(CGFloat)value;
- (NSImage *)image;
- (void)setImage:(NSImage *)image;
- (NSImage *)alternateImage;
- (void)setAlternateImage:(NSImage *)image;
@end
