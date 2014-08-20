/*
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
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
 * Declaration of content_type enum.
 *
 * The content_type enum is defined here to prevent cyclic dependencies.
 */

#ifndef _NETSURF_DESKTOP_CONTENT_TYPE_H_
#define _NETSURF_DESKTOP_CONTENT_TYPE_H_

#include "utils/config.h"


/** The type of a content. */
typedef enum {
	CONTENT_NONE		= 0x00,

	CONTENT_HTML		= 0x01,
	CONTENT_TEXTPLAIN	= 0x02,
	CONTENT_CSS		= 0x04,

	/** All images */
	CONTENT_IMAGE		= 0x08,

	/** Navigator API Plugins */
	CONTENT_PLUGIN		= 0x10,

	/** Themes (only GTK and RISC OS) */
	CONTENT_THEME		= 0x20,

	/** Javascript */
	CONTENT_JS		= 0x40,
	/** All script types. */
	CONTENT_SCRIPT		= 0x40,

	/** Any content matches */
	CONTENT_ANY		= 0x7f
} content_type;


#endif
