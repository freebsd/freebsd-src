/* Copyright (c) 1011 Sven Weidauer
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import "ArrowWindow.h"
#import "ArrowBox.h"

@implementation ArrowWindow

@synthesize acceptsKey;

- (id) initWithContentRect: (NSRect)contentRect styleMask: (NSUInteger)aStyle backing: (NSBackingStoreType)bufferingType defer: (BOOL)flag
{
	if ((self = [super initWithContentRect: contentRect styleMask: NSBorderlessWindowMask backing: bufferingType defer: flag]) == nil) return nil;
	
	[self setBackgroundColor: [NSColor clearColor]];
	[self setOpaque: NO];
	[self setHasShadow: YES];
	
	return self;
}

- (void) setContentView: (NSView *)aView;
{
	if (aView == content) return;

	[content removeFromSuperview];
	content = aView;
	
	if (content == nil) return;
	
	if (box == nil) {
		box = [[ArrowBox alloc] initWithFrame: NSZeroRect];
		[box setArrowEdge: ArrowTopEdge];
		[super setContentView: box];
		[box release];
	}
	
	[box addSubview: content];

	NSRect frame = [self contentRectForFrameRect: [self frame]];
	frame.origin = [self convertScreenToBase: frame.origin];
	frame = [box convertRect: frame fromView: nil];
	
	[content setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];	
	[content setFrame: frame];
}

- (void) setContentSize: (NSSize)aSize;
{
	NSRect frame = [content frame];
	frame.size = aSize;
	
	frame = [box convertRect: frame toView: nil];
	frame.origin = [self convertBaseToScreen: frame.origin];
	frame = [self frameRectForContentRect: frame];
	
	[self setFrame: frame display: YES];
}

static const CGFloat padding = 0;

- (NSRect) contentRectForFrameRect: (NSRect)frameRect;
{
	const CGFloat arrowSize = [box arrowSize];
	const CGFloat offset = 2 * (padding + arrowSize );

	return NSInsetRect( frameRect, offset, offset );
}

- (NSRect) frameRectForContentRect: (NSRect)contentRect;
{
	const CGFloat arrowSize = [box arrowSize];
	const CGFloat offset = -2 * (padding + arrowSize );
	
	return NSInsetRect( contentRect, offset, offset );
}

+ (NSRect) frameRectForContentRect: (NSRect)cRect styleMask: (NSUInteger)aStyle;
{
	const CGFloat DefaultArrowSize = 15;
	const CGFloat offset = -2 * (padding + DefaultArrowSize);
	
	return NSInsetRect( cRect, offset, offset );
}

- (BOOL) canBecomeKeyWindow;
{
	return acceptsKey;
}

- (void) moveToPoint: (NSPoint) screenPoint;
{
	switch ([box arrowEdge]) {
		case ArrowNone:
			screenPoint.x -= [box arrowSize];
			screenPoint.y += [box arrowSize];
			break;
			
		case ArrowTopEdge:
			screenPoint.x -= [box arrowPosition];
			break;
			
		case ArrowBottomEdge:
			screenPoint.x -= [box arrowPosition];
			screenPoint.y += NSHeight( [self frame] );
			break;
			
		case ArrowLeftEdge:
			screenPoint.y += NSHeight( [self frame] ) - [box arrowPosition] - [box arrowSize];
			break;
			
		case ArrowRightEdge:
			screenPoint.x -= NSWidth( [self frame] );
			screenPoint.y += NSHeight( [self frame] ) - [box arrowPosition] - [box arrowSize];
			break;
	}
	
	[self setFrameTopLeftPoint: screenPoint];
}

static NSRect ScreenRectForView( NSView *view )
{
	NSRect viewRect = [view bounds];										// in View coordinate system
	viewRect = [view convertRect: viewRect toView: nil];					// translate to window coordinates
	viewRect.origin = [[view window] convertBaseToScreen: viewRect.origin]; // translate to screen coordinates
	return viewRect;
}

- (void) attachToView: (NSView *) view;
{
	if (nil != attachedWindow) [self detach];
	
	NSRect viewRect = ScreenRectForView( view );
	NSPoint arrowPoint;
	switch ([box arrowEdge]) {
		case ArrowNone:
		case ArrowTopEdge:
			arrowPoint = NSMakePoint( NSMidX( viewRect ), NSMinY( viewRect ) );
			break;
			
		case ArrowLeftEdge:
			arrowPoint = NSMakePoint( NSMaxX( viewRect ), NSMidY( viewRect ) );
			break;

		case ArrowBottomEdge:
			arrowPoint = NSMakePoint( NSMidX( viewRect ), NSMaxY( viewRect ) );
			break;

		case ArrowRightEdge:
			arrowPoint = NSMakePoint( NSMinX( viewRect ), NSMidY( viewRect ) );
			break;
	}
	attachedWindow = [view window];
	[self moveToPoint: arrowPoint];
	[attachedWindow addChildWindow: self ordered: NSWindowAbove];
}

- (void) detach;
{
	[attachedWindow removeChildWindow: self];
	[self close];
	attachedWindow = nil;
}

//MARK: -
//MARK: Properties

- (void) setArrowPosition: (CGFloat) newPosition;
{
	[box setArrowPosition: newPosition];
}

- (CGFloat) arrowPosition;
{
	return [box arrowPosition];
}

- (void) setArrowSize: (CGFloat)newSize;
{
	NSRect contentRect = [self contentRectForFrameRect: [self frame]];
	[box setArrowSize: newSize];
	[self setFrame: [self frameRectForContentRect: contentRect] display: [self isVisible]];
}

- (CGFloat) arrowSize;
{
	return [box arrowSize];
}

- (void) setArrowEdge: (ArrowEdge) newEdge;
{
	[box setArrowEdge: newEdge];
}

- (ArrowEdge) arrowEdge;
{
	return [box arrowEdge];
}

- (void) setCornerRadius: (CGFloat)newRadius;
{
	[box setCornerRadius: newRadius];
}

- (CGFloat) cornerRadius;
{
	return [box cornerRadius];
}

@end
