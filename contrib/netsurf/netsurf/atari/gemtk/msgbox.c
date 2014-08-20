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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gem.h>
#include "gemtk.h"

#ifndef min
# define min(x,y) ((x<y) ? x : y )
#endif

/***
 * Display an message box
 *
 * \param type Valid values: GEMTK_MSG_BOX_CONFIRM, GEMTK_MSG_BOX_ALERT
 * \param msg The message / query to display
 * \return 0 on "No"
 *
 */
short gemtk_msg_box_show(short type, const char * msg)
{
	#define GEMTK_MSG_BOX_STR_SIZE 256
	short retval=0, i=0, z=0, l=0;
	char c;
	int len_msg = strlen(msg);

	// TODO: localize strings
	const char *str_yes = "Yes";
	const char *str_no = "No";
	const char *str_ok = "OK";
	char msg_box_str[GEMTK_MSG_BOX_STR_SIZE];
	char *dst = msg_box_str;

	memset(msg_box_str, 0, GEMTK_MSG_BOX_STR_SIZE);

	strncat(msg_box_str, "[1]", GEMTK_MSG_BOX_STR_SIZE);
	strncat(msg_box_str, "[", GEMTK_MSG_BOX_STR_SIZE);

	dst = msg_box_str + strlen(msg_box_str);

	for (i=0; i<min(len_msg,40*5); i++) {

		c = msg[i];

		if(c==0)
			break;

		if (z==40) {
			if(l==4){
				break;
			}
			z = 0;
			l++;
			*dst = (char)'|';
			dst++;
		}

		if ((c=='\r' || c=='\n') && *dst != '|') {
			if(l==4){
				break;
			}
			z = 0;
			l++;
			*dst = '|';
			dst++;
		}
		else {
			z++;
			*dst = c;
			dst++;
		}
	}
	strncat(msg_box_str, "][", GEMTK_MSG_BOX_STR_SIZE);

	if(type == GEMTK_MSG_BOX_CONFIRM){
		strncat(msg_box_str, str_yes, GEMTK_MSG_BOX_STR_SIZE);
		strncat(msg_box_str, "|", GEMTK_MSG_BOX_STR_SIZE);
		strncat(msg_box_str, str_no, GEMTK_MSG_BOX_STR_SIZE);
	} else {
		strncat(msg_box_str, str_ok, GEMTK_MSG_BOX_STR_SIZE);
	}
	strncat(msg_box_str, "]", GEMTK_MSG_BOX_STR_SIZE);

	retval = form_alert(type, msg_box_str);
	if(type == GEMTK_MSG_BOX_CONFIRM){
		if(retval != 1){
			retval = 0;
		}
	}
	return(retval);

	#undef GEMTK_MSG_BOX_STR_SIZE
}
