/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*  ci  -- command interpreter
 *
 *  Usage (etc.)
 *
 * HISTORY
 * 22-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Removed checks for VIRTUE window manager.  If they don't like
 *	it then they can fix the more program.
 *
 * 08-May-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Increased MAXENTRIES and MAXHELPS from 200 to 400.
 *
 * 30-Apr-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Adapted for 4.2 UNIX.  Added calls to check for
 *	using window manager of VIRTUE.
 *
 * 29-Apr-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added two small bug fixes (courtesy of Richard Cohn).
 *
 * 14-Aug-84  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added fflush(stdout) after printing prompt, before asking for input line.
 *
 * 01-Jul-83  Steven Shafer (sas) at Carnegie-Mellon University
 *	Bug fix: whitespace now required before ">filename" and not permitted
 *	within or after filename.
 *
 * 06-Jun-83  Steven Shafer (sas) at Carnegie-Mellon University
 *	Bug fix: added line to initialize "redirected" to 0.
 *
 * 20-May-83  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added quiet bits CINOSEM, CINOFILE, CIFIRSTEQUAL to allow user to
 *	have special characters ; > = treated as normal data (except =
 *	after first argument, which still means "variable assignment").
 *	Also added output redirection via >filename on input line.
 *
 * 07-Mar-83  Dave McKeown (dmm) at Carnegie-Mellon University
 *	(Slight alterations by Steve Shafer.)
 *	Made cidepth a global, used for recursive and nested calls to
 *	ci(), and accessable to the user.  Added '@x' command, similar
 *	to '^x' except that the previous command interpreter name is
 *	remembered and after 'x' is executed, the previous command
 *	interpreter is reinvoked.  Users who plan to use this feature
 *	must save the name of the previous ci in global variable 
 *	'ciprev' after exit from the ci().  ie.  
 *		ci(.........);
 *		strcpy(ciprev,"ci-name");
 *	Added ci state CICMDNOINDENT to allow for no indentation of the
 *	command line prompt based on cidepth.
 *	Reduced amount of indentation on source code.
 *	Bug: the "depth" argument is now a no-op, retained for backward
 *	compatibility.  Cidepth is initialized to zero, and incremented
 *	upon invocation of a ci().  If cidepth is <1 then you are not
 *	in a ci() instantiation.
 *
 * 21-Feb-83  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added up-arrow (^) command (and variable cinext).  ^x is used when
 *	you have a ci program in which one command invokes ci with a
 *	new set of commands (i.e. a subsystem of the program).  Inside the
 *	subsystem, ^x will exit the subsystem, and cause the main level
 *	to execute the command line "x" before reading more input lines.
 *	The cinext variable is used to implement this.  Cinext can also be
 *	used by any user code which desires to force ci to execute a
 *	specific command before reading more input from the current file.
 *
 * 16-Jul-82  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added extra code in _ci_help to eliminate duplicate help file
 *	names.  This way, if several directories are specified and there
 *	is are files with the same name in more than one directory, only
 *	the first of each file will be included in the help list.
 *	
 *	It would have been nice to do this after the qsort instead of
 *	before (in ci_help).  However, qsort does not ensure that
 *	"equivalent" entries are kept in the same relative
 *	order; thus there would be no way to ensure that the
 *	file being used was the first such file found.
 *
 * 07-Jul-82  William Chiles (wpc) at Carnegie-Mellon University
 *	Modified so that "!" invokes shell commands from the type of 
 *      shell specified by the environment variable SHELL.  If SHELL
 *	is not defined the standard shell is used.
 *
 * 21-Sep-81  Steven Shafer (sas) at Carnegie-Mellon University
 *	Increased LINELENGTH (input buffer length) to 1100 to satisfy
 *	voracious requirements of a certain user whose name I won't mention
 *	but whose initials are "faa".
 *
 * 08-Oct-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added class variables:  ci_tclass cases in ci_show and ci_set.
 *	Also added CICMDFPEEK in addition to existing CISETPEEK.
 *
 * 22-May-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Ci now sorts help topics into alphabetical order.  Some interrupt
 *	handling has been added, but there are bugs, for example, when
 *	you interrupt "*" (the listing of command names).  The right thing
 *	happens, but bogus messages are printed.
 *
 * 16-Apr-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Ci now prints lists of names with prstab().  This uses multiple
 *	columns when appropriate.
 *
 * 12-Mar-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added code to skip over leading blanks and tabs in the argument list
 *	when executing commands, and setting and displaying variables.
 *	Also fixed meta-help, which mysteriously disappeared.
 *
 * 19-Feb-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Added "if (0) del();" to force del() routine to be loaded.  This is
 *	the safest way I know of to define the external int "_del_".  If you
 *	don't believe it, think again about separately compiled files.
 *
 * 28-Jan-80  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created.  Patterned (somewhat) after ci() on PDP-11.
 *
 */


