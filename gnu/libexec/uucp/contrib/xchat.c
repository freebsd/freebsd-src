/*
 *  ***********
 *  * XCHAT.C *
 *  ***********
 *
 * Extended chat processor for Taylor UUCP. See accompanying documentation.
 *
 * Written by:
 *   Bob Denny (denny@alisa.com)
 *   Based on code in DECUS UUCP (for VAX/VMS)
 *
 * Small modification by:
 *   Daniel Hagerty (hag@eddie.mit.edu)
 *
 * History:
 *   Version 1.0 shipped with Taylor 1.03. No configuration info inside.
 *
 *   Bob Denny - Sun Aug 30 18:41:30 1992
 *     V1.1 - long overdue changes for other systems. Rip out interval
 *            timer code, use timer code from Taylor UUCP, use select()
 *            for timed reads. Use Taylor UUCP "conf.h" file to set
 *            configuration for this program. Add defaulting of script
 *            and log file paths.
 *   
 *   Daniel Hagerty - Mon Nov 22 18:17:38 1993
 *     V1.2 - Added a new opcode to xchat. "expectstr" is a cross between
 *            sendstr and expect, looking for a parameter supplied string.
 *            Useful where a prompt could change for different dial in
 *            lines and such.
 *
 * Bugs:
 *   Does not support BSD terminal I/O. Anyone care to add it?
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/termio.h>

#include "xc-conf.h"

/* 
 * Pick a timing routine to use, as done in Taylor UUCP.
 */
#if HAVE_USLEEP || HAVE_NAP || HAVE_NAPMS || HAVE_POLL
#define USE_SELECT_TIMER 0
#else
#define USE_SELECT_TIMER HAVE_SELECT
#if USE_SELECT_TIMER
#include <sys/time.h>
#endif
#endif

#if HAVE_USLEEP || HAVE_NAP || HAVE_NAPMS
#undef HAVE_POLL
#define HAVE_POLL 0
#endif

#if HAVE_USLEEP || HAVE_NAP
#undef HAVE_NAPMS
#define HAVE_NAPMS 0
#endif

#if HAVE_USLEEP
#undef HAVE_NAP
#define HAVE_NAP 0
#endif

static int ttblind();
static int ttcd();

/* script entry -- "compiled" form of dial, hangup, or login script */

struct script {
	struct	script	*next;	/* pointer to next entry, or null */
	int		 opcode;	/* numeric opcode */
	char		*strprm;	/* pointer to string param */
	long		 intprm;	/* integer parameter */
	char		*newstate;	/* new state name */
};

/* opcode definition array element -- one for each possible opcode */

struct script_opdef {
	char	*opname;
	int	 opcode;	/* numeric opcode -- same as array index */
	int	 prmtype;	/* one of SC_NONE, SC_STR, SC_XSTR, SC_INT */
	int	 newstate;	/* one of SC_NONE, SC_NWST */
};

	/* values for opcode */

#define	SC_LABEL 0	/* "label" (state name) */
#define	SC_CDLY	1	/* set char output delay in msec */
#define	SC_PCHR	2	/* pause char for dial string (from P in input) */
#define	SC_PTIM	3	/* seconds to allow for pause char */
#define	SC_WCHR	4	/* wait char for dial string (from W in input) */
#define	SC_WTIM	5	/* seconds to allow for wait char */
#define	SC_ZERO	6	/* zero counter */
#define	SC_INCR	7	/* increment counter */
#define SC_IFGT	8	/* change state if counter > int param */
#define	SC_WAIT	9	/* wait for int param seconds */
#define	SC_GOTO	10	/* unconditional change to new state */
#define	SC_SEND	11	/* send strparam (after sprintf substitutions) */
#define	SC_BRK	12	/* send a break */
#define	SC_HANG	13	/* drop DTR */
#define	SC_DIAL	14	/* send telno string (after subst PCHR & WCHR) */
#define	SC_DTIM	15	/* time in msec per digit (for timeout calculations) */
			/* default = 100 (one tenth second) */
#define	SC_CTIM	16	/* additional time (in seconds) to wait for carrier */
			/* default = 45 seconds */
#define	SC_EXIT	17	/* script done, success */
#define	SC_FAIL	18	/* script done, failure */
#define	SC_LOG	19	/* write strparam to uucp.log */
#define	SC_LOGE	20	/* write strparam to uucp.log w/error ind */
#define	SC_DBG	21	/* write strparam to debug log if debug lvl = LGI */
#define	SC_DBGE	22	/* write strparam to debug log if debug lvl = LGIE */
#define	SC_DBST	23	/* 'or' intparam into debug mask */
#define	SC_DBCL	24	/* 'bicl' intparam into debug mask */
#define	SC_TIMO	25	/* newstate if no match in intparam secs */
			/* (uses calculated dial time if intparam is 0) */
#define	SC_XPCT	26	/* wait for strparam, goto _newstate if found */
#define	SC_CARR	27	/* goto _newstate if carrier detected */
#define	SC_FLSH	28	/* flush typeahead buffer */
#define	SC_IFBL	29	/* change state if controller is blind w/o CD */
#define	SC_IFBG	30	/* chg state if ctlr is blind and counter > intprm */
#define	SC_SNDP	31	/* send parameter n */
#define	SC_IF1P	32	/* if parameter n present */
#define	SC_IF0P	33	/* if parameter n absent */
#define SC_DBOF 34	/* open debugging file */
#define SC_TELN 35	/* Set telno from parameter n */
#define SC_7BIT 36	/* Set port to 7-bit stripping */
#define SC_8BIT 37	/* Set port for 8-bit characters */
#define SC_PNON 38	/* Set port for 8-bit, no parity */
#define SC_PEVN 39	/* Set port for 7-bit, even parity */
#define SC_PODD 40	/* Set port for 7-bit, odd parity */
#define SC_HUPS 41	/* Change state on HUP signal */
#define SC_XPST	42	/* Expect a param string */
#define	SC_END	43	/* end of array */

	/* values for prmtype, prm2type */

