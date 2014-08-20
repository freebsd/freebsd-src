/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_types_h_
#define libcss_types_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/fpmath.h>

/**
 * Source of charset information, in order of importance.
 * A client-dictated charset will override all others.
 * A document-specified charset will override autodetection or the default.
 */
typedef enum css_charset_source {
	CSS_CHARSET_DEFAULT          = 0,	/**< Default setting */
	CSS_CHARSET_REFERRED         = 1,	/**< From referring document */
	CSS_CHARSET_METADATA         = 2,	/**< From linking metadata */
	CSS_CHARSET_DOCUMENT         = 3,	/**< Defined in document */
	CSS_CHARSET_DICTATED         = 4	/**< Dictated by client */
} css_charset_source;

/**
 * Stylesheet language level -- defines parsing rules and supported properties
 */
typedef enum css_language_level {
	CSS_LEVEL_1                 = 0,	/**< CSS 1 */
	CSS_LEVEL_2                 = 1,	/**< CSS 2 */
	CSS_LEVEL_21                = 2,	/**< CSS 2.1 */
	CSS_LEVEL_3                 = 3,	/**< CSS 3 */
	CSS_LEVEL_DEFAULT           = CSS_LEVEL_21	/**< Default level */
} css_language_level;

/**
 * Stylesheet media types
 */
typedef enum css_media_type {
	CSS_MEDIA_AURAL             = (1 << 0),
	CSS_MEDIA_BRAILLE           = (1 << 1),
	CSS_MEDIA_EMBOSSED          = (1 << 2),
	CSS_MEDIA_HANDHELD          = (1 << 3),
	CSS_MEDIA_PRINT             = (1 << 4),
	CSS_MEDIA_PROJECTION        = (1 << 5),
	CSS_MEDIA_SCREEN            = (1 << 6),
	CSS_MEDIA_SPEECH            = (1 << 7),
	CSS_MEDIA_TTY               = (1 << 8),
	CSS_MEDIA_TV                = (1 << 9),
	CSS_MEDIA_ALL               = CSS_MEDIA_AURAL | CSS_MEDIA_BRAILLE |
                                      CSS_MEDIA_EMBOSSED | CSS_MEDIA_HANDHELD |
                                      CSS_MEDIA_PRINT | CSS_MEDIA_PROJECTION |
                                      CSS_MEDIA_SCREEN | CSS_MEDIA_SPEECH |
                                      CSS_MEDIA_TTY | CSS_MEDIA_TV
} css_media_type;

/**
 * Stylesheet origin
 */
typedef enum css_origin {
	CSS_ORIGIN_UA               = 0,	/**< User agent stylesheet */
	CSS_ORIGIN_USER             = 1,	/**< User stylesheet */
	CSS_ORIGIN_AUTHOR           = 2		/**< Author stylesheet */
} css_origin;

/** CSS colour -- AARRGGBB */
typedef uint32_t css_color;

/* CSS unit */
typedef enum css_unit {
	CSS_UNIT_PX                 = 0x0,
	CSS_UNIT_EX                 = 0x1,
	CSS_UNIT_EM                 = 0x2,
	CSS_UNIT_IN                 = 0x3,
	CSS_UNIT_CM                 = 0x4,
	CSS_UNIT_MM                 = 0x5,
	CSS_UNIT_PT                 = 0x6,
	CSS_UNIT_PC                 = 0x7,

	CSS_UNIT_PCT                = 0x8,	/* Percentage */

	CSS_UNIT_DEG                = 0x9,
	CSS_UNIT_GRAD               = 0xa,
	CSS_UNIT_RAD                = 0xb,

	CSS_UNIT_MS                 = 0xc,
	CSS_UNIT_S                  = 0xd,

	CSS_UNIT_HZ                 = 0xe,
	CSS_UNIT_KHZ                = 0xf
} css_unit;

/**
 * Type of a qualified name
 */
typedef struct css_qname {
	/**
	 * Namespace URI:
	 *
	 * NULL for no namespace
	 * '*' for any namespace (including none)
	 * URI for a specific namespace
	 */
	lwc_string *ns;

	/**
	 * Local part of qualified name
	 */
	lwc_string *name;
} css_qname;

typedef struct css_stylesheet css_stylesheet;

typedef struct css_select_ctx css_select_ctx;

typedef struct css_computed_style css_computed_style;

typedef struct css_font_face css_font_face;

typedef struct css_font_face_src css_font_face_src;

#ifdef __cplusplus
}
#endif

#endif
