/*
 * ntp_util.c - stuff I didn't have any other place for
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_filegen.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_IEEEFP_H
# include <ieeefp.h>
#endif
#ifdef HAVE_MATH_H
# include <math.h>
#endif

#ifdef  DOSYNCTODR
#if !defined(VMS)
#include <sys/resource.h>
#endif /* VMS */
#endif

#if defined(VMS)
#include <descrip.h>
#endif /* VMS */

/*
 * This contains odds and ends.  Right now the only thing you'll find
 * in here is the hourly stats printer and some code to support rereading
 * the keys file, but I may eventually put other things in here such as
 * code to do something with the leap bits.
 */

/*
 * Name of the keys file
 */
static	char *key_file_name;

/*
 * The name of the drift_comp file and the temporary.
 */
static	char *stats_drift_file;
static	char *stats_temp_file;

/*
 * Statistics file stuff
 */
#ifndef NTP_VAR
#ifndef SYS_WINNT
#define NTP_VAR "/var/NTP/"		/* NOTE the trailing '/' */
#else
#define NTP_VAR "c:\\var\\ntp\\"		/* NOTE the trailing '\\' */
#endif /* SYS_WINNT */
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

static	char statsdir[MAXPATHLEN] = NTP_VAR;

static FILEGEN peerstats;
static FILEGEN loopstats;
static FILEGEN clockstats;
static FILEGEN rawstats;

/*
 * This controls whether stats are written to the fileset. Provided
 * so that ntpdc can turn off stats when the file system fills up. 
 */
int stats_control;

/*
 * init_util - initialize the utilities
 */
void
init_util(void)
{
	stats_drift_file = 0;
	stats_temp_file = 0;
	key_file_name = 0;

#define PEERNAME "peerstats"
#define LOOPNAME "loopstats"
#define CLOCKNAME "clockstats"
#define RAWNAME "rawstats"
	peerstats.fp       = NULL;
	peerstats.prefix   = &statsdir[0];
	peerstats.basename = (char*)emalloc(strlen(PEERNAME)+1);
	strcpy(peerstats.basename, PEERNAME);
	peerstats.id       = 0;
	peerstats.type     = FILEGEN_DAY;
	peerstats.flag     = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("peerstats", &peerstats);
	
	loopstats.fp       = NULL;
	loopstats.prefix   = &statsdir[0];
	loopstats.basename = (char*)emalloc(strlen(LOOPNAME)+1);
	strcpy(loopstats.basename, LOOPNAME);
	loopstats.id       = 0;
	loopstats.type     = FILEGEN_DAY;
	loopstats.flag     = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("loopstats", &loopstats);

	clockstats.fp      = NULL;
	clockstats.prefix  = &statsdir[0];
	clockstats.basename = (char*)emalloc(strlen(CLOCKNAME)+1);
	strcpy(clockstats.basename, CLOCKNAME);
	clockstats.id      = 0;
	clockstats.type    = FILEGEN_DAY;
	clockstats.flag    = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("clockstats", &clockstats);

	rawstats.fp      = NULL;
	rawstats.prefix  = &statsdir[0];
	rawstats.basename = (char*)emalloc(strlen(RAWNAME)+1);
	strcpy(rawstats.basename, RAWNAME);
	rawstats.id      = 0;
	rawstats.type    = FILEGEN_DAY;
	rawstats.flag    = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("rawstats", &rawstats);

#undef PEERNAME
#undef LOOPNAME
#undef CLOCKNAME
#undef RAWNAME

}


/*
 * hourly_stats - print some interesting stats
 */
