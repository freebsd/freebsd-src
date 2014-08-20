/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Knockout rendering (interface).
 */

#ifndef _NETSURF_DESKTOP_KNOCKOUT_H_
#define _NETSURF_DESKTOP_KNOCKOUT_H_

#include "desktop/plotters.h"


bool knockout_plot_start(const struct redraw_context *ctx,
		struct redraw_context *knk_ctx);
bool knockout_plot_end(void);

extern const struct plotter_table knockout_plotters;

#endif
