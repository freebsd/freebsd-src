/*
 * Copyright 2013 Vincent Sanders <vince@kyllikki.org>
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
 * Localised gettext message support (implementation).
 *
 * Wrappers for gettext to the internal native language message support.
 */

#include <stdlib.h>

#include "utils/messages.h"
#include "gtk/gettext.h"

char *gettext(const char *msgid)
{
	return dcgettext(NULL, msgid, 0);
}

char *dgettext(const char *domainname, const char *msgid)
{
	return dcgettext(domainname, msgid, 0);
}

char *dcgettext(const char *domainname, const char *msgid, int category)
{
	return (void *)messages_get(msgid);
}
