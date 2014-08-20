/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Cocoa/Cocoa.h>

#import "cocoa/font.h"
#import "cocoa/plotter.h"
#import "cocoa/bitmap.h"

#import "desktop/plotters.h"
#import "desktop/plot_style.h"
#import "utils/log.h"
#import "css/utils.h"

static void cocoa_plot_render_path(NSBezierPath *path,const plot_style_t *pstyle);
static void cocoa_plot_path_set_stroke_pattern(NSBezierPath *path,const plot_style_t *pstyle);
static inline void cocoa_center_pixel( bool x, bool y );

static NSRect cocoa_plot_clip_rect;

#define colour_red_component( c )		(((c) >>  0) & 0xFF)
#define colour_green_component( c )		(((c) >>  8) & 0xFF)
#define colour_blue_component( c )		(((c) >> 16) & 0xFF)
#define colour_alpha_component( c )		(((c) >> 24) & 0xFF)
#define colour_from_rgba( r, g, b, a)	((((colour)(r)) <<  0) | \
										 (((colour)(g)) <<  8) | \
										 (((colour)(b)) << 16) | \
										 (((colour)(a)) << 24))
#define colour_from_rgb( r, g, b ) colour_from_rgba( (r), (g), (b), 0xFF )

NSColor *cocoa_convert_colour( colour clr )
{
	return [NSColor colorWithDeviceRed: (float)colour_red_component( clr ) / 0xFF 
								 green: (float)colour_green_component( clr ) / 0xFF 
								  blue: (float)colour_blue_component( clr ) / 0xFF 
								 alpha: 1.0];
}

static void cocoa_plot_path_set_stroke_pattern(NSBezierPath *path,const plot_style_t *pstyle) 
{
	static const CGFloat dashed_pattern[2] = { 5.0, 2.0 };
	static const CGFloat dotted_pattern[2] = { 2.0, 2.0 };
	
	switch (pstyle->stroke_type) {
		case PLOT_OP_TYPE_DASH: 
			[path setLineDash: dashed_pattern count: 2 phase: 0];
			break;
			
		case PLOT_OP_TYPE_DOT: 
			[path setLineDash: dotted_pattern count: 2 phase: 0];
			break;
			
		default:
			// ignore
			break;
	}

	[path setLineWidth: cocoa_px_to_pt( pstyle->stroke_width > 0 ? pstyle->stroke_width : 1 )];
}

static bool plot_line(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	if (pstyle->stroke_type == PLOT_OP_TYPE_NONE) return true;
	
	[NSGraphicsContext saveGraphicsState];
	[NSBezierPath clipRect: cocoa_plot_clip_rect];
	
	NSBezierPath *path = [NSBezierPath bezierPath];
	[path moveToPoint: cocoa_point( x0, y0 )];
	[path lineToPoint: cocoa_point( x1, y1 )];
	cocoa_plot_path_set_stroke_pattern( path, pstyle );
	
	const bool horizontal = y0 == y1;
	const bool vertical = x0 == x1;
	const bool oddThickness = pstyle->stroke_width != 0 ? (pstyle->stroke_width % 2) != 0 : true;
	
	if (oddThickness) cocoa_center_pixel( !horizontal, !vertical );
	
	[cocoa_convert_colour( pstyle->stroke_colour ) set];
	[path stroke];
	
	[NSGraphicsContext restoreGraphicsState];
	
	return true;
}

static bool plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	NSRect rect = cocoa_rect( x0, y0, x1, y1 );
	NSBezierPath *path = [NSBezierPath bezierPathWithRect: rect];
	cocoa_plot_render_path( path, pstyle );
	
	return true;
}

static bool plot_text(int x, int y, const char *text, size_t length,
			 const plot_font_style_t *fstyle)
{
	[NSGraphicsContext saveGraphicsState];
	[NSBezierPath clipRect: cocoa_plot_clip_rect];
	
	cocoa_draw_string( cocoa_px_to_pt( x ), cocoa_px_to_pt( y ), text, length, fstyle );
	
	[NSGraphicsContext restoreGraphicsState];
	
	return true;
}

void cocoa_set_clip( NSRect rect )
{
	cocoa_plot_clip_rect = rect;
}

static bool plot_clip(const struct rect *clip)
{
	cocoa_plot_clip_rect = cocoa_rect( clip->x0, clip->y0, clip->x1, clip->y1 );
	return true;
}

void cocoa_plot_render_path(NSBezierPath *path,const plot_style_t *pstyle) 
{
	[NSGraphicsContext saveGraphicsState];
	[NSBezierPath clipRect: cocoa_plot_clip_rect];
	
	if (pstyle->fill_type != PLOT_OP_TYPE_NONE) {
		[cocoa_convert_colour( pstyle->fill_colour ) setFill];
		[path fill];
	}
	
	if (pstyle->stroke_type != PLOT_OP_TYPE_NONE) {
		if (pstyle->stroke_width == 0 || pstyle->stroke_width % 2 != 0) cocoa_center_pixel( true, true );
		
		cocoa_plot_path_set_stroke_pattern(path,pstyle);
		
		[cocoa_convert_colour( pstyle->stroke_colour ) set];
		
		[path stroke];
	}
	
	[NSGraphicsContext restoreGraphicsState];
}

