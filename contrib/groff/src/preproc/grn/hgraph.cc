/* Last non-groff version: hgraph.c  1.14 (Berkeley) 84/11/27
 *
 * This file contains the graphics routines for converting gremlin pictures
 * to troff input.
 */

#include "lib.h"

#include "gprint.h"

#ifdef NEED_DECLARATION_HYPOT
extern "C" {
  double hypot(double, double);
}
#endif /* NEED_DECLARATION_HYPOT */

#define MAXVECT	40
#define MAXPOINTS	200
#define LINELENGTH	1
#define PointsPerInterval 64
#define pi		3.14159265358979324
#define twopi		(2.0 * pi)
#define len(a, b)	hypot((double)(b.x-a.x), (double)(b.y-a.y))


extern int dotshifter;		/* for the length of dotted curves */

extern int style[];		/* line and character styles */
extern double thick[];
extern char *tfont[];
extern int tsize[];
extern int stipple_index[];	/* stipple font index for stipples 0 - 16 */
extern char *stipple;		/* stipple type (cf or ug) */


extern double troffscale;	/* imports from main.c */
extern double linethickness;
extern int linmod;
extern int lastx;
extern int lasty;
extern int lastyline;
extern int ytop;
extern int ybottom;
extern int xleft;
extern int xright;
extern enum {
  OUTLINE, FILL, BOTH
} polyfill;

extern double adj1;
extern double adj2;
extern double adj3;
extern double adj4;
extern int res;

void HGSetFont(int font, int size);
void HGPutText(int justify, POINT pnt, register char *string);
void HGSetBrush(int mode);
void tmove2(int px, int py);
void doarc(POINT cp, POINT sp, int angle);
void tmove(POINT * ptr);
void cr();
void drawwig(POINT * ptr, int type);
void HGtline(int x1, int y1);
void dx(double x);
void dy(double y);
void HGArc(register int cx, register int cy, int px, int py, int angle);
void picurve(register int *x, register int *y, int npts);
void HGCurve(int *x, int *y, int numpoints);
void Paramaterize(int x[], int y[], float h[], int n);
void PeriodicSpline(float h[], int z[],
		    float dz[], float d2z[], float d3z[],
		    int npoints);
void NaturalEndSpline(float h[], int z[],
		      float dz[], float d2z[], float d3z[],
		      int npoints);



/*----------------------------------------------------------------------------*
 | Routine:	HGPrintElt (element_pointer, baseline)
 |
 | Results:	Examines a picture element and calls the appropriate
 |		routine(s) to print them according to their type.  After the
 |		picture is drawn, current position is (lastx, lasty).
 *----------------------------------------------------------------------------*/

