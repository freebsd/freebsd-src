/* Like upstream localtime.c, but also define tzsetwall compat symbol.
   This file is in the public domain, so clarified as of 1996-06-05 by
   Arthur David Olson.  */

#include "localtime.c"

void
freebsd13_tzsetwall(void)
{
  monotime_t now = get_monotonic_time();
  int err = lock();
  if (0 < err) {
    errno = err;
    return;
  }
  tzset_unlocked(!err, true, now);
  unlock(!err);
}
__sym_compat(tzsetwall, freebsd13_tzsetwall, FBSD_1.0);
__warn_references(tzsetwall,
    "warning: tzsetwall() is deprecated, use tzset() instead.");
