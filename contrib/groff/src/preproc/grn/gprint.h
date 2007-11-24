/* Last non-groff version: gprint.h  1.1  84/10/08
 *
 * This file contains standard definitions used by the gprint program.
 */

#include <stdio.h>
#include <math.h>


#define xorn(x,y)	(x)
			/* was 512 */
#define yorn(x,y)	(511 - (y))	/* switch direction for */
					/* y-coordinates        */

#define STYLES 6
#define SIZES 4
#define FONTS 4
#define SOLID -1
#define DOTTED 004		/* 014 */
#define DASHED 020		/* 034 */
#define DOTDASHED 024		/* 054 */
#define LONGDASHED 074

#define DEFTHICK	-1	/* default thicknes */
#define DEFSTYLE	SOLID	/* default line style */

#define TRUE	1
#define FALSE	0

#define nullelt	-1
#define nullpt	-1
#define nullun	NULL

#define BOTLEFT	0
#define BOTRIGHT 1
#define CENTCENT 2
#define VECTOR 3
#define ARC 4
#define CURVE 5
#define POLYGON 6
#define BSPLINE 7
#define BEZIER 8
#define TOPLEFT 10
#define TOPCENT 11
#define TOPRIGHT 12
#define CENTLEFT 13
#define CENTRIGHT 14
#define BOTCENT 15
#define TEXT(t) ( (t <= CENTCENT) || (t >= TOPLEFT) )

/* WARNING * WARNING * WARNING * WARNING * WARNING * WARNING * WARNING 
 *    The above (TEXT) test is dependent on the relative values of the
 *    constants and will have to change if these values change or if new
 *    commands are added with value greater than BOTCENT
 */

#define NUSER 4
#define NFONTS 4
#define NBRUSHES 6
#define NSIZES 4
#define NJUSTS 9
#define NSTIPPLES 16

#define ADD 1
#define DELETE 2
#define MOD 3

typedef struct point {
  double x, y;
  struct point *nextpt;
} POINT;

typedef struct elmt {
  int type, brushf, size, textlength;
  char *textpt;
  POINT *ptlist;
  struct elmt *nextelt, *setnext;
} ELT;

#define DBNextElt(elt) (elt->nextelt)
#define DBNextofSet(elt) (elt->setnext)
#define DBNullelt(elt) (elt == NULL)
#define Nullpoint(pt)  ((pt) == (POINT *) NULL)
#define PTNextPoint(pt) (pt->nextpt)

/* EOF */