#define	SC_NONE	0		/* no parameter */
#define	SC_STR	1		/* simple string */
#define	SC_INT	2		/* integer */
#define	SC_NWST	3		/* new state name */
#define	SC_XSTR	4		/* translated string */

/* opcode definition table for dial/login/hangup scripts */

static struct	script_opdef	sc_opdef[] =
      {
	{"label",	SC_LABEL,	SC_NONE,	SC_NONE},
	{"chrdly",	SC_CDLY,	SC_INT,		SC_NONE},
	{"pchar",	SC_PCHR,	SC_STR,		SC_NONE},
	{"ptime",	SC_PTIM,	SC_INT,		SC_NONE},
	{"wchar",	SC_WCHR,	SC_STR,		SC_NONE},
	{"wtime",	SC_WTIM,	SC_INT,		SC_NONE},
	{"zero",	SC_ZERO,	SC_NONE,	SC_NONE},
	{"count",	SC_INCR,	SC_NONE,	SC_NONE},
	{"ifgtr",	SC_IFGT,	SC_INT,		SC_NWST},
	{"sleep",	SC_WAIT,	SC_INT,		SC_NONE},
	{"goto",	SC_GOTO,	SC_NONE,	SC_NWST},
	{"send",	SC_SEND,	SC_XSTR,	SC_NONE},
	{"break",	SC_BRK,		SC_NONE,	SC_NONE},
	{"hangup",	SC_HANG,	SC_NONE,	SC_NONE},
	{"7bit",	SC_7BIT,	SC_NONE,	SC_NONE},
	{"8bit",	SC_8BIT,	SC_NONE,	SC_NONE},
	{"nopar",	SC_PNON,	SC_NONE,	SC_NONE},
	{"evenpar",	SC_PEVN,	SC_NONE,	SC_NONE},
	{"oddpar",	SC_PODD,	SC_NONE,	SC_NONE},
	{"telno",	SC_TELN,	SC_INT,		SC_NONE},
	{"dial",	SC_DIAL,	SC_NONE,	SC_NONE},
	{"dgttime",	SC_DTIM,	SC_INT,		SC_NONE},
	{"ctime",	SC_CTIM,	SC_INT,		SC_NONE},
	{"success",	SC_EXIT,	SC_NONE,	SC_NONE},
	{"failed",	SC_FAIL,	SC_NONE,	SC_NONE},
	{"log",		SC_LOG,		SC_XSTR,	SC_NONE},
	{"logerr",	SC_LOGE,	SC_XSTR,	SC_NONE},
	{"debug",	SC_DBG,		SC_XSTR,	SC_NONE},
	{"debuge",	SC_DBGE,	SC_XSTR,	SC_NONE},
	{"dbgset",	SC_DBST,	SC_INT,		SC_NONE},
	{"dbgclr",	SC_DBCL,	SC_INT,		SC_NONE},
	{"dbgfile",	SC_DBOF,	SC_XSTR,	SC_NONE},
	{"timeout",	SC_TIMO,	SC_INT,		SC_NWST},
	{"expect",	SC_XPCT,	SC_XSTR,	SC_NWST},
	{"ifcarr",	SC_CARR,	SC_NONE,	SC_NWST},
	{"ifhang",	SC_HUPS,	SC_NONE,	SC_NWST},
	{"flush",	SC_FLSH,	SC_NONE,	SC_NONE},
	{"ifblind",	SC_IFBL,	SC_NONE,	SC_NWST},
	{"ifblgtr",	SC_IFBG,	SC_INT,		SC_NWST},
	{"sendstr",	SC_SNDP,	SC_INT,		SC_NONE},
	{"ifstr",	SC_IF1P,	SC_INT,		SC_NWST},
	{"ifnstr",	SC_IF0P,	SC_INT,		SC_NWST},
	{"expectstr",	SC_XPST,	SC_INT,		SC_NWST},
	{"table end",	SC_END,		SC_NONE,	SC_NONE}
      };

#define SUCCESS 0
#define	FAIL	1
#define ERROR	-1
#define MAX_SCLINE	255	/* max length of a line in a script file */
#define MAX_EXPCT	127	/* max length of an expect string */
#define	CTL_DELIM	" \t\n\r" /* Delimiters for tokens */
#define	SAME		0	/* if (strcmp(a,b) == SAME) ... */
#define	SLOP		10	/* Slop space on arrays */
#define	MAX_STRING	200	/* Max length string to send/expect */

#define	DEBUG_LEVEL(level) \
	   (Debug & (1 << level))

#define	DB_LOG	0	/* error messages and a copy of the LOGFILE output */
#define	DB_LGIE	1	/* dial,login,init trace -- errors only */
#define	DB_LGI	2	/* dial,login,init trace -- nonerrors (incl chr I/O) */
#define	DB_LGII	3	/* script processing internals */

#define TRUE    1
#define FALSE   0

#define NONE	0
#define EVEN	1
#define ODD	2

#define logit(m, p1) fprintf(stderr, "%s %s\n", m, p1)

static char **paramv;		/* Parameter vector */
static int paramc;		/* Parameter count */
static char telno[64];		/* Telephone number w/meta-chars */
static int Debug;
static int fShangup = FALSE;	/* TRUE if HUP signal received */
static FILE  *dbf = NULL;
static struct termio old, new;

