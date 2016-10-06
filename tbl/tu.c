/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */
  
/*	from OpenSolaris "tu.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 * Portions Copyright (c) 2015 Carsten Kunze
 *
 * Sccsid @(#)tu.c	1.4 (gritter) 9/8/06
 */

 /* tu.c: draws horizontal lines */
# include "t..c"
# include <string.h>

/*           0   1   2
 * lintype   -   =
 * tl            |   ||
 * bl            |   ||
 * c         |<  ~   >|  */

static char *udbdc[2][3][3][3] = { /* vs. uhbdc */
	{ { { NULL                ,       /* 0000 */
	      NULL                ,       /* 0001 */
	      NULL                } ,     /* 0002 */
	    { "\\U'250C'" /* ┌ */ ,       /* 0010 */
	      "\\U'252C'" /* ┬ */ ,       /* 0011 */
	      "\\U'2510'" /* ┐ */ } ,     /* 0012 */
	    { "\\U'2553'" /* ╓ */ ,       /* 0020 */
	      "\\U'2565'" /* ╥ */ ,       /* 0021 */
	      "\\U'2556'" /* ╖ */ } } ,   /* 0022 */
	  { { "\\U'2514'" /* └ */ ,       /* 0100 */
	      "\\U'2534'" /* ┴ */ ,       /* 0101 */
	      "\\U'2518'" /* ┘ */ } ,     /* 0102 */
	    { "\\U'251c'" /* ├ */ ,       /* 0110 */
	      "\\U'253c'" /* ┼ */ ,       /* 0111 */
	      "\\U'2524'" /* ┤ */ } ,     /* 0112 */
	    { "\\U'2553'" /* ╓ */ ,       /* 0120 ! */
	      "\\U'2565'" /* ╥ */ ,       /* 0121 ! */
	      "\\U'2556'" /* ╖ */ } } ,   /* 0122 ! */
	  { { "\\U'2559'" /* ╙ */ ,       /* 0200 */
	      "\\U'2568'" /* ╨ */ ,       /* 0201 */
	      "\\U'255C'" /* ╜ */ } ,     /* 0202 */
	    { "\\U'2559'" /* ╙ */ ,       /* 0210 ! */
	      "\\U'2568'" /* ╨ */ ,       /* 0211 ! */
	      "\\U'255C'" /* ╜ */ } ,     /* 0212 ! */
	    { "\\U'255F'" /* ╟ */ ,       /* 0220 */
	      "\\U'256B'" /* ╫ */ ,       /* 0221 */
	      "\\U'2562'" /* ╢ */ } } } , /* 0222 */
	{ { { NULL                ,       /* 1000 */
	      NULL                ,       /* 1001 */
	      NULL                } ,     /* 1002 */
	    { "\\U'2552'" /* ╒ */ ,       /* 1010 */
	      "\\U'2564'" /* ╤ */ ,       /* 1011 */
	      "\\U'2555'" /* ╕ */ } ,     /* 1012 */
	    { "\\U'2554'" /* ╔ */ ,       /* 1020 */
	      "\\U'2566'" /* ╦ */ ,       /* 1021 */
	      "\\U'2557'" /* ╗ */ } } ,   /* 1022 */
	  { { "\\U'2558'" /* ╘ */ ,       /* 1100 */
	      "\\U'2567'" /* ╧ */ ,       /* 1101 */
	      "\\U'255B'" /* ╛ */ } ,     /* 1102 */
	    { "\\U'255E'" /* ╞ */ ,       /* 1110 */
	      "\\U'256A'" /* ╪ */ ,       /* 1111 */
	      "\\U'2561'" /* ╡ */ } ,     /* 1112 */
	    { "\\U'2554'" /* ╔ */ ,       /* 1120 ! */
	      "\\U'2566'" /* ╦ */ ,       /* 1121 ! */
	      "\\U'2557'" /* ╗ */ } } ,   /* 1122 ! */
	  { { "\\U'255A'" /* ╚ */ ,       /* 1200 */
	      "\\U'2569'" /* ╩ */ ,       /* 1201 */
	      "\\U'255D'" /* ╝ */ } ,     /* 1202 */
	    { "\\U'255A'" /* ╚ */ ,       /* 1210 ! */
	      "\\U'2569'" /* ╩ */ ,       /* 1211 ! */
	      "\\U'255D'" /* ╝ */ } ,     /* 1212 ! */
	    { "\\U'2560'" /* ╠ */ ,       /* 1220 */
	      "\\U'256C'" /* ╬ */ ,       /* 1221 */
	      "\\U'2563'" /* ╣ */ } } }   /* 1222 */
};

static char *grbe(int i, int lintype);
static char *glibe(int, int, int, int, int);

