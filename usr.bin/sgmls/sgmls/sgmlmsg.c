/* sgmlmsg.c -
   message handling for core parser

   Written by James Clark (jjc@jclark.com).
*/

#include "config.h"
#include "sgmlaux.h"
#include "msg.h"

static nl_catd catd;

#define TEXT_SET 1		/* message set number for text of messages */
#define HEADER_SET 2		/* message set number for header strings */
#define PARM_SET 3		/* message set number for special parameters */

#ifdef HAVE_EXTENDED_PRINTF
#define xfprintf fprintf
#else
extern int xfprintf VP((FILE *, char *,...));
#endif

#define SIZEOF(v) (sizeof(v)/sizeof(v[0]))

static char *gettext P((int));
static char *getheader P((int));
static char *getparm P((int));
static VOID elttrace P((FILE *, int));
static int printit P((FILE *, struct error *));
static char *transparm P((UNCH *, char *));
static VOID spaces P((FILE *, int));

#define PARMBUFSIZ 50
static char parmbuf[PARMBUFSIZ*2];
static char *parmbuf1 = parmbuf;
static char *parmbuf2 = parmbuf + PARMBUFSIZ;

static char *prog;		/* program name */
static int sweltr;		/* non-zero means print an element trace */
static int swenttr;		/* non-zero means print an entity trace */
static int cnterr = 0;
static VOID (*die) P((void));

static char *headers[] = {
"In file included",
"SGML error",			      /* parameters: type, severity, number */
"Unsupported feature",		      /* type U errors */
"Error",			      /* for type R errors */
"Warning",			      /* severity type I */
" at %s, %.0sline %lu",		      /* ignore entity name and ccnt */
" at entity %s, line %lu",
"%.0s%.0s in declaration parameter %d", /* ignore first two parameters */
"%.0s in declaration parameter %d",     /* ignore first parameter */
"%.0s",				        /* parse mode */
" at end of file",
" at end of entity",
" at record start",
" at record end",
" at \"%c\"",
" at \"\\%03o\"",
" accessing \"%s\"",
"Element structure:"
};

/* Indexes into headers[] */

#define HDRPFX 0 
#define HDRALL 1 
#define HDRUNSUP 2 
#define HDRSYS 3 
#define HDRWARN 4 
#define HDRLOC 5 
#define HDRELOC 6 
#define HDRMD 7 
#define HDRMD2 8 
#define HDRMODE 9 
#define HDREOF 10
#define HDREE 11
#define HDRRS 12
#define HDRRE 13
#define HDRPRT 14
#define HDRCTL 15
#define HDRFIL 16
#define HDRELT 17

/* Special parameters (error::errsp) */
static char *parms[] = {
"character data",
"element content",
"mixed content",
"replaceable character data",
"tag close",
"content model group",
"content model occurrence indicator",
"name group",
"name token group",
"system data",
"parameter literal",
"attribute value literal",
"tokenized attribute value literal",
"minimum literal",
"markup declaration",
"markup declaration comment",
"ignored markup declaration",
"declaration subset",
"CDATA marked section",
"IGNORE marked section",
"RCDATA marked section",
"prolog",
"reference",
"attribute specification list",
"tokenized attribute value",
"attribute specification list close",
"SGML declaration",
"attribute definition list",
"document type",
"element",
"entity",
"link type",
"link set",
"notation",
"SGML",
"short reference mapping",
"link set use",
"short reference use",
};

static FILE *tfp;		/* temporary file for saved messages */

struct saved {
     long start;
     long end;
     char exiterr;
     char countit;
};

VOID msgprint(e)
struct error *e;
{
     if (printit(stderr, e))
	  ++cnterr;
     fflush(stderr);
     if (e->errtype == EXITERR) {
	  if (die) {
	       (*die)();
	       abort();
	  }
	  else
	       exit(EXIT_FAILURE);
     }
}

/* Save an error message. */

UNIV msgsave(e)
struct error *e;
{
     struct saved *sv;

     sv = (struct saved *)rmalloc(sizeof(struct saved));
     if (!tfp) {
	  tfp = tmpfile();
	  if (!tfp)
	       exiterr(160, (struct parse *)0);
     }
     sv->start = ftell(tfp);
     sv->countit = (char)printit(tfp, e);
     sv->end = ftell(tfp);
     sv->exiterr = (char)(e->errtype == EXITERR);
     return (UNIV)sv;
}

/* Print a saved error message. */

