/* Last non-groff version: main.c 1.23  (Berkeley)  85/08/05
 *
 * Adapted to GNU troff by Daniel Senderowicz 99/12/29.
 *
 * Further refinements by Werner Lemberg 00/02/20.
 *
 *
 * This file contains the main and file system dependent routines for
 * processing gremlin files into troff input.  The program watches input go
 * by to standard output, only interpreting things between .GS and .GE
 * lines.  Default values (font, size, scale, thickness) may be overridden
 * with a `default' command and are further overridden by commands in the
 * input.
 *
 * Inside the GS and GE, commands are accepted to reconfigure the picture. 
 * At most one command may reside on each line, and each command is followed
 * by a parameter separated by white space.  The commands are as follows,
 * and may be abbreviated down to one character (with exception of `scale'
 * and `stipple' down to "sc" and "st") and may be upper or lower case.
 *
 *                        default  -  Make all settings in the current
 *                                    .GS/.GE the global defaults.  Height,
 *                                    width and file are NOT saved.
 *                     1, 2, 3, 4  -  Set size 1, 2, 3, or 4 (followed by an
 *                                    integer point size).
 *  roman, italics, bold, special  -  Set gremlin's fonts to any other troff
 *                                    font (one or two characters).
 *                     stipple, l  -  Use a stipple font for polygons.  Arg
 *                                    is troff font name.  No Default.  Can
 *                                    use only one stipple font per picture. 
 *                                    (See below for stipple font index.)
 *                       scale, x  -  Scale is IN ADDITION to the global
 *                                    scale factor from the default.
 *                     pointscale  -  Turn on scaling point sizes to match
 *                                    `scale' commands.  (Optional operand
 *                                    `off' to turn it off.)
 *          narrow, medium, thick  -  Set widths of lines.
 *                           file  -  Set the file name to read the gremlin
 *                                    picture from.  If the file isn't in
 *                                    the current directory, the gremlin
 *                                    library is tried.
 *                  width, height  -  These two commands override any
 *                                    scaling factor that is in effect, and
 *                                    forces the picture to fit into either
 *                                    the height or width specified,
 *                                    whichever makes the picture smaller. 
 *                                    The operand for these two commands is
 *                                    a floating-point number in units of
 *                                    inches.
 *            l<nn> (integer <nn>) -  Set association between stipple <nn>
 *                                    and a stipple `character'.  <nn> must
 *                                    be in the range 0 to NSTIPPLES (16)
 *                                    inclusive.  The integer operand is an
 *                                    index in the stipple font selected. 
 *                                    Valid cf (cifplot) indices are 1-32
 *                                    (although 24 is not defined), valid ug
 *                                    (unigrafix) indices are 1-14, and
 *                                    valid gs (gray scale) indices are
 *                                    0-16.  Nonetheless, any number between
 *                                    0 and 255 is accepted since new
 *                                    stipple fonts may be added.  An
 *                                    integer operand is required.
 *
 * Troff number registers used:  g1 through g9.  g1 is the width of the
 * picture, and g2 is the height.  g3, and g4, save information, g8 and g9
 * are used for text processing and g5-g7 are reserved.
 */


#include "lib.h"

#include <ctype.h>
#include <stdlib.h>
#include "gprint.h"

#include "device.h"
#include "font.h"
#include "searchpath.h"
#include "macropath.h"

#include "errarg.h"
#include "error.h"
#include "defs.h"

extern "C" const char *Version_string;

/* database imports */

extern void HGPrintElt(ELT *element, int baseline);
extern ELT *DBInit();
extern ELT *DBRead(register FILE *file);
extern POINT *PTInit();
extern POINT *PTMakePoint(double x, double y, POINT **pplist);


#define SUN_SCALEFACTOR 0.70

/* #define DEFSTIPPLE    "gs" */
#define DEFSTIPPLE	"cf"

#define MAXINLINE	100	/* input line length */

#define SCREENtoINCH	0.02	/* scaling factor, screen to inches */

#define BIG	999999999999.0	/* unweildly large floating number */