extern int usignal();
extern int uhup();

static struct siglist
{
  int signal;
  int (*o_catcher) ();
  int (*n_catcher) ();
} sigtbl[] = {
             { SIGHUP,   NULL, uhup },
             { SIGINT,   NULL, usignal },
	     { SIGIOT,   NULL, usignal },
             { SIGQUIT,  NULL, usignal },
             { SIGTERM,  NULL, usignal },
             { SIGALRM,  NULL, usignal },
             { 0,        NULL, NULL    }    /* Table end */
           };

extern struct script *read_script();
extern void msleep();
extern char xgetc();
extern void charlog();
extern void setup_tty();
extern void restore_tty();
extern void ttoslow();
extern void ttflui();
extern void tthang();
extern void ttbreak();
extern void tt7bit();
extern void ttpar();
extern void DEBUG();

extern void *malloc();


/*
 * **********************************
 * * BEGIN EXECUTION - MAIN PROGRAM *
 * **********************************
 *
 * This program is called by Taylor UUCP with a list of
 * arguments in argc/argv, and stdin/stdout mapped to the
 * tty device, and stderr mapped to the Taylor logfile, where
 * anything written to stdout will be logged as an error.
 * 
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int i, stat;
  FILE *sf;
  char sfname[256];
  struct script *script;
  struct siglist *sigs;

  /*
   * The following is needed because my cpp does not have the
   * #error directive...
   */
#if ! HAVE_SELECT
  no_select_sorry();		/* Sad way to fail make */
#endif

  paramv = &argv[2];		/* Parameters start at 2nd arg */
  paramc = argc - 2;		/* Number of live parameters */

  telno[0] = '\0';

  if (argc < 2)
    {
      fprintf(stderr, "%s: no script file supplied\n", argv[0]);
      exit(FAIL);
    }

  /*
   * If the script file argument begins with '/', then we assume
   * it is an absolute pathname, otherwise, we prepend the 
   * SCRIPT_DIR path.
   */
  *sfname = '\0';		/* Empty name string */
  if(argv[1][0] != '/')		/* If relative path */
    strcat(sfname, SCRIPT_DIR); /* Prepend the default dir. */
  strcat(sfname, argv[1]);	/* Add the script file name */

  /*
   * Now open the script file.
   */
  if ((sf = fopen(sfname, "r")) == NULL)
    {
      fprintf(stderr, "%s: Failed to open script %s\n", argv[0], sfname);
      perror(" ");
      exit(FAIL);
    }

  /*
   * COMPILE SCRIPT
   */
  if ((script = read_script(sf)) == NULL)
    {
      fprintf(stderr, "%s: script error in \"%s\"\n", argv[0], argv[1]);
      exit(FAIL);
    }

  /*
   * Set up a signal catcher so the line can be returned to
   * it's current state if something nasty happens.
   */
  sigs = &sigtbl[0];
  while(sigs->signal)
    {
      sigs->o_catcher = (int (*) ())signal(sigs->signal, sigs->n_catcher);
      sigs += 1;
    }

  /*
   * Save current tty settings, then set up raw, single
   * character input processing, with 7-bit stripping.
   */
  setup_tty();

  /*
   * EXECUTE SCRIPT
   */
  if ((stat = do_script(script)) != SUCCESS)
    fprintf(stderr, "%s: script %s failed.\n", argv[0], argv[1]);

  /*
   * Clean up and exit.
   */
  restore_tty();
#ifdef FIXSIGS
  sigs = &sigtbl[0];
  while(sigs->signal)
    if(sigs->o_catcher != -1)
      signal(sigs->signal, sigs->o_catcher);
#endif
  exit(stat);
}

/* 
 * deal_script - deallocate a script and all strings it points to
 */
int deal_script(loc)
struct script *loc;
{
  /*
   * If pointer is null, just exit
   */
  if (loc == (struct script *)NULL)
    return SUCCESS;
  
  /*
   * Deallocate the rest of the script
   */
  deal_script(loc->next);
  
  /*
   * Deallocate the string parameter, if any
   */
  if (loc->strprm != (char *)NULL)
    free(loc->strprm);
  
  /*
   * Deallocate the new state name parameter, if any
   */
  if (loc->newstate != (char *)NULL)
    free(loc->newstate);
  
  /*
   * Deallocate this entry
   */
  free(loc);
  
  return SUCCESS;
}


/* 
 * read_script
 *
 * Read & compile a script, return pointer to first entry, or null if bad
 */
