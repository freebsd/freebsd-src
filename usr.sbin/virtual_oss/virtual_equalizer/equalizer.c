/*-
 * Copyright (c) 2019 Google LLC, written by Richard Kralovic <riso@google.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fftw3.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "virtual_oss.h"

struct Equalizer {
	double	rate;
	int	block_size;
	int	do_normalize;

	/* (block_size * 2) elements, time domain */
	double *fftw_time;

	/* (block_size * 2) elements, half-complex, freq domain */
	double *fftw_freq;

	fftw_plan forward;
	fftw_plan inverse;
};

static int be_silent = 0;

static void
message(const char *fmt,...)
{
	va_list list;

	if (be_silent)
		return;
	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);
}

/*
 * Masking window value for -1 < x < 1.
 *
 * Window must be symmetric, thus, this function is queried for x >= 0
 * only. Currently a Hann window.
 */
static double
equalizer_get_window(double x)
{
	return (0.5 + 0.5 * cos(M_PI * x));
}

static int
equalizer_load_freq_amps(struct Equalizer *e, const char *config)
{
	double prev_f = 0.0;
	double prev_amp = 1.0;
	double next_f = 0.0;
	double next_amp = 1.0;
	int i;

	if (strncasecmp(config, "normalize", 4) == 0) {
		while (*config != 0) {
			if (*config == '\n') {
				config++;
				break;
			}
			config++;
		}
		e->do_normalize = 1;
	} else {
		e->do_normalize = 0;
	}

	for (i = 0; i <= (e->block_size / 2); ++i) {
		const double f = (i * e->rate) / e->block_size;

		while (f >= next_f) {
			prev_f = next_f;
			prev_amp = next_amp;

			if (*config == 0) {
				next_f = e->rate;
				next_amp = prev_amp;
			} else {
				int len;

				if (sscanf(config, "%lf %lf %n", &next_f, &next_amp, &len) == 2) {
					config += len;
					if (next_f < prev_f) {
						message("Parse error: Nonincreasing sequence of frequencies.\n");
						return (0);
					}
				} else {
					message("Parse error.\n");
					return (0);
				}
			}
			if (prev_f == 0.0)
				prev_amp = next_amp;
		}
		e->fftw_freq[i] = ((f - prev_f) / (next_f - prev_f)) * (next_amp - prev_amp) + prev_amp;
	}
	return (1);
}

static void
equalizer_init(struct Equalizer *e, int rate, int block_size)
{
	size_t buffer_size;

	e->rate = rate;
	e->block_size = block_size;

	buffer_size = sizeof(double) * e->block_size;

	e->fftw_time = (double *)malloc(buffer_size);
	e->fftw_freq = (double *)malloc(buffer_size);

	e->forward = fftw_plan_r2r_1d(block_size, e->fftw_time, e->fftw_freq,
	    FFTW_R2HC, FFTW_MEASURE);
	e->inverse = fftw_plan_r2r_1d(block_size, e->fftw_freq, e->fftw_time,
	    FFTW_HC2R, FFTW_MEASURE);
}

