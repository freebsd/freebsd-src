/*
 * This file is the collected implementation of libdyn.a, the C
 * Dynamic Object library.  It contains everything.
 *
 * There are no restrictions on this code; however, if you make any
 * changes, I request that you document them so that I do not get
 * credit or blame for your modifications.
 *
 * Written by Barr3y Jaspan, Student Information Processing Board (SIPB)
 * and MIT-Project Athena, 1989.
 *
 * 2002-07-17 Collected full implementation into one source file for
 *            easy inclusion into the one library still dependent on
 *            libdyn.  Assume memmove.  Old ChangeLog appended.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dynP.h"


/* old dyn_append.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynAppend().
 */

/*
 * Made obsolete by DynInsert, now just a convenience function.
 */
int DynAppend(obj, els, num)
   DynObjectP obj;
   DynPtr els;
   int num;
{
     return DynInsert(obj, DynSize(obj), els, num);
}


/* old dyn_create.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the functions DynCreate() and
 * DynDestroy().
 */

#ifndef DEFAULT_INC
#define DEFAULT_INC	100
#endif

static int default_increment = DEFAULT_INC;

DynObjectP DynCreate(el_size, inc)
   int	el_size, inc;
{
     DynObjectP obj;

     obj = (DynObjectP) malloc(sizeof(DynObjectRecP));
     if (obj == NULL)
	  return NULL;

     obj->array = (DynPtr) malloc(1);
     if (obj->array == NULL) {
	 free(obj);
	 return NULL;
     }
     obj->array[0] = '\0';

     obj->el_size = el_size;
     obj->num_el = obj->size = 0;
     obj->debug = obj->paranoid = 0;
     obj->inc = (inc) ? inc : default_increment;
     obj->initzero = 0;

     return obj;
}

DynObjectP DynCopy(obj)
   DynObjectP obj;
{
     DynObjectP obj1;

     obj1 = (DynObjectP) malloc(sizeof(DynObjectRecP));
     if (obj1 == NULL)
	  return NULL;

     obj1->el_size = obj->el_size;
     obj1->num_el = obj->num_el;
     obj1->size = obj->size;
     obj1->inc = obj->inc;
     obj1->debug = obj->debug;
     obj1->paranoid = obj->paranoid;
     obj1->initzero = obj->initzero;
     obj1->array = (char *) malloc((size_t) (obj1->el_size * obj1->size));
     if (obj1->array == NULL) {
	  free(obj1);
	  return NULL;
     }
     memcpy(obj1->array, obj->array,
	    (size_t) (obj1->el_size * obj1->size));

     return obj1;
}

int DynDestroy(obj)
     /*@only@*/DynObjectP obj;
{
     if (obj->paranoid) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: destroy: zeroing %d bytes from %p.\n",
		       obj->el_size * obj->size, obj->array);
	  memset(obj->array, 0, (size_t) (obj->el_size * obj->size));
     }
     free(obj->array);
     free(obj);
     return DYN_OK;
}

int DynRelease(obj)
   DynObjectP obj;
{
     if (obj->debug)
	  fprintf(stderr, "dyn: release: freeing object structure.\n");
     free(obj);
     return DYN_OK;
}


/* old dyn_debug.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynDebug().
 */

int DynDebug(obj, state)
   DynObjectP obj;
   int state;
{
     obj->debug = state;

     fprintf(stderr, "dyn: debug: Debug state set to %d.\n", state);
     return DYN_OK;
}


/* old dyn_delete.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynDelete().
 */

/*
 * Checkers!  Get away from that "hard disk erase" button!
 *    (Stupid dog.  He almost did it to me again ...)
 */
int DynDelete(obj, idx)
   DynObjectP obj;
   int idx;
{
     if (idx < 0) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: delete: bad index %d\n", idx);
	  return DYN_BADINDEX;
     }

     if (idx >= obj->num_el) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: delete: Highest index is %d.\n",
		       obj->num_el);
	  return DYN_BADINDEX;
     }

     if (idx == obj->num_el-1) {
	  if (obj->paranoid) {
	       if (obj->debug)
		    fprintf(stderr, "dyn: delete: last element, zeroing.\n");
	       memset(obj->array + idx*obj->el_size, 0, (size_t) obj->el_size);
	  }
	  else {
	       if (obj->debug)
		    fprintf(stderr, "dyn: delete: last element, punting.\n");
	  }
     }
     else {
	  if (obj->debug)
	       fprintf(stderr,
		       "dyn: delete: copying %d bytes from %p + %d to + %d.\n",
		       obj->el_size*(obj->num_el - idx), obj->array,
		       (idx+1)*obj->el_size, idx*obj->el_size);

	  memmove(obj->array + idx*obj->el_size,
		  obj->array + (idx+1)*obj->el_size,
		  (size_t) obj->el_size*(obj->num_el - idx));
	  if (obj->paranoid) {
	       if (obj->debug)
		    fprintf(stderr,
			    "dyn: delete: zeroing %d bytes from %p + %d\n",
			    obj->el_size, obj->array,
			    obj->el_size*(obj->num_el - 1));
	       memset(obj->array + obj->el_size*(obj->num_el - 1), 0,
		     (size_t) obj->el_size);
	  }
     }

     --obj->num_el;

     if (obj->debug)
	  fprintf(stderr, "dyn: delete: done.\n");

     return DYN_OK;
}