void
HGPrintElt(ELT *element,
	   int baseline)
{
  register POINT *p1;
  register POINT *p2;
  register int length;
  register int graylevel;

  if (!DBNullelt(element) && !Nullpoint((p1 = element->ptlist))) {
    /* p1 always has first point */
    if (TEXT(element->type)) {
      HGSetFont(element->brushf, element->size);
      switch (element->size) {
      case 1:
	p1->y += adj1;
	break;
      case 2:
	p1->y += adj2;
	break;
      case 3:
	p1->y += adj3;
	break;
      case 4:
	p1->y += adj4;
	break;
      default:
	break;
      }
      HGPutText(element->type, *p1, element->textpt);
    } else {
      if (element->brushf)		/* if there is a brush, the */
	HGSetBrush(element->brushf);	/* graphics need it set     */

      switch (element->type) {

      case ARC:
	p2 = PTNextPoint(p1);
	tmove(p2);
	doarc(*p1, *p2, element->size);
	cr();
	break;

      case CURVE:
	length = 0;	/* keep track of line length */
	drawwig(p1, CURVE);
	cr();
	break;

      case BSPLINE:
	length = 0;	/* keep track of line length */
	drawwig(p1, BSPLINE);
	cr();
	break;

      case VECTOR:
	length = 0;		/* keep track of line length so */
	tmove(p1);		/* single lines don't get long  */
	while (!Nullpoint((p1 = PTNextPoint(p1)))) {
	  HGtline((int) (p1->x * troffscale),
		  (int) (p1->y * troffscale));
	  if (length++ > LINELENGTH) {
	    length = 0;
	    printf("\\\n");
	  }
	}			/* end while */
	cr();
	break;

      case POLYGON:
	{
	  /* brushf = style of outline; size = color of fill:
	   * on first pass (polyfill=FILL), do the interior using 'P'
	   *    unless size=0
	   * on second pass (polyfill=OUTLINE), do the outline using a series
	   *    of vectors. It might make more sense to use \D'p ...',
	   *    but there is no uniform way to specify a 'fill character'
	   *    that prints as 'no fill' on all output devices (and
	   *    stipple fonts).
	   * If polyfill=BOTH, just use the \D'p ...' command.
	   */
	  float firstx = p1->x;
	  float firsty = p1->y;

	  length = 0;		/* keep track of line length so */
				/* single lines don't get long  */

	  if (polyfill == FILL || polyfill == BOTH) {
	    /* do the interior */
	    char command = (polyfill == BOTH && element->brushf) ? 'p' : 'P';

	    /* include outline, if there is one and */
	    /* the -p flag was set                  */

	    /* switch based on what gremlin gives */
	    switch (element->size) {
	    case 1:
	      graylevel = 1;
	      break;
	    case 3:
	      graylevel = 2;
	      break;
	    case 12:
	      graylevel = 3;
	      break;
	    case 14:
	      graylevel = 4;
	      break;
	    case 16:
	      graylevel = 5;
	      break;
	    case 19:
	      graylevel = 6;
	      break;
	    case 21:
	      graylevel = 7;
	      break;
	    case 23:
	      graylevel = 8;
	      break;
	    default:		/* who's giving something else? */
	      graylevel = NSTIPPLES;
	      break;
	    }
	    /* int graylevel = element->size; */

	    if (graylevel < 0)
	      break;
	    if (graylevel > NSTIPPLES)
	      graylevel = NSTIPPLES;
	    printf("\\h'-%du'\\D'f %du'",
		   stipple_index[graylevel],
		   stipple_index[graylevel]);
	    cr();
	    tmove(p1);
	    printf("\\D'%c", command);

	    while (!Nullpoint((PTNextPoint(p1)))) {
	      p1 = PTNextPoint(p1);
	      dx((double) p1->x);
	      dy((double) p1->y);
	      if (length++ > LINELENGTH) {
		length = 0;
		printf("\\\n");
	      }
	    } /* end while */

	    /* close polygon if not done so by user */
	    if ((firstx != p1->x) || (firsty != p1->y)) {
	      dx((double) firstx);
	      dy((double) firsty);
	    }
	    putchar('\'');
	    cr();
	    break;
	  }
	  /* else polyfill == OUTLINE; only draw the outline */
	  if (!(element->brushf))
	    break;
	  length = 0;		/* keep track of line length */
	  tmove(p1);

	  while (!Nullpoint((PTNextPoint(p1)))) {
	    p1 = PTNextPoint(p1);
	    HGtline((int) (p1->x * troffscale),
		    (int) (p1->y * troffscale));
	    if (length++ > LINELENGTH) {
	      length = 0;
	      printf("\\\n");
	    }
	  }			/* end while */

	  /* close polygon if not done so by user */
	  if ((firstx != p1->x) || (firsty != p1->y)) {
	    HGtline((int) (firstx * troffscale),
		    (int) (firsty * troffscale));
	  }
	  cr();
	  break;
	}			/* end case POLYGON */
      }				/* end switch */
    }				/* end else Text */
  }				/* end if */
}				/* end PrintElt */


/*----------------------------------------------------------------------------*
 | Routine:	HGPutText (justification, position_point, string)
 |
 | Results:	Given the justification, a point to position with, and a
 |		string to put, HGPutText first sends the string into a
 |		diversion, moves to the positioning point, then outputs
 |		local vertical and horizontal motions as needed to justify
 |		the text.  After all motions are done, the diversion is
 |		printed out.
 *----------------------------------------------------------------------------*/

