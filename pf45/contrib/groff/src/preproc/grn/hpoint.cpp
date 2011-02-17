/* Last non-groff version: hpoint.c  1.1  84/10/08 */

/*
 * This file contains routines for manipulating the point data structures
 * for the gremlin picture editor.
 */

#include <stdlib.h>
#include "gprint.h"


/*
 * Return pointer to empty point list.
 */
POINT *
PTInit()
{
  return ((POINT *) NULL);
}


/*
 * This routine creates a new point with coordinates x and y and links it
 * into the pointlist.
 */
POINT *
PTMakePoint(double x,
	    double y,
	    POINT **pplist)
{
  register POINT *pt;

  if (Nullpoint(pt = *pplist)) {	/* empty list */
    *pplist = (POINT *) malloc(sizeof(POINT));
    pt = *pplist;
  } else {
    while (!Nullpoint(pt->nextpt))
      pt = pt->nextpt;
    pt->nextpt = (POINT *) malloc(sizeof(POINT));
    pt = pt->nextpt;
  }

  pt->x = x;
  pt->y = y;
  pt->nextpt = PTInit();
  return (pt);
}				/* end PTMakePoint */

/* EOF */
