/*-
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/stat.h>
#include <sys/types.h>

#include <cheri_tcpdump_control.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int temp_control_file;
static char *control_file;

static void
cleanup(void)
{

	if (temp_control_file && control_file != NULL)
		unlink(control_file);
}

static void
usage(void)
{

	fprintf(stderr,
"usage:	cheri_tcpdump_control [<options>]\n"
"\n"
"options:\n"
"	-c			colorize flows\n"
"	-C			do not colorize flows\n"
"	-f <file>		config file\n"
"	-l <seconds>		max sandbox lifetime\n"
"	-m <mode>		none, one, local, ip-hash\n"
"	-p <packets>		max packets per sandbox\n"
"	-s <sandboxes>		number of sandboxes (mode dependent)\n"
);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int cfd, opt;
	struct cheri_tcpdump_control ctdc;

	if (argc == 1)
		usage();

	memset(&ctdc, 0, sizeof(struct cheri_tcpdump_control));
	ctdc.ctdc_sb_mode = CTDC_MODE_HASH_TCP;
	ctdc.ctdc_colorize = 1;
	ctdc.ctdc_sandboxes = 3;

	while ((opt = getopt(argc, argv, "cCf:l:m:p:s:")) != -1) {
		switch (opt) {
		case 'c':
			ctdc.ctdc_colorize = 1;
			break;
		case 'C':
			ctdc.ctdc_colorize = 0;
			break;
		case 'f':
			control_file = optarg;
			break;
		case 'l':
			if (!isdigit(*optarg)) {
				warnx("invalid lifetime '%s'", optarg);
				usage();
			}
			ctdc.ctdc_sb_max_lifetime = atoi(optarg);
			break;
		case 'm':
			if (strcmp(optarg, "none") == 0)
				ctdc.ctdc_sb_mode = CTDC_MODE_NONE;
			else if (strcmp(optarg, "one") == 0)
				ctdc.ctdc_sb_mode = CTDC_MODE_ONE_SANDBOX;
			else if (strcmp(optarg, "local") == 0)
				ctdc.ctdc_sb_mode = CTDC_MODE_SEPARATE_LOCAL;
			else if (strcmp(optarg, "ip-hash") == 0)
				ctdc.ctdc_sb_mode = CTDC_MODE_HASH_TCP;
			else {
				warnx("invalid mode '%s'", optarg);
				usage();
			}
			break;
		case 'p':
			if (!isdigit(*optarg)) {
				warnx("invalid packet count '%s'", optarg);
				usage();
			}
			ctdc.ctdc_sb_max_packets = atoi(optarg);
			break;
		case 's':
			if (!isdigit(*optarg)) {
				warnx("invalid number of sandboxes '%s'",
				    optarg);
				usage();
			}
			ctdc.ctdc_sandboxes = atoi(optarg);
			break;
		default:
			warnx("unknown option %c", opt);
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if (atexit(cleanup) == -1)
		err(1, "atexit(cleanup)");

	if (control_file != NULL) {
		cfd = open(control_file, O_WRONLY | O_CREAT,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		if (cfd == -1)
			err(1, "open(%s)", control_file);
	} else {
		control_file = strdup("/tmp/cheri_tcpdump_control.XXXXXX");
		if (control_file == NULL)
			err(1, "strdup");
		cfd = mkstemp(control_file);
		if (cfd == -1)
			err(1, "mkstemp");
		temp_control_file = 1;
	}
	if (write(cfd, &ctdc, sizeof(ctdc)) != sizeof(ctdc))
		err(1, "write() cheri_tcpdump_control");
	if (ftruncate(cfd, sizeof(ctdc)) == -1)
		err(1, "ftruncate()");
	close(cfd);
	if (temp_control_file) {
		printf("%s\n", control_file);
		temp_control_file = 0;
	}

	return (0);
}