static int
equalizer_load(struct Equalizer *eq, const char *config)
{
	int retval = 0;
	int N = eq->block_size;
	int buffer_size = sizeof(double) * N;
	int i;

	memset(eq->fftw_freq, 0, buffer_size);

	message("\n\nReloading amplification specifications:\n%s\n", config);

	if (!equalizer_load_freq_amps(eq, config))
		goto end;

	double *requested_freq = (double *)malloc(buffer_size);

	memcpy(requested_freq, eq->fftw_freq, buffer_size);

	fftw_execute(eq->inverse);

	/* Multiply by symmetric window and shift */
	for (i = 0; i < (N / 2); ++i) {
		double weight = equalizer_get_window(i / (double)(N / 2)) / N;

		eq->fftw_time[N / 2 + i] = eq->fftw_time[i] * weight;
	}
	for (i = (N / 2 - 1); i > 0; --i) {
		eq->fftw_time[i] = eq->fftw_time[N - i];
	}
	eq->fftw_time[0] = 0;

	fftw_execute(eq->forward);
	for (i = 0; i < N; ++i) {
		eq->fftw_freq[i] /= (double)N;
	}

	/* Debug output */
	for (i = 0; i <= (N / 2); ++i) {
		double f = (eq->rate / N) * i;
		double a = sqrt(pow(eq->fftw_freq[i], 2.0) +
		    ((i > 0 && i < N / 2) ? pow(eq->fftw_freq[N - i], 2.0) : 0));

		a *= N;
		double r = requested_freq[i];

		message("%3.1lf Hz: requested %2.2lf, got %2.7lf (log10 = %.2lf), %3.7lfdb\n",
		    f, r, a, log(a) / log(10), (log(a / r) / log(10.0)) * 10.0);
	}

	/* Normalize FIR filter, if any */
	if (eq->do_normalize) {
		double sum = 0;

		for (i = 0; i < N; ++i)
			sum += fabs(eq->fftw_time[i]);
		if (sum != 0.0) {
			for (i = 0; i < N; ++i)
				eq->fftw_time[i] /= sum;
		}
	}
	for (i = 0; i < N; ++i) {
		message("%.3lf ms: %.10lf\n", 1000.0 * i / eq->rate, eq->fftw_time[i]);
	}

	/* End of debug */

	retval = 1;

	free(requested_freq);
end:
	return (retval);
}

static void
equalizer_done(struct Equalizer *eq)
{

	fftw_destroy_plan(eq->forward);
	fftw_destroy_plan(eq->inverse);
	free(eq->fftw_time);
	free(eq->fftw_freq);
}

static struct option equalizer_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"part", required_argument, NULL, 'p'},
	{"channels", required_argument, NULL, 'c'},
	{"what", required_argument, NULL, 'w'},
	{"off", no_argument, NULL, 'o'},
	{"quiet", no_argument, NULL, 'q'},
	{"file", no_argument, NULL, 'f'},
	{"help", no_argument, NULL, 'h'},
};

