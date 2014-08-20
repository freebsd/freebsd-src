/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "utils/utf8.h"
#include "desktop/gui.h"
#include <proto/exec.h>
#include <proto/diskfont.h>
#include <diskfont/diskfonttag.h>

#include "amiga/utf8.h"

nserror utf8_from_local_encoding(const char *string, size_t len, char **result)
{
	const char *encname = "ISO-8859-1";

#ifdef __amigaos4__
	LONG charset;

	charset = GetDiskFontCtrl(DFCTRL_CHARSET);
	encname = (const char *) ObtainCharsetInfo(DFCS_NUMBER, charset, DFCS_MIMENAME);
#endif
	
	return utf8_from_enc(string,encname,len,result,NULL);
}

nserror utf8_to_local_encoding(const char *string, size_t len, char **result)
{
	const char *encname = "ISO-8859-1";

#ifdef __amigaos4__
	LONG charset;

	charset = GetDiskFontCtrl(DFCTRL_CHARSET);
	encname = (const char *) ObtainCharsetInfo(DFCS_NUMBER, charset, DFCS_MIMENAME);
#endif

	return utf8_to_enc(string,encname,len,result);
}

void ami_utf8_free(char *ptr)
{
	if(ptr) free(ptr);
}

char *ami_utf8_easy(const char *string)
{
	char *localtext;

	if(utf8_to_local_encoding(string,strlen(string),&localtext) == NSERROR_OK)
	{
		return localtext;
	}
	else
	{
		return strdup(string);
	}
}

char *ami_to_utf8_easy(const char *string)
{
	char *localtext;

	if(utf8_from_local_encoding(string,strlen(string),&localtext) == NSERROR_OK)
	{
		return localtext;
	}
	else
	{
		return strdup(string);
	}
}


static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local_encoding,
	.local_to_utf8 = utf8_from_local_encoding,
};

struct gui_utf8_table *amiga_utf8_table = &utf8_table;
