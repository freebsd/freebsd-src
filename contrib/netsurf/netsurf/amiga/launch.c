/*
 * Copyright 2008-10 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Fetching of data from a file (implementation).
 */

#include "amiga/os3support.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/openurl.h>

#include "utils/nsoption.h"
#include "utils/url.h"

struct Library *OpenURLBase = NULL;
struct OpenURLIFace *IOpenURL = NULL;

struct MinList ami_unsupportedprotocols;

struct ami_protocol
{
	struct MinNode node;
	char *protocol;
};

struct ami_protocol *ami_openurl_add_protocol(const char *url)
{
	struct ami_protocol *ami_p =
		(struct ami_protocol *)AllocVecTagList(sizeof(struct ami_protocol), NULL);

	if(url_scheme(url, &ami_p->protocol) != URL_FUNC_OK)
	{
		FreeVec(ami_p);
		return NULL;
	}

	AddTail((struct List *)&ami_unsupportedprotocols, (struct Node *)ami_p);
	return ami_p;
}

void ami_openurl_free_list(struct MinList *list)
{
	struct ami_protocol *node;
	struct ami_protocol *nnode;

	if(IsMinListEmpty(list)) return;
	node = (struct ami_protocol *)GetHead((struct List *)list);

	do
	{
		nnode=(struct ami_protocol *)GetSucc((struct Node *)node);

		Remove((struct Node *)node);
		if(node->protocol) free(node->protocol);
		FreeVec(node);
		node = NULL;
	}while(node=nnode);
}

BOOL ami_openurl_check_list(struct MinList *list, const char *url)
{
	struct ami_protocol *node;
	struct ami_protocol *nnode;

	if(IsMinListEmpty(list)) return FALSE;
	node = (struct ami_protocol *)GetHead((struct List *)list);

	do
	{
		nnode=(struct ami_protocol *)GetSucc((struct Node *)node);

		if(!strncasecmp(url, node->protocol, strlen(node->protocol)))
			return TRUE;
	}while(node=nnode);

	return FALSE;
}

/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void ami_openurl_open(void)
{
	if(nsoption_bool(use_openurl_lib)) {
		if(OpenURLBase = OpenLibrary("openurl.library",0))
			IOpenURL = (struct OpenURLIFace *)GetInterface(OpenURLBase,"main",1,NULL);
	}

	NewMinList(&ami_unsupportedprotocols);
	ami_openurl_add_protocol("javascript:");
}

void ami_openurl_close(const char *scheme)
{
	if(IOpenURL) DropInterface((struct Interface *)IOpenURL);
	if(OpenURLBase) CloseLibrary(OpenURLBase);

	ami_openurl_free_list(&ami_unsupportedprotocols);
}

void gui_launch_url(const char *url)
{
	APTR procwin = SetProcWindow((APTR)-1L);
	char *launchurl = NULL;

	if(ami_openurl_check_list(&ami_unsupportedprotocols, url) == FALSE)
	{
		if(IOpenURL)
		{
			URL_OpenA((STRPTR)url,NULL);
		} else {
			if(launchurl = ASPrintf("URL:%s",url)) {
				BPTR fptr = Open(launchurl,MODE_OLDFILE);
				if(fptr) Close(fptr);
					else ami_openurl_add_protocol(url);
				FreeVec(launchurl);
			}
		}
	}

	SetProcWindow(procwin);		
}
