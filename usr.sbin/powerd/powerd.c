/*-
 * Copyright (c) 2004 Colin Percival
 * Copyright (c) 2005 Nate Lawson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_APM
#include <machine/apm_bios.h>
#endif

#define DEFAULT_ACTIVE_PERCENT	65
#define DEFAULT_IDLE_PERCENT	90
#define DEFAULT_POLL_INTERVAL	500	/* Poll interval in milliseconds */

typedef enum {
	MODE_MIN,
	MODE_ADAPTIVE,
	MODE_MAX,
} modes_t;

typedef enum {
	SRC_AC,
	SRC_BATTERY,
	SRC_UNKNOWN,
} power_src_t;

const char *modes[] = {
	"AC",
	"battery",
	"unknown"
};

#define ACPIAC		"hw.acpi.acline"
#define APMDEV		"/dev/apm"
#define DEVDPIPE	"/var/run/devd.pipe"
#define DEVCTL_MAXBUF	1024

static int	read_usage_times(long *idle, long *total);
static int	read_freqs(int *numfreqs, int **freqs, int **power);
static int	set_freq(int freq);
static void	acline_init(void);
static void	acline_read(void);
static int	devd_init(void);
static void	devd_close(void);
static void	handle_sigs(int sig);
static void	parse_mode(char *arg, int *mode, int ch);
static void	usage(void);

/* Sysctl data structures. */
static int	cp_time_mib[2];
static int	freq_mib[4];
static int	levels_mib[4];
static int	acline_mib[3];

/* Configuration */
static int	cpu_running_mark;
static int	cpu_idle_mark;
static int	poll_ival;
static int	vflag;

static volatile sig_atomic_t exit_requested;
static power_src_t acline_status;
static enum {
	ac_none,
	ac_acpi_sysctl,
	ac_acpi_devd,
#ifdef USE_APM
	ac_apm,
#endif
} acline_mode;
#ifdef USE_APM
static int	apm_fd = -1;
#endif
static int	devd_pipe = -1;

#define DEVD_RETRY_INTERVAL 60 /* seconds */
static struct timeval tried_devd;

static int
read_usage_times(long *idle, long *total)
{
	static long idle_old, total_old;
	long cp_time[CPUSTATES], i, total_new;
	size_t cp_time_len;
	int error;

	cp_time_len = sizeof(cp_time);
	error = sysctl(cp_time_mib, 2, cp_time, &cp_time_len, NULL, 0);
	if (error)
		return (error);
	for (total_new = 0, i = 0; i < CPUSTATES; i++)
		total_new += cp_time[i];

	if (idle)
		*idle = cp_time[CP_IDLE] - idle_old;
	if (total)
		*total = total_new - total_old;

	idle_old = cp_time[CP_IDLE];
	total_old = total_new;

	return (0);
}

static int
read_freqs(int *numfreqs, int **freqs, int **power)
{
	char *freqstr, *p, *q;
	int i;
	size_t len = 0;

	if (sysctl(levels_mib, 4, NULL, &len, NULL, 0))
		return (-1);
	if ((freqstr = malloc(len)) == NULL)
		return (-1);
	if (sysctl(levels_mib, 4, freqstr, &len, NULL, 0))
		return (-1);

	*numfreqs = 1;
	for (p = freqstr; *p != '\0'; p++)
		if (*p == ' ')
			(*numfreqs)++;

	if ((*freqs = malloc(*numfreqs * sizeof(int))) == NULL) {
		free(freqstr);
		return (-1);
	}
	if ((*power = malloc(*numfreqs * sizeof(int))) == NULL) {
		free(freqstr);
		free(*freqs);
		return (-1);
	}
	for (i = 0, p = freqstr; i < *numfreqs; i++) {
		q = strchr(p, ' ');
		if (q != NULL)
			*q = '\0';
		if (sscanf(p, "%d/%d", &(*freqs)[i], &(*power)[i]) != 2) {
			free(freqstr);
			free(*freqs);
			free(*power);
			return (-1);
		}
		p = q + 1;
	}

	free(freqstr);
	return (0);
}

static int
set_freq(int freq)
{

	if (sysctl(freq_mib, 4, NULL, NULL, &freq, sizeof(freq))) {
		if (errno != EPERM)
			return (-1);
	}

	return (0);
}

