#if defined(REFCLOCK) && (defined(PARSE) || defined(PARSEPPS)) && defined(CLOCK_TRIMSV6)
/*
 * /src/NTP/REPOSITORY/v3/parse/clk_trimble.c,v 3.9 1994/02/02 17:45:27 kardel Exp
 *
 * Trimble SV6 clock support
 */

#include "sys/types.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_calendar.h"

#include "parse.h"

/*	0000000000111111111122222222223333333	/ char
 *	0123456789012345678901234567890123456	\ posn
 *	>RTMhhmmssdddDDMMYYYYoodnnvrrrrr;*xx<	Actual
 *	----33445566600112222BB7__-_____--99-	Parse
 *	>RTM                      1     ;*  <",	Check
 */

#define	hexval(x) (('0' <= (x) && (x) <= '9') ? (x) - '0' : \
	('a' <= (x) && (x) <= 'f') ? (x) - 'a' + 10 : \
	('A' <= (x) && (x) <= 'F') ? (x) - 'A' + 10 : \
	-1)
#define	O_USEC		O_WDAY
#define	O_GPSFIX	O_FLAGS
#define	O_CHKSUM	O_UTCHOFFSET
static struct format trimsv6_fmt =
{ { { 13, 2 }, {15, 2}, { 17, 4}, /* Day, Month, Year */
    {  4, 2 }, { 6, 2}, {  8, 2}, /* Hour, Minute, Second */
    { 10, 3 }, {23, 1}, {  0, 0}, /* uSec, FIXes (WeekDAY, FLAGS, ZONE) */
    { 34, 2 }, { 0, 0}, { 21, 2}, /* cksum, -, utcS (UTC[HMS]OFFSET) */
  },
  ">RTM                      1     ;*  <",
  0
};

static unsigned LONG cvt_trimsv6();

clockformat_t clock_trimsv6 =
{
  (unsigned LONG (*)())0,	/* XXX?: no input handling */
  cvt_trimsv6,			/* Trimble conversion */
  syn_simple,			/* easy time stamps for RS232 (fallback) */
  pps_simple,			/* easy PPS monitoring */
  (unsigned LONG (*)())0,	/* no time code synthesizer monitoring */
  (void *)&trimsv6_fmt,		/* conversion configuration */
  "Trimble SV6",
  37,				/* string buffer */
  F_START|F_END|SYNC_START|SYNC_ONE, /* paket START/END delimiter, START synchronisation, PPS ONE sampling */
  0,				/* XXX?: no private data (complete messages) */
  { 0, 0},
  '>',
  '<',
  '\0'
};

static unsigned LONG
cvt_trimsv6(buffer, size, format, clock)
  register char          *buffer;
  register int            size;
  register struct format *format;
  register clocktime_t   *clock;
{
  LONG gpsfix;
  u_char calc_csum = 0;
  long   recv_csum;
  int	 i;

  if (!Strok(buffer, format->fixed_string)) return CVT_NONE;
#define	OFFS(x) format->field_offsets[(x)].offset
#define	STOI(x, y) \
	Stoi(&buffer[OFFS(x)], y, \
	       format->field_offsets[(x)].length)
  if (	STOI(O_DAY,	&clock->day)	||
	STOI(O_MONTH,	&clock->month)	||
	STOI(O_YEAR,	&clock->year)	||
	STOI(O_HOUR,	&clock->hour)	||
	STOI(O_MIN,	&clock->minute)	||
	STOI(O_SEC,	&clock->second)	||
	STOI(O_USEC,	&clock->usecond)||
	STOI(O_GPSFIX,	&gpsfix)
     ) return CVT_FAIL|CVT_BADFMT;

  clock->usecond *= 1000;
  /* Check that the checksum is right */
  for (i=OFFS(O_CHKSUM)-1; i >= 0; i--) calc_csum ^= buffer[i];
  recv_csum =	(hexval(buffer[OFFS(O_CHKSUM)]) << 4) |
	 hexval(buffer[OFFS(O_CHKSUM)+1]);
  if (recv_csum < 0) return CVT_FAIL|CVT_BADTIME;
  if (((u_char) recv_csum) != calc_csum) return CVT_FAIL|CVT_BADTIME;

  clock->utcoffset = 0;

  /* What should flags be set to ? */
  clock->flags = PARSEB_UTC;

  /* if the current GPS fix is 9 (unknown), reject */
  if (0 > gpsfix || gpsfix > 9) clock->flags |= PARSEB_POWERUP;

  return CVT_OK;
}
#endif /* defined(PARSE) && defined(CLOCK_TRIMSV6) */

/*
 * History:
 *
 * clk_trimble.c,v
 * Revision 3.9  1994/02/02  17:45:27  kardel
 * rcs ids fixed
 *
 * Revision 3.7  1994/01/25  19:05:17  kardel
 * 94/01/23 reconcilation
 *
 * Revision 3.6  1993/10/30  09:44:45  kardel
 * conditional compilation flag cleanup
 *
 * Revision 3.5  1993/10/09  15:01:35  kardel
 * file structure unified
 *
 * revision 3.4
 * date: 1993/10/08 14:44:51;  author: kardel;
 * trimble - initial working version
 *
 * revision 3.3
 * date: 1993/10/03 19:10:50;  author: kardel;
 * restructured I/O handling
 *
 * revision 3.2
 * date: 1993/09/27 21:07:17;  author: kardel;
 * Trimble alpha integration
 *
 * revision 3.1
 * date: 1993/09/26 23:40:29;  author: kardel;
 * new parse driver logic
 *
 */