void
makeline(int i, int c, int lintype)
{
	int cr, type, shortl;
	type = thish(i,c);
	if (type==0) return;
	cr=c;
	shortl = (table[i][c].col[0]=='\\');
	if (c>0 && !shortl && thish(i,c-1) == type)return;
	if (shortl==0)
		for(cr=c; cr < ncol && (ctype(i,cr)=='s'||type==thish(i,cr)); cr++);
	else
		for(cr=c+1; cr<ncol && ctype(i,cr)=='s'; cr++);
	drawline(i, c, cr-1, lintype, 0, shortl);
}
void
fullwide(int i, int lintype)
{
	int cr, cl;
	if (!nflm)
		fprintf(tabout, ".nr %d \\n(.v\n.vs \\n(.vu-\\n(.sp\n", SVS);
	cr= 0;
	while (cr<ncol)
	{
		cl=cr;
		while (i>0 && vspand(prev(i),cl,1))
			cl++;
		for(cr=cl; cr<ncol; cr++)
			if (i>0 && vspand(prev(i),cr,1))
				break;
		if (cl<ncol)
			drawline(i,cl,(cr<ncol?cr-1:cr),lintype,1,0);
	}
	fprintf(tabout, "\n");
	if (!nflm)
		fprintf(tabout, ".vs \\n(%du\n", SVS);
}

void
drawline(int i, int cl, int cr, int lintype, int noheight, int shortl)
{
	char *exhr, *exhl, *lnch;
	int lcount, ln, linpos, oldpos, nodata;
	lcount=0;
	exhr=exhl= "";
	switch(lintype)
	{
	case '-': lcount=1;break;
	case '=': lcount = nflm ? 1 : 2; break;
	case SHORTLINE: lcount=1; break;
	}
	if (lcount<=0) return;
	nodata = cr-cl>=ncol || noheight || allh(i);
	if (!nflm) {
		if (!nodata)
			fprintf(tabout, "\\v'-.5m'");
		if (graphics)
			fprintf(tabout, "\\v'\\n(#Du'");
	}
	for(ln=oldpos=0; ln<lcount; ln++)
	{
		linpos = 2*ln - lcount +1;
		if (linpos != oldpos)
			fprintf(tabout, "\\v'%dp'", linpos-oldpos);
		oldpos=linpos;
		if (shortl==0)
		{
			tohcol(cl);
			if (lcount>1)
			{
				switch(interv(i,cl))
				{
				case TOP: exhl = ln==0 ? "1p" : "-1p"; break;
				case BOT: exhl = ln==1 ? "1p" : "-1p"; break;
				case THRU: exhl = "1p"; break;
				}
				if (exhl[0])
					fprintf(tabout, "\\h'%s'", exhl);
			}
			else if (lcount==1)
			{
				switch(interv(i,cl))
				{
				case TOP: case BOT: exhl = "-1p"; break;
				case THRU: exhl = "1p"; break;
				}
				if (exhl[0])
					fprintf(tabout, "\\h'%s'", exhl);
			}
			if (lcount>1)
			{
				switch(interv(i,cr+1))
				{
				case TOP: exhr = ln==0 ? "-1p" : "+1p"; break;
				case BOT: exhr = ln==1 ? "-1p" : "+1p"; break;
				case THRU: exhr = "-1p"; break;
				}
			}
			else if (lcount==1)
			{
				switch(interv(i,cr+1))
				{
				case TOP: case BOT: exhr = "+1p"; break;
				case THRU: exhr = "-1p"; break;
				}
			}
		}
		else
			fprintf(tabout, "\\h'|\\n(%du'", cl+CLEFT);
		if (!graphics)
			fprintf(tabout, "\\s\\n(%d",LSIZE);
		if (linsize)
			fprintf(tabout, "\\v'-\\n(%dp/6u'", LSIZE);
		if (shortl) {
			if (graphics)
				fprintf(tabout, "\\D'l |\\n(%du 0'", cr+CRIGHT);
			else
				fprintf(tabout, "\\l'|\\n(%du%s'", cr+CRIGHT,
				    utf8 ?  "\\U'2500'" : /* ─ */
				    tlp  ?  "\\-"       :
				            "");
		}
		else if (graphics) {
			if (cr+1>=ncol)
				fprintf(tabout, "\\D'l |\\n(TWu%s 0'", exhr);
			else
				fprintf(tabout, "\\D'l (|\\n(%du+|\\n(%du)/2u%s 0'",
						cr+CRIGHT, cr+1+CLEFT, exhr);
		}
		else
		{
			lnch = "\\(ul";
			if (utf8)
				lnch = lintype == '=' ? "\\U'2550'" : /* ═ */
				                        "\\U'2500'" ; /* ─ */
			else if (tlp)
				lnch = lintype == '=' ? "\\&=" : "\\-";
			else
			if (pr1403)
				lnch = lintype == '=' ? "\\&=" : "\\(ru";
			if (cr+1>=ncol)
				fprintf(tabout, "\\l'|\\n(TWu%s%s'", exhr,lnch);
			else
				fprintf(tabout, "\\l'(|\\n(%du+|\\n(%du)/2u%s%s'", cr+CRIGHT,
					cr+1+CLEFT, exhr, lnch);
		}
		if (linsize)
			fprintf(tabout, "\\v'\\n(%dp/6u'", LSIZE);
		if (!graphics)
			fprintf(tabout, "\\s0");
	}
	if (oldpos!=0)
		fprintf(tabout, "\\v'%dp'", -oldpos);
	if (graphics)
		fprintf(tabout, "\\v'-\\n(#Du'");
	if (!nodata)
		fprintf(tabout, "\\v'+.5m'");
	if (!shortl && (utf8 || tlp)) {
		int corred, c, ccr, licr;
		char *s;
		ccr = cr;
		if (ccr == cl) ccr++;
		corred = 0;
		if (ccr == ncol && (boxflg || allflg || dboxflg) &&
		    (s = grbe(i, lintype))) {
			fprintf(tabout, "\n.sp -1\n");
			corred = 1;
			fprintf(tabout, "\\h'|\\n(TWu'");
			fprintf(tabout, "%s", s);
		}
		licr = ccr;
		if (licr == ncol) {
			licr--;
			if (!(boxflg || allflg || dboxflg)) ccr--;
		}
		for(c = cl; c <= licr; c++) {
			if ((s = glibe(i, c, cl, ccr, lintype))) {
				if (!corred) {
					fprintf(tabout, "\n.sp -1\n");
					corred = 1;
				}
				tohcol(c);
				fprintf(tabout, "%s", s);
			}
		}
	}
}

