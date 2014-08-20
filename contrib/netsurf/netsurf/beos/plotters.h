/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (BeOS interface).
 */

#ifndef NETSURF_BEOS_PLOTTERS_H
#define NETSURF_BEOS_PLOTTERS_H 1

extern "C" {

struct plotter_table;

extern const struct plotter_table nsbeos_plotters;

}

#include <View.h>

extern BView *current_view;

extern BView *nsbeos_current_gc(void);
extern BView *nsbeos_current_gc_lock(void);
extern void nsbeos_current_gc_unlock(void);
extern void nsbeos_current_gc_set(BView *view);

rgb_color nsbeos_rgb_colour(colour c);
void nsbeos_set_colour(colour c);
void nsbeos_plot_caret(int x, int y, int h);

#endif /* NETSURF_GTK_PLOTTERS_H */