/* old dyn_initzero.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynInitZero().
 */

int DynInitzero(obj, state)
   DynObjectP obj;
   int state;
{
     obj->initzero = state;

     if (obj->debug)
	  fprintf(stderr, "dyn: initzero: initzero set to %d.\n", state);
     return DYN_OK;
}


/* old dyn_insert.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynInsert().
 */

int DynInsert(obj, idx, els_in, num)
   DynObjectP obj;
   void *els_in;
   int idx, num;
{
     DynPtr els = (DynPtr) els_in;
     int ret;

     if (idx < 0 || idx > obj->num_el) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: insert: index %d is not in [0,%d]\n",
		       idx, obj->num_el);
	  return DYN_BADINDEX;
     }

     if (num < 1) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: insert: cannot insert %d elements\n",
		       num);
	  return DYN_BADVALUE;
     }

     if (obj->debug)
	  fprintf(stderr,"dyn: insert: Moving %d bytes from %p + %d to + %d\n",
		  (obj->num_el-idx)*obj->el_size, obj->array,
		  obj->el_size*idx, obj->el_size*(idx+num));

     if ((ret = _DynResize(obj, obj->num_el + num)) != DYN_OK)
	  return ret;
     memmove(obj->array + obj->el_size*(idx + num),
	     obj->array + obj->el_size*idx,
	     (size_t) ((obj->num_el-idx)*obj->el_size));

     if (obj->debug)
	  fprintf(stderr, "dyn: insert: Copying %d bytes from %p to %p + %d\n",
		  obj->el_size*num, els, obj->array, obj->el_size*idx);

     memmove(obj->array + obj->el_size*idx, els, (size_t) (obj->el_size*num));
     obj->num_el += num;

     if (obj->debug)
	  fprintf(stderr, "dyn: insert: done.\n");

     return DYN_OK;
}


/* old dyn_paranoid.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynDebug().
 */

int DynParanoid(obj, state)
   DynObjectP obj;
   int state;
{
     obj->paranoid = state;

     if (obj->debug)
	  fprintf(stderr, "dyn: paranoid: Paranoia set to %d.\n", state);
     return DYN_OK;
}


/* old dyn_put.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the functions DynGet() and DynAdd().
 */

DynPtr DynArray(obj)
   DynObjectP obj;
{
     if (obj->debug)
	  fprintf(stderr, "dyn: array: returning array pointer %p.\n",
		  obj->array);

     return obj->array;
}

DynPtr DynGet(obj, num)
   DynObjectP obj;
   int num;
{
     if (num < 0) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: get: bad index %d\n", num);
	  return NULL;
     }

     if (num >= obj->num_el) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: get: highest element is %d.\n",
		       obj->num_el);
	  return NULL;
     }

     if (obj->debug)
	  fprintf(stderr, "dyn: get: Returning address %p + %d.\n",
		  obj->array, obj->el_size*num);

     return (DynPtr) obj->array + obj->el_size*num;
}

int DynAdd(obj, el)
   DynObjectP obj;
   void *el;
{
     int	ret;

     ret = DynPut(obj, el, obj->num_el);
     if (ret != DYN_OK)
	  return ret;

     ++obj->num_el;
     return ret;
}

/*
 * WARNING!  There is a reason this function is not documented in the
 * man page.  If DynPut used to mutate already existing elements,
 * everything will go fine.  If it is used to add new elements
 * directly, however, the state within the object (such as
 * obj->num_el) will not be updated properly and many other functions
 * in the library will lose.  Have a nice day.
 */
int DynPut(obj, el_in, idx)
   DynObjectP obj;
   void *el_in;
   int idx;
{
     DynPtr el = (DynPtr) el_in;
     int ret;

     if (obj->debug)
	  fprintf(stderr, "dyn: put: Writing %d bytes from %p to %p + %d\n",
		  obj->el_size, el, obj->array, idx*obj->el_size);

     if ((ret = _DynResize(obj, idx)) != DYN_OK)
	  return ret;

     memmove(obj->array + idx*obj->el_size, el, (size_t) obj->el_size);

     if (obj->debug)
	  fprintf(stderr, "dyn: put: done.\n");

     return DYN_OK;
}


/* old dyn_realloc.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the internal function _DynRealloc().
 */

/*
 * Resize the array so that element req exists.
 */
