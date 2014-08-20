/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
	PDF Plotting
*/

#ifndef NETSURF_PDF_PLOTTERS_H
#define NETSURF_PDF_PLOTTERS_H

#include "desktop/print.h"

extern const struct printer pdf_printer;

/**Start plotting a pdf file*/
bool pdf_begin(struct print_settings *settings);

/**Finish the current page and start a new one*/
bool pdf_next_page(void);

/**Close pdf document and save changes to file*/
void pdf_end(void);

void save_pdf(const char *path);

#endif /*NETSURF_PDF_PLOTTERS_H*/
