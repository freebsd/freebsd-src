/*
 * Copyright 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static const char rcsid[] =
	"$Id: main.c,v 1.4 1995/10/06 02:46:23 jkh Exp $";

#include <stdio.h>
#include <ncurses.h>
#include <dialog.h>
#include <limits.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "tzsetup.h"

#define PATH_LOCALTIME		"/etc/localtime"
#define PATH_WALL_CMOS_CLOCK	"/etc/wall_cmos_clock"
#define PATH_ZONEINFO		"/usr/share/zoneinfo"

static int set_time();

enum cmos { CMOS_UTC, CMOS_LOCAL, CMOS_LEAVE } cmos_state = CMOS_LEAVE;
static int time_adjust = 0;
static enum cmos cmos(enum cmos);
static void fiddle_cmos(void);

int
main(void)
{
	const char *tz;

	init_dialog();
	if (set_time() != 0) {
		end_dialog();
		exit(1);
	}

	tz = tzmenu();

	fiddle_cmos();

	dialog_notify("Reboot the machine for changes to take effect.\n");
	end_dialog();

	fprintf(stderr,
		"Now reboot your computer for the changes to take effect.\n");
	return tz ? 0 : 1;
}

static int
set_time(void)
{
	unsigned char result[_POSIX2_LINE_MAX];
	unsigned char buf2[_POSIX2_LINE_MAX];
	static struct tm usertm, systm;
	time_t usertime, systime;
	long diff;
	int rv;

	/*
	 * If /etc/wall_cmos_clock exists, or if there is already a timezone
	 * file installed, then just leave it alone and don't ask the user
	 * what time it is (because the system clock is already in POSIX
	 * time so we don't need to adjust anything later on).
	 */
	time(&systime);
	systm = *localtime(&systime);
	if (systm.tm_zone[0]
	    || access(PATH_WALL_CMOS_CLOCK, 0) == 0) {
		cmos_state = CMOS_LEAVE;
		return 0;
	}

	usertm = systm;
	usertm.tm_isdst = -1;

	result[0] = '\0';

	while(1) {
		rv = dialog_inputbox("Checking current time",
				     "Please enter the current local time"
				     " using 24-hour style,\n"
				     "in the form HH:MM",
				     8, 78, result);
		if (rv != 0)
			return 1;

		if (result[0]) {
			if (sscanf(result, "%d:%d:%d", &usertm.tm_hour,
				   &usertm.tm_min,
				   &usertm.tm_sec) < 2) {
				snprintf(buf2, sizeof buf2,
					 "Invalid time format: %s", result);
				dialog_notify(buf2);
				continue;
			}
			usertime = mktime(&usertm);
			if (usertime == (time_t)-1) {
				snprintf(buf2, sizeof buf2,
					 "Unreasonable time: %s", result);
				dialog_notify(buf2);
				continue;
			}

			diff = usertime - systime;
			if (labs(diff) > 15*60) {
				cmos_state = cmos(CMOS_LOCAL);
				if (diff > 0) {
					time_adjust = ((diff + 15*60)/30*60
						       * 30*60);
				} else {
					time_adjust = ((diff - 15*60)/30*60
						       * 30*60);
				}
			} else {
				cmos_state = cmos(CMOS_UTC);
				time_adjust = 0;
			}
			break;
		}
	}
	return 0;
}

static unsigned char *cmos_list[] = {
	"1", "CMOS clock is set to Universal time (UTC)",
	"2", "CMOS clock is set to local time",
	"3", "I'm not sure, leave it alone"
};

static enum cmos
cmos(enum cmos state)
{
	int rv, sel = 0;
	unsigned char buf[_POSIX2_LINE_MAX];
	unsigned char result[_POSIX2_LINE_MAX];

	snprintf(buf, sizeof buf, "%s seems most likely",
		 state == CMOS_UTC ? "UTC" : "local time");

	rv = dialog_menu("CMOS clock in local time or UTC",
			 buf, 12, 78, 3, 3, cmos_list, result, &sel, 0);
	if (rv == 0) {
		return sel;
	} else {
		return state;
	}
}

static void
fiddle_cmos(void)
{
	FILE *fp;

	switch(cmos_state) {
	case CMOS_LEAVE:
	case CMOS_UTC:
		break;
	case CMOS_LOCAL:
		fp = fopen(PATH_WALL_CMOS_CLOCK, "w");
		if(fp) {
			fclose(fp);
		} /* xxx should have error message */
	}
}


int
setzone(const char *zone)
{
	time_t systime;
	struct tm *tm;
	char msg[_POSIX2_LINE_MAX];
	int rv;
	FILE *ifp, *ofp;

	snprintf(msg, sizeof msg, "TZ=%s", zone);
	putenv(msg);
	tzset();
	time(&systime);
	systime += time_adjust;
	tm = localtime(&systime);

#if 0	/* This never prints the right value! :( */
	snprintf(msg, sizeof msg,
		 "Does %02d:%02d:%02d %d.%d.%04d %s look reasonable?",
		 tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday, tm->tm_mon,
		 tm->tm_year + 1900, tm->tm_zone);

	rv = dialog_yesno("Verifying timezone selection",
			  msg, -1, -1);
	if (rv)
		return 1;
#endif

	snprintf(msg, sizeof msg, PATH_ZONEINFO "/%s", zone);
	ifp = fopen(msg, "r");
	if (!ifp) {
		snprintf(msg, sizeof msg,
			 "Could not open " PATH_ZONEINFO "/%s: %s",
			 zone, strerror(errno));
		dialog_notify(msg);
		return 1;
	}

	ofp = fopen(PATH_LOCALTIME, "w");
	if (!ofp) {
		snprintf(msg, sizeof msg, "Could not open " PATH_LOCALTIME
			 ": %s", strerror(errno));
		dialog_notify(msg);
		fclose(ifp);
		return 1;
	}

	while((rv = fread(msg, 1, sizeof msg, ifp)) > 0) {
		int rv2;
		if((rv2 = fwrite(msg, 1, rv, ofp)) != rv) {
			snprintf(msg, sizeof msg,
				 "Could not write " PATH_LOCALTIME ": %s",
				 strerror(errno));
out:
			dialog_notify(msg);
			fclose(ifp);
			fclose(ofp);
			unlink(PATH_LOCALTIME);
			return 1;
		}
	}
	if (rv < 0) {
		snprintf(msg, sizeof msg, "Could not read timezone file: %s",
			 strerror(errno));
		goto out;
	}

	fclose(ifp);
	fclose(ofp);
	snprintf(msg, sizeof msg, "Installed timezone file %s", zone);
	dialog_notify(msg);
	return 0;
}
