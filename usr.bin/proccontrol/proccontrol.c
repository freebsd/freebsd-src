/*-
 * Copyright (c) 2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/procctl.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
	MODE_ASLR,
	MODE_INVALID,
	MODE_TRACE,
	MODE_TRAPCAP,
	MODE_PROTMAX,
	MODE_STACKGAP,
	MODE_NO_NEW_PRIVS,
	MODE_WXMAP,
#ifdef PROC_KPTI_CTL
	MODE_KPTI,
#endif
#ifdef PROC_LA_CTL
	MODE_LA57,
	MODE_LA48,
#endif
};

static pid_t
str2pid(const char *str)
{
	pid_t res;
	char *tail;

	res = strtol(str, &tail, 0);
	if (*tail != '\0') {
		warnx("non-numeric pid");
		return (-1);
	}
	return (res);
}

#ifdef PROC_KPTI_CTL
#define	KPTI_USAGE "|kpti"
#else
#define	KPTI_USAGE
#endif
#ifdef PROC_LA_CTL
#define	LA_USAGE "|la48|la57"
#else
#define	LA_USAGE
#endif

static void __dead2
usage(void)
{

	fprintf(stderr, "Usage: proccontrol -m (aslr|protmax|trace|trapcap|"
	    "stackgap|nonewprivs|wxmap"KPTI_USAGE LA_USAGE") [-q] "
	    "[-s (enable|disable)] [-p pid | command]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int arg, ch, error, mode;
	pid_t pid;
	bool enable, do_command, query;

	mode = MODE_INVALID;
	enable = true;
	pid = -1;
	query = false;
	while ((ch = getopt(argc, argv, "m:qs:p:")) != -1) {
		switch (ch) {
		case 'm':
			if (strcmp(optarg, "aslr") == 0)
				mode = MODE_ASLR;
			else if (strcmp(optarg, "protmax") == 0)
				mode = MODE_PROTMAX;
			else if (strcmp(optarg, "trace") == 0)
				mode = MODE_TRACE;
			else if (strcmp(optarg, "trapcap") == 0)
				mode = MODE_TRAPCAP;
			else if (strcmp(optarg, "stackgap") == 0)
				mode = MODE_STACKGAP;
			else if (strcmp(optarg, "nonewprivs") == 0)
				mode = MODE_NO_NEW_PRIVS;
			else if (strcmp(optarg, "wxmap") == 0)
				mode = MODE_WXMAP;
#ifdef PROC_KPTI_CTL
			else if (strcmp(optarg, "kpti") == 0)
				mode = MODE_KPTI;
#endif
#ifdef PROC_LA_CTL
			else if (strcmp(optarg, "la57") == 0)
				mode = MODE_LA57;
			else if (strcmp(optarg, "la48") == 0)
				mode = MODE_LA48;
#endif
			else
				usage();
			break;
		case 's':
			if (strcmp(optarg, "enable") == 0)
				enable = true;
			else if (strcmp(optarg, "disable") == 0)
				enable = false;
			else
				usage();
			break;
		case 'p':
			pid = str2pid(optarg);
			break;
		case 'q':
			query = true;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	do_command = argc != 0;
	if (do_command) {
		if (pid != -1 || query)
			usage();
		pid = getpid();
	} else if (pid == -1) {
		pid = getpid();
	}

	if (query) {
		switch (mode) {
		case MODE_ASLR:
			error = procctl(P_PID, pid, PROC_ASLR_STATUS, &arg);
			break;
		case MODE_TRACE:
			error = procctl(P_PID, pid, PROC_TRACE_STATUS, &arg);
			break;
		case MODE_TRAPCAP:
			error = procctl(P_PID, pid, PROC_TRAPCAP_STATUS, &arg);
			break;
		case MODE_PROTMAX:
			error = procctl(P_PID, pid, PROC_PROTMAX_STATUS, &arg);
			break;
		case MODE_STACKGAP:
			error = procctl(P_PID, pid, PROC_STACKGAP_STATUS, &arg);
			break;
		case MODE_NO_NEW_PRIVS:
			error = procctl(P_PID, pid, PROC_NO_NEW_PRIVS_STATUS,
			    &arg);
			break;
		case MODE_WXMAP:
			error = procctl(P_PID, pid, PROC_WXMAP_STATUS, &arg);
			break;
#ifdef PROC_KPTI_CTL
		case MODE_KPTI:
			error = procctl(P_PID, pid, PROC_KPTI_STATUS, &arg);
			break;
#endif
#ifdef PROC_LA_CTL
		case MODE_LA57:
		case MODE_LA48:
			error = procctl(P_PID, pid, PROC_LA_STATUS, &arg);
			break;
#endif
		default:
			usage();
			break;
		}
		if (error != 0)
			err(1, "procctl status");
		switch (mode) {
		case MODE_ASLR:
			switch (arg & ~PROC_ASLR_ACTIVE) {
			case PROC_ASLR_FORCE_ENABLE:
				printf("force enabled");
				break;
			case PROC_ASLR_FORCE_DISABLE:
				printf("force disabled");
				break;
			case PROC_ASLR_NOFORCE:
				printf("not forced");
				break;
			}
			if ((arg & PROC_ASLR_ACTIVE) != 0)
				printf(", active\n");
			else
				printf(", not active\n");
			break;
		case MODE_TRACE:
			if (arg == -1)
				printf("disabled\n");
			else if (arg == 0)
				printf("enabled, no debugger\n");
			else
				printf("enabled, traced by %d\n", arg);
			break;
		case MODE_TRAPCAP:
			switch (arg) {
			case PROC_TRAPCAP_CTL_ENABLE:
				printf("enabled\n");
				break;
			case PROC_TRAPCAP_CTL_DISABLE:
				printf("disabled\n");
				break;
			}
			break;
		case MODE_PROTMAX:
			switch (arg & ~PROC_PROTMAX_ACTIVE) {
			case PROC_PROTMAX_FORCE_ENABLE:
				printf("force enabled");
				break;
			case PROC_PROTMAX_FORCE_DISABLE:
				printf("force disabled");
				break;
			case PROC_PROTMAX_NOFORCE:
				printf("not forced");
				break;
			}
			if ((arg & PROC_PROTMAX_ACTIVE) != 0)
				printf(", active\n");
			else
				printf(", not active\n");
			break;
		case MODE_STACKGAP:
			switch (arg & (PROC_STACKGAP_ENABLE |
			    PROC_STACKGAP_DISABLE)) {
			case PROC_STACKGAP_ENABLE:
				printf("enabled\n");
				break;
			case PROC_STACKGAP_DISABLE:
				printf("disabled\n");
				break;
			}
			switch (arg & (PROC_STACKGAP_ENABLE_EXEC |
			    PROC_STACKGAP_DISABLE_EXEC)) {
			case PROC_STACKGAP_ENABLE_EXEC:
				printf("enabled after exec\n");
				break;
			case PROC_STACKGAP_DISABLE_EXEC:
				printf("disabled after exec\n");
				break;
			}
			break;
		case MODE_NO_NEW_PRIVS:
			switch (arg) {
			case PROC_NO_NEW_PRIVS_ENABLE:
				printf("enabled\n");
				break;
			case PROC_NO_NEW_PRIVS_DISABLE:
				printf("disabled\n");
				break;
			}
			break;
		case MODE_WXMAP:
			if ((arg & PROC_WX_MAPPINGS_PERMIT) != 0)
				printf("enabled");
			else
				printf("disabled");
			if ((arg & PROC_WX_MAPPINGS_DISALLOW_EXEC) != 0)
				printf(", disabled on exec");
			if ((arg & PROC_WXORX_ENFORCE) != 0)
				printf(", wxorx enforced");
			printf("\n");
			break;
#ifdef PROC_KPTI_CTL
		case MODE_KPTI:
			switch (arg & ~PROC_KPTI_STATUS_ACTIVE) {
			case PROC_KPTI_CTL_ENABLE_ON_EXEC:
				printf("enabled");
				break;
			case PROC_KPTI_CTL_DISABLE_ON_EXEC:
				printf("disabled");
				break;
			}
			if ((arg & PROC_KPTI_STATUS_ACTIVE) != 0)
				printf(", active\n");
			else
				printf(", not active\n");
			break;
#endif
#ifdef PROC_LA_CTL
		case MODE_LA57:
		case MODE_LA48:
			switch (arg & ~(PROC_LA_STATUS_LA48 |
			    PROC_LA_STATUS_LA57)) {
			case PROC_LA_CTL_LA48_ON_EXEC:
				printf("la48 on exec");
				break;
			case PROC_LA_CTL_LA57_ON_EXEC:
				printf("la57 on exec");
				break;
			case PROC_LA_CTL_DEFAULT_ON_EXEC:
				printf("default on exec");
				break;
			}
			if ((arg & PROC_LA_STATUS_LA48) != 0)
				printf(", la48 active\n");
			else if ((arg & PROC_LA_STATUS_LA57) != 0)
				printf(", la57 active\n");
			break;
#endif
		}
	} else {
		switch (mode) {
		case MODE_ASLR:
			arg = enable ? PROC_ASLR_FORCE_ENABLE :
			    PROC_ASLR_FORCE_DISABLE;
			error = procctl(P_PID, pid, PROC_ASLR_CTL, &arg);
			break;
		case MODE_TRACE:
			arg = enable ? PROC_TRACE_CTL_ENABLE :
			    PROC_TRACE_CTL_DISABLE;
			error = procctl(P_PID, pid, PROC_TRACE_CTL, &arg);
			break;
		case MODE_TRAPCAP:
			arg = enable ? PROC_TRAPCAP_CTL_ENABLE :
			    PROC_TRAPCAP_CTL_DISABLE;
			error = procctl(P_PID, pid, PROC_TRAPCAP_CTL, &arg);
			break;
		case MODE_PROTMAX:
			arg = enable ? PROC_PROTMAX_FORCE_ENABLE :
			    PROC_PROTMAX_FORCE_DISABLE;
			error = procctl(P_PID, pid, PROC_PROTMAX_CTL, &arg);
			break;
		case MODE_STACKGAP:
			arg = enable ? PROC_STACKGAP_ENABLE_EXEC :
			    (PROC_STACKGAP_DISABLE |
			    PROC_STACKGAP_DISABLE_EXEC);
			error = procctl(P_PID, pid, PROC_STACKGAP_CTL, &arg);
			break;
		case MODE_NO_NEW_PRIVS:
			arg = enable ? PROC_NO_NEW_PRIVS_ENABLE :
			    PROC_NO_NEW_PRIVS_DISABLE;
			error = procctl(P_PID, pid, PROC_NO_NEW_PRIVS_CTL,
			    &arg);
			break;
		case MODE_WXMAP:
			arg = enable ? PROC_WX_MAPPINGS_PERMIT :
			    PROC_WX_MAPPINGS_DISALLOW_EXEC;
			error = procctl(P_PID, pid, PROC_WXMAP_CTL, &arg);
			break;
#ifdef PROC_KPTI_CTL
		case MODE_KPTI:
			arg = enable ? PROC_KPTI_CTL_ENABLE_ON_EXEC :
			    PROC_KPTI_CTL_DISABLE_ON_EXEC;
			error = procctl(P_PID, pid, PROC_KPTI_CTL, &arg);
			break;
#endif
#ifdef PROC_LA_CTL
		case MODE_LA57:
			arg = enable ? PROC_LA_CTL_LA57_ON_EXEC :
			    PROC_LA_CTL_DEFAULT_ON_EXEC;
			error = procctl(P_PID, pid, PROC_LA_CTL, &arg);
			break;
		case MODE_LA48:
			arg = enable ? PROC_LA_CTL_LA48_ON_EXEC :
			    PROC_LA_CTL_DEFAULT_ON_EXEC;
			error = procctl(P_PID, pid, PROC_LA_CTL, &arg);
			break;
#endif
		default:
			usage();
			break;
		}
		if (error != 0)
			err(1, "procctl ctl");
		if (do_command) {
			error = execvp(argv[0], argv);
			err(1, "exec");
		}
	}
	exit(0);
}