int _DynResize(obj, req)
   DynObjectP obj;
   int req;
{
     int size;

     if (obj->size > req)
	  return DYN_OK;
     else if (obj->inc > 0)
	  return _DynRealloc(obj, (req - obj->size) / obj->inc + 1);
     else {
	  if (obj->size == 0)
	       size = -obj->inc;
	  else
	       size = obj->size;

	  /*@-shiftsigned@*/
	  while (size <= req)
	       size <<= 1;
	  /*@=shiftsigned@*/

	  return _DynRealloc(obj, size);
     }
}

/*
 * Resize the array by num_incs units.  If obj->inc is positive, this
 * means make it obj->inc*num_incs elements larger.  If obj->inc is
 * negative, this means make the array num_incs elements long.
 *
 * Ideally, this function should not be called from outside the
 * library.  However, nothing will break if it is.
 */
int _DynRealloc(obj, num_incs)
   DynObjectP obj;
   int num_incs;
{
     DynPtr temp;
     int new_size_in_bytes;

     if (obj->inc > 0)
	  new_size_in_bytes = obj->el_size*(obj->size + obj->inc*num_incs);
     else
	  new_size_in_bytes = obj->el_size*num_incs;

     if (obj->debug)
	  fprintf(stderr,
		  "dyn: alloc: Increasing object by %d bytes (%d incs).\n",
		  new_size_in_bytes - obj->el_size*obj->size,
		  num_incs);

     temp = (DynPtr) realloc(obj->array, (size_t) new_size_in_bytes);
     if (temp == NULL) {
	  if (obj->debug)
	       fprintf(stderr, "dyn: alloc: Out of memory.\n");
	  return DYN_NOMEM;
     }
     else {
	  obj->array = temp;
	  if (obj->inc > 0)
	       obj->size += obj->inc*num_incs;
	  else
	       obj->size = num_incs;
     }

     if (obj->debug)
	  fprintf(stderr, "dyn: alloc: done.\n");

     return DYN_OK;
}


/* old dyn_size.c */
/*
 * This file is part of libdyn.a, the C Dynamic Object library.  It
 * contains the source code for the function DynSize().
 */

int DynSize(obj)
   DynObjectP obj;
{
     if (obj->debug)
	  fprintf(stderr, "dyn: size: returning size %d.\n", obj->num_el);

     return obj->num_el;
}

int DynCapacity(obj)
   DynObjectP obj;
{
     if (obj->debug)
	  fprintf(stderr, "dyn: capacity: returning cap of %d.\n", obj->size);

     return obj->size;
}

/* Old change log, as it relates to source code; build system stuff
   discarded.

2001-10-09  Ken Raeburn  <raeburn@mit.edu>

	* dyn.h, dynP.h: Make prototypes unconditional.  Don't define
	P().

2001-04-25  Ezra Peisach  <epeisach@mit.edu>

	* dyn.h: Lclint annotate functions.

	* dyn_create.c (DynCreate): Do not assume that malloc(0) is valid
	and returns a valid pointer. Fix memory leak if malloc fails.

	* dyn_realloc.c (_DynResize): Turn off warning of shifting a
	signed variable.

Thu Nov  9 15:31:31 2000  Ezra Peisach  <epeisach@mit.edu>

	* dyn_create.c (DynCopy): Arguments to memcpy were reversed. Found
 	while playing with lclint.

2000-11-09  Ezra Peisach  <epeisach@mit.edu>

	* dyn_create.c, dyn_delete.c, dyn_insert.c, dyn_put.c,
	dyn_realloc.c: Cast arguments to malloc(), realloc(), memmove() to
	size_t.

	* dynP.h: Provide full prototypes for _DynRealloc() and _DynResize().

	* dyn.h: Add prototype for DynAppend.

2000-06-29  Ezra Peisach  <epeisach@mit.edu>

	* dyn_insert.c, dyn_put.c: Include string.h for memmove prototype.

2000-06-28  Ezra Peisach  <epeisach@mit.edu>

	* dyn_create.c, dyn_delete.c, dyn_insert.c, dyn_put.c: Use %p
	format for displaying pointers.

2000-06-26  Ezra Peisach  <epeisach@mit.edu>

	* dyn_realloc.c: Remove unused variable.

Sat Dec  6 22:50:03 1997  Ezra Peisach  <epeisach@mit.edu>

	* dyn_delete.c: Include <string.h>

Mon Jul 22 21:37:52 1996  Ezra Peisach  <epeisach@mit.edu>

	* dyn.h: If __STDC__ is not defined, generate prototypes implying
		functions and not variables.

Mon Jul 22 04:20:48 1996  Marc Horowitz  <marc@mit.edu>

	* dyn_insert.c (DynInsert): what used to be #ifdef POSIX, should
 	be #ifdef HAVE_MEMMOVE
*/
