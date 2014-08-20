/*
 * Copyright 2008-2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <proto/requester.h>
#include <classes/requester.h>

#include "amiga/gui.h"
#include "amiga/utf8.h"
#include "desktop/cookie_manager.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

void warn_user(const char *warning, const char *detail)
{
	Object *req = NULL;
	char *utf8warning = ami_utf8_easy(messages_get(warning));
	STRPTR bodytext = NULL;

	LOG(("%s %s", warning, detail));

	bodytext = ASPrintf("\33b%s\33n\n%s",
		utf8warning != NULL ? utf8warning : warning, detail);

	req = NewObject(REQUESTER_GetClass(), NULL,
		REQ_Type,               REQTYPE_INFO,
		REQ_TitleText,          messages_get("NetSurf"),
		REQ_BodyText,           bodytext,
		REQ_GadgetText,         messages_get("OK"),
#ifdef __amigaos4__
		REQ_VarArgs,			
		REQ_Image,				(struct Image *)REQIMAGE_WARNING,
		/* REQ_CharSet,			106, */
#endif
		TAG_DONE);

	if (req) {
		LONG result = IDoMethod(req, RM_OPENREQ, NULL, NULL, scrn);
		DisposeObject(req);
	}

	if(bodytext) FreeVec(bodytext);
	if(utf8warning) free(utf8warning);
}

int32 ami_warn_user_multi(const char *body, const char *opt1, const char *opt2, struct Window *win)
{
	int res = 0;
	char *utf8text = ami_utf8_easy(body);
	char *utf8gadget1 = ami_utf8_easy(messages_get(opt1));
	char *utf8gadget2 = ami_utf8_easy(messages_get(opt2));
	char *utf8gadgets = ASPrintf("%s|%s", utf8gadget1, utf8gadget2);
	free(utf8gadget1);
	free(utf8gadget2);

	res = TimedDosRequesterTags(TDR_ImageType, TDRIMAGE_WARNING,
		TDR_TitleString, messages_get("NetSurf"),
		TDR_FormatString, utf8text,
		TDR_GadgetString, utf8gadgets,
		TDR_Window, win,
		TAG_DONE);

	if(utf8text) free(utf8text);
	if(utf8gadgets) FreeVec(utf8gadgets);
	
	return res;
}

void die(const char *error)
{
	TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_ERROR,
							TDR_TitleString,messages_get("NetSurf"),
							TDR_GadgetString,messages_get("OK"),
//							TDR_CharSet,106,
							TDR_FormatString,"%s",
							TDR_Arg1,error,
							TAG_DONE);
	exit(1);
}

char *url_to_path(const char *url)
{
	char *unesc, *slash, *colon, *url2;

	if (strncmp(url, "file://", SLEN("file://")) != 0)
		return NULL;

	url += SLEN("file://");

	if (strncmp(url, "localhost", SLEN("localhost")) == 0)
		url += SLEN("localhost");

	if (strncmp(url, "/", SLEN("/")) == 0)
		url += SLEN("/");

	if(*url == '\0')
		return NULL; /* file:/// is not a valid path */

	url2 = malloc(strlen(url) + 2);
	strcpy(url2, url);

	colon = strchr(url2, ':');
	if(colon == NULL)
	{
		if(slash = strchr(url2, '/'))
		{
			*slash = ':';
		}
		else
		{
			int len = strlen(url2);
			url2[len] = ':';
			url2[len + 1] = '\0';
		}
	}

	if(url_unescape(url2,&unesc) == URL_FUNC_OK)
		return unesc;

	return (char *)url2;
}

char *path_to_url(const char *path)
{
	char *colon = NULL;
	char *r = NULL;
	char newpath[1024 + strlen(path)];
	BPTR lock = 0;

	if(lock = Lock(path, MODE_OLDFILE))
	{
		DevNameFromLock(lock, newpath, sizeof newpath, DN_FULLPATH);
		UnLock(lock);
	}
	else strlcpy(newpath, path, sizeof newpath);

	r = malloc(strlen(newpath) + SLEN("file:///") + 1);

	if(colon = strchr(newpath, ':')) *colon = '/';

	strcpy(r, "file:///");
	strcat(r, newpath);

	return r;
}

/**
 * returns a string with escape chars translated.
 * (based on remove_underscores from utils.c)
 */

char *translate_escape_chars(const char *s)
{
	size_t i, ii, len;
	char *ret;
	len = strlen(s);
	ret = malloc(len + 1);
	if (ret == NULL)
		return NULL;
	for (i = 0, ii = 0; i < len; i++) {
		if (s[i] != '\\') {
			ret[ii++] = s[i];
		}
		else if (s[i+1] == 'n') {
			ret[ii++] = '\n';
			i++;
		}
	}
	ret[ii] = '\0';
	return ret;
}