void
HGPutText(int justify,
	  POINT pnt,
	  register char *string)
{
  int savelasty = lasty;	/* vertical motion for text is to be */
				/* ignored.  Save current y here     */

  printf(".nr g8 \\n(.d\n");	/* save current vertical position. */
  printf(".ds g9 \"");		/* define string containing the text. */
  while (*string) {		/* put out the string */
    if (*string == '\\' &&
	*(string + 1) == '\\') {	/* one character at a */
      printf("\\\\\\");			/* time replacing //  */
      string++;				/* by //// to prevent */
    }					/* interpretation at  */
    printf("%c", *(string++));		/* printout time      */
  }
  printf("\n");

  tmove(&pnt);			/* move to positioning point */

  switch (justify) {
    /* local vertical motions                                            */
    /* (the numbers here are used to be somewhat compatible with gprint) */
  case CENTLEFT:
  case CENTCENT:
  case CENTRIGHT:
    printf("\\v'0.85n'");	/* down half */
    break;

  case TOPLEFT:
  case TOPCENT:
  case TOPRIGHT:
    printf("\\v'1.7n'");	/* down whole */
  }

  switch (justify) {
    /* local horizontal motions */
  case BOTCENT:
  case CENTCENT:
  case TOPCENT:
    printf("\\h'-\\w'\\*(g9'u/2u'");	/* back half */
    break;

  case BOTRIGHT:
  case CENTRIGHT:
  case TOPRIGHT:
    printf("\\h'-\\w'\\*(g9'u'");	/* back whole */
  }

  printf("\\&\\*(g9\n");	/* now print the text. */
  printf(".sp |\\n(g8u\n");	/* restore vertical position */
  lasty = savelasty;		/* vertical position restored to where it */
  lastx = xleft;		/* was before text, also horizontal is at */
				/* left                                   */
}				/* end HGPutText */


/*----------------------------------------------------------------------------*
 | Routine:	doarc (center_point, start_point, angle)
 |
 | Results:	Produces either drawarc command or a drawcircle command
 |		depending on the angle needed to draw through.
 *----------------------------------------------------------------------------*/

void
doarc(POINT cp,
      POINT sp,
      int angle)
{
  if (angle)			/* arc with angle */
    HGArc((int) (cp.x * troffscale), (int) (cp.y * troffscale),
	  (int) (sp.x * troffscale), (int) (sp.y * troffscale), angle);
  else				/* a full circle (angle == 0) */
    HGArc((int) (cp.x * troffscale), (int) (cp.y * troffscale),
	  (int) (sp.x * troffscale), (int) (sp.y * troffscale), 0);
}


/*----------------------------------------------------------------------------*
 | Routine:	HGSetFont (font_number, Point_size)
 |
 | Results:	ALWAYS outputs a .ft and .ps directive to troff.  This is
 |		done because someone may change stuff inside a text string. 
 |		Changes thickness back to default thickness.  Default
 |		thickness depends on font and pointsize.
 *----------------------------------------------------------------------------*/

void
HGSetFont(int font,
	  int size)
{
  printf(".ft %s\n"
	 ".ps %d\n", tfont[font - 1], tsize[size - 1]);
  linethickness = DEFTHICK;
}


/*----------------------------------------------------------------------------*
 | Routine:	HGSetBrush (line_mode)
 |
 | Results:	Generates the troff commands to set up the line width and
 |		style of subsequent lines.  Does nothing if no change is
 |              needed.
 |
 | Side Efct:	Sets `linmode' and `linethicknes'.
 *----------------------------------------------------------------------------*/

void
HGSetBrush(int mode)
{
  register int printed = 0;

  if (linmod != style[--mode]) {
    /* Groff doesn't understand \Ds, so we take it out */
    /* printf ("\\D's %du'", linmod = style[mode]); */
    linmod = style[mode];
    printed = 1;
  }
  if (linethickness != thick[mode]) {
    linethickness = thick[mode];
    printf("\\h'-%.2fp'\\D't %.2fp'", linethickness, linethickness);
    printed = 1;
  }
  if (printed)
    cr();
}