static bool plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *pstyle)
{
	NSBezierPath *path = [NSBezierPath bezierPath];
	[path appendBezierPathWithArcWithCenter: NSMakePoint( x, y ) radius: radius 
								 startAngle: angle1 endAngle: angle2 
								  clockwise: NO];
	
	cocoa_plot_render_path( path, pstyle);
	
	return true;
}

static bool plot_disc(int x, int y, int radius, const plot_style_t *pstyle)
{
	NSBezierPath *path  = [NSBezierPath bezierPathWithOvalInRect: 
						   NSMakeRect( x - radius, y-radius, 2*radius, 2*radius )];
	
	cocoa_plot_render_path( path, pstyle );
	
	return true;
}

static bool plot_polygon(const int *p, unsigned int n, const plot_style_t *pstyle)
{
	if (n <= 1) return true;
	
	NSBezierPath *path = [NSBezierPath bezierPath];
	[path moveToPoint: cocoa_point( p[0], p[1] )];
	for (unsigned i = 1; i < n; i++) {
		[path lineToPoint: cocoa_point( p[2*i], p[2*i+1] )];
	}
	[path closePath];
	
	cocoa_plot_render_path( path, pstyle );
	
	return true;
}

/* complex path (for SVG) */
static bool plot_path(const float *p, unsigned int n, colour fill, float width,
			 colour c, const float transform[6])
{
	if (n == 0) return true;
	
	if (*p != PLOTTER_PATH_MOVE) {
		LOG(("Path does not start with move"));
		return false;
	}
	
	NSBezierPath *path = [NSBezierPath bezierPath];

#define NEXT_POINT() NSMakePoint( *p++, *p++ )
	
	while (n--) {
		switch ((int)*p++) {
			case PLOTTER_PATH_MOVE: {
				const NSPoint pt = NEXT_POINT();
				[path moveToPoint: pt];
				break;
			}
				
			case PLOTTER_PATH_LINE: {
				const NSPoint pt = NEXT_POINT();
				[path lineToPoint: pt];
				break;
			}
				
			case PLOTTER_PATH_BEZIER: {
				const NSPoint cp1 = NEXT_POINT();
				const NSPoint cp2 = NEXT_POINT();
				const NSPoint ep = NEXT_POINT();
				[path curveToPoint: ep controlPoint1: cp1 controlPoint2: cp2];
				break;
			}
				
			case PLOTTER_PATH_CLOSE:
				[path closePath];
				break;
				
			default:
				LOG(("Invalid path"));
				return false;
		}
	}
	
#undef NEXT_POINT
	
	[path setLineWidth: width];

	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
	CGContextSaveGState( context );
	
	CGContextClipToRect( context, NSRectToCGRect( cocoa_plot_clip_rect ) );
	
	CGContextConcatCTM( context, CGAffineTransformMake( transform[0], transform[1], transform[2], 
													    transform[3], transform[4], transform[5] ) );
	
	if (fill != NS_TRANSPARENT) {
		[cocoa_convert_colour( fill ) setFill];
		[path fill];
	}

	if (c != NS_TRANSPARENT) {
		cocoa_center_pixel( true, true );
		[cocoa_convert_colour( c ) set];
		[path stroke];
	}
	
	CGContextRestoreGState( context );

	return true;
}

/* Image */
static bool plot_bitmap(int x, int y, int width, int height,
			   struct bitmap *bitmap, colour bg,
			   bitmap_flags_t flags)
{
	CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
	CGContextSaveGState( context );

	CGContextClipToRect( context, NSRectToCGRect( cocoa_plot_clip_rect ) );
	
	const bool tileX = flags & BITMAPF_REPEAT_X;
	const bool tileY = flags & BITMAPF_REPEAT_Y;

	CGImageRef img = cocoa_get_cgimage( bitmap );

	CGRect rect = NSRectToCGRect( cocoa_rect_wh( x, y, width, height ) );
	
	if (tileX || tileY) {
		CGContextDrawTiledImage( context, rect, img );
	} else {
		CGContextDrawImage( context, rect, img );
	}
	
	CGContextRestoreGState( context );
	
	return true;
}

const struct plotter_table cocoa_plotters = {
	.clip = plot_clip,
	.arc = plot_arc,
	.disc = plot_disc,
	.rectangle = plot_rectangle,
	.line = plot_line,
	.polygon = plot_polygon,
	
	.path = plot_path,
	
	.bitmap = plot_bitmap,
	
	.text = plot_text,

	.option_knockout = true
};


CGFloat cocoa_scale_factor;
static const CGFloat points_per_inch = 72.0;
static CGFloat cocoa_half_pixel;

void cocoa_update_scale_factor( void )
{
	const CGFloat scale = [[NSScreen mainScreen] userSpaceScaleFactor];
	cocoa_scale_factor = scale == 1.0 ? 1.0 : 1.0 / scale;
	cocoa_half_pixel = 0.5 * cocoa_scale_factor;
	nscss_screen_dpi = FLTTOFIX( points_per_inch * scale );
}

static inline void cocoa_center_pixel( bool x, bool y ) 
{
	NSAffineTransform *transform = [NSAffineTransform transform];
	[transform translateXBy: x ? cocoa_half_pixel : 0.0 yBy: y ? cocoa_half_pixel : 0.0];
	[transform concat];
}
