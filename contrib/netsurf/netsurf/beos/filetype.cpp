/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#define __STDBOOL_H__	1
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <Mime.h>
#include <NodeInfo.h>
#include <String.h>

extern "C" {
#include "content/fetch.h"
#include "utils/log.h"
#include "utils/hashtable.h"
#include "utils/utils.h"
}

#include "beos/filetype.h"

static struct {
	const char *type;
	const char *ext1;
	const char *ext2;
} default_types[] = {
	{ "text/plain", "txt", NULL },
	{ "text/html", "htm", "html" },
	{ "text/css", "css", NULL },
	{ "image/gif", "gif", NULL },
	{ "image/jpeg", "jpg", "jpeg" },
	{ "image/png", "png", NULL },
	{ "image/jng", "jng", NULL },
	{ NULL, NULL, NULL }
};

void beos_fetch_filetype_init(void)
{
	BMimeType m;
	status_t err;
	int i;

	// make sure we have basic mime types in the database
	for (i = 0; default_types[i].type; i++) {
		if (m.SetTo(default_types[i].type) < B_OK)
			continue;
		if (m.IsInstalled())
			continue;
		err = m.Install();
		if (err < B_OK) {
			warn_user("Mime", strerror(err));
			continue;
		}
		// the mime db doesn't know about it yet
		BMessage extensions(0UL);
		if (default_types[i].ext1)
			extensions.AddString("extensions", default_types[i].ext1);
		if (default_types[i].ext2)
			extensions.AddString("extensions", default_types[i].ext2);
		err = m.SetFileExtensions(&extensions);
		if (err < B_OK) {
			warn_user("Mime", strerror(err));
		}
	}
}

void beos_fetch_filetype_fin(void)
{
}

const char *fetch_filetype(const char *unix_path)
{
	struct stat statbuf;
	status_t err;
	int i;
	// NOT THREADSAFE
	static char type[B_MIME_TYPE_LENGTH];

	// override reading the mime type for known types
	// avoids getting CSS files as text/x-source-code
	// even though it's the mime sniffer rules that should be fixed.
	BString ext(unix_path);
	ext.Remove(0, ext.FindLast('.') + 1);
	for (i = 0; default_types[i].type; i++) {
		if (ext == default_types[i].ext1)
			return default_types[i].type;
		if (ext == default_types[i].ext2)
			return default_types[i].type;
	}

	BEntry entry(unix_path, true);
	BNode node(&entry);
	err = node.InitCheck();
	if (err < B_OK)
		return "text/plain";

	if (node.IsDirectory())
		return "application/x-netsurf-directory";

	BNodeInfo info(&node);
	err = info.InitCheck();
	if (err < B_OK)
		return "test/plain";

	err = info.GetType(type);
	if (err < B_OK) {
		// not there yet, sniff and retry
		err = update_mime_info(unix_path, false, true, false);
		if (err < B_OK)
			return "text/plain";
		err = info.GetType(type);
		if (err < B_OK)
			return "text/plain";
	}
	
	return type;
}
