/*
 * /src/NTP/ntp-4/libntp/gpstolfp.c,v 4.3 1999/02/28 11:42:44 kardel RELEASE_19990228_A
 *
 * $Created: Sun Jun 28 16:30:38 1998 $
 *
 * Copyright (C) 1998 by Frank Kardel
 */
#include "ntp_fp.h"

#define GPSORIGIN	ULONG_CONST(2524953600)	/* NTP origin - GPS origin in seconds */
#define SECSPERWEEK	(unsigned)(604800)	/* seconds per week - GPS tells us about weeks */
#define GPSWRAP		990	/* assume week count less than this in the previous epoch */

void
gpstolfp(
	 int weeks,
	 int days,
	 unsigned long  seconds,
	 l_fp * lfp
	 )
{
  if (weeks < GPSWRAP)
    {
      weeks += 1024;
    }

  lfp->l_ui = weeks * SECSPERWEEK + days * 86400 + seconds + GPSORIGIN; /* convert to NTP time */
  lfp->l_uf = 0;
}

/*
 * gpstolfp.c,v
 * Revision 4.3  1999/02/28 11:42:44  kardel
 * (GPSWRAP): update GPS rollover to 990 weeks
 *
 * Revision 4.2  1998/07/11 10:05:25  kardel
 * Release 4.0.73d reconcilation
 *
 * Revision 4.1  1998/06/28 16:47:15  kardel
 * added gpstolfp() function
 */
