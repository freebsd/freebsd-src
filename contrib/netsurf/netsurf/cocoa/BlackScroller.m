/* Copyright (c) 1011 Sven Weidauer
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#import "BlackScroller.h"


@implementation BlackScroller

- (void) setFrame: (NSRect)frameRect;
{
	[super setFrame: frameRect];
	if (tag != 0) [self removeTrackingRect: tag];
	tag = [self addTrackingRect: [self bounds] owner: self userData: NULL assumeInside: NO];
}

- (void) drawRect: (NSRect)dirtyRect;
{
	[[NSColor clearColor] set];
	[NSBezierPath fillRect: dirtyRect];
	
	if (drawTrack) [self drawKnobSlotInRect: [self rectForPart: NSScrollerKnobSlot] 
								  highlight: NO];
	[self drawKnob];
}

- (void) drawKnobSlotInRect: (NSRect)slotRect highlight: (BOOL)flag;
{
	slotRect = NSInsetRect( slotRect, 2, 2 );
	slotRect = [self convertRectToBase: slotRect];
	slotRect.origin.x = floor( slotRect.origin.x ) + 0.5;
	slotRect.origin.y = floor( slotRect.origin.y ) + 0.5;
	slotRect.size.width = floor( slotRect.size.width );
	slotRect.size.height = floor( slotRect.size.height );
	slotRect = [self convertRectFromBase: slotRect];
	
	NSGradient *gradient = [[[NSGradient alloc] initWithColorsAndLocations: 
							 [NSColor clearColor], 0.0,
							 [NSColor clearColor], 0.4,
							 [NSColor whiteColor], 1.0,
							 nil] autorelease];
	[[NSColor whiteColor] set];
	const float radius = 0.5 * ([self isHorizontal] ? NSHeight( slotRect ) : NSWidth( slotRect ));
	NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect: slotRect 
														 xRadius: radius 
														 yRadius: radius];
	[gradient drawInBezierPath: path angle: [self isHorizontal] ? 90 : 0];
	
	[path stroke];
}


- (NSUsableScrollerParts) usableParts;
{
	return NSScrollerKnob|NSScrollerKnobSlot;
}

- (void) drawKnob;
{
	NSRect rect = NSInsetRect( [self rectForPart: NSScrollerKnob], 2, 2 );
	
	rect = [self convertRectToBase: rect];
	rect.origin.x = floor( rect.origin.x ) + 0.5;
	rect.origin.y = floor( rect.origin.y ) + 0.5;
	rect.size.width = floor( rect.size.width );
	rect.size.height = floor( rect.size.height );
	rect = [self convertRectFromBase: rect];

	[[NSColor colorWithDeviceWhite: 1.0 alpha: drawTrack ? 1.0 : 0.6] set];
	
	const float radius = 0.5 * ([self isHorizontal] ? NSHeight( rect ) : NSWidth( rect ));
	NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect: rect 
														 xRadius: radius 
														 yRadius: radius];
	[path fill];
	[path stroke];
}

- (NSRect) rectForPart: (NSScrollerPart)partCode;
{
	const bool horizontal = [self isHorizontal];
	
	NSRect rect = horizontal ? NSInsetRect( [self bounds], 4, 0 ) : NSInsetRect( [self bounds], 0, 4 );
	
	switch (partCode) {
		case NSScrollerKnobSlot:
			return rect;
			
		case NSScrollerKnob: {
		const CGFloat len = horizontal ? NSWidth( rect ) : NSHeight( rect );
			CGFloat knobLen = [self knobProportion] * len;
			const CGFloat minKnobLen = horizontal ? NSHeight( rect ) : NSWidth( rect );
			if (knobLen < minKnobLen) knobLen = minKnobLen;
			
			const CGFloat start = [self doubleValue] * (len - knobLen);
			
			if (horizontal) {
				rect.origin.x += start;
				rect.size.width = knobLen;
			} else {
				rect.origin.y += start;
				rect.size.height = knobLen;
			}
			
			return rect;
		}
			
		default:
			return [super rectForPart: partCode];
	}
}

- (BOOL) isOpaque;
{
	return NO;
}

- (BOOL) isHorizontal;
{
	NSRect bounds = [self bounds];
	return NSWidth( bounds ) > NSHeight( bounds );
}

- (void) mouseEntered: (NSEvent *)theEvent;
{
	drawTrack = YES;
	[self setNeedsDisplay: YES];
}

- (void) mouseExited: (NSEvent *)theEvent;
{
	drawTrack = NO;
	[self setNeedsDisplay: YES];
}

@end
