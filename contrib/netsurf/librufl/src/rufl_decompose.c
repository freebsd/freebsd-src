/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stdio.h>

#include "oslib/font.h"

#include "rufl_internal.h"

/**
 * Process a Draw path object
 *
 * \param path Pointer to path block
 * \param funcs Struct of callback functions
 * \param user User pointer to be passed to callbacks
 * \return Pointer to word after this path, or NULL to terminate processing
 */
static int *process_path(int *path, struct rufl_decomp_funcs *funcs,
		void *user)
{
	/* skip forward to style entry */
	path += 9;

	/* skip dash pattern */
	if (path[0] & (1<<7))
		path += path[2] + 2;

	/* and move to start of path components */
	path++;

	while (path[0] != 0) {
		switch (path[0]) {
		case 2: /* Move to */
			if (funcs->move_to((os_coord *)(path + 1), user))
				return NULL;
			path += 3;
			break;
		case 5: /* Close path */
			path++;
			break;
		case 6: /* Cubic Bezier to */
			if (funcs->cubic_to((os_coord *)(path + 1),
					    (os_coord *)(path + 3),
					    (os_coord *)(path + 5),
					    user))
				return NULL;
			path += 7;
			break;
		case 8: /* Line to */
			if (funcs->line_to((os_coord *)(path + 1), user))
				return NULL;
			path += 3;
			break;
		default: /* Anything else is broken */
			assert(0);
		}
	}

	/* + 1 to account for tag 0 - end of path */
	return path + 1;
}

rufl_code rufl_decompose_glyph(const char *font_family,
		rufl_style font_style, unsigned int font_size,
		const char *string, size_t len,
		struct rufl_decomp_funcs *funcs, void *user)
{
	int *buf, *p, *ep;
	int buf_size;
	char *buf_end;
	rufl_code err;

	/* Get required buffer size */
	rufl_fm_error = xfont_switch_output_to_buffer(
			font_NO_OUTPUT | font_ADD_HINTS, (byte *)8, 0);
	if (rufl_fm_error) {
		LOG("xfont_switch_output_to_buffer: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	err = rufl_paint(font_family, font_style, font_size, string, len,
			0, 0, rufl_BLEND_FONT);
	if (err) {
		/* reset font redirection - too bad if this fails */
		xfont_switch_output_to_buffer(0, 0, 0);
		return err;
	}

	rufl_fm_error = xfont_switch_output_to_buffer(0, NULL, &buf_end);
	if (rufl_fm_error) {
		LOG("xfont_switch_output_to_buffer: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}
	buf_size = buf_end - (char *)NULL;

	/* Allocate and initialise buffer */
	buf = malloc(buf_size);
	if (!buf) {
		LOG("Failed to allocate decompose buffer of size %i", buf_size);
		return rufl_OUT_OF_MEMORY;
	}
	buf[0] = 0;
	buf[1] = buf_size - 8;

	/* Populate buffer */
	rufl_fm_error = xfont_switch_output_to_buffer(
			font_ADD_HINTS, (byte *)buf, 0);
	if (rufl_fm_error) {
		LOG("xfont_switch_output_to_buffer: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		free(buf);
		return rufl_FONT_MANAGER_ERROR;
	}

	err = rufl_paint(font_family, font_style, font_size, string, len,
			0, 0, rufl_BLEND_FONT);
	if (err) {
		/* reset font redirection - too bad if this fails */
		xfont_switch_output_to_buffer(0, 0, 0);
		free(buf);
		return err;
	}

	rufl_fm_error = xfont_switch_output_to_buffer(0, 0, &buf_end);
	if (rufl_fm_error) {
		LOG("xfont_switch_output_to_buffer: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		free(buf);
		return rufl_FONT_MANAGER_ERROR;
	}
	ep = (int *)(void *)buf_end;

	/* Parse buffer, calling callbacks as required */
	for (p = buf; p < ep;) {
		if (p[0] != 2) {
			LOG("Object type %d not known", p[0]);
			break;
		}

		p = process_path(p, funcs, user);

		/* Have the callbacks asked for us to stop? */
		if (p == NULL)
			break;
	}

	free(buf);

	return rufl_OK;
}
