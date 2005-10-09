/* Last non-groff version: hdb.c  1.8 (Berkeley) 84/10/20
 *
 * Copyright -C- 1982 Barry S. Roitblat
 *
 * This file contains database routines for the hard copy programs of the
 * gremlin picture editor.
 */

#include "gprint.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "errarg.h"
#include "error.h"

#define MAXSTRING 128
#define MAXSTRING_S "127"

/* imports from main.cpp */

extern int linenum;		/* current line number in input file */
extern char gremlinfile[];	/* name of file currently reading */
extern int SUNFILE;		/* TRUE if SUN gremlin file */
extern void savebounds(float x, float y);

/* imports from hpoint.cpp */

extern POINT *PTInit();
extern POINT *PTMakePoint(float x, float y, POINT ** pplist);


int DBGetType(register char *s);


/*
 * This routine returns a pointer to an initialized database element which
 * would be the only element in an empty list.
 */
ELT *
DBInit()
{
  return ((ELT *) NULL);
}				/* end DBInit */


/*
 * This routine creates a new element with the specified attributes and
 * links it into database.
 */
ELT *
DBCreateElt(int type,
	    POINT * pointlist,
	    int brush,
	    int size,
	    char *text,
	    ELT **db)
{
  register ELT *temp;

  temp = (ELT *) malloc(sizeof(ELT));
  temp->nextelt = *db;
  temp->type = type;
  temp->ptlist = pointlist;
  temp->brushf = brush;
  temp->size = size;
  temp->textpt = text;
  *db = temp;
  return (temp);
}				/* end CreateElt */


/*
 * This routine reads the specified file into a database and returns a
 * pointer to that database.
 */
ELT *
DBRead(register FILE *file)
{
  register int i;
  register int done;		/* flag for input exhausted */
  register float nx;		/* x holder so x is not set before orienting */
  int type;			/* element type */
  ELT *elist;			/* pointer to the file's elements */
  POINT *plist;			/* pointer for reading in points */
  char string[MAXSTRING], *txt;
  float x, y;			/* x and y are read in point coords */
  int len, brush, size;
  int lastpoint;

  SUNFILE = FALSE;
  elist = DBInit();
  (void) fscanf(file, "%" MAXSTRING_S "s%*[^\n]\n", string);
  if (strcmp(string, "gremlinfile")) {
    if (strcmp(string, "sungremlinfile")) {
      error("`%1' is not a gremlin file", gremlinfile);
      return (elist);
    }
    SUNFILE = TRUE;
  }

  (void) fscanf(file, "%d%f%f\n", &size, &x, &y);
  /* ignore orientation and file positioning point */

  done = FALSE;
  while (!done) {
    /* if (fscanf(file,"%" MAXSTRING_S "s\n", string) == EOF) */
    /* I changed the scanf format because the element */
    /* can have two words (e.g. CURVE SPLINE)         */
    if (fscanf(file, "\n%" MAXSTRING_S "[^\n]%*[^\n]\n", string) == EOF) {
      error("`%1', error in file format", gremlinfile);
      return (elist);
    }

    type = DBGetType(string);	/* interpret element type */
    if (type < 0) {		/* no more data */
      done = TRUE;
      (void) fclose(file);
    } else {
#ifdef UW_FASTSCAN
      (void) xscanf(file, &x, &y);		/* always one point */
#else
      (void) fscanf(file, "%f%f\n", &x, &y);	/* always one point */
#endif	/* UW_FASTSCAN */
      plist = PTInit();		/* NULL point list */

      /*
       * Files created on the SUN have point lists terminated by a line
       * containing only an asterik ('*').  Files created on the AED have
       * point lists terminated by the coordinate pair (-1.00 -1.00).
       */
      if (TEXT(type)) {		/* read only first point for TEXT elements */
	nx = xorn(x, y);
	y = yorn(x, y);
	(void) PTMakePoint(nx, y, &plist);
	savebounds(nx, y);

#ifdef UW_FASTSCAN
	while (xscanf(file, &x, &y));
#else
	lastpoint = FALSE;
	do {
	  fgets(string, MAXSTRING, file);
	  if (string[0] == '*') {	/* SUN gremlin file */
	    lastpoint = TRUE;
	  } else {
	    (void) sscanf(string, "%f%f", &x, &y);
	    if ((x == -1.00 && y == -1.00) && (!SUNFILE))
	      lastpoint = TRUE;
	  }
	} while (!lastpoint);
#endif	/* UW_FASTSCAN */
      } else {			/* not TEXT element */
#ifdef UW_FASTSCAN
	do {
	  nx = xorn(x, y);
	  y = yorn(x, y);
	  (void) PTMakePoint(nx, y, &plist);
	  savebounds(nx, y);
	} while (xscanf(file, &x, &y));
#else
	lastpoint = FALSE;
	while (!lastpoint) {
	  nx = xorn(x, y);
	  y = yorn(x, y);
	  (void) PTMakePoint(nx, y, &plist);
	  savebounds(nx, y);

	  fgets(string, MAXSTRING, file);
	  if (string[0] == '*') {	/* SUN gremlin file */
	    lastpoint = TRUE;
	  } else {
	    (void) sscanf(string, "%f%f", &x, &y);
	    if ((x == -1.00 && y == -1.00) && (!SUNFILE))
	      lastpoint = TRUE;
	  }
	}
#endif	/* UW_FASTSCAN */
      }
      (void) fscanf(file, "%d%d\n", &brush, &size);
      (void) fscanf(file, "%d", &len);	/* text length */
      (void) getc(file);		/* eat blank */
      txt = (char *) malloc((unsigned) len + 1);
      for (i = 0; i < len; ++i) {	/* read text */
	txt[i] = getc(file);
      }
      txt[len] = '\0';
      (void) DBCreateElt(type, plist, brush, size, txt, &elist);
    }				/* end else */
  } /* end while not done */ ;
  return (elist);
}				/* end DBRead */


