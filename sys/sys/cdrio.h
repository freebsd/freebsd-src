/*-
 * Copyright (c) 2000 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/cdrio.h,v 1.1 2000/01/06 22:38:33 sos Exp $
 */

#ifndef	_SYS_CDRIO_H_
#define	_SYS_CDRIO_H_

#include <sys/ioccom.h>

struct cdr_track {
        int track_type;         	/* type of this track */
#define CDR_DB_RAW              0x0     /* 2352 bytes of raw data */
#define CDR_DB_RAW_PQ           0x1     /* 2368 bytes raw data + P/Q subchan */
#define CDR_DB_RAW_PW           0x2     /* 2448 bytes raw data + P-W subchan */
#define CDR_DB_RAW_PW_R         0x3     /* 2448 bytes raw data + P-W raw sub */
#define CDR_DB_RES_4            0x4     /* reserved */
#define CDR_DB_RES_5            0x5     /* reserved */
#define CDR_DB_RES_6            0x6     /* reserved */
#define CDR_DB_VS_7             0x7     /* vendor specific */
#define CDR_DB_ROM_MODE1        0x8     /* 2048 bytes Mode 1 (ISO/IEC 10149) */
#define CDR_DB_ROM_MODE2        0x9     /* 2336 bytes Mode 2 (ISO/IEC 10149) */
#define CDR_DB_XA_MODE1         0xa     /* 2048 bytes Mode 1 (CD-ROM XA 1) */
#define CDR_DB_XA_MODE2_F1      0xb     /* 2056 bytes Mode 2 (CD-ROM XA 1) */
#define CDR_DB_XA_MODE2_F2      0xc     /* 2324 bytes Mode 2 (CD-ROM XA 2) */
#define CDR_DB_XA_MODE2_MIX     0xd     /* 2332 bytes Mode 2 (CD-ROM XA 1/2) */
#define CDR_DB_RES_14           0xe     /* reserved */
#define CDR_DB_VS_15            0xf     /* vendor specific */

	int preemp;			/* preemphasis if audio track*/
	int test_write;			/* use test writes, laser turned off */
};

#define CDRIOCBLANK		_IOW('c', 100, int)
#define CDRIOCNEXTWRITEABLEADDR	_IOR('c', 101, int)

#define CDRIOCOPENDISK		_IO('c', 102)
#define CDRIOCCLOSEDISK		_IO('c', 103)

#define CDRIOCOPENTRACK		_IOW('c', 104, struct cdr_track)
#define CDRIOCCLOSETRACK	_IO('c', 105)

#define CDRIOCWRITESPEED	_IOW('c', 106, int)
#define CDRIOCGETBLOCKSIZE	_IOR('c', 107, int)
#define CDRIOCSETBLOCKSIZE	_IOW('c', 108, int)

#endif /* !_SYS_CDRIO_H_ */
