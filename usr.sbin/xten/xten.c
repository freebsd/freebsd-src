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
 * Xten - user command interface to X-10 daemon
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "xtend.h"
#include "xten.h"
#include "paths.h"

#define RETRIES 10
#define CMDLEN 512

char *X10housenames[] = {
  "A", "B", "C", "D", "E", "F", "G", "H",
  "I", "J", "K", "L", "M", "N", "O", "P",
  NULL
};

char *X10cmdnames[] = {
  "1", "2", "3", "4", "5", "6", "7", "8",
  "9", "10", "11", "12", "13", "14", "15", "16",
  "AllUnitsOff", "AllLightsOn", "On", "Off", "Dim", "Bright", "AllLightsOff",
  "ExtendedCode", "HailRequest", "HailAcknowledge", "PreSetDim0", "PreSetDim1",
  "ExtendedData", "StatusOn", "StatusOff", "StatusRequest",
  NULL
};

main(argc, argv)
int argc;
char *argv[];
{
  int c, tmp, h, k, sock, error;
  FILE *daemon;
  struct sockaddr_un sa;
  char *sockpath = SOCKPATH;
  char reply[CMDLEN], cmd[CMDLEN], *cp;
  int interactive = 0;

  if(argc == 2 && !strcmp(argv[1], "-")) interactive++;
  else if(argc < 3) {
    fprintf(stderr, "Usage: %s house key[:cnt] [ [house] key[:cnt] ... ]\n", argv[0]);
    exit(1);
  }
  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "%s:  Can't create socket\n", argv[0]);
    exit(1);
  }
  strcpy(sa.sun_path, sockpath);
  sa.sun_family = AF_UNIX;
  if(connect(sock, (struct sockaddr *)(&sa), strlen(sa.sun_path) + 2) < 0) {
    fprintf(stderr, "%s:  Can't connect to X-10 daemon\n", argv[0]);
    exit(1);
  }
  if((daemon = fdopen(sock, "w+")) == NULL) {
    fprintf(stderr, "%s:  Can't attach stream to socket\n", argv[0]);
    exit(1);
  }
  /*
   * If interactive, copy standard input to daemon and report results
   * on standard output.
   */
  if(interactive) {
    while(!feof(stdin)) {
      if(fgets(cmd, CMDLEN, stdin) != NULL) {
	fprintf(daemon, "%s", cmd);
	fflush(daemon);
	if(fgets(reply, CMDLEN, daemon) != NULL) {
	  fprintf(stdout, "%s", reply);
	  fflush(stdout);
	}
      }
    }
    exit(0);
  }
  /*
   * Otherwise, interpret arguments and issue commands to daemon,
   * handling retries in case of errors.
   */
  if((h = find(argv[1], X10housenames)) < 0) {
    fprintf(stderr, "Invalid house code: %s\n", argv[1]);
    exit(1);
  }
  argv++;
  argv++;
  while(argc >= 3) {
    cp = argv[0];
    if((tmp = find(cp, X10housenames)) >= 0) {
      h = tmp;
      argv++;
      argc--;
      continue;
    }
    while(*cp != '\0' && *cp != ':') cp++;
    if(*cp == ':') c = atoi(cp+1);
    else c = 2;
    *cp = '\0';
    if((k = find(argv[0], X10cmdnames)) < 0) {
      fprintf(stderr, "Invalid key/unit code: %s\n", argv[0]);
      error++;
    }
    error = 0;
    while(error < RETRIES) {
      fprintf(daemon, "send %s %s %d\n", X10housenames[h], X10cmdnames[k], c);
      fflush(daemon);
      fgets(reply, CMDLEN, daemon);
      if(strncmp(reply, "ERROR", 5)) break;
      error++;
      usleep(200000);
    }
    if(error == RETRIES) {
      fprintf(stderr, "Command failed: send %s %s %d\n",
	      X10housenames[h], X10cmdnames[k], c);
    }
    argc--;
    argv++;
  }
  fprintf(daemon, "done\n");
  fgets(reply, CMDLEN, daemon);
  exit(0);
}

find(s, tab)
char *s;
char *tab[];
{
	int i;

	for(i = 0; tab[i] != NULL; i++) {
	  if(strcasecmp(s, tab[i]) == 0) return(i);
	}
	return(-1);
}
