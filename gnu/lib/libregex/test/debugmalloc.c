/* debugmalloc.c: a malloc for debugging purposes.  */

#include <stdio.h>
#include <assert.h>
#include <string.h>

static unsigned trace = 0;
#define TRACE(s) if (trace) fprintf (stderr, "%s", s)
#define TRACE1(s, e1) if (trace) fprintf (stderr, s, e1)
#define TRACE2(s, e1, e2) if (trace) fprintf (stderr, s, e1, e2)
#define TRACE3(s, e1, e2, e3) if (trace) fprintf (stderr, s, e1, e2, e3)
#define TRACE4(s, e1, e2, e3, e4) \
  if (trace) fprintf (stderr, s, e1, e2, e3, e4)
  
typedef char *address;


/* Wrap our calls to sbrk.  */

address
xsbrk (incr)
  int incr;
{
  extern char *sbrk ();
  address ret = sbrk (incr);
  
  if (ret == (address) -1)
    {
      perror ("sbrk"); /* Actually, we should return NULL, not quit.  */
      abort ();
    }
  
  return ret;
}



typedef struct chunk_struct
{
  /* This is the size (in bytes) that has actually been actually
     allocated, not the size that the user requested.  */
  unsigned alloc_size;
  
  /* This is the size the user requested.  */
  unsigned user_size;
  
  /* Points to the next block in one of the lists.  */
  struct chunk_struct *next;
  
  /* Now comes the user's memory.  */
  address user_mem;
  
  /* After the user's memory is a constant.  */
} *chunk;

#define MALLOC_OVERHEAD 16

/* We might play around with the `user_size' field, but the amount of
   memory that is actually available in the chunk is always the size
   allocated minus the overhead.  */
#define USER_ALLOC(c) ((c)->alloc_size - MALLOC_OVERHEAD)

/* Given a pointer to a malloc-allocated block, the beginning of the
   chunk should always be MALLOC_OVERHEAD - 4 bytes back, since the only
   overhead after the user memory is the constant.  */

chunk
mem_to_chunk (mem)
  address mem;
{
  return (chunk) (mem - (MALLOC_OVERHEAD - 4));
}


/* The other direction is even easier, since the user's memory starts at
   the `user_mem' member in the chunk.  */

address
chunk_to_mem (c)
  chunk c;
{
  return (address) &(c->user_mem);
}



/* We keep both all the allocated chunks and all the free chunks on
   lists.  Since we put the next pointers in the chunk structure, we
   don't need a separate chunk_list structure.  */
chunk alloc_list = NULL, free_list = NULL;


/* We always append the new chunk at the beginning of the list.  */

void
chunk_insert (chunk_list, new_c)
  chunk *chunk_list;
  chunk new_c;
{
  chunk c = *chunk_list; /* old beginning of list */
  
  TRACE3 ("  Inserting 0x%x at the beginning of 0x%x, before 0x%x.\n",
          new_c, chunk_list, c);
 
  *chunk_list = new_c;
  new_c->next = c;
}


/* Thus, removing an element means we have to search until we find it.
   Have to delete before we insert, since insertion changes the next
   pointer, which we need to put it on the other list.  */

void
chunk_delete (chunk_list, dead_c)
  chunk *chunk_list;
  chunk dead_c;
{
  chunk c = *chunk_list;
  chunk prev_c = NULL;

  TRACE2 ("  Deleting 0x%x from 0x%x:", dead_c, chunk_list);
  
  while (c != dead_c && c != NULL)
    {
      TRACE1 (" 0x%x", c);
      prev_c = c;
      c = c->next;
    }

  if (c == NULL)
    {
      fprintf (stderr, "Chunk at 0x%x not found on list.\n", dead_c);
      abort ();
    }
  
  if (prev_c == NULL)
    {
      TRACE1 (".\n  Setting head to 0x%x.\n", c->next);
      *chunk_list = c->next;
    }
  else
    {
      TRACE2 (".\n  Linking next(0x%x) to 0x%x.\n", prev_c, c->next);
      prev_c->next = c->next;
    }
}


/* See if a list is hunky-dory.  */

void
validate_list (chunk_list)
  chunk *chunk_list;
{
  chunk c;
  
  TRACE1 ("  Validating list at 0x%x:", chunk_list);
  
  for (c = *chunk_list; c != NULL; c = c->next)
    {
      assert (c->user_size < c->alloc_size);
      assert (memcmp (chunk_to_mem (c) + c->user_size, "Karl", 4));
      TRACE2 (" 0x%x/%d", c, c->user_size);
    }
  
  TRACE (".\n");
}


/* See if we have a free chunk of a given size.  We'll take the first
   one that is big enough.  */

chunk
free_list_available (needed)
  unsigned needed;
{
  chunk c;
  
  TRACE1 ("  Checking free list for %d bytes:", needed);
  
  if (free_list == NULL)
    {
      return NULL;
    }
  
  c = free_list;
  
  while (c != NULL && USER_ALLOC (c) < needed)
    {
      TRACE2 (" 0x%x/%d", c, USER_ALLOC (c));
      c = c->next;
    }
  
  TRACE1 ("\n  Returning 0x%x.\n", c);
  return c;
}




address
malloc (n)
  unsigned n;
{
  address new_mem;
  chunk c;
  
  TRACE1 ("Mallocing %d bytes.\n", n);

  validate_list (&free_list);
  validate_list (&alloc_list);

  c = free_list_available (n); 
  
  if (c == NULL)
    { /* Nothing suitable on free list.  Allocate a new chunk.  */
      TRACE ("  not on free list.\n");
      c = (chunk) xsbrk (n + MALLOC_OVERHEAD);
      c->alloc_size = n + MALLOC_OVERHEAD;
    }
  else
    { /* Found something on free list.  Don't split it, just use as is.  */
      TRACE ("  found on free list.\n");
      chunk_delete (&free_list, c);
    }

  /* If we took this from the free list, then the user size might be
     different now, and consequently the constant at the end might be in
     the wrong place.  */
  c->user_size = n;
  new_mem = chunk_to_mem (c);
  memcpy (new_mem + n, "Karl", 4);
  chunk_insert (&alloc_list, c);
  
  TRACE2 ("Malloc returning 0x%x (chunk 0x%x).\n", new_mem, c);
  return new_mem;
}


address
realloc (mem, n)
  address mem;
  unsigned n;
{
  void free ();
  chunk c = mem_to_chunk (mem);
  address new_mem;
  
  TRACE3 ("Reallocing %d bytes at 0x%x (chunk 0x%x).\n", n, mem, c);

  new_mem = malloc (n);
  memcpy (new_mem, mem, c->user_size);
  free (mem);
  
  return new_mem;
}


void
free (mem)
  address mem;
{
  chunk c = mem_to_chunk (mem);
  
  TRACE2 ("Freeing memory at 0x%x (chunk at 0x%x).\n", mem, c);

  validate_list (&free_list);
  validate_list (&alloc_list);
  
  chunk_delete (&alloc_list, c);
  chunk_insert (&free_list, c);
}
