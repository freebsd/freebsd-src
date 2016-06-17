/*
 *  linux/kernel/time.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  This file contains the interface functions for the various
 *  time related system calls: time, stime, gettimeofday, settimeofday,
 *			       adjtime
 */
/*
 * Modification history kernel/time.c
 * 
 * 1993-09-02    Philip Gladstone
 *      Created file with time related functions from sched.c and adjtimex() 
 * 1993-10-08    Torsten Duwe
 *      adjtime interface update and CMOS clock write code
 * 1995-08-13    Torsten Duwe
 *      kernel PLL updated to 1994-12-13 specs (rfc-1589)
 * 1999-01-16    Ulrich Windl
 *	Introduced error checking for many cases in adjtimex().
 *	Updated NTP code according to technical memorandum Jan '96
 *	"A Kernel Model for Precision Timekeeping" by Dave Mills
 *	Allow time_constant larger than MAXTC(6) for NTP v4 (MAXTC == 10)
 *	(Even though the technical memorandum forbids it)
 */

#include <linux/mm.h>
#include <linux/timex.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

/* 
 * The timezone where the local system is located.  Used as a default by some
 * programs who obtain this value by using gettimeofday.
 */
struct timezone sys_tz;

/* The xtime_lock is not only serializing the xtime read/writes but it's also
   serializing all accesses to the global NTP variables now. */
extern rwlock_t xtime_lock;

#if !defined(__alpha__) && !defined(__ia64__)

/*
 * sys_time() can be implemented in user-level using
 * sys_gettimeofday().  Is this for backwards compatibility?  If so,
 * why not move it into the appropriate arch directory (for those
 * architectures that need it).
 *
 * XXX This function is NOT 64-bit clean!
 */
asmlinkage long sys_time(int * tloc)
{
	struct timeval now; 
	int i; 

	do_gettimeofday(&now);
	i = now.tv_sec;
	if (tloc) {
		if (put_user(i,tloc))
			i = -EFAULT;
	}
	return i;
}

/*
 * sys_stime() can be implemented in user-level using
 * sys_settimeofday().  Is this for backwards compatibility?  If so,
 * why not move it into the appropriate arch directory (for those
 * architectures that need it).
 */
 
asmlinkage long sys_stime(int * tptr)
{
	int value;

	if (!capable(CAP_SYS_TIME))
		return -EPERM;
	if (get_user(value, tptr))
		return -EFAULT;
	write_lock_irq(&xtime_lock);
	vxtime_lock();
	xtime.tv_sec = value;
	xtime.tv_usec = 0;
	vxtime_unlock();
	time_adjust = 0;	/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
	return 0;
}

#endif

asmlinkage long sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (tv) {
		struct timeval ktv;
		do_gettimeofday(&ktv);
		if (copy_to_user(tv, &ktv, sizeof(ktv)))
			return -EFAULT;
	}
	if (tz) {
		if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
			return -EFAULT;
	}
	return 0;
}

/*
 * Adjust the time obtained from the CMOS to be UTC time instead of
 * local time.
 * 
 * This is ugly, but preferable to the alternatives.  Otherwise we
 * would either need to write a program to do it in /etc/rc (and risk
 * confusion if the program gets run more than once; it would also be 
 * hard to make the program warp the clock precisely n hours)  or
 * compile in the timezone information into the kernel.  Bad, bad....
 *
 *              				- TYT, 1992-01-01
 *
 * The best thing to do is to keep the CMOS clock in universal time (UTC)
 * as real UNIX machines always do it. This avoids all headaches about
 * daylight saving times and warping kernel clocks.
 */
inline static void warp_clock(void)
{
	write_lock_irq(&xtime_lock);
	vxtime_lock();
	xtime.tv_sec += sys_tz.tz_minuteswest * 60;
	vxtime_unlock();
	write_unlock_irq(&xtime_lock);
}

/*
 * In case for some reason the CMOS clock has not already been running
 * in UTC, but in some local time: The first time we set the timezone,
 * we will warp the clock so that it is ticking UTC time instead of
 * local time. Presumably, if someone is setting the timezone then we
 * are running in an environment where the programs understand about
 * timezones. This should be done at boot time in the /etc/rc script,
 * as soon as possible, so that the clock can be set right. Otherwise,
 * various programs will get confused when the clock gets warped.
 */

int do_sys_settimeofday(struct timeval *tv, struct timezone *tz)
{
	static int firsttime = 1;

	if (!capable(CAP_SYS_TIME))
		return -EPERM;
		
	if (tz) {
		/* SMP safe, global irq locking makes it work. */
		sys_tz = *tz;
		if (firsttime) {
			firsttime = 0;
			if (!tv)
				warp_clock();
		}
	}
	if (tv)
	{
		/* SMP safe, again the code in arch/foo/time.c should
		 * globally block out interrupts when it runs.
		 */
		do_settimeofday(tv);
	}
	return 0;
}

