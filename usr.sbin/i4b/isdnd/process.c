/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - process handling routines
 *	--------------------------------------
 *
 *	$Id: process.c,v 1.8 1999/12/13 21:25:25 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnd/process.c,v 1.6 1999/12/14 21:07:31 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:48:19 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

/*---------------------------------------------------------------------------*
 *	check if another instance of us is already running
 *---------------------------------------------------------------------------*/
void
check_pid(void)
{
	FILE *fp;
	
	/* check if another lock-file already exists */

	if((fp = fopen(PIDFILE, "r")) != NULL)
	{
		/* lockfile found, check */
		
		int oldpid;

		/* read pid from file */
		
		if((fscanf(fp, "%d", &oldpid)) != 1)
		{
			log(LL_ERR, "ERROR, reading pid from lockfile failed, terminating!");
			exit(1);
		}

		/* check if process got from file is still alive */
		
		if((kill(oldpid, 0)) != 0)
		{
			/* process does not exist */

			/* close file */

			fclose(fp);

			DBGL(DL_PROC, (log(LL_DBG, "removing old lock-file %s", PIDFILE)));

			/* remove file */
			
			unlink(PIDFILE);
		}
		else
		{
			/* process is still alive */
			
			log(LL_ERR, "ERROR, another daemon is already running, pid = %d, terminating!", oldpid);
			exit(1);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	establish and init process lock file
 *---------------------------------------------------------------------------*/
void
write_pid(void)
{
	FILE *fp;
	
	/* write my pid into lock-file */
	
	if((fp = fopen(PIDFILE, "w")) == NULL)
	{
		log(LL_ERR, "ERROR, can't open lockfile for writing, terminating");
		do_exit(1);
	}

	if((fprintf(fp, "%d", (int)getpid())) == EOF)
	{
		log(LL_ERR, "ERROR, can't write pid to lockfile, terminating");
		do_exit(1);
	}

	fsync(fileno(fp));

	fclose(fp);
}

/*---------------------------------------------------------------------------*
 *	become a daemon
 *---------------------------------------------------------------------------*/
void
daemonize(void)
{
	int fd;

	switch (fork())
	{
		case -1:		/* error */
			log(LL_ERR, "ERROR, daemonize/fork: %s", strerror(errno));
			exit(1);
		case 0:			/* child */
			break;
		default:		/* parent */
			exit(0);
	}

	/* new session / no control tty */

	if(setsid() == -1)
	{
		log(LL_ERR, "ERROR, setsid returns: %s", strerror(errno));
		exit(1);
	}

	/* go away from mounted dir */
	
	chdir("/");

	/* move i/o to another device ? */
	
	if(do_fullscreen && do_rdev)
	{
		char *tp;
		
		if((fd = open(rdev, O_RDWR, 0)) != -1)
		{
			if(!isatty(fd))
			{
				log(LL_ERR, "ERROR, device %s is not a tty!", rdev);
				exit(1);
			}
			if((dup2(fd, STDIN_FILENO)) == -1)
			{
				log(LL_ERR, "ERROR, dup2 stdin: %s", strerror(errno));
				exit(1);
			}				
			if((dup2(fd, STDOUT_FILENO)) == -1)
			{
				log(LL_ERR, "ERROR, dup2 stdout: %s", strerror(errno));
				exit(1);
			}				
			if((dup2(fd, STDERR_FILENO)) == -1)
			{
				log(LL_ERR, "ERROR, dup2 stderr: %s", strerror(errno));
				exit(1);
			}				
		}
		else
		{
			log(LL_ERR, "ERROR, cannot open redirected device: %s", strerror(errno));
			exit(1);
		}
			
		if(fd > 2)
		{
			if((close(fd)) == -1)
			{
				log(LL_ERR, "ERROR, close in daemonize: %s", strerror(errno));
				exit(1);
			}				
		}

		/* curses output && fork NEEDS controlling tty */
		
		if((ioctl(STDIN_FILENO, TIOCSCTTY, (char *)NULL)) < 0)
		{
			log(LL_ERR, "ERROR, cannot setup tty as controlling terminal: %s", strerror(errno));
			exit(1);
		}

		/* in case there is no environment ... */

		if(((tp = getenv("TERM")) == NULL) || (*tp == '\0'))
		{
			if(do_ttytype == 0)
			{
				log(LL_ERR, "ERROR, no environment variable TERM found and -t not specified!");
				exit(1);
			}

			if((setenv("TERM", ttype, 1)) != 0)
			{
				log(LL_ERR, "ERROR, setenv TERM=%s failed: %s", ttype, strerror(errno));
				exit(1);
			}
		}
	}
}

/* EOF */
