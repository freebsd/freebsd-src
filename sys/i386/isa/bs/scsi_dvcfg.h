/*	$Id$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  Copyright (c) 1994, 1995, 1996 Naofumi HONDA.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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
 */

#ifndef	_SCSI_DVCFG_H_
#define	_SCSI_DVCFG_H_

/* common dvcfg flags defitions for bs, ncv, stg */

#define	DVF_SCSI_SYNC		0x01
#define	DVF_SCSI_DISC		0x02
#define	DVF_SCSI_WAIT		0x04
#define	DVF_SCSI_LINK		0x08
#define	DVF_SCSI_QTAG		0x10
#define	DVF_SCSI_SP0		0x100
#define	DVF_SCSI_NOPARITY	0x200
#define	DVF_SCSI_SAVESP		0x400
#define	DVF_SCSI_SP1		0x800
#define	DVF_SCSI_PERIOD(XXX)	(((XXX) >> 24) & 0xff)
#define	DVF_SCSI_OFFSET(XXX)	(((XXX) >> 16) & 0xff)
#define DVF_SCSI_SYNCMASK	0xffff0000

#define	DVF_SCSI_DEFCFG	(DVF_SCSI_SYNC | DVF_SCSI_NOPARITY | DVF_SCSI_SYNCMASK)

#define	DVF_SCSI_BITS		"\020\13fssp\12noparity\11nosat\005qtag\004cmdlnk\003wait\002disc\001sync"

#endif	/* !_SCSI_DVCFG_H_ */
