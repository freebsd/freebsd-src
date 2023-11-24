/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2023, Juniper Networks, Inc.
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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/mac.h>

#include <unistd.h>
#include <fcntl.h>

#include <security/mac_grantbylabel/mac_grantbylabel.h>

/**
 * @brief does path have a gbl label
 *
 * @return
 * @li 0 if no/empty label or module not loaded
 * @li value of label
 */
unsigned int
gbl_check_path(const char *path)
{
	struct mac_grantbylabel_fetch_gbl_args gbl;
	int fd;
	int rc;

	rc = 0;
	if ((fd = open(path, O_RDONLY|O_VERIFY)) >= 0) {
		gbl.u.fd = fd;
		if (mac_syscall(MAC_GRANTBYLABEL_NAME,
			MAC_GRANTBYLABEL_FETCH_GBL,
			&gbl) == 0) {
			if (gbl.gbl != GBL_EMPTY)
				rc = gbl.gbl;
		}
		close(fd);
	}
	return(rc);
}

/**
 * @brief does pid have a gbl label
 *
 * @return
 * @li 0 if no/empty label or module not loaded
 * @li value of label
 */
unsigned int
gbl_check_pid(pid_t pid)
{
	struct mac_grantbylabel_fetch_gbl_args gbl;
	int rc;

	rc = 0;
	gbl.u.pid = pid;
	if (mac_syscall(MAC_GRANTBYLABEL_NAME,
		MAC_GRANTBYLABEL_FETCH_PID_GBL, &gbl) == 0) {
		if (gbl.gbl != GBL_EMPTY)
			rc = gbl.gbl;
	}
	return(rc);
}


#ifdef UNIT_TEST
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	pid_t pid;
	int pflag = 0;
	int c;
	unsigned int gbl;

	while ((c = getopt(argc, argv, "p")) != -1) {
		switch (c) {
		case 'p':
			pflag = 1;
			break;
		default:
			break;
		}
	}
	for (; optind < argc; optind++) {

		if (pflag) {
			pid = atoi(argv[optind]);
			gbl = gbl_check_pid(pid);
		} else {
			gbl = gbl_check_path(argv[optind]);
		}
		printf("arg=%s, gbl=%#o\n", argv[optind], gbl);
	}
	return 0;
}
#endif