struct script *read_script(fd)
     FILE *fd;
{
  struct script	*this = NULL;
  struct script	*prev = NULL;
  struct script	*first = NULL;
  long len, i;
  char inpline[MAX_SCLINE];
  char inpcopy[MAX_SCLINE];
  char *c, *cln, *opc, *cp;
  
  /*
   * MAIN COMPILATION LOOP
   */  
  while ((c = fgets(inpline, (sizeof inpline - 1), fd)) != (char *)NULL)
    {
      /*
       * Skip comments and blank lines
       */
      if (*c == '#' || *c == '\n')
	continue;
      
      /* 
       * Get rid of the trailing newline, and copy the string
       */
      inpline[strlen(inpline)-1] = '\0';
      strcpy(inpcopy, inpline);
      
      /*
       * Look for text starting in the first col (a label)
       */
      if ((!isspace(inpline[0])) &&
	  (cln = strchr (inpline, ':')) != (char *)NULL) {
	this = (struct script *)malloc (sizeof (struct script));
	if (prev != (struct script *)NULL)
	  prev->next = this;
	prev = this;
	if (first == (struct script *)NULL)
	  first = this;
	this->next = (struct script *)NULL;
	this->opcode = SC_LABEL;
	len = cln - c;
	this->strprm = (char *)malloc(len+1);
	strncpy(this->strprm, c, len);
	(this->strprm)[len] = '\0';
	this->intprm = 0;
	this->newstate = (char *)NULL;
	c = cln + 1;
      }
      
      /*
       * Now handle the opcode. Fold it to lower case.
       */
      opc = strtok(c, CTL_DELIM);
      if (opc == (char *)NULL)	/* If no opcode... */
	continue;			/* ...read the next line */
      cp = opc;
      while(*cp)
	tolower(*cp++);
      
      /* 
       * If we have an opcode but we haven't seen anything
       * else (like a label) yet, i.e., this is the first
       * entry, and there was no label.  We need to 
       * cobble up a label so that read_script is happy
       */
      if (first == (struct script *)NULL) 
	{
	  this = (struct script *)malloc (sizeof (struct script));
	  prev = this;
	  first = this;
	  this->next = (struct script *)NULL;
	  this->opcode = SC_LABEL;
	  this->strprm = (char *)malloc(2);
	  strcpy(this->strprm, ":");
	  this->intprm = 0;
	  this->newstate = (char *)NULL;
	}
      
      /* 
       * Find opcode - ndex through the opcode definition table
       */
      for (i=1; sc_opdef[i].opcode != SC_END; i++)
	if (strcmp(opc, sc_opdef[i].opname) == SAME) 
	  break;
      if ((sc_opdef[i].opcode) == SC_END)
	{
	  logit ("Bad opcode in script", opc);
	  deal_script(first);
	  return (struct script *)NULL;
        }
      
      /*
       * Found opcode. Allocate a new command node and initialize
       */
      this = (struct script *)malloc(sizeof (struct script));
      prev->next = this;
      prev = this;
      this->next = (struct script *)NULL;
      this->opcode = sc_opdef[i].opcode;
      this->strprm = (char *)NULL;
      this->intprm = 0;
      this->newstate = (char *)NULL;
      
      /* 
       * Pick up new state parameter, if any
       */
      if (sc_opdef[i].newstate == SC_NWST)
	{
	  c = strtok((char *)NULL, CTL_DELIM);
	  if (c == (char *)NULL)
	    {
	      logit("Missing new state", opc);
	      deal_script(first);
	      return (struct script *)NULL;
	    }
	  else
	    {
	      this->newstate = (char *)malloc(strlen(c)+1);
	      strcpy(this->newstate, c);
	    }
	}
      
      /*
       * Pick up the string or integer parameter. Handle missing
       * parameter gracefully.
       */
      switch (sc_opdef[i].prmtype)
	{
	/*
	 * INT parameter - convert and store in node
	 */
	case SC_INT:
	  c = strtok((char *)NULL, CTL_DELIM);
	  if (c == (char *)NULL)
	    {
	      logit("Missing script param", opc);
	      deal_script(first);
	      return (struct script *)NULL;
	    }
	  /*
	   * If this is the parameter to DBST or DBCL, force
           * base-10 conversion, else convert per parameter.
	   */
	  if (sc_opdef[i].opcode == SC_DBST ||
	      sc_opdef[i].opcode == SC_DBCL)
	    this->intprm = strtol(c, (char **)NULL, 0);
	  else
	    this->intprm = strtol(c, (char **)NULL, 10);
	  break;

	/*
	 * STR/XSTR strings.
	 */
	case SC_STR:		
	case SC_XSTR:		
	  c = strtok((char *)NULL, CTL_DELIM);
	  if (c == (char *)NULL)
	    {
	      logit("Missing script param", opc);
	      deal_script(first);
	      return (struct script *)NULL;
	    }
	  /*
	   * For XSTR opcode, use c to find out where
	   * the string param begins in the copy of the
	   * input line, and pick up all that's left of
	   * the line (to allow imbedded blanks, etc.).
	   */
	  if (sc_opdef[i].prmtype == SC_XSTR)
	    c = &inpcopy[0] + (c - &inpline[0]);

	  /*
	   * Allocate a buffer for the string parameter
	   */
	  this->strprm = (char *)malloc(strlen(c)+1);

	  /*
	   * For XSTR, Translate the string and store its
	   * length. Note that, after escape sequences are 
	   * compressed, the resulting string may well be a 
	   * few bytes shorter than the input string (whose 
	   * length was the basis for the malloc above),
	   * but it will never be longer.
	   */
	  if (sc_opdef[i].prmtype == SC_XSTR)
	    {
	      this->intprm = xlat_str(this->strprm, c);
	      this->strprm[this->intprm] = '\0';
	    }
	  else
	    strcpy(this->strprm, c);
	  break;
	  
	}
    }
  
  /*
   * EOF
   */
  return first;
}


/*
 * xlat_str
 *
 * Translate embedded escape characters in a "send" or "expect" string.
 *
 * Called by read_script(), above.
 *
 * Returns the actual length of the resulting string.  Note that imbedded
 * nulls (specified by \000 in the input) ARE allowed in the result.  
 */