/*----------------------------------------------------------------------------*
 | Routine:	dx (x_destination)
 |
 | Results:	Scales and outputs a number for delta x (with a leading
 |		space) given `lastx' and x_destination.
 |
 | Side Efct:	Resets `lastx' to x_destination.
 *----------------------------------------------------------------------------*/

void
dx(double x)
{
  register int ix = (int) (x * troffscale);

  printf(" %du", ix - lastx);
  lastx = ix;
}


/*----------------------------------------------------------------------------*
 | Routine:	dy (y_destination)
 |
 | Results:	Scales and outputs a number for delta y (with a leading
 |		space) given `lastyline' and y_destination.
 |
 | Side Efct:	Resets `lastyline' to y_destination.  Since `line' vertical
 |		motions don't affect `page' ones, `lasty' isn't updated.
 *----------------------------------------------------------------------------*/

void
dy(double y)
{
  register int iy = (int) (y * troffscale);

  printf(" %du", iy - lastyline);
  lastyline = iy;
}


/*----------------------------------------------------------------------------*
 | Routine:	tmove2 (px, py)
 |
 | Results:	Produces horizontal and vertical moves for troff given the
 |		pair of points to move to and knowing the current position. 
 |		Also puts out a horizontal move to start the line.  This is
 |		a variation without the .sp command.
 *----------------------------------------------------------------------------*/

void
tmove2(int px,
       int py)
{
  register int dx;
  register int dy;

  if ((dy = py - lasty)) {
    printf("\\v'%du'", dy);
  }
  lastyline = lasty = py;	/* lasty is always set to current */
  if ((dx = px - lastx)) {
    printf("\\h'%du'", dx);
    lastx = px;
  }
}


/*----------------------------------------------------------------------------*
 | Routine:	tmove (point_pointer)
 |
 | Results:	Produces horizontal and vertical moves for troff given the
 |		pointer of a point to move to and knowing the current
 |		position.  Also puts out a horizontal move to start the
 |		line.
 *----------------------------------------------------------------------------*/

void
tmove(POINT * ptr)
{
  register int ix = (int) (ptr->x * troffscale);
  register int iy = (int) (ptr->y * troffscale);
  register int dx;
  register int dy;

  if ((dy = iy - lasty)) {
    printf(".sp %du\n", dy);
  }
  lastyline = lasty = iy;	/* lasty is always set to current */
  if ((dx = ix - lastx)) {
    printf("\\h'%du'", dx);
    lastx = ix;
  }
}


/*----------------------------------------------------------------------------*
 | Routine:	cr ( )
 |
 | Results:	Ends off an input line.  `.sp -1' is also added to counteract
 |		the vertical move done at the end of text lines.
 |
 | Side Efct:	Sets `lastx' to `xleft' for troff's return to left margin.
 *----------------------------------------------------------------------------*/

void
cr()
{
  printf("\n.sp -1\n");
  lastx = xleft;
}


/*----------------------------------------------------------------------------*
 | Routine:	line ( )
 |
 | Results:	Draws a single solid line to (x,y).
 *----------------------------------------------------------------------------*/

void
line(int px,
     int py)
{
  printf("\\D'l");
  printf(" %du", px - lastx);
  printf(" %du'", py - lastyline);
  lastx = px;
  lastyline = lasty = py;
}


/*----------------------------------------------------------------------------
 | Routine:	drawwig (ptr, type)
 |
 | Results:	The point sequence found in the structure pointed by ptr is
 |		placed in integer arrays for further manipulation by the
 |		existing routing.  With the corresponding type parameter,
 |		either picurve or HGCurve are called.
 *----------------------------------------------------------------------------*/

void
drawwig(POINT * ptr,
	int type)
{
  register int npts;			/* point list index */
  int x[MAXPOINTS], y[MAXPOINTS];	/* point list */

  for (npts = 1; !Nullpoint(ptr); ptr = PTNextPoint(ptr), npts++) {
    x[npts] = (int) (ptr->x * troffscale);
    y[npts] = (int) (ptr->y * troffscale);
  }
  if (--npts) {
    if (type == CURVE) /* Use the 2 different types of curves */
      HGCurve(&x[0], &y[0], npts);
    else
      picurve(&x[0], &y[0], npts);
  }
}