#include <strings.h>
#include <libc.h>
#include <ci.h>
#include <del.h>

char *index(),*getenv(),*rindex();
extern char _argbreak;
long atol();
double atof();
static int ci_help(), ci_show();
static int _ci_sho(), _ci_set(), ci_set();

/*************************
 ***    M A C R O S    ***
 *************************/

#define LINELENGTH 1100		/* max length of input line */
#define MAXENTRIES 400		/* max entries in entry list */
#define MAXHELPS 400		/* max help files available */
#define METAHELP "/usr/lib/ci.help"	/* standard help file */

/*********************************************
 ***    G L O B A L   V A R I A B L E S    ***
 *********************************************/

int ciquiet = 0;		/* init globals */
int ciexit  = 0;
int cidepth = 0;
int ciback  = 0;		/* for use in '@' command */
FILE *ciinput;

char cinext[LINELENGTH] = "";
char ciprev[LINELENGTH] = "";

static char *delchoice[] = {	/* breakpoint choices */
	"abort		abort command file",
	"breakpoint	break to tty, then resume command file",
	0};

/*************************************
 ***    M A I N   R O U T I N E    ***
 *************************************/

ci (prompt,fil,depth,list,helppath,cmdfpath)
char *prompt;			/* prompt message */
FILE *fil;			/* input file */
int depth;			/* recursion depth */
CIENTRY *list;			/* entry list */
char *helppath;			/* search list for help files */
char *cmdfpath;			/* search list for command files */

