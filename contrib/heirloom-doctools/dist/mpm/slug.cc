/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 source code published at the 9fans list by Rob Pike,
 * <http://lists.cse.psu.edu/archives/9fans/2002-February/015773.html>
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)slug.cc	1.7 (gritter) 7/29/06	*/
#include	"misc.h"
#include	"slug.h"
#include	<math.h>

static char	*bufptr(int);

void slug::coalesce()
{
	(this+1)->dp = dp;	// pretty grimy, but meant to ensure
				// that all output goes out.
			// maybe it has to skip over PT's;
			// some stuff is getting pushed inside PT..END
}

void slug::neutralize()
{
	switch (type) {
	case PAGE:
	case UF:
	case BF:
	case PARM:
		type = NEUTRAL;
		coalesce();
		break;
	default:
		WARNING("neutralized %d (%s) with %s\n",
			type, typname(), headstr());
		break;
	}
}

void slug::dump()	// print contents of a slug
{
	printf("# %d %-4.4s parm %d dv %d base %d s%g f%d H%d\n#\t\t%s\n",
		serialno(), typname(), parm, dv, base,
		size, font, hpos, headstr());
}

char *slug::headstr()
{
	const int HEADLEN = 4096;
	static char buf[2*HEADLEN];
	int j = 0;
	char *s = bufptr(dp);
	int n = (this+1)->dp - dp;
	if (n >= HEADLEN)
		n = HEADLEN;
	for (int i = 0; i < n; i++)
		switch (s[i]) {
			case '\n':
			case '\t':
			case '\0':
			case ' ':
				break;
			default:
				buf[j++] = s[i];
				break;
		}
	buf[j] = 0;
	return buf;
}

static char *strindex(char s[], const char t[])	// index of earliest t[] in s[]
{
	for (int i = 0; s[i] != '\0'; i++) {
		int j, k;
		for (j = i, k = 0; t[k]!='\0' && s[j] == t[k]; j++, k++)
			;
		if (k > 0 && t[k] == '\0')
			return s+i;
	}
	return 0;
}

void slug::slugout(int col)
{
	static int numout = 0;
	if (seen++)
		WARNING("%s slug #%d seen %d times [%s]\n",
			typname(), serialno(), seen, headstr());
	if (type == TM) {
		char *p;
		if ((p = strindex(bufptr(dp), "x X TM ")))
			p += strlen("x X TM ");		// skip junk
		else
			FATAL("strange TM [%s]\n", headstr());
		fprintf(stderr, "%d\t", userpn);	// page # as prefix
		for ( ; p < bufptr((this+1)->dp); p++)
			putc(*p, stderr);
	} else if (type == COORD) {
		for (char *p = bufptr(dp); p < bufptr((this+1)->dp) && *p != '\n'; p++)
			putc(*p, stdout);
		printf(" # P %d X %d", userpn, hpos + col*offset);
		return;
	} else if (type == VBOX) {
		if (numout++ > 0) {	// BUG??? might miss something
			if (size == (int)size)
				printf("s%d\n", (int)size);
			else
				printf("s-23 %g\n", size);
			printf("f%d\n", font);
		}
		printf("H%d\n", hpos + col*offset);
	}
	fwrite(bufptr(dp), sizeof(char), (this+1)->dp - dp, stdout);
}

char *slug::typname()
{
	static char buf[50];
	const char *p = buf;		// return value
	switch(type) {
	case EOF:	p = "EOF"; break;
	case VBOX:	p = "VBOX"; break;
	case SP:	p = "SP"; break;
	case BS:	p = "BS"; break;
	case US:	p = "US"; break;
	case BF:	p = "BF"; break;
	case UF:	p = "UF"; break;
	case PT:	p = "PT"; break;
	case BT:	p = "BT"; break;
	case END:	p = "END"; break;
	case NEUTRAL:	p = "NEUT"; break;
	case PAGE:	p = "PAGE"; break;
	case TM:	p = "TM"; break;
	case COORD:	p = "COORD"; break;
	case NE:	p = "NE"; break;
	case CMD:	p = "CMD"; break;
	case PARM:	p = "PARM"; break;
	default:	snprintf(buf, sizeof(buf), "weird type %d", type);
	}
	return (char *)p;
}

