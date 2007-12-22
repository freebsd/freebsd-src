/*
 * Copyright (c) 1999 Michael Reifenberger (Michael@Reifenberger.com). 
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
 *	i4btemux - record while playing
 *      ===============================
 *
 * $FreeBSD$
 *
 *----------------------------------------------------------------------------*/

#include<stdio.h>
#include<stdarg.h>
#include<signal.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<ctype.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/param.h>
#include<i4b/i4b_tel_ioctl.h>

// DECL DEFS
#define BUFLEN 2048
#define MAXBLOCKS_DEFAULT 23

// DECL VARS
int ibytes = 0;
int obytes = 0;
int maxbytes = (BUFLEN * MAXBLOCKS_DEFAULT);

int xfd = -1, xact = 0;
int ifd = -1, iact = 0;
int ofd = -1;
FILE *dfp = NULL;
int opt_dbg = 0;
int maxfd = 0;
fd_set set;
struct timeval timeout;
char nambuf[PATH_MAX];
int ch;

// DECL FUNC
void ifd_hdlr( void);
void xfd_hdlr( void);
void usage( void);
void dbg( char *fmt, ... );

// DEF FUNC
int main (int argc, char **argv) {
  int dummy;
  int x = -1;

  dfp = stderr;
  while( ( ch = getopt( argc, argv, "x:i:o:b:D:")) != -1 ){
    switch(ch){
      case 'b':
        x = atoi(optarg);
        maxbytes = x * BUFLEN;
        break;
      case 'i':
        ifd = open( optarg, O_RDONLY );
        iact = 1;
        break;
      case 'o':
        ofd = open( optarg, O_WRONLY|O_TRUNC|O_CREAT );
        break;
      case 'x':
        xfd = open( optarg, O_RDWR );
        xact = 1;
        break;
      case 'D':
        opt_dbg = 1;
        if( (dfp = fopen( optarg, "w" )) < 0) {
          dfp = stderr;
          dbg("Err for opening %s\n", optarg);
          exit(1);
        }
        break;
      case '?':
      default:
        usage();
        break;
    }
  }
  if( ( xfd < 0 ) || ( ifd < 0 ) || ( ofd < 0 ) ) {
    dbg("Err opening one ore more Files.\n");
    dbg("xfd: %d, ifd: %d, ofd: %d\n", xfd, ifd, ofd );
    usage();
  }

  if((x = ioctl(xfd, I4B_TEL_EMPTYINPUTQUEUE, &dummy)) < 0){
    dbg("Err I4B_TEL_EMPTYINPUTQUEUE\n");
  }

  while( (iact == 1) || ( (obytes < maxbytes) && (xact == 1) ) ){
    FD_ZERO( &set);
    if( iact == 1){
      FD_SET( ifd, &set);
      if( ifd > maxfd)
        maxfd = ifd; 
      dbg("FSET ifd\n");
    }
    if( xact == 1){
      FD_SET( xfd, &set);
      if( xfd > maxfd)
        maxfd = xfd; 
      dbg("FSET xfd\n");
    }
    x=select( maxfd+1, &set, NULL, NULL, NULL);
    if( x > 0){
      if( (iact == 1) && FD_ISSET( ifd, &set) ){
        ifd_hdlr();
      }
      if( (xact == 1) && FD_ISSET( xfd, &set) ){
        xfd_hdlr();
      }
    }
  }
  dbg("exit0\n");
  return(0);
}

void ifd_hdlr( void) {
  int x;
  unsigned char buf[BUFLEN];

  x = read( ifd, buf, BUFLEN); 
  dbg("ifd read %d bytes\n", x);
  if( x > 0 ){
    write( xfd, buf, x);
    ibytes += x;
    dbg("xfd %d bytes written to %d\n", x, ibytes);
  } else {
    iact = 0;
  }
}

void xfd_hdlr( void) {
  int x;
  unsigned char buf[BUFLEN];

  x = read( xfd, buf, BUFLEN);
  dbg("xfd read %d bytes\n", x);
  if( x > 0){
    write( ofd, buf, x);
    obytes += x;
    dbg("ofd %d bytes written to %d\n", x, obytes);
  } else {
    xact = 0;
  }
}

void usage( void) {
  fprintf(dfp, "isdntelmux V.1\n");
  fprintf(dfp, "usage: isdntelmux -x device -i ifile -o ofile [-b blocks]\n");
  exit(1);
}

void dbg( char *fmt, ... ) {
  va_list ap;

  if( opt_dbg == 0 )
    return;
  va_start( ap, fmt );
  vfprintf( dfp, fmt, ap);
  va_end(ap);
}