{

  FILE *savfile;	  /* input file for calling instance of ci */
  int savquiet, savexit;  /* globals for calling instance of ci */
  char *p,*q,*cmd,*arg;	  /* temps for parsing input */
  int i;			/* temp */
  char line[LINELENGTH];	/* input line buffer */
  int firststmt;	/* temp */
  char *equals,*star;	/* index of = and * in input line */
  char cfnam[200];	/* name of command file */
  char *name[MAXENTRIES];	/* name list for entries */
  char *vname[MAXENTRIES];	/* name list for just variables */
  int vnum[MAXENTRIES];	/* correspondence list for variables */
  int nv;			/* number of variables */
  int helpcmd;		/* "help" command index */
  FILE *newfile;		/* command file just opened */
  char bprompt[100];	/* breakpoint prompt */
  char *tname[MAXENTRIES];	/* temp name list */
  int tnum;		/* # entries in tname */
  char *Shell;            /* holds SHELL value from .login */
  int redirected;	/* 1 iff currently redirected output */
  FILE savestdout;		/* place to save normal std. output */
  FILE *outfile;		/* current output file */
  char *outname;		/* output file name */
  

  /* force del() routine to be declared */
  if (0) del();
  /* save globals on stack */
  cidepth++;	/* bump the global depth, first CI() is 1 */
  savquiet = ciquiet;
  savexit = ciexit;
  savfile = ciinput;
  ciexit = 0;		/* don't exit until this is set */
  ciinput = (fil ? fil : stdin);		/* new input file */

  /* construct name lists for stablk */

  nv = 0;
  for (i=0; list[i].ci_etyp != ci_tend; i++) {
  	name[i] = list[i].ci_enam;
  	if (list[i].ci_etyp != ci_tcmd) {	/* is variable */
  		vname[nv] = name[i];
  		vnum[nv] = i;
  		nv++;
  	}
  }
  helpcmd = i++;		/* force-feed "help" onto list */
  name[helpcmd] = "help";
  name[i] = 0;
  vname[nv] = 0;

  /* loop for input lines */

  redirected = 0;
  while (!ciexit) {

    if (*cinext) {		/* get line from ^ command */
     	if (ciback) { 
      	  sprintf(line,"%s;%s",cinext,ciprev);	
    	  ciback = 0;
	}
    	else {
	  strcpy (line,cinext);
	}
    	strcpy (cinext,"");
    	p = line;
    }
    else {			/* else read file */
    	if ((ciinput == stderr) || (ciinput == stdin) || (!(ciquiet&CICMDFPROMPT))) {
    	  if (!(ciquiet &CICMDNOINDENT)) {
	    for (i=1; i<cidepth; i++) {
		printf ("    ");
	    }
	  }
    	  printf ("%s ",prompt);
	  if ((ciinput == stderr) || (ciinput == stdin))  fflush (stdout);
    	}
    	p = fgets (line,LINELENGTH,ciinput);	/* read input line */
    	if (p == 0) {		/* EOF */
    	  if (_del_) {
    		DELCLEAR;
    		strcpy (line,"");
    		p = line;
    	  }
    	  else {
    		ciexit = 1;
    		if ((ciinput==stdin) || (ciinput==stderr) || 
		  (!(ciquiet&CICMDFECHO))) printf ("\n");
    	  }
    	}
    	else {
    		if ((ciinput != stderr) && (ciinput != stdin) && 
		  (!(ciquiet&CICMDFECHO))) printf ("%s",line);

    		for (p=line; (*p) && (*p != '\n'); p++) ;
    		*p = 0;		/* kill trailing newline */
    		p = line;	/* points to start of line */
    	}
    }

    /* check for redirection of output */

    if (!ciexit) {
	outname = rindex (p,'>');
	    if (outname) {
            if (outname == p || *(outname+1) == 0
             || ((*(outname-1) != ' ') && (*(outname-1) != '\t'))) {
	        outname = 0;
            }
	    else {
	        for (q=outname+1; *q && (*q != ' ') && (*q != '\t'); q++) ;
	        if (*q)  outname = 0;
            }
        }
        if (outname && !(ciquiet&CINOFILE)) {
	    *outname++ = 0;
	    outfile = fopen (outname,"w");
	    if (outfile == 0) {
		    printf ("ci: Can't create output file %s\n",outname);
		    p = "";
	    }
	    else {
		    fflush (stdout);
		    savestdout = *stdout;
		    *stdout = *outfile;
		    redirected = 1;
	    }
        }
    }

    /* loop for each command */

    firststmt = 1;		/* first time through loop */
    while ((!ciexit) && (((ciquiet&CINOSEM) && firststmt) || *(cmd=nxtarg(&p,";")) || _argbreak)) {

	if (ciquiet & CINOSEM) {
	    cmd = p;
	    firststmt = 0;
	}

    	switch (*cmd) {		/* what kind of line? */

    	case ':':		/* :  comment */
    	case 0:			/*    null line */
      	  break;

    	case '!':		/* !  shell command */
    	  cmd = skipover (cmd+1," ");
    	  if ((Shell = getenv("SHELL")) == 0) Shell = "sh";
    	  if (*cmd)	runp (Shell, Shell, "-c", cmd, 0);
    	  else		runp (Shell, Shell, 0);
    	  if (!(ciquiet&CISHEXIT))  printf ("Back to %s\n",prompt);
   	  break;

    	case '?':		/* ?  help */
    	  cmd = skipover (cmd+1," ");
    	  ci_help (cmd,helppath);
    	  break;

    	case '<':		/* <  command file */
    	  arg = cmd + 1;
    	  cmd = nxtarg (&arg,0);	/* parse name */
    	  if (*cmd == 0) printf ("ci: missing filename\n");
    	  else {
    	    if (cmdfpath)  newfile = fopenp (cmdfpath,cmd,cfnam,"r");
    	    else	   newfile = fopen (cmd,"r");

      	    if (newfile == 0) 
    	      printf ("ci: can't open command file %s\n",cmd);
    	    else {
    	      if (!(ciquiet&CICMDFECHO))  printf ("\n");
    	      ci (prompt,newfile,cidepth,list,helppath,cmdfpath);
    	      fclose (newfile);
    	      if (!(ciquiet&CICMDFEXIT))  printf ("End of file\n\n");
    	    }
    	  }
    	  break;
    	
    	case '^':		/* exit and do command */
	case '@':
    	  if (cidepth > 1) {
    	    if (*cmd == '@') ciback = 1;
    	    if (_argbreak == ';')  *(cmd+strlen(cmd)) = ';';
    	    ciexit = 1;
    	    cmd = skipover(cmd+1," ");
    	    strcpy (cinext,cmd);
    	  }
    	  else printf ("ci: ^ not allowed at top level of ci\n");
    	  break;

    	default:		/* list cmds, etc. */
    	  equals = index (cmd,'=');
    	  if (equals == cmd)  cmd++;

    	  if (equals) {
    	    if (*(equals+1) == 0)  *equals = 0;
    	    else  *equals = ' ';
    	  }

    	  arg = cmd;	/* parse first word */
    	  cmd = nxtarg (&arg,0);
	  if ((ciquiet&CIFIRSTEQUAL) && equals && equals>arg) {
		*equals = '=';	/* if user doesn't want extra =, kill */
		equals = 0;
	  }
    	  star = index (cmd,'*');
    	  if (star)  *star = 0;
    	  if (star && equals) {	/* list vars */
    	    printf ("\n");
    	    for (i=0; vname[i]; i++) {
    	      if (stlmatch (vname[i],cmd)) {
    		ci_show (list[vnum[i]],arg,CIPEEK);
    	      }
    	      DELBREAK;
    	    }
    	    printf ("\n");
    	  }
    	  else if (star) {	/* list cmds */
    	    printf ("\n");
    	    tnum = 0;
    	    for (i=0;name[i]; i++) {
    	      if ((i==helpcmd || list[i].ci_etyp == ci_tcmd) && 
		stlmatch (name[i],cmd)) {
    	          tname[tnum++] = name[i];
    	      }
    	    }
    	    tname[tnum] = 0;
    	    prstab (tname);
    	    if (_del_)  {_DELNOTE_}
    	    printf ("\n");
    	  }
    	else if (equals) {	/* set var */
    	  i = stablk (cmd,vname,0);
    	  if (i >= 0)  ci_set (list[vnum[i]],skipover(arg," \t"));
    	}
      else {
    	i = stablk (cmd,name,0);

    	if (i == helpcmd) ci_help (arg,helppath);
    	else if (i >= 0) {
      	  if (list[i].ci_etyp == ci_tcmd) {
    	    (* (int(*)()) (list[i].ci_eptr)) (skipover(arg," \t"));
    	  }
    	  else ci_show (list[i],skipover(arg," \t"),CISHOW);
    	}
      }
    }

    	/* end of command */

    	/* DEL trapping */

   if (_del_) {
   	if (ciinput == stdin) {
      	  DELCLEAR;	/* already at tty level */
    	  }
    	else {
     	  _del_ = 0;
    	  i = getstab ("INTERRUPT:  abort or breakpoint?",delchoice,"abort");
    	  if (i == 0) ciexit = 1; 	/* abort */
    	  else {		/* breakpoint */
    	    sprintf (bprompt,"Breakpoint for %s",prompt);
    	    ci (bprompt,0,cidepth,list,helppath,cmdfpath);
    	    }
    	  }
    	}

    	/* end of loop for commands */

    }

    /* end of loop for lines of input file */

    if (redirected) {
	fflush (stdout);
	fclose (stdout);
	*stdout = savestdout;
	redirected = 0;
    }

  }

  /* restore globals */
  cidepth --;		/* update current depth */
  ciinput = savfile;
  ciquiet = savquiet;
  ciexit = savexit;
}

