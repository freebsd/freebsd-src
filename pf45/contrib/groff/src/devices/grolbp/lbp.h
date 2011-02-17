// -*- C -*-
/* Copyright (C) 1994, 2000, 2001, 2003, 2004, 2005
   Free Software Foundation, Inc.
     Written by Francisco Andrés Verdú <pandres@dragonet.es>

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

/*  This file contains a set of utility functions to use canon CAPSL printers
 *  (lbp-4 and lbp-8 series printers) */

#ifndef LBP_H
#define LBP_H

#include <stdio.h>
#include <stdarg.h>

static FILE *lbpoutput = NULL;
static FILE *vdmoutput = NULL;


static inline void 
lbpinit(FILE *outfile)
{
	lbpoutput = outfile;
}


static void 
lbpprintf(const char *format, ... )
{ /* Taken from cjet */
  va_list stuff;

  va_start(stuff, format);
  vfprintf(lbpoutput, format, stuff);
  va_end(stuff);
}


static inline void
lbpputs(const char *data)
{
	fputs(data,lbpoutput);
}


static inline void
lbpputc(unsigned char c)
{
	fputc(c,lbpoutput);
}


static inline void 
lbpsavestatus(int idx )
{
	fprintf(lbpoutput,"\033[%d%%y",idx);
}


static inline void 
lbprestorestatus(int idx )
{
	fprintf(lbpoutput,"\033[%d%cz",idx ,'%');
}


static inline void 
lbpsavepos(int idx)
{
	fprintf(lbpoutput,"\033[1;%d;0x",idx);
}


static inline void 
lbprestorepos(int idx)
{
	fprintf(lbpoutput,"\033[0;%d;0x",idx);
}


static inline void 
lbprestoreposx(int idx)
{
	fprintf(lbpoutput,"\033[0;%d;1x",idx);
}


static inline void 
lbpmoverel(int despl, char direction)
{
	fprintf(lbpoutput,"\033[%d%c",despl,direction);
}


static inline void 
lbplinerel(int width,int despl,char direction )
{
	fprintf(lbpoutput,"\033[%d;0;9{\033[%d%c\033[9}",width,despl,direction);
}


static inline void 
lbpmoveabs(int x, int y)
{
	fprintf(lbpoutput,"\033[%d;%df",y,x);
}


static inline void
lbplineto(int x,int y, int width )
{
	fprintf(lbpoutput,"\033[%d;0;9{",width);
	lbpmoveabs(x,y);
	fprintf(lbpoutput,"\033[9}\n");
}


static inline void
lbpruleabs(int x, int y, int hsize, int vsize)
{
	lbpmoveabs(x,y);
	fprintf(lbpoutput,"\033[0;9;000s");
	lbpmoveabs(x+hsize,y+vsize);
	fprintf(lbpoutput,"\033[9r");
}


static void vdmprintf(const char *format, ... );


static inline char *
vdmnum(int num,char *result)
{
  char b1,b2,b3;
  char *p = result;
  int nm;

  nm = abs(num);
  /* First byte 1024 - 32768 */
  b1 = ((nm >> 10) & 0x3F);
  if (b1) *p++ = b1 | 0x40;

  /* Second Byte 16 - 1024 */
  b2 = ((nm >> 4) & 0x3F);
  if ( b1 || b2) *p++= b2 | 0x40;

  /* Third byte 0 - 15 */
  b3 = ((nm & 0x0F) | 32);
  if (num >= 0) b3 |= 16;
  *p++ = b3;
  *p = 0x00; /* End of the resulting string */
  return result;
}


static inline void
vdmorigin(int newx, int newy)
{
   char nx[4],ny[4];

	vdmprintf("}\"%s%s\x1e",vdmnum(newx,nx),vdmnum(newy,ny));
}


static inline FILE *
vdminit(FILE *vdmfile)
{
  char scale[4],size[4],lineend[4];
  
/*  vdmoutput = tmpfile();*/
  vdmoutput = vdmfile;
  /* Initialize the VDM mode */
  vdmprintf("\033[0&}#GROLBP\x1e!0%s%s\x1e$\x1e}F%s\x1e",\
		  vdmnum(-3,scale),vdmnum(1,size),vdmnum(1,lineend));
  return vdmoutput;
  
}


static inline void
vdmend()
{
	vdmprintf("}p\x1e");
}