void
hourly_stats(void)
{
	FILE *fp;

#ifdef DOSYNCTODR
	struct timeval tv;
#if !defined(VMS)
	int prio_set;
#endif
#ifdef HAVE_GETCLOCK
        struct timespec ts;
#endif
	int o_prio;

	/*
	 * Sometimes having a Sun can be a drag.
	 *
	 * The kernel variable dosynctodr controls whether the system's
	 * soft clock is kept in sync with the battery clock. If it
	 * is zero, then the soft clock is not synced, and the battery
	 * clock is simply left to rot. That means that when the system
	 * reboots, the battery clock (which has probably gone wacky)
	 * sets the soft clock. That means ntpd starts off with a very
	 * confused idea of what time it is. It then takes a large
	 * amount of time to figure out just how wacky the battery clock
	 * has made things drift, etc, etc. The solution is to make the
	 * battery clock sync up to system time. The way to do THAT is
	 * to simply set the time of day to the current time of day, but
	 * as quickly as possible. This may, or may not be a sensible
	 * thing to do.
	 *
	 * CAVEAT: settimeofday() steps the sun clock by about 800 us,
	 *         so setting DOSYNCTODR seems a bad idea in the
	 *         case of us resolution
	 */

#if !defined(VMS)
	/* (prr) getpriority returns -1 on error, but -1 is also a valid
	 * return value (!), so instead we have to zero errno before the call
	 * and check it for non-zero afterwards.
	 */

	errno = 0;
	prio_set = 0;
	o_prio = getpriority(PRIO_PROCESS,0); /* Save setting */

	/* (prr) if getpriority succeeded, call setpriority to raise
	 * scheduling priority as high as possible.  If that succeeds
	 * as well, set the prio_set flag so we remember to reset
	 * priority to its previous value below.  Note that on Solaris 2.6
	 * (and beyond?), both getpriority and setpriority will fail with
	 * ESRCH, because sched_setscheduler (called from main) put us in
	 * the real-time scheduling class which setpriority doesn't know about.
	 * Being in the real-time class is better than anything setpriority
	 * can do, anyhow, so this error is silently ignored.
	 */

	if ((errno == 0) && (setpriority(PRIO_PROCESS,0,-20) == 0))
	    prio_set = 1;	/* overdrive */
#endif /* VMS */
#ifdef HAVE_GETCLOCK
        (void) getclock(TIMEOFDAY, &ts);
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tv,(struct timezone *)NULL);
#endif /* not HAVE_GETCLOCK */
	if (ntp_set_tod(&tv,(struct timezone *)NULL) != 0)
	{
		msyslog(LOG_ERR, "can't sync battery time: %m");
	}
#if !defined(VMS)
	if (prio_set)
	    setpriority(PRIO_PROCESS, 0, o_prio); /* downshift */
#endif /* VMS */
#endif /* DOSYNCTODR */

	NLOG(NLOG_SYSSTATIST)
		msyslog(LOG_INFO,
		    "offset %.6f sec freq %.3f ppm error %.6f poll %d",
		    last_offset, drift_comp * 1e6, sys_jitter, sys_poll);

	
	if (stats_drift_file != 0) {
		if ((fp = fopen(stats_temp_file, "w")) == NULL) {
			msyslog(LOG_ERR, "can't open %s: %m",
			    stats_temp_file);
			return;
		}
		fprintf(fp, "%.3f\n", drift_comp * 1e6);
		(void)fclose(fp);
		/* atomic */
#ifdef SYS_WINNT
		(void) unlink(stats_drift_file); /* rename semantics differ under NT */
#endif /* SYS_WINNT */

#ifndef NO_RENAME
		(void) rename(stats_temp_file, stats_drift_file);
#else
        /* we have no rename NFS of ftp in use*/
		if ((fp = fopen(stats_drift_file, "w")) == NULL) {
			msyslog(LOG_ERR, "can't open %s: %m",
			    stats_drift_file);
			return;
		}

#endif

#if defined(VMS)
		/* PURGE */
		{
			$DESCRIPTOR(oldvers,";-1");
			struct dsc$descriptor driftdsc = {
				strlen(stats_drift_file),0,0,stats_drift_file };

			while(lib$delete_file(&oldvers,&driftdsc) & 1) ;
		}
#endif
	}
}


/*
 * stats_config - configure the stats operation
 */