xlat_str(out, in)
     char *out, *in;
{
  register int i = 0, j = 0;
  int byte, k;
  
  while (in[i]) 
    {
      if (in[i] != '\\') 
	{
	  out[j++] = in[i++];
	}
      else 
	{
	  switch (in[++i]) 
	    {
	    case 'd':		/* EOT */
	      out[j++] = 0x04;
	      break;
	    case 'N':		/* null */
	      out[j++] = 0x00;
	      break;
	    case 'n':		/* line feed */
	      out[j++] = 0x0a;
	      break;
	    case 'r':		/* carriage return */
	      out[j++] = 0x0d;
	      break;
	    case 's':		/* space */
	      out[j++] = ' ';
	      break;
	    case 't':		/* tab */
	      out[j++] = '\t';
	      break;
	    case '-':		/* hyphen */
	      out[j++] = '-';
	      break;
	    case '\\':		/* back slash */
	      out[j++] = '\\';
	      break;
	    case '0':		/* '\nnn' format */
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	      byte = in[i] - '0';
	      k = 0;
	      
	      while (3 > ++k)	
		if ((in[i+1] < '0') || (in[i+1] > '7'))
		  break;
		else 
		  {
		    byte = (byte<<3) + in[i+1] - '0';
		    ++i;
		  }
	      out[j++] = byte;
	      break;
	    default:            /* don't know so skip it */
	      break;
	    }
	  ++i;
	}
    } 
  return j;
}


/* find a state within a script */

struct script *
  find_state(begin, newstate)
struct script *begin;
char *newstate;
{
  struct script *here;
  
  for (here=begin; here != (struct script *)NULL; here=here->next) {
    if (here->opcode == SC_LABEL && 
	strcmp(here->strprm, newstate) == SAME)
      return here;
  }
  return (struct script *)NULL;
}


/* 
 * do_script() - execute a script 
 */
int do_script(begin)
     struct script *begin;
{
  struct script *curstate, *newstate, *curscr;
  int	 dbgsave;
  char	 tempstr[MAX_SCLINE];
  char   dfname[256];
  char	*c, chr;
  int	 prmlen;
  int    dbfd;
  
  time_t sc_carrtime = 45000;	/* time to wf carr after dial */
  time_t sc_chrdly   = 100;	/* delay time for ttoslow */
  time_t sc_ptime    = 2000;	/* time to allow for pause char */
  time_t sc_wtime    = 10000;	/* time to allow for wait char */
  time_t sc_dtime    = 100;	/* time to allow for each digit */
  time_t sc_dtmo;		/* total time to dial number */
  int    sc_counter;		/* random counter */
  char   sc_pchar    = ',';	/* modem pause character */
  char   sc_wchar    = 'W';	/* modem wait-for-dialtone character */
  time_t sc_begwait;		/* time at beg of wait */
  time_t sc_secs;		/* timeout period */
  
  int    expcnt;
  int    expin;
  static char expbuf[MAX_EXPCT];
  
  dbgsave = Debug;
  curstate = begin;
  
  if (curstate == (struct script *)NULL) 
    return SUCCESS;
  
  _newstate:
  /* 
   * do all of curstate's actions.  Enter with curstate pointing
   * to a label entry
   */
  expin = 0;
  
