/****************************************************************
Copyright 1990, 1993 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#include "defs.h"

warni(s,t)
 char *s;
 int t;
{
	char buf[100];
	sprintf(buf,s,t);
	warn(buf);
	}

warn1(s,t)
char *s, *t;
{
	char buff[100];
	sprintf(buff, s, t);
	warn(buff);
}


warn(s)
char *s;
{
	if(nowarnflag)
		return;
	if (infname && *infname)
		fprintf(diagfile, "Warning on line %ld of %s: %s\n",
			lineno, infname, s);
	else
		fprintf(diagfile, "Warning on line %ld: %s\n", lineno, s);
	fflush(diagfile);
	++nwarn;
}


errstr(s, t)
char *s, *t;
{
	char buff[100];
	sprintf(buff, s, t);
	err(buff);
}



erri(s,t)
char *s;
int t;
{
	char buff[100];
	sprintf(buff, s, t);
	err(buff);
}

errl(s,t)
char *s;
long t;
{
	char buff[100];
	sprintf(buff, s, t);
	err(buff);
}

 char *err_proc = 0;

err(s)
char *s;
{
	if (err_proc)
		fprintf(diagfile,
			"Error processing %s before line %ld",
			err_proc, lineno);
	else
		fprintf(diagfile, "Error on line %ld", lineno);
	if (infname && *infname)
		fprintf(diagfile, " of %s", infname);
	fprintf(diagfile, ": %s\n", s);
	fflush(diagfile);
	++nerr;
}


yyerror(s)
char *s;
{
	err(s);
}



dclerr(s, v)
char *s;
Namep v;
{
	char buff[100];

	if(v)
	{
		sprintf(buff, "Declaration error for %s: %s", v->fvarname, s);
		err(buff);
	}
	else
		errstr("Declaration error %s", s);
}



execerr(s, n)
char *s, *n;
{
	char buf1[100], buf2[100];

	sprintf(buf1, "Execution error %s", s);
	sprintf(buf2, buf1, n);
	err(buf2);
}


Fatal(t)
char *t;
{
	fprintf(diagfile, "Compiler error line %ld", lineno);
	if (infname)
		fprintf(diagfile, " of %s", infname);
	fprintf(diagfile, ": %s\n", t);
	done(3);
}




fatalstr(t,s)
char *t, *s;
{
	char buff[100];
	sprintf(buff, t, s);
	Fatal(buff);
}



fatali(t,d)
char *t;
int d;
{
	char buff[100];
	sprintf(buff, t, d);
	Fatal(buff);
}



badthing(thing, r, t)
char *thing, *r;
int t;
{
	char buff[50];
	sprintf(buff, "Impossible %s %d in routine %s", thing, t, r);
	Fatal(buff);
}



badop(r, t)
char *r;
int t;
{
	badthing("opcode", r, t);
}



badtag(r, t)
char *r;
int t;
{
	badthing("tag", r, t);
}





badstg(r, t)
char *r;
int t;
{
	badthing("storage class", r, t);
}




badtype(r, t)
char *r;
int t;
{
	badthing("type", r, t);
}


many(s, c, n)
char *s, c;
int n;
{
	char buff[250];

	sprintf(buff,
	    "Too many %s.\nTable limit now %d.\nTry rerunning with the -N%c%d option.\n",
	    s, n, c, 2*n);
	Fatal(buff);
}


err66(s)
char *s;
{
	errstr("Fortran 77 feature used: %s", s);
	--nerr;
}



errext(s)
char *s;
{
	errstr("f2c extension used: %s", s);
	--nerr;
}
