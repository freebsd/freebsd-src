#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS))
/*
 * /src/NTP/REPOSITORY/v3/parse/parse_conf.c,v 3.15 1994/02/02 17:45:32 kardel Exp
 *  
 * parse_conf.c,v 3.15 1994/02/02 17:45:32 kardel Exp
 *
 * Parser configuration module for reference clocks
 *
 * STREAM define switches between two personalities of the module
 * if STREAM is defined this module can be used with dcf77sync.c as
 * a STREAMS kernel module. In this case the time stamps will be
 * a struct timeval.
 * when STREAM is not defined NTP time stamps will be used.
 *
 * Copyright (c) 1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "sys/types.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

#ifdef CLOCK_SCHMID
extern clockformat_t clock_schmid;
#endif

#ifdef CLOCK_DCF7000
extern clockformat_t clock_dcf7000;
#endif

#ifdef CLOCK_MEINBERG
extern clockformat_t clock_meinberg[];
#endif

#ifdef CLOCK_RAWDCF
extern clockformat_t clock_rawdcf;
#endif

#ifdef CLOCK_TRIMSV6
extern clockformat_t clock_trimsv6;
#endif

/*
 * format definitions
 */
clockformat_t *clockformats[] =
{
#ifdef CLOCK_MEINBERG
  &clock_meinberg[0],
  &clock_meinberg[1],
  &clock_meinberg[2],
#endif
#ifdef CLOCK_DCF7000
  &clock_dcf7000,
#endif
#ifdef CLOCK_SCHMID
  &clock_schmid,
#endif
#ifdef CLOCK_RAWDCF
  &clock_rawdcf,
#endif
#ifdef CLOCK_TRIMSV6
  &clock_trimsv6,
#endif
0};

unsigned short nformats = sizeof(clockformats) / sizeof(clockformats[0]) - 1;
#endif /* REFCLOCK PARSE */

/*
 * History:
 *
 * parse_conf.c,v
 * Revision 3.15  1994/02/02  17:45:32  kardel
 * rcs ids fixed
 *
 * Revision 3.13  1994/01/25  19:05:23  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.12  1994/01/23  17:22:02  kardel
 * 1994 reconcilation
 *
 * Revision 3.11  1993/11/01  20:00:24  kardel
 * parse Solaris support (initial version)
 *
 * Revision 3.10  1993/10/09  15:01:37  kardel
 * file structure unified
 *
 * Revision 3.9  1993/09/26  23:40:19  kardel
 * new parse driver logic
 *
 * Revision 3.8  1993/09/02  23:20:57  kardel
 * dragon extiction
 *
 * Revision 3.7  1993/09/01  21:44:52  kardel
 * conditional cleanup
 *
 * Revision 3.6  1993/09/01  11:25:09  kardel
 * patch accident 8-(
 *
 * Revision 3.5  1993/08/31  22:31:14  kardel
 * SINIX-M SysVR4 integration
 *
 * Revision 3.4  1993/08/27  00:29:42  kardel
 * compilation cleanup
 *
 * Revision 3.3  1993/07/14  09:04:45  kardel
 * only when REFCLOCK && PARSE is defined
 *
 * Revision 3.2  1993/07/09  11:37:13  kardel
 * Initial restructured version + GPS support
 *
 * Revision 3.1  1993/07/06  10:00:11  kardel
 * DCF77 driver goes generic...
 *
 */
