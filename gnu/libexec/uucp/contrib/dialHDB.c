/*
# File:		dialHDB.c
# Author:	Daniel Hagerty , hag@eddie.mit.edu
#		Copyright (C) 1993
# Date:		Fri Nov 26 19:22:31 1993
# Description:	Program for using HDB dialers for dialing modems, exiting
		with 0 on success, else failure.
# Version:	1.0
# Revision History:
######
### 11/26/93 Hag - File creation
######
### 1/5/94 Hag - Finally got around to finishing this damn thing.
######
*/
/* Basic theory behind this program-
   dialHDB forks into two processes, a monitor parent, and a child
   that does the exec of the dialer. Child pretty much just execs the
   dialer program, unless there's an exec problem, in which case the
   child sends the parent a SIGUSR1 to indicate failed execution.
*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define kUsage	"Usage:\n\t%s dialerPath device number speed\n\
%s dialer -h device speed\n"

#define kExitErrFlag	0x80	/* & in with exit code to determine error */
#define kErrorMask	0x0f	/* Mask to determine error code */

/* Error code defines as lifted from an HDB dialer */
#define	RCE_NULL	0	/* general purpose or unknown error code */
#define	RCE_INUSE	1	/* line in use */
#define	RCE_SIG		2	/* signal aborted dialer */
#define	RCE_ARGS	3	/* invalid arguments */
#define	RCE_PHNO	4	/* invalid phone number */
#define	RCE_SPEED	5	/* invalid baud rate -or- bad connect baud */
#define	RCE_OPEN	6	/* can't open line */
#define	RCE_IOCTL	7	/* ioctl error */
#define	RCE_TIMOUT	8	/* timeout */
#define	RCE_NOTONE	9	/* no dial tone */
#define	RCE_BUSY	13	/* phone is busy */
#define	RCE_NOCARR	14	/* no carrier */
#define	RCE_ANSWER	15	/* no answer */

/* Structure definition to map error codes to strings */
typedef struct
{
  int errNum;
  char *errString;
} errTable;

const errTable errors[]=
{
  { RCE_NULL,	"Unknown Error" },
  { RCE_INUSE,	"Line is being used" },
  { RCE_SIG,	"Recieved fatal signal" },
  { RCE_ARGS,	"Bad arguments" },
  { RCE_PHNO,	"Invalid phone number" },
  { RCE_SPEED,	"Invalid baud rate or bad connection" },
  { RCE_OPEN,	"Unable to open line" },
  { RCE_IOCTL,	"ioctl error" },
  { RCE_TIMOUT,	"Timed out" },
  { RCE_NOTONE,	"No dialtone" },
  { RCE_BUSY,	"Phone number is busy" },
  { RCE_NOCARR,	"No carrier" },
  { RCE_ANSWER,	"No answer" },
  { 0,NULL}
};

/* Function Prototypes */
int figureStat(int stat);
char *findInTable(int error);
void badExec(void);

char *dialerName;		/* basename of our dialer program */
char *dialerPath;		/* full path of dialer program */

main(int argc,char *argv[])
{
  int parent;			/* pid of parent process */
  int child;			/* pid of child process */
  int stat;			/* exit status of child process */
  char *temp;			/* used to get basename of dialer */
  
  if(argc!=5)
  {
    fprintf(stderr,kUsage,argv[0],argv[0]);
    exit(1);
  }

  dialerPath=argv[1];
  dialerName= (temp=strrchr(argv[1],'/'))!=NULL ? temp+1 : argv[1];
  
  parent=getpid();
  
  signal(SIGUSR1,badExec);	/* set up for possible failed exec */

  if((child=fork())<0)
  {
    perror("fork");
    exit(2);
  }
  if(child>0)			/* We're parent, wait for child to exit */
  {
    /* Set up to ignore signals so we can report them on stderror */
    signal(SIGHUP,SIG_IGN);
    signal(SIGINT,SIG_IGN);
    signal(SIGTERM,SIG_IGN);
    
    wait(&stat);		/* wait for child to exit */
    exit(figureStat(stat));	/* figure out our exit code and die */
  }
  else				/* child process */
  {
    close(0);			/* close of modem file desc, since HDB */
    close(1);			/* doesn't use them */
    dup2(2,1);			/* and remap stdout to stderr, just in case */
    if(execvp(argv[1],argv+1)<0) /* exec program with argv shifted by 1 */
    {				/* if exec fails, send SIGUSR1 to parent */
      kill(parent,SIGUSR1);
      exit(0);
    }
  }  
  exit(0);
}

/* Figure out whether or not dialer ran succesfully, and return
with 0 if it worked, otherwise error */
int figureStat(int stat)
{
  int exit;
  int errFlag;
  int error;
  
  if(WIFSIGNALED(stat))		/* determine if exit was from signal or what */
  {
    fprintf(stderr,"Error: Dialer %s recieved signal %d.\n",dialerName,
	    WTERMSIG(stat));
    return(1);
  }
  if(WIFSTOPPED(stat))
  {
    fprintf(stderr,"Error: Dialer %s recieved signal %d.\n",dialerName,
	    WSTOPSIG(stat));
    return(1);
  }
  exit=WEXITSTATUS(stat);

  errFlag=exit&kExitErrFlag;	/* Is the error flag set? */
  if(errFlag)
  {
    char *errString;

    error=exit&kErrorMask;
    errString=findInTable(error); /* find it's string, print it on stderr */
    fprintf(stderr,"Error: %s - %s.\n",dialerName,errString); /* and return */
    return(1);
  }
  return(0);
}

/* Support routine, look up exit code in error table, and return pointer
to proper string */
char *findInTable(int error)
{
  int i=0;

  for(i=0;errors[i].errString!=NULL;i++)
  {
    if(errors[i].errNum==error)
      return(errors[i].errString);
  }
  /* Still here, return the top entry, for unknown error */
  return(errors[0].errString);
}

/* Called by signal if we recieve SIGUSR 1 */
void badExec(void)
{
  fprintf(stderr,"Error: %s - Execution problem.\n",dialerPath);
  exit(1);
}