static char sccsid[] = "@(#) (Berkeley) 8/5/85, 12/28/99";

int res;			/* the printer's resolution goes here */

int dotshifter;			/* for the length of dotted curves */

double linethickness;		/* brush styles */
int linmod;
int lastx;			/* point registers for printing elements */
int lasty;
int lastyline;			/* A line's vertical position is NOT the  */
				/* same after that line is over, so for a */
				/* line of drawing commands, vertical     */
				/* spacing is kept in lastyline           */

/* These are the default fonts, sizes, line styles, */
/* and thicknesses.  They can be modified from a    */
/* `default' command and are reset each time the    */
/* start of a picture (.GS) is found.               */

const char *deffont[] =
{"R", "I", "B", "S"};
int defsize[] =
{10, 16, 24, 36};
/* #define BASE_THICKNESS 1.0 */
#define BASE_THICKNESS 0.15
double defthick[STYLES] =
{1 * BASE_THICKNESS,
 1 * BASE_THICKNESS,
 5 * BASE_THICKNESS,
 1 * BASE_THICKNESS,
 1 * BASE_THICKNESS,
 3 * BASE_THICKNESS};

/* int cf_stipple_index[NSTIPPLES + 1] =                                  */
/* {0, 1, 3, 12, 14, 16, 19, 21, 23};                                     */
/* a logarithmic scale looks better than a linear one for the gray shades */
/*                                                                        */
/* int other_stipple_index[NSTIPPLES + 1] =                               */
/* {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};            */

int cf_stipple_index[NSTIPPLES + 1] =
{0, 18, 32, 56, 100, 178, 316, 562, 1000};	/* only 1-8 used */
int other_stipple_index[NSTIPPLES + 1] =
{0, 62, 125, 187, 250, 312, 375, 437, 500,
 562, 625, 687, 750, 812, 875, 937, 1000};

/* int *defstipple_index = other_stipple_index; */
int *defstipple_index = cf_stipple_index;

int style[STYLES] =
{DOTTED, DOTDASHED, SOLID, DASHED, SOLID, SOLID};
double scale = 1.0;		/* no scaling, default */
int defpoint = 0;		/* flag for pointsize scaling */
char *defstipple = (char *) 0;
enum E {
  OUTLINE, FILL, BOTH
} polyfill;

/* flag to controll filling of polygons */

double adj1 = 0.0;
double adj2 = 0.0;
double adj3 = 0.0;
double adj4 = 0.0;

double thick[STYLES];		/* thicknesses set by defaults, then by */
				/* commands                             */
char *tfont[FONTS];		/* fonts originally set to deffont values, */
				/* then                                    */
int tsize[SIZES];		/* optionally changed by commands inside */
				/* grn                                   */
int stipple_index[NSTIPPLES + 1];	/* stipple font file indices */
char *stipple;

double xscale;			/* scaling factor from individual pictures */
double troffscale;		/* scaling factor at output time */

double width;			/* user-request maximum width for picture */
				/* (in inches)                            */
double height;			/* user-request height */
int pointscale;			/* flag for pointsize scaling */
int setdefault;			/* flag for a .GS/.GE to remember all */
				/* settings                           */
int sflag;			/* -s flag: sort order (do polyfill first) */

double toppoint;		/* remember the picture */
double bottompoint;		/* bounds in these variables */
double leftpoint;
double rightpoint;

int ytop;			/* these are integer versions of the above */
int ybottom;			/* so not to convert each time they're used */
int xleft;
int xright;

int linenum = 0;		/* line number of input file */
char inputline[MAXINLINE];	/* spot to filter through the file */
char *c1 = inputline;		/* c1, c2, and c3 will be used to */
char *c2 = inputline + 1;	/* hunt for lines that begin with */
char *c3 = inputline + 2;	/* ".GS" by looking individually */
char *c4 = inputline + 3;	/* needed for compatibility mode */
char GScommand[MAXINLINE];	/* put user's ".GS" command line here */
char gremlinfile[MAXINLINE];	/* filename to use for a picture */
int SUNFILE = FALSE;		/* TRUE if SUN gremlin file */
int compatibility_flag = FALSE;	/* TRUE if in compatibility mode */


