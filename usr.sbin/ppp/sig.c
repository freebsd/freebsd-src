/*-
 * Copyright (c) 1997
 *	Brian Somers <brian@awfulhak.demon.co.uk>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
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
 *
 * $Id: sig.c,v 1.7 1997/06/09 03:27:36 brian Exp $
 *
 *  TODO:
 *
 */

#include "sig.h"
#include <sys/types.h>
#include <signal.h>
#include "mbuf.h"
#include "log.h"

static caused[NSIG];		/* An array of pending signals */
static sig_type handler[NSIG];	/* all start at SIG_DFL */


/* Record a signal in the "caused" array */

static void
signal_recorder(int sig)
{
  caused[sig - 1]++;
}


/*
 * Set up signal_recorder, and record handler as the function to ultimately
 * call in handle_signal()
*/

sig_type
pending_signal(int sig, sig_type fn)
{
  sig_type Result;

  if (sig <= 0 || sig > NSIG) {
    /* Oops - we must be a bit out of date (too many sigs ?) */
    LogPrintf(LogALERT, "Eeek! %s:%s: I must be out of date!\n",
	      __FILE__, __LINE__);
    return signal(sig, fn);
  }
  Result = handler[sig - 1];
  if (fn == SIG_DFL || fn == SIG_IGN) {
    signal(sig, fn);
    handler[sig - 1] = (sig_type) 0;
  } else {
    handler[sig - 1] = fn;
    signal(sig, signal_recorder);
  }
  caused[sig - 1] = 0;
  return Result;
}


/* Call the handlers for any pending signals */

void
handle_signals()
{
  int sig;
  int got;

  do {
    got = 0;
    for (sig = 0; sig < NSIG; sig++)
      if (caused[sig]) {
	caused[sig]--;
	got++;
	(*handler[sig]) (sig + 1);
      }
  } while (got);
}