asmlinkage long sys_settimeofday(struct timeval *tv, struct timezone *tz)
{
	struct timeval	new_tv;
	struct timezone new_tz;

	if (tv) {
		if (copy_from_user(&new_tv, tv, sizeof(*tv)))
			return -EFAULT;
	}
	if (tz) {
		if (copy_from_user(&new_tz, tz, sizeof(*tz)))
			return -EFAULT;
	}

	return do_sys_settimeofday(tv ? &new_tv : NULL, tz ? &new_tz : NULL);
}

long pps_offset;		/* pps time offset (us) */
long pps_jitter = MAXTIME;	/* time dispersion (jitter) (us) */

long pps_freq;			/* frequency offset (scaled ppm) */
long pps_stabil = MAXFREQ;	/* frequency dispersion (scaled ppm) */

long pps_valid = PPS_VALID;	/* pps signal watchdog counter */

int pps_shift = PPS_SHIFT;	/* interval duration (s) (shift) */

long pps_jitcnt;		/* jitter limit exceeded */
long pps_calcnt;		/* calibration intervals */
long pps_errcnt;		/* calibration errors */
long pps_stbcnt;		/* stability limit exceeded */

/* hook for a loadable hardpps kernel module */
void (*hardpps_ptr)(struct timeval *);

/* adjtimex mainly allows reading (and writing, if superuser) of
 * kernel time-keeping variables. used by xntpd.
 */