/********************************************
 ***    P R I N T   H E L P   F I L E     ***
 ********************************************/

static int _h_found;  	/* how many matching names? */
static char **_h_list;  	/* list of matching names */
static char (*_h_nlist)[20];  /* list of filename part of names */

static int _ci_help (filspec)
/* called by searchp to expand filspec, adding names to _h_list */
char *filspec;
{
  register int i,j,result;
  char dir[200];

  result = expand (filspec, _h_list + _h_found, MAXHELPS - _h_found);
  if (result > 0) { 
    for (i=0; i<result; ) {		/* elim duplicates */
      path (_h_list[i+_h_found],dir,_h_nlist[i+_h_found]);
      for (j=0;
  	  j<_h_found && strcmp(_h_nlist[j],_h_nlist[i+_h_found]) != 0;
          j++) ;
  	if (j < _h_found) {	/* is duplicate */
  	  --result;
  	  strcpy (_h_list[i+_h_found],_h_list[result+_h_found]);
  	  }
  	else i++;	/* isn't duplicate */
  	}

  _h_found += result;
  }
  
  return (1);	/* keep searching */
}

/*  for use in sorting help file names */
static ci_hcomp (p,q)
char **p,**q;
{
  char dir[200],file1[20],file2[20];
  path ((*p),dir,file1);
  path ((*q),dir,file2);
  return (strcmp(file1,file2));
}

