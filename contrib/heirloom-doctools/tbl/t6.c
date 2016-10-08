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
  
/*	from OpenSolaris "t6.c	1.3	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 * Portions Copyright (c) 2015 Carsten Kunze
 *
 * Sccsid @(#)t6.c	1.6 (gritter) 2/8/06
 */

 /* t6.c: compute tab stops */
# define tx(a) ((a)!=(char *)0 && !point((intptr_t)(a)))
# include "t..c"
# include <inttypes.h>
void
maktab(void)
{
# define FN(i,c) font[stynum[i]][c]
# define SZ(i,c) csize[stynum[i]][c]
	/* define the tab stops of the table */
	int icol, ilin, tsep, k, ik, vforml, il, text;
	int doubled[MAXCOL], acase[MAXCOL];
	char *s;
	char space[40];
	for(icol=0; icol <ncol; icol++)
	{
		doubled[icol] = acase[icol] = 0;
		fprintf(tabout, ".nr %d 0\n", icol+CRIGHT);
		for(text=0; text<2; text++)
		{
			if (text) {
				warnoff();
				fprintf(tabout, ".%02d\n.rm %02d\n", icol+80, icol+80);
				warnon();
			}
			for(ilin=0; ilin<nlin; ilin++)
			{
				if (instead[ilin]|| fullbot[ilin]) continue;
				vforml=ilin;
				for(il=prev(ilin); il>=0 && vspen(table[il][icol].col); il=prev(il))
					vforml=il;
				if (fspan(vforml,icol)) continue;
				if (filler(table[ilin][icol].col)) continue;
				switch(ctype(vforml,icol))
				{
				case 'a':
					acase[icol]=1;
					s = table[ilin][icol].col;
					if (tx(s) && text)
					{
						if (doubled[icol]==0)
							fprintf(tabout, ".nr %d 0\n.nr %d 0\n",S1,S2);
						doubled[icol]=1;
						nreg(space, sizeof(space), s,
						    '-');
						fprintf(tabout, ".if %s>\\n(%d .nr %d %s\n",space,S2,S2,space);
					}
				case 'n':
					if (table[ilin][icol].rcol!=0)
					{
						if (doubled[icol]==0 && text==0)
							fprintf(tabout, ".nr %d 0\n.nr %d 0\n", S1, S2);
						doubled[icol]=1;
						if (real(s=table[ilin][icol].col) && !vspen(s))
						{
							if (tx(s) != text) continue;
							fprintf(tabout, ".nr %d ", TMP);
							wide(s, FN(vforml,icol), SZ(vforml,icol)); fprintf(tabout, "\n");
							fprintf(tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n", S1, TMP, S1, TMP);
						}
						if (text==0 && real(s=table[ilin][icol].rcol) && !vspen(s) && !barent(s))
						{
							fprintf(tabout, ".nr %d \\w%c%s%c\n",TMP, F1, s, F1);
							fprintf(tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n",S2,TMP,S2,TMP);
						}
						continue;
					}
				case 'r':
				case 'c':
				case 'l':
					if (real(s=table[ilin][icol].col) && !vspen(s))
					{
						if (tx(s) != text) continue;
						fprintf(tabout, ".nr %d ", TMP);
						wide(s, FN(vforml,icol), SZ(vforml,icol)); fprintf(tabout, "\n");
						fprintf(tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n", icol+CRIGHT, TMP, icol+CRIGHT, TMP);
					}
				}
			}
		}
		if (acase[icol])
		{
			fprintf(tabout, ".if \\n(%d>=\\n(%d .nr %d \\n(%du+2n\n",S2,icol+CRIGHT,icol+CRIGHT,S2);
		}
		if (doubled[icol])
		{
			fprintf(tabout, ".nr %d \\n(%d\n", icol+CMID, S1);
			fprintf(tabout, ".nr %d \\n(%d+\\n(%d\n",TMP,icol+CMID,S2);
			fprintf(tabout, ".if \\n(%d>\\n(%d .nr %d \\n(%d\n",TMP,icol+CRIGHT,icol+CRIGHT,TMP);
			fprintf(tabout, ".if \\n(%d<\\n(%d .nr %d +(\\n(%d-\\n(%d)/2\n",TMP,icol+CRIGHT,icol+CMID,icol+CRIGHT,TMP);
		}
		if (cll[icol][0])
		{
			fprintf(tabout, ".nr %d %sn\n", TMP, cll[icol]);
			fprintf(tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n",icol+CRIGHT, TMP, icol+CRIGHT, TMP);
		}
		for(ilin=0; ilin<nlin; ilin++)
			if ((k=lspan(ilin, icol)))
			{
				s=table[ilin][icol-k].col;
				if (!real(s) || barent(s) || vspen(s) ) continue;
				fprintf(tabout, ".nr %d ", TMP);
				wide(table[ilin][icol-k].col, FN(ilin,icol-k), SZ(ilin,icol-k));
				for(ik=k; ik>=0; ik--)
				{
					fprintf(tabout, "-\\n(%d",CRIGHT+icol-ik);
					if (!expflg && ik>0) fprintf(tabout, "-%dn", sep[icol-ik]);
				}
				fprintf(tabout, "\n");
				fprintf(tabout, ".if \\n(%d>0 .nr %d \\n(%d/%d\n", TMP, TMP, TMP, k);
				fprintf(tabout, ".if \\n(%d<0 .nr %d 0\n", TMP, TMP);
				for(ik=0; ik<k; ik++)
				{
					if (doubled[icol-k+ik])
						fprintf(tabout, ".nr %d +\\n(%d/2\n", icol-k+ik+CMID, TMP);
					fprintf(tabout, ".nr %d +\\n(%d\n", icol-k+ik+CRIGHT, TMP);
				}
			}
	}
	if (textflg) untext();
	/* if even requested, make all columns widest width */
# define TMP1 S1
# define TMP2 S2
	if (evenflg)
	{
		fprintf(tabout, ".nr %d 0\n", TMP);
		for(icol=0; icol<ncol; icol++)
		{
			if (evenup[icol]==0) continue;
			fprintf(tabout, ".if \\n(%d>\\n(%d .nr %d \\n(%d\n",
			    icol+CRIGHT, TMP, TMP, icol+CRIGHT);
		}
		for(icol=0; icol<ncol; icol++)
		{
			if (evenup[icol]==0)
				/* if column not evened just retain old interval */
				continue;
			if (doubled[icol])
				fprintf(tabout, ".nr %d (100*\\n(%d/\\n(%d)*\\n(%d/100\n",
				    icol+CMID, icol+CMID, icol+CRIGHT, TMP);
				/* that nonsense with the 100's and parens tries
				   to avoid overflow while proportionally shifting
				   the middle of the number */
			fprintf(tabout, ".nr %d \\n(%d\n", icol+CRIGHT, TMP);
		}
	}
	/* now adjust for total table width */
	for(tsep=icol=0; icol<ncol; icol++)
		tsep+= sep[icol];
	if (expflg)
	{
		fprintf(tabout, ".nr %d 0", TMP);
		for(icol=0; icol<ncol; icol++)
			fprintf(tabout, "+\\n(%d", icol+CRIGHT);
		fprintf(tabout, "\n");
		fprintf(tabout, ".nr %d \\n(.l-\\n(.i-\\n(%d%s\n", TMP, TMP,
		    (utf8 || tlp) && (boxflg || dboxflg || allflg) ? "-1n" : "");
		if (boxflg || dboxflg || allflg)
			tsep += 1;
		else
			tsep -= sep[ncol-1];
		fprintf(tabout, ".nr %d \\n(%d/%d\n", TMP, TMP,  tsep);
		fprintf(tabout, ".if \\n(%d<1n .nr %d 1n\n", TMP, TMP);
	}
	else if (xcolflg) {
		fprintf(tabout, ".nr %d 0", TMP);
		for(icol=0; icol<ncol; icol++)
			fprintf(tabout, "+\\n(%d", icol+CRIGHT);
		fprintf(tabout, "\n");
		fprintf(tabout, ".nr %d \\n(.l-\\n(.i-\\n(%d-%dn/%d\n", TMP,
		    TMP, tsep + ((boxflg || dboxflg || allflg) ? 
		    (utf8 || tlp) ? 2 : 1 : -1), xcolflg);
		for(icol=0; icol<ncol; icol++) {
			if (!xcol[icol]) continue;
			fprintf(tabout, ".nr %d +\\n(%d\n", icol+CRIGHT, TMP);
		}
		fprintf(tabout, ".nr %d 1n\n", TMP);
	}
	else
		fprintf(tabout, ".nr %d 1n\n", TMP);
	fprintf(tabout, ".nr %d 0\n",CRIGHT-1);
	tsep= (boxflg || allflg || dboxflg || left1flg) ? 1 : 0;
	for(icol=0; icol<ncol; icol++)
	{
		fprintf(tabout, ".nr %d \\n(%d+(%d*\\n(%d)\n",icol+CLEFT, icol+CRIGHT-1, tsep, TMP);
		fprintf(tabout, ".nr %d +\\n(%d\n",icol+CRIGHT, icol+CLEFT);
		if (doubled[icol])
		{
			/* the next line is last-ditch effort to avoid zero field width */
			/*fprintf(tabout, ".if \\n(%d=0 .nr %d 1\n",icol+CMID, icol+CMID);*/
			fprintf(tabout, ".nr %d +\\n(%d\n", icol+CMID, icol+CLEFT);
			/*  fprintf(tabout, ".if n .if \\n(%d%%24>0 .nr %d +12u\n",icol+CMID, icol+CMID); */
		}
		tsep=sep[icol];
	}
	if (rightl)
		fprintf(tabout, ".nr %d (\\n(%d+\\n(%d)/2\n",ncol+CRIGHT-1, ncol+CLEFT-1, ncol+CRIGHT-2);
	fprintf(tabout, ".nr TW \\n(%d\n", ncol+CRIGHT-1);
	if (boxflg || allflg || dboxflg)
		fprintf(tabout, ".nr TW +%d*\\n(%d\n", sep[ncol-1], TMP);
	fprintf(tabout,
	    ".if t .if \\n(TW>\\n(.l .tm Table at line %d file %s is too wide - \\n(TW units\n", iline-1, ifile);
	return;
}
void
wide(char *s, char *fn, char *size)
{
	char space[40];
	if (point((intptr_t)s))
	{
		fprintf(tabout, "\\w%c", F1);
		if (*fn>0) putfont(fn);
		if (*size) putsize(size);
		fprintf(tabout, "%s", s);
		if (*fn>0) putfont("P");
		if (*size) putsize("0");
		fprintf(tabout, "%c",F1);
	}
	else
		fprintf(tabout, "%s", nreg(space, sizeof(space), s, '-'));
}
int 
filler(char *s)
{
return (point((intptr_t)s) && s[0]=='\\' && s[1] == 'R');
}
