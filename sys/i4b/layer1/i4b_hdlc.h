/*-
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_hdlc.h - software-HDLC header file
 *	--------------------------------------
 *
 *	$Id: i4b_hdlc.h,v 1.5 2000/08/28 07:41:19 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_hdlc.h,v 1.6 2005/01/06 22:18:19 imp Exp $
 *
 *	last edit-date: [Wed Jul 19 09:41:13 2000]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_HDLC_H_
#define _I4B_HDLC_H_

extern const u_short HDLC_FCS_TAB[256];
extern const u_short HDLC_BIT_TAB[256];

/*---------------------------------------------------------------------------*
 *	HDLC_DECODE
 *	===========
 *
 *	u_char:  flag, blevel
 *	u_short: crc, ib, tmp, tmp2, len
 *
 *	next: 'continue' or 'goto xxx'
 *
 *	cfr: complete frame
 *	nfr: new frame
 *	     NOTE: must setup 'len' and 'dst', so that 'dst' may be written
 *		   at most 'len' times.
 *
 *	rab: abort
 *	rdd: read data (read byte is stored in 'tmp2')
 *	rdo: overflow
 *
 *	d: dummy
 *
 *	NOTE: setting flag to '0' and len to '0' => recover from rdu
 *	NOTE: bits[8 .. ] of tmp2 may be used to store custom data/flags
 *	NOTE: these variables have to be 'suspended' / 'resumed' somehow:
 *		flag, blevel, crc, ib, tmp, len
 *	NOTE: zero is default value for all variables.
 *	NOTE: each time 'dst' is written, 'len' is decreased by one.
 *---------------------------------------------------------------------------*/

