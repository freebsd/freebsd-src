/*-
 * Copyright (c) 2026 Stéphane Rochoy <sheda@FreeBSD.org>
 * Copyright (c) 2022 Corey Hinshaw <corey@electrickite.org>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#ifdef USE_CAPSICUM
#include <sys/capsicum.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <dev/pwm/pwmc.h>

#include "cprofile.h"
#include "utils.h"

#define KELVIN_OFFSET (-273.15f)

cprofile_t cprofile = {0};

static struct pidfh *pfh = NULL;
static const char *pid_path = "/var/run/fand.pid";
static char pwm_device[PATH_MAX];
static char *temp_name = NULL;
static int fd = -1;
static bool inverted = false;
static bool daemonize = true;
static bool high_temp_set = false;
static bool show_status = false;
static float multiplier = 1.0;
static float offset = 0.0;
static char temp_unit[20]; /* SP MULTIPLY SP %.3f SP °C NUL */
static int off_temp = 50;
static int low_temp = 50;
static int high_temp = 0;
static unsigned int interval = 60;
static unsigned int low_period = 0;
static unsigned int low_duty = 0;
static bool low_duty_percent = false;
static unsigned int high_period = 0;
static unsigned int high_duty = 0;
static bool high_duty_percent = false;
static bool quit;

static void print_usage(void)
{
	printf("Usage:\n");
	printf("\tfand [options...] sensor fan_device\n");
}

static void set_temp_name(char *name)
{
	if (name[0] == '\0')
		errx(EX_USAGE, "Temperature sensor sysctl name not specified;"
		     " use -h to display usage");
	temp_name = name;
}

static void set_pwm_device(const char *dev)
{
	if (dev[0] == '\0')
		errx(EX_USAGE, "PWM device not specified;"
		     " use -h to display usage");
	if (dev[0] == '/')
		strlcpy(pwm_device, dev, sizeof(pwm_device));
	else
		snprintf(pwm_device, sizeof(pwm_device), "/dev/pwm/%s", dev);
}

static void set_duty(char *arg, unsigned int *duty, bool *percent)
{
	const char *errstr;

	if (strtoduty(arg, duty, percent, &errstr))
		errx(EX_USAGE, "Invalid duty (%s); use -h to display usage",
		    errstr);
}

static unsigned int calculate_duty(unsigned int duty, unsigned int period,
                                   bool percent)
{
	if (percent)
		return (unsigned int)((uint64_t)period * duty / 100);
	else
		return (duty);
}

static float normalize_temperature(float raw_temp)
{
	return (raw_temp * multiplier + offset);
}

static int current_temperature(float *temp)
{
	int    raw_temp;
	size_t len;

	if (temp == NULL) {
		errno = EINVAL;
		return (-1);
	}

	len = sizeof(raw_temp);
	if (sysctlbyname(temp_name, &raw_temp, &len, NULL, 0) != 0)
		return (-1);
	*temp = normalize_temperature((float)raw_temp);
	return (0);
}

static void fprintf_status(FILE *stream, struct pwm_state state, float temp)
{
	fprintf(stream, "Sensor: %s\n", temp_name);
	fprintf(stream, "  Temperature: %.1f %s\n", temp, temp_unit);
	fprintf(stream, "Fan PWM Device: %s\n", pwm_device);
	fprintf(stream, "  Period:   %u ns\n", state.period);
	fprintf(stream, "  Duty:     %u ns (%u%%)\n", state.duty,
	        state.duty * 100 / state.period);
	fprintf(stream, "  Enabled:  %d\n", state.enable);
	fprintf(stream, "  Inverted: %d\n",
	        state.flags & PWM_POLARITY_INVERTED);
}

static void cleanup(void)
{
	if (!daemonize)
		warnx("Terminating");
	pidfile_remove(pfh);
	if (fd != -1) {
		struct pwm_state state;

		if (ioctl(fd, PWMGETSTATE, &state) == -1) {
			warn("Cannot read state of the PWM controller");
			close(fd);
			return;
		}
		state.enable = false;
		if (ioctl(fd, PWMSETSTATE, &state) == -1)
			warn("Cannot configure the PWM controller");
		close(fd);
	}
}

