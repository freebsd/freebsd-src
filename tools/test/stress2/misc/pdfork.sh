#!/bin/sh

# truss / pdfork regression test.
# Test scenario by: Ryan Stone rstone@, slightly mangled by pho@

# Interruptable hang seen:
# $ ps -lp992
#  UID PID PPID CPU PRI NI  VSZ  RSS MWCHAN STAT TT     TIME COMMAND
# 1001 992  991   0  27  0 4168 1908 -      TX+   0  0:00.00 /tmp/pdfork -p
# $

cat > /tmp/pdfork.c <<EOF
#include <sys/types.h>
#include <sys/procdesc.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	pid_t pid;
	int fd;

	if (argc > 1 && strcmp(argv[1], "-p") == 0) {
		pid = pdfork(&fd, 0);
	} else {
		pid = fork();
	}

	if (pid == 0) {
		sleep(1);
		exit(0);
	} else if (pid < 0) {
		err(1, "fork() failed");
	} else {
		int status = 0;
		if (argc > 1 && strcmp(argv[1], "-p") != 0) {
			int error = wait4(pid, &status, WEXITED, NULL);
			if (error < 0)
				err(1, "wait4 failed");
		}
		exit(status);
	}
}
EOF
cc -o /tmp/pdfork -Wall -Wextra -O2 /tmp/pdfork.c || exit 1

timeout 20s truss -f /tmp/pdfork    2> /dev/null; s1=$?
timeout 20s truss -f /tmp/pdfork -p 2> /dev/null; s2=$?

rm -f /tmp/pdfork /tmp/pdfork.c
return $((s1 + s2))
