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

#import "cocoa/plotter.h"
#import "cocoa/font.h"

#import "css/css.h"
#import "utils/nsoption.h"
#import "render/font.h"
#import "desktop/plotters.h"


static NSLayoutManager *cocoa_prepare_layout_manager( const char *string, size_t length, 
													 const plot_font_style_t *style );

static CGFloat cocoa_layout_width( NSLayoutManager *layout );
static CGFloat cocoa_layout_width_chars( NSLayoutManager *layout, size_t characters );
static NSUInteger cocoa_glyph_for_location( NSLayoutManager *layout, CGFloat x );
static size_t cocoa_bytes_for_characters( const char *string, size_t characters );
static NSDictionary *cocoa_font_attributes( const plot_font_style_t *style );

static NSTextStorage *cocoa_text_storage = nil;
static NSTextContainer *cocoa_text_container = nil;

static bool nsfont_width(const plot_font_style_t *style,
						 const char *string, size_t length,
						 int *width)
{
	NSLayoutManager *layout = cocoa_prepare_layout_manager( string, length, style );
	*width = cocoa_layout_width( layout );
	return true;
}

static bool nsfont_position_in_string(const plot_font_style_t *style,
									  const char *string, size_t length,
									  int x, size_t *char_offset, int *actual_x)
{
	NSLayoutManager *layout = cocoa_prepare_layout_manager( string, length, style );
	if (layout == nil) return false;
	
	NSUInteger glyphIndex = cocoa_glyph_for_location( layout, x );
	NSUInteger chars = [layout characterIndexForGlyphAtIndex: glyphIndex];
	
	if (chars >= [cocoa_text_storage length]) *char_offset = length;
	else *char_offset = cocoa_bytes_for_characters( string, chars );
	
	*actual_x = cocoa_pt_to_px( NSMaxX( [layout boundingRectForGlyphRange: NSMakeRange( glyphIndex - 1, 1 ) 
										  inTextContainer: cocoa_text_container] ) );
	
	return true;
}

static bool nsfont_split(const plot_font_style_t *style,
						 const char *string, size_t length,
						 int x, size_t *char_offset, int *actual_x)
{
	NSLayoutManager *layout = cocoa_prepare_layout_manager( string, length, style );
	if (layout == nil) return false;

	NSUInteger glyphIndex = cocoa_glyph_for_location( layout, x );
	NSUInteger chars = [layout characterIndexForGlyphAtIndex: glyphIndex];
	
	if (chars >= [cocoa_text_storage length]) {
		*char_offset = length;
		*actual_x = cocoa_layout_width( layout );
		return true;
	}
	

	chars = [[cocoa_text_storage string] rangeOfString: @" " options: NSBackwardsSearch range: NSMakeRange( 0, chars + 1 )].location;
	if (chars == NSNotFound) {
		*char_offset = 0;
		*actual_x = 0;
		return true;
	}
	
	*char_offset = cocoa_bytes_for_characters( string, chars );
	*actual_x = cocoa_layout_width_chars( layout, chars );
	
	return true;
}

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

#pragma mark -

void cocoa_draw_string( CGFloat x, CGFloat y, const char *bytes, size_t length, const plot_font_style_t *style )
{
	NSLayoutManager *layout = cocoa_prepare_layout_manager( bytes, length, style );
	if (layout == nil) return;
	
	NSFont *font = [cocoa_text_storage attribute: NSFontAttributeName atIndex: 0 effectiveRange: NULL];
	CGFloat baseline = [layout defaultLineHeightForFont: font] * 3.0 / 4.0;
	
	NSRange glyphRange = [layout glyphRangeForTextContainer: cocoa_text_container];
	[layout drawGlyphsForGlyphRange: glyphRange atPoint: NSMakePoint( x, y - baseline )];
}


#pragma mark -

static inline CGFloat cocoa_layout_width( NSLayoutManager *layout )
{
	if (layout == nil) return 0.0;
	
	return cocoa_pt_to_px( NSWidth( [layout usedRectForTextContainer: cocoa_text_container] ) );
}

static inline CGFloat cocoa_layout_width_chars( NSLayoutManager *layout, size_t characters )
{
	NSUInteger glyphIndex = [layout glyphIndexForCharacterAtIndex: characters];
	return cocoa_pt_to_px( [layout locationForGlyphAtIndex: glyphIndex].x );
}

