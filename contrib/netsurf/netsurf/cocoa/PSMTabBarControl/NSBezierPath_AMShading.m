//
//  NSBezierPath_AMShading.m
//  ------------------------
//
//  Created by Andreas on 2005-06-01.
//  Copyright 2005 Andreas Mayer. All rights reserved.
//

#import "NSBezierPath_AMShading.h"


@implementation NSBezierPath (AMShading)

static void linearShadedColor(void *info, const CGFloat *in, CGFloat *out){
	CGFloat *colors = (CGFloat *)info;
	*out++ = colors[0] + *in * colors[8];
	*out++ = colors[1] + *in * colors[9];
	*out++ = colors[2] + *in * colors[10];
	*out++ = colors[3] + *in * colors[11];
}

static void bilinearShadedColor(void *info, const CGFloat *in, CGFloat *out){
	CGFloat *colors = (CGFloat *)info;
	CGFloat factor = (*in) * 2.0;
	if(*in > 0.5) {
		factor = 2 - factor;
	}
	*out++ = colors[0] + factor * colors[8];
	*out++ = colors[1] + factor * colors[9];
	*out++ = colors[2] + factor * colors[10];
	*out++ = colors[3] + factor * colors[11];
}

- (void)linearGradientFillWithStartColor:(NSColor *)startColor endColor:(NSColor *)endColor {
	static const CGFunctionCallbacks callbacks = {0, &linearShadedColor, NULL};

	[self customHorizontalFillWithCallbacks:callbacks firstColor:startColor secondColor:endColor];
}

- (void)linearVerticalGradientFillWithStartColor:(NSColor *)startColor endColor:(NSColor *)endColor {
	static const CGFunctionCallbacks callbacks = {0, &linearShadedColor, NULL};

	[self customVerticalFillWithCallbacks:callbacks firstColor:startColor secondColor:endColor];
}

- (void)bilinearGradientFillWithOuterColor:(NSColor *)outerColor innerColor:(NSColor *)innerColor {
	static const CGFunctionCallbacks callbacks = {0, &bilinearShadedColor, NULL};

	[self customHorizontalFillWithCallbacks:callbacks firstColor:innerColor secondColor:outerColor];
}

- (void)customFillWithCallbacks:(CGFunctionCallbacks)functionCallbacks firstColor:(NSColor *)firstColor secondColor:(NSColor *)secondColor startPoint:(CGPoint)startPoint endPoint:(CGPoint)endPoint {
	CGColorSpaceRef colorspace;
	CGShadingRef shading;
	CGFunctionRef function;
	CGFloat colors[12];     // pointer to color values

	// get my context
	CGContextRef currentContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

	NSColor *deviceDependentFirstColor = [firstColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
	NSColor *deviceDependentSecondColor = [secondColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];

	// set up colors for gradient
	colors[0] = [deviceDependentFirstColor redComponent];
	colors[1] = [deviceDependentFirstColor greenComponent];
	colors[2] = [deviceDependentFirstColor blueComponent];
	colors[3] = [deviceDependentFirstColor alphaComponent];

	colors[4] = [deviceDependentSecondColor redComponent];
	colors[5] = [deviceDependentSecondColor greenComponent];
	colors[6] = [deviceDependentSecondColor blueComponent];
	colors[7] = [deviceDependentSecondColor alphaComponent];

	// difference between start and end color for each color components
	colors[8] = (colors[4] - colors[0]);
	colors[9] = (colors[5] - colors[1]);
	colors[10] = (colors[6] - colors[2]);
	colors[11] = (colors[7] - colors[3]);

	// draw gradient
	colorspace = CGColorSpaceCreateDeviceRGB();
	size_t components = 1 + CGColorSpaceGetNumberOfComponents(colorspace);
	static const CGFloat domain[2] = {0.0, 1.0};
	static const CGFloat range[10] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
	//static const CGFunctionCallbacks callbacks = {0, &bilinearShadedColor, NULL};

	// Create a CGFunctionRef that describes a function taking 1 input and kChannelsPerColor outputs.
	function = CGFunctionCreate(colors, 1, domain, components, range, &functionCallbacks);

	shading = CGShadingCreateAxial(colorspace, startPoint, endPoint, function, NO, NO);

	CGContextSaveGState(currentContext);
	[self addClip];
	CGContextDrawShading(currentContext, shading);
	CGContextRestoreGState(currentContext);

	CGShadingRelease(shading);
	CGFunctionRelease(function);
	CGColorSpaceRelease(colorspace);
}

- (void)customHorizontalFillWithCallbacks:(CGFunctionCallbacks)functionCallbacks firstColor:(NSColor *)firstColor secondColor:(NSColor *)secondColor {
	[self customFillWithCallbacks:functionCallbacks
	 firstColor:firstColor
	 secondColor:secondColor
	 startPoint:CGPointMake(0, NSMinY([self bounds]))
	 endPoint:CGPointMake(0, NSMaxY([self bounds]))];
}

- (void)customVerticalFillWithCallbacks:(CGFunctionCallbacks)functionCallbacks firstColor:(NSColor *)firstColor secondColor:(NSColor *)secondColor {
	[self customFillWithCallbacks:functionCallbacks
	 firstColor:firstColor
	 secondColor:secondColor
	 startPoint:CGPointMake(NSMinX([self bounds]), 0)
	 endPoint:CGPointMake(NSMaxX([self bounds]), 0)];
}

@end