  for (curscr = curstate->next; /* point to 1st scr after label */
       (curscr != (struct script *)NULL) &&  /* do until end of scr */
       (curscr->opcode != SC_LABEL);		/* or next label */
       curscr = curscr->next) 
    {
      expcnt = 0;
      switch (curscr->opcode) 
	{
	case SC_LABEL:
	  logit("Script proc err", curstate->strprm);
	  return FAIL;
	  
	case SC_FLSH:
	  DEBUG(DB_LGII, "Flushing typeahead buffer\n", 0);
	  ttflui();
	  break;
	  
	case SC_CDLY:
	  sc_chrdly = curscr->intprm;
	  DEBUG(DB_LGII, "Set chrdly to %d\n", sc_chrdly);
	  break;
	  
	case SC_PCHR:
	  sc_pchar = *(curscr->strprm);
	  DEBUG(DB_LGII, "Set pause char to %c\n", sc_pchar);
	  break;
	  
	case SC_PTIM:
	  sc_ptime = curscr->intprm;
	  DEBUG(DB_LGII, "Set pause time to %d\n", sc_ptime);
	  break;
	  
	case SC_WCHR:
	  sc_wchar = *(curscr->strprm);
	  DEBUG(DB_LGII, "Set wait char to %c\n", sc_wchar);
	  break;
	  
	case SC_WTIM:
	  sc_wtime = curscr->intprm;
	  DEBUG(DB_LGII, "Set wait time to %d\n", sc_wtime);
	  break;
	  
	case SC_ZERO:
	  sc_counter = 0;
	  DEBUG(DB_LGII, "Set counter to %d\n", sc_counter);
	  break;
	  
	case SC_INCR:
	  sc_counter++;
	  DEBUG(DB_LGII, "Incr counter to %d\n", sc_counter);
	  break;
	  
	case SC_WAIT:
	  DEBUG(DB_LGII, "Sleeping %d tenth-secs\n", curscr->intprm);
	  msleep(curscr->intprm);
	  break;
	  
	case SC_DTIM:
	  sc_dtime = curscr->intprm;
	  DEBUG(DB_LGII, "Digit time is %d\n", sc_dtime);
	  break;
	  
	case SC_CTIM:
	  sc_carrtime = curscr->intprm;
	  DEBUG(DB_LGII, "Carrier time is %d\n", sc_carrtime);
	  break;
	  
	case SC_EXIT:
	  Debug = dbgsave;
	  DEBUG(DB_LGI, "Script ended successfully\n", 0);
	  return SUCCESS;
	  
	case SC_FAIL:
	  Debug = dbgsave;
	  if (DEBUG_LEVEL(DB_LGI) && dbf != NULL)
	    fprintf(dbf, "Script failed\n");
	  else if (expin)
	    charlog(expbuf, expin, DB_LOG, 
		    "Script failed.  Last received data");
	  return FAIL;
	  
	case SC_LOG:
	  logit(curscr->strprm, "");
	  break;
	  
	case SC_LOGE:
	  logit("ERROR: ", curscr->strprm);
	  break;
	  
	case SC_DBOF:
	  /*
	   * If the debug file name does not begin with "/", then
	   * we prepend the LOG_DIR to the string. Then CREATE the
	   * file. This WIPES OUT previous logs. 
	   */
	  *dfname = '\0';	/* Zero name string */
	  if(curscr->strprm[0] != '/')
	    strcat(dfname, LOG_DIR); /* Prepend default directory */
	  strcat(dfname, curscr->strprm); /* Add given string */
	  DEBUG(DB_LGII, "Open debug file %s\n", dfname);
	  if ((dbfd = creat (dfname, 0600)) <= 0)
	    {
	      logit("Failed to create debug log %s", dfname);
	      perror("");
	      return FAIL;
	    }
	  if ((dbf = fdopen(dbfd, "w")) == NULL)
	    {
	      logit("Failed to open debug log fildes.", "");
	      perror("");
	      return FAIL;
	    }
	  break;
	  
	case SC_DBG:
	  DEBUG(DB_LGI, "<%s>\n", curscr->strprm);
	  break;
	  
	case SC_DBGE:
	  DEBUG(DB_LGIE, "ERROR: <%s>\n", curscr->strprm);
	  break;
	  
	case SC_DBST:
	  Debug |= curscr->intprm;
	  DEBUG(DB_LGII, "Debug mask set to %04o (octal)\n", Debug);
	  break;
	  
	case SC_DBCL:
	  Debug &= ~(curscr->intprm);
	  DEBUG(DB_LGII, "Debug mask set to %04o (octal)\n", Debug);
	  break;
	  
	case SC_BRK:
	  DEBUG(DB_LGI, "Sending break\n", 0);
	  ttbreak();
	  break;
	  
	case SC_HANG:
	  DEBUG(DB_LGI, "Dropping DTR\n", 0);
	  tthang();
	  break;
	  
	case SC_7BIT:
	  DEBUG(DB_LGI, "Enabling 7-bit stripping\n", 0);
	  tt7bit(TRUE);
	  break;
	  
	case SC_8BIT:
	  DEBUG(DB_LGI, "Disabling 7-bit stripping\n", 0);
	  tt7bit(FALSE);
	  break;
	  
	case SC_PNON:
	  DEBUG(DB_LGI, "Setting 8-bit, no parity\n", 0);
	  ttpar(NONE);
	  break;
	  
	case SC_PEVN:
	  DEBUG(DB_LGI, "Setting 7-bit, even parity\n", 0);
	  ttpar(EVEN);
	  break;
	  
	case SC_PODD:
	  DEBUG(DB_LGI, "Setting 7-bit, odd parity\n", 0);
	  ttpar(ODD);
	  break;
	  
	case SC_IFBL:
	  if (ttblind()) 
	    {
	      DEBUG(DB_LGI, "Blind mux,\n", 0);
	      goto _chgstate;
	    }
	  break;
	  
	case SC_IFBG:
	  if (ttblind() && sc_counter > curscr->intprm) 
	    {
	      DEBUG(DB_LGI, "Blind mux & ctr > %d\n", 
		    curscr->intprm);
	      goto _chgstate;
	    }
	  break;
	  
	case SC_IFGT:
	  if (sc_counter > curscr->intprm) 
	    {
	      DEBUG(DB_LGI, "Counter > %d\n", curscr->intprm);
	      goto _chgstate;
	    }
	  break;
	  
	case SC_GOTO:
	  _chgstate:
	  DEBUG(DB_LGI, "Changing to state %s\n",
		curscr->newstate);
	  curstate = find_state(begin, curscr->newstate);
	  if (curstate == NULL) 
	    {
	      logit("New state not found",
		    curscr->newstate);
	      return FAIL;
	    }
	  goto _newstate;
	  
	case SC_SEND:
	  ttoslow(curscr->strprm, curscr->intprm, sc_chrdly);
	  break;
	  
	case SC_TELN:
	  if (curscr->intprm > paramc - 1)
	    {
	      sprintf(tempstr, "telno - param #%d", curscr->intprm);
	      logit(tempstr, " not present");
	      return FAIL;
	    }
	  strcpy(telno, paramv[curscr->intprm]);
	  DEBUG(DB_LGII, "telno set to %s\n", telno);
	  break;
	  
	case SC_SNDP:
	  if (curscr->intprm > paramc - 1)
	    {
	      sprintf(tempstr, "sendstr - param #%d", curscr->intprm);
	      logit(tempstr, " not present");
	      return FAIL;
	    }
	  prmlen = xlat_str(tempstr, paramv[curscr->intprm]);
	  ttoslow(tempstr, prmlen, sc_chrdly);
	  break;
	  
	case SC_IF1P:
	  if (curscr->intprm < paramc)
	    goto _chgstate;
	  break;
	  
	case SC_IF0P:
	  if (curscr->intprm >= paramc)
	    goto _chgstate;
	  break;
	  
	case SC_DIAL:
	  if(telno[0] == '\0')
	    {
	      logit("telno not set", "");
	      return(FAIL);
	    }
	  /*
	   * Compute and set a default timeout for the 'timeout'
	   * command. Some parameters in this computation may be
	   * changed by the script. See the man page xchat(8) for
	   * details.
	   */
	  sc_dtmo = (sc_dtime+sc_chrdly)*strlen(telno) 
	    + sc_carrtime;
	  c=strcpy(tempstr, telno);
	  for (; *c!='\0'; c++) 
	    {
	      if (*c == 'W') 
		{
		  *c = sc_wchar;
		  sc_dtmo += sc_wtime;
		}
	      else if (*c == 'P') 
		{
		  *c = sc_pchar;
		  sc_dtmo += sc_ptime;
		}
	    }
	  DEBUG(DB_LGI, "Dialing, default timeout is %d millisecs\n", sc_dtmo);
	  ttoslow(tempstr, 0, sc_chrdly);
	  break;
	  
	case SC_TIMO:	/* these are "expects", don't bother */
	case SC_XPCT:	/* with them yet, other than noting that */
	case SC_CARR:	/* they exist */
	case SC_XPST:
	  expcnt++;
	  break;
	}
      
    }
  