int do_adjtimex(struct timex *txc)
{
        long ltemp, mtemp, save_adjust;
	int result;

	/* In order to modify anything, you gotta be super-user! */
	if (txc->modes && !capable(CAP_SYS_TIME))
		return -EPERM;
		
	/* Now we validate the data before disabling interrupts */

	if ((txc->modes & ADJ_OFFSET_SINGLESHOT) == ADJ_OFFSET_SINGLESHOT)
	  /* singleshot must not be used with any other mode bits */
		if (txc->modes != ADJ_OFFSET_SINGLESHOT)
			return -EINVAL;

	if (txc->modes != ADJ_OFFSET_SINGLESHOT && (txc->modes & ADJ_OFFSET))
	  /* adjustment Offset limited to +- .512 seconds */
		if (txc->offset <= - MAXPHASE || txc->offset >= MAXPHASE )
			return -EINVAL;	

	/* if the quartz is off by more than 10% something is VERY wrong ! */
	if (txc->modes & ADJ_TICK)
		if (txc->tick < 900000/HZ || txc->tick > 1100000/HZ)
			return -EINVAL;

	write_lock_irq(&xtime_lock);
	result = time_state;	/* mostly `TIME_OK' */

	/* Save for later - semantics of adjtime is to return old value */
	save_adjust = time_adjust;

#if 0	/* STA_CLOCKERR is never set yet */
	time_status &= ~STA_CLOCKERR;		/* reset STA_CLOCKERR */
#endif
	/* If there are input parameters, then process them */
	if (txc->modes)
	{
	    if (txc->modes & ADJ_STATUS)	/* only set allowed bits */
		time_status =  (txc->status & ~STA_RONLY) |
			      (time_status & STA_RONLY);

	    if (txc->modes & ADJ_FREQUENCY) {	/* p. 22 */
		if (txc->freq > MAXFREQ || txc->freq < -MAXFREQ) {
		    result = -EINVAL;
		    goto leave;
		}
		time_freq = txc->freq - pps_freq;
	    }

	    if (txc->modes & ADJ_MAXERROR) {
		if (txc->maxerror < 0 || txc->maxerror >= NTP_PHASE_LIMIT) {
		    result = -EINVAL;
		    goto leave;
		}
		time_maxerror = txc->maxerror;
	    }

	    if (txc->modes & ADJ_ESTERROR) {
		if (txc->esterror < 0 || txc->esterror >= NTP_PHASE_LIMIT) {
		    result = -EINVAL;
		    goto leave;
		}
		time_esterror = txc->esterror;
	    }

	    if (txc->modes & ADJ_TIMECONST) {	/* p. 24 */
		if (txc->constant < 0) {	/* NTP v4 uses values > 6 */
		    result = -EINVAL;
		    goto leave;
		}
		time_constant = txc->constant;
	    }

	    if (txc->modes & ADJ_OFFSET) {	/* values checked earlier */
		if (txc->modes == ADJ_OFFSET_SINGLESHOT) {
		    /* adjtime() is independent from ntp_adjtime() */
		    time_adjust = txc->offset;
		}
		else if ( time_status & (STA_PLL | STA_PPSTIME) ) {
		    ltemp = (time_status & (STA_PPSTIME | STA_PPSSIGNAL)) ==
		            (STA_PPSTIME | STA_PPSSIGNAL) ?
		            pps_offset : txc->offset;

		    /*
		     * Scale the phase adjustment and
		     * clamp to the operating range.
		     */
		    if (ltemp > MAXPHASE)
		        time_offset = MAXPHASE << SHIFT_UPDATE;
		    else if (ltemp < -MAXPHASE)
			time_offset = -(MAXPHASE << SHIFT_UPDATE);
		    else
		        time_offset = ltemp << SHIFT_UPDATE;

		    /*
		     * Select whether the frequency is to be controlled
		     * and in which mode (PLL or FLL). Clamp to the operating
		     * range. Ugly multiply/divide should be replaced someday.
		     */

		    if (time_status & STA_FREQHOLD || time_reftime == 0)
		        time_reftime = xtime.tv_sec;
		    mtemp = xtime.tv_sec - time_reftime;
		    time_reftime = xtime.tv_sec;
		    if (time_status & STA_FLL) {
		        if (mtemp >= MINSEC) {
			    ltemp = (time_offset / mtemp) << (SHIFT_USEC -
							      SHIFT_UPDATE);
			    if (ltemp < 0)
			        time_freq -= -ltemp >> SHIFT_KH;
			    else
			        time_freq += ltemp >> SHIFT_KH;
			} else /* calibration interval too short (p. 12) */
				result = TIME_ERROR;
		    } else {	/* PLL mode */
		        if (mtemp < MAXSEC) {
			    ltemp *= mtemp;
			    if (ltemp < 0)
			        time_freq -= -ltemp >> (time_constant +
							time_constant +
							SHIFT_KF - SHIFT_USEC);
			    else
			        time_freq += ltemp >> (time_constant +
						       time_constant +
						       SHIFT_KF - SHIFT_USEC);
			} else /* calibration interval too long (p. 12) */
				result = TIME_ERROR;
		    }
		    if (time_freq > time_tolerance)
		        time_freq = time_tolerance;
		    else if (time_freq < -time_tolerance)
		        time_freq = -time_tolerance;
		} /* STA_PLL || STA_PPSTIME */
	    } /* txc->modes & ADJ_OFFSET */
	    if (txc->modes & ADJ_TICK) {
		/* if the quartz is off by more than 10% something is
		   VERY wrong ! */
		if (txc->tick < 900000/HZ || txc->tick > 1100000/HZ) {
		    result = -EINVAL;
		    goto leave;
		}
		tick = txc->tick;
	    }
	} /* txc->modes */
leave:	if ((time_status & (STA_UNSYNC|STA_CLOCKERR)) != 0
	    || ((time_status & (STA_PPSFREQ|STA_PPSTIME)) != 0
		&& (time_status & STA_PPSSIGNAL) == 0)
	    /* p. 24, (b) */
	    || ((time_status & (STA_PPSTIME|STA_PPSJITTER))
		== (STA_PPSTIME|STA_PPSJITTER))
	    /* p. 24, (c) */
	    || ((time_status & STA_PPSFREQ) != 0
		&& (time_status & (STA_PPSWANDER|STA_PPSERROR)) != 0))
	    /* p. 24, (d) */
		result = TIME_ERROR;
	
	if ((txc->modes & ADJ_OFFSET_SINGLESHOT) == ADJ_OFFSET_SINGLESHOT)
	    txc->offset	   = save_adjust;
	else {
	    if (time_offset < 0)
		txc->offset = -(-time_offset >> SHIFT_UPDATE);
	    else
		txc->offset = time_offset >> SHIFT_UPDATE;
	}
	txc->freq	   = time_freq + pps_freq;
	txc->maxerror	   = time_maxerror;
	txc->esterror	   = time_esterror;
	txc->status	   = time_status;
	txc->constant	   = time_constant;
	txc->precision	   = time_precision;
	txc->tolerance	   = time_tolerance;
	txc->tick	   = tick;
	txc->ppsfreq	   = pps_freq;
	txc->jitter	   = pps_jitter >> PPS_AVG;
	txc->shift	   = pps_shift;
	txc->stabil	   = pps_stabil;
	txc->jitcnt	   = pps_jitcnt;
	txc->calcnt	   = pps_calcnt;
	txc->errcnt	   = pps_errcnt;
	txc->stbcnt	   = pps_stbcnt;
	write_unlock_irq(&xtime_lock);
	do_gettimeofday(&txc->time);
	return(result);
}

asmlinkage long sys_adjtimex(struct timex *txc_p)
{
	struct timex txc;		/* Local copy of parameter */
	int ret;

	/* Copy the user data space into the kernel copy
	 * structure. But bear in mind that the structures
	 * may change
	 */
	if(copy_from_user(&txc, txc_p, sizeof(struct timex)))
		return -EFAULT;
	ret = do_adjtimex(&txc);
	return copy_to_user(txc_p, &txc, sizeof(struct timex)) ? -EFAULT : ret;
}
