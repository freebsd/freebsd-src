#include <stdio.h>
extern char *malloc ();

#ifndef NULL
#define NULL 0
#endif

void *
xmalloc (size)
  unsigned size;
{
  char *new_mem = malloc (size); 
  
  if (new_mem == NULL)
    {
      fprintf (stderr, "xmalloc: request for %u bytes failed.\n", size);
      abort ();
    }

  return new_mem;
}