static void
usage(void)
{
	message("Usage: virtual_equalizer -d /dev/vdsp.ctl \n"
	    "\t -d, --device [control device]\n"
	    "\t -w, --what [rx_dev,tx_dev,rx_loop,tx_loop, default tx_dev]\n"
	    "\t -p, --part [part number, default 0]\n"
	    "\t -c, --channels [channels, default -1]\n"
	    "\t -f, --file [read input from file, default standard input]\n"
	    "\t -o, --off [disable equalizer]\n"
	    "\t -q, --quiet\n"
	    "\t -h, --help\n");
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	struct virtual_oss_fir_filter fir = {};
	struct virtual_oss_io_info info = {};

	struct Equalizer e;

	char buffer[65536];
	unsigned cmd_fir_set = VIRTUAL_OSS_SET_TX_DEV_FIR_FILTER;
	unsigned cmd_fir_get = VIRTUAL_OSS_GET_TX_DEV_FIR_FILTER;
	unsigned cmd_info = VIRTUAL_OSS_GET_DEV_INFO;
	const char *dsp = NULL;
	int rate;
	int channels = -1;
	int part = 0;
	int opt;
	int len;
	int offset;
	int disable = 0;
	int f = STDIN_FILENO;

	while ((opt = getopt_long(argc, argv, "d:c:f:op:w:qh",
	    equalizer_opts, NULL)) != -1) {
		switch (opt) {
		case 'd':
			dsp = optarg;
			break;
		case 'c':
			channels = atoi(optarg);
			if (channels == 0) {
				message("Wrong number of channels\n");
				usage();
			}
			break;
		case 'p':
			part = atoi(optarg);
			if (part < 0) {
				message("Invalid part number\n");
				usage();
			}
			break;
		case 'w':
			if (strcmp(optarg, "rx_dev") == 0) {
				cmd_fir_set = VIRTUAL_OSS_SET_RX_DEV_FIR_FILTER;
				cmd_fir_get = VIRTUAL_OSS_GET_RX_DEV_FIR_FILTER;
				cmd_info = VIRTUAL_OSS_GET_DEV_INFO;
			} else if (strcmp(optarg, "tx_dev") == 0) {
				cmd_fir_set = VIRTUAL_OSS_SET_TX_DEV_FIR_FILTER;
				cmd_fir_get = VIRTUAL_OSS_GET_TX_DEV_FIR_FILTER;
				cmd_info = VIRTUAL_OSS_GET_DEV_INFO;
			} else if (strcmp(optarg, "rx_loop") == 0) {
				cmd_fir_set = VIRTUAL_OSS_SET_RX_LOOP_FIR_FILTER;
				cmd_fir_get = VIRTUAL_OSS_GET_RX_LOOP_FIR_FILTER;
				cmd_info = VIRTUAL_OSS_GET_LOOP_INFO;
			} else if (strcmp(optarg, "tx_loop") == 0) {
				cmd_fir_set = VIRTUAL_OSS_SET_TX_LOOP_FIR_FILTER;
				cmd_fir_get = VIRTUAL_OSS_GET_TX_LOOP_FIR_FILTER;
				cmd_info = VIRTUAL_OSS_GET_LOOP_INFO;
			} else {
				message("Bad -w argument not recognized\n");
				usage();
			}
			break;
		case 'f':
			if (f != STDIN_FILENO) {
				message("Can only specify one file\n");
				usage();
			}
			f = open(optarg, O_RDONLY);
			if (f < 0) {
				message("Cannot open specified file\n");
				usage();
			}
			break;
		case 'o':
			disable = 1;
			break;
		case 'q':
			be_silent = 1;
			break;
		default:
			usage();
		}
	}

	fir.number = part;
	info.number = part;

	int fd = open(dsp, O_RDWR);

	if (fd < 0) {
		message("Cannot open DSP device\n");
		return (EX_SOFTWARE);
	}
	if (ioctl(fd, VIRTUAL_OSS_GET_SAMPLE_RATE, &rate) < 0) {
		message("Cannot get sample rate\n");
		return (EX_SOFTWARE);
	}
	if (ioctl(fd, cmd_fir_get, &fir) < 0) {
		message("Cannot get current FIR filter\n");
		return (EX_SOFTWARE);
	}
	if (disable) {
	  	for (fir.channel = 0; fir.channel != channels; fir.channel++) {
			if (ioctl(fd, cmd_fir_set, &fir) < 0) {
				if (fir.channel == 0) {
					message("Cannot disable FIR filter\n");
					return (EX_SOFTWARE);
				}
				break;
			}
		}
		return (0);
	}
	equalizer_init(&e, rate, fir.filter_size);
	equalizer_load(&e, "");

	if (f == STDIN_FILENO) {
		if (ioctl(fd, cmd_info, &info) < 0) {
			message("Cannot read part information\n");
			return (EX_SOFTWARE);
		}
		message("Please enter EQ layout for %s, <freq> <gain>:\n", info.name);
	}
	offset = 0;
	while (1) {
		if (offset == (int)(sizeof(buffer) - 1)) {
			message("Too much input data\n");
			return (EX_SOFTWARE);
		}
		len = read(f, buffer + offset, sizeof(buffer) - 1 - offset);
		if (len <= 0)
			break;
		offset += len;
	}
	buffer[offset] = 0;
	close(f);

	if (f == STDIN_FILENO)
		message("Loading new EQ layout\n");

	if (equalizer_load(&e, buffer) == 0) {
		message("Invalid equalizer data\n");
		return (EX_SOFTWARE);
	}
	fir.filter_data = e.fftw_time;

	for (fir.channel = 0; fir.channel != channels; fir.channel++) {
		if (ioctl(fd, cmd_fir_set, &fir) < 0) {
			if (fir.channel == 0)
				message("Cannot set FIR filter on channel\n");
			break;
		}
	}

	close(fd);
	equalizer_done(&e);

	return (0);
}
