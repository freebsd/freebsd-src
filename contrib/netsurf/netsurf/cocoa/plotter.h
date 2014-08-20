/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#ifndef COCOA_PLOTTER_H
#define COCOA_PLOTTER_H

#import <Cocoa/Cocoa.h>
#import "desktop/plot_style.h"
#import "cocoa/coordinates.h"

extern const struct plotter_table cocoa_plotters;

NSColor *cocoa_convert_colour( colour clr );

void cocoa_update_scale_factor( void );

void cocoa_set_clip( NSRect rect );

#endif
