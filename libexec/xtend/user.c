/*-
 * Copyright (c) 1992, 1993, 1995 Eugene W. Stark
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/time.h>
#include "xtend.h"
#include "xten.h"
#include "paths.h"

MONENTRY Monitor[MAXMON];

int find(char *, char *[]);
void printstatus(FILE *, STATUS *);

/*
 * Process a user command
 */

int
user_command(void)
{
  char h;
  char *m;
  int i, k, n, error;
  char cmd[512], dumppath[MAXPATHLEN], pkt[3];
  FILE *dumpf;

  error = 0;
  if(fgets(cmd, 512, User) != NULL) {
    m = cmd;
    while ( *m != '\0' ) {
	if(isupper(*m))
	    *m = tolower(*m);
	m++;
    }
    if(sscanf(cmd, "status %c %d", &h, &i) == 2
		&& h >= 'a' && h <= 'p' && i >= 1 && i <= 16) {
      h -= 'a';
      i--;
      printstatus(User, &Status[h][i]);
    } else if(sscanf(cmd, "send %c %s %d", &h, cmd, &n) == 3
	      && h >= 'a' && h <= 'p' && (i = find(cmd, X10cmdnames)) >= 0) {
      h -= 'a';
      pkt[0] = h;
      pkt[1] = i;
      pkt[2] = n;
      if(write(tw523, pkt, 3) != 3) {
	fprintf(Log, "%s:  Transmission error (packet [%s %s]:%d).\n",
		thedate(), X10housenames[h], X10cmdnames[i], n);
	error++;
      } else {
	fprintf(User, "OK\n");
      }
    } else if(!strcmp("dump\n", cmd)) {
      snprintf(dumppath, sizeof(dumppath), "%s/%s", X10DIR, X10DUMPNAME);
      if((dumpf = fopen(dumppath, "w")) != NULL) {
	for(h = 0; h < 16; h++) {
	  for(i = 0; i < 16; i++) {
	    if(Status[h][i].lastchange) {
	      fprintf(dumpf, "%s%d\t", X10housenames[h], i+1);
	      printstatus(dumpf, &Status[h][i]);
	    }
	  }
	}
	fclose(dumpf);
	fprintf(User, "OK\n");
      } else {
	error++;
      }
    } else if(sscanf(cmd, "monitor %c %d", &h, &i) == 2
	      && h >= 'a' && h <= 'p' && i >= 1 && i <= 16) {
      h -= 'a';
      i--;
      for(k = 0; k < MAXMON; k++) {
	if(!Monitor[k].inuse) break;
      }
      if(k == MAXMON) {
	error++;
      } else {
	Monitor[k].house = h;
	Monitor[k].unit = i;
	Monitor[k].user = User;
	Monitor[k].inuse = 1;
	fprintf(Log, "%s:  Adding %c %d to monitor list (entry %d)\n",
		thedate(), h+'A', i+1, k);
	fprintf(User, "OK\n");
	fflush(User);
	User = NULL;
	return(0);  /* We don't want caller to close stream */
      }
    } else if(!strcmp("done\n", cmd)) {
	fprintf(User, "OK\n");
	fflush(User);
	return(1);
    } else {
      if(feof(User)) {
	return(1);
      } else {
	error++;
      }
    }
  } else {
    error++;
  }
  if(error) {
    fprintf(User, "ERROR\n");
  }
  fflush(User);
  return(0);
}

int
find(char *s, char *tab[])
{
	int i;

	for(i = 0; tab[i] != NULL; i++) {
	  if(strcasecmp(s, tab[i]) == 0) return(i);
	}
	return(-1);
}

void
printstatus(FILE *f, STATUS *s)
{
  fprintf(f, "%s:%d", s->onoff ? "On" : "Off", s->brightness);
  switch(s->selected) {
  case IDLE:
    fprintf(f, " (normal) "); break;
  case SELECTED:
    fprintf(f, " (selected) "); break;
  case DIMMING:
    fprintf(f, " (dimming) "); break;
  case BRIGHTENING:
    fprintf(f, " (brightening) "); break;
  case REQUESTED:
    fprintf(f, " (requested) "); break;
  case HAILED:
    fprintf(f, " (hailed) "); break;
  default:
    fprintf(f, " (bogus) "); break;
  }
  fprintf(f, "%s", ctime(&s->lastchange));
}

