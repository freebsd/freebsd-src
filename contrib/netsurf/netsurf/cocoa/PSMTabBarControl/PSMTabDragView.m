//
//  PSMTabDragView.m
//  PSMTabBarControl
//
//  Created by Kent Sutherland on 6/17/07.
//  Copyright 2007 Kent Sutherland. All rights reserved.
//

#import "PSMTabDragView.h"


@implementation PSMTabDragView

- (id)initWithFrame:(NSRect)frame {
	if((self = [super initWithFrame:frame])) {
		_alpha = 1.0;
	}
	return self;
}

- (void)dealloc {
	[_image release];
	[_alternateImage release];
	[super dealloc];
}

- (void)drawRect:(NSRect)rect {
	//1.0 fade means show the primary image
	//0.0 fade means show the secondary image
	CGFloat primaryAlpha = _alpha + 0.001f, alternateAlpha = 1.001f - _alpha;
	NSRect srcRect;
	srcRect.origin = NSZeroPoint;
	srcRect.size = [_image size];

	[_image drawInRect:[self bounds] fromRect:srcRect operation:NSCompositeSourceOver fraction:primaryAlpha];
	srcRect.size = [_alternateImage size];
	[_alternateImage drawInRect:[self bounds] fromRect:srcRect operation:NSCompositeSourceOver fraction:alternateAlpha];
}

- (void)setFadeValue:(CGFloat)value {
	_alpha = value;
}

- (NSImage *)image {
	return _image;
}

- (void)setImage:(NSImage *)image {
	[_image release];
	_image = [image retain];
}

- (NSImage *)alternateImage {
	return _alternateImage;
}

- (void)setAlternateImage:(NSImage *)image {
	[_alternateImage release];
	_alternateImage = [image retain];
}

@end
