/* $RCSfile: usersub.c,v $$Revision: 4.0.1.1 $$Date: 91/11/05 19:07:24 $
 *
 * $Log:	usersub.c,v $
 * Revision 4.0.1.1  91/11/05  19:07:24  lwall
 * patch11: there are now subroutines for calling back from C into Perl
 * 
 * Revision 4.0  91/03/20  01:56:34  lwall
 * 4.0 baseline.
 * 
 * Revision 3.0.1.1  90/08/09  04:06:10  lwall
 * patch19: Initial revision
 * 
 */

#include "EXTERN.h"
#include "perl.h"

int
userinit()
{
    init_curses();
}

/* Be sure to refetch the stack pointer after calling these routines. */

int
callback(subname, sp, gimme, hasargs, numargs)
char *subname;
int sp;			/* stack pointer after args are pushed */
int gimme;		/* called in array or scalar context */
int hasargs;		/* whether to create a @_ array for routine */
int numargs;		/* how many args are pushed on the stack */
{
    static ARG myarg[3];	/* fake syntax tree node */
    int arglast[3];
    
    arglast[2] = sp;
    sp -= numargs;
    arglast[1] = sp--;
    arglast[0] = sp;

    if (!myarg[0].arg_ptr.arg_str)
	myarg[0].arg_ptr.arg_str = str_make("",0);

    myarg[1].arg_type = A_WORD;
    myarg[1].arg_ptr.arg_stab = stabent(subname, FALSE);

    myarg[2].arg_type = hasargs ? A_EXPR : A_NULL;

    return do_subr(myarg, gimme, arglast);
}

int
callv(subname, sp, gimme, argv)
char *subname;
register int sp;	/* current stack pointer */
int gimme;		/* called in array or scalar context */
register char **argv;	/* null terminated arg list, NULL for no arglist */
{
    register int items = 0;
    int hasargs = (argv != 0);

    astore(stack, ++sp, Nullstr);	/* reserve spot for 1st return arg */
    if (hasargs) {
	while (*argv) {
	    astore(stack, ++sp, str_2mortal(str_make(*argv,0)));
	    items++;
	    argv++;
	}
    }
    return callback(subname, sp, gimme, hasargs, items);
}
