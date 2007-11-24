/* valloc -- return memory aligned to the page size.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "system.h"

#ifndef HAVE_GETPAGESIZE
# include "getpagesize.h"
#endif

void *
valloc (bytes)
     size_t bytes;
{
  long pagesize;
  char *ret;

  pagesize = getpagesize ();
  ret = (char *) malloc (bytes + pagesize - 1);
  if (ret)
    ret = (char *) ((long) (ret + pagesize - 1) &~ (pagesize - 1));
  return ret;
}
