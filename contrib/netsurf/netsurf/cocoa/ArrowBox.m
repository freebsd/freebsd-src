/* Copyright (c) 1011 Sven Weidauer
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#import "ArrowBox.h"


@implementation ArrowBox

@synthesize arrowPosition;
@synthesize arrowSize;
@synthesize arrowEdge;
@synthesize cornerRadius;

- (void) setArrowEdge: (ArrowEdge)newEdge;
{
	if (arrowEdge == newEdge) return;
	
	arrowEdge = newEdge;
	
	[self setNeedsDisplay: YES];
	updateShadow = YES;
}

- (void) setArrowSize: (CGFloat)newSize;
{
	arrowSize = newSize;
	[self setNeedsDisplay: YES];
	updateShadow = YES;
}

- (void) setCornerRadius: (CGFloat)newRadius;
{
	cornerRadius = newRadius;
	[self setNeedsDisplay: YES];
	updateShadow = YES;
}

- (void) setArrowPosition: (CGFloat)newPosition;
{
	arrowPosition = newPosition;
	
	[self setNeedsDisplay: YES];
	updateShadow = YES;
}

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
		arrowPosition = 50;
		cornerRadius = 10;
		arrowSize = 15;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect 
{
	[[NSColor clearColor] set];
	[NSBezierPath fillRect: dirtyRect];
	
	NSBezierPath *path = [NSBezierPath bezierPath];
	
	NSRect bounds = [self convertRectToBase: NSInsetRect( [self bounds], 2, 2 )];
	bounds.origin.x = floor( bounds.origin.x );
	bounds.origin.y = floor( bounds.origin.y );
	bounds.size.width = floor( bounds.size.width );
	bounds.size.height = floor( bounds.size.height );
	bounds = [self convertRectFromBase: bounds];
	
	const CGFloat right = bounds.size.width - arrowSize;
	const CGFloat top = bounds.size.height - arrowSize;
	const CGFloat left = arrowSize;
	const CGFloat bottom = arrowSize;
	
	[path setLineJoinStyle:NSRoundLineJoinStyle];

	[path moveToPoint: NSMakePoint( right - cornerRadius, top )];

	if (arrowEdge == ArrowTopEdge) {
		[path lineToPoint: NSMakePoint( arrowPosition + arrowSize, top )];
		[path lineToPoint: NSMakePoint( arrowPosition, top + arrowSize )];
		[path lineToPoint: NSMakePoint( arrowPosition - arrowSize, top )];
	}
	
	[path appendBezierPathWithArcFromPoint: NSMakePoint( left, top ) 
								   toPoint: NSMakePoint( left, top - cornerRadius ) 
									radius: cornerRadius];
	
	if (arrowEdge == ArrowLeftEdge) {
		[path lineToPoint: NSMakePoint( left, bottom + arrowPosition + arrowSize )];
		[path lineToPoint: NSMakePoint( left - arrowSize, bottom + arrowPosition )];
		[path lineToPoint: NSMakePoint( left, bottom + arrowPosition - arrowSize )];
	}
	
	[path appendBezierPathWithArcFromPoint: NSMakePoint( left, bottom ) 
								   toPoint: NSMakePoint( left + cornerRadius, bottom ) 
									radius: cornerRadius];
	
	if (arrowEdge == ArrowBottomEdge) {
		[path lineToPoint: NSMakePoint( arrowPosition - arrowSize, bottom )];
		[path lineToPoint: NSMakePoint( arrowPosition, bottom - arrowSize )];
		[path lineToPoint: NSMakePoint( arrowPosition + arrowSize, bottom )];
	}
	
	[path appendBezierPathWithArcFromPoint: NSMakePoint( right, bottom )
								   toPoint: NSMakePoint( right, bottom + cornerRadius ) 
									radius: cornerRadius];

	if (arrowEdge == ArrowRightEdge) {
		[path lineToPoint: NSMakePoint( right, bottom + arrowPosition - arrowSize )];
		[path lineToPoint: NSMakePoint( right + arrowSize, bottom + arrowPosition )];
		[path lineToPoint: NSMakePoint( right, bottom + arrowPosition + arrowSize )];
	}
	
	[path appendBezierPathWithArcFromPoint: NSMakePoint( right, top ) 
								   toPoint: NSMakePoint( right - cornerRadius, top ) 
									radius: cornerRadius];
	[path closePath];
	
	[[NSColor colorWithDeviceWhite: 1.0 alpha: 0.4] set];
	[[NSColor colorWithDeviceWhite: 0.0 alpha: 0.75] setFill];

	NSAffineTransform *transform = [NSAffineTransform transform];
	[transform translateXBy: bounds.origin.x yBy: bounds.origin.y];
	[transform concat];
	
	[path setLineWidth: 2.0];
	[path fill];
	[path stroke];
	
	if (updateShadow) {
		[[self window] invalidateShadow];
		[[self window] update];
		updateShadow = NO;
	}
}

@end