// ================================================================================

// 	troff output-specific functions

// ================================================================================

const int	DELTABUF = 500000;	// grow the input buffer in chunks

static char	*inbuf = 0;		// raw text input collects here
static int	ninbuf = 0;		// byte count for inbuf
static char	*inbp = 0;		// next free slot in inbuf
int		linenum = 0;		// input line number

static inline void addc(int c) { *inbp++ = c; }

static void adds(char *s)
{
	for (char *p = s; *p; p++)
		addc(*p);
}

static char *getutf(FILE *fp)	// get 1 utf-encoded char (might be multiple bytes)
{
	static char buf[100];
	char *p = buf;

	for (*p = 0; (*p++ = getc(fp)) != EOF; ) {
		*p = 0;
#ifdef	EUC
		if (mblen(buf, sizeof buf) > 0)	// found a valid character
#endif
			break;
	}
	return buf;
}

static char *bufptr(int n) { return inbuf + n; }  // scope of inbuf is too local

static inline int wherebuf() { return inbp - inbuf; }

static char *getstr(char *p, char *temp)
{		// copy next non-blank string from p to temp, update p
	while (*p == ' ' || *p == '\t' || *p == '\n')
		p++;
	if (*p == '\0') {
		temp[0] = 0;
		return(NULL);
	}
	while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
		*temp++ = *p++;
	*temp = '\0';
	return(p);
}

/***************************************************************************
   bounding box of a circular arc             Eric Grosse  24 May 84

Conceptually, this routine generates a list consisting of the start,
end, and whichever north, east, south, and west points lie on the arc.
The bounding box is then the range of this list.
    list = {start,end}
    j = quadrant(start)
    k = quadrant(end)
    if( j==k && long way 'round )  append north,west,south,east
    else
      while( j != k )
         append center+radius*[j-th of north,west,south,east unit vectors]
         j += 1  (mod 4)
    return( bounding box of list )
The following code implements this, with simple optimizations.
***********************************************************************/

static int quadrant(double x, double y)
{
	if (     x>=0.0 && y> 0.0) return(1);
	else if( x< 0.0 && y>=0.0) return(2);
	else if( x<=0.0 && y< 0.0) return(3);
	else if( x> 0.0 && y<=0.0) return(4);
	else			   return 0;	/* shut up lint */
}

static double xmin, ymin, xmax, ymax;	// used by getDy

static void arc_extreme(double x0, double y0, double x1, double y1, double xc, double yc)
		/* start, end, center */
{		/* assumes center isn't too far out */
	double r;
	int j, k;
	printf("#start %g,%g, end %g,%g, ctr %g,%g\n", x0,y0, x1,y1, xc,yc);
	y0 = -y0; y1 = -y1; yc = -yc;	// troff's up is eric's down
	x0 -= xc; y0 -= yc;	/* move to center */
	x1 -= xc; y1 -= yc;
	xmin = (x0<x1)?x0:x1; ymin = (y0<y1)?y0:y1;
	xmax = (x0>x1)?x0:x1; ymax = (y0>y1)?y0:y1;
	r = sqrt(x0*x0 + y0*y0);
	if (r > 0.0) {
		j = quadrant(x0,y0);
		k = quadrant(x1,y1);
		if (j == k && y1*x0 < x1*y0) {
			/* viewed as complex numbers, if Im(z1/z0)<0, arc is big */
			if( xmin > -r) xmin = -r; if( ymin > -r) ymin = -r;
			if( xmax <  r) xmax =  r; if( ymax <  r) ymax =  r;
		} else {
			while (j != k) {
				switch (j) {
				case 1: if( ymax <  r) ymax =  r; break; /* north */
				case 2: if( xmin > -r) xmin = -r; break; /* west */
				case 3: if( ymin > -r) ymin = -r; break; /* south */
				case 4: if( xmax <  r) xmax =  r; break; /* east */
				}
				j = j%4 + 1;
			}
		}
	}
	xmin += xc; ymin += yc; ymin = -ymin;
	xmax += xc; ymax += yc; ymax = -ymax;
}


