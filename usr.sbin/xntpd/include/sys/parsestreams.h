/*
 * /src/NTP/REPOSITORY/v3/include/sys/parsestreams.h,v 3.11 1994/01/25 19:04:30 kardel Exp
 *
 * parsestreams.h,v 3.11 1994/01/25 19:04:30 kardel Exp
 *
 * Copyright (c) 1989,1990,1991,1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                   
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#if	!(defined(lint) || defined(__GNUC__))
  static char parse77hrcsid[]="parsestreams.h,v 3.11 1994/01/25 19:04:30 kardel Exp";
#endif

#undef PARSEKERNEL
#if defined(KERNEL) || defined(_KERNEL)
#ifndef PARSESTREAM
#define PARSESTREAM
#endif
#endif
#if defined(PARSESTREAM) && defined(STREAM)
#define PARSEKERNEL
#include <sys/ppsclock.h>

struct parsestream		/* parse module local data */
{
  queue_t       *parse_queue;	/* read stream for this channel */
  queue_t	*parse_dqueue;	/* driver queue entry (PPS support) */
  unsigned long  parse_status;  /* operation flags */
  void          *parse_data;	/* local data space (PPS support) */
  parse_t	 parse_io;	/* io structure */
  struct ppsclockev parse_ppsclockev; /* copy of last pps event */
};

typedef struct parsestream parsestream_t;

#define PARSE_ENABLE	0x0001

/*--------------- debugging support ---------------------------------*/

#ifdef DEBUG_DCF

extern int parsedebug;

#define DD_OPEN    0x00000001
#define DD_CLOSE   0x00000002
#define DD_RPUT    0x00000004
#define DD_WPUT    0x00000008
#define DD_RSVC    0x00000010
#define DD_PARSE   0x00000020
#define DD_INSTALL 0x00000040
#define DD_ISR     0x00000080
#define DD_RAWDCF  0x00000100

#define parseprintf(X, Y) if ((X) & parsedebug) printf Y

#else

#define parseprintf(X, Y)

#endif
#endif
