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

#import <Cocoa/Cocoa.h>

#import "desktop/browser.h"
#import "desktop/plotters.h"
#import "desktop/thumbnail.h"
#import "content/urldb.h"
#import "cocoa/plotter.h"
#import "image/bitmap.h"

/* In platform specific thumbnail.c. */
bool thumbnail_create(struct hlcache_handle *content, struct bitmap *bitmap, nsurl *url)
{
	int bwidth = bitmap_get_width( bitmap );
	int bheight = bitmap_get_height( bitmap );

	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &cocoa_plotters
	};

	CGColorSpaceRef cspace = CGColorSpaceCreateWithName( kCGColorSpaceGenericRGB );
	CGContextRef bitmapContext = CGBitmapContextCreate( bitmap_get_buffer( bitmap ), 
													   bwidth, bheight, 
													   bitmap_get_bpp( bitmap ) * 8 / 4, 
													   bitmap_get_rowstride( bitmap ), 
													   cspace, kCGImageAlphaNoneSkipLast );
	CGColorSpaceRelease( cspace );

	size_t width = MIN( content_get_width( content ), 1024 );
	size_t height = ((width * bheight) + bwidth / 2) / bwidth;
	
	CGContextTranslateCTM( bitmapContext, 0, bheight );
	CGContextScaleCTM( bitmapContext, (CGFloat)bwidth / width, -(CGFloat)bheight / height );

	[NSGraphicsContext setCurrentContext: [NSGraphicsContext graphicsContextWithGraphicsPort: bitmapContext flipped: YES]];

	thumbnail_redraw( content, width, height, &ctx );

	[NSGraphicsContext setCurrentContext: nil];
	CGContextRelease( bitmapContext );

	bitmap_modified( bitmap );

	if (NULL != url) urldb_set_thumbnail( url, bitmap );

	return true;
}

