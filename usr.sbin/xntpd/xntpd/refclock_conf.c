/*
 * refclock_conf.c - reference clock configuration
 */
#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef REFCLOCK

static struct refclock refclock_none = {
	noentry, noentry, noentry, noentry, noentry, noentry, NOFLAGS
};

#ifdef	LOCAL_CLOCK
extern	struct refclock		refclock_local;
#else
#define	refclock_local	refclock_none
#endif

#if defined(PST) || defined(PSTCLK) || defined(PSTPPS)
extern	struct refclock		refclock_pst;
#else
#define	refclock_pst	refclock_none
#endif

#if defined(CHU) || defined(CHUCLK) || defined(CHUPPS)
extern	struct refclock		refclock_chu;
#else
#define	refclock_chu	refclock_none
#endif

#if defined(GOES) || defined(GOESCLK) || defined(GOESPPS)
extern	struct refclock		refclock_goes;
#else
#define	refclock_goes	refclock_none
#endif

#if defined(WWVB) || defined(WWVBCLK) || defined(WWVBPPS)
extern	struct refclock		refclock_wwvb;
#else
#define	refclock_wwvb	refclock_none
#endif

#if defined(PARSE) || defined(PARSEPPS)
extern	struct refclock		refclock_parse;
#else
#define	refclock_parse	refclock_none
#endif

#if defined(MX4200) || defined(MX4200CLK) || defined(MX4200PPS)
extern	struct refclock		refclock_mx4200;
#else
#define	refclock_mx4200	refclock_none
#endif

#if defined(AS2201) || defined(AS2201CLK) || defined(AS2201PPS)
extern	struct refclock		refclock_as2201;
#else
#define	refclock_as2201	refclock_none
#endif

#if defined(OMEGA) || defined(OMEGACLK) || defined(OMEGAPPS)
extern	struct refclock		refclock_omega;
#else
#define	refclock_omega	refclock_none
#endif

#ifdef TPRO
extern	struct refclock		refclock_tpro;
#else
#define	refclock_tpro	refclock_none
#endif

#if defined(LEITCH) || defined(LEITCHCLK) || defined(LEITCHPPS)
extern	struct refclock		refclock_leitch;
#else
#define	refclock_leitch	refclock_none
#endif

#ifdef IRIG
extern	struct refclock		refclock_irig;
#else
#define refclock_irig	refclock_none
#endif

#if defined(MSFEESPPS)
extern	struct refclock		refclock_msfees;
#else
#define refclock_msfees	refclock_none
#endif

#if defined(GPSTM) || defined(GPSTMCLK) || defined(GPSTMPPS)
extern	struct refclock		refclock_gpstm;
#else
#define	refclock_gpstm	refclock_none
#endif

/*
 * Order is clock_start(), clock_shutdown(), clock_poll(),
 * clock_control(), clock_init(), clock_buginfo, clock_flags;
 *
 * Types are defined in ntp.h.  The index must match this.
 */
struct refclock *refclock_conf[] = {
	&refclock_none,		/* 0 REFCLK_NONE */
	&refclock_local,	/* 1 REFCLK_LOCAL */
	&refclock_none,		/* 2 REFCLK_WWV_HEATH */
	&refclock_pst,		/* 3 REFCLK_WWV_PST */
	&refclock_wwvb, 	/* 4 REFCLK_WWVB_SPECTRACOM */
	&refclock_goes,		/* 5 REFCLK_GOES_TRUETIME */
	&refclock_irig,		/* 6 REFCLK_IRIG_AUDIO */
	&refclock_chu,		/* 7 REFCLK_CHU */
	&refclock_parse,	/* 8 REFCLK_PARSE */
	&refclock_mx4200,	/* 9 REFCLK_GPS_MX4200 */
	&refclock_as2201,	/* 10 REFCLK_GPS_AS2201 */
	&refclock_omega,	/* 11 REFCLK_OMEGA_TRUETIME */
        &refclock_tpro,		/* 12 REFCLK_IRIG_TPRO */
	&refclock_leitch,	/* 13 REFCLK_ATOM_LEITCH */
	&refclock_msfees,	/* 14 REFCLK_MSF_EES */
	&refclock_gpstm,	/* 15 REFCLK_GPSTM_TRUETIME */
};

u_char num_refclock_conf = sizeof(refclock_conf)/sizeof(struct refclock *);

#endif
