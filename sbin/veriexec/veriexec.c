/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <paths.h>
#include <err.h>
#include <syslog.h>
#include <libsecureboot.h>
#include <libveriexec.h>
#include <sys/types.h>

#include "veriexec.h"

/* Globals that are shared with manifest_parser.c */
int dev_fd = -1;
int ForceFlags = 0;
int Verbose = 0;
int VeriexecVersion = 0;
const char *Cdir = NULL;

/*!
 * @brief Print help message describing program's usage
 * @param void
 * @return always returns code 0
 */
static int
veriexec_usage(void)
{
	printf("%s",
	    "Usage:\tveriexec [-C path] [-hlxv] [-[iz] state] [path]\n");

	return (0);
}

/*!
 * @brief Load a veriexec manifest
 * @param manifest Pointer to the location of the manifest file
 * @retval the error code returned from the parser
 */
static int
veriexec_load(const char *manifest)
{
	unsigned char *content;
	int rc;

	content = verify_signed(manifest, VEF_VERBOSE);
	if (!content)
		errx(EX_USAGE, "cannot verify %s", manifest);
	if (manifest_open(manifest, (const char *)content)) {
		rc = yyparse();
	} else {
		err(EX_NOINPUT, "cannot load %s", manifest);
	}
	free(content);
	return (rc);
}

/*!
 * @brief Get the veriexec state for the supplied argument
 * @param arg_text String containing the argument to be processed
 * @retval The veriexec state number for the specified argument
 */
static uint32_t
veriexec_state_query(const char *arg_text)
{
	uint32_t state = 0;
	unsigned long len;

	len = strlen(arg_text);

	if (strncmp(arg_text, "active", len) == 0)
		state |= VERIEXEC_STATE_ACTIVE;
	else if (strncmp(arg_text, "enforce", len) == 0)
		state |= VERIEXEC_STATE_ENFORCE;
	if (strncmp(arg_text, "loaded", len) == 0)
		state |= VERIEXEC_STATE_LOADED;
	if (strncmp(arg_text, "locked", len) == 0)
		state |= VERIEXEC_STATE_LOCKED;
	if (state == 0 || __bitcount(state) > 1)
		errx(EX_USAGE, "Unknown state \'%s\'", arg_text);

	return (state);
}

/*!
 * @brief Get the veriexec command state for the supplied argument
 * @param arg_text String containing the argument to be processed
 * @retval The veriexec command state for the specified argument
 */
static uint32_t
veriexec_state_modify(const char *arg_text)
{
	uint32_t state = 0;
	unsigned long len;

	len = strlen(arg_text);

	if (strncmp(arg_text, "active", len) == 0)
		state = VERIEXEC_ACTIVE;
	else if (strncmp(arg_text, "enforce", len) == 0)
		state = VERIEXEC_ENFORCE;
	else if (strncmp(arg_text, "getstate", len) == 0)
		state = VERIEXEC_GETSTATE;
	else if (strncmp(arg_text, "lock", len) == 0)
		state = VERIEXEC_LOCK;
	else
		errx(EX_USAGE, "Unknown command \'%s\'", arg_text);

	return (state);
}

#ifdef HAVE_VERIEXEC_GET_PATH_LABEL
static void
veriexec_check_labels(int argc, char *argv[])
{
	char buf[BUFSIZ];
	char *cp;
	int n;

	n = (argc - optind);
	for (; optind < argc; optind++) {
		cp = veriexec_get_path_label(argv[optind], buf, sizeof(buf));
		if (cp) {
			if (n > 1)
				printf("%s: %s\n", argv[optind], cp);
			else
				printf("%s\n", cp);
			if (cp != buf)
				free(cp);
		}
	}
	exit(EX_OK);
}
#endif

static void
veriexec_check_paths(int argc, char *argv[])
{
	int x;

	x = EX_OK;
	for (; optind < argc; optind++) {
		if (veriexec_check_path(argv[optind])) {
			warn("%s", argv[optind]);
			x = 2;
		}
	}
	exit(x);
}