static void 
vdmprintf(const char *format, ... )
{ /* Taken from cjet */
  va_list stuff;
  
  if (vdmoutput == NULL)  vdminit(tmpfile());
  va_start(stuff, format);
  vfprintf(vdmoutput, format, stuff);
  va_end(stuff);
}


static inline void
vdmsetfillmode(int pattern,int perimeter, int inverted)
{
   char patt[4],perim[4],
   	rot[4], /* rotation */
	espejo[4], /* espejo */
	inv[4]; /* Inverted */

   	vdmprintf("I%s%s%s%s%s\x1e",vdmnum(pattern,patt),\
		vdmnum(perimeter,perim),vdmnum(0,rot),
		vdmnum(0,espejo),vdmnum(inverted,inv));
}


static inline void
vdmcircle(int centerx, int centery, int radius)
{
  char x[4],y[4],rad[4];
  
	vdmprintf("5%s%s%s\x1e",vdmnum(centerx,x),vdmnum(centery,y),\
			vdmnum(radius,rad));
}


static inline void
vdmaarc(int centerx, int centery, int radius,int startangle,int angle,int style,int arcopen)
{
  char x[4],y[4],rad[4],stx[4],sty[4],styl[4],op[4];
  
	vdmprintf("}6%s%s%s%s%s%s%s\x1e",vdmnum(arcopen,op),\
		vdmnum(centerx,x),vdmnum(centery,y),\
		vdmnum(radius,rad),vdmnum(startangle,stx),vdmnum(angle,sty),\
		vdmnum(style,styl));
}


static inline void
vdmvarc(int centerx, int centery,int radius, int startx, int starty, int endx, int endy,\
	int style,int arcopen)
{
  char x[4],y[4],rad[4],stx[4],sty[4],enx[4],eny[4],styl[4],op[4];
  
	vdmprintf("}6%s%s%s%s%s%s%s%s%s\x1e",vdmnum(arcopen,op),\
		vdmnum(centerx,x),vdmnum(centery,y),\
		vdmnum(radius,rad),vdmnum(startx,stx),vdmnum(starty,sty),\
		vdmnum(endx,enx),vdmnum(endy,eny),vdmnum(style,styl));
}


static inline void
vdmellipse(int centerx, int centery, int radiusx, int radiusy,int rotation)
{
  char x[4],y[4],radx[4],rady[4],rotat[4];
  
	vdmprintf("}7%s%s%s%s%s\x1e\n",vdmnum(centerx,x),vdmnum(centery,y),\
			vdmnum(radiusx,radx),vdmnum(radiusy,rady),\
			vdmnum(rotation,rotat));
}


static inline void
vdmsetlinetype(int lintype)
{
  char ltyp[4], expfact[4];

  vdmprintf("E1%s%s\x1e",vdmnum(lintype,ltyp),vdmnum(1,expfact));
  
}


static inline void
vdmsetlinestyle(int lintype, int pattern,int unionstyle)
{
   char patt[4],ltip[4],
   	rot[4], /* rotation */
	espejo[4], /* espejo */
	in[4]; /* Inverted */

   	vdmprintf("}G%s%s%s%s%s\x1e",vdmnum(lintype,ltip),\
		vdmnum(pattern,patt),vdmnum(0,rot),
		vdmnum(0,espejo),vdmnum(0,in));
	vdmprintf("}F%s",vdmnum(unionstyle,rot));
}


static inline void
vdmlinewidth(int width)
{
  char wh[4];

  	vdmprintf("F1%s\x1e",vdmnum(width,wh));
}


static inline void
vdmrectangle(int origx, int origy,int dstx, int dsty)
{
  char xcoord[4],ycoord[4],sdstx[4],sdsty[4];

  vdmprintf("}:%s%s%s%s\x1e\n",vdmnum(origx,xcoord),vdmnum(dstx,sdstx),\
		  vdmnum(origy,ycoord),vdmnum(dsty,sdsty));
}


static inline void
vdmpolyline(int numpoints, int *points)
{
  int i,*p = points;
  char xcoord[4],ycoord[4];

  if (numpoints < 2) return;
  vdmprintf("1%s%s",vdmnum(*p,xcoord),vdmnum(*(p+1),ycoord));
  p += 2;
  for (i = 1; i < numpoints ; i++) {
	  vdmprintf("%s%s",vdmnum(*p,xcoord),vdmnum(*(p+1),ycoord));
	  p += 2;
  } /* for */
  vdmprintf("\x1e\n");
}
	 