static inline NSUInteger cocoa_glyph_for_location( NSLayoutManager *layout, CGFloat x )
{
	CGFloat fraction = 0.0;
	NSUInteger glyphIndex = [layout glyphIndexForPoint: NSMakePoint( cocoa_px_to_pt( x ), 0 ) 
									   inTextContainer: cocoa_text_container 
						fractionOfDistanceThroughGlyph: &fraction];
	if (fraction >= 1.0) ++glyphIndex;
	return glyphIndex;
}

static inline size_t cocoa_bytes_for_characters( const char *string, size_t chars )
{
	size_t offset = 0;
	while (chars-- > 0) {
		uint8_t ch = ((uint8_t *)string)[offset];
		
		if (0xC2 <= ch && ch <= 0xDF) offset += 2;
		else if (0xE0 <= ch && ch <= 0xEF) offset += 3;
		else if (0xF0 <= ch && ch <= 0xF4) offset += 4;
		else offset++;
	}
	return offset;
}

static NSLayoutManager *cocoa_prepare_layout_manager( const char *bytes, size_t length, 
													 const plot_font_style_t *style )
{
	if (NULL == bytes || 0 == length) return nil;

	NSString *string = [[[NSString alloc] initWithBytes: bytes length:length encoding:NSUTF8StringEncoding] autorelease];
	if (string == nil) return nil;

	static NSLayoutManager *layout = nil;
	if (nil == layout) {
		cocoa_text_container = [[NSTextContainer alloc] initWithContainerSize: NSMakeSize( CGFLOAT_MAX, CGFLOAT_MAX )];
		[cocoa_text_container setLineFragmentPadding: 0];
		
		layout = [[NSLayoutManager alloc] init];
		[layout addTextContainer: cocoa_text_container];
	}
	
	static NSString *oldString = 0;
	static plot_font_style_t oldStyle = { 0, 0, 0, 0, 0, 0 };

	const bool styleChanged = memcmp( style, &oldStyle, sizeof oldStyle ) != 0;
	
	if ([oldString isEqualToString: string] && !styleChanged) {
		return layout;
	}
	
	[oldString release]; 
	oldString = [string copy];
	oldStyle = *style;
	
	static NSDictionary *attributes = nil;
	if (styleChanged || attributes == nil) {
		[attributes release];
		attributes = [cocoa_font_attributes( style ) retain];
	}

	[cocoa_text_storage release];
	cocoa_text_storage = [[NSTextStorage alloc] initWithString: string attributes: attributes];
	[cocoa_text_storage addLayoutManager: layout];

	[layout ensureLayoutForTextContainer: cocoa_text_container];
	
	return layout;
}

static NSString * const cocoa_font_families[PLOT_FONT_FAMILY_COUNT] = {
	[PLOT_FONT_FAMILY_SERIF] = @"Times",
	[PLOT_FONT_FAMILY_SANS_SERIF] = @"Helvetica",
	[PLOT_FONT_FAMILY_MONOSPACE] = @"Courier",
	[PLOT_FONT_FAMILY_CURSIVE] = @"Apple Chancery",
	[PLOT_FONT_FAMILY_FANTASY] = @"Marker Felt"
};

static inline NSFont *cocoa_font_get_nsfont( const plot_font_style_t *style )
{
	NSFont *font = [NSFont fontWithName: cocoa_font_families[style->family]
								   size: (CGFloat)style->size / FONT_SIZE_SCALE];
	
	NSFontTraitMask traits = 0;
	if (style->flags & FONTF_ITALIC || style->flags & FONTF_OBLIQUE) traits |= NSItalicFontMask;
	if (style->flags & FONTF_SMALLCAPS) traits |= NSSmallCapsFontMask;
	if (style->weight > 400) traits |= NSBoldFontMask;
	
	if (0 != traits) {
		NSFontManager *fm = [NSFontManager sharedFontManager];
		font = [fm convertFont: font toHaveTrait: traits];
	}
	
	return font;
}

static inline NSDictionary *cocoa_font_attributes( const plot_font_style_t *style )
{
	return [NSDictionary dictionaryWithObjectsAndKeys: 
			cocoa_font_get_nsfont( style ), NSFontAttributeName, 
			cocoa_convert_colour( style->foreground ), NSForegroundColorAttributeName,
			nil];
}