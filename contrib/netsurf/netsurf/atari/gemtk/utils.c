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
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <mt_gem.h>
#include "gemtk.h"

/* -------------------------------------------------------------------------- */
/* GEM Utillity functions:                                                    */
/* -------------------------------------------------------------------------- */

unsigned short _systype_v;
unsigned short _systype (void)
{
    int32_t * cptr = NULL;
    _systype_v = SYS_TOS;

    cptr = (int32_t *)Setexc(0x0168, -1L);
    if (cptr == NULL ) {
        return _systype_v;   /* stone old TOS without any cookie support */
    }
    while (*cptr) {
        if (*cptr == C_MgMc || *cptr == C_MgMx ) {
            _systype_v = (_systype_v & ~0xF) | SYS_MAGIC;
        } else if (*cptr == C_MiNT ) {
            _systype_v = (_systype_v & ~0xF) | SYS_MINT;
        } else if (*cptr == C_Gnva /* Gnva */ ) {
            _systype_v |= SYS_GENEVA;
        } else if (*cptr == C_nAES /* nAES */ ) {
            _systype_v |= SYS_NAES;
        }
        cptr += 2;
    }
    if (_systype_v & SYS_MINT) { /* check for XaAES */
        short out = 0, u;
        if (wind_get (0, (((short)'X') <<8)|'A', &out, &u,&u,&u) && out) {
            _systype_v |= SYS_XAAES;
        }
    }
    return _systype_v;
}

bool gemtk_rc_intersect_ro(GRECT *a, GRECT *b)
{
    GRECT r1, r2;

    r1 = *a;
    r2 = *b;

    return((bool)rc_intersect(&r1, &r2));
}


typedef struct {
    char *unshift;
    char *shift;
    char *capslock;
} KEYTAB;

int gemtk_keybd2ascii( int keybd, int shift)
{

    KEYTAB *key;
    key = (KEYTAB *)Keytbl( (char*)-1, (char*)-1, (char*)-1);
    return (shift)?key->shift[keybd>>8]:key->unshift[keybd>>8];
}


void gemtk_clip_grect(VdiHdl vh, GRECT *rect)
{
	PXY pxy[2];

	pxy[0].p_x = rect->g_x;
	pxy[0].p_y = rect->g_y;
	pxy[1].p_x = pxy[0].p_x + rect->g_w - 1;
	pxy[1].p_y = pxy[0].p_y + rect->g_h - 1;

	vs_clip_pxy(vh, pxy);
}

/** Send an Message to a GUIWIN using AES message pipe
* \param win the GUIWIN which shall receive the message
* \param msg_type the WM_ message definition
* \param a the 4th parameter to appl_write
* \param b the 5th parameter to appl_write
* \param c the 6th parameter to appl_write
* \param d the 7th parameter to appl_write
*/
void gemtk_send_msg(short msg_type, short data2, short data3, short data4,
                     short data5, short data6, short data7)
{
    short msg[8];

    msg[0] = msg_type;
    msg[1] = gl_apid;
    msg[2] = data2;
    msg[3] = data3;
    msg[4] = data4;
    msg[5] = data5;
    msg[6] = data6;
    msg[7] = data7;

    appl_write(gl_apid, 16, &msg);
}


void gemtk_wind_get_str(short aes_handle, short mode, char *str, int len)
{
	char tmp_str[255];

    // TODO: remove or implement function

	if(len>255) {
		len = 255;
	}

	memset(str, 0, len);
	return;
	/*

	wind_get(aes_handle, mode, (short)(((unsigned long)tmp_str)>>16),
			(short)(((unsigned long)tmp_str) & 0xffff), 0, 0);

	strncpy(str, tmp_str, len);
	*/
}