static inline void
vdmpolygon(int numpoints, int *points)
{
  int i,*p = points;
  char xcoord[4],ycoord[4];

  if (numpoints < 2) return;
  vdmprintf("2%s%s",vdmnum(*p,xcoord),vdmnum(*(p+1),ycoord));
  p += 2;
  for (i = 1; i < numpoints ; i++) {
	  vdmprintf("%s%s",vdmnum(*p,xcoord),vdmnum(*(p+1),ycoord));
	  p += 2;
  } /* for */
  vdmprintf("\x1e\n");

}


/************************************************************************
 *		Highter level auxiliary functions			*
 ************************************************************************/		
static inline int
vdminited()
{
	return (vdmoutput != NULL);
}


static inline void
vdmline(int startx, int starty, int sizex, int sizey)
{
  int points[4];

  points[0] = startx;
  points[1] = starty;
  points[2] = sizex;
  points[3] = sizey;

  vdmpolyline(2,points);

}


/*#define         THRESHOLD       .05    */ /* inch */
#define         THRESHOLD       1     /* points (1/300 inch) */
static inline void
splinerel(double px,double py,int flush)
{
  static int lx = 0 ,ly = 0;
  static double pend = 0.0;
  static int dy = 0, despx = 0, despy = 0, sigpend = 0;
  int dxnew = 0, dynew = 0, sg;
  char xcoord[4],ycoord[4];
  double npend ;

  if (flush == -1) {lx = (int)px; ly = (int)py; return;}

  if (flush == 0) {
  dxnew = (int)px -lx;
  dynew = (int)py -ly;
  if ((dxnew == 0) && (dynew == 0)) return;
  sg = (dxnew < 0)? -1 : 0;
/*  fprintf(stderr,"s (%d,%d) (%d,%d)\n",dxnew,dynew,despx,despy);*/
  if (dynew == 0) { 
	  despx = dxnew; 
	  if ((sg == sigpend) && (dy == 0)){
		  return;
	  }
	dy = 0;
  }
  else {
	  dy = 1;
	npend = (1.0*dxnew)/dynew;
  	if (( npend == pend) && (sigpend == sg))
  	{ despy = dynew; despx = dxnew; return; }
  	else
  	{ sigpend = sg;
    	pend = npend;
  	} /* else (( npend == pend) && ... */
  } /* else (if (dynew == 0)) */
  } /* if (!flush ) */

  /* if we've changed direction we must draw the line */
/*  fprintf(stderr," (%d) %.2f,%.2f\n",flush,(float)px,(float)py);*/
  if ((despx != 0) || (despy != 0)) vdmprintf("%s%s",vdmnum(despx,xcoord),\
		vdmnum(despy,ycoord));
  /*if ((despx != 0) || (despy != 0)) fprintf(stderr,"2
   *%d,%d\n",despx,despy);*/
  if (flush) {
  	dxnew = dy = despx = despy = 0;
	return;
  } /* if (flush) */
  dxnew -= despx;
  dynew -= despy;
  if ((dxnew != 0) || (dynew != 0)) vdmprintf("%s%s",vdmnum(dxnew,xcoord),\
		vdmnum(dynew,ycoord));
  
/*  if ((dxnew != 0) || (dynew != 0)) fprintf(stderr,"3
 *  %d,%d\n",dxnew,dynew);*/
  lx = (int)px; ly = (int)py; 
  dxnew = dy = despx = despy = 0;
  
}


/**********************************************************************
 *  The following code to draw splines is adapted from the transfig package
 */
static void
quadratic_spline(double a_1, double b_1, double a_2, double b_2, \
		 double a_3, double b_3, double a_4, double b_4)
{
	double	x_1, y_1, x_4, y_4;
	double	x_mid, y_mid;

	x_1	 = a_1; y_1 = b_1;
	x_4	 = a_4; y_4 = b_4;
	x_mid	 = (a_2 + a_3)/2.0;
	y_mid	 = (b_2 + b_3)/2.0;
	if ((fabs(x_1 - x_mid) < THRESHOLD)
	    && (fabs(y_1 - y_mid) < THRESHOLD)) {
        	splinerel(x_mid, y_mid, 0);
/*	    fprintf(tfp, "PA%.4f,%.4f;\n", x_mid, y_mid);*/
	}
	else {
	    quadratic_spline(x_1, y_1, ((x_1+a_2)/2.0), ((y_1+b_2)/2.0),
		((3.0*a_2+a_3)/4.0), ((3.0*b_2+b_3)/4.0), x_mid, y_mid);
	}

	if ((fabs(x_mid - x_4) < THRESHOLD)
	    && (fabs(y_mid - y_4) < THRESHOLD)) {
		splinerel(x_4, y_4, 0);
/*	    fprintf(tfp, "PA%.4f,%.4f;\n", x_4, y_4);*/
	}
	else {
	    quadratic_spline(x_mid, y_mid,
			     ((a_2+3.0*a_3)/4.0), ((b_2+3.0*b_3)/4.0),
			     ((a_3+x_4)/2.0), ((b_3+y_4)/2.0), x_4, y_4);
	}
}


