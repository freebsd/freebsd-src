/*
 * Copyright 2010 <ole@monochrom.net>
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
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <support.h>
#include <mint/osbind.h>
#include <mint/cookie.h>

#include "utils/log.h"
#include "atari/osspec.h"
#include "atari/gemtk/gemtk.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

NS_ATARI_SYSINFO atari_sysinfo;

void init_os_info(void)
{
	int16_t out[4];
   unsigned long cookie_FSMC = 0;

	atari_sysinfo.gemdos_version = Sversion();

	if( tos_getcookie (C_FSMC, &cookie_FSMC ) == C_FOUND ) {
		atari_sysinfo.gdos_FSMC = 1;
	} else {
		atari_sysinfo.gdos_FSMC = 0;
	}
	atari_sysinfo.large_sfont_pxh = 13;
	atari_sysinfo.medium_sfont_pxh = 6;
	atari_sysinfo.small_sfont_pxh = 4;
	/* todo: detect if system font is monospaced */
	atari_sysinfo.sfont_monospaced = true;
	if( appl_xgetinfo(AES_LARGEFONT, &out[0],  &out[1],  &out[2], &out[3] ) > 0 ){
		atari_sysinfo.large_sfont_pxh = out[0];
	}
	if( appl_xgetinfo(AES_SMALLFONT, &out[0],  &out[1],  &out[2], &out[3] ) > 0 ){
		atari_sysinfo.small_sfont_pxh = out[0];
	}
	atari_sysinfo.aes_max_win_title_len = 79;
	if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
		if (sys_NAES()) {
			atari_sysinfo.aes_max_win_title_len = 127;
		}
		if (sys_XAAES()) {
			atari_sysinfo.aes_max_win_title_len = 200;
		}
	}
}

int tos_getcookie(long tag, long * value)
{
	COOKIE * cptr;
	long oldsp;

	if( atari_sysinfo.gemdos_version > TOS4VER ){
		return( Getcookie(tag, value) );
	}

	cptr = (COOKIE*)Setexc(0x0168, -1L);
	if(cptr != NULL) {
		do {
			if( cptr->c == tag ){
				if(cptr->v != 0 ){
					if( value != NULL ){
						*value = cptr->v;
					}
					return( C_FOUND );
				}
			}
		} while( (cptr++)->c != 0L );
	}
	return( C_NOTFOUND );
}

/*

 a fixed version of realpath() which returns valid
 paths for TOS which have no U: drive

*/

char * gemdos_realpath(const char * path, char * rpath)
{
	char work[PATH_MAX+1];
	char * r;


	if (rpath == NULL) {
		return (NULL);
	}

	// Check if the path is already absolute:
	if(path[1] == ':'){
		strcpy(rpath, path);
		return(rpath);
	}

	LOG(("realpath in: %s\n", path));
	r = realpath(path, work);
	if (r != NULL) {
		int e = unx2dos((const char *)r, rpath);
		LOG(("realpath out: %s\n", rpath));
		return(rpath);
	}
	else {
		LOG(("realpath out: NULL!\n"));
	}
	return (NULL);
}

