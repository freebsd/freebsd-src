/*
 * Copyright (c) 1993 Atsushi Murai (amurai@spec.co.jp)
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
 *    must display the following acknowledgement:
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Atsushi Murai(amurai@spec.co.jp)``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)iso_rrip.h
 *	$Id: iso_rrip.h,v 1.4 1993/11/07 17:46:03 wollman Exp $
 */

#ifndef _ISOFS_ISO_RRIP_H_
#define _ISOFS_ISO_RRIP_H_ 1

/*
 *	Analyze function flag
 */
#define	ISO_SUSP_ATTR		0x0001
#define	ISO_SUSP_DEVICE		0x0002
#define	ISO_SUSP_SLINK		0x0004
#define	ISO_SUSP_ALTNAME	0x0008
#define	ISO_SUSP_CLINK		0x0010
#define	ISO_SUSP_PLINK		0x0020
#define	ISO_SUSP_RELDIR		0x0040
#define	ISO_SUSP_TSTAMP		0x0080
#define	ISO_SUSP_IDFLAG		0x0100
#define	ISO_SUSP_EXFLAG		0x0200
#define	ISO_SUSP_UNKNOWN	0x0400

typedef struct {
	ISO_RRIP_INODE	inode;
	u_short		iso_altlen;	/* Alt Name length */
	u_short		iso_symlen;	/* Symbol Name length */
	char		*iso_altname;	/* Alt Name (no Null terminated ) */
	char		*iso_symname;	/* Symbol Name (no NULL termninated )*/
} ISO_RRIP_ANALYZE;
#endif /* _ISOFS_ISO_RRIP_H_ */
