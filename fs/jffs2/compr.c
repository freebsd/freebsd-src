/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * Created by Arjan van de Ven <arjanv@redhat.com>
 *
 * The original JFFS, from which the design for JFFS2 was derived,
 * was designed and implemented by Axis Communications AB.
 *
 * The contents of this file are subject to the Red Hat eCos Public
 * License Version 1.1 (the "Licence"); you may not use this file
 * except in compliance with the Licence.  You may obtain a copy of
 * the Licence at http://www.redhat.com/
 *
 * Software distributed under the Licence is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing rights and
 * limitations under the Licence.
 *
 * The Original Code is JFFS2 - Journalling Flash File System, version 2
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the RHEPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the RHEPL or the GPL.
 *
 * $Id: compr.c,v 1.17 2001/09/23 09:56:46 dwmw2 Exp $
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/jffs2.h>

int zlib_compress(unsigned char *data_in, unsigned char *cpage_out, __u32 *sourcelen, __u32 *dstlen);
void zlib_decompress(unsigned char *data_in, unsigned char *cpage_out, __u32 srclen, __u32 destlen);
int rtime_compress(unsigned char *data_in, unsigned char *cpage_out, __u32 *sourcelen, __u32 *dstlen);
void rtime_decompress(unsigned char *data_in, unsigned char *cpage_out, __u32 srclen, __u32 destlen);
int rubinmips_compress(unsigned char *data_in, unsigned char *cpage_out, __u32 *sourcelen, __u32 *dstlen);
void rubinmips_decompress(unsigned char *data_in, unsigned char *cpage_out, __u32 srclen, __u32 destlen);
int dynrubin_compress(unsigned char *data_in, unsigned char *cpage_out, __u32 *sourcelen, __u32 *dstlen);
void dynrubin_decompress(unsigned char *data_in, unsigned char *cpage_out, __u32 srclen, __u32 destlen);


/* jffs2_compress:
 * @data: Pointer to uncompressed data
 * @cdata: Pointer to buffer for compressed data
 * @datalen: On entry, holds the amount of data available for compression.
 *	On exit, expected to hold the amount of data actually compressed.
 * @cdatalen: On entry, holds the amount of space available for compressed
 *	data. On exit, expected to hold the actual size of the compressed
 *	data.
 *
 * Returns: Byte to be stored with data indicating compression type used.
 * Zero is used to show that the data could not be compressed - the 
 * compressed version was actually larger than the original.
 *
 * If the cdata buffer isn't large enough to hold all the uncompressed data,
 * jffs2_compress should compress as much as will fit, and should set 
 * *datalen accordingly to show the amount of data which were compressed.
 */
unsigned char jffs2_compress(unsigned char *data_in, unsigned char *cpage_out, 
		    __u32 *datalen, __u32 *cdatalen)
{
	int ret;

	ret = zlib_compress(data_in, cpage_out, datalen, cdatalen);
	if (!ret) {
		return JFFS2_COMPR_ZLIB;
	}
#if 0 /* Disabled 23/9/1. With zlib it hardly ever gets a look in */
	ret = dynrubin_compress(data_in, cpage_out, datalen, cdatalen);
	if (!ret) {
		return JFFS2_COMPR_DYNRUBIN;
	}
#endif
#if 0 /* Disabled 26/2/1. Obsoleted by dynrubin */
	ret = rubinmips_compress(data_in, cpage_out, datalen, cdatalen);
	if (!ret) {
		return JFFS2_COMPR_RUBINMIPS;
	}
#endif
	/* rtime does manage to recompress already-compressed data */
	ret = rtime_compress(data_in, cpage_out, datalen, cdatalen);
	if (!ret) {
		return JFFS2_COMPR_RTIME;
	}
#if 0
	/* We don't need to copy. Let the caller special-case the COMPR_NONE case. */
	/* If we get here, no compression is going to work */
	/* But we might want to use the fragmentation part -- Arjan */
	memcpy(cpage_out,data_in,min(*datalen,*cdatalen));
	if (*datalen > *cdatalen)
		*datalen = *cdatalen;
#endif		
	return JFFS2_COMPR_NONE; /* We failed to compress */

}


int jffs2_decompress(unsigned char comprtype, unsigned char *cdata_in, 
		     unsigned char *data_out, __u32 cdatalen, __u32 datalen)
{
	switch (comprtype) {
	case JFFS2_COMPR_NONE:
		/* This should be special-cased elsewhere, but we might as well deal with it */
		memcpy(data_out, cdata_in, datalen);
		break;

	case JFFS2_COMPR_ZERO:
		memset(data_out, 0, datalen);
		break;

	case JFFS2_COMPR_ZLIB:
		zlib_decompress(cdata_in, data_out, cdatalen, datalen);
		break;

	case JFFS2_COMPR_RTIME:
		rtime_decompress(cdata_in, data_out, cdatalen, datalen);
		break;

	case JFFS2_COMPR_RUBINMIPS:
#if 0 /* Disabled 23/9/1 */
		rubinmips_decompress(cdata_in, data_out, cdatalen, datalen);
#else
		printk(KERN_WARNING "JFFS2: Rubinmips compression encountered but support not compiled in!\n");
#endif
		break;
	case JFFS2_COMPR_DYNRUBIN:
#if 1 /* Phase this one out */
		dynrubin_decompress(cdata_in, data_out, cdatalen, datalen);
#else
		printk(KERN_WARNING "JFFS2: Dynrubin compression encountered but support not compiled in!\n");
#endif
		break;

	default:
		printk(KERN_NOTICE "Unknown JFFS2 compression type 0x%02x\n", comprtype);
		return -EIO;
	}
	return 0;
}
