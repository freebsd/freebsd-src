/*
 * ntp_util.c - stuff I didn't have any other place for
 */
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_filegen.h"
#include "ntp_if.h"
#include "ntp_stdlib.h"

#ifdef  DOSYNCTODR
#include <sys/resource.h>
#endif

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
#define NTP_VAR "/var/NTP/"		/* NOTE the trailing '/' */
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

static	char statsdir[MAXPATHLEN] = NTP_VAR;

static FILEGEN peerstats;
static FILEGEN loopstats;
static FILEGEN clockstats;
/*
 * We query the errno to see what kind of error occured
 * when opening the drift file.
 */
extern int errno;

/*
 * This controls whether stats are written to the fileset. Provided
 * so that xntpdc can turn off stats when the file system fills up. 
 */
int stats_control;

#ifdef DEBUG
extern int debug;
#endif

/*
 * init_util - initialize the utilities
 */
void
init_util()
{
	stats_drift_file = 0;
	stats_temp_file = 0;
	key_file_name = 0;

#define PEERNAME "peerstats"
#define LOOPNAME "loopstats"
#define CLOCKNAME "clockstats"
	peerstats.fp       = NULL;
	peerstats.prefix   = &statsdir[0];
	peerstats.basename = emalloc(strlen(PEERNAME)+1);
	strcpy(peerstats.basename, PEERNAME);
	peerstats.id       = 0;
	peerstats.type     = FILEGEN_DAY;
	peerstats.flag     = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("peerstats", &peerstats);
	
	loopstats.fp       = NULL;
	loopstats.prefix   = &statsdir[0];
	loopstats.basename = emalloc(strlen(LOOPNAME)+1);
	strcpy(loopstats.basename, LOOPNAME);
	loopstats.id       = 0;
	loopstats.type     = FILEGEN_DAY;
	loopstats.flag     = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("loopstats", &loopstats);

	clockstats.fp      = NULL;
	clockstats.prefix  = &statsdir[0];
	clockstats.basename = emalloc(strlen(CLOCKNAME)+1);
	strcpy(clockstats.basename, CLOCKNAME);
	clockstats.id      = 0;
	clockstats.type    = FILEGEN_DAY;
	clockstats.flag    = FGEN_FLAG_LINK; /* not yet enabled !!*/
	filegen_register("clockstats", &clockstats);

#undef PEERNAME
#undef LOOPNAME
#undef CLOCKNAME

}


/*
 * hourly_stats - print some interesting stats
 */
void
hourly_stats()
{
	FILE *fp;
	extern l_fp last_offset;
	extern s_fp drift_comp;
	extern u_char sys_poll;
	extern int pll_status;

#ifdef DOSYNCTODR
	struct timeval tv;
	int o_prio;

	/*
	 * Sometimes having a Sun can be a drag.
	 *
	 * The kernel variable dosynctodr controls whether the system's
	 * soft clock is kept in sync with the battery clock. If it
	 * is zero, then the soft clock is not synced, and the battery
	 * clock is simply left to rot. That means that when the system
	 * reboots, the battery clock (which has probably gone wacky)
	 * sets the soft clock. That means xntpd starts off with a very
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

	o_prio=getpriority(PRIO_PROCESS,0); /* Save setting */
	if (setpriority(PRIO_PROCESS,0,-20) != 0) /* overdrive */
	{
		syslog(LOG_ERR, "can't elevate priority: %m");
		goto skip;
	}
	GETTIMEOFDAY(&tv,(struct timezone *)NULL);
	if (SETTIMEOFDAY(&tv,(struct timezone *)NULL) != 0)
	{
		syslog(LOG_ERR, "can't sync battery time: %m");
	}
	setpriority(PRIO_PROCESS,0,o_prio); /* downshift */

 skip:
#endif

	syslog(LOG_INFO, "offset %s freq %s poll %d",
	    lfptoa(&last_offset, 6), fptoa(drift_comp, 3),
	    sys_poll);
	
	if (stats_drift_file != 0) {
		if ((fp = fopen(stats_temp_file, "w")) == NULL) {
			syslog(LOG_ERR, "can't open %s: %m",
			    stats_temp_file);
			return;
		}
		fprintf(fp, "%s %x\n", fptoa(drift_comp, 3),
		    pll_status);
		(void)fclose(fp);
		/* atomic */
		(void) rename(stats_temp_file, stats_drift_file);
	}
}