void getres();
int doinput(FILE *fp);
void conv(register FILE *fp, int baseline);
void savestate();
int has_polygon(register ELT *elist);
void interpret(char *line);


void
usage(FILE *stream)
{
  fprintf(stream,
	  "usage: %s [ -vCs ] [ -M dir ] [ -F dir ] [ -T dev ] [ file ]\n",
	  program_name);
}


/*----------------------------------------------------------------------------*
 | Routine:	main (argument_count, argument_pointer)
 |
 | Results:	Parses the command line, accumulating input file names, then
 |		reads the inputs, passing it directly to output until a `.GS'
 |		line is read.  Main then passes control to `conv' to do the
 |		gremlin file conversions.
 *----------------------------------------------------------------------------*/

int
main(int argc,
     char **argv)
{
  setlocale(LC_NUMERIC, "C");
  program_name = argv[0];
  register FILE *fp;
  register int k;
  register char c;
  register int gfil = 0;
  char *file[50];
  char *operand(int *argcp, char ***argvp);

  while (--argc) {
    if (**++argv != '-')
      file[gfil++] = *argv;
    else
      switch (c = (*argv)[1]) {

      case 0:
	file[gfil++] = NULL;
	break;

      case 'C':		/* compatibility mode */
	compatibility_flag = TRUE;
	break;

      case 'F':		/* font path to find DESC */
	font::command_line_font_dir(operand(&argc, &argv));
	break;

      case 'T':		/* final output typesetter name */
	device = operand(&argc, &argv);
	break;

      case 'M':		/* set library directory */
	macro_path.command_line_dir(operand(&argc, &argv));
	break;

      case 's':		/* preserve order of elements */
	sflag = 1;
	break;

      case '-':
	if (strcmp(*argv,"--version")==0) {
      case 'v':
	  printf("GNU grn (groff) version %s\n", Version_string);
	  exit(0);
	  break;
	}
	if (strcmp(*argv,"--help")==0) {
      case '?':
	  usage(stdout);
	  exit(0);
	  break;
	}
	// fallthrough
      default:
	error("unknown switch: %1", c);
	usage(stderr);
	exit(1);
      }
  }

  getres();			/* set the resolution for an output device */

  if (gfil == 0) {		/* no filename, use standard input */
    file[0] = NULL;
    gfil++;
  }

  for (k = 0; k < gfil; k++) {
    if (file[k] != NULL) {
      if ((fp = fopen(file[k], "r")) == NULL)
	fatal("can't open %1", file[k]);
    } else
      fp = stdin;

    while (doinput(fp)) {
      if (*c1 == '.' && *c2 == 'G' && *c3 == 'S') {
	if (compatibility_flag ||
	    *c4 == '\n' || *c4 == ' ' || *c4 == '\0')
	  conv(fp, linenum);
	else
	  fputs(inputline, stdout);
      } else
	fputs(inputline, stdout);
    }
  }

  return 0;
}


/*----------------------------------------------------------------------------*
 | Routine:	char  * operand (& argc, & argv)
 |
 | Results:	Returns address of the operand given with a command-line
 |		option.  It uses either `-Xoperand' or `-X operand', whichever
 |		is present.  The program is terminated if no option is
 |		present.
 |
 | Side Efct:	argc and argv are updated as necessary.
 *----------------------------------------------------------------------------*/

char *
operand(int *argcp,
	char ***argvp)
{
  if ((**argvp)[2])
    return (**argvp + 2);	/* operand immediately follows */
  if ((--*argcp) <= 0) {	/* no operand */
    error("command-line option operand missing.");
    exit(8);
  }
  return (*(++(*argvp)));	/* operand is next word */
}


/*----------------------------------------------------------------------------*
 | Routine:	getres ()
 |
 | Results:	Sets `res' to the resolution of the output device.
 *----------------------------------------------------------------------------*/

