/*
 * This file is a (rather silly) demonstration of the use of the
 * C Dynamic Object library.  It is a also reasonably thorough test
 * of the library (except that it only tests it with one data size).
 *
 * There are no restrictions on this code; however, if you make any
 * changes, I request that you document them so that I do not get
 * credit or blame for your modifications.
 *
 * Written by Barr3y Jaspan, Student Information Processing Board (SIPB)
 * and MIT-Project Athena, 1989.
 *
 * 2002-07-17 Moved here from util/dyn/test.c.  Older changes described
 *            at end of file; for newer changes see ChangeLog.
 */

#include <stdio.h>
#include <string.h>
#ifdef USE_DBMALLOC
#include <sys/stdtypes.h>
#include <malloc.h>
#endif
#include <stdlib.h>

#include "dyn.h"

static char random_string[] = "This is a random string.";
static char insert1[] = "This will be put at the beginning.";
static char insert2[] = "(parenthetical remark!) ";
static char insert3[] = "  This follows the random string.";

int
main(argc, argv)
/*@unused@*/int argc;
/*@unused@*/char **argv;
{
     /*@-exitarg@*/
     DynObject	obj;
     int	i, s;
     char	d, *data;

#ifdef _DEBUG_MALLOC_INC
     union dbmalloptarg arg;
     unsigned long hist1, hist2, o_size, c_size;
#endif

#ifdef _DEBUG_MALLOC_INC
     arg.i = 0;
     dbmallopt(MALLOC_ZERO, &arg);
     dbmallopt(MALLOC_REUSE, &arg);

     o_size = malloc_inuse(&hist1);
#endif

     /*@+matchanyintegral@*/
     obj = DynCreate(sizeof(char), -8);
     if (! obj) {
	  fprintf(stderr, "test: create failed.\n");
	  exit(1);
     }

     if(DynDebug(obj, 1) != DYN_OK) {
	  fprintf(stderr, "test: setting paranoid failed.\n");
	  exit(1);
     }
     if(DynParanoid(obj, 1) != DYN_OK) {
	  fprintf(stderr, "test: setting paranoid failed.\n");
	  exit(1);
     }


     if ((DynGet(obj, -5) != NULL) ||
	 (DynGet(obj, 0) != NULL) || (DynGet(obj, 1000) != NULL)) {
	  fprintf(stderr, "test: Get did not fail when it should have.\n");
	  exit(1);
     }

     if (DynDelete(obj, -1) != DYN_BADINDEX ||
	 DynDelete(obj, 0) != DYN_BADINDEX ||
	 DynDelete(obj, 100) != DYN_BADINDEX) {
	  fprintf(stderr, "test: Delete did not fail when it should have.\n");
	  exit(1);
     }

     printf("Size of empty object: %d\n", DynSize(obj));

     for (i=0; i<14; i++) {
	  d = (char) i;
	  if (DynAdd(obj, &d) != DYN_OK) {
	       fprintf(stderr, "test: Adding %d failed.\n", i);
	       exit(1);
	  }
     }

     if (DynAppend(obj, random_string, strlen(random_string)+1) != DYN_OK) {
	  fprintf(stderr, "test: appending array failed.\n");
	  exit(1);
     }

     if (DynDelete(obj, DynHigh(obj) / 2) != DYN_OK) {
	  fprintf(stderr, "test: deleting element failed.\n");
	  exit(1);
     }

     if (DynDelete(obj, DynHigh(obj) * 2) == DYN_OK) {
	  fprintf(stderr, "test: delete should have failed here.\n");
	  exit(1);
     }

     d = '\200';
     if (DynAdd(obj, &d) != DYN_OK) {
	  fprintf(stderr, "test: Adding %d failed.\n", i);
	  exit(1);
     }

     data = (char *) DynGet(obj, 0);
     if(data == NULL) {
	 fprintf(stderr, "test: getting object 0 failed.\n");
	 exit(1);
     }
     s = DynSize(obj);
     for (i=0; i < s; i++)
	  printf("Element %d is %d.\n", i, (int) data[i]);

     data = (char *) DynGet(obj, 13);
     if(data == NULL) {
	 fprintf(stderr, "test: getting element 13 failed.\n");
	 exit(1);
     }
     printf("Element 13 is %d.\n", (int) *data);

     data = (char *) DynGet(obj, DynSize(obj));
     if (data) {
	  fprintf(stderr, "DynGet did not return NULL when it should have.\n");
	  exit(1);
     }

     data = DynGet(obj, 14);
     if(data == NULL) {
	 fprintf(stderr, "test: getting element 13 failed.\n");
	 exit(1);
     }
     printf("This should be the random string: \"%s\"\n", data);

     if (DynInsert(obj, -1, "foo", 4) != DYN_BADINDEX ||
	 DynInsert(obj, DynSize(obj) + 1, "foo", 4) != DYN_BADINDEX ||
	 DynInsert(obj, 0, "foo", -1) != DYN_BADVALUE) {
	  fprintf(stderr, "DynInsert did not fail when it should have.\n");
	  exit(1);
     }

     if (DynInsert(obj, DynSize(obj) - 2, insert3, strlen(insert3) +
		   1) != DYN_OK) {
	  fprintf(stderr, "DynInsert to end failed.\n");
	  exit(1);
     }

     if (DynInsert(obj, 19, insert2, strlen(insert2)) != DYN_OK) {
	  fprintf(stderr, "DynInsert to middle failed.\n");
	  exit(1);
     }

     if (DynInsert(obj, 0, insert1, strlen(insert1)+1) != DYN_OK) {
	  fprintf(stderr, "DynInsert to start failed.\n");
	  exit(1);
     }

     data = DynGet(obj, 14 + strlen(insert1) + 1);
     if (data == NULL) {
	  fprintf(stderr, "DynGet of 14+strelen(insert1) failed.\n");
	  exit(1);

     }
     printf("A new random string: \"%s\"\n", data);

     data = DynGet(obj, 0);
     if (data == NULL) {
	  fprintf(stderr, "DynGet of 0 failed.\n");
	  exit(1);

     }
     printf("This was put at the beginning: \"%s\"\n", data);

     if(DynDestroy(obj) != DYN_OK) {
	  fprintf(stderr, "test: destroy failed.\n");
	  exit(1);
     }

#ifdef _DEBUG_MALLOC_INC
     c_size = malloc_inuse(&hist2);
     if (o_size != c_size) {
	  printf("\n\nIgnore a single unfreed malloc segment "
		 "(stdout buffer).\n\n");
	  malloc_list(2, hist1, hist2);
     }
#endif

     printf("All tests pass\n");

     return 0;
}

/* Old change log, as it relates to source code; build system stuff
   discarded.

2001-04-25  Ezra Peisach  <epeisach@mit.edu>

	* test.c: Always include stdlib.h

	* test.c: Check the return values of all library calls.

2000-11-09  Ezra Peisach  <epeisach@mit.edu>

	* test.c: Include string,h, stdlib.h.

*/