  /* we've done the current state's actions, now do its expects, if any */
  
  if (expcnt == 0) 
    {
      if (curscr != (struct script *)NULL &&  
	  (curscr->opcode == SC_LABEL)) 
	{
	  curstate = curscr;
	  DEBUG(DB_LGI, "Fell through to state %s\n",
		curstate->strprm);
	  goto _newstate;
	}
      else 
	{
	  logit("No way out of state", curstate->strprm);
	  return FAIL;
	}
    }
  
  time(&sc_begwait);	/* log time at beg of expect */
  DEBUG(DB_LGI, "Doing expects for state %s\n", curstate->strprm);
  charlog((char *)NULL, 0, DB_LGI, "Received");
  
  while (1) 
    {
      chr = xgetc(1);		/* Returns upon char input or 1 sec. tmo */
      
      charlog(&chr, 1, DB_LGI, (char *)NULL);
      
      if (chr != EOF) 
	{
	  if (expin < MAX_EXPCT) 
	    {
	      expbuf[expin++] = chr & 0x7f;
	    }
	  else 
	    {
	      strncpy(expbuf, &expbuf[1], MAX_EXPCT-1);
	      expbuf[MAX_EXPCT-1] = chr & 0x7f;
	    }
	}
      
      /* for each entry in the current state... */
      
      for (curscr = curstate->next; 
	   (curscr != (struct script *)NULL) &&
	   (curscr->opcode != SC_LABEL);
	   curscr = curscr->next) 
	{
	  
	  switch (curscr->opcode) 
	    {
	    case SC_TIMO:
	      sc_secs = curscr->intprm;
	      if (sc_secs == 0)
		sc_secs = sc_dtmo;
	      sc_secs /= 1000;
	      if (time(NULL)-sc_begwait > sc_secs) 
		{
		  DEBUG(DB_LGI,
			"\nTimed out (%d secs)\n", sc_secs);
		  goto _chgstate;
		}
	      break;
	      
	    case SC_CARR:
	      if (ttcd()) 
		{
		  DEBUG(DB_LGI, "\nGot carrier\n", 0);
		  goto _chgstate;
		}
	      break;
	      
	    case SC_HUPS:
	      if (fShangup) 
		{
		  DEBUG(DB_LGI, "\nGot data set hangup\n", 0);
		  goto _chgstate;
		}
	      break;
	      
	    case SC_XPCT:
	      if ((expin >= curscr->intprm) &&
		  (strncmp(curscr->strprm, 
			   &expbuf[expin - curscr->intprm],
			   curscr->intprm) == SAME)) 
		{
		  charlog(curscr->strprm, curscr->intprm,
			  DB_LGI, "Matched");
		  goto _chgstate;
		}
	      break;
	      
	    }
	}
    }
}
	      /* New opcode added by hag@eddie.mit.edu for expecting a 
		 parameter supplied string */
	     case SC_XPST:
	      if(curscr->intprm >paramc-1)
	      {
		sprintf(tempstr,"expectstr - param#%d",curscr->intprm);
		logit(tempstr, " not present");
		return(FAIL);
	      }
	      prmlen=xlat_str(tempstr,paramv[curscr->intprm]);
	      if((expin >= prmlen) &&
		 (strncmp(tempstr,&expbuf[expin-prmlen],
			  prmlen) == SAME))
	      {
		charlog(tempstr,prmlen,DB_LGI, "Matched");
		goto _chgstate;
	      }
	      break;
/*
 * SIGNAL HANDLERS
 */

/*
 * usignal - generic signal catcher
 */
static int usignal(isig)
     int isig;
{
  DEBUG(DB_LOG, "Caught signal %d. Exiting...\n", isig);
  restore_tty();
  exit(FAIL);
}

/*
 * uhup - HUP catcher
 */
static int uhup(isig)
     int isig;
{
  DEBUG(DB_LOG, "Data set hangup.\n");
  fShangup = TRUE;
}

/*
 * TERMINAL I/O ROUTINES
 */

/*
 * xgetc - get a character with timeout
 *
 * Assumes that stdin is opened on a terminal or TCP socket 
 * with O_NONBLOCK. 
 */
static char xgetc(tmo)
int tmo;			/* Timeout, seconds */
{
  char c;
  struct timeval s;
  int f = 1;			/* Select on stdin */
  int result;

  if(read(0, &c, 1)  <= 0)	/* If no data available */
    {
      s.tv_sec = (long)tmo;
      s.tv_usec = 0L;
      if(select (1, &f, (int *) NULL, &f, &s) == 1)
	read(0, &c, 1);
      else
	c = '\377';
    }

  return(c);
}

/* 
 * Pause for an interval in milliseconds
 */
