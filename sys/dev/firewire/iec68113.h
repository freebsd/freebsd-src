/*
 * Copyright (c) 1998-2001 Katsushi Kobayashi and Hidetoshi Shimokawa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */
struct ciphdr {
	u_int8_t src:6,
		 form0:1,	/* 0 */
		 eoh0:1;	/* 0 */
	u_int8_t len;
	u_int8_t :2,
		 sph:1,
		 qpc:3,
		 fn:1;
	u_int8_t dbc;
	u_int8_t fmt:6,
#define CIP_FMT_DVCR	0
#define CIP_FMT_MPEG	(1<<5)
		 form1:1,	/* 0 */
		 eoh1:1;	/* 1 */
	union {
		struct {
			u_int8_t  :2,
				stype:5,
#define	CIP_STYPE_SD	0
#define	CIP_STYPE_SDL	1
#define	CIP_STYPE_HD	2
		  		fs:1;		/* 50/60 field system
								NTSC/PAL */
	  		u_int16_t cyc:16;	/* take care of byte order! */
		} __attribute__ ((packed)) dv;
		u_int8_t bytes[3];
	} fdf;

};
struct dvdbc{
	u_int8_t arb:4,		/* Arbitrary bit */
		 :1,		/* Reserved */
		 sct:3;		/* Section type */
#define	DV_SCT_HEADER	0
#define	DV_SCT_SUBCODE	1
#define	DV_SCT_VAUX	2
#define	DV_SCT_AUDIO	3
#define	DV_SCT_VIDEO	4
	u_int8_t :3,
		 fsc:1,		/* ID of a DIF block in each channel */
		 dseq:4;	/* DIF sequence number */
	u_int8_t dbn;		/* DIF block number */
	u_int8_t payload[77];
#define	DV_DSF_12	0x80	/* PAL: payload[0] in Header DIF */
};
