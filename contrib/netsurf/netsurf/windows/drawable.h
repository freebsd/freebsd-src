/*
 * Copyright 2011 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef _NETSURF_WINDOWS_DRAWABLE_H_
#define _NETSURF_WINDOWS_DRAWABLE_H_

nserror nsws_create_drawable_class(HINSTANCE hinstance);
HWND nsws_window_create_drawable(HINSTANCE hinstance, HWND hparent, struct gui_window *gw);

#endif /* _NETSURF_WINDOWS_DRAWABLE_H_ */
