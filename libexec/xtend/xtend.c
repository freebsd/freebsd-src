/*-
 * Copyright (c) 1992, 1993 Eugene W. Stark
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
 *	This product includes software developed by Eugene W. Stark.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY EUGENE W. STARK (THE AUTHOR) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * xtend - X-10 daemon
 * Eugene W. Stark (stark@cs.sunysb.edu)
 * January 14, 1993
 */

#include <stdio.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "xtend.h"
#include "xten.h"
#include "paths.h"

FILE *Log;			/* Log file */
FILE *User;    			/* User connection */
STATUS Status[16][16];		/* Device status table */
int status;			/* Status file descriptor */
int tw523;			/* tw523 controller */
int sock;			/* socket for user */
jmp_buf mainloop;		/* longjmp point after SIGHUP */
void onhup();			/* SIGHUP handler */
void onterm();			/* SIGTERM handler */
void onpipe();			/* SIGPIPE handler */

main(argc, argv)
int argc;
char *argv[];
{
  char *twpath = TWPATH;
  char *sockpath = SOCKPATH;
  char logpath[MAXPATHLEN+1];
  char statpath[MAXPATHLEN+1];
  struct sockaddr_un sa;
  struct timeval tv;
  int user;
  int fd;
  FILE *pidf;

  /*
   * Open the log file before doing anything
   */
  strcpy(logpath, X10DIR);
  strcat(logpath, X10LOGNAME);
  if((Log = fopen(logpath, "a")) == NULL) {
    fprintf(stderr, "Can't open log file %s\n", logpath);
    exit(1);
  }

  /*
   * Next fork like a proper daemon
   */
  switch(fork()) {
  case -1:
    fprintf(Log, "%s:  %s unable to fork\n", thedate(), argv[0]);
    exit(1);
  case 0:
    break;
  default:
    exit(0);
  }
  fprintf(Log, "%s:  %s[%d] started\n", thedate(), argv[0], getpid());

  /*
   * Release the controlling terminal so we don't get any signals from it.
   */
  setpgrp(0, getpid());
  if ((fd = open("/dev/tty", 2)) >= 0)
    {
      ioctl(fd, TIOCNOTTY, (char*)0);
      close(fd);
    }

  /*
   * Get ahold of the TW523 device
   */
  if((tw523 = open(twpath, O_RDWR)) < 0) {
    fprintf(Log, "%s:  Can't open %s\n", thedate(), twpath);
    exit(1);
  }
  fprintf(Log, "%s:  %s successfully opened\n", thedate(), twpath);

  /*
   * Initialize the status table
   */
  strcpy(statpath, X10DIR);
  strcat(statpath, X10STATNAME);
  if((status = open(statpath, O_RDWR)) < 0) {
    if((status = open(statpath, O_RDWR | O_CREAT, 0666)) < 0) {
      fprintf(Log, "%s:  Can't open %s\n", thedate(), statpath);
      exit(1);
    }
    if(write(status, Status, 16 * 16 * sizeof(STATUS))
       != 16 * 16 * sizeof(STATUS)) {
      fprintf(Log, "%s:  Error initializing status file\n", thedate());
      exit(1);
    }
  }
  initstatus();

  /*
   * Put our pid in a file so we can be signalled by shell scripts
   */
  if((pidf = fopen(PIDPATH, "w")) == NULL) {
      fprintf(Log, "%s:  Error writing pid file: %s\n", thedate(), PIDPATH);
      exit(1);
  }
  fprintf(pidf, "%d\n", getpid());
  fclose(pidf);

  /*
   * Set up socket to accept user commands
   */
  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    fprintf(Log, "%s:  Can't create socket\n", thedate());
    exit(1);
  }
  strcpy(sa.sun_path, sockpath);
  sa.sun_family = AF_UNIX;
  unlink(sockpath);
  if(bind(sock, (struct sockaddr *)(&sa), strlen(sa.sun_path) + 2) < 0) {
    fprintf(Log, "%s:  Can't bind socket to %s\n", thedate(), sockpath);
    exit(1);
  }
  if(listen(sock, 5) < 0) {
    fprintf(Log, "%s:  Can't listen on socket\n", thedate());
    exit(1);
  }

  signal(SIGHUP, onhup);
  signal(SIGTERM, onterm);
  signal(SIGPIPE, onpipe);
  /*
   * Return here on SIGHUP after closing and reopening log file.
   * Also on SIGPIPE after closing user connection.
   */
  setjmp(mainloop);

  /*
   * Now start the main processing loop.
   */
  tv.tv_sec = 0;
  tv.tv_usec = 250000;
  while(1) {
    fd_set fs;
    unsigned char rpkt[3];
    int sel, h, k;
    STATUS *s;

    FD_ZERO(&fs);
    FD_SET(tw523, &fs);
    if(User != NULL) FD_SET(user, &fs);
    else FD_SET(sock, &fs);
    sel = select(FD_SETSIZE, &fs, 0, 0, &tv);
    if(sel == 0) {
      /*
       * Cancel brightening and dimming on ALL units on ALL house codes,
       * because the fact that we haven't gotten a packet for awhile means
       * that there was a gap in transmission.
       */
      for(h = 0; h < 16; h++) {
	for(k = 0; k < 16; k++) {
	  s = &Status[h][k];
	  if(s->selected == BRIGHTENING || s->selected == DIMMING) {
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
      }
      fflush(Log);
      checkpoint_status();
      /*
       * Now that we've done this stuff, we'll set the timeout a little
       * longer, so we don't keep looping too frequently.
       */
      tv.tv_sec = 60;
      tv.tv_usec = 0;
      continue;
    }
    /*
     * While there is stuff happening, we keep a short timeout, so we
     * don't get stuck for some unknown reason, and so we can keep the
     * brightening and dimming data up-to-date.
     */
    tv.tv_sec = 0;
    tv.tv_usec = 250000;
    if(FD_ISSET(tw523, &fs)) {  /* X10 data arriving from TW523 */
      if(read(tw523, rpkt, 3) < 3) {
	fprintf(Log, "%s:  Error reading from TW523\n", thedate());
      } else {
	logpacket(rpkt);
	processpacket(rpkt);
      }
    } else if(FD_ISSET(user, &fs)) {
      if(User != NULL) {
	if(user_command()) {
	  fprintf(Log, "%s:  Closing user connection\n", thedate());
	  fclose(User);
	  User = NULL;
	}
      } else {
	/* "Can't" happen */
      }
    } else if(FD_ISSET(sock, &fs)) {  /* Accept a connection */
      if (User == NULL) {
	int len = sizeof(struct sockaddr_un);
	if((user = accept(sock, (struct sockaddr *)(&sa), &len)) >= 0) {
	  fprintf(Log, "%s:  Accepting user connection\n", thedate());
	  if((User = fdopen(user, "w+")) == NULL) {
	    fprintf(Log, "%s:  Can't attach socket to stream\n", thedate());
	  }
	} else {
	  fprintf(Log, "%s:  Failure in attempt to accept connection\n", thedate());
	}
      } else {
	/* "Can't happen */
      }
    }
  }
  /* Not reached */
}

char *thedate()
{
  char *cp, *cp1;
  time_t tod;

  tod = time(NULL);
  cp = cp1 = ctime(&tod);
  while(*cp1 != '\n') cp1++;
  *cp1 = '\0';
  return(cp);
}

/*
 * When SIGHUP received, close and reopen the Log file
 */

void onhup()
{
  char logpath[MAXPATHLEN+1];

  fprintf(Log, "%s:  SIGHUP received, reopening Log\n", thedate());
  fclose(Log);
  strcpy(logpath, X10DIR);
  strcat(logpath, X10LOGNAME);
  if((Log = fopen(logpath, "a")) == NULL) {
    fprintf(stderr, "Can't open log file %s\n", logpath);
    exit(1);
  }
  longjmp(mainloop, 1);
  /* No return */
}

/*
 * When SIGTERM received, just exit normally
 */

void onterm()
{
  fprintf(Log, "%s:  SIGTERM received, shutting down\n", thedate());
  exit(0);
}

/*
 * When SIGPIPE received, reset user connection
 */

void onpipe()
{
  fprintf(Log, "%s:  SIGPIPE received, resetting user connection\n",
	  thedate());
  if(User != NULL) {
    fclose(User);
    User = NULL;
  }
  longjmp(mainloop, 1);
  /* No return */
}