static void print_cprofile(void)
{
	cstep_t *step, *step_end;

	if (cprofile.step_count == 0) {
		printf("No cooling profile.\n");
		return;
	}

	step     = &cprofile.steps[0];
	step_end = step + cprofile.step_count;

	printf("Cooling Profile:\n");
	printf("    Low Duty: %u ns\n", cprofile.lo_duty);
	printf("    Steps:\n");
	for (; step < step_end; step++)
		printf("        %.2f %s → %u ns\n", step->raw_temp, temp_unit,
		       step->duty);
}

/*
 * Change the PWM state from old to new by applying the given cooling profile to
 * the given temperature.
 * Assume:
 * - step_count > 0
 * - curtemp is already normalized
 */
static void apply_cprofile(float curtemp,
                           struct pwm_state *old, struct pwm_state *new)
{
	cstep_t *step, *step_start;
	float    step_temp;

	step       = &cprofile.steps[cprofile.step_count - 1];
	step_start = &cprofile.steps[0];

	/* Walk backward through steps */
	for (; step_start <= step; step--) {
		new->duty = step->duty;
		step_temp = normalize_temperature(step->raw_temp);
		if (curtemp >= step_temp) {
			warnx("cprofile: Step found: %.2f%s ≥ %.2f%s",
			      curtemp, temp_unit, step_temp, temp_unit);
			break;
		}
	}
	if (++step == step_start) { /* no matching step found */
		warnx("cprofile: No matching step found, using low duty");
		new->duty = cprofile.lo_duty;
	}
	new->enable = true;

	warnx("cprofile: %u ns (%u%%) → %u ns (%u%%)",
	      old->duty, old->duty * 100 / old->period,
	      new->duty, new->duty * 100 / new->period);
}


static void sigterm(int sig __unused)
{
	quit = true;
}

#ifdef USE_CAPSICUM
/* Setup capabilities on fd. */
static void capset(void)
{
	cap_rights_t        rights;
	const unsigned long cmds[] = { PWMGETSTATE, PWMSETSTATE };

	if (cap_enter() < 0)
		err(EX_OSERR, "cap_enter");

	cap_rights_init(&rights, CAP_IOCTL);
	if (cap_rights_limit(fd, &rights) < 0)
		err(EX_OSERR, "cap_rights_limit");
	if (cap_ioctls_limit(fd, cmds, nitems(cmds)))
		err(EX_OSERR, "cap_ioctls_limit");
}
#endif /* USE_CAPSICUM */