static int getDy(char *p, int *dx, int *maxv)
				// figure out where we are after a D'...'
{
	int x, y, x1, y1;	// for input values
	char temp[50];
	p++;		// get to command letter
	switch (*p++) {
	case 'l':	// line
		sscanf(p, "%d %d", dx, &y);
		return *maxv = y;
	case 'a':	// arc
		sscanf(p, "%d %d %d %d", &x, &y, &x1, &y1);
		*dx = x1 - x;
		arc_extreme(0, 0, x+x1, y+y1, x, y);	// sets [xy][max|min]
		printf("#arc bounds x %g, %g; y %g, %g\n",
			xmin, xmax, ymin, ymax);
		*maxv = (int) (ymin+0.5);
		return y + y1;
	case '~':	// spline
		for (*dx = *maxv = y = 0; (p=getstr(p, temp)) != NULL; ) {
						// above getstr() gets x value
			*dx += atoi(temp);
			p = getstr(p, temp);	// this one gets y value
			y += atoi(temp);
			*maxv = max(*maxv, y);	// ok???
			if (*p == '\n' || *p == 0)	// input is a single line;
				break;			// don't walk off end if realloc
		}
		return y;
	case 'c':	// circle, ellipse
		sscanf(p, "%d", dx);
		*maxv = *dx/2;		// high water mark is ht/2
		return 0;
	case 'e':
		sscanf(p, "%d %d", dx, &y);
		*maxv = y/2;		// high water mark is ht/2
		return 0;
	default:	// weird stuff
		return 0;
	}
}

static int serialnum = 0;

slug eofslug()
{
	slug ret;
	ret.serialnum = serialnum;
	ret.type = EOF;
	ret.dp = wherebuf();
	return ret;
}