VOID msgsprint(p)
UNIV p;
{
     struct saved *sv = (struct saved *)p;
     long cnt;

     assert(p != 0);
     assert(tfp != 0);
     if (fseek(tfp, sv->start, SEEK_SET) < 0)
	  return;
     /* Temporary files are opened in binary mode, so this is portable. */
     cnt = sv->end - sv->start;
     while (--cnt >= 0) {
	  int c = getc(tfp);
	  if (c == EOF)
	       break;
	  putc(c, stderr);
     }
     fflush(stderr);
     if (sv->countit)
	  ++cnterr;
     if (sv->exiterr)
	  exit(EXIT_FAILURE);
}

/* Free a sved error message. */

VOID msgsfree(p)
UNIV p;
{
     frem(p);
}

/* Return 1 if it should be counted as an error. */

static int printit(efp, e)
FILE *efp;
struct error *e;
{
     int indent;
     int countit;
     int hdrcode;
     int filelevel = -1, prevfilelevel = -1, toplevel;
     struct location loc;
     char type[2], severity[2];

     assert(e->errnum < SIZEOF(messages));
     assert(messages[e->errnum].text != NULL);
     if (prog) {
	  fprintf(efp, "%s: ", prog);
	  indent = strlen(prog) + 2; /* don't rely on return value of fprintf */
	  /* Don't want to waste too much space on indenting. */
	  if (indent > 10)
	       indent = 4;
     }
     else
	  indent = 4;
     
     for (toplevel = 0; getlocation(toplevel, &loc); toplevel++)
	  if (loc.filesw) {
	       prevfilelevel = filelevel;
	       filelevel = toplevel;
	  }
     toplevel--;

     if (e->errtype == FILERR) {
	  toplevel--;
	  filelevel = prevfilelevel;
     }
     if (swenttr && filelevel > 0) {
	  int level = 0;
	  int middle = 0;	/* in the middle of a line */
	  do {
	       (void)getlocation(level, &loc);
	       if (loc.filesw) {
		    if (middle) {
			 fputs(":\n", efp);
			 spaces(efp, indent);
		    }
		    else
			 middle = 1;
		    xfprintf(efp, getheader(HDRPFX));
		    xfprintf(efp, getheader(HDRLOC), ioflid(loc.fcb),
			     loc.ename, loc.rcnt, loc.ccnt);
	       }
	       else if (middle)
		    xfprintf(efp, getheader(HDRELOC),
			     loc.ename, loc.rcnt + 1, loc.ccnt);
	  }
	  while (++level != filelevel);
	  if (middle) {
	       fputs(":\n", efp);
	       spaces(efp, indent);
	  }
     }

     /* We use strings for the type and severity,
	so that the format can use %.0s to ignore them. */

     type[0] = messages[e->errnum].type;
     type[1] = '\0';
     severity[0] = messages[e->errnum].severity;
     severity[1] = '\0';

     countit = (severity[0] != 'I');
     if (!countit)
	  hdrcode = HDRWARN;
     else if (type[0] == 'R')
	  hdrcode = HDRSYS;
     else if (type[0] == 'U')
	  hdrcode = HDRUNSUP;
     else
	  hdrcode = HDRALL;
	  
     xfprintf(efp, getheader(hdrcode), type, severity, e->errnum);

     if (filelevel >= 0) {
	  (void)getlocation(filelevel, &loc);
	  xfprintf(efp, getheader(HDRLOC),
		   ioflid(loc.fcb), loc.ename, loc.rcnt, loc.ccnt);
	  while (filelevel < toplevel) {
	       ++filelevel;
	       if (swenttr) {
		    (void)getlocation(filelevel, &loc);
		    xfprintf(efp, getheader(HDRELOC),
			     loc.ename, loc.rcnt + 1, loc.ccnt);
	       }
	  }
     }
     
     /* It is necessary to copy the result of getparm() because
	the specification of catgets() says in can return a 
	pointer to a static buffer which may get overwritten
	by the next call to catgets(). */