static ci_help (topic,helppath)
char *topic,*helppath;
{
  char *fnames[MAXHELPS];		/* names of matching files */
  char names[MAXHELPS][20];	/* stripped filenames */
  char *nptr[MAXHELPS+1];		/* list of ptrs for stablk */
  char dir[200];			/* temp */
  char shstr[300];		/* shell string for system */
  int i;
  char *star;
  FILE *f;

  if (*topic == 0) {		/* wants meta-help */
  	f = fopen (METAHELP,"r");
  	if (f == 0) {
  		printf ("Yikes!!  Can't open standard help file!\n");
  	}
  	else {
  		printf ("\n");
		runp("more","more",METAHELP,0);
  		if (_del_)  {_DELNOTE_}
  		printf ("\n");
  		fclose (f);
  	}
  	if (helppath && (*helppath) && (!getbool("Do you want a list of help topics?",1))) {
  		return;
  	}
  }
  else {				/* chop at * */
  	star = index (topic,'*');
  	if (star)  *star = 0;
  }

  if (helppath == 0) {		/* no help at all */
  	printf ("Sorry, no specific help is available for this program.\n");
  }
  else {
  	_h_found = 0;
  	_h_list = fnames;
  	_h_nlist = names;
  	searchp (helppath,"*",dir,_ci_help);	/* find file names */
  	qsort (fnames,_h_found,sizeof(char *),ci_hcomp);

  	for (i=0; i<_h_found; i++) {	/* strip pathnames */
  		path (fnames[i],dir,names[i]);
  		nptr[i] = names[i];
  	}
  	nptr[i] = 0;

  	if (*topic) {		/* request some topic */
  	  if (_h_found == 0) {
  	    printf ("No help for %s.  Type '?*' for list of help messages.\n",topic);
  	    }
  	  else {
  	    i = stablk (topic,nptr,1);
  	    if (i < 0)  i = stabsearch (topic,nptr,0);
  	    if (i >= 0) {
  	      f = fopen (fnames[i],"r");
  	      if (f == 0) 
  		printf ("Yikes!  Can't open help file %s\n",fnames[i]);
  	      else {
  	        printf ("\n");
		runp("more","more",fnames[i],0);
  		if (_del_)  {_DELNOTE_}
  		printf ("\n");
  		fclose (f);
  		}
  	      }
  	    }
  	}
  	else {			/* request topic list */
  	  printf ("\nHelp is available for these topics:\n");
  	  prstab (nptr);
  	  if (_del_)  {_DELNOTE_}
  	  printf ("\n");
  	}

  	for (i=0; i<_h_found; i++)  free (fnames[i]);

  }
}

/*********************************************************
 ***    S H O W   V A L U E   O F   V A R I A B L E    ***
 *********************************************************/

static ci_show (entry,arg,mode)
CIENTRY entry;  		/* entry to display */
char *arg;  		/* arg for variable procedures */
CIMODE mode;  		/* mode (CIPEEK or CISHOW) */
{
  if (entry.ci_etyp == ci_tproc) {	/* procedure */
  	(* (int(*)()) (entry.ci_eptr)) (mode,arg);
  }
  else if (entry.ci_etyp == ci_tclass) {	/* class variable */
  	(* (int(*)()) (entry.ci_eptr)) (mode,arg,entry.ci_evar,entry.ci_enam);
  }
  else {
  	printf ("%-14s \t",entry.ci_enam);
  	_ci_sho (entry.ci_etyp, entry.ci_eptr);
  	printf ("\n");
  }
}