void
getres()
{
  int linepiece;

  if (!font::load_desc())
    fatal("sorry, I can't continue");

  res = font::res;

  /* Correct the brush thicknesses based on res */
  /* if (res >= 256) {
      defthick[0] = res >> 8;
      defthick[1] = res >> 8;
      defthick[2] = res >> 4;
      defthick[3] = res >> 8;
      defthick[4] = res >> 8;
      defthick[5] = res >> 6;
      } */

  linepiece = res >> 9;
  for (dotshifter = 0; linepiece; dotshifter++)
    linepiece = linepiece >> 1;
}


/*----------------------------------------------------------------------------*
 | Routine:	int  doinput (file_pointer)
 |
 | Results:	A line of input is read into `inputline'.
 |
 | Side Efct:	"linenum" is incremented.
 |
 | Bugs:	Lines longer than MAXINLINE are NOT checked, except for 
 |		updating `linenum'.
 *----------------------------------------------------------------------------*/

int
doinput(FILE *fp)
{
  if (fgets(inputline, MAXINLINE, fp) == NULL)
    return 0;
  if (strchr(inputline, '\n'))	/* ++ only if it's a complete line */
    linenum++;
  return 1;
}


/*----------------------------------------------------------------------------*
 | Routine:	initpic ( )
 |
 | Results:	Sets all parameters to the normal defaults, possibly
 |		overridden by a setdefault command.  Initialize the picture
 |		variables, and output the startup commands to troff to begin
 |		the picture.
 *----------------------------------------------------------------------------*/

void
initpic()
{
  register int i;

  for (i = 0; i < STYLES; i++) {	/* line thickness defaults */
    thick[i] = defthick[i];
  }
  for (i = 0; i < FONTS; i++) {		/* font name defaults */
    tfont[i] = (char *)deffont[i];
  }
  for (i = 0; i < SIZES; i++) {		/* font size defaults */
    tsize[i] = defsize[i];
  }
  for (i = 0; i <= NSTIPPLES; i++) {	/* stipple font file default indices */
    stipple_index[i] = defstipple_index[i];
  }
  stipple = defstipple;

  gremlinfile[0] = 0;		/* filename is `null' */
  setdefault = 0;		/* this is not the default settings (yet) */

  toppoint = BIG;		/* set the picture bounds out */
  bottompoint = -BIG;		/* of range so they'll be set */
  leftpoint = BIG;		/* by `savebounds' on input */
  rightpoint = -BIG;

  pointscale = defpoint;	/* flag for scaling point sizes default */
  xscale = scale;		/* default scale of individual pictures */
  width = 0.0;			/* size specifications input by user */
  height = 0.0;

  linethickness = DEFTHICK;	/* brush styles */
  linmod = DEFSTYLE;
}


/*----------------------------------------------------------------------------*
 | Routine:	conv (file_pointer, starting_line)
 |
 | Results:	At this point, we just passed a `.GS' line in the input
 |		file.  conv reads the input and calls `interpret' to process
 |		commands, gathering up information until a `.GE' line is
 |		found.  It then calls `HGPrint' to do the translation of the
 |		gremlin file to troff commands.
 *----------------------------------------------------------------------------*/