int main(int argc, char *argv[])
{
	struct pwm_state current_state, new_state;
	struct  sigaction sigtermact = {.sa_handler = sigterm };
	pid_t otherpid;
	int ch;
	float temp;
	bool use_cprofile = false, show_cprofile = false;

	pwm_device[0] = '\0';
	memset(&current_state, 0, sizeof(current_state));
	memset(&new_state, 0, sizeof(new_state));

	atexit(cleanup);

	while ((ch = getopt(argc, argv,
	                    "c:ht:T:o:i:IKlm:p:P:d:D:r:fs")) != -1) {
		switch (ch) {
		case 'c':
			if (parse_cprofile(optarg) != 0)
				errx(EX_CONFIG, "Invalid cprofile: %s",
				     optarg);
			use_cprofile = true;
			break;
		case 'h':
			print_usage();
			exit(EX_OK);
		case 't':
			low_temp = (int)strtol(optarg, NULL, 10);
			break;
		case 'T':
			high_temp_set = true;
			high_temp = (int)strtol(optarg, NULL, 10);
			break;
		case 'o':
			off_temp = (int)strtol(optarg, NULL, 10);
			break;
		case 'i':
			interval = (unsigned int)strtol(optarg, NULL, 10);
			break;
		case 'I':
			inverted = true;
			break;
		case 'K':
			offset = KELVIN_OFFSET;
			break;
		case 'l':
			show_cprofile = true;
			break;
		case 'm':
			multiplier = strtof(optarg, NULL);
			break;
		case 'p':
			low_period = (unsigned int)strtoul(optarg, NULL, 10);
			break;
		case 'P':
			high_period = (unsigned int)strtoul(optarg, NULL, 10);
			break;
		case 'd':
			set_duty(optarg, &low_duty, &low_duty_percent);
			break;
		case 'D':
			set_duty(optarg, &high_duty, &high_duty_percent);
			break;
		case 'f':
			daemonize = false;
			break;
		case 'r':
			pid_path = optarg;
			break;
		case 's':
			show_status = true;
			break;
		case '?':
			errx(EX_USAGE, "Unknown option `-%c';"
			     " use -h to display usage", optopt);
		case ':':
			errx(EX_USAGE, "Option -%c requires an argument;"
			     " use -h to display usage", optopt);
		}
	}

	if (multiplier != 1.0)
		snprintf(temp_unit, sizeof(temp_unit), "× %5.3f ", multiplier);
	if (offset == 0)
		snprintf(temp_unit + strlen(temp_unit),
		    sizeof(temp_unit) - strlen(temp_unit), "°C");
	else
		snprintf(temp_unit + strlen(temp_unit),
		    sizeof(temp_unit) - strlen(temp_unit), "K");

	if (show_cprofile) {
		print_cprofile();
		exit(EX_OK);
	}

	if (argc - optind != 2)
		errx(EX_USAGE, "Wrong number of arguments;"
		     " use -h to display usage");
	set_temp_name(argv[optind]);
	set_pwm_device(argv[optind + 1]);

	if (use_cprofile && offset > 0)
		errx(EX_USAGE, "Conflicting options: -K and -c;"
		     " use -h to display usage");
	/* FIXME Handle other conflicts with -c */

	if (off_temp > low_temp)
		off_temp = low_temp;
	if (high_temp <= low_temp)
		high_temp_set = false;

	if ((fd = open(pwm_device, 0)) == -1)
		err(EX_OSFILE, "Cannot open %s", pwm_device);
#ifdef USE_CAPSICUM
	capset(); /* will err() on error. */
#endif /* USE_CAPSICUM */
	if (ioctl(fd, PWMGETSTATE, &current_state) == -1)
		err(EX_OSERR, "%s: Cannot read state of the PWM controller",
		    pwm_device);
	if (current_state.enable)
		warnx("%s: Already enabled", pwm_device);
	if (current_temperature(&temp) != 0) {
		/*
		 * Don't attempt to disable PWM as we didn't enable it ourself.
		 */
		close(fd);
		fd = -1;
		err(EX_OSERR, "Cannot read temperature from %s", temp_name);
	}

	if (show_status) {
		fprintf_status(stdout, current_state, temp);
		exit(EX_OK);
	}

	if (daemonize) {
		pfh = pidfile_open(pid_path, 0600, &otherpid);
		if (pfh == NULL) {
			if (errno == EEXIST)
				errx(EX_SOFTWARE, "already running, pid: %jd.",
				     (intmax_t)otherpid);
			warn("Cannot open or create pidfile");
		}
		if (daemon(1, 1) < 0)
			err(EX_OSERR, "Failed to daemonize!");
		pidfile_write(pfh);
	} else
		fprintf_status(stderr, current_state, temp);

	if (sigaction(SIGTERM, &sigtermact, NULL) != 0)
		err(EX_OSERR, "sigaction(SIGTERM)");
	if (sigaction(SIGINT, &sigtermact, NULL) != 0)
		err(EX_OSERR, "sigaction(SIGINT)");

	for (quit = false; !quit;) {
		new_state = current_state;

		if (current_temperature(&temp) != 0)
			err(EX_OSERR, "Cannot read temperature from %s",
			    temp_name);

		if (!daemonize)
			warnx("Current Temperature: %.2f %s", temp, temp_unit);

		if (use_cprofile) /* Apply the cooling profile */
			apply_cprofile(temp, &current_state, &new_state);
		else {
			if (temp >= (float)low_temp)
				new_state.enable = true;
			else if (temp < (float)off_temp)
				new_state.enable = false;

			if (high_temp_set && temp >= (float)high_temp) {
				if (high_period > 0)
					new_state.period = high_period;
				new_state.duty = calculate_duty(high_duty,
				    new_state.period, high_duty_percent);
			} else {
				if (low_period > 0)
					new_state.period = low_period;
				new_state.duty = calculate_duty(low_duty,
				    new_state.period, low_duty_percent);
			}
		}

		if (inverted)
			new_state.flags |= PWM_POLARITY_INVERTED;
		else
			new_state.flags &= ~(uint32_t)PWM_POLARITY_INVERTED;

		if (memcmp(&current_state, &new_state,
		           sizeof(current_state)) != 0) {
			if (!daemonize) {
				warnx(">> Fan state change <<");
				fprintf_status(stderr, new_state, temp);
			}
			if (ioctl(fd, PWMSETSTATE, &new_state) == -1)
				err(EX_OSERR,
				    "Cannot configure the PWM controller");
			current_state = new_state;
		}

		sleep(interval);
	}
	return (EX_OK);
}
