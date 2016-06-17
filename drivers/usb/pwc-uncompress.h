/* (C) 1999-2003 Nemosoft Unv. (webcam@smcc.demon.nl)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* This file is the bridge between the kernel module and the plugin; it
   describes the structures and datatypes used in both modules. Any
   significant change should be reflected by increasing the 
   pwc_decompressor_version major number.
 */
#ifndef PWC_UNCOMPRESS_H
#define PWC_UNCOMPRESS_H

#include <linux/config.h>
#include <linux/list.h>

#include "pwc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The decompressor structure. 
   Every type of decompressor registers itself with the main module. 
   When a device is opened, it looks up the correct compressor, and
   uses that when a compressed video mode is requested.
 */
struct pwc_decompressor
{
	int  type;		/* type of camera (645, 680, etc) */
	int  table_size;	/* memory needed */

	void (* init)(int release, void *buffer, void *table);	/* Initialization routine; should be called after each set_video_mode */
	void (* exit)(void);	/* Cleanup routine */
	void (* decompress)(struct pwc_coord *image, struct pwc_coord *view, struct pwc_coord *offset,
                            void *src, void *dst, int planar,
	                    void *table, int bandlength);
	void (* lock)(void);	/* make sure module cannot be unloaded */
	void (* unlock)(void);	/* release lock on module */

	struct list_head pwcd_list;
};


/* Our structure version number. Is set to the version number major */
extern const int pwc_decompressor_version;

/* Adds decompressor to list, based on its 'type' field (which matches the 'type' field in pwc_device; ignores any double requests */
extern void pwc_register_decompressor(struct pwc_decompressor *pwcd);
/* Removes decompressor, based on the type number */
extern void pwc_unregister_decompressor(int type);
/* Returns pointer to decompressor struct, or NULL if it doesn't exist */
extern struct pwc_decompressor *pwc_find_decompressor(int type);

#ifdef CONFIG_USB_PWCX
/* If the decompressor is compiled in, we must call these manually */
extern int usb_pwcx_init(void);
extern void usb_pwcx_exit(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