/*----------------------------------------------------------------------------
 | Routine:	HGArc (xcenter, ycenter, xstart, ystart, angle)
 |
 | Results:	This routine plots an arc centered about (cx, cy) counter
 |		clockwise starting from the point (px, py) through `angle'
 |		degrees.  If angle is 0, a full circle is drawn.  It does so
 |		by creating a draw-path around the arc whose density of
 |		points depends on the size of the arc.
 *----------------------------------------------------------------------------*/

void
HGArc(register int cx,
      register int cy,
      int px,
      int py,
      int angle)
{
  double xs, ys, resolution, fullcircle;
  int m;
  register int mask;
  register int extent;
  register int nx;
  register int ny;
  register int length;
  register double epsilon;

  xs = px - cx;
  ys = py - cy;

  length = 0;

  resolution = (1.0 + hypot(xs, ys) / res) * PointsPerInterval;
  /* mask = (1 << (int) log10(resolution + 1.0)) - 1; */
  (void) frexp(resolution, &m);		/* A bit more elegant than log10 */
  for (mask = 1; mask < m; mask = mask << 1);
  mask -= 1;

  epsilon = 1.0 / resolution;
  fullcircle = (2.0 * pi) * resolution;
  if (angle == 0)
    extent = (int) fullcircle;
  else
    extent = (int) (angle * fullcircle / 360.0);

  HGtline(px, py);
  while (--extent >= 0) {
    xs += epsilon * ys;
    nx = cx + (int) (xs + 0.5);
    ys -= epsilon * xs;
    ny = cy + (int) (ys + 0.5);
    if (!(extent & mask)) {
      HGtline(nx, ny);		/* put out a point on circle */
      if (length++ > LINELENGTH) {
	length = 0;
	printf("\\\n");
      }
    }
  }				/* end for */
}				/* end HGArc */


/*----------------------------------------------------------------------------
 | Routine:	picurve (xpoints, ypoints, num_of_points)
 |
 | Results:	Draws a curve delimited by (not through) the line segments
 |		traced by (xpoints, ypoints) point list.  This is the `Pic'
 |		style curve.
 *----------------------------------------------------------------------------*/

void
picurve(register int *x,
	register int *y,
	int npts)
{
  register int nseg;		/* effective resolution for each curve */
  register int xp;		/* current point (and temporary) */
  register int yp;
  int pxp, pyp;			/* previous point (to make lines from) */
  int i;			/* inner curve segment traverser */
  int length = 0;
  double w;			/* position factor */
  double t1, t2, t3;		/* calculation temps */

  if (x[1] == x[npts] && y[1] == y[npts]) {
    x[0] = x[npts - 1];		/* if the lines' ends meet, make */
    y[0] = y[npts - 1];		/* sure the curve meets          */
    x[npts + 1] = x[2];
    y[npts + 1] = y[2];
  } else {			/* otherwise, make the ends of the  */
    x[0] = x[1];		/* curve touch the ending points of */
    y[0] = y[1];		/* the line segments                */
    x[npts + 1] = x[npts];
    y[npts + 1] = y[npts];
  }

  pxp = (x[0] + x[1]) / 2;	/* make the last point pointers       */
  pyp = (y[0] + y[1]) / 2;	/* point to the start of the 1st line */
  tmove2(pxp, pyp);

  for (; npts--; x++, y++) {	/* traverse the line segments */
    xp = x[0] - x[1];
    yp = y[0] - y[1];
    nseg = (int) hypot((double) xp, (double) yp);
    xp = x[1] - x[2];
    yp = y[1] - y[2];
				/* `nseg' is the number of line    */
				/* segments that will be drawn for */
				/* each curve segment.             */
    nseg = (int) ((double) (nseg + (int) hypot((double) xp, (double) yp)) /
		  res * PointsPerInterval);

    for (i = 1; i < nseg; i++) {
      w = (double) i / (double) nseg;
      t1 = w * w;
      t3 = t1 + 1.0 - (w + w);
      t2 = 2.0 - (t3 + t1);
      xp = (((int) (t1 * x[2] + t2 * x[1] + t3 * x[0])) + 1) / 2;
      yp = (((int) (t1 * y[2] + t2 * y[1] + t3 * y[0])) + 1) / 2;

      HGtline(xp, yp);
      if (length++ > LINELENGTH) {
	length = 0;
	printf("\\\n");
      }
    }
  }
}