slug getslug(FILE *fp)
{
	if (inbuf == NULL) {
		if ((inbuf = (char *) malloc(ninbuf = DELTABUF)) == NULL)
			FATAL("no room for %d character input buffer\n", ninbuf);
		inbp = inbuf;
	}
	if (wherebuf() > ninbuf-5000) {
		// this is still flaky -- lines can be very long
		int where = wherebuf();	// where we were
		if ((inbuf = (char *) realloc(inbuf, ninbuf += DELTABUF)) == NULL)
			FATAL("no room for %d character input buffer\n", ninbuf);
		WARNING("grew input buffer to %d characters\n", ninbuf);
		inbp = inbuf + where;	// same offset in new array
	}
	static int baseV = 0;	// first V command of preceding slug
	static int curV = 0, curH = 0;
	static int font = 0;
	static float size = 0;
	static int baseadj = 0;
	static int ncol = 1, offset = 0;	// multi-column stuff
	char str[4096], str2[4096], buf[4096], *p;
	int firstV = 0, firstH = 0;
	int maxV = curV;
	int ocurV = curV, mxv = 0, dx = 0;
	int sawD = 0;		// > 0 if have seen D...
	slug ret;
	ret.serialnum = serialnum++;
	ret.type = VBOX;	// use the same as last by default
	ret.dv = curV - baseV;
	ret.hpos = curH;
	ret.base =  ret.parm = ret.parm2 = ret.seen = 0;
	ret.font = font;
	ret.size = size;
	ret.dp = wherebuf();
	ret.ncol = ncol;
	ret.offset = offset;
	ret.linenum = linenum;	// might be low

	for (;;) {
		int c, m, n;	// for input values
		int sign;		// hoisted from case 'h' below
		switch (c = getc(fp)) {
		case EOF:
			ret.type = EOF;
			ret.dv = 0;
			if (baseadj)
				printf("# adjusted %d bases\n", baseadj);
			printf("# %d characters, %d lines\n", wherebuf(), linenum);
			return ret;
		case 'V':
			fscanf(fp, "%d", &n);
			if (firstV++ == 0) {
				ret.dv = n - baseV;
				baseV = n;
			} else {
				snprintf(buf, sizeof(buf), "v%d", n - curV);
				adds(buf);
			}
			curV = n;
			maxV = max(maxV, curV);
			break;
		case 'H':		// absolute H motion
			fscanf(fp, "%d", &n);
			if (firstH++ == 0) {
				ret.hpos = n;
			} else {
				snprintf(buf, sizeof(buf), "h%d", n - curH);
				adds(buf);
			}
			curH = n;
			break;
		case 'h':		// relative H motion
			addc(c);
			sign = 1;
			if ((c = getc(fp)) == '-') {
				addc(c);
				sign = -1;
				c = getc(fp);
			}
			for (n = 0; isdigit(c); c = getc(fp)) {
				addc(c);
				n = 10 * n + c - '0';
			}
			curH += n * sign;
			ungetc(c, fp);
			break;
		case 'x':	// device control: x ...
			addc(c);
			fgets(buf, (int) sizeof(buf), fp);
			linenum++;
			adds(buf);
			if (buf[0] == ' ' && buf[1] == 'X') {	// x X ...
				if (2 != sscanf(buf+2, "%s %d", str, &n))
					n = 0;
				if (eq(str, "SP")) {	// X SP n
					ret.type = SP;	// paddable SPace
					ret.dv = n;	// of height n
				} else if (eq(str, "BS")) {
					ret.type = BS;	// Breakable Stream
					ret.parm = n;	// >=n VBOXES on a page
				} else if (eq(str, "BF")) {
					ret.type = BF;	// Breakable Float
					ret.parm = ret.parm2 = n;
							// n = pref center (as UF)
				} else if (eq(str, "US")) {
					ret.type = US;	// Unbreakable Stream
					ret.parm = n;
				} else if (eq(str, "UF")) {
					ret.type = UF;	// Unbreakable Float
					ret.parm = ret.parm2 = n;
							// n = preferred center
							// to select several,
							// use several UF lines
				} else if (eq(str, "PT")) {
					ret.type = PT;	// Page Title
					ret.parm = n;
				} else if (eq(str, "BT")) {
					ret.type = BT;	// Bottom Title
					ret.parm = n;
				} else if (eq(str, "END")) {
					ret.type = END;
					ret.parm = n;
				} else if (eq(str, "TM")) {
					ret.type = TM;	// Terminal Message
					ret.dv = 0;
				} else if (eq(str, "COORD")) {
					ret.type = COORD;// page COORDinates
					ret.dv = 0;
				} else if (eq(str, "NE")) {
					ret.type = NE;	// NEed to break page
					ret.dv = n;	// if <n units left
				} else if (eq(str, "MC")) {
					ret.type = MC;	// Multiple Columns
					sscanf(buf+2, "%s %d %d",
						str, &ncol, &offset);
					ret.ncol = ncol;
					ret.offset = offset;
				} else if (eq(str, "CMD")) {
					ret.type = CMD;	// CoMmaNd
					sscanf(buf+2, "%s %s", str2, str);
					if (eq(str, "FC"))	// Freeze 2-Col
						ret.parm = FC;
					else if (eq(str, "FL"))	// FLush
						ret.parm = FL;
					else if (eq(str, "BP"))	// Break Page
						ret.parm = BP;
					else WARNING("unknown command %s\n",
						str);
				} else if (eq(str, "PARM")) {
					ret.type = PARM;// PARaMeter
					sscanf(buf+2, "%s %s %d", str2, str, &ret.parm2);
					if (eq(str, "NP"))	// New Page
						ret.parm = NP;
					else if (eq(str, "FO"))	// FOoter
						ret.parm = FO;
					else if (eq(str, "PL")) // Page Length
						ret.parm = PL;
					else if (eq(str, "MF")) // MinFull
						ret.parm = MF;
					else if (eq(str, "CT")) // ColTol
						ret.parm = CT;
					else if (eq(str, "WARN")) //WARNings?
						ret.parm = WARN;
					else if (eq(str, "DBG"))// DeBuG
						ret.parm = DBG;
					else WARNING("unknown parameter %s\n",
						str);
				} else
					break;		// out of switch
				if (firstV > 0)
					WARNING("weird x X %s in mid-VBOX\n",
						str);
				for (;;) {
					c = getc(fp);
					ungetc(c, fp);
					if (c != '+')
						break;
					fgets(buf, (int) sizeof(buf), fp);
					linenum++;
					adds(buf);
				}
				return ret;
			}
			break;
		case 'n':	// end of line
			fscanf(fp, "%d %d", &n, &m);
			ret.ht = n;
			ret.base = m;
			getc(fp);	// newline
			linenum++;
			snprintf(buf, sizeof(buf), "n%d %d\n", ret.ht,
			    ret.base);
			adds(buf);
			if (!firstV++)
				baseV = curV;
			// older incarnations of this program used ret.base
			// in complicated and unreliable ways;
			// example:  if ret.ht + ret.base < ret.dv, ret.base = 0
			// this was meant to avoid double-counting the space
			// around displayed equations; it didn't work
			// Now, we believe ret.base = 0, otherwise we give it
			// a value we have computed.
			if (ret.base == 0 && sawD == 0)
				return ret;	// don't fiddle 0-bases
			if (ret.base != maxV - baseV) {
				ret.base = maxV - baseV;
				baseadj++;
			}
			if (ret.type != VBOX)
				WARNING("%s slug (type %d) has base = %d\n",
					ret.typname(), ret.type, ret.base);
			return ret;
		case 'p':	// new page
			fscanf(fp, "%d", &n);
			ret.type = PAGE;
			curV = baseV = ret.dv = 0;
			ret.parm = n;	// just in case someone needs it
			return ret;
		case 's': {	// size change snnn
				int	isize;
				fscanf(fp, "%d", &isize);
				if (isize == -23) {
					fscanf(fp, "%f", &size);
					snprintf(buf, sizeof(buf),
					    "s-23 %g\n", size);
				} else {
					size = isize;
					snprintf(buf, sizeof(buf),
					    "s%d\n", isize);
				}
				adds(buf);
			}
			break;
		case 'f':	// font fnnn
			fscanf(fp, "%d", &font);
			snprintf(buf, sizeof(buf), "f%d\n", font);
			adds(buf);
			break;
		case '\n':
			linenum++;
			/* fall through */
		case ' ':
			addc(c);
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			// two motion digits plus a character
			addc(c);
			n = c - '0';
			addc(c = getc(fp));
			curH += 10 * n + c - '0';
			adds(getutf(fp));
			if (!firstV++)
				baseV = curV;
			break;
		case 'c':	// single ascii character
			addc(c);
			adds(getutf(fp));
			if (!firstV++)
				baseV = curV;
			break;
		case 'C':	// Cxyz\n
		case 'N':	// Nnnn\n
			addc(c);
			while ((c = getc(fp)) != ' ' && c != '\n')
				addc(c);
			addc(c);
			if (!firstV++)
				baseV = curV;
			linenum++;
			break;
		case 'D':	// draw function: D.*\n
			sawD++;
			p = bufptr(wherebuf());	// where does the D start
			addc(c);
			while ((c = getc(fp)) != '\n')
				addc(c);
			addc(c);
			if (!firstV++)
				baseV = curV;
			ocurV = curV, mxv = 0, dx = 0;
			curV += getDy(p, &dx, &mxv);	// figure out how big it is
			maxV = max(max(maxV, curV), ocurV+mxv);
			curH += dx;
			linenum++;
			break;
		case 'v':	// relative vertical vnnn
			addc(c);
			if (!firstV++)
				baseV = curV;
			sign = 1;
			if ((c = getc(fp)) == '-') {
				addc(c);
				sign = -1;
				c = getc(fp);
			}
			for (n = 0; isdigit(c); c = getc(fp)) {
				addc(c);
				n = 10 * n + c - '0';
			}
			ungetc(c, fp);
			curV += n * sign;
			maxV = max(maxV, curV);
			addc('\n');
			break;
		case 'w':	// word space
			addc(c);
			break;
		case '#':	// comment
			addc(c);
			while ((c = getc(fp)) != '\n')
				addc(c);
			addc('\n');
			linenum++;
			break;
		default:
			WARNING("unknown input character %o %c (%50.50s)\n",
				c, c, bufptr(wherebuf()-50));
			break;
		}
	}
}