static _ci_sho (etype,eptr)
ci_type etype;
ci_union *eptr;
{
  int i;
  unsigned int u;

  switch (etype) {

  case ci_tint:
  	printf ("%d",eptr->ci_uint);
  	break;
  case ci_tshort:
  	printf ("%d",eptr->ci_ushort);
  	break;
  case ci_tlong:
  	printf ("%D",eptr->ci_ulong);
  	break;
  case ci_toct:
  	if (eptr->ci_uoct)  printf ("0");
  	printf ("%o",eptr->ci_uoct);
  	break;
  case ci_thex:
  	if (eptr->ci_uhex)  printf ("0x");
  	printf ("%x",eptr->ci_uhex);
  	break;
  case ci_tdouble:
  	printf ("%g",eptr->ci_udouble);
  	break;
  case ci_tfloat:
  	printf ("%g",eptr->ci_ufloat);
  	break;
  case ci_tbool:
  	if (eptr->ci_ubool)	printf ("yes");
  	else			printf ("no");
  	break;
  case ci_tstring:
  	printf ("%s",(char *)eptr);
  	break;
  case ci_tcint:
  	printf ("%d",*(eptr->ci_ucint.ci_ival));
  	break;
  case ci_tcshort:
  	printf ("%d",*(eptr->ci_ucshort.ci_sval));
  	break;
  case ci_tclong:
  	printf ("%D",*(eptr->ci_uclong.ci_lval));
  	break;
  case ci_tcoct:
  	u = *(eptr->ci_ucoct.ci_uval);
  	if (u)  printf ("0");
  	printf ("%o",u);
  	break;
  case ci_tchex:
  	u = *(eptr->ci_uchex.ci_uval);
  	if (u)  printf ("0x");
  	printf ("%x",u);
  	break;
  case ci_tcdouble:
  	printf ("%g",*(eptr->ci_ucdouble.ci_dval));
  	break;
  case ci_tcfloat:
  	printf ("%g",*(eptr->ci_ucfloat.ci_fval));
  	break;
  case ci_tcbool:
  	i = *(eptr->ci_ucbool.ci_bval);
  	if (i)	printf ("yes");
  	else	printf ("no");
  	break;
  case ci_tcchr:
  	i = *(eptr->ci_ucchr.ci_cval);
  	printf ("%c",eptr->ci_ucchr.ci_cleg[i]);
  	break;
  case ci_tcstring:
  	printf ("%s",eptr->ci_ucstring.ci_pval);
  	break;
  case ci_tctab:
  	i = *(eptr->ci_ucstab.ci_tval);
  	printf ("%s",eptr->ci_ucstab.ci_ttab[i]);
  	break;
  case ci_tcsearch:
  	i = *(eptr->ci_ucsearch.ci_tval);
  	printf ("%s",eptr->ci_ucsearch.ci_ttab[i]);
  	break;
  default:
  	printf ("Yeek!  Illegal cientry type %d!\n",(int) etype);
  }
}

/*************************************************************
 ***    A S S I G N   V A L U E   T O   V A R I A B L E    ***
 *************************************************************/

static ci_set (entry,arg)
CIENTRY entry;
char *arg;
{
  if (entry.ci_etyp == ci_tproc) {	/* variable procedure */
  	(* (int(*)()) (entry.ci_eptr)) (CISET,arg);
  }
  else if (entry.ci_etyp == ci_tclass) {	/* class variable */
  	(* (int(*)()) (entry.ci_eptr)) (CISET,arg,entry.ci_evar,entry.ci_enam);
  }
  else {
  	_ci_set (entry.ci_etyp, entry.ci_eptr, arg);
  	if (!(ciquiet & (((ciinput==stdin)||(ciinput==stderr)) ? CISETPEEK : CICMDFPEEK)))
  		ci_show (entry,arg,CIPEEK);
  }
}