/*----------------------------------------------------------------------------
 | Routine:	HGCurve(xpoints, ypoints, num_points)
 |
 | Results:	This routine generates a smooth curve through a set of
 |		points.  The method used is the parametric spline curve on
 |		unit knot mesh described in `Spline Curve Techniques' by
 |		Patrick Baudelaire, Robert Flegal, and Robert Sproull --
 |		Xerox Parc.
 *----------------------------------------------------------------------------*/

void
HGCurve(int *x,
	int *y,
	int numpoints)
{
  float h[MAXPOINTS], dx[MAXPOINTS], dy[MAXPOINTS];
  float d2x[MAXPOINTS], d2y[MAXPOINTS], d3x[MAXPOINTS], d3y[MAXPOINTS];
  float t, t2, t3;
  register int j;
  register int k;
  register int nx;
  register int ny;
  int lx, ly;
  int length = 0;

  lx = x[1];
  ly = y[1];
  tmove2(lx, ly);

  /*
   * Solve for derivatives of the curve at each point separately for x and y
   * (parametric).
   */
  Paramaterize(x, y, h, numpoints);

  /* closed curve */
  if ((x[1] == x[numpoints]) && (y[1] == y[numpoints])) {
    PeriodicSpline(h, x, dx, d2x, d3x, numpoints);
    PeriodicSpline(h, y, dy, d2y, d3y, numpoints);
  } else {
    NaturalEndSpline(h, x, dx, d2x, d3x, numpoints);
    NaturalEndSpline(h, y, dy, d2y, d3y, numpoints);
  }

  /*
   * generate the curve using the above information and PointsPerInterval
   * vectors between each specified knot.
   */

  for (j = 1; j < numpoints; ++j) {
    if ((x[j] == x[j + 1]) && (y[j] == y[j + 1]))
      continue;
    for (k = 0; k <= PointsPerInterval; ++k) {
      t = (float) k *h[j] / (float) PointsPerInterval;
      t2 = t * t;
      t3 = t * t * t;
      nx = x[j] + (int) (t * dx[j] + t2 * d2x[j] / 2 + t3 * d3x[j] / 6);
      ny = y[j] + (int) (t * dy[j] + t2 * d2y[j] / 2 + t3 * d3y[j] / 6);
      HGtline(nx, ny);
      if (length++ > LINELENGTH) {
	length = 0;
	printf("\\\n");
      }
    }				/* end for k */
  }				/* end for j */
}				/* end HGCurve */


/*----------------------------------------------------------------------------
 | Routine:	Paramaterize (xpoints, ypoints, hparams, num_points)
 |
 | Results:	This routine calculates parameteric values for use in
 |		calculating curves.  The parametric values are returned
 |		in the array h.  The values are an approximation of
 |		cumulative arc lengths of the curve (uses cord length).
 |		For additional information, see paper cited below.
 *----------------------------------------------------------------------------*/

void
Paramaterize(int x[],
	     int y[],
	     float h[],
	     int n)
{
  register int dx;
  register int dy;
  register int i;
  register int j;
  float u[MAXPOINTS];

  for (i = 1; i <= n; ++i) {
    u[i] = 0;
    for (j = 1; j < i; j++) {
      dx = x[j + 1] - x[j];
      dy = y[j + 1] - y[j];
      /* Here was overflowing, so I changed it.       */
      /* u[i] += sqrt ((double) (dx * dx + dy * dy)); */
      u[i] += hypot((double) dx, (double) dy);
    }
  }
  for (i = 1; i < n; ++i)
    h[i] = u[i + 1] - u[i];
}				/* end Paramaterize */


/*----------------------------------------------------------------------------
 | Routine:	PeriodicSpline (h, z, dz, d2z, d3z, npoints)
 |
 | Results:	This routine solves for the cubic polynomial to fit a spline
 |		curve to the the points specified by the list of values. 
 |		The Curve generated is periodic.  The algorithms for this
 |		curve are from the `Spline Curve Techniques' paper cited
 |		above.
 *----------------------------------------------------------------------------*/

