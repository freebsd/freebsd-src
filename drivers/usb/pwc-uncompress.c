/* Linux driver for Philips webcam 
   Decompression frontend.
   (C) 1999-2003 Nemosoft Unv. (webcam@smcc.demon.nl)

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
/*
   This is where the decompression routines register and unregister 
   themselves. It also has a decompressor wrapper function.
*/

#include <asm/types.h>

#include "pwc.h"
#include "pwc-uncompress.h"


/* This contains a list of all registered decompressors */
static LIST_HEAD(pwc_decompressor_list);

/* Should the pwc_decompress structure ever change, we increase the 
   version number so that we don't get nasty surprises, or can 
   dynamically adjust our structure.
 */
const int pwc_decompressor_version = PWC_MAJOR;

/* Add decompressor to list, ignoring duplicates */
void pwc_register_decompressor(struct pwc_decompressor *pwcd)
{
	if (pwc_find_decompressor(pwcd->type) == NULL) {
		Trace(TRACE_PWCX, "Adding decompressor for model %d.\n", pwcd->type);
		list_add_tail(&pwcd->pwcd_list, &pwc_decompressor_list);
	}
}

/* Remove decompressor from list */
void pwc_unregister_decompressor(int type)
{
	struct pwc_decompressor *find;
	
	find = pwc_find_decompressor(type);
	if (find != NULL) {
		Trace(TRACE_PWCX, "Removing decompressor for model %d.\n", type);
		list_del(&find->pwcd_list);
	}
}

/* Find decompressor in list */
struct pwc_decompressor *pwc_find_decompressor(int type)
{
	struct list_head *tmp;
	struct pwc_decompressor *pwcd;

	list_for_each(tmp, &pwc_decompressor_list) {
		pwcd  = list_entry(tmp, struct pwc_decompressor, pwcd_list);
		if (pwcd->type == type)
			return pwcd;
	}
	return NULL;
}



int pwc_decompress(struct pwc_device *pdev)
{
	struct pwc_frame_buf *fbuf;
	int n, line, col, stride;
	void *yuv, *image;
	u16 *src;
	u16 *dsty, *dstu, *dstv;

	
	if (pdev == NULL)
		return -EFAULT;
#if defined(__KERNEL__) && defined(PWC_MAGIC)
	if (pdev->magic != PWC_MAGIC) {
		Err("pwc_decompress(): magic failed.\n");
		return -EFAULT;
	}
#endif

	fbuf = pdev->read_frame;
	if (fbuf == NULL)
		return -EFAULT;
	image = pdev->image_ptr[pdev->fill_image];
	if (!image)
		return -EFAULT;
	
	yuv = fbuf->data + pdev->frame_header_size;  /* Skip header */
	if (pdev->vbandlength == 0) { 
		/* Uncompressed mode. We copy the data into the output buffer,
		   using the viewport size (which may be larger than the image
		   size). Unfortunately we have to do a bit of byte stuffing
		   to get the desired output format/size.
		 */
			/* 
			 * We do some byte shuffling here to go from the 
			 * native format to YUV420P.
			 */
			src = (u16 *)yuv;
			n = pdev->view.x * pdev->view.y;

			/* offset in Y plane */
			stride = pdev->view.x * pdev->offset.y + pdev->offset.x;
			dsty = (u16 *)(image + stride);

			/* offsets in U/V planes */
			stride = pdev->view.x * pdev->offset.y / 4 + pdev->offset.x / 2;
			dstu = (u16 *)(image + n +         stride);
			dstv = (u16 *)(image + n + n / 4 + stride);

			/* increment after each line */
			stride = (pdev->view.x - pdev->image.x) / 2; /* u16 is 2 bytes */

			for (line = 0; line < pdev->image.y; line++) {
				for (col = 0; col < pdev->image.x; col += 4) {
					*dsty++ = *src++;
					*dsty++ = *src++;
					if (line & 1)
						*dstv++ = *src++;
					else
						*dstu++ = *src++;
				}
				dsty += stride;
				if (line & 1)
					dstv += (stride >> 1);
				else
					dstu += (stride >> 1);
			}
	}
	else { 
		/* Compressed; the decompressor routines will write the data 
		   in planar format immediately.
		 */
		if (pdev->decompressor)
			pdev->decompressor->decompress(
				&pdev->image, &pdev->view, &pdev->offset,
				yuv, image,
				1,
				pdev->decompress_data, pdev->vbandlength);
		else
			return -ENXIO; /* No such device or address: missing decompressor */
	}
	return 0;
}

/* Make sure these functions are available for the decompressor plugin
   both when this code is compiled into the kernel or as as module.
 */

EXPORT_SYMBOL_NOVERS(pwc_decompressor_version);
EXPORT_SYMBOL(pwc_register_decompressor);
EXPORT_SYMBOL(pwc_unregister_decompressor);
