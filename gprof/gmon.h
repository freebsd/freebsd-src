/*
 * Copyright (c) 1983, 1991, 1993, 2001
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
#ifndef gmon_h
#define gmon_h

/* Size of the 4.4BSD gmon header */
#define GMON_HDRSIZE_BSD44_32 (4 + 4 + 4 + 4 + 4 + (3 * 4))
#define GMON_HDRSIZE_BSD44_64 (8 + 8 + 4 + 4 + 4 + (3 * 4))

/* *INDENT-OFF* */
/* For documentation purposes only.

   struct raw_phdr
    {
      char low_pc[sizeof(void *)];   -- base pc address of sample buffer
      char high_pc[sizeof(void *)];  -- max pc address of sampled buffer
      char ncnt[4];		     -- size of sample buffer (plus this
				        header)

      char version[4];		     -- version number
      char profrate[4];		     -- profiling clock rate
      char spare[3*4];		     -- reserved
    };
*/
/* *INDENT-ON* */

#define GMONVERSION     0x00051879

/* Size of the old BSD gmon header */
#define GMON_HDRSIZE_OLDBSD_32 (4 + 4 + 4) 

/* FIXME: Checking host compiler defines here means that we can't
   use a cross gprof alpha OSF.  */
#if defined(__alpha__) && defined (__osf__) 
#define GMON_HDRSIZE_OLDBSD_64 (8 + 8 + 4 + 4)
#else
#define GMON_HDRSIZE_OLDBSD_64 (8 + 8 + 4)
#endif

/* *INDENT-OFF* */
/* For documentation purposes only.

  struct old_raw_phdr
    {
      char low_pc[sizeof(void *)];  -- base pc address of sample buffer
      char high_pc[sizeof(void *)]  -- max pc address of sampled buffer
      char ncnt[4];		    -- size of sample buffer (plus this
				       header)

      if defined (__alpha__) && defined (__osf__)
      char pad[4];		    -- DEC's OSF v3.0 uses 4 bytes of padding
				    -- to bring the header to a size that is a
				    -- multiple of 8.
      endif
    };
*/
/* *INDENT-ON* */

/*
 * Histogram counters are unsigned shorts:
 */
#define	HISTCOUNTER unsigned short

/*
 * Fraction of text space to allocate for histogram counters here, 1/2:
 */
#define	HISTFRACTION	2

/*
 * Fraction of text space to allocate for from hash buckets.  The
 * value of HASHFRACTION is based on the minimum number of bytes of
 * separation between two subroutine call points in the object code.
 * Given MIN_SUBR_SEPARATION bytes of separation the value of
 * HASHFRACTION is calculated as:
 *
 *      HASHFRACTION = MIN_SUBR_SEPARATION / (2 * sizeof(short) - 1);
 *
 * For the VAX, the shortest two call sequence is:
 *
 *      calls   $0,(r0)
 *      calls   $0,(r0)
 *
 * which is separated by only three bytes, thus HASHFRACTION is
 * calculated as:
 *
 *      HASHFRACTION = 3 / (2 * 2 - 1) = 1
 *
 * Note that the division above rounds down, thus if MIN_SUBR_FRACTION
 * is less than three, this algorithm will not work!
 */
#define	HASHFRACTION 1

/*
 * Percent of text space to allocate for tostructs with a minimum:
 */
#define ARCDENSITY	2
#define MINARCS		50

struct tostruct
  {
    char *selfpc;
    int count;
    unsigned short link;
  };

/*
 * A raw arc, with pointers to the calling site and the called site
 * and a count.  Everything is defined in terms of characters so
 * as to get a packed representation (otherwise, different compilers
 * might introduce different padding):
 */

/* *INDENT-OFF* */
/* For documentation purposes only.

  struct raw_arc
    {
      char from_pc[sizeof(void *)];
      char self_pc[sizeof(void *)];
      char count[sizeof(long)];
    };
*/
/* *INDENT-ON* */

/*
 * General rounding functions:
 */
#define ROUNDDOWN(x,y)	(((x)/(y))*(y))
#define ROUNDUP(x,y)	((((x)+(y)-1)/(y))*(y))

#endif /* gmon_h */