void
conv(register FILE *fp,
     int baseline)
{
  register FILE *gfp = NULL;	/* input file pointer */
  register int done = 0;	/* flag to remember if finished */
  register ELT *e;		/* current element pointer */
  ELT *PICTURE;			/* whole picture data base pointer */
  double temp;			/* temporary calculating area */
  /* POINT ptr; */		/* coordinates of a point to pass to `mov' */
				/* routine                                 */
  int flyback;			/* flag `want to end up at the top of the */
				/* picture?'                              */
  int compat;			/* test character after .GE or .GF */


  initpic();			/* set defaults, ranges, etc. */
  strcpy(GScommand, inputline);	/* save `.GS' line for later */

  do {
    done = !doinput(fp);		/* test for EOF */
    flyback = (*c3 == 'F');		/* and .GE or .GF */
    compat = (compatibility_flag ||
	      *c4 == '\n' || *c4 == ' ' || *c4 == '\0');
    done |= (*c1 == '.' && *c2 == 'G' && (*c3 == 'E' || flyback) &&
	     compat);

    if (done) {
      if (setdefault)
	savestate();

      if (!gremlinfile[0]) {
	if (!setdefault)
	  error("at line %1: no picture filename.\n", baseline);
	return;
      }
      char *path;
      gfp = macro_path.open_file(gremlinfile, &path);
      if (!gfp)
	return;
      PICTURE = DBRead(gfp);	/* read picture file */
      fclose(gfp);
      a_delete path;
      if (DBNullelt(PICTURE))
	return;			/* If a request is made to make the  */
				/* picture fit into a specific area, */
				/* set the scale to do that.         */

      if (stipple == (char *) NULL)	/* if user forgot stipple    */
	if (has_polygon(PICTURE))	/* and picture has a polygon */
	  stipple = (char *)DEFSTIPPLE;		/* then set the default      */

      if ((temp = bottompoint - toppoint) < 0.1)
	temp = 0.1;
      temp = (height != 0.0) ? height / (temp * SCREENtoINCH) : BIG;
      if ((troffscale = rightpoint - leftpoint) < 0.1)
	troffscale = 0.1;
      troffscale = (width != 0.0) ?
	  width / (troffscale * SCREENtoINCH) : BIG;
      if (temp == BIG && troffscale == BIG)
	troffscale = xscale;
      else {
	if (temp < troffscale)
	  troffscale = temp;
      }				/* here, troffscale is the */
				/* picture's scaling factor */
      if (pointscale) {
	register int i;		/* do pointscaling here, when */
				/* scale is known, before output */
	for (i = 0; i < SIZES; i++)
	  tsize[i] = (int) (troffscale * (double) tsize[i] + 0.5);
      }

						/* change to device units */
      troffscale *= SCREENtoINCH * res;		/* from screen units */

      ytop = (int) (toppoint * troffscale);		/* calculate integer */
      ybottom = (int) (bottompoint * troffscale);	/* versions of the   */
      xleft = (int) (leftpoint * troffscale);		/* picture limits    */
      xright = (int) (rightpoint * troffscale);

      /* save stuff in number registers,    */
      /*   register g1 = picture width and  */
      /*   register g2 = picture height,    */
      /*   set vertical spacing, no fill,   */
      /*   and break (to make sure picture  */
      /*   starts on left), and put out the */
      /*   user's `.GS' line.               */
      printf(".br\n"
	     ".nr g1 %du\n"
	     ".nr g2 %du\n"
	     "%s"
	     ".nr g3 \\n(.f\n"
	     ".nr g4 \\n(.s\n"
	     "\\0\n"
	     ".sp -1\n",
	     xright - xleft, ybottom - ytop, GScommand);

      if (stipple)		/* stipple requested for this picture */
	printf(".st %s\n", stipple);
      lastx = xleft;		/* note where we are (upper left */
      lastyline = lasty = ytop;	/* corner of the picture)        */

      /* Just dump everything in the order it appears.
       *
       * If -s command-line option, traverse picture twice: First time,
       * print only the interiors of filled polygons (as borderless
       * polygons).  Second time, print the outline as series of line
       * segments.  This way, postprocessors that overwrite rather than
       * merge picture elements (such as Postscript) can still have text and
       * graphics on a shaded background.
       */
      /* if (sflag) */
      if (!sflag) {		/* changing the default for filled polygons */
	e = PICTURE;
	polyfill = FILL;
	while (!DBNullelt(e)) {
	  printf(".mk\n");
	  if (e->type == POLYGON)
	    HGPrintElt(e, baseline);
	  printf(".rt\n");
	  lastx = xleft;
	  lastyline = lasty = ytop;
	  e = DBNextElt(e);
	}
      }
      e = PICTURE;

      /* polyfill = !sflag ? BOTH : OUTLINE; */
      polyfill = sflag ? BOTH : OUTLINE;	/* changing the default */
      while (!DBNullelt(e)) {
	printf(".mk\n");
	HGPrintElt(e, baseline);
	printf(".rt\n");
	lastx = xleft;
	lastyline = lasty = ytop;
	e = DBNextElt(e);
      }

      /* decide where to end picture */

      /* I changed everything here.  I always use the combination .mk and */
      /* .rt so once finished I just space down the heigth of the picture */
      /* that is \n(g2u                                                   */
      if (flyback) {		/* end picture at upper left */
	/* ptr.x = leftpoint;
	   ptr.y = toppoint; */
      } else {			/* end picture at lower left */
	/* ptr.x = leftpoint;
	   ptr.y = bottompoint; */
	printf(".sp \\n(g2u\n");
      }

      /* tmove(&ptr); */	/* restore default line parameters */

      /* restore everything to the way it was before the .GS, then put */
      /* out the `.GE' line from user                                  */

      /* printf("\\D't %du'\\D's %du'\n", DEFTHICK, DEFSTYLE); */
      /* groff doesn't understand the \Ds command */

      printf("\\D't %du'\n", DEFTHICK);
      if (flyback)		/* make sure we end up at top of */
	printf(".sp -1\n");	/* picture if `flying back'      */
      if (stipple)		/* restore stipple to previous */
	printf(".st\n");
      printf(".br\n"
	     ".ft \\n(g3\n"
	     ".ps \\n(g4\n"
	     "%s", inputline);
    } else
      interpret(inputline);	/* take commands from the input file */
  } while (!done);
}