void
PeriodicSpline(float h[],	/* paramaterization  */
	       int z[],		/* point list */
	       float dz[],	/* to return the 1st derivative */
	       float d2z[],	/* 2nd derivative */
	       float d3z[],	/* 3rd derivative */
	       int npoints)	/* number of valid points */
{
  float d[MAXPOINTS];
  float deltaz[MAXPOINTS], a[MAXPOINTS], b[MAXPOINTS];
  float c[MAXPOINTS], r[MAXPOINTS], s[MAXPOINTS];
  int i;

  /* step 1 */
  for (i = 1; i < npoints; ++i) {
    deltaz[i] = h[i] ? ((double) (z[i + 1] - z[i])) / h[i] : 0;
  }
  h[0] = h[npoints - 1];
  deltaz[0] = deltaz[npoints - 1];

  /* step 2 */
  for (i = 1; i < npoints - 1; ++i) {
    d[i] = deltaz[i + 1] - deltaz[i];
  }
  d[0] = deltaz[1] - deltaz[0];

  /* step 3a */
  a[1] = 2 * (h[0] + h[1]);
  b[1] = d[0];
  c[1] = h[0];
  for (i = 2; i < npoints - 1; ++i) {
    a[i] = 2 * (h[i - 1] + h[i]) -
	   pow((double) h[i - 1], (double) 2.0) / a[i - 1];
    b[i] = d[i - 1] - h[i - 1] * b[i - 1] / a[i - 1];
    c[i] = -h[i - 1] * c[i - 1] / a[i - 1];
  }

  /* step 3b */
  r[npoints - 1] = 1;
  s[npoints - 1] = 0;
  for (i = npoints - 2; i > 0; --i) {
    r[i] = -(h[i] * r[i + 1] + c[i]) / a[i];
    s[i] = (6 * b[i] - h[i] * s[i + 1]) / a[i];
  }

  /* step 4 */
  d2z[npoints - 1] = (6 * d[npoints - 2] - h[0] * s[1]
		      - h[npoints - 1] * s[npoints - 2])
		     / (h[0] * r[1] + h[npoints - 1] * r[npoints - 2]
		      + 2 * (h[npoints - 2] + h[0]));
  for (i = 1; i < npoints - 1; ++i) {
    d2z[i] = r[i] * d2z[npoints - 1] + s[i];
  }
  d2z[npoints] = d2z[1];

  /* step 5 */
  for (i = 1; i < npoints; ++i) {
    dz[i] = deltaz[i] - h[i] * (2 * d2z[i] + d2z[i + 1]) / 6;
    d3z[i] = h[i] ? (d2z[i + 1] - d2z[i]) / h[i] : 0;
  }
}				/* end PeriodicSpline */


/*----------------------------------------------------------------------------
 | Routine:	NaturalEndSpline (h, z, dz, d2z, d3z, npoints)
 |
 | Results:	This routine solves for the cubic polynomial to fit a spline
 |		curve the the points specified by the list of values.  The
 |		alogrithms for this curve are from the `Spline Curve
 |		Techniques' paper cited above.
 *----------------------------------------------------------------------------*/

