/*
 * /src/NTP/ntp-4/kernel/sys/parsestreams.h,v 4.4 1998/06/14 21:09:32 kardel RELEASE_19990228_A
 *
 * parsestreams.h,v 4.4 1998/06/14 21:09:32 kardel RELEASE_19990228_A
 *
 * Copyright (c) 1989-1998 by Frank Kardel
 * Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 *                                   
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#if	!(defined(lint) || defined(__GNUC__))
  static char sysparsehrcsid[] = "parsestreams.h,v 4.4 1998/06/14 21:09:32 kardel RELEASE_19990228_A";
#endif

#undef PARSEKERNEL
#if defined(KERNEL) || defined(_KERNEL)
#ifndef PARSESTREAM
#define PARSESTREAM
#endif
#endif
#if defined(PARSESTREAM) && defined(HAVE_SYS_STREAM_H)
#define PARSEKERNEL

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#include <sys/ppsclock.h>

#ifndef NTP_NEED_BOPS
#define NTP_NEED_BOPS
#endif

#if defined(PARSESTREAM) && (defined(_sun) || defined(__sun)) && defined(HAVE_SYS_STREAM_H)
/*
 * Sorry, but in SunOS 4.x AND Solaris 2.x kernels there are no
 * mem* operations. I don't want them - bcopy, bzero
 * are fine in the kernel
 */
#undef HAVE_STRING_H	/* don't include that at kernel level - prototype mismatch in Solaris 2.6 */
#include "ntp_string.h"
#else
#include <stdio.h>
#endif

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

#define DD_OPEN    0x00000001
#define DD_CLOSE   0x00000002
#define DD_RPUT    0x00000004
#define DD_WPUT    0x00000008
#define DD_RSVC    0x00000010
#define DD_PARSE   0x00000020
#define DD_INSTALL 0x00000040
#define DD_ISR     0x00000080
#define DD_RAWDCF  0x00000100

extern int parsedebug;

#ifdef DEBUG_PARSE

#define parseprintf(X, Y) if ((X) & parsedebug) printf Y

#else

#define parseprintf(X, Y)

#endif
#endif

/*
 * parsestreams.h,v
 * Revision 4.4  1998/06/14 21:09:32  kardel
 * Sun acc cleanup
 *
 * Revision 4.3  1998/06/13 18:14:32  kardel
 * make mem*() to b*() mapping magic work on Solaris too
 *
 * Revision 4.2  1998/06/13 15:16:22  kardel
 * fix mem*() to b*() function macro emulation
 *
 * Revision 4.1  1998/06/13 11:50:37  kardel
 * STREAM macro gone in favor of HAVE_SYS_STREAM_H
 *
 * Revision 4.0  1998/04/10 19:51:30  kardel
 * Start 4.0 release version numbering
 *
 * Revision 1.2  1998/04/10 19:27:42  kardel
 * initial NTP VERSION 4 integration of PARSE with GPS166 binary support
 *
 */