/*----------------------------------------------------------------------------*
 | Routine:	savestate  ( )
 |
 | Results:	all the current  scaling / font size / font name / thickness
 |		/ pointscale settings are saved to be the defaults.  Scaled
 |		point sizes are NOT saved.  The scaling is done each time a
 |		new picture is started.
 |
 | Side Efct:	scale, and def* are modified.
 *----------------------------------------------------------------------------*/

void
savestate()
{
  register int i;

  for (i = 0; i < STYLES; i++)	/* line thickness defaults */
    defthick[i] = thick[i];
  for (i = 0; i < FONTS; i++)	/* font name defaults */
    deffont[i] = tfont[i];
  for (i = 0; i < SIZES; i++)	/* font size defaults */
    defsize[i] = tsize[i];
  for (i = 0; i <= NSTIPPLES; i++)	/* stipple font file default indices */
    defstipple_index[i] = stipple_index[i];

  defstipple = stipple;		/* if stipple has been set, it's remembered */
  scale *= xscale;		/* default scale of individual pictures */
  defpoint = pointscale;	/* flag for scaling pointsizes from x factors */
}


/*----------------------------------------------------------------------------*
 | Routine:	savebounds (x_coordinate, y_coordinate)
 |
 | Results:	Keeps track of the maximum and minimum extent of a picture
 |		in the global variables: left-, right-, top- and
 |		bottompoint.  `savebounds' assumes that the points have been
 |		oriented to the correct direction.  No scaling has taken
 |		place, though.
 *----------------------------------------------------------------------------*/

void
savebounds(double x,
	   double y)
{
  if (x < leftpoint)
    leftpoint = x;
  if (x > rightpoint)
    rightpoint = x;
  if (y < toppoint)
    toppoint = y;
  if (y > bottompoint)
    bottompoint = y;
}


/*----------------------------------------------------------------------------*
 | Routine:	interpret (character_string)
 |
 | Results:	Commands are taken from the input string and performed.
 |		Commands are separated by the endofline, and are of the
 |		format:
 |			string1 string2
 |
 |		where string1 is the command and string2 is the argument.
 |
 | Side Efct:	Font and size strings, plus the gremlin file name and the
 |		width and height variables are set by this routine.
 *----------------------------------------------------------------------------*/

