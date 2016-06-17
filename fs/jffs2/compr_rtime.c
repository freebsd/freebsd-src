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
 * $Id: compr_rtime.c,v 1.5 2001/03/15 15:38:23 dwmw2 Exp $
 *
 *
 * Very simple lz77-ish encoder.
 *
 * Theory of operation: Both encoder and decoder have a list of "last
 * occurances" for every possible source-value; after sending the
 * first source-byte, the second byte indicated the "run" length of
 * matches
 *
 * The algorithm is intended to only send "whole bytes", no bit-messing.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h> 

/* _compress returns the compressed size, -1 if bigger */
int rtime_compress(unsigned char *data_in, unsigned char *cpage_out, 
		   __u32 *sourcelen, __u32 *dstlen)
{
	int positions[256];
	int outpos = 0;
	int pos=0;

	memset(positions,0,sizeof(positions)); 
	
	while (pos < (*sourcelen) && outpos <= (*dstlen)-2) {
		int backpos, runlen=0;
		unsigned char value;
		
		value = data_in[pos];

		cpage_out[outpos++] = data_in[pos++];
		
		backpos = positions[value];
		positions[value]=pos;
		
		while ((backpos < pos) && (pos < (*sourcelen)) &&
		       (data_in[pos]==data_in[backpos++]) && (runlen<255)) {
			pos++;
			runlen++;
		}
		cpage_out[outpos++] = runlen;
	}

	if (outpos >= pos) {
		/* We failed */
		return -1;
	}
	
	/* Tell the caller how much we managed to compress, and how much space it took */
	*sourcelen = pos;
	*dstlen = outpos;
	return 0;
}		   


void rtime_decompress(unsigned char *data_in, unsigned char *cpage_out,
		      __u32 srclen, __u32 destlen)
{
	int positions[256];
	int outpos = 0;
	int pos=0;
	
	memset(positions,0,sizeof(positions)); 
	
	while (outpos<destlen) {
		unsigned char value;
		int backoffs;
		int repeat;
		
		value = data_in[pos++];
		cpage_out[outpos++] = value; /* first the verbatim copied byte */
		repeat = data_in[pos++];
		backoffs = positions[value];
		
		positions[value]=outpos;
		if (repeat) {
			if (backoffs + repeat >= outpos) {
				while(repeat) {
					cpage_out[outpos++] = cpage_out[backoffs++];
					repeat--;
				}
			} else {
				memcpy(&cpage_out[outpos],&cpage_out[backoffs],repeat);
				outpos+=repeat;		
			}
		}
	}		
}		   