/*
 * Try to use ACPI to find the AC line status.  If this fails, fall back
 * to APM.  If nothing succeeds, we'll just run in default mode.
 */
static void
acline_init()
{
	size_t len;

	len = 3;
	if (sysctlnametomib(ACPIAC, acline_mib, &len) == 0) {
		acline_mode = ac_acpi_sysctl;
		if (vflag)
			warnx("using sysctl for AC line status");
#ifdef USE_APM
	} else if ((apm_fd = open(APMDEV, O_RDONLY)) >= 0) {
		if (vflag)
			warnx("using APM for AC line status");
		acline_mode = ac_apm;
#endif
	} else {
		warnx("unable to determine AC line status");
		acline_mode = ac_none;
	}
}

static void
acline_read(void)
{
	if (acline_mode == ac_acpi_devd) {
		char buf[DEVCTL_MAXBUF], *ptr;
		ssize_t rlen;
		int notify;

		rlen = read(devd_pipe, buf, sizeof(buf));
		if (rlen == 0 || (rlen < 0 && errno != EWOULDBLOCK)) {
			if (vflag)
				warnx("lost devd connection, switching to sysctl");
			devd_close();
			acline_mode = ac_acpi_sysctl;
			/* FALLTHROUGH */
		}
		if (rlen > 0 &&
		    (ptr = strstr(buf, "system=ACPI")) != NULL &&
		    (ptr = strstr(ptr, "subsystem=ACAD")) != NULL &&
		    (ptr = strstr(ptr, "notify=")) != NULL &&
		    sscanf(ptr, "notify=%x", &notify) == 1)
			acline_status = (notify ? SRC_AC : SRC_BATTERY);
	}
	if (acline_mode == ac_acpi_sysctl) {
		int acline;
		size_t len;

		len = sizeof(acline);
		if (sysctl(acline_mib, 3, &acline, &len, NULL, 0) == 0)
			acline_status = (acline ? SRC_AC : SRC_BATTERY);
		else
			acline_status = SRC_UNKNOWN;
	}
#ifdef USE_APM
	if (acline_mode == ac_apm) {
		struct apm_info info;

		if (ioctl(apm_fd, APMIO_GETINFO, &info) == 0) {
			acline_status = (info.ai_acline ? SRC_AC : SRC_BATTERY);
		} else {
			close(apm_fd);
			apm_fd = -1;
			acline_mode = ac_none;
			acline_status = SRC_UNKNOWN;
		}
	}
#endif
	/* try to (re)connect to devd */
	if (acline_mode == ac_acpi_sysctl) {
		struct timeval now;

		gettimeofday(&now, NULL);
		if (now.tv_sec > tried_devd.tv_sec + DEVD_RETRY_INTERVAL) {
			if (devd_init() >= 0) {
				if (vflag)
					warnx("using devd for AC line status");
				acline_mode = ac_acpi_devd;
			}
			tried_devd = now;
		}
	}
}