void
interpret(char *line)
{
  char str1[MAXINLINE];
  char str2[MAXINLINE];
  register char *chr;
  register int i;
  double par;

  str2[0] = '\0';
  sscanf(line, "%80s%80s", &str1[0], &str2[0]);
  for (chr = &str1[0]; *chr; chr++)	/* convert command to */
    if (isupper(*chr))
      *chr = tolower(*chr);	/* lower case */

  switch (str1[0]) {

  case '1':
  case '2':			/* font sizes */
  case '3':
  case '4':
    i = atoi(str2);
    if (i > 0 && i < 1000)
      tsize[str1[0] - '1'] = i;
    else
      error("bad font size value at line %1", linenum);
    break;

  case 'r':			/* roman */
    if (str2[0] < '0')
      goto nofont;
    tfont[0] = (char *) malloc(strlen(str2) + 1);
    strcpy(tfont[0], str2);
    break;

  case 'i':			/* italics */
    if (str2[0] < '0')
      goto nofont;
    tfont[1] = (char *) malloc(strlen(str2) + 1);
    strcpy(tfont[1], str2);
    break;

  case 'b':			/* bold */
    if (str2[0] < '0')
      goto nofont;
    tfont[2] = (char *) malloc(strlen(str2) + 1);
    strcpy(tfont[2], str2);
    break;

  case 's':			/* special */
    if (str1[1] == 'c')
      goto scalecommand;	/* or scale */

    if (str2[0] < '0') {
  nofont:
      error("no fontname specified in line %1", linenum);
      break;
    }
    if (str1[1] == 't')
      goto stipplecommand;	/* or stipple */

    tfont[3] = (char *) malloc(strlen(str2) + 1);
    strcpy(tfont[3], str2);
    break;

  case 'l':			/* l */
    if (isdigit(str1[1])) {	/* set stipple index */
      int idx = atoi(str1 + 1), val;

      if (idx < 0 || idx > NSTIPPLES) {
	error("bad stipple number %1 at line %2", idx, linenum);
	break;
      }
      if (!defstipple_index)
	defstipple_index = other_stipple_index;
      val = atoi(str2);
      if (val >= 0 && val < 256)
	stipple_index[idx] = val;
      else
	error("bad stipple index value at line %1", linenum);
      break;
    }

  stipplecommand:		/* set stipple name */
    stipple = (char *) malloc(strlen(str2) + 1);
    strcpy(stipple, str2);
    /* if its a `known' font (currently only `cf'), set indicies    */
    if (strcmp(stipple, "cf") == 0)
      defstipple_index = cf_stipple_index;
    else
      defstipple_index = other_stipple_index;
    for (i = 0; i <= NSTIPPLES; i++)
      stipple_index[i] = defstipple_index[i];
    break;

  case 'a':			/* text adjust */
    par = atof(str2);
    switch (str1[1]) {
    case '1':
      adj1 = par;
      break;
    case '2':
      adj2 = par;
      break;
    case '3':
      adj3 = par;
      break;
    case '4':
      adj4 = par;
      break;
    default:
      error("bad adjust command at line %1", linenum);
      break;
    }
    break;

  case 't':			/* thick */
    thick[2] = defthick[0] * atof(str2);
    break;

  case 'm':			/* medium */
    thick[5] = defthick[0] * atof(str2);
    break;

  case 'n':			/* narrow */
    thick[0] = thick[1] = thick[3] = thick[4] =
	defthick[0] * atof(str2);
    break;

  case 'x':			/* x */
  scalecommand:			/* scale */
    par = atof(str2);
    if (par > 0.0)
      xscale *= par;
    else
      error("invalid scale value on line %1", linenum);
    break;

  case 'f':			/* file */
    strcpy(gremlinfile, str2);
    break;

  case 'w':			/* width */
    width = atof(str2);
    if (width < 0.0)
      width = -width;
    break;

  case 'h':			/* height */
    height = atof(str2);
    if (height < 0.0)
      height = -height;
    break;

  case 'd':			/* defaults */
    setdefault = 1;
    break;

  case 'p':			/* pointscale */
    if (strcmp("off", str2))
      pointscale = 1;
    else
      pointscale = 0;
    break;

  default:
    error("unknown command `%1' on line %2", str1, linenum);
    exit(8);
    break;
  };
}


/*
 * return TRUE if picture contains a polygon
 * otherwise FALSE
 */

int
has_polygon(register ELT *elist)
{
  while (!DBNullelt(elist)) {
    if (elist->type == POLYGON)
      return (1);
    elist = DBNextElt(elist);
  }

  return (0);
}

/* EOF */
