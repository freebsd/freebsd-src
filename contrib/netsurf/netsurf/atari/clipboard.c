/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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
 *
 * Module Description:
 *
 *
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <mint/osbind.h>
#include <cflib.h>
#include "atari/clipboard.h"
#include "atari/gemtk/gemtk.h"


static int filesize(char * path)
{
	FILE 	*f;
	int 	fs;

	f = fopen( path, "r+b");
	if(!f)
		return(-1);

	fseek(f, 0L, SEEK_END);
	fs = ftell(f);
	fclose(f);

	return(fs);
}

int scrap_txt_write(char *str)
{
	scrap_wtxt(str);


    // Send SC_CHANGED message:
    gemtk_send_msg(SC_CHANGED, 0, 2, 0, 0, 0, 0);

    return(0);

}

char *scrap_txt_read(void)
{
	char * buf = NULL;
	char path[80];
	int file;
	int len;

	if (get_scrapdir (path))
	{
		strcat (path, "scrap.txt");
		len = filesize(path);
		if(len > 0){
			if ((file = (int) Fopen (path, 0)) >= 0)
			{
				buf = malloc(len);
				if(buf){
					len = Fread (file, len, buf);
					Fclose (file);
					buf[len] = '\0';
					return buf;
				}
			}
		}
	}

}