int
main(int argc, char *argv[])
{
	long long converted_int;
	uint32_t state;
	int c, x;

	if (argc < 2)
		return (veriexec_usage());

	dev_fd = open(_PATH_DEV_VERIEXEC, O_WRONLY, 0);

	while ((c = getopt(argc, argv, "C:hi:lSxvz:")) != -1) {
		switch (c) {
		case 'h':
			/* Print usage info */

			return (veriexec_usage());
		case 'C':
			/* Get the provided directory argument */

			Cdir = optarg;
			break;
		case 'i':
			/* Query the current state */

			if (dev_fd < 0) {
				err(EX_UNAVAILABLE, "cannot open veriexec");
			}
			if (ioctl(dev_fd, VERIEXEC_GETSTATE, &x)) {
				err(EX_UNAVAILABLE,
				    "Cannot get veriexec state");
			}

			state = veriexec_state_query(optarg);

			exit((x & state) == 0);
			break;
#ifdef HAVE_VERIEXEC_GET_PATH_LABEL
		case 'l':
			veriexec_check_labels(argc, argv);
			break;
#endif
		case 'S':
			/* Strictly enforce certificate validity */
			ve_enforce_validity_set(1);
			break;
		case 'v':
			/* Increase the verbosity */

			Verbose++;
			break;
		case 'x':
			/* Check veriexec paths */

			/*
			 * -x says all other args are paths to check.
			 */
			veriexec_check_paths(argc, argv);
			break;
		case 'z':
			/* Modify the state */

			if (strncmp(optarg, "debug", strlen(optarg)) == 0) {
				const char *error;

				if (optind >= argc)
					errx(EX_USAGE,
					    "Missing mac_veriexec verbosity level \'N\', veriexec -z debug N, where N is \'off\' or the value 0 or greater");

				if (strncmp(argv[optind], "off", strlen(argv[optind])) == 0) {
					state = VERIEXEC_DEBUG_OFF;
					x = 0;
				} else {
					state = VERIEXEC_DEBUG_ON;

					converted_int = strtonum(argv[optind], 0, INT_MAX, &error);

					if (error != NULL)
						errx(EX_USAGE, "Conversion error for argument \'%s\' : %s",
						    argv[optind], error);

					x = (int) converted_int;


					if (x == 0)
						state = VERIEXEC_DEBUG_OFF;
				}
			} else
				state = veriexec_state_modify(optarg);

			if (dev_fd < 0)
				err(EX_UNAVAILABLE, "Cannot open veriexec");
			if (ioctl(dev_fd, state, &x))
				err(EX_UNAVAILABLE, "Cannot %s veriexec", optarg);

			if (state == VERIEXEC_DEBUG_ON || state == VERIEXEC_DEBUG_OFF)
				printf("mac_veriexec debug verbosity level: %d\n", x);
			else if (state == VERIEXEC_GETSTATE)
				printf("Veriexec state (octal) : %#o\n", x);

			exit(EX_OK);
			break;
		default:

			/* Missing argument, print usage info.*/
			veriexec_usage();
			exit(EX_USAGE);
			break;
		}
	}

	if (Verbose)
		printf("Verbosity level : %d\n", Verbose);

	if (dev_fd < 0)
		err(EX_UNAVAILABLE, "Cannot open veriexec");

	openlog(getprogname(), LOG_PID, LOG_AUTH);
	if (ve_trust_init() < 1)
		errx(EX_OSFILE, "cannot initialize trust store");
#ifdef VERIEXEC_GETVERSION
	if (ioctl(dev_fd, VERIEXEC_GETVERSION, &VeriexecVersion)) {
		VeriexecVersion = 0;	/* unknown */
	}
#endif

	for (; optind < argc; optind++) {
		if (veriexec_load(argv[optind])) {
			err(EX_DATAERR, "cannot load %s", argv[optind]);
		}
	}
	exit(EX_OK);
}