static _ci_set (etype,eptr,arg)
ci_type etype;
ci_union *eptr;
char *arg;
{
  int i;
  unsigned int u;
  char *p;

  if (etype == ci_tstring) {
  	strcpy ((char *)eptr,arg);
  	return;
  }
  if (etype == ci_tcstring) {
  	strarg (&arg, ";", eptr->ci_ucstring.ci_pmsg,
  	eptr->ci_ucstring.ci_pval,eptr->ci_ucstring.ci_pval);
  	return;
  }

  p = arg;		/* parse first word */
  arg = nxtarg (&p,0);

  switch (etype) {

  case ci_tint:
  	eptr->ci_uint = atoi (arg);
  	break;
  case ci_tshort:
  	eptr->ci_ushort = atoi (arg);
  	break;
  case ci_tlong:
  	eptr->ci_ulong = atol (arg);
  	break;
  case ci_toct:
  	eptr->ci_uoct = atoo (arg);
  	break;
  case ci_thex:
  	if (stlmatch(arg,"0x") || stlmatch(arg,"0X"))  arg += 2;
  	eptr->ci_uhex = atoh (arg);
  	break;
  case ci_tdouble:
  	eptr->ci_udouble = atof (arg);
  	break;
  case ci_tfloat:
  	eptr->ci_ufloat = atof (arg);
  	break;
  case ci_tbool:
  	eptr->ci_ubool = (index("yYtT",*arg) != 0);
  	break;
  case ci_tcint:
  	*(eptr->ci_ucint.ci_ival) =
  	intarg (&arg,0,eptr->ci_ucint.ci_imsg,eptr->ci_ucint.ci_imin,
  	eptr->ci_ucint.ci_imax,*(eptr->ci_ucint.ci_ival));
  	break;
  case ci_tcshort:
  	*(eptr->ci_ucshort.ci_sval) =
  	shortarg (&arg,0,eptr->ci_ucshort.ci_smsg,eptr->ci_ucshort.ci_smin,
  	eptr->ci_ucshort.ci_smax,*(eptr->ci_ucshort.ci_sval));
  	break;
  case ci_tclong:
  	*(eptr->ci_uclong.ci_lval) =
  	longarg (&arg,0,eptr->ci_uclong.ci_lmsg,eptr->ci_uclong.ci_lmin,
  	eptr->ci_uclong.ci_lmax,*(eptr->ci_uclong.ci_lval));
  	break;
  case ci_tcoct:
  	*(eptr->ci_ucoct.ci_uval) =
  	octarg (&arg,0,eptr->ci_ucoct.ci_umsg,eptr->ci_ucoct.ci_umin,
  	eptr->ci_ucoct.ci_umax,*(eptr->ci_ucoct.ci_uval));
  	break;
  case ci_tchex:
  	*(eptr->ci_uchex.ci_uval) =
  	hexarg (&arg,0,eptr->ci_uchex.ci_umsg,eptr->ci_uchex.ci_umin,
  	eptr->ci_uchex.ci_umax,*(eptr->ci_uchex.ci_uval));
  	break;
  case ci_tcdouble:
  	*(eptr->ci_ucdouble.ci_dval) =
  	doublearg (&arg,0,eptr->ci_ucdouble.ci_dmsg,eptr->ci_ucdouble.ci_dmin,
  	eptr->ci_ucdouble.ci_dmax,*(eptr->ci_ucdouble.ci_dval));
  	break;
  case ci_tcfloat:
  	*(eptr->ci_ucfloat.ci_fval) =
  	floatarg (&arg,0,eptr->ci_ucfloat.ci_fmsg,eptr->ci_ucfloat.ci_fmin,
  	eptr->ci_ucfloat.ci_fmax,*(eptr->ci_ucfloat.ci_fval));
  	break;
  case ci_tcbool:
  	*(eptr->ci_ucbool.ci_bval) =
  	boolarg (&arg,0,eptr->ci_ucbool.ci_bmsg,*(eptr->ci_ucbool.ci_bval));
  	break;
  case ci_tcchr:
  	*(eptr->ci_ucchr.ci_cval) =
  	chrarg (&arg,0,eptr->ci_ucchr.ci_cmsg,eptr->ci_ucchr.ci_cleg,
  	eptr->ci_ucchr.ci_cleg[*(eptr->ci_ucchr.ci_cval)]);
  	break;
  case ci_tctab:
  	*(eptr->ci_ucstab.ci_tval) =
  	stabarg (&arg,0,eptr->ci_ucstab.ci_tmsg,eptr->ci_ucstab.ci_ttab,
  	eptr->ci_ucstab.ci_ttab[*(eptr->ci_ucstab.ci_tval)]);
  	break;
  case ci_tcsearch:
  	*(eptr->ci_ucsearch.ci_tval) =
  	searcharg (&arg,0,eptr->ci_ucsearch.ci_tmsg,
  	eptr->ci_ucsearch.ci_ttab,
  	eptr->ci_ucsearch.ci_ttab[*(eptr->ci_ucsearch.ci_tval)]);
  	break;
  default:;
  }
}
