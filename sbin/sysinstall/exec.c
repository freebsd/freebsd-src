/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: exec.c,v 1.2 1994/10/21 05:36:42 phk Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <dialog.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "sysinstall.h"

int
exec(int magic, char *cmd, char *args, ...)
{
	int pid, w, status;
	char *argv[EXEC_MAXARG];
	int arg = 0;
	va_list ap;
	struct stat dummy;

	if (stat(cmd, &dummy) == -1) {
		sprintf(errmsg, "Executable %s does not exist\n", cmd);
		return(-1);
	}

	va_start(ap, args);
	argv[arg++] = (char *)args;
	do {
		if (arg >= EXEC_MAXARG) 
			Fatal("Too many arguments");
	} while ((argv[arg++] = va_arg(ap, char *)));
	va_end(ap);

	if ((pid = fork()) == 0) {
		switch (magic) {
		case 0:
			close(0); open("/dev/null",O_RDONLY);
			close(1); open("/dev/null",O_WRONLY);
			close(2); open("/dev/null",O_WRONLY);
			break;
		case 1:
			close(2); open("/dev/null",O_WRONLY);
			break;
		case 2:
		default:
			break;
		}
		execv(cmd, argv);
		exit(1);
	}
	
	while ((w = wait(&status)) != pid && w != -1)
		;

	if (w == -1)
		Fatal("Child process %s terminated abnormally\n", cmd);
	return(status);
}
