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

#import "cocoa/bitmap.h"

#import "image/bitmap.h"

#define BITS_PER_SAMPLE (8)
#define SAMPLES_PER_PIXEL (4)
#define BITS_PER_PIXEL (BITS_PER_SAMPLE * SAMPLES_PER_PIXEL)
#define BYTES_PER_PIXEL (BITS_PER_PIXEL / 8)
#define RED_OFFSET (0)
#define GREEN_OFFSET (1)
#define BLUE_OFFSET (2)
#define ALPHA_OFFSET (3)

static CGImageRef cocoa_prepare_bitmap( void *bitmap );
static NSMapTable *cocoa_get_bitmap_cache( void );

int bitmap_get_width(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp pixelsWide];
}

int bitmap_get_height(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp pixelsHigh];
}

bool bitmap_get_opaque(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp isOpaque];
}

void bitmap_destroy(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );

	NSMapTable *cache = cocoa_get_bitmap_cache();
	CGImageRef image = NSMapGet( cache, bitmap );
	if (NULL != image) {
		CGImageRelease( image );
		NSMapRemove( cache, bitmap );
	}
	
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	[bmp release];
}

void *bitmap_create(int width, int height, unsigned int state)
{
	NSBitmapImageRep *bmp = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: NULL 
																	 pixelsWide: width 
																	 pixelsHigh: height
																  bitsPerSample: BITS_PER_SAMPLE
																samplesPerPixel: SAMPLES_PER_PIXEL
																	   hasAlpha: YES
																	   isPlanar: NO
																 colorSpaceName: NSDeviceRGBColorSpace 
																   bitmapFormat: NSAlphaNonpremultipliedBitmapFormat 
																	bytesPerRow: BYTES_PER_PIXEL * width 
																   bitsPerPixel: BITS_PER_PIXEL];

	return bmp;
}

void bitmap_set_opaque(void *bitmap, bool opaque)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	[bmp setOpaque: opaque ? YES : NO];
}

bool bitmap_test_opaque(void *bitmap)
{
	NSCParameterAssert( bitmap_get_bpp( bitmap ) == BYTES_PER_PIXEL );
	
	unsigned char *buf = bitmap_get_buffer( bitmap );
	
	const size_t height = bitmap_get_height( bitmap );
	const size_t width = bitmap_get_width( bitmap );
	
	const size_t line_step = bitmap_get_rowstride( bitmap ) - BYTES_PER_PIXEL * width;

	for (size_t y = 0; y < height; y++) {
		for (size_t x = 0; x < height; x++) {
			if (buf[ALPHA_OFFSET] != 0xFF) return false;
			buf += BYTES_PER_PIXEL;
		}
		buf += line_step;
	}
	
	return true;
}

unsigned char *bitmap_get_buffer(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp bitmapData];
}

size_t bitmap_get_rowstride(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp bytesPerRow];
}

size_t bitmap_get_bpp(void *bitmap)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	return [bmp bitsPerPixel] / 8;
}

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	NSCParameterAssert( NULL != bitmap );
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	
	NSData *tiff = [bmp TIFFRepresentation];
	return [tiff writeToFile: [NSString stringWithUTF8String: path] atomically: YES];
}

void bitmap_modified(void *bitmap)
{
	NSMapTable *cache = cocoa_get_bitmap_cache();
	CGImageRef image = NSMapGet( cache, bitmap );
	if (NULL != image) {
		CGImageRelease( image );
		NSMapRemove( cache, bitmap );
	}
}

CGImageRef cocoa_get_cgimage( void *bitmap )
{
	NSMapTable *cache = cocoa_get_bitmap_cache();

	CGImageRef result = NSMapGet( cache, bitmap );
	if (NULL == result) {
		result = cocoa_prepare_bitmap( bitmap );
		NSMapInsertKnownAbsent( cache, bitmap, result );
	}
	
	return result;
}

static inline NSMapTable *cocoa_get_bitmap_cache( void )
{
	static NSMapTable *cache = nil;
	if (cache == nil) {
		cache = NSCreateMapTable( NSNonOwnedPointerMapKeyCallBacks, NSNonOwnedPointerMapValueCallBacks, 0 );
	}
	return cache;
}

static CGImageRef cocoa_prepare_bitmap( void *bitmap )
{
	NSCParameterAssert( NULL != bitmap );
	
	NSBitmapImageRep *bmp = (NSBitmapImageRep *)bitmap;
	
	size_t w = [bmp pixelsWide];
	size_t h = [bmp pixelsHigh];
	
	CGImageRef original = [bmp CGImage];
	
	if (h <= 1) return CGImageRetain( original );
	
	void *data = malloc( 4 * w * h );
	
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef context = CGBitmapContextCreate( data, w, h, BITS_PER_SAMPLE, 
												 BYTES_PER_PIXEL * w, colorSpace, 
												 [bmp isOpaque] ? kCGImageAlphaNoneSkipLast 
																: kCGImageAlphaPremultipliedLast );
	CGColorSpaceRelease( colorSpace );
	
	CGContextTranslateCTM( context, 0.0, h );
	CGContextScaleCTM( context, 1.0, -1.0 );
	
	CGRect rect = CGRectMake( 0, 0, w, h );
	CGContextClearRect( context, rect );
	CGContextDrawImage( context, rect, original );
	
	CGImageRef result = CGBitmapContextCreateImage( context );
	
	CGContextRelease( context );
	free( data );
	
	return result;
}

