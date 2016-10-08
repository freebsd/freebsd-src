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
  
/*	from OpenSolaris "t8.c	1.6	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 * Portions Copyright (c) 2015 Carsten Kunze
 *
 * Sccsid @(#)t8.c	1.12 (gritter) 10/2/07
 */

 /* t8.c: write out one line of output table */
# include "t..c"
# include <inttypes.h>
# define realsplit ((ct=='a'||ct=='n') && table[nl][c].rcol)
int watchout;
int once;
void
putline (
	/* i is line number for deciding format */
	/* nl is line number for finding data   usually identical */
    int i,
    int nl
)
	/* i is line number for deciding format */
	/* nl is line number for finding data   usually identical */
{
	int c, lf, ct, form, lwid, vspf, ip = -1, cmidx = 0, exvspen, vforml;
	int vct, chfont;
	char *s, *size, *fn;
	char space[40];
	watchout=vspf=exvspen=0;
	if (graphics)
		svgraph();
	if (i==0) once=0;
	if (i==0 && ( allflg || boxflg || dboxflg))
	{
		fullwide(0,   dboxflg? '=' : '-');
	}
	if (instead[nl]==0 && fullbot[nl] ==0)
		for(c=0; c<ncol; c++)
		{
			s = table[nl][c].col;
			if (s==0) continue;
			if (vspen(s))
			{
				for(ip=nl; ip<nlin; ip=next(ip))
					if (!vspen(s=table[ip][c].col)) break;
				if (s!=(char *)0 && !point((intptr_t)s))
					fprintf(tabout, ".ne %su+\\n(.Vu\n",
					    nreg(space, sizeof(space), s,
					    '|'));
				continue;
			}
			if (point((intptr_t)s)) continue;
			fprintf(tabout, ".ne %su+\\n(.Vu\n",
			    nreg(space, sizeof(space), s, '|'));
			watchout=1;
		}
	if (linestop[nl])
		fprintf(tabout, ".mk #%c\n", linestop[nl]+'a'-1);
	lf = prev(nl);
	if (instead[nl])
	{
		puts(instead[nl]);
		return;
	}
	if (fullbot[nl])
	{
		switch (ct=fullbot[nl])
		{
		case '=':
		case '-':
			fullwide(nl,ct);
		}
		return;
	}
	for(c=0; c<ncol; c++)
	{
		if (instead[nl]==0 && fullbot[nl]==0)
		if (vspen(table[nl][c].col)) vspf=1;
		if (lf>=0)
			if (vspen(table[lf][c].col)) vspf=1;
	}
	if (vspf)
	{
		fprintf(tabout, ".nr #^ \\n(\\*(#du\n");
		fprintf(tabout, ".nr #- \\n(#^\n"); /* current line position relative to bottom */
	}
	vspf=0;
	chfont=0;
	for(c=0; c<ncol; c++)
	{
		s = table[nl][c].col;
		if (s==0) continue;
		chfont |= (intptr_t)(font[stynum[nl]][c]);
		if (point((intptr_t)s) ) continue;
		lf=prev(nl);
		nreg(space, sizeof(space), s, '|');
		warnoff();
		if (lf>=0 && vspen(table[lf][c].col))
			fprintf(tabout, ".if (%s+\\n(^%c-1v)>\\n(#- .nr #- +(%s+\\n(^%c-\\n(#--1v)\n",space,'a'+c,space,'a'+c);
		else
			fprintf(tabout, ".if (%s+\\n(#^-1v)>\\n(#- .nr #- +(%s+\\n(#^-\\n(#--1v)\n",space,space);
	}
	warnon();
	if (allflg && once>0 )
		fullwide(i,'-');
	once=1;
	runtabs(i, nl);
	if (allh(nl) && !nflm)
	{
		fprintf(tabout, ".nr %d \\n(.v\n", SVS);
		fprintf(tabout, ".vs \\n(.vu-\\n(.sp\n");
	}
	if (chfont)
		fprintf(tabout, ".nr %2d \\n(.f\n", S1);
	fprintf(tabout, ".nr 35 1m\n");
	if ((utf8 || tlp) && (boxflg || allflg || dboxflg)) { /* right hand line */
		fprintf(tabout, "\\h'|\\n(TWu'");
		fprintf(tabout, "%s", tlp     ? "|"         :
		                      dboxflg ? "\\U'2551'" : /* ║ */
		                                "\\U'2502'"); /* │ */
	}
	fprintf(tabout, "\\&");
	vct = 0;
	for(c=0; c<ncol; c++)
	{
		if (utf8 || tlp) {
			char *s = table[nl][c ? c-1 : 0].col;
			if ((lwid = lefdata(i, c)) && (!ifline(s) ||
			    *s == '\\')) {
				tohcol(c);
				fprintf(tabout, "%s",
				    tlp       ? "|"         :
				    lwid == 2 ? "\\U'2551'" : /* ║ */
				                "\\U'2502'"); /* │ */
				vct += 2;
			}
		} else
		if (watchout==0 && i+1<nlin && (lf=left(i,c, &lwid))>=0)
		{
			tohcol(c);
			drawvert(lf, i, c, lwid);
			vct += 2;
		}
		if (rightl && c+1==ncol) continue;
		vforml=i;
		for(lf=prev(nl); lf>=0 && vspen(table[lf][c].col); lf=prev(lf))
			vforml= lf;
		form= ctype(vforml,c);
		if (form != 's')
		{
			ct = c+CLEFT;
			if (form=='a') ct = c+CMID;
			if (form=='n' && table[nl][c].rcol && lused[c]==0) ct= c+CMID;
			fprintf(tabout, "\\h'|\\n(%du'", ct);
		}
		s= table[nl][c].col;
		fn = font[stynum[vforml]][c];
		size = csize[stynum[vforml]][c];
		if (*size==0)size=0;
		switch(ct=ctype(vforml, c))
		{
		case 'n':
		case 'a':
			if (table[nl][c].rcol)
			{
				if (lused[c]) /*Zero field width*/
				{
					ip = prev(nl);
					if (ip>=0)
					if (vspen(table[ip][c].col))
					{
						if (exvspen==0)
						{
							fprintf(tabout, "\\v'-(\\n(\\*(#du-\\n(^%cu", c+'a');
							if (cmidx)
								fprintf(tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c+'a');
							vct++;
							fprintf(tabout, ")'");
							exvspen=1;
						}
					}
					fprintf(tabout, "%c%c",F1,F2);
					puttext(s,fn,size);
					fprintf(tabout, "%c",F1);
				}
				s= table[nl][c].rcol;
				form=1;
				break;
			}
		case 'c':
			form=3; break;
		case 'r':
			form=2; break;
		case 'l':
			form=1; break;
		case '-':
		case '=':
			if (real(table[nl][c].col))
				fprintf(stderr,"%s: line %d: Data ignored on table line %d\n", ifile, iline-1, i+1);
			makeline(i,c,ct);
			continue;
		default:
			continue;
		}
		if (realsplit ? rused[c]: used[c]) /*Zero field width*/
		{
			/* form: 1 left, 2 right, 3 center adjust */
			if (ifline(s))
			{
				makeline(i,c,ifline(s));
				continue;
			}
			if (filler(s))
			{
				printf("\\l'|\\n(%du\\&%s'", c+CRIGHT, s+2);
				continue;
			}
			ip = prev(nl);
			cmidx = ctop[stynum[nl]][c]==0;
			if (ip>=0)
			if (vspen(table[ip][c].col))
			{
				if (exvspen==0)
				{
					fprintf(tabout, "\\v'-(\\n(\\*(#du-\\n(^%cu", c+'a');
					if (cmidx)
						fprintf(tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c+'a');
					vct++;
					fprintf(tabout, ")'");
				}
			}
			fprintf(tabout, "%c", F1);
			if (form!= 1)
				fprintf(tabout, "%c", F2);
			if (vspen(s))
				vspf=1;
			else
			puttext(s, fn, size);
			if (form !=2)
				fprintf(tabout, "%c", F2);
			fprintf(tabout, "%c", F1);
		}
		if (ip>=0)
		{
			if (vspen(table[ip][c].col))
			{
				exvspen = (c+1 < ncol) && vspen(table[ip][c+1].col) &&
					(topat[c] == topat[c+1]) &&
					(cmidx == (ctop [stynum[nl]][c+1]==0)) && (left(i,c+1,&lwid)<0);
				if (exvspen==0)
				{
					fprintf(tabout, "\\v'(\\n(\\*(#du-\\n(^%cu", c+'a');
					if (cmidx)
						fprintf(tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c+'a');
					vct++;
					fprintf(tabout, ")'");
				}
			}
			else
				exvspen=0;
		}
		/* if lines need to be split for gcos here is the place for a backslash */
		if (vct > 7 && c < ncol)
		{
			fprintf(tabout, "\n.sp -1\n\\&");
			vct=0;
		}
	}
	fprintf(tabout, "\n");
	if (allh(nl) && !nflm) fprintf(tabout, ".vs \\n(%du\n", SVS);
	if (watchout)
		funnies(i,nl);
	if (vspf)
	{
		for(c=0; c<ncol; c++)
			if (vspen(table[nl][c].col) && (nl==0 || (lf=prev(nl))<0 || !vspen(table[lf][c].col)))
			{
				fprintf(tabout, ".nr ^%c \\n(#^u\n", 'a'+c);
				topat[c]=nl;
			}
	}
}
void
puttext(char *s, char *fn, char *size)
{
	if (point((intptr_t)s))
	{
		putfont(fn);
		putsize(size);
		fprintf(tabout, "%s",s);
		if (*fn>0) fprintf(tabout, "\\f\\n(%2d", S1);
		if (size!=0) putsize("0");
	}
}
void
funnies(int stl, int lin)
{
	/* write out funny diverted things */
	int c, pl, lwid, dv, lf, ct = 0;
	intptr_t s;
	char *fn;
	char space[40];
	fprintf(tabout, ".mk ##\n"); /* rmember current vertical position */
	fprintf(tabout, ".nr %d \\n(##\n", S1); /* bottom position */
	for(c=0; c<ncol; c++)
	{
		s = (intptr_t)table[lin][c].col;
		if (point(s)) continue;
		if (s==0) continue;
		fprintf(tabout, ".sp |\\n(##u-1v\n");
		fprintf(tabout, ".nr %d ", SIND);
		for(pl=stl; pl>=0 && !isalpha(ct=ctype(pl,c)); pl=prev(pl))
			;
		switch (ct)
		{
		case 'n':
		case 'c':
			fprintf(tabout, "(\\n(%du+\\n(%du-%su)/2u\n",
			    c + CLEFT, c - 1 + ctspan(lin, c) + CRIGHT,
			    nreg(space, sizeof(space), (char *)s, '-'));
			break;
		case 'l':
			fprintf(tabout, "\\n(%du\n",c+CLEFT);
			break;
		case 'a':
			fprintf(tabout, "\\n(%du\n",c+CMID);
			break;
		case 'r':
			fprintf(tabout, "\\n(%du-%su\n", c + CRIGHT,
			    nreg(space, sizeof(space), (char *)s, '-'));
			break;
		}
		fprintf(tabout, ".in +\\n(%du\n", SIND);
		fn=font[stynum[stl]][c];
		putfont(fn);
		pl = prev(stl);
		if (stl>0 && pl>=0 && vspen(table[pl][c].col))
		{
			fprintf(tabout, ".sp |\\n(^%cu\n", 'a'+c);
			if (ctop[stynum[stl]][c]==0)
			{
				fprintf(tabout,
				    ".nr %d \\n(#-u-\\n(^%c-%s+1v\n", TMP,
				    'a' + c, nreg(space, sizeof(space),
				    (char *)s, '|'));
				fprintf(tabout, ".if \\n(%d>0 .sp \\n(%du/2u\n", TMP, TMP);
			}
		}
		if (s<128)
			fprintf(tabout, ".%c+\n",(int)s);
		else
			fprintf(tabout, ".do %ld+\n",s);
		fprintf(tabout, ".in -\\n(%du\n", SIND);
		if (*fn>0) putfont("P");
		fprintf(tabout, ".mk %d\n", S2);
		fprintf(tabout, ".if \\n(%d>\\n(%d .nr %d \\n(%d\n", S2, S1, S1, S2);
	}
	fprintf(tabout, ".sp |\\n(%du\n", S1);
	for(c=dv=0; c<ncol; c++)
	{
		if (utf8 || tlp) {
			if ((lwid = lefdata(stl,c))) {
				if (!dv++)
					fprintf(tabout, ".sp -1\n");
				tohcol(c);
				dv++;
				fprintf(tabout,
				    "\\L'-(\\n(%du-\\n(##u)%s'", S1,
				    tlp       ? "|"         :
				    lwid == 2 ? "\\U'2551'" : /* ║ */
				                "\\U'2502'"); /* │ */
				fprintf(tabout, "\\v'\\n(%du-\\n(##u'", S1);
			}
		} else
		if (stl+1< nlin && (lf=left(stl,c,&lwid))>=0)
		{
			if (dv++ == 0)
				fprintf(tabout, ".sp -1\n");
			tohcol(c);
			dv++;
			drawvert(lf, stl, c, lwid);
		}
	}
	if ((utf8 || tlp) && (allflg || boxflg || dboxflg)) {
		if (!dv++)
			fprintf(tabout, ".sp -1\n");
		fprintf(tabout, "\\h'|\\n(TWu'");
		fprintf(tabout,
		    "\\L'-(\\n(%du-\\n(##u)%s'", S1,
		    tlp       ? "|"         :
		    lwid == 2 ? "\\U'2551'" : /* ║ */
		                "\\U'2502'"); /* │ */
		fprintf(tabout, "\\v'\\n(%du-\\n(##u'", S1);
	}
	if (dv)
		fprintf(tabout,"\n");
}
void
putfont(char *fn)
{
	if (fn && *fn)
		fprintf(tabout,  fn[1] ? "\\f(%.2s" : "\\f%.2s",  fn);
}
void
putsize(char *s)
{
	if (s && *s)
		fprintf(tabout, "\\s%s",s);
}