/*
 * Interpret element type in string s.
 * Old file format consisted of integer element types.
 * New file format has literal names for element types.
 */
int
DBGetType(register char *s)
{
  if (isdigit(s[0]) || (s[0] == '-'))	/* old element format or EOF */
    return (atoi(s));

  switch (s[0]) {
  case 'P':
    return (POLYGON);
  case 'V':
    return (VECTOR);
  case 'A':
    return (ARC);
  case 'C':
    if (s[1] == 'U') {
      if (s[5] == '\n')
	return (CURVE);
      switch (s[7]) {
      case 'S': 
	return(BSPLINE);
      case 'E':
	fprintf(stderr,
		"Warning: Bezier Curves will be printed as B-Splines\n");
	return(BSPLINE);
      default:
	return(CURVE);
      }
    }
    switch (s[4]) {
    case 'L':
      return (CENTLEFT);
    case 'C':
      return (CENTCENT);
    case 'R':
      return (CENTRIGHT);
    default:
      fatal("unknown element type");
    }
  case 'B':
    switch (s[3]) {
    case 'L':
      return (BOTLEFT);
    case 'C':
      return (BOTCENT);
    case 'R':
      return (BOTRIGHT);
    default:
      fatal("unknown element type");
    }
  case 'T':
    switch (s[3]) {
    case 'L':
      return (TOPLEFT);
    case 'C':
      return (TOPCENT);
    case 'R':
      return (TOPRIGHT);
    default:
      fatal("unknown element type");
    }
  default:
    fatal("unknown element type");
  }

  return 0;				/* never reached */
}

#ifdef UW_FASTSCAN
/*
 * Optimization hack added by solomon@crys.wisc.edu, 12/2/86.
 * A huge fraction of the time was spent reading floating point numbers from
 * the input file, but the numbers always have the format 'ddd.dd'.  Thus
 * the following special-purpose version of fscanf.
 *
 * xscanf(f,xp,yp) does roughly what fscanf(f,"%f%f",xp,yp) does except:
 *   -the next piece of input must be of the form
 *      <space>* <digit>*'.'<digit>* <space>* <digit>*'.'<digit>*
 *   -xscanf eats the character following the second number
 *   -xscanf returns 0 for "end-of-data" indication, 1 otherwise, where
 *    end-of-data is signalled by a '*' [in which case the rest of the
 *    line is gobbled], or by '-1.00 -1.00' [but only if !SUNFILE].
 */
int
xscanf(FILE *f,
       float *xp,
       float *yp)
{
  register int c, i, j, m, frac;
  int iscale = 1, jscale = 1;	/* x = i/scale, y=j/jscale */

  while ((c = getc(f)) == ' ');
  if (c == '*') {
    while ((c = getc(f)) != '\n');
    return 0;
  }
  i = m = frac = 0;
  while (isdigit(c) || c == '.' || c == '-') {
    if (c == '-') {
      m++;
      c = getc(f);
      continue;
    }
    if (c == '.')
      frac = 1;
    else {
      if (frac)
	iscale *= 10;
      i = 10 * i + c - '0';
    }
    c = getc(f);
  }
  if (m)
    i = -i;
  *xp = (double) i / (double) iscale;

  while ((c = getc(f)) == ' ');
  j = m = frac = 0;
  while (isdigit(c) || c == '.' || c == '-') {
    if (c == '-') {
      m++;
      c = getc(f);
      continue;
    }
    if (c == '.')
      frac = 1;
    else {
      if (frac)
	jscale *= 10;
      j = 10 * j + c - '0';
    }
    c = getc(f);
  }
  if (m)
    j = -j;
  *yp = (double) j / (double) jscale;
  return (SUNFILE || i != -iscale || j != -jscale);
}
#endif	/* UW_FASTSCAN */

/* EOF */