void
stats_config(
	int item,
	char *invalue	/* only one type so far */
	)
{
	FILE *fp;
	char *value;
	double old_drift;
	int len;

	/* Expand environment strings under Windows NT, since the command
	 * interpreter doesn't do this, the program must.
	 */
#ifdef SYS_WINNT
	char newvalue[MAX_PATH], parameter[MAX_PATH];

	if (!ExpandEnvironmentStrings(invalue, newvalue, MAX_PATH))
	{
 		switch(item) {
		    case STATS_FREQ_FILE:
			strcpy(parameter,"STATS_FREQ_FILE");
			break;
		    case STATS_STATSDIR:
			strcpy(parameter,"STATS_STATSDIR");
			break;
		    case STATS_PID_FILE:
			strcpy(parameter,"STATS_PID_FILE");
			break;
		    default:
			strcpy(parameter,"UNKNOWN");
			break;
		}
		value = invalue;

		msyslog(LOG_ERR,
		    "ExpandEnvironmentStrings(%s) failed: %m\n", parameter);
	}
	else 
		value = newvalue;
#else    
	value = invalue;
#endif /* SYS_WINNT */

	
	
	switch(item) {
	    case STATS_FREQ_FILE:
		if (stats_drift_file != 0) {
			(void) free(stats_drift_file);
			(void) free(stats_temp_file);
			stats_drift_file = 0;
			stats_temp_file = 0;
		}

		if (value == 0 || (len = strlen(value)) == 0)
		    break;

		stats_drift_file = (char*)emalloc((u_int)(len + 1));
#if !defined(VMS)
		stats_temp_file = (char*)emalloc((u_int)(len + sizeof(".TEMP")));
#else
		stats_temp_file = (char*)emalloc((u_int)(len + sizeof("-TEMP")));
#endif /* VMS */
		memmove(stats_drift_file, value, (unsigned)(len+1));
		memmove(stats_temp_file, value, (unsigned)len);
#if !defined(VMS)
		memmove(stats_temp_file + len, ".TEMP", sizeof(".TEMP"));
#else
		memmove(stats_temp_file + len, "-TEMP", sizeof("-TEMP"));
#endif /* VMS */

		/*
		 * Open drift file and read frequency
		 */
		if ((fp = fopen(stats_drift_file, "r")) == NULL) {
			break;
		}
		if (fscanf(fp, "%lf", &old_drift) != 1) {
			msyslog(LOG_ERR, "Un-parsable frequency in %s", 
			    stats_drift_file);
			(void) fclose(fp);
			break;
		}
		(void) fclose(fp);
		if (
#ifdef HAVE_FINITE
			!finite(old_drift)
#else  /* not HAVE_FINITE */
# ifdef HAVE_ISFINITE
			!isfinite(old_drift)
# else  /* not HAVE_ISFINITE */
			0
# endif /* not HAVE_ISFINITE */
#endif /* not HAVE_FINITE */
		    || (fabs(old_drift) > (NTP_MAXFREQ * 1e6))) {
			msyslog(LOG_ERR, "invalid frequency (%f) in %s", 
			    old_drift, stats_drift_file);
			old_drift = 0.0;
		}
		msyslog(LOG_INFO, "frequency initialized %.3f from %s",
		    old_drift, stats_drift_file);
		loop_config(LOOP_DRIFTCOMP, old_drift / 1e6);
		break;
	
	    case STATS_STATSDIR:
		if (strlen(value) >= sizeof(statsdir)) {
			msyslog(LOG_ERR,
			    "value for statsdir too long (>%d, sigh)",
			    (int)sizeof(statsdir)-1);
		} else {
			l_fp now;

			get_systime(&now);
			strcpy(statsdir,value);
			if(peerstats.prefix == &statsdir[0] &&
			    peerstats.fp != NULL) {
				fclose(peerstats.fp);
				peerstats.fp = NULL;
				filegen_setup(&peerstats, now.l_ui);
			}
			if(loopstats.prefix == &statsdir[0] &&
			    loopstats.fp != NULL) {
				fclose(loopstats.fp);
				loopstats.fp = NULL;
				filegen_setup(&loopstats, now.l_ui);
			}
			if(clockstats.prefix == &statsdir[0] &&
			    clockstats.fp != NULL) {
				fclose(clockstats.fp);
				clockstats.fp = NULL;
				filegen_setup(&clockstats, now.l_ui);
			}
			if(rawstats.prefix == &statsdir[0] &&
			    rawstats.fp != NULL) {
				fclose(rawstats.fp);
				rawstats.fp = NULL;
				filegen_setup(&rawstats, now.l_ui);
			}
		}
		break;

	    case STATS_PID_FILE:
		if ((fp = fopen(value, "w")) == NULL) {
			msyslog(LOG_ERR, "Can't open %s: %m", value);
			break;
		}
		fprintf(fp, "%d", (int) getpid());
		fclose(fp);;
		break;

	    default:
		/* oh well */
		break;
	}
}

/*
 * record_peer_stats - write peer statistics to file
 *
 * file format:
 * day (mjd)
 * time (s past UTC midnight)
 * peer (ip address)
 * peer status word (hex)
 * peer offset (s)
 * peer delay (s)
 * peer error bound (s)
 * peer error (s)
*/
void
record_peer_stats(
	struct sockaddr_in *addr,
	int status,
	double offset,
	double delay,
	double dispersion,
	double skew
	)
{
	struct timeval tv;
#ifdef HAVE_GETCLOCK
        struct timespec ts;
#endif
	u_long day, sec, msec;

	if (!stats_control)
		return;
#ifdef HAVE_GETCLOCK
        (void) getclock(TIMEOFDAY, &ts);
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
#endif /* not HAVE_GETCLOCK */
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&peerstats, (u_long)(tv.tv_sec + JAN_1970));
	if (peerstats.fp != NULL) {
		fprintf(peerstats.fp,
		    "%lu %lu.%03lu %s %x %.9f %.9f %.9f %.9f\n",
		    day, sec, msec, ntoa(addr), status, offset,
		    delay, dispersion, skew);
		fflush(peerstats.fp);
	}
}
/*
 * record_loop_stats - write loop filter statistics to file
 *
 * file format:
 * day (mjd)
 * time (s past midnight)
 * offset (s)
 * frequency (approx ppm)
 * time constant (log base 2)
 */
