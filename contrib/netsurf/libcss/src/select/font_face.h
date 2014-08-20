/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2011 Things Made Out Of Other Things Ltd.
 * Written by James Montgomerie <jamie@th.ingsmadeoutofotherthin.gs>
 */

#ifndef css_select_font_face_h_
#define css_select_font_face_h_

#include <libcss/font_face.h>


struct css_font_face_src {
	lwc_string *location;
	/*
	 * Bit allocations:
	 *
	 *    76543210
	 *  1 _fffffll	format | location type
	 */
	uint8_t bits[1];
};

struct css_font_face {
	lwc_string *font_family;
	css_font_face_src *srcs;
	uint32_t n_srcs;
	
	/*
	 * Bit allocations:
	 *
	 *    76543210
	 *  1 __wwwwss	font-weight | font-style
	 */
	uint8_t bits[1];
};

css_error css__font_face_create(css_font_face **result);
css_error css__font_face_destroy(css_font_face *font_face);

css_error css__font_face_set_font_family(css_font_face *font_face,
		lwc_string *font_family);

css_error css__font_face_set_srcs(css_font_face *font_face,
		css_font_face_src *srcs, uint32_t n_srcs);

#endif
