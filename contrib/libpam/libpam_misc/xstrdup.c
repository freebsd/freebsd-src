/* $FreeBSD$ */
/* $Header: /home/morgan/pam/Linux-PAM-0.53/libpam_misc/RCS/xstrdup.c,v 1.4 1996/11/10 20:10:56 morgan Exp $ */

/*
 * $Log: xstrdup.c,v $
 * Revision 1.4  1996/11/10 20:10:56  morgan
 * modification for stack paranoia
 *
 */

#include <stdlib.h>
#include <security/pam_misc.h>

/*
 * Safe duplication of character strings. "Paranoid"; don't leave
 * evidence of old token around for later stack analysis.
 */

char *xstrdup(const char *x)
{
     register char *new=NULL;

     if (x != NULL) {
	  register int i;

	  for (i=0; x[i]; ++i);                       /* length of string */
	  if ((new = malloc(++i)) == NULL) {
	       i = 0;
	  } else {
	       while (i-- > 0) {
		    new[i] = x[i];
	       }
	  }
	  x = NULL;
     }

     return new;                 /* return the duplicate or NULL on error */
}