static int
devd_init(void)
{
	struct sockaddr_un devd_addr;

	bzero(&devd_addr, sizeof(devd_addr));
	if ((devd_pipe = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
		if (vflag)
			warn("%s(): socket()", __func__);
		return (-1);
	}

	devd_addr.sun_family = PF_LOCAL;
	strlcpy(devd_addr.sun_path, DEVDPIPE, sizeof(devd_addr.sun_path));
	if (connect(devd_pipe, (struct sockaddr *)&devd_addr,
	    sizeof(devd_addr)) == -1) {
		if (vflag)
			warn("%s(): connect()", __func__);
		close(devd_pipe);
		devd_pipe = -1;
		return (-1);
	}

	if (fcntl(devd_pipe, F_SETFL, O_NONBLOCK) == -1) {
		if (vflag)
			warn("%s(): fcntl()", __func__);
		close(devd_pipe);
		return (-1);
	}

	return (devd_pipe);
}

static void
devd_close(void)
{

	close(devd_pipe);
	devd_pipe = -1;
}

static void
parse_mode(char *arg, int *mode, int ch)
{

	if (strcmp(arg, "minimum") == 0 || strcmp(arg, "min") == 0)
		*mode = MODE_MIN;
	else if (strcmp(arg, "maximum") == 0 || strcmp(arg, "max") == 0)
		*mode = MODE_MAX;
	else if (strcmp(arg, "adaptive") == 0)
		*mode = MODE_ADAPTIVE;
	else
		errx(1, "bad option: -%c %s", (char)ch, optarg);
}

static void
handle_sigs(int __unused sig)
{

	exit_requested = 1;
}

static void
usage(void)
{

	fprintf(stderr,
"usage: powerd [-v] [-a mode] [-b mode] [-i %%] [-n mode] [-p ival] [-r %%] [-P pidfile]\n");
	exit(1);
}

int
main(int argc, char * argv[])
{
	struct timeval timeout;
	fd_set fdset;
	int nfds;
	struct pidfh *pfh = NULL;
	const char *pidfile = NULL;
	long idle, total;
	int curfreq, *freqs, i, *mwatts, numfreqs;
	int ch, mode, mode_ac, mode_battery, mode_none;
	uint64_t mjoules_used;
	size_t len;

	/* Default mode for all AC states is adaptive. */
	mode_ac = mode_battery = mode_none = MODE_ADAPTIVE;
	cpu_running_mark = DEFAULT_ACTIVE_PERCENT;
	cpu_idle_mark = DEFAULT_IDLE_PERCENT;
	poll_ival = DEFAULT_POLL_INTERVAL;
	mjoules_used = 0;
	vflag = 0;

	/* User must be root to control frequencies. */
	if (geteuid() != 0)
		errx(1, "must be root to run");

	while ((ch = getopt(argc, argv, "a:b:i:n:p:P:r:v")) != EOF)
		switch (ch) {
		case 'a':
			parse_mode(optarg, &mode_ac, ch);
			break;
		case 'b':
			parse_mode(optarg, &mode_battery, ch);
			break;
		case 'i':
			cpu_idle_mark = atoi(optarg);
			if (cpu_idle_mark < 0 || cpu_idle_mark > 100) {
				warnx("%d is not a valid percent",
				    cpu_idle_mark);
				usage();
			}
			break;
		case 'n':
			parse_mode(optarg, &mode_none, ch);
			break;
		case 'p':
			poll_ival = atoi(optarg);
			if (poll_ival < 5) {
				warnx("poll interval is in units of ms");
				usage();
			}
			break;
		case 'P':
			pidfile = optarg;
			break;
		case 'r':
			cpu_running_mark = atoi(optarg);
			if (cpu_running_mark < 0 || cpu_running_mark > 100) {
				warnx("%d is not a valid percent",
				    cpu_running_mark);
				usage();
			}
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}

	mode = mode_none;

	/* Poll interval is in units of ms. */
	poll_ival *= 1000;

	/* Look up various sysctl MIBs. */
	len = 2;
	if (sysctlnametomib("kern.cp_time", cp_time_mib, &len))
		err(1, "lookup kern.cp_time");
	len = 4;
	if (sysctlnametomib("dev.cpu.0.freq", freq_mib, &len))
		err(1, "lookup freq");
	len = 4;
	if (sysctlnametomib("dev.cpu.0.freq_levels", levels_mib, &len))
		err(1, "lookup freq_levels");

	/* Check if we can read the idle time and supported freqs. */
	if (read_usage_times(NULL, NULL))
		err(1, "read_usage_times");
	if (read_freqs(&numfreqs, &freqs, &mwatts))
		err(1, "error reading supported CPU frequencies");

	/* Run in the background unless in verbose mode. */
	if (!vflag) {
		pid_t otherpid;

		pfh = pidfile_open(pidfile, 0600, &otherpid);
		if (pfh == NULL) {
			if (errno == EEXIST) {
				errx(1, "powerd already running, pid: %d",
				    otherpid);
			}
			warn("cannot open pid file");
		}
		if (daemon(0, 0) != 0) {
			warn("cannot enter daemon mode, exiting");
			pidfile_remove(pfh);
			exit(EXIT_FAILURE);

		}
		pidfile_write(pfh);
	}

	/* Decide whether to use ACPI or APM to read the AC line status. */
	acline_init();

	/*
	 * Exit cleanly on signals.
	 */
	signal(SIGINT, handle_sigs);
	signal(SIGTERM, handle_sigs);

	/* Main loop. */
	for (;;) {
		FD_ZERO(&fdset);
		if (devd_pipe >= 0) {
			FD_SET(devd_pipe, &fdset);
			nfds = devd_pipe + 1;
		} else {
			nfds = 0;
		}
		timeout.tv_sec = poll_ival / 1000000;
		timeout.tv_usec = poll_ival % 1000000;
		select(nfds, &fdset, NULL, &fdset, &timeout);

		/* If the user requested we quit, print some statistics. */
		if (exit_requested) {
			if (vflag && mjoules_used != 0)
				printf("total joules used: %u.%03u\n",
				    (u_int)(mjoules_used / 1000),
				    (int)mjoules_used % 1000);
			break;
		}

		/* Read the current AC status and record the mode. */
		acline_read();
		switch (acline_status) {
		case SRC_AC:
			mode = mode_ac;
			break;
		case SRC_BATTERY:
			mode = mode_battery;
			break;
		case SRC_UNKNOWN:
			mode = mode_none;
			break;
		default:
			errx(1, "invalid AC line status %d", acline_status);
		}

		/* Read the current frequency. */
		len = sizeof(curfreq);
		if (sysctl(freq_mib, 4, &curfreq, &len, NULL, 0) != 0) {
			if (vflag)
				warn("error reading current CPU frequency");
			continue;
		}

		if (vflag) {
			for (i = 0; i < numfreqs; i++) {
				if (freqs[i] == curfreq)
					break;
			}

			/* Keep a sum of all power actually used. */
			if (i < numfreqs && mwatts[i] != -1)
				mjoules_used +=
				    (mwatts[i] * (poll_ival / 1000)) / 1000;
		}

		/* Always switch to the lowest frequency in min mode. */
		if (mode == MODE_MIN) {
			if (curfreq != freqs[numfreqs - 1]) {
				if (vflag) {
					printf("now operating on %s power; "
					    "changing frequency to %d MHz\n",
					    modes[acline_status],
					    freqs[numfreqs - 1]);
				}
				if (set_freq(freqs[numfreqs - 1]) != 0) {
					warn("error setting CPU freq %d",
					    freqs[numfreqs - 1]);
					continue;
				}
			}
			continue;
		}

		/* Always switch to the highest frequency in max mode. */
		if (mode == MODE_MAX) {
			if (curfreq != freqs[0]) {
				if (vflag) {
					printf("now operating on %s power; "
					    "changing frequency to %d MHz\n",
					    modes[acline_status],
					    freqs[0]);
				}
				if (set_freq(freqs[0]) != 0) {
					warn("error setting CPU freq %d",
				    	    freqs[0]);
					continue;
				}
			}
			continue;
		}

		/* Adaptive mode; get the current CPU usage times. */
		if (read_usage_times(&idle, &total)) {
			if (vflag)
				warn("read_usage_times() failed");
			continue;
		}

		/*
		 * If we're idle less than the active mark, bump up two levels.
		 * If we're idle more than the idle mark, drop down one level.
		 */
		for (i = 0; i < numfreqs - 1; i++) {
			if (freqs[i] == curfreq)
				break;
		}
		if (idle < (total * cpu_running_mark) / 100 &&
		    curfreq < freqs[0]) {
			i -= 2;
			if (i < 0)
				i = 0;
			if (vflag) {
				printf("idle time < %d%%, increasing clock"
				    " speed from %d MHz to %d MHz\n",
				    cpu_running_mark, curfreq, freqs[i]);
			}
			if (set_freq(freqs[i]))
				err(1, "error setting CPU frequency %d",
				    freqs[i]);
		} else if (idle > (total * cpu_idle_mark) / 100 &&
		    curfreq > freqs[numfreqs - 1]) {
			i++;
			if (vflag) {
				printf("idle time > %d%%, decreasing clock"
				    " speed from %d MHz to %d MHz\n",
				    cpu_idle_mark, curfreq, freqs[i]);
			}
			if (set_freq(freqs[i]) != 0)
				warn("error setting CPU frequency %d",
				    freqs[i]);
		}
	}
	free(freqs);
	free(mwatts);
	devd_close();
	if (!vflag)
		pidfile_remove(pfh);

	exit(0);
}
