/*
 * /src/NTP/ntp-4/libntp/binio.c,v 4.2 1999/02/21 12:17:34 kardel RELEASE_19990228_A
 *
 * $Created: Sun Jul 20 12:55:33 1997 $
 *
 * Copyright (C) 1997, 1998 by Frank Kardel
 */

#include "binio.h"

long
get_lsb_short(
	unsigned char **bufpp
	)
{
  long retval;

  retval  = *((*bufpp)++);
  retval |= *((*bufpp)++) << 8;

  return (retval & 0x8000) ? (~0xFFFF | retval) : retval;
}

void
put_lsb_short(
	unsigned char **bufpp,
	long val
	)
{
  *((*bufpp)++) =  val       & 0xFF;
  *((*bufpp)++) = (val >> 8) & 0xFF;
}

long
get_lsb_long(
	unsigned char **bufpp
	)
{
  long retval;

  retval  = *((*bufpp)++);
  retval |= *((*bufpp)++) << 8;
  retval |= *((*bufpp)++) << 16;
  retval |= *((*bufpp)++) << 24;

  return retval;
}

void
put_lsb_long(
	unsigned char **bufpp,
	long val
	)
{
  *((*bufpp)++) =  val        & 0xFF;
  *((*bufpp)++) = (val >> 8)  & 0xFF;
  *((*bufpp)++) = (val >> 16) & 0xFF;
  *((*bufpp)++) = (val >> 24) & 0xFF;
}

long
get_msb_short(
	unsigned char **bufpp
	)
{
  long retval;

  retval  = *((*bufpp)++) << 8;
  retval |= *((*bufpp)++);

  return (retval & 0x8000) ? (~0xFFFF | retval) : retval;
}

void
put_msb_short(
	unsigned char **bufpp,
	long val
	)
{
  *((*bufpp)++) = (val >> 8) & 0xFF;
  *((*bufpp)++) =  val       & 0xFF;
}

long
get_msb_long(
	unsigned char **bufpp
	)
{
  long retval;

  retval  = *((*bufpp)++) << 24;
  retval |= *((*bufpp)++) << 16;
  retval |= *((*bufpp)++) << 8;
  retval |= *((*bufpp)++);

  return retval;
}

void
put_msb_long(
	unsigned char **bufpp,
	long val
	)
{
  *((*bufpp)++) = (val >> 24) & 0xFF;
  *((*bufpp)++) = (val >> 16) & 0xFF;
  *((*bufpp)++) = (val >> 8 ) & 0xFF;
  *((*bufpp)++) =  val        & 0xFF;
}

/*
 * binio.c,v
 * Revision 4.2  1999/02/21 12:17:34  kardel
 * 4.91f reconcilation
 *
 * Revision 4.1  1998/06/28 16:47:50  kardel
 * added {get,put}_msb_{short,long} functions
 *
 * Revision 4.0  1998/04/10 19:46:16  kardel
 * Start 4.0 release version numbering
 *
 * Revision 1.1  1998/04/10 19:27:46  kardel
 * initial NTP VERSION 4 integration of PARSE with GPS166 binary support
 *
 * Revision 1.1  1997/10/06 21:05:46  kardel
 * new parse structure
 *
 */
