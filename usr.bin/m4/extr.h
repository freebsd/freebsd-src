/*  Header : extr.h
    Author : Ozan Yigit
    Updated: %G%
*/
#ifndef	putback

extern ndptr hashtab[];		/* hash table for macros etc.  */
extern char  buf[];		/* push-back buffer	       */
extern char *bp;		/* first available character   */
extern char *bb;		/* current beginning of bp     */
extern char *endpbb;		/* end of push-back buffer     */
extern stae  mstack[];		/* stack of m4 machine         */
extern char *ep;		/* first free char in strspace */
extern char *endest;		/* end of string space	       */
extern int   sp;		/* current m4  stack pointer   */
extern int   fp;		/* m4 call frame pointer       */
extern char *bbstack[];
extern FILE *infile[];		/* input file stack (0=stdin)  */
extern FILE *outfile[];		/* diversion array(0=bitbucket)*/
extern FILE *active;		/* active output file pointer  */
extern char *m4temp;		/* filename for diversions     */
extern int   UNIQUE;		/* where to change m4temp      */
extern int   ilevel;		/* input file stack pointer    */
extern int   oindex;		/* diversion index..	       */
extern char *null;		/* as it says.. just a null..  */
extern char *m4wraps;		/* m4wrap string default..     */
extern char  lquote;		/* left quote character  (`)   */
extern char  rquote;		/* right quote character (')   */
extern char  vquote;		/* verbatim quote character ^V */
extern char  scommt;		/* start character for comment */
extern char  ecommt;		/* end character for comment   */

/* inlined versions of chrsave() and putback() */

extern char  pbmsg[];		/* error message for putback */
extern char  csmsg[];		/* error message for chrsave */

#define putback(c) do { if (bp >= endpbb) error(pbmsg); *bp++ = c; } while (0)
#define chrsave(c) do { if (ep >= endest) error(csmsg); *ep++ = c; } while (0)


#ifdef	__STDC__
#include <stdlib.h>

/* functions from misc.c */

extern	char *	strsave(char *);
extern	int	indx(char *, char *);
extern	void	pbstr(char *);
extern	void	pbqtd(char *);
extern	void	pbnum(int);
extern	void	pbrad(long int, int, int);
extern	void	getdiv(int);
extern	void	killdiv();
extern	void	error(char *);
extern	void	onintr(int);
extern	void	usage();

/* functions from look.c */

extern	ndptr	lookup(char *);
extern	ndptr	addent(char *);
extern	void	remhash(char *, int);
extern	void	addkywd(char *, int);

/* functions from int2str.c */

extern	char*	int2str(/* char*, int, long */);

/* functions from serv.c */

extern	void	expand(char **, int);
extern	void	dodefine(char *, char *);
extern	void	dopushdef(char *, char *);
extern	void	dodefn(char *);
extern	void	dodump(char **, int);
extern	void	doifelse(char **, int);
extern	int	doincl(char *);
extern	void	dochq(char **, int);
extern	void	dochc(char **, int);
extern	void	dodiv(int);
extern	void	doundiv(char **, int);
extern	void	dosub(char **, int);
extern	void	map(char *, char *, char *, char *);
#ifdef	EXTENDED
extern	int	dopaste(char *);
extern	void	m4trim(char **, int);
extern	void	dodefqt(char **, int);
extern	void	doqutr(char **, int);
#endif

/* functions from expr.c */

extern	long	expr(char *);

#else

/* functions from misc.c */

extern	char *	malloc();
extern	char *	strsave();
extern	int	indx();
extern	void	pbstr();
extern	void	pbqtd();
extern	void	pbnum();
extern	void	pbrad();
extern	void	getdiv();
extern	void	killdiv();
extern	void	error();
extern	int	onintr();
extern	void	usage();

/* functions from look.c */

extern	ndptr	lookup();
extern	ndptr	addent();
extern	void	remhash();
extern	void	addkywd();

/* functions from int2str.c */

extern	char*	int2str(/* char*, int, long */);

/* functions from serv.c */

extern	void	expand();
extern	void	dodefine();
extern	void	dopushdef();
extern	void	dodefn();
extern	void	dodump();
extern	void	doifelse();
extern	int	doincl();
extern	void	dochq();
extern	void	dochc();
extern	void	dodiv();
extern	void	doundiv();
extern	void	dosub();
extern	void	map();
#ifdef	EXTENDED
extern	int	dopaste();
extern	void	m4trim();
extern	void	dodefqt();
extern	void	doqutr();
#endif

/* functions from expr.c */

extern	long	expr();

#endif
#endif
