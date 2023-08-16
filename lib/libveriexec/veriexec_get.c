/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023, Juniper Networks, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/mac.h>

#include <unistd.h>
#include <string.h>

#include <security/mac_veriexec/mac_veriexec.h>

/**
 * @brief get veriexec params for a process
 *
 * @return
 * @li 0 if successful
 */
int
veriexec_get_pid_params(pid_t pid,
    struct mac_veriexec_syscall_params *params)
{
	struct mac_veriexec_syscall_params_args args;

	if (params == NULL)
		return EINVAL;

	args.u.pid = pid;
	args.params = params;
	return mac_syscall(MAC_VERIEXEC_NAME,
	    MAC_VERIEXEC_GET_PARAMS_PID_SYSCALL, &args);
}

/**
 * @brief get veriexec params for a process
 *
 * @return
 * @li 0 if successful
 */
int
veriexec_get_path_params(const char *file,
    struct mac_veriexec_syscall_params *params)
{
	struct mac_veriexec_syscall_params_args args;

	if (file == NULL || params == NULL)
		return EINVAL;

	args.u.filename = file;
	args.params = params;
	return mac_syscall(MAC_VERIEXEC_NAME,
	    MAC_VERIEXEC_GET_PARAMS_PATH_SYSCALL, &args);
}

/**
 * @brief check if a process has label that contains what we want
 *
 * @return
 * @li 0 if no
 * @li 1 if yes
 */
int
veriexec_check_pid_label(pid_t pid, const char *want)
{
	struct mac_veriexec_syscall_params params;
	char *cp;
	size_t n;

	if (want != NULL &&
	    veriexec_get_pid_params(pid, &params) == 0) {
		/* Does label contain [,]<want>[,] ? */
		if (params.labellen > 0 &&
		    (cp = strstr(params.label, want)) != NULL) {
			if (cp == params.label || cp[-1] == ',') {
				n = strlen(want);
				if (cp[n] == '\0' || cp[n] == ',')
					return 1; /* yes */
			}
		}
	}
	return 0;			/* no */
}

/**
 * @brief check if a path has label that contains what we want
 *
 * @return
 * @li 0 if no
 * @li 1 if yes
 */
int
veriexec_check_path_label(const char *file, const char *want)
{
	struct mac_veriexec_syscall_params params;
	char *cp;
	size_t n;

	if (want != NULL && file != NULL &&
	    veriexec_get_path_params(file, &params) == 0) {
		/* Does label contain [,]<want>[,] ? */
		if (params.labellen > 0 &&
		    (cp = strstr(params.label, want)) != NULL) {
			if (cp == params.label || cp[-1] == ',') {
				n = strlen(want);
				if (cp[n] == '\0' || cp[n] == ',')
					return 1; /* yes */
			}
		}
	}
	return 0;			/* no */
}

#ifdef UNIT_TEST
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static char *
hash2hex(char *type, unsigned char *digest)
{
	static char buf[2*MAXFINGERPRINTLEN+1];
	size_t n;
	int i;

	if (strcmp(type, "SHA1") == 0) {
		n = 20;
	} else if (strcmp(type, "SHA256") == 0) {
		n = 32;
	} else if (strcmp(type, "SHA384") == 0) {
		n = 48;
	}
	for (i = 0; i < n; i++) {
		sprintf(&buf[2*i], "%02x", (unsigned)digest[i]);
	}
	return buf;
}

int
main(int argc, char *argv[])
{
	struct mac_veriexec_syscall_params params;
	pid_t pid;
	char *want = NULL;
	int pflag = 0;
	int error;
	int c;

	while ((c = getopt(argc, argv, "pw:")) != -1) {
		switch (c) {
		case 'p':
			pflag = 1;
			break;
		case 'w':
			want = optarg;
			break;
		default:
			break;
		}
	}
	for (; optind < argc; optind++) {

		if (pflag) {
			pid = atoi(argv[optind]);
			if (want) {
				error = veriexec_check_pid_label(pid, want);
				printf("pid=%d want='%s': %d\n",
				    pid, want, error);
				continue;
			}
			error = veriexec_get_pid_params(pid, &params);
		} else {
			error = veriexec_get_path_params(argv[optind], &params);
		}
		if (error) {
			err(2, "%s, error=%d", argv[optind], error);
		}

		printf("arg=%s, type=%s, flags=%u, label='%s', fingerprint='%s'\n",
		    argv[optind], params.fp_type, (unsigned)params.flags,
		    params.label,
		    hash2hex(params.fp_type, params.fingerprint));
	}
	return 0;
}
#endif