#define HDLC_DECODE(dst, len, tmp, tmp2, blevel, ib, crc, flag,	rddcmd, nfrcmd,	\
		 cfrcmd, rabcmd, rdocmd, nextcmd, d) 				\
										\
	rddcmd;									\
										\
	ib  += HDLC_BIT_TAB[(u_char)tmp2];					\
										\
	if ((u_char)ib >= 5)							\
	{									\
		if (ib & 0x20)		/* de-stuff (msb) */			\
		{								\
			if ((u_char)tmp2 == 0x7e) goto j0##d;			\
			tmp2 += tmp2 & 0x7f;					\
			blevel--;						\
										\
			if ((ib += 0x100) & 0xc) tmp2 |= 1; /* */		\
		}								\
										\
		ib &= ~0xe0;							\
										\
		if ((u_char)ib == 6)	/* flag seq (lsb) */			\
		{								\
		 j0##d:	if (flag >= 2)						\
			{							\
				len += (4 - flag) & 3;	/* remove CRC bytes */	\
				crc ^= 0xf0b8;					\
				cfrcmd;						\
				len = 0;					\
			}							\
										\
			flag   = 1;						\
										\
			blevel = (ib >> 8) & 0xf;				\
			tmp    = ((u_char)tmp2) >> blevel;			\
			blevel = 8 - blevel;					\
										\
			ib >>= 12;						\
										\
			nextcmd;						\
		}								\
		if ((u_char)ib >= 7)	/* abort (msb & lsb) */			\
		{								\
			if (flag >= 2)						\
			{							\
				rabcmd;						\
				len = 0;					\
			}							\
										\
			flag = 0;						\
										\
			ib >>= 12;						\
										\
			nextcmd;						\
		}								\
		if ((u_char)ib == 5)	/* de-stuff (lsb) */			\
		{								\
			tmp2 = (tmp2 | (tmp2 + 1)) & ~0x1;			\
			blevel--;						\
		}								\
		if (blevel > 7)		/* EO - bits */				\
		{								\
			tmp |= (u_char)tmp2 >> (8 - (blevel &= 7));		\
										\
			ib >>= 12;						\
										\
			nextcmd;						\
		}								\
	}									\
										\
	tmp |= (u_char)tmp2 << blevel;						\
										\
	if (!len--)								\
	{									\
		len++;								\
										\
		if (!flag++) { flag--; goto j5##d;} /* hunt mode */		\
										\
		switch (flag)							\
		{   case 2: 		/* new frame */				\
			nfrcmd;							\
			crc = -1;						\
			if (!len--) { len++; flag++; goto j4##d; }		\
			goto j3##d;						\
		    case 3:		/* CRC (lsb's) */			\
		    case 4:		/* CRC (msb's) */			\
			goto j4##d;						\
		    case 5:		/* RDO */				\
			rdocmd;							\
			flag = 0;						\
			break;							\
		}								\
	}									\
	else									\
	{ 									\
	 j3##d:	dst = (u_char)tmp;						\
	 j4##d: crc = (HDLC_FCS_TAB[(u_char)(tmp ^ crc)] ^ (u_char)(crc >> 8));	\
	}									\
										\
 j5##d:	ib >>= 12;								\
	tmp >>= 8;								\

/*------ end of HDLC_DECODE -------------------------------------------------*/


/*---------------------------------------------------------------------------*
 *	HDLC_ENCODE
 *	===========
 *
 *	u_char:  flag, src
 *	u_short: tmp2, blevel, ib, crc, len
 *	u_int:   tmp
 *
 *	gfr: This is the place where you free the last [mbuf] chain, and get
 *	     the next one. If a mbuf is available the code should setup 'len'
 *	     and 'src' so that 'src' may be read 'len' times. If no mbuf is
 *	     available leave 'len' and 'src' untouched.
 *
 *	wrd: write data (output = (u_char)tmp)
 *
 *	d: dummy
 *
 *	NOTE: setting flag to '-2' and len to '0' => abort bytes will be sent
 *	NOTE: these variables have to be 'suspended' / 'resumed' somehow:
 *		flag, blevel, crc, ib, tmp, len
 *	NOTE: zero is default value for all variables.
 *	NOTE: each time 'src' is read, 'len' is decreased by one.
 *	NOTE: neither cmd's should exit through 'goto' or 'break' statements.
 *---------------------------------------------------------------------------*/

#define HDLC_ENCODE(src, len, tmp, tmp2, blevel, ib, crc, flag, gfrcmd, wrdcmd, d) \
										\
	if (blevel >= 0x800) { blevel -= 0x800; goto j4##d; }			\
										\
	if (!len--)								\
	{									\
		len++;								\
										\
		switch(++flag)							\
		{ default:			/* abort */			\
			tmp  = blevel = 0;	/* zero is default */		\
			tmp2 = 0xff;						\
			goto j3##d;						\
		  case 1:			/* 1st time FS */		\
		  case 2:			/* 2nd time FS */		\
			tmp2 = 0x7e;						\
			goto j3##d;						\
		  case 3:							\
			gfrcmd;			/* get new frame */		\
			if (!len--)						\
			{							\
				len++;						\
				flag--;		/* don't proceed */		\
				tmp2 = 0x7e;					\
				goto j3##d;	/* final FS */			\
			}							\
			else							\
			{							\
				crc = -1;					\
				ib  = 0;					\
				goto j1##d; 	/* first byte */		\
			}							\
		  case 4:							\
			crc ^= -1;						\
			tmp2 = (u_char)crc;					\
			goto j2##d;		/* CRC (lsb's) */		\
		  case 5:							\
			tmp2  = (u_char)(crc >> 8);				\
			flag  = 1;						\
			goto j2##d;		/* CRC (msb's) */		\
		}								\
	}									\
 	else									\
  	{ j1##d	:								\
		tmp2 = (u_char)src;						\
		crc =(HDLC_FCS_TAB[(u_char)(crc ^ tmp2)] ^ (u_char)(crc >> 8));	\
	  j2##d:								\
										\
		ib >>= 12;							\
		ib  += HDLC_BIT_TAB[(u_char)tmp2];				\
										\
		if ((u_char)ib >= 5)	/* stuffing */				\
		{								\
			blevel &= ~0xff;					\
										\
			if (ib & 0xc0)		/* bit stuff (msb) */		\
			{							\
				tmp2 += tmp2 & (0xff * (ib & 0xc0));		\
				ib %= 0x5000;					\
				blevel++;					\
			}							\
										\
			ib &= ~0xf0;						\
										\
			if ((u_char)ib >= 5)	/* bit stuff (lsb) */		\
			{							\
				tmp2 += tmp2 & ~0x1f >> ((ib - (ib >> 8) + 1)	\
								& 7);		\
				blevel++;					\
										\
				if ((u_char)ib >= 10)	/* bit stuff (msb) */	\
				{						\
					tmp2 += tmp2 & ~0x7ff >> ((ib - 	\
							(ib >> 8) + 1) & 7);	\
					blevel++;				\
				}						\
				if (ib & 0x8000)	/* bit walk */		\
				{						\
					ib = ((u_char)ib % 5) << 12;		\
				}						\
			}							\
										\
			tmp    |= tmp2 << (u_char)(blevel >> 8);		\
			blevel += (u_char)blevel << 8;				\
		}								\
		else		/* no stuffing */				\
		{								\
		  j3##d:tmp    |= tmp2 << (u_char)(blevel >> 8);		\
		}								\
	}									\
										\
 j4##d:	wrdcmd;									\
	tmp >>= 8;								\

/*------ end of HDLC_ENCODE -------------------------------------------------*/


#endif /* _I4B_HDLC_H_ */