#define XCOORD(i) numbers[(2*i)]
#define YCOORD(i) numbers[(2*i)+1]
static void
vdmspline(int numpoints, int o_x, int o_y, int *numbers)
{
	double	cx_1, cy_1, cx_2, cy_2, cx_3, cy_3, cx_4, cy_4;
	double	x_1, y_1, x_2, y_2;
	char xcoord[4],ycoord[4];
	int i;

	/*p	 = s->points;
	x_1	 = p->x/ppi;*/
	x_1	 = o_x;
	y_1	 = o_y;
/*	p	 = p->next;
	x_2	 = p->x/ppi;
	y_2	 = p->y/ppi;*/
	x_2	 = o_x + XCOORD(0);
	y_2	 = o_y + YCOORD(0);
	cx_1	 = (x_1 + x_2)/2.0;
	cy_1	 = (y_1 + y_2)/2.0;
	cx_2	 = (x_1 + 3.0*x_2)/4.0;
	cy_2	 = (y_1 + 3.0*y_2)/4.0;

/*	fprintf(stderr,"Spline %d (%d,%d)\n",numpoints,(int)x_1,(int)y_1);*/
    	vdmprintf("1%s%s",vdmnum((int)x_1,xcoord),vdmnum((int)y_1,ycoord));
	splinerel(x_1,y_1,-1);
	splinerel(cx_1,cy_1,0);
/*	    fprintf(tfp, "PA%.4f,%.4f;PD%.4f,%.4f;\n",
		    x_1, y_1, cx_1, cy_1);*/

	/*for (p = p->next; p != NULL; p = p->next) {*/
	for (i = 1; i < (numpoints); i++) {
	    x_1	 = x_2;
	    y_1	 = y_2;
/*	    x_2	 = p->x/ppi;
	    y_2	 = p->y/ppi;*/
            x_2	 = x_1 + XCOORD(i);
            y_2	 = y_1 + YCOORD(i);
	    cx_3 = (3.0*x_1 + x_2)/4.0;
	    cy_3 = (3.0*y_1 + y_2)/4.0;
	    cx_4 = (x_1 + x_2)/2.0;
	    cy_4 = (y_1 + y_2)/2.0;
	    /* fprintf(stderr,"Point (%d,%d) - (%d,%d)\n",(int)x_1,(int)(y_1),(int)x_2,(int)y_2);*/
	    quadratic_spline(cx_1, cy_1, cx_2, cy_2, cx_3, cy_3, cx_4, cy_4);
	    cx_1 = cx_4;
	    cy_1 = cy_4;
	    cx_2 = (x_1 + 3.0*x_2)/4.0;
	    cy_2 = (y_1 + 3.0*y_2)/4.0;
	}
	x_1	= x_2; 
	y_1	= y_2;
/*	p	= s->points->next;
	x_2	= p->x/ppi;
	y_2	= p->y/ppi;*/
        x_2     = o_x + XCOORD(0);
        y_2     = o_y + YCOORD(0);
	cx_3	= (3.0*x_1 + x_2)/4.0;
	cy_3	= (3.0*y_1 + y_2)/4.0;
	cx_4	= (x_1 + x_2)/2.0;
	cy_4	= (y_1 + y_2)/2.0;
	splinerel(x_1, y_1, 0);
	splinerel(x_1, y_1, 1);
       	/*vdmprintf("%s%s",vdmnum((int)(x_1-lx),xcoord),\
			vdmnum((int)(y_1-ly),ycoord));*/
        vdmprintf("\x1e\n");
/*	    fprintf(tfp, "PA%.4f,%.4f;PU;\n", x_1, y_1);*/


}
	

#endif
