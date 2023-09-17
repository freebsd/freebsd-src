/*
 * ntp_calgps.h - calendar for GPS/GNSS based clocks
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 *
 * This module implements stuff often used with GPS/GNSS receivers
 */
#ifndef NTP_CALGPS_H
#define NTP_CALGPS_H

#include <time.h>

#include "ntp_types.h"
#include "ntp_fp.h"
#include "ntp_calendar.h"

/* GPS week calendar (extended weeks)
 * We use weeks based on 1899-31-12, which was the last Sunday before
 * the begin of the NTP epoch. (Which is equivalent to saying 1900-01-01
 * was a Monday...)
 *
 * We simply pre-calculate the offsets and cycle shifts for the real GPS
 * calendar, which starts at 1980-01-06, to simplyfy some expressions.
 *
 * This has a fringe benefit that should not be overlooked: Since week zero
 * is around 1900, and we should never have to deal with dates before
 * 1970 or 1980, a week number of zero can be easily used to indicate
 * an invalid week time stamp.
 */
#define GPSNTP_WSHIFT	4175	/* weeks 1899-31-12 --> 1980-01-06 */
#define GPSNTP_WCYCLE	  79	/* above, modulo 1024 */
#define GPSNTP_DSHIFT	   1	/* day number of 1900-01-01 in week */

struct gpsdatum {
	uint32_t weeks;		/* weeks since GPS epoch	*/
	int32_t  wsecs;		/* seconds since week start	*/
	uint32_t frac;		/* fractional seconds		*/
};
typedef struct gpsdatum TGpsDatum;
typedef struct gpsdatum const TcGpsDatum;

/* NTP date/time in split representation */
struct ntpdatum {
	uint32_t days;		/* since NTP epoch		*/
	int32_t  secs;		/* since midnight, denorm is ok */
	uint32_t frac;		/* fractional seconds		*/
};
typedef struct ntpdatum TNtpDatum;
typedef struct ntpdatum const TcNtpDatum;

/*
 * GPS week/sec calendar functions
 *
 * see the implementation for details, especially the
 * 'gpscal_from_weektime{1,2}()'
 */

extern TGpsDatum
gpscal_fix_gps_era(TcGpsDatum *);

extern void
gpscal_add_offset(TGpsDatum *datum, l_fp offset);

extern TGpsDatum
gpscal_from_calendar_ex(TcCivilDate*, l_fp fofs, int/*BOOL*/ warp);

static inline TGpsDatum
gpscal_from_calendar(TcCivilDate *pCiv, l_fp fofs) {
    return gpscal_from_calendar_ex(pCiv, fofs, TRUE);
}

extern TGpsDatum 	/* see source for semantic of the 'fofs' value! */
gpscal_from_gpsweek(uint16_t w, int32_t s, l_fp fofs);

extern TGpsDatum
gpscal_from_weektime1(int32_t wsecs, l_fp fofs, l_fp pivot);

extern TGpsDatum
gpscal_from_weektime2(int32_t wsecs, l_fp fofs,	TcGpsDatum *pivot);

extern void
gpscal_to_calendar(TCivilDate*, TcGpsDatum*);

extern TGpsDatum
gpscal_from_gpsntp(TcNtpDatum*);

extern l_fp
ntpfp_from_gpsdatum(TcGpsDatum *);

/*
 * NTP day/sec calendar functions
 *
 * see the implementation for details, especially the
 * 'gpscal_from_daytime{1,2}()'
 */
extern TNtpDatum
gpsntp_fix_gps_era(TcNtpDatum *);

extern void
gpsntp_add_offset(TNtpDatum *datum, l_fp offset);

extern TNtpDatum
gpsntp_from_calendar_ex(TcCivilDate*, l_fp fofs, int/*BOOL*/ warp);

static inline TNtpDatum
gpsntp_from_calendar(TcCivilDate * pCiv, l_fp fofs) {
	return gpsntp_from_calendar_ex(pCiv, fofs, TRUE);
}

extern TNtpDatum
gpsntp_from_daytime1_ex(TcCivilDate *dt, l_fp fofs, l_fp pivot, int/*BOOL*/ warp);

static inline TNtpDatum
gpsntp_from_daytime1(TcCivilDate *dt, l_fp fofs, l_fp pivot) {
	return gpsntp_from_daytime1_ex(dt, fofs, pivot, TRUE);
}

extern TNtpDatum
gpsntp_from_daytime2_ex(TcCivilDate *dt, l_fp fofs, TcNtpDatum *pivot, int/*BOOL*/ warp);

static inline TNtpDatum
gpsntp_from_daytime2(TcCivilDate *dt, l_fp fofs, TcNtpDatum *pivot) {
	return gpsntp_from_daytime2_ex(dt, fofs, pivot, TRUE);
}

extern TNtpDatum
gpsntp_from_gpscal_ex(TcGpsDatum*, int/*BOOL*/ warp);

static inline TNtpDatum
gpsntp_from_gpscal(TcGpsDatum *wd) {
	return gpsntp_from_gpscal_ex(wd, FALSE);
}

extern void
gpsntp_to_calendar(TCivilDate*, TcNtpDatum*);

extern l_fp
ntpfp_from_ntpdatum(TcNtpDatum*);

/*
 * Some helpers
 */

/* apply fudge to time stamp: *SUBTRACT* the given offset from an l_fp*/
extern l_fp
ntpfp_with_fudge(l_fp lfp, double ofs);

#endif /*!defined(NTP_CALGPS_H)*/
