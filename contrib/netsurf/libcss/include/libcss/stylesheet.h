/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_stylesheet_h_
#define libcss_stylesheet_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <libcss/errors.h>
#include <libcss/types.h>
#include <libcss/properties.h>

/**
 * Callback to resolve an URL
 *
 * \param pw    Client data
 * \param dict  String internment context
 * \param base  Base URI (absolute)
 * \param rel   URL to resolve, either absolute or relative to base
 * \param abs   Pointer to location to receive result
 * \return CSS_OK on success, appropriate error otherwise.
 */
typedef css_error (*css_url_resolution_fn)(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs);

/**
 * Callback to be notified of the need for an imported stylesheet
 *
 * \param pw      Client data
 * \param parent  Stylesheet requesting the import
 * \param url     URL of the imported sheet
 * \param media   Applicable media for the imported sheet
 * \return CSS_OK on success, appropriate error otherwise
 *
 * \note This function will be invoked for notification purposes
 *       only. The client may use this to trigger a parallel fetch
 *       of the imported stylesheet. The imported sheet must be
 *       registered with its parent using the post-parse import
 *       registration API.
 */
typedef css_error (*css_import_notification_fn)(void *pw,
		css_stylesheet *parent, lwc_string *url, uint64_t media);

/**
 * Callback use to resolve system colour names to RGB values
 *
 * \param pw     Client data
 * \param name   System colour name
 * \param color  Pointer to location to receive color value
 * \return CSS_OK       on success,
 *         CSS_INVALID  if the name is unknown.
 */
typedef css_error (*css_color_resolution_fn)(void *pw,
		lwc_string *name, css_color *color);

/** System font callback result data. */
typedef struct css_system_font {
	enum css_font_style_e style;
	enum css_font_variant_e variant;
	enum css_font_weight_e weight;
	struct {                  
		css_fixed size;           
		css_unit unit;
	} size;
	struct {                  
		css_fixed size;           
		css_unit unit;
	} line_height;
	/* Note: must be a single family name only */
	lwc_string *family;
} css_system_font;

/**
 * Callback use to resolve system font names to font values
 *
 * \param pw           Client data
 * \param name         System font identifier
 * \param system_font  Pointer to system font descriptor to be filled
 * \return CSS_OK       on success,
 *         CSS_INVALID  if the name is unknown.
 */
typedef css_error (*css_font_resolution_fn)(void *pw,
		lwc_string *name, css_system_font *system_font);

typedef enum css_stylesheet_params_version {
	CSS_STYLESHEET_PARAMS_VERSION_1 = 1
} css_stylesheet_params_version;

/**
 * Parameter block for css_stylesheet_create()
 */
typedef struct css_stylesheet_params {
	/** ABI version of this structure */
	uint32_t params_version;

	/** The language level of the stylesheet */
	css_language_level level;

	/** The charset of the stylesheet data, or NULL to detect */
	const char *charset;
	/** URL of stylesheet */
	const char *url;
	/** Title of stylesheet */
	const char *title;

	/** Permit quirky parsing of stylesheet */
	bool allow_quirks;
	/** This stylesheet is an inline style */
	bool inline_style;

	/** URL resolution function */
	css_url_resolution_fn resolve;
	/** Client private data for resolve */
	void *resolve_pw;

	/** Import notification function */
	css_import_notification_fn import;
	/** Client private data for import */
	void *import_pw;

	/** Colour resolution function */
	css_color_resolution_fn color;
	/** Client private data for color */
	void *color_pw;

	/** Font resolution function */
	css_font_resolution_fn font;
	/** Client private data for font */
	void *font_pw;
} css_stylesheet_params;

css_error css_stylesheet_create(const css_stylesheet_params *params,
		css_stylesheet **stylesheet);
css_error css_stylesheet_destroy(css_stylesheet *sheet);

css_error css_stylesheet_append_data(css_stylesheet *sheet,
		const uint8_t *data, size_t len);
css_error css_stylesheet_data_done(css_stylesheet *sheet);

css_error css_stylesheet_next_pending_import(css_stylesheet *parent,
		lwc_string **url, uint64_t *media);
css_error css_stylesheet_register_import(css_stylesheet *parent,
		css_stylesheet *child);

css_error css_stylesheet_get_language_level(css_stylesheet *sheet,
		css_language_level *level);
css_error css_stylesheet_get_url(css_stylesheet *sheet, const char **url);
css_error css_stylesheet_get_title(css_stylesheet *sheet, const char **title);
css_error css_stylesheet_quirks_allowed(css_stylesheet *sheet, bool *allowed);
css_error css_stylesheet_used_quirks(css_stylesheet *sheet, bool *quirks);

css_error css_stylesheet_get_disabled(css_stylesheet *sheet, bool *disabled);
css_error css_stylesheet_set_disabled(css_stylesheet *sheet, bool disabled);

css_error css_stylesheet_size(css_stylesheet *sheet, size_t *size);

#ifdef __cplusplus
}
#endif

#endif

