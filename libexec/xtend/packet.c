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
 *	$FreeBSD$
 */

#include <stdio.h>
#include <sys/time.h>
#include "xtend.h"
#include "xten.h"

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

/*
 * Log a packet and update device status accordingly
 */

logpacket(p)
unsigned char *p;
{
  fprintf(Log, "%s:  %s %s ", thedate(),
	  X10housenames[p[1]], X10cmdnames[p[2]]);
  if(p[0] & TW_RCV_LOCAL) fprintf(Log, "(loc,");
  else fprintf(Log, "(rem,");
  if(p[0] & TW_RCV_ERROR) fprintf(Log, "err)");
  else fprintf(Log, " ok)");
  fprintf(Log, "\n");
}

/*
 * Process a received packet p, updating device status information both
 * in core and on disk.
 */

processpacket(p)
unsigned char *p;
{
  int i, j, h, k;
  STATUS *s;

  /*
   * If the packet had the error flag set, there is no other useful info.
   */
  if(p[0] & TW_RCV_ERROR) return;
  /*
   * First update in-core status information for the device.
   */
  h = p[1]; k = p[2];
  if(k < 16) {	/* We received a unit code, to select a particular device */
    s = &Status[h][k];
    s->selected = SELECTED;
    s->lastchange = time(NULL);
    s->changed = 1;
  } else {  /* We received a key code, to execute some function */
    /*
     * Change in status depends on the key code received
     */
    if(k == DIM) {
      /*
       * We can't really track DIM/BRIGHT properly the way things are right
       * now.  The TW523 reports the first, fourth, seventh, etc. Dim packet.
       * We don't really have any way to tell when gaps occur, to cancel
       * selection.  For now, we'll assume that successive sequences of
       * Dim/Bright commands are never transmitted without some other
       * intervening command, and we make a good guess about how many units of
       * dim/bright are represented by each packet actually reported by the
       * TW523.
       */
      for(i = 0; i < 16; i++) {
	s = &Status[h][i];
	switch(s->selected) {
	case SELECTED:  /* Selected, but not being dimmed or brightened */
	  if(s->onoff == 0) {
	    s->onoff = 1;
	    s->brightness = 15;
	  }
	  s->brightness -= 2;
	  if(s->brightness < 0) s->brightness = 0;
	  s->selected = DIMMING;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	case DIMMING:  /* Selected and being dimmed */
	  s->brightness -=3;
	  if(s->brightness < 0) s->brightness = 0;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	case BRIGHTENING:  /* Selected and being brightened (an error) */
	  s->selected = IDLE;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	default:
	  break;
	}
      }
    } else if(k == BRIGHT) {
      /*
       * Same problem here...
       */
      for(i = 0; i < 16; i++) {
	s = &Status[h][i];
	switch(s->selected) {
	case SELECTED:  /* Selected, but not being dimmed or brightened */
	  if(s->onoff == 0) {
	    s->onoff = 1;
	    s->brightness = 15;
	  }
	  s->brightness += 2;
	  if(s->brightness > 15) s->brightness = 15;
	  s->selected = BRIGHTENING;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	case DIMMING:  /* Selected and being dimmed (an error) */
	  s->selected = IDLE;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	case BRIGHTENING:  /* Selected and being brightened */
	  s->brightness +=3;
	  if(s->brightness > 15) s->brightness = 15;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	  break;
	default:
	  break;
	}
      }
    } else {  /* Other key codes besides Bright and Dim */
      /*
       * We cancel brightening and dimming on ALL units on ALL house codes,
       * because the arrival of a different packet indicates a gap that
       * terminates any prior sequence of brightening and dimming
       */
      for(j = 0; j < 16; j++) {
	for(i = 0; i < 16; i++) {
	  s = &Status[j][i];
	  if(s->selected == BRIGHTENING || s->selected == DIMMING) {
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
      }
      switch(k) {
      case ALLUNITSOFF:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  s->onoff = 0;
	  s->selected = IDLE;
	  s->brightness = 0;
	  s->lastchange = time(NULL);
	  s->changed = 1;
	}
	break;
      case ALLLIGHTSON:
	/* Does AllLightsOn cancel selectedness of non-lights? */
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->devcap & ISLIGHT) {
	    s->onoff = 1;
	    s->selected = IDLE;
	    s->brightness = 15;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case UNITON:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == SELECTED) {
	    s->onoff = 1;
	    s->selected = IDLE;
	    s->brightness = 15;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case UNITOFF:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == SELECTED) {
	    s->onoff = 0;
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case ALLLIGHTSOFF:
	/* Does AllLightsOff cancel selectedness of non-lights? */
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->devcap & ISLIGHT) {
	    s->onoff = 0;
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case EXTENDEDCODE:
	break;
      case HAILREQUEST:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == SELECTED) {
	    s->selected = HAILED;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case HAILACKNOWLEDGE:
	/* Do these commands cancel selection of devices not affected? */
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == HAILED) {
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case PRESETDIM0:
      case PRESETDIM1:
	/* I don't really understand these */
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == SELECTED) {
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case EXTENDEDDATA:
	/* Who knows?  The TW523 can't receive these anyway. */
	break;
      case STATUSON:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected == REQUESTED) {
	    s->onoff = 1;
	    s->selected = IDLE;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      case STATUSOFF:
	for(i = 0; i < 16; i++) {
	  if(s->selected == REQUESTED) {
	    s = &Status[h][i];
	    s->onoff = 0;
	    s->selected = IDLE;
	    s->brightness = 0;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
      case STATUSREQUEST:
	for(i = 0; i < 16; i++) {
	  s = &Status[h][i];
	  if(s->selected) {
	    s->selected = REQUESTED;
	    s->lastchange = time(NULL);
	    s->changed = 1;
	  }
	}
	break;
      }
    }
  }
}
