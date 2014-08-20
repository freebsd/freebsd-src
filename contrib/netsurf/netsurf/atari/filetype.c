/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/log.h"
#include "content/fetch.h"

#include "atari/filetype.h"

/**
 * filetype -- determine the MIME type of a local file
 */
const char *fetch_filetype(const char *unix_path)
{
	int l;
	char * res = (char*)"text/html";
	l = strlen(unix_path);

	LOG(("unix path: %s", unix_path));

	/* This line is added for devlopment versions running from the root dir: */
	if( strchr( unix_path, (int)'.' ) ){
		if (2 < l && strcasecmp(unix_path + l - 3, "f79") == 0)
			res = (char*)"text/css";
		else if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
			res = (char*)"text/css";
		else if (2 < l && strcasecmp(unix_path + l - 3, "jpg") == 0)
			res = (char*)"image/jpeg";
		else if (3 < l && strcasecmp(unix_path + l - 4, "jpeg") == 0)
			res = (char*)"image/jpeg";
		else if (2 < l && strcasecmp(unix_path + l - 3, "gif") == 0)
			res = (char*)"image/gif";
		else if (2 < l && strcasecmp(unix_path + l - 3, "png") == 0)
			res = (char*)"image/png";
		else if (2 < l && strcasecmp(unix_path + l - 3, "jng") == 0)
			res = (char*)"image/jng";
		else if (2 < l && strcasecmp(unix_path + l - 3, "svg") == 0)
			res = (char*)"image/svg";
		else if (2 < l && strcasecmp(unix_path + l - 3, "txt") == 0)
			res = (char*)"text/plain";
	} else {
		int n=0;
		int c;
		FILE * fp;
		char buffer[16];
		fp = fopen( unix_path, "r" );
		if( fp ){
			do {
				c = fgetc (fp);
				if( c != EOF )
					buffer[n] = (char)c;
				else
					buffer[n] = 0;
				n++;
			} while (c != EOF && n<15);
			fclose( fp );
			if( n > 0 ){
				if( n > 5 && strncasecmp("GIF89", buffer, 5) == 0 )
					res = (char*)"image/gif";
				else if( n > 4 && strncasecmp("PNG", &buffer[1], 3) ==0 )
					res = (char*)"image/png";
				else if( n > 10 && strncasecmp("JFIF", &buffer[5], 4) == 0 )
					res = (char*)"image/jpeg";
			}
		}
	}

error:
	LOG(("mime type: %s", res ));
	return( res );
}


char *fetch_mimetype(const char *ro_path)
{
	return strdup("text/plain");
}
