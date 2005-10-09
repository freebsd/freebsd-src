/*
 * /src/NTP/ntp-4/include/parse_conf.h,v 4.2 1998/06/14 21:09:28 kardel RELEASE_19990228_A
 *
 * parse_conf.h,v 4.2 1998/06/14 21:09:28 kardel RELEASE_19990228_A
 *
 * Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998 by Frank Kardel
 * Friedrich-Alexander Universität Erlangen-Nürnberg, Germany
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __PARSE_CONF_H__
#define __PARSE_CONF_H__
#if	!(defined(lint) || defined(__GNUC__))
  static char prshrcsid[] = "parse_conf.h,v 4.2 1998/06/14 21:09:28 kardel RELEASE_19990228_A";
#endif

/*
 * field location structure
 */
#define O_DAY	0
#define O_MONTH 1
#define O_YEAR	2
#define O_HOUR	3
#define O_MIN	4
#define	O_SEC	5
#define O_WDAY	6
#define O_FLAGS 7
#define O_ZONE  8
#define O_UTCHOFFSET 9
#define O_UTCMOFFSET 10
#define O_UTCSOFFSET 11
#define O_COUNT (O_UTCSOFFSET+1)

#define MBG_EXTENDED	0x00000001

/*
 * see below for field offsets
 */

struct format
{
  struct foff
    {
      unsigned short offset;		/* offset into buffer */
      unsigned short length;		/* length of field */
    }         field_offsets[O_COUNT];
  const unsigned char *fixed_string;		/* string with must be chars (blanks = wildcards) */
  u_long      flags;
};
#endif
