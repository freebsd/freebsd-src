/*
 * $Id: sig.c,v 1.9 1997/10/26 01:03:42 brian Exp $
 */

#include <sys/types.h>

#include <signal.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "sig.h"

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