void msleep(msec)
long msec;
{

#if HAVE_USLEEP
  if(msec == 0)			/* Skip all of this if delay = 0 */
    return;
  usleep (msec * (long)1000);
#endif /* HAVE_USLEEP */

#if HAVE_NAPMS
  if(msec == 0)			/* Skip all of this if delay = 0 */
    return;
  napms (msec);
#endif /* HAVE_NAPMS */

#if HAVE_NAP
  if(msec == 0)			/* Skip all of this if delay = 0 */
    return;
  nap (msec);
#endif /* HAVE_NAP */

#if HAVE_POLL
  struct pollfd sdummy;

  if(msec == 0)
    return;
  /* 
   * We need to pass an unused pollfd structure because poll checks
   * the address before checking the number of elements.
   */
  poll (&sdummy, 0, msec);
#endif /* HAVE_POLL */

#if USE_SELECT_TIMER
  struct timeval s;

  if(msec == 0)
    return;
  s.tv_sec = msec / 1000L;
  s.tv_usec = (msec % 1000L) * 1000L;
  select (0, (int *) NULL, (int *) NULL, (int *) NULL, &s);
#endif /* USE_SELECT_TIMER */

#if ! HAVE_NAPMS && ! HAVE_NAP && ! HAVE_USLEEP && \
    ! HAVE_POLL && ! USE_SELECT_TIMER
  if(msec == 0)
    return;
  sleep (1);			/* Sleep for a whole second (UGH!) */
#endif /* HAVE_ and USE_ nothing */
}

/*
 * Debugging output
 */
static void DEBUG(level, msg1, msg2)
int level;
char *msg1, *msg2;
{
  if ((dbf != NULL) && DEBUG_LEVEL(level))
    fprintf(dbf, msg1, msg2);
}

/*
 * charlog - log a string of characters
 *
 * SPECIAL CASE: msg=NULL, len=1 and msg[0]='\377' gets logged
 *               when read does its 1 sec. timeout. Log "<1 sec.>"
 *               so user can see elapsed time 
 */
static void charlog(buf, len, mask, msg)
char *buf;
int len, mask;
char *msg;
{
  char tbuf[256];

  if (DEBUG_LEVEL(mask) && dbf != NULL)
    {
      if(msg == (char *)NULL)
	msg = "";
      strncpy(tbuf, buf, len);
      tbuf[len] = '\0';
      if(len == 1 && tbuf[0] == '\377')
	strcpy(tbuf, "<1 sec.>");
      fprintf(dbf, "%s %s\n", msg, tbuf);
    }
}

/*
 * setup_tty()
 *
 * Save current tty settings, then set up raw, single
 * character input processing, with 7-bit stripping.
 */
static void setup_tty()
{
  register int i;

  ioctl(0, TCGETA, &old);

  new = old;

  for(i = 0; i < 7; i++)
    new.c_cc[i] = '\0';
  new.c_cc[VMIN] = 0;		/* MIN = 0, use requested count */
  new.c_cc[VTIME] = 10;		/* TIME = 1 sec. */
  new.c_iflag = ISTRIP;		/* Raw mode, 7-bit stripping */
  new.c_lflag = 0;		/* No special line discipline */

  ioctl(0, TCSETA, &new);
}

/*
 * restore_tty() - restore signal handlers and tty modes on exit.
 */
static void restore_tty(sig)
int sig;
{
  ioctl(0, TCSETA, &old);
  return;
}

/* 
 * ttoslow() - Send characters with pacing delays
 */
static void ttoslow(s, len, delay)
     char *s; 
     int len;
     time_t delay; 
{
  int i;
  
  if (len == 0)
    len = strlen(s);
  
  charlog (s, len, DB_LGI, "Sending slowly");
  
  for (i = 0; i < len; i++, s++)
    {
      write(1, s, 1);
      msleep(delay);
    }
}

/*
 * ttflui - flush input buffer
 */
static void ttflui()
{
  if(isatty(0))
    (void) ioctl ( 0, TCFLSH, 0);
}

/*
 * ttcd - Test if carrier is present
 *
 * NOT IMPLEMENTED. I don't know how!!!
 */
static int ttcd()
{
  return TRUE;
}

/*
 * tthang - Force DTR low for 1-2 sec.
 */
static void tthang()
{
  if(!isatty())
    return;

#ifdef TCCLRDTR
  (void) ioctl (1, TCCLRDTR, 0);
  sleep (2);
  (void) ioctl (1, TCSETDTR, 0);
#endif

  return;
}

/*
 * ttbreak - Send a "break" on the line
 */
static void ttbreak()
{
  (void) ioctl (1, TCSBRK, 0);
}

/*
 * ttblind - return TRUE if tty is "blind"
 *
 * NOT IMPLEMENTED - Don't know how!!!
 */
static int ttblind()
{
  return FALSE;
}

/*
 * tt7bit - enable/disable 7-bit stripping on line
 */
static void tt7bit(enable)
     int enable;
{
  if(enable)
    new.c_iflag |= ISTRIP;
  else
    new.c_iflag &= ~ISTRIP;

  ioctl(0, TCSETA, &new);
}

/*
 * ttpar - Set parity mode on line. Ignore parity errors on input.
 */
static void ttpar(mode)
     int mode;
{
  switch(mode)
    {
    case NONE:
      new.c_iflag &= ~(INPCK | IGNPAR);
      new.c_cflag &= ~(CSIZE | PARENB | PARODD);
      new.c_cflag |= CS8;
      break;

    case EVEN:
      new.c_iflag |= (INPCK | IGNPAR);
      new.c_cflag &= ~(CSIZE | PARODD);
      new.c_cflag |= (CS7 | PARENB);

      break;

    case ODD:
      new.c_iflag |= (INPCK | IGNPAR);
      new.c_cflag &= ~(CSIZE);
      new.c_cflag |= (CS7 | PARENB | PARODD);
      break;
    }

  ioctl(0, TCSETA, &new);
}







