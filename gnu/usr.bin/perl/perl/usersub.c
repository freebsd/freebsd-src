/* $RCSfile: usersub.c,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:40 $
 *
 *  This file contains stubs for routines that the user may define to
 *  set up glue routines for C libraries or to decrypt encrypted scripts
 *  for execution.
 *
 * $Log: usersub.c,v $
 * Revision 1.1.1.1  1993/08/23  21:29:40  nate
 * PERL!
 *
 * Revision 4.0.1.2  92/06/08  16:04:24  lwall
 * patch20: removed implicit int declarations on functions
 * 
 * Revision 4.0.1.1  91/11/11  16:47:17  lwall
 * patch19: deleted some unused functions from usersub.c
 * 
 * Revision 4.0  91/03/20  01:55:56  lwall
 * 4.0 baseline.
 * 
 */

#include "EXTERN.h"
#include "perl.h"

int
userinit()
{
    return 0;
}

/*
 * The following is supplied by John Macdonald as a means of decrypting
 * and executing (presumably proprietary) scripts that have been encrypted
 * by a (presumably secret) method.  The idea is that you supply your own
 * routine in place of cryptfilter (which is purposefully a very weak
 * encryption).  If an encrypted script is detected, a process is forked
 * off to run the cryptfilter routine as input to perl.
 */

#ifdef CRYPTSCRIPT

#include <signal.h>
#ifdef I_VFORK
#include <vfork.h>
#endif

#ifdef CRYPTLOCAL

#include "cryptlocal.h"

#else	/* ndef CRYPTLOCAL */

#define	CRYPT_MAGIC_1	0xfb
#define	CRYPT_MAGIC_2	0xf1

void
cryptfilter( fil )
FILE *	fil;
{
    int    ch;

    while( (ch = getc( fil )) != EOF ) {
	putchar( (ch ^ 0x80) );
    }
}

#endif	/* CRYPTLOCAL */

#ifndef MSDOS
static FILE	*lastpipefile;
static int	pipepid;

#ifdef VOIDSIG
#  define	VOID	void
#else
#  define	VOID	int
#endif

FILE *
mypfiopen(fil,func)		/* open a pipe to function call for input */
FILE	*fil;
VOID	(*func)();
{
    int p[2];
    STR *str;

    if (pipe(p) < 0) {
	fclose( fil );
	fatal("Can't get pipe for decrypt");
    }

    /* make sure that the child doesn't get anything extra */
    fflush(stdout);
    fflush(stderr);

    while ((pipepid = fork()) < 0) {
	if (errno != EAGAIN) {
	    close(p[0]);
	    close(p[1]);
	    fclose( fil );
	    fatal("Can't fork for decrypt");
	}
	sleep(5);
    }
    if (pipepid == 0) {
	close(p[0]);
	if (p[1] != 1) {
	    dup2(p[1], 1);
	    close(p[1]);
	}
	(*func)(fil);
	fflush(stdout);
	fflush(stderr);
	_exit(0);
    }
    close(p[1]);
    close(fileno(fil));
    fclose(fil);
    str = afetch(fdpid,p[0],TRUE);
    str->str_u.str_useful = pipepid;
    return fdopen(p[0], "r");
}

void
cryptswitch()
{
    int ch;
#ifdef STDSTDIO
    /* cheat on stdio if possible */
    if (rsfp->_cnt > 0 && (*rsfp->_ptr & 0xff) != CRYPT_MAGIC_1)
	return;
#endif
    ch = getc(rsfp);
    if (ch == CRYPT_MAGIC_1) {
	if (getc(rsfp) == CRYPT_MAGIC_2) {
	    if( perldb ) fatal("can't debug an encrypted script");
	    rsfp = mypfiopen( rsfp, cryptfilter );
	    preprocess = 1;	/* force call to pclose when done */
	}
	else
	    fatal( "bad encryption format" );
    }
    else
	ungetc(ch,rsfp);
}
#endif /* !MSDOS */

#endif /* CRYPTSCRIPT */
