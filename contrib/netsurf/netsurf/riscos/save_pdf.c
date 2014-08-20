/*
 * Copyright 2008 John Tytgat <joty@netsurf-browser.org>
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
 * Export a content as a PDF file (implementation).
 */

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

#include <stdbool.h>
#include "oslib/osfile.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/print.h"
#include "desktop/save_pdf/font_haru.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "riscos/save_pdf.h"
#include "utils/log.h"
#include "utils/config.h"

/**
 * Export a content as a PDF file.
 *
 * \param  h     content to export
 * \param  path  path to save PDF as
 * \return  true on success, false on error and error reported
 */
bool save_as_pdf(hlcache_handle *h, const char *path)
{
	struct print_settings *psettings;
	
	psettings = print_make_settings(PRINT_DEFAULT, path, &haru_nsfont);
	if (psettings == NULL)
		return false;

	if (!print_basic_run(h, &pdf_printer, psettings))
		return false;

	xosfile_set_type(path, 0xadf);

	return true;
}

#endif
