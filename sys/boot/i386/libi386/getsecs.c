/*
 * $Id$
 * From: $NetBSD: getsecs.c,v 1.1.1.1 1997/03/14 02:40:32 perry Exp $
 */

/* extracted from netbsd:sys/arch/i386/netboot/misc.c */

#include <sys/types.h>
#include <stand.h>

#include "libi386.h"

extern int biosgetrtc __P((u_long*));

time_t
time(time_t *tloc) {
  /*
   * Return the current time (of day) in seconds.
   * XXX should be extended to do it "more right" perhaps?
   * XXX uses undocumented BCD support from libstand.
   */

  u_long t;
  time_t sec;

  if(biosgetrtc(&t))
      panic("RTC invalid");

  sec = bcd2bin(t & 0xff);
  sec *= 60;
  t >>= 8;
  sec += bcd2bin(t & 0xff);
  sec *= 60;
  t >>= 8;
  sec += bcd2bin(t & 0xff);

  if (tloc != NULL)
      *tloc = sec;
  return(sec);
}