/*
 * stats_config - configure the stats operation
 */
void
stats_config(item, value)
	int item;
	char *value;	/* only one type so far */
{
	FILE *fp;
	char buf[128];
	l_fp old_drift;
	int temp = 0;
	int len;

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

		stats_drift_file = emalloc((u_int)(len + 1));
		stats_temp_file = emalloc((u_int)(len +
		    sizeof(".TEMP")));
		memmove(stats_drift_file, value, len+1);
		memmove(stats_temp_file, value, len);
		memmove(stats_temp_file + len, ".TEMP",
		    sizeof(".TEMP"));
		L_CLR(&old_drift);

		/*
		 * Open drift file and read frequency and mode.
		 */
		if ((fp = fopen(stats_drift_file, "r")) == NULL) {
			if (errno != ENOENT)
				syslog(LOG_ERR, "can't open %s: %m",
				       stats_drift_file);
		        loop_config(LOOP_DRIFTCOMP, &old_drift, 0);
			break;
		}

		if (fscanf(fp, "%s %x", buf, &temp) == 0) {
			syslog(LOG_ERR, "can't read %s: %m",
			       stats_drift_file);
			(void) fclose(fp);
		        loop_config(LOOP_DRIFTCOMP, &old_drift, 0);
			break;
		}
		(void) fclose(fp);
		if (!atolfp(buf, &old_drift)) {
			syslog(LOG_ERR, "drift value %s invalid", buf);
			break;
		}
		loop_config(LOOP_DRIFTCOMP, &old_drift, temp);
		break;
	
	case STATS_STATSDIR:
		if (strlen(value) >= sizeof(statsdir)) {
			syslog(LOG_ERR,
			       "value for statsdir too long (>%d, sigh)",
			       sizeof(statsdir)-1);
		} else {
			l_fp now;

			gettstamp(&now);
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
		}
		break;

	case STATS_PID_FILE:
		if ((fp = fopen(value, "w")) == NULL) {
			syslog(LOG_ERR, "Can't open %s: %m", value);
			break;
		}
		fprintf(fp, "%d", getpid());
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
 * time (s past midnight)
 * peer (ip address)
 * peer status word (hex)
 * peer offset (s)
 * peer delay (s)
 * peer dispersion (s)
 */
void
record_peer_stats(addr, status, offset, delay, dispersion)
	struct sockaddr_in *addr;
	int status;
	l_fp *offset;
	s_fp delay;
	u_fp dispersion;
{
	struct timeval tv;
	u_long day, sec, msec;

	if (!stats_control)
		return;
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&peerstats, (u_long)(tv.tv_sec + JAN_1970));
	if (peerstats.fp != NULL) {
		fprintf(peerstats.fp, "%lu %lu.%03lu %s %x %s %s %s\n",
			day, sec, msec, ntoa(addr), status, lfptoa(offset, 6),
			fptoa(delay, 5), ufptoa(dispersion, 5));
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
record_loop_stats(offset, freq, poll)
	l_fp *offset;
        s_fp freq;
        u_char poll;
{
	struct timeval tv;
	u_long day, sec, msec;

	if (!stats_control)
		return;
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
	day = tv.tv_sec / 86400 + MJD_1970;
	sec = tv.tv_sec % 86400;
	msec = tv.tv_usec / 1000;

	filegen_setup(&loopstats, (u_long)(tv.tv_sec + JAN_1970));
	if (loopstats.fp != NULL) {
		fprintf(loopstats.fp, "%lu %lu.%03lu %s %s %d\n",
			day, sec, msec, lfptoa(offset, 6),
			fptoa(freq, 4), poll);
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
record_clock_stats(addr, text)
	struct sockaddr_in *addr;
	char *text;
{
	struct timeval tv;
	u_long day, sec, msec;

	if (!stats_control)
		return;
	GETTIMEOFDAY(&tv, (struct timezone *)NULL);
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
 * getauthkeys - read the authentication keys from the specified file
 */
void
getauthkeys(keyfile)
	char *keyfile;
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

	if (key_file_name == 0)
		key_file_name = emalloc((u_int)(len + 1));
	
	memmove(key_file_name, keyfile, len+1);

	authreadkeys(key_file_name);
}


/*
 * rereadkeys - read the authentication key file over again.
 */
void
rereadkeys()
{
	if (key_file_name != 0)
		authreadkeys(key_file_name);
}
