/*
 * Copyright (C) 1992-1993 by Joerg Wunsch, Dresden
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 */

#ifndef _IOCTL_FD_H
#define _IOCTL_FD_H

#include <sys/types.h>
#include <sys/ioctl.h>

#define FD_FORMAT_VERSION 110	/* used to validate before formatting */
#define FD_MAX_NSEC 36		/* highest known number of spt - allow for */
				/* 2.88 MB drives */

struct fd_formb {
	int format_version;	/* == FD_FORMAT_VERSION */
	int cyl, head;
	int transfer_rate;	/* fdreg.h: FDC_???KBPS */

	union {
		struct fd_form_data {
			/*
			 * DO NOT CHANGE THE LAYOUT OF THIS STRUCTS
			 * it is hardware-dependant since it exactly
			 * matches the byte sequence to write to FDC
			 * during its `format track' operation
			 */
			u_char secshift; /* 0 -> 128, ...; usually 2 -> 512 */
			u_char nsecs;	/* must be <= FD_MAX_NSEC */
			u_char gaplen;	/* GAP 3 length; usually 84 */
			u_char fillbyte; /* usually 0xf6 */
			struct fd_idfield_data {
				/*
				 * data to write into id fields;
				 * for obscure formats, they mustn't match
				 * the real values (but mostly do)
				 */
				u_char cylno;	/* 0 thru 79 (or 39) */
				u_char headno;	/* 0, or 1 */
				u_char secno;	/* starting at 1! */
				u_char secsize;	/* usually 2 */
			} idfields[FD_MAX_NSEC]; /* 0 <= idx < nsecs used */
		} structured;
		u_char raw[1];	/* to have continuous indexed access */
	} format_info;
};

/* make life easier */
# define fd_formb_secshift   format_info.structured.secshift
# define fd_formb_nsecs      format_info.structured.nsecs
# define fd_formb_gaplen     format_info.structured.gaplen
# define fd_formb_fillbyte   format_info.structured.fillbyte
/* these data must be filled in for(i = 0; i < fd_formb_nsecs; i++) */
# define fd_formb_cylno(i)   format_info.structured.idfields[i].cylno
# define fd_formb_headno(i)  format_info.structured.idfields[i].headno
# define fd_formb_secno(i)   format_info.structured.idfields[i].secno
# define fd_formb_secsize(i) format_info.structured.idfields[i].secsize

struct fd_type {
	int	sectrac;		/* sectors per track         */
	int	secsize;		/* size code for sectors     */
	int	datalen;		/* data len when secsize = 0 */
	int	gap;			/* gap len between sectors   */
	int	tracks;			/* total num of tracks       */
	int	size;			/* size of disk in sectors   */
	int	steptrac;		/* steps per cylinder        */
	int	trans;			/* transfer speed code       */
	int	heads;			/* number of heads	     */
	int     f_gap;                  /* format gap len            */
	int     f_inter;                /* format interleave factor  */
};

#define FD_FORM   _IOW('F', 61, struct fd_formb) /* format a track */
#define FD_GTYPE  _IOR('F', 62, struct fd_type)  /* get drive type */

#endif  /* !def _IOCTL_FD_H */
