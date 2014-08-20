/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#include "desktop/gui.h"

#include "atari/encoding.h"


/* TODO: this need a rework..., encoding to atari st doesn|t always work.
( gui_add_to_clipboard...) */
nserror utf8_to_local_encoding(const char *string,
				       size_t len,
				       char **result)
{
	nserror r;
	r = utf8_to_enc(string, "ATARIST", len, result);
	if(r != NSERROR_OK) {
		r = utf8_to_enc(string, "UTF-8", len, result);
		assert( r == NSERROR_OK );
	}
	return r;
}


nserror utf8_from_local_encoding(const char *string, size_t len, char **result)
{
	return utf8_from_enc(string, "ATARIST", len, result, NULL);
}


/* borrowed from highwire project: */
static const uint16_t Atari_to_Unicode[] = {
	/*         .0     .1     .2     .3     .4     .5     .6     .7     .8     .9     .A     .B     .C     .D     .E     .F */
	/* 7F */ 0x0394,
	/* 8. */ 0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
	/* 9. */ 0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x00DF,0x0192,
	/* A. */ 0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
	/* B. */ 0x00C3,0x00F5,0x00D8,0x00F8,0x0153,0x0152,0x00C0,0x00C3,0x00D5,0x00A8,0x00B4,0x2020,0x00B6,0x00A9,0x00AE,0x2122,
	/* C. */ 0x0133,0x0132,0x05D0,0x05D1,0x05D2,0x05D3,0x05D4,0x05D5,0x05D6,0x05D7,0x05D8,0x05D9,0x05DB,0x05DC,0x05DE,0x05E0,
	/* D. */ 0x05E1,0x05E2,0x05E4,0x05E6,0x05E7,0x05E8,0x05E9,0x05EA,0x05DF,0x05DA,0x05DD,0x05E3,0x05E5,0x00A7,0x2038,0x221E,
	/* E. */ 0x03B1,0x03B2,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,0x03A6,0x0398,0x03A9,0x03B4,0x222E,0x03C6,0x2208,0x2229,
	/* F. */ 0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,0x00B0,0x2022,0x00B7,0x221A,0x207F,0x00B2,0x00B3,0x00AF
};
#define BEG_Atari_to_Unicode 0x7F

int atari_to_ucs4(unsigned char atari)
{
	uint32_t ucs4 = 0xfffd;
	if ( atari >= BEG_Atari_to_Unicode && atari <= 0xFE )
		ucs4 = (int)Atari_to_Unicode[(short)atari - BEG_Atari_to_Unicode];
	else
		ucs4 = (int)atari;
	return( ucs4 );
}


static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local_encoding,
	.local_to_utf8 = utf8_from_local_encoding,
};

struct gui_utf8_table *atari_utf8_table = &utf8_table;