static char *glibe(int i, int c, int cl, int cr, int lintype) {
	char *s = NULL;
	int tl, bl;
	int cx = c == cl ? 0 :
	         c == cr ? 2 : 1 ;
	lintype = lintype == '=' ? 1 : 0;
	if (!i) {
		bl = lefdata(1, c);
		if (bl >= 1 && bl <= 2)
			s = tlp ? "+" :
			    udbdc[lintype][0][bl][cx];
	} else if (i < nlin - 1) {
		tl = lefdata(i - 1, c);
		bl = lefdata(i + 1, c);
		if (tl >= 0 && tl <= 2 && bl >= 0 && bl <= 2)
		{
			if (tlp) {
				if (tl || bl) s = "+";
			} else
				s = udbdc[lintype][tl][bl][cx];
		}
	} else {
		tl = lefdata(i - 1, c);
		if (tl >= 1 && tl <= 2)
			s = tlp ? "+" :
			    udbdc[lintype][tl][0][cx];
	}
	return s;
}

static char *grbe(int i, int lintype) {
	int tl, bl;
	lintype = lintype == '=' ? 1 : 0;
	tl = !i      ? 0 :
	     dboxflg ? 2 : 1;
	bl = i && i >= nlin - 1 ? 0 :
	     dboxflg            ? 2 : 1;
	if (utf8) return udbdc[lintype][tl][bl][2];
	else if (tl || bl) return "+";
	else return NULL;
}

void
getstop(void)
{
int i,c,k,junk, stopp;
stopp=1;
for(i=0; i<MAXLIN; i++)
	linestop[i]=0;
for(i=0; i<nlin; i++)
	for(c=0; c<ncol; c++)
		{
		k = left(i,c,&junk);
		if (k>=0 && linestop[k]==0)
			linestop[k]= ++stopp;
		}
if (boxflg || allflg || dboxflg)
	linestop[0]=1;
}
int 
left(int i, int c, int *lwidp)
{
	int kind, li = 0, lj;
	/* returns -1 if no line to left */
	/* returns number of line where it starts */
	/* stores into lwid the kind of line */
	*lwidp=0;
	kind = lefdata(i,c);
	if (kind==0) return(-1);
	if (i+1<nlin)
		if (lefdata(next(i),c)== kind) return(-1);
	while (i>=0 && lefdata(i,c)==kind)
		i=prev(li=i);
	if (prev(li)== -1) li=0;
	*lwidp=kind;
	for(lj= i+1; lj<li; lj++)
		if (instead[lj] && strcmp(instead[lj], ".TH")==0)
			return(li);
	for(i= i+1; i<li; i++)
		if (fullbot[i])
			li=i;
	return(li);
}
int 
lefdata(int i, int c)
{
	int ck;
	if (i>=nlin) i=nlin-1;
	if (ctype(i,c) == 's')
	{
		for(ck=c; ctype(i,ck)=='s'; ck--);
		if (thish(i,ck)==0)
			return(0);
	}
	i =stynum[i];
	i = lefline[i][c];
	if (i>0) return(i);
	if (dboxflg && c==0) return(2);
	if (allflg)return(1);
	if (boxflg && c==0) return(1);
	return(0);
}
int 
next(int i)
{
while (i+1 <nlin)
	{
	i++;
	if (!fullbot[i] && !instead[i]) break;
	}
return(i);
}
int 
prev(int i)
{
while (--i >=0  && (fullbot[i] || instead[i]))
	;
return(i);
}
