/*
 * Copyright (c) 1983 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)announce.c	5.9 (Berkeley) 2/26/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <protocols/talkd.h>
#include <sgtty.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>

extern char hostname[];

/*
 * Announce an invitation to talk.
 *
 * Because the tty driver insists on attaching a terminal-less
 * process to any terminal that it writes on, we must fork a child
 * to protect ourselves
 */
announce(request, remote_machine)
	CTL_MSG *request;
	char *remote_machine;
{
	int pid, val, status;

	if (pid = fork()) {
		/* we are the parent, so wait for the child */
		if (pid == -1)		/* the fork failed */
			return (FAILED);
		do {
			val = wait(&status);
			if (val == -1) {
				if (errno == EINTR)
					continue;
				/* shouldn't happen */
				syslog(LOG_WARNING, "announce: wait: %m");
				return (FAILED);
			}
		} while (val != pid);
		if (status&0377 > 0)	/* we were killed by some signal */
			return (FAILED);
		/* Get the second byte, this is the exit/return code */
		return ((status >> 8) & 0377);
	}
	/* we are the child, go and do it */
	_exit(announce_proc(request, remote_machine));
}
	
/*
 * See if the user is accepting messages. If so, announce that 
 * a talk is requested.
 */
announce_proc(request, remote_machine)
	CTL_MSG *request;
	char *remote_machine;
{
	int pid, status;
	char full_tty[32];
	FILE *tf;
	struct stat stbuf;

	(void)sprintf(full_tty, "%s/%s", _PATH_DEV, request->r_tty);
	if (access(full_tty, 0) != 0)
		return (FAILED);
	if ((tf = fopen(full_tty, "w")) == NULL)
		return (PERMISSION_DENIED);
	/*
	 * On first tty open, the server will have
	 * it's pgrp set, so disconnect us from the
	 * tty before we catch a signal.
	 */
	ioctl(fileno(tf), TIOCNOTTY, (struct sgttyb *) 0);
	if (fstat(fileno(tf), &stbuf) < 0)
		return (PERMISSION_DENIED);
	if ((stbuf.st_mode&020) == 0)
		return (PERMISSION_DENIED);
	print_mesg(tf, request, remote_machine);
	fclose(tf);
	return (SUCCESS);
}

#define max(a,b) ( (a) > (b) ? (a) : (b) )
#define N_LINES 5
#define N_CHARS 120

/*
 * Build a block of characters containing the message. 
 * It is sent blank filled and in a single block to
 * try to keep the message in one piece if the recipient
 * in in vi at the time
 */
print_mesg(tf, request, remote_machine)
	FILE *tf;
	CTL_MSG *request;
	char *remote_machine;
{
	struct timeval clock;
	struct timezone zone;
	struct tm *localtime();
	struct tm *localclock;
	char line_buf[N_LINES][N_CHARS];
	int sizes[N_LINES];
	char big_buf[N_LINES*N_CHARS];
	char *bptr, *lptr;
	int i, j, max_size;

	i = 0;
	max_size = 0;
	gettimeofday(&clock, &zone);
	localclock = localtime( &clock.tv_sec );
	(void)sprintf(line_buf[i], " ");
	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;
	(void)sprintf(line_buf[i], "Message from Talk_Daemon@%s at %d:%02d ...",
	hostname, localclock->tm_hour , localclock->tm_min );
	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;
	(void)sprintf(line_buf[i], "talk: connection requested by %s@%s.",
		request->l_name, remote_machine);
	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;
	(void)sprintf(line_buf[i], "talk: respond with:  talk %s@%s",
		request->l_name, remote_machine);
	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;
	(void)sprintf(line_buf[i], " ");
	sizes[i] = strlen(line_buf[i]);
	max_size = max(max_size, sizes[i]);
	i++;
	bptr = big_buf;
	*bptr++ = ''; /* send something to wake them up */
	*bptr++ = '\r';	/* add a \r in case of raw mode */
	*bptr++ = '\n';
	for (i = 0; i < N_LINES; i++) {
		/* copy the line into the big buffer */
		lptr = line_buf[i];
		while (*lptr != '\0')
			*(bptr++) = *(lptr++);
		/* pad out the rest of the lines with blanks */
		for (j = sizes[i]; j < max_size + 2; j++)
			*(bptr++) = ' ';
		*(bptr++) = '\r';	/* add a \r in case of raw mode */
		*(bptr++) = '\n';
	}
	*bptr = '\0';
	fprintf(tf, big_buf);
	fflush(tf);
	ioctl(fileno(tf), TIOCNOTTY, (struct sgttyb *) 0);
}
