/* exclude.c -
   Exclusion checking.

     Written by James Clark (jjc@jclark.com).
*/

#include "sgmlincl.h"

static int excktok P((struct thdr *, int, int *));
static int exmark P((int));

/* Check that the current exclusions are legal for the content model
of the current element. */

VOID exclude()
{
     struct thdr *mod = tags[ts].tetd->etdmod;

     if ((mod->ttype & MKEYWORD) == 0 && exmark(1)) {
	  int excl;

	  excktok(mod + 1, 0, &excl);
	  exmark(0);
     }
}

/* Set the mark field of all current exclusions to val.  Return 1 if
there are some current exclusions. */

static
int exmark(val)
int val;
{
     int i;
     int gotone = 0;

     for (i = ts; i > 0; --i) {
	  struct etd **p = tags[i].tetd->etdmex;
	  if (p) {
	       for (; *p; p++)
		    (*p)->mark = val;
	       gotone = 1;
	  }
     }
     return gotone;
}

/* Check exclusions for this token.  Return size of token. */

static
int excktok(t, orgrp, excl)
struct thdr *t;
int orgrp;			/* 1 if token is member of or group */
int *excl;			/* Set to 1 if token is excluded. */
{
     int size;
     struct thdr *tem;
     int tnum;
     int optional = 0;
     int hadopt, hadreq;

     *excl = 0;

     switch (t->ttype & TTMASK) {
     case TTETD:
	  if (t->tu.thetd->mark) {
	       if (orgrp || (t->ttype & TOPT))
		    *excl = 1;
	       else
		    sgmlerr(217, &pcbstag, t->tu.thetd->etdgi + 1,
			    tags[ts].tetd->etdgi + 1);
	  }
	  /* fall through */
     case TTCHARS:
	  size = 1;
	  break;
     case TTOR:
     case TTAND:
     case TTSEQ:
	  tem = t + 1;
	  hadopt = 0;
	  hadreq = 0;
	  for (tnum = t->tu.tnum; tnum > 0; --tnum) {
	       int ex;
	       int n = excktok(tem, (t->ttype & TTMASK) == TTOR, &ex);
	       if (!ex) {
		    if (tem->ttype & TOPT)
			 hadopt = 1;
		    else
			 hadreq = 1;
	       }
	       tem += n;
	  }
	  size = tem - t;
	  if ((t->ttype & TTMASK) == TTOR)
	       optional = hadreq ? hadopt : 1;
	  else
	       optional = !hadreq;
	  break;
     default:
	  abort();
     }

     /* Was required, but exclusions have made it optional.
       eg  <!element foo - - (a | b) -(a, b)> */

     if (optional && !(t->ttype & TOPT))
	  sgmlerr(216, &pcbstag, tags[ts].tetd->etdgi + 1, (UNCH *)0);

     return size;
}

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