void
record_loop_stats(
	double offset,
	double freq,
	double jitter,
	double stability,
	int poll
	)
{
	struct timeval tv;
#ifdef HAVE_GETCLOCK
        struct timespec ts;
#endif
	u_long day, sec, msec;

	if (!stats_control)
		return;
#ifdef HAVE_GETCLOCK
        (void) getclock(TIMEOFDAY, &ts);
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
#endif /* not HAVE_GETCLOCK */
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&loopstats, (u_long)(tv.tv_sec + JAN_1970));
	if (loopstats.fp != NULL) {
		fprintf(loopstats.fp, "%lu %lu.%03lu %.9f %.6f %.9f %.6f %d\n",
		    day, sec, msec, offset, freq * 1e6, jitter,
		    stability * 1e6, poll);
		fflush(loopstats.fp);
	}
}

/*
 * record_clock_stats - write clock statistics to file
 *
 * file format:
 * day (mjd)
 * time (s past midnight)
 * peer (ip address)
 * text message
 */
void
record_clock_stats(
	struct sockaddr_in *addr,
	const char *text
	)
{
	struct timeval tv;
#ifdef HAVE_GETCLOCK
        struct timespec ts;
#endif
	u_long day, sec, msec;

	if (!stats_control)
		return;
#ifdef HAVE_GETCLOCK
        (void) getclock(TIMEOFDAY, &ts);
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
#endif /* not HAVE_GETCLOCK */
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&clockstats, (u_long)(tv.tv_sec + JAN_1970));
	if (clockstats.fp != NULL) {
		fprintf(clockstats.fp, "%lu %lu.%03lu %s %s\n",
		    day, sec, msec, ntoa(addr), text);
		fflush(clockstats.fp);
	}
}

/*
 * record_raw_stats - write raw timestamps to file
 *
 *
 * file format
 * time (s past midnight)
 * peer ip address
 * local ip address
 * t1 t2 t3 t4 timestamps
 */
void
record_raw_stats(
        struct sockaddr_in *srcadr,
        struct sockaddr_in *dstadr,
	l_fp *t1,
	l_fp *t2,
	l_fp *t3,
	l_fp *t4
	)
{
	struct timeval tv;
#ifdef HAVE_GETCLOCK
        struct timespec ts;
#endif
	u_long day, sec, msec;

	if (!stats_control)
		return;
#ifdef HAVE_GETCLOCK
        (void) getclock(TIMEOFDAY, &ts);
        tv.tv_sec = ts.tv_sec;
        tv.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
#endif /* not HAVE_GETCLOCK */
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&rawstats, (u_long)(tv.tv_sec + JAN_1970));
	if (rawstats.fp != NULL) {
                fprintf(rawstats.fp, "%lu %lu.%03lu %s %s %s %s %s %s\n",
		    day, sec, msec, ntoa(srcadr), ntoa(dstadr),
		    ulfptoa(t1, 9), ulfptoa(t2, 9), ulfptoa(t3, 9),
		    ulfptoa(t4, 9));
		fflush(rawstats.fp);
	}
}

/*
 * getauthkeys - read the authentication keys from the specified file
 */
void
getauthkeys(
	char *keyfile
	)
{
	int len;

	len = strlen(keyfile);
	if (len == 0)
		return;
	
	if (key_file_name != 0) {
		if (len > (int)strlen(key_file_name)) {
			(void) free(key_file_name);
			key_file_name = 0;
		}
	}

	if (key_file_name == 0) {
#ifndef SYS_WINNT
		key_file_name = (char*)emalloc((u_int) (len + 1));
#else
		key_file_name = (char*)emalloc((u_int)  (MAXPATHLEN));
#endif
	}
#ifndef SYS_WINNT
 	memmove(key_file_name, keyfile, (unsigned)(len+1));
#else
	if (!ExpandEnvironmentStrings(keyfile, key_file_name, MAXPATHLEN)) 
	{
		msyslog(LOG_ERR,
		    "ExpandEnvironmentStrings(KEY_FILE) failed: %m\n");
	}
#endif /* SYS_WINNT */

	authreadkeys(key_file_name);
}


/*
 * rereadkeys - read the authentication key file over again.
 */
void
rereadkeys(void)
{
	if (key_file_name != 0)
	    authreadkeys(key_file_name);
}
