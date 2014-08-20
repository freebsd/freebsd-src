/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2011 Things Made Out Of Other Things Ltd.
 * Written by James Montgomerie <jamie@th.ingsmadeoutofotherthin.gs>
 */

#include <string.h>

#include "select/font_face.h"

static void font_faces_srcs_destroy(css_font_face *font_face)
{
	uint32_t i;
	css_font_face_src *srcs = font_face->srcs;

	for (i = 0; i < font_face->n_srcs; ++i) {
		if (srcs[i].location != NULL) {
			lwc_string_unref(srcs[i].location);
		}
	}
	
	free(srcs);
	font_face->srcs = NULL;
}

static const css_font_face default_font_face = {
	NULL,
	NULL,
	0,
	{ (CSS_FONT_WEIGHT_NORMAL << 2) | CSS_FONT_STYLE_NORMAL }
};

/**
 * Create a font-face
 *
 * \param result  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_BADPARM on bad parameters.
 */
css_error css__font_face_create(css_font_face **result)
{
	css_font_face *f;
	
	if (result == NULL)
		return CSS_BADPARM;
	
	f = malloc(sizeof(css_font_face));
	if (f == NULL)
		return CSS_NOMEM;
	
	memcpy(f, &default_font_face, sizeof(css_font_face));
	
	*result = f;
	
	return CSS_OK;
}

/**
 * Destroy a font-face
 *
 * \param font_face  Font-face to destroy
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css__font_face_destroy(css_font_face *font_face)
{	
	if (font_face == NULL)
		return CSS_BADPARM;

	if (font_face->font_family != NULL)
		lwc_string_unref(font_face->font_family);
	
	if (font_face->srcs != NULL)
		font_faces_srcs_destroy(font_face);

	free(font_face);
	
	return CSS_OK;
}


/**
 * Set a font-face's font-family name
 *
 * \param font_face    The font-face
 * \param font_family  Font-family name
 * \param result       Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters.
 */
css_error css__font_face_set_font_family(css_font_face *font_face,
		lwc_string *font_family)
{
	if (font_face == NULL || font_family == NULL)
		return CSS_BADPARM;
	
	if (font_face->font_family != NULL)
		lwc_string_unref(font_face->font_family);
		
	font_face->font_family = lwc_string_ref(font_family);

	return CSS_OK;
}

/**
 * Get a font-face's font-family name
 *
 * \param font_face  The font-face
 * \param result     Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters.
 */
css_error css_font_face_get_font_family(const css_font_face *font_face,
		lwc_string **font_family)
{
	if (font_face == NULL || font_family == NULL)
		return CSS_BADPARM;
	
	*font_family = font_face->font_family;
	
	return CSS_OK;
}

/**
 * Get the style of font for a font-face.
 *
 * \param src  The font-face
 * \return The style, as a css_font_style_e
 */
uint8_t css_font_face_font_style(const css_font_face *font_face)
{
	return font_face->bits[0] & 0x3;
}

/**
 * Get the weight of font for a font-face.
 *
 * \param src  The font-face
 * \return The style, as a css_font_weight_e
 */
uint8_t css_font_face_font_weight(const css_font_face *font_face)
{
	return (font_face->bits[0] >> 2) & 0xf;
}

/**
 * Get the number of potential src locations for a font-face
 *
 * \param font_face  The font-face
 * \param count      Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters.
 */
css_error css_font_face_count_srcs(const css_font_face *font_face, 
		uint32_t *count)
{
	if (font_face == NULL || count == NULL)
		return CSS_BADPARM;

	*count = font_face->n_srcs;
	return CSS_OK;
}

/**
 * Get a specific src location from a font-face
 *
 * \param font_face  The font-face
 * \param index	     The index for the wanted src.
 * \param count      Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters.
 */
css_error css_font_face_get_src(const css_font_face *font_face,
		uint32_t index, const css_font_face_src **src)
{
	if (font_face == NULL || src == NULL || index >= font_face->n_srcs)
		return CSS_BADPARM;

	*src = &(font_face->srcs[index]);
	
	return CSS_OK;
}

/**
 * Get the location for a font-face src.
 *
 * \param font_face  The font-face
 * \param count      Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_BADPARM on bad parameters.
 *
 * \note  The type of location (local or URL) can be gathered from 
 *        css_font_face_src_location_type, and the format of font (if specified)
 *        from css_font_face_src_format.
 */
css_error css_font_face_src_get_location(const css_font_face_src *src,
		lwc_string **location)
{
	if (src == NULL || location == NULL) 
		return CSS_BADPARM;
	
	*location = src->location;

	return CSS_OK;	
}

/**
 * Get the location type for a font-face src.
 *
 * \param src  The font-face src
 * \return The location type
 */
css_font_face_location_type css_font_face_src_location_type(
		const css_font_face_src *src)
{
	return src->bits[0] & 0x3;
}

/**
 * Get the format of font for a font-face src.
 *
 * \param src  The font-face src
 * \return The format, if specified
 */
css_font_face_format css_font_face_src_format(const css_font_face_src *src)
{
	return (src->bits[0] >> 2) & 0x1f;
}

/**
 * Set a font-faces array of srcs.
 *
 * \param font_face  The font-face 
 * \param srcs	     The array of css_font_face_srcs
 * \param n_srcs     The count of css_font_face_srcs in the array 
 * \return The format, if specified
 */
css_error css__font_face_set_srcs(css_font_face *font_face,
		css_font_face_src *srcs, uint32_t n_srcs)
{
	if (font_face->srcs != NULL)
		font_faces_srcs_destroy(font_face);
	
	font_face->srcs = srcs;
	font_face->n_srcs = n_srcs;
	
	return CSS_OK;
}