     switch (e->errtype) {
     case MDERR:
	  strncpy(parmbuf, getparm(e->errsp), PARMBUFSIZ*2 - 1);
	  xfprintf(efp, getheader(HDRMD), parmbuf,
		   (e->subdcl ? e->subdcl : (UNCH *)""), e->parmno);
	  break;
     case MDERR2:
	  /* no subdcl parameter */
	  strncpy(parmbuf, getparm(e->errsp), PARMBUFSIZ*2 - 1);
	  xfprintf(efp, getheader(HDRMD2), parmbuf, e->parmno);
	  break;
     case DOCERR:
     case EXITERR:
	  if (toplevel < 0)
	       break;
	  strncpy(parmbuf, getparm(e->errsp), PARMBUFSIZ*2 - 1);
	  xfprintf(efp, getheader(HDRMODE), parmbuf);
	  switch (loc.curchar) {
	  case EOFCHAR:
	       xfprintf(efp, getheader(HDREOF));
	       break;
	  case RSCHAR:
	       xfprintf(efp, getheader(HDRRS));
	       break;
	  case RECHAR:
	       xfprintf(efp, getheader(HDRRE));
	       break;
	  case DELNONCH:
	       xfprintf(efp, getheader(HDRCTL), UNSHIFTNON(loc.nextchar));
	       break;
	  case EOS:
	       xfprintf(efp, getheader(HDREE));
	       break;
	  case EOBCHAR:
	       break;
	  default:
	       if (ISASCII(loc.curchar) && isprint(loc.curchar))
		    xfprintf(efp, getheader(HDRPRT), loc.curchar);
	       else
		    xfprintf(efp, getheader(HDRCTL), loc.curchar);
	       break;
	  }
	  break;
     case FILERR:
	  if (getlocation(toplevel + 1, &loc))
	       xfprintf(efp, getheader(HDRFIL), ioflid(loc.fcb));
	  break;
     }
     fputs(":\n", efp);

     if (e->errtype == FILERR && e->sverrno != 0) {
	  char *errstr = strerror(e->sverrno);
	  UNS len = strlen(errstr);
	  /* Strip a trailing newline if there is one. */
	  if (len > 0 && errstr[len - 1] == '\n')
	       len--;
	  spaces(efp, indent);
	  for (; len > 0; len--, errstr++)
	       putc(*errstr, efp);
	  fputs(":\n", efp);
     }

     spaces(efp, indent);

     xfprintf(efp, gettext(e->errnum),
	      transparm((UNCH *)e->eparm[0], parmbuf1),
	      transparm((UNCH *)e->eparm[1], parmbuf2));
     putc('\n', efp);

     if (sweltr)
	  elttrace(efp, indent);
     return countit;
}

/* Print an element trace. */
static VOID elttrace(efp, indent)
FILE *efp;
int indent;
{
     int i = 1;
     UNCH *gi;
     
     gi = getgi(i);
     if (!gi)
	  return;
     spaces(efp, indent);
     xfprintf(efp, getheader(HDRELT));
     do {
	  fprintf(efp, " %s", (char *)gi);
	  gi = getgi(++i);
     } while (gi);
     putc('\n', efp);
}

static VOID spaces(efp, indent)
FILE *efp;
int indent;
{
     while (--indent >= 0)
	  putc(' ', efp);
}

VOID msginit(swp)
struct switches *swp;
{
     catd = swp->catd;
     prog = swp->prog;
     sweltr = swp->sweltr;
     swenttr = swp->swenttr;
     die = swp->die;
}

/* Return the error count. */

int msgcnterr()
{
     return cnterr;
}

/* Transform a parameter into a form suitable for printing. */

static char *transparm(s, buf)
UNCH *s;
char *buf;
{
     char *ptr;
     int cnt;

     if (!s)
	  return 0;

     ptr = buf;
     cnt = PARMBUFSIZ - 4;	/* space for `...\0'  */

     while (*s) {
	  UNCH ch = *s++;
	  if (ch == DELNONCH) {
	       if (*s == '\0')
		    break;
	       ch = UNSHIFTNON(*s);
	       s++;
	  }
	  if (ch == DELCDATA || ch == DELSDATA)
	       ;
	  else if (ch == '\\') {
	       if (cnt < 2)
		    break;
	       *ptr++ = '\\';
	       *ptr++ = '\\';
	       cnt -= 2;
	  }
	  else if (ISASCII(ch) && isprint(ch)) {
	       if (cnt < 1)
		    break;
	       *ptr++ = ch;
	       cnt--;
	  }
	  else {
	       if (cnt < 4)
		    break;
	       sprintf(ptr, "\\%03o", ch);
	       ptr += 4;
	       cnt -= 4;
	  }
     }
     if (!*s)
	  *ptr = '\0';
     else
	  strcpy(ptr, "...");
     return buf;
}

/* The message and set numbers in the catgets function must be > 0. */

static char *gettext(n)
int n;
{
     assert(n > 0 && n < SIZEOF(messages));
     assert(messages[n].text != 0);
     return catgets(catd, TEXT_SET, n, messages[n].text);
}

static char *getheader(n)
int n;
{
     assert(n >= 0 && n < SIZEOF(headers));
     return catgets(catd, HEADER_SET, n + 1, headers[n]);
}

static char *getparm(n)
int n;
{
     assert(n >= 0 && n < SIZEOF(parms));
     return catgets(catd, PARM_SET, n + 1, parms[n]);
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
