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
 *
 *	$Id$
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "xtend.h"
#include "xten.h"
#include "paths.h"

/*
 * Initialize the status table from the status files
 */

initstatus()
{
  int h, i;

  if(lseek(status, 0, SEEK_SET) != 0) {
    fprintf(Log, "%s:  Seek error on status file\n", thedate());
    return;
  }
  if(read(status, Status, 16*16*sizeof(STATUS)) != 16*16*sizeof(STATUS)) {
        fprintf(Log, "%s:  Read error on status file\n", thedate());
	return;
  }
}

/*
 * Checkpoint status of any devices whose status has changed
 * and notify anyone monitoring those devices.
 */

checkpoint_status()
{
  int h, i, k, offset;

  offset = 0;
  for(h = 0; h < 16; h++) {
    for(i = 0; i < 16; i++) {
      if(Status[h][i].changed) {
	if(lseek(status, offset, SEEK_SET) != offset) {
	  fprintf(Log, "%s:  Seek error on status file\n", thedate());
	} else {
	  if(write(status, &Status[h][i], sizeof(STATUS)) != sizeof(STATUS)) {
	    fprintf(Log, "%s:  Write error on status file\n", thedate());
	  }
	}
	Status[h][i].changed = 0;
	for(k = 0; k < MAXMON; k++) {
	  if(Monitor[k].inuse
	     && Monitor[k].house == h && Monitor[k].unit == i) {
	    /*
	     * Arrange to catch SIGPIPE in case client has gone away.
	     */
	    extern int client;
	    extern void clientgone();
	    void (*prev)();

	    client = k;
	    prev = signal(SIGPIPE, clientgone);
	    printstatus(Monitor[k].user, &Status[h][i]);
	    fflush(Monitor[k].user);
	    signal(SIGPIPE, prev);
	  }
	}
      }
      offset += sizeof(STATUS);
    }
  }
}

int client;

void clientgone()
{
    fprintf(Log, "%s:  Deleting monitor table entry %d, client gone\n", thedate(), client);
    fclose(Monitor[client].user);
    Monitor[client].inuse = 0;
}
