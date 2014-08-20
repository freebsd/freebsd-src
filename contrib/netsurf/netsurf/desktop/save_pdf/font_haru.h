/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
 * Copyright 2009 John Tytgat <joty@netsurf-browser.org>
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
  * Font handling in Haru pdf documents (interface).
  */
 
#ifndef _NETSURF_RENDER_FONT_HARU_H_
#define _NETSURF_RENDER_FONT_HARU_H_

#include <hpdf.h>

#include "render/font.h"
#include "desktop/plot_style.h"
 
bool haru_nsfont_apply_style(const plot_font_style_t *fstyle,
			      	HPDF_Doc doc, HPDF_Page page,
	  			HPDF_Font *font, HPDF_REAL *font_size);

void haru_nsfont_set_scale(float s);

extern const struct font_functions haru_nsfont;

#endif
