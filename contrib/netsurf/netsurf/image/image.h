/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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

/** \file
 * Initialisation/finalisation of image handlers.
 */

#ifndef NETSURF_IMAGE_IMAGE_H_
#define NETSURF_IMAGE_IMAGE_H_

#include "utils/errors.h"

/** Initialise the content handlers for image types.
 */
nserror image_init(void);

/** Common image content handler bitmap plot call.
 *
 * This plots the specified bitmap controlled by the redraw context
 * and specific content redraw data. It is a helper specifically
 * provided for image content handlers redraw callback.
 */
bool image_bitmap_plot(struct bitmap *bitmap,
		       struct content_redraw_data *data, 
		       const struct rect *clip,
		       const struct redraw_context *ctx);

#endif