void
NaturalEndSpline(float h[],	/* parameterization */
		 int z[],	/* Point list */
		 float dz[],	/* to return the 1st derivative */
		 float d2z[],	/* 2nd derivative */
		 float d3z[],	/* 3rd derivative */
		 int npoints)	/* number of valid points */
{
  float d[MAXPOINTS];
  float deltaz[MAXPOINTS], a[MAXPOINTS], b[MAXPOINTS];
  int i;

  /* step 1 */
  for (i = 1; i < npoints; ++i) {
    deltaz[i] = h[i] ? ((double) (z[i + 1] - z[i])) / h[i] : 0;
  }
  deltaz[0] = deltaz[npoints - 1];

  /* step 2 */
  for (i = 1; i < npoints - 1; ++i) {
    d[i] = deltaz[i + 1] - deltaz[i];
  }
  d[0] = deltaz[1] - deltaz[0];

  /* step 3 */
  a[0] = 2 * (h[2] + h[1]);
  b[0] = d[1];
  for (i = 1; i < npoints - 2; ++i) {
    a[i] = 2 * (h[i + 1] + h[i + 2]) -
	    pow((double) h[i + 1], (double) 2.0) / a[i - 1];
    b[i] = d[i + 1] - h[i + 1] * b[i - 1] / a[i - 1];
  }

  /* step 4 */
  d2z[npoints] = d2z[1] = 0;
  for (i = npoints - 1; i > 1; --i) {
    d2z[i] = (6 * b[i - 2] - h[i] * d2z[i + 1]) / a[i - 2];
  }

  /* step 5 */
  for (i = 1; i < npoints; ++i) {
    dz[i] = deltaz[i] - h[i] * (2 * d2z[i] + d2z[i + 1]) / 6;
    d3z[i] = h[i] ? (d2z[i + 1] - d2z[i]) / h[i] : 0;
  }
}				/* end NaturalEndSpline */


/*----------------------------------------------------------------------------*
 | Routine:	change (x_position, y_position, visible_flag)
 |
 | Results:	As HGtline passes from the invisible to visible (or vice
 |		versa) portion of a line, change is called to either draw
 |		the line, or initialize the beginning of the next one.
 |		Change calls line to draw segments if visible_flag is set
 |		(which means we're leaving a visible area).
 *----------------------------------------------------------------------------*/

void
change(register int x,
       register int y,
       register int vis)
{
  static int length = 0;

  if (vis) {			/* leaving a visible area, draw it. */
    line(x, y);
    if (length++ > LINELENGTH) {
      length = 0;
      printf("\\\n");
    }
  } else {			/* otherwise, we're entering one, remember */
				/* beginning                               */
    tmove2(x, y);
  }
}


/*----------------------------------------------------------------------------
 | Routine:	HGtline (xstart, ystart, xend, yend)
 |
 | Results:	Draws a line from current position to (x1,y1) using line(x1,
 |		y1) to place individual segments of dotted or dashed lines.
 *----------------------------------------------------------------------------*/

void
HGtline(int x1,
	int y1)
{
  register int x0 = lastx;
  register int y0 = lasty;
  register int dx;
  register int dy;
  register int oldcoord;
  register int res1;
  register int visible;
  register int res2;
  register int xinc;
  register int yinc;
  register int dotcounter;

  if (linmod == SOLID) {
    line(x1, y1);
    return;
  }

  /* for handling different resolutions */
  dotcounter = linmod << dotshifter;

  xinc = 1;
  yinc = 1;
  if ((dx = x1 - x0) < 0) {
    xinc = -xinc;
    dx = -dx;
  }
  if ((dy = y1 - y0) < 0) {
    yinc = -yinc;
    dy = -dy;
  }
  res1 = 0;
  res2 = 0;
  visible = 0;
  if (dx >= dy) {
    oldcoord = y0;
    while (x0 != x1) {
      if ((x0 & dotcounter) && !visible) {
	change(x0, y0, 0);
	visible = 1;
      } else if (visible && !(x0 & dotcounter)) {
	change(x0 - xinc, oldcoord, 1);
	visible = 0;
      }
      if (res1 > res2) {
	oldcoord = y0;
	res2 += dx - res1;
	res1 = 0;
	y0 += yinc;
      }
      res1 += dy;
      x0 += xinc;
    }
  } else {
    oldcoord = x0;
    while (y0 != y1) {
      if ((y0 & dotcounter) && !visible) {
	change(x0, y0, 0);
	visible = 1;
      } else if (visible && !(y0 & dotcounter)) {
	change(oldcoord, y0 - yinc, 1);
	visible = 0;
      }
      if (res1 > res2) {
	oldcoord = x0;
	res2 += dy - res1;
	res1 = 0;
	x0 += xinc;
      }
      res1 += dx;
      y0 += yinc;
    }
  }
  if (visible)
    change(x1, y1, 1);
  else
    change(x1, y1, 0);
}

/* EOF */
