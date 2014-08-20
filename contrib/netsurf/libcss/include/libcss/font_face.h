/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2011 Things Made Out Of Other Things Ltd.
 * Written by James Montgomerie <jamie@th.ingsmadeoutofotherthin.gs>
 */

#ifndef libcss_font_face_h_
#define libcss_font_face_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <libwapcaplet/libwapcaplet.h>
	
#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/properties.h>
#include <libcss/types.h>

typedef enum css_font_face_format {
	CSS_FONT_FACE_FORMAT_UNSPECIFIED 	= 0x00,

	CSS_FONT_FACE_FORMAT_WOFF		= 0x01,
		/* WOFF (Web Open Font Format); .woff */
	CSS_FONT_FACE_FORMAT_OPENTYPE		= 0x02,
		/* TrueType or OpenType; .ttf, .otf */
	CSS_FONT_FACE_FORMAT_EMBEDDED_OPENTYPE 	= 0x04,
		/* Embedded OpenType; .eot */
	CSS_FONT_FACE_FORMAT_SVG		= 0x08,
		/* SVG Font; .svg, .svgz */
	
	CSS_FONT_FACE_FORMAT_UNKNOWN 		= 0x10,
		/* Format specified, but not recognised */
	
	/* We don't define CSS_FONT_FACE_SRC_FORMAT_TRUETYPE as might be 
	 * expected, because the CSS3 specification 
	 *  (http://www.w3.org/TR/css3-fonts/, §4.3) says:
	 *	"Given the overlap in common usage between TrueType and
	 *	 OpenType, the format hints "truetype" and "opentype" must be 
	 *	 considered as synonymous"
	 * so we compute a hint of 'truetype' to css_font_face_format_opentype.
	 */
} css_font_face_format;

typedef enum css_font_face_location_type {
	CSS_FONT_FACE_LOCATION_TYPE_UNSPECIFIED	= 0,
	CSS_FONT_FACE_LOCATION_TYPE_LOCAL	= 1,
	CSS_FONT_FACE_LOCATION_TYPE_URI		= 2,
} css_font_face_location_type;

	
css_error css_font_face_get_font_family(
		const css_font_face *font_face, 
		lwc_string **font_family);
		
css_error css_font_face_count_srcs(const css_font_face *font_face, 
		uint32_t *count);
css_error css_font_face_get_src(const css_font_face *font_face, uint32_t index,
		const css_font_face_src **src);
	
css_error css_font_face_src_get_location(const css_font_face_src *src,
		lwc_string **location);
	
css_font_face_location_type css_font_face_src_location_type(
		const css_font_face_src *src);
css_font_face_format css_font_face_src_format(const css_font_face_src *src);

uint8_t css_font_face_font_style(const css_font_face *font_face);
uint8_t css_font_face_font_weight(const css_font_face *font_face);

#ifdef __cplusplus
}
#endif

#endif
