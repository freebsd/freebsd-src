/*
 * Copyright (c) 1997, 1998 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_tel_ioctl.h telephony interface ioctls
 *	------------------------------------------
 *
 *	$Id: i4b_tel_ioctl.h,v 1.5 1998/12/14 10:31:58 hm Exp $ 
 *
 *      last edit-date: [Sat Dec  5 18:37:36 1998]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_TEL_IOCTL_H_
#define _I4B_TEL_IOCTL_H_

/* supported audio format conversions for /dev/i4btelXX devices */

#define CVT_NONE	0		/* no format conversion	*/
#define CVT_ALAW2ULAW	1		/* kernel A-law, userland mu-law */
      
/*---------------------------------------------------------------------------*
 *	get / set audio format 
 *---------------------------------------------------------------------------*/

#define	I4B_TEL_GETAUDIOFMT	_IOR('A', 0, int)
#define	I4B_TEL_SETAUDIOFMT	_IOW('A', 1, int)
#define	I4B_TEL_EMPTYINPUTQUEUE	_IOW('A', 2, int)

#endif /* _I4B_TEL_IOCTL_H_ */
