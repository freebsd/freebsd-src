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
 * $Id: sig.c,v 1.2 1997/02/22 16:10:51 peter Exp $
 *
 *  TODO:
 *
 */

#include "sig.h"
#include <sys/types.h>
#include "mbuf.h"
#include "log.h"

#define __MAXSIG (32)		/* Sizeof u_long: Make life convenient.... */
static u_long caused;				/* A mask of pending signals */
static sig_type handler[ __MAXSIG ];	/* all start at SIG_DFL */


/* Record a signal in the "caused" mask */

static void signal_recorder(int sig) {
    if (sig > 0 && sig <= __MAXSIG)
        caused |= (1<<(sig-1));
}


/*
    set up signal_recorder, and record handler as the function to ultimately
    call in handle_signal()
*/

sig_type pending_signal(int sig,sig_type fn) {
    sig_type Result;

    if (sig <= 0 || sig > __MAXSIG) {
	/* Oops - we must be a bit out of date (too many sigs ?) */
        logprintf("Eeek! %s:%s: I must be out of date!\n",__FILE__,__LINE__);
        return signal(sig,fn);
    }

    Result = handler[sig-1];
    if (fn == SIG_DFL || fn == SIG_IGN) {
        handler[sig-1] = (sig_type)0;
        signal(sig,fn);
    } else {
        handler[sig-1] = fn;
        signal(sig,signal_recorder);
    }
    caused &= ~(1<<(sig-1));
    return Result;
}


/* Call the handlers for any pending signals */

void handle_signals() {
    int sig;

    if (caused)
       for (sig=0; sig<__MAXSIG; sig++, caused>>=1)
           if (caused&1)
               (*handler[sig])(sig+1);
}
