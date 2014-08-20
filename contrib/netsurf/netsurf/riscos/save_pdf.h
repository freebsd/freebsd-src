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

#ifndef _NETSURF_RISCOS_SAVE_PDF_H_
#define _NETSURF_RISCOS_SAVE_PDF_H_

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

struct hlcache_handle;

bool save_as_pdf(struct hlcache_handle *h, const char *path);

#endif /* WITH_PDF_EXPORT */

#endif
