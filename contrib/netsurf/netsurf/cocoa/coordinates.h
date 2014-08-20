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

#ifndef COCOA_COORDINATES_H
#define COCOA_COORDINATES_H

#import "desktop/browser_private.h"
#import "render/box.h"

extern CGFloat cocoa_scale_factor;

static inline CGFloat cocoa_px_to_pt( int location ) __attribute__((always_inline,pure));
static inline CGFloat cocoa_px_to_pt_f( CGFloat location ) __attribute__((always_inline,pure));

static inline int cocoa_pt_to_px( CGFloat location ) __attribute__((always_inline,pure));

static inline NSPoint cocoa_point( int x, int y ) __attribute__((always_inline,pure));
static inline NSPoint cocoa_scaled_point( CGFloat scale, int x, int y ) __attribute__((always_inline,pure));

static inline NSSize cocoa_size( int w, int h ) __attribute__((always_inline,pure));
static inline NSSize cocoa_scaled_size( CGFloat scale, int w, int h ) __attribute__((always_inline,pure));

static inline NSRect cocoa_rect( int x0, int y0, int x1, int y1 ) __attribute__((always_inline,pure));
static inline NSRect cocoa_rect_wh( int x, int y, int w, int h ) __attribute__((always_inline,pure));

static inline NSRect cocoa_scaled_rect( CGFloat scale, int x0, int y0, int x1, int y1 ) __attribute__((always_inline,pure));
static inline NSRect cocoa_scaled_rect_wh( CGFloat scale, int x, int y, int w, int h ) __attribute__((always_inline,pure));

static inline CGFloat cocoa_px_to_pt( int location )
{
	return (CGFloat)location * cocoa_scale_factor;
}

static inline CGFloat cocoa_px_to_pt_f( CGFloat location )
{
	return floor( location ) * cocoa_scale_factor;
}

static inline int cocoa_pt_to_px( CGFloat location )
{
	return location / cocoa_scale_factor;
}

static inline NSPoint cocoa_point( int x, int y )
{
	return NSMakePoint( cocoa_px_to_pt( x ), cocoa_px_to_pt( y ) );
}

static inline NSPoint cocoa_scaled_point( CGFloat scale, int x, int y )
{
	return NSMakePoint( cocoa_px_to_pt_f( scale * x ), cocoa_px_to_pt_f( scale * y ) );
}

static inline NSSize cocoa_size( int w, int h )
{
	return NSMakeSize( cocoa_px_to_pt( w ), cocoa_px_to_pt( h ) );
}

static inline NSSize cocoa_scaled_size( CGFloat scale, int w, int h )
{
	return NSMakeSize( cocoa_px_to_pt_f( scale * w ), cocoa_px_to_pt_f( scale * h ) );
}

static inline NSRect cocoa_rect( int x0, int y0, int x1, int y1 )
{
	return cocoa_rect_wh( x0, y0, x1 - x0, y1 - y0 );
}

static inline NSRect cocoa_rect_wh( int x, int y, int w, int h )
{
	const NSRect result = {
		.origin = cocoa_point( x, y ),
		.size = cocoa_size( w, h )
	};
	return result;
}

static inline NSRect cocoa_scaled_rect_wh( CGFloat scale, int x, int y, int w, int h )
{
	const NSRect result = {
		.origin = cocoa_scaled_point( scale, x, y ),
		.size = cocoa_scaled_size( scale, w, h )
	};
	return result;
}

static inline NSRect cocoa_scaled_rect( CGFloat scale, int x0, int y0, int x1, int y1 )
{
	return cocoa_scaled_rect_wh( scale, x0, y0, x1 - x0, y1 - y0 );
}

static inline NSRect cocoa_rect_for_box( struct browser_window *bw, struct box *box )
{
	struct rect r;
	box_bounds( box, &r );
	return cocoa_scaled_rect( bw->scale, r.x0, r.y0, r.x1, r.y1 );
}

#endif
