/* Remote debugging interface for AMD 290*0 Adapt Monitor Version 2.1d18. 
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Contributed by David Wood at New York University (wood@lab.ultra.nyu.edu).
   Adapted from work done at Cygnus Support in remote-eb.c.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This is like remote.c but is for an esoteric situation--
   having a 29k board attached to an Adapt inline monitor. 
   The  monitor is connected via serial line to a unix machine 
   running gdb. 

   3/91 -  developed on Sun3 OS 4.1, by David Wood
   	o - I can't get binary coff to load. 
	o - I can't get 19200 baud rate to work. 
   7/91 o - Freeze mode tracing can be done on a 29050.  */

#include "defs.h"
#include "gdb_string.h"
#include "inferior.h"
#include "wait.h"
#include "value.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "terminal.h"
#include "target.h"
#include "gdbcore.h"

/* External data declarations */
extern int stop_soon_quietly;           /* for wait_for_inferior */

/* Forward data declarations */
extern struct target_ops adapt_ops;		/* Forward declaration */

/* Forward function declarations */
static void adapt_fetch_registers ();
static void adapt_store_registers ();
static void adapt_close ();
static int  adapt_clear_breakpoints();

#define FREEZE_MODE 	(read_register(CPS_REGNUM) && 0x400) 
#define USE_SHADOW_PC	((processor_type == a29k_freeze_mode) && FREEZE_MODE)

/* Can't seem to get binary coff working */
#define ASCII_COFF		/* Adapt will be downloaded with ascii coff */

/* FIXME: Replace with `set remotedebug'.  */
#define LOG_FILE "adapt.log"
#if defined (LOG_FILE)
FILE *log_file=NULL;
#endif

static int timeout = 5;
static char *dev_name;

/* Descriptor for I/O to remote machine.  Initialize it to -1 so that
   adapt_open knows that we don't have a file open when the program
   starts.  */
int adapt_desc = -1;

/* stream which is fdopen'd from adapt_desc.  Only valid when
   adapt_desc != -1.  */
FILE *adapt_stream;

#define ON	1
#define OFF	0
static void
rawmode(desc, turnon)
int	desc;
int	turnon;
{
  TERMINAL sg;

  if (desc < 0)
    return;

  ioctl (desc, TIOCGETP, &sg);

  if (turnon) {
#ifdef HAVE_TERMIO
  	sg.c_lflag &= ~(ICANON);
#else
	sg.sg_flags |= RAW;
#endif
  } else {
#ifdef HAVE_TERMIO
  	sg.c_lflag |= ICANON;
#else
	sg.sg_flags &= ~(RAW);
#endif
  }
  ioctl (desc, TIOCSETP, &sg);
}

/* Suck up all the input from the adapt */
slurp_input()
{
  char buf[8];

#ifdef HAVE_TERMIO
  /* termio does the timeout for us.  */
  while (read (adapt_desc, buf, 8) > 0);
#else
  alarm (timeout);
  while (read (adapt_desc, buf, 8) > 0);
  alarm (0);
#endif
}

/* Read a character from the remote system, doing all the fancy
   timeout stuff.  */
static int
readchar ()
{
  char buf;

  buf = '\0';
#ifdef HAVE_TERMIO
  /* termio does the timeout for us.  */
  read (adapt_desc, &buf, 1);
#else
  alarm (timeout);
  if (read (adapt_desc, &buf, 1) < 0)
    {
      if (errno == EINTR)
	error ("Timeout reading from remote system.");
      else
	perror_with_name ("remote");
    }
  alarm (0);
#endif

  if (buf == '\0')
    error ("Timeout reading from remote system.");
#if defined (LOG_FILE)
  putc (buf & 0x7f, log_file);
#endif
  return buf & 0x7f;
}

/* Keep discarding input from the remote system, until STRING is found. 
   Let the user break out immediately.  */
static void
expect (string)
     char *string;
{
  char *p = string;

  fflush(adapt_stream);
  immediate_quit = 1;
  while (1)
    {
      if (readchar() == *p)
	{
	  p++;
	  if (*p == '\0')
	    {
	      immediate_quit = 0;
	      return;
	    }
	}
      else
	p = string;
    }
}

/* Keep discarding input until we see the adapt prompt.

   The convention for dealing with the prompt is that you
   o give your command
   o *then* wait for the prompt.

   Thus the last thing that a procedure does with the serial line
   will be an expect_prompt().  Exception:  adapt_resume does not
   wait for the prompt, because the terminal is being handed over
   to the inferior.  However, the next thing which happens after that
   is a adapt_wait which does wait for the prompt.
   Note that this includes abnormal exit, e.g. error().  This is
   necessary to prevent getting into states from which we can't
   recover.  */
static void
expect_prompt ()
{
#if defined (LOG_FILE)
  /* This is a convenient place to do this.  The idea is to do it often
     enough that we never lose much data if we terminate abnormally.  */
  fflush (log_file);
#endif
  fflush(adapt_stream);
  expect ("\n# ");
}

/* Get a hex digit from the remote system & return its value.
   If ignore_space is nonzero, ignore spaces (not newline, tab, etc).  */
static int
get_hex_digit (ignore_space)
     int ignore_space;
{
  int ch;
  while (1)
    {
      ch = readchar ();
      if (ch >= '0' && ch <= '9')
	return ch - '0';
      else if (ch >= 'A' && ch <= 'F')
	return ch - 'A' + 10;
      else if (ch >= 'a' && ch <= 'f')
	return ch - 'a' + 10;
      else if (ch == ' ' && ignore_space)
	;
      else
	{
	  expect_prompt ();
	  error ("Invalid hex digit from remote system.");
	}
    }
}

/* Get a byte from adapt_desc and put it in *BYT.  Accept any number
   leading spaces.  */
static void
get_hex_byte (byt)
     char *byt;
{
  int val;

  val = get_hex_digit (1) << 4;
  val |= get_hex_digit (0);
  *byt = val;
}

/* Read a 32-bit hex word from the adapt, preceded by a space  */
static long 
get_hex_word()
{
  long val;
  int j;
      
  val = 0;
  for (j = 0; j < 8; j++)
	val = (val << 4) + get_hex_digit (j == 0);
  return val;
}
/* Get N 32-bit hex words from remote, each preceded by a space 
   and put them in registers starting at REGNO.  */
static void
get_hex_regs (n, regno)
     int n;
     int regno;
{
	long val;
	while (n--) {
		val = get_hex_word();
		supply_register(regno++,(char *) &val);
	}
}
/* Called when SIGALRM signal sent due to alarm() timeout.  */
#ifndef HAVE_TERMIO

#ifndef __STDC__
# ifndef volatile
#  define volatile /**/
# endif
#endif
volatile int n_alarms;

void
adapt_timer ()
{
#if 0
  if (kiodebug)
    printf ("adapt_timer called\n");
#endif
  n_alarms++;
}
#endif

/* malloc'd name of the program on the remote system.  */
static char *prog_name = NULL;

/* Number of SIGTRAPs we need to simulate.  That is, the next
   NEED_ARTIFICIAL_TRAP calls to adapt_wait should just return
   SIGTRAP without actually waiting for anything.  */

static int need_artificial_trap = 0;

void
adapt_kill(arg,from_tty)
char	*arg;
int	from_tty;
{
	fprintf (adapt_stream, "K");
	fprintf (adapt_stream, "\r");
	expect_prompt ();
}
/*
 * Download a file specified in 'args', to the adapt. 
 * FIXME: Assumes the file to download is a binary coff file.
 */
static void
adapt_load(args,fromtty)
char	*args;
int	fromtty;
{
	FILE *fp;
	int	n;
	char	buffer[1024];
	
	if (!adapt_stream) {
		printf_filtered("Adapt not open. Use 'target' command to open adapt\n");
		return;
	}

  	/* OK, now read in the file.  Y=read, C=COFF, T=dTe port
     		0=start address.  */

#ifdef ASCII_COFF	/* Ascii coff */
	fprintf (adapt_stream, "YA T,0\r");
	fflush(adapt_stream); 	/* Just in case */
	/* FIXME: should check args for only 1 argument */
	sprintf(buffer,"cat %s | btoa > /tmp/#adapt-btoa",args);
	system(buffer);
	fp = fopen("/tmp/#adapt-btoa","r");
	rawmode(adapt_desc,OFF);	
	while (n=fread(buffer,1,1024,fp)) {
		do { n -= write(adapt_desc,buffer,n); } while (n>0);
		if (n<0) { perror("writing ascii coff"); break; }
	}
	fclose(fp);
	rawmode(adapt_desc,ON);	
	system("rm /tmp/#adapt-btoa");
#else	/* Binary coff - can't get it to work .*/
	fprintf (adapt_stream, "YC T,0\r");
	fflush(adapt_stream); 	/* Just in case */
	if (!(fp = fopen(args,"r"))) {
		printf_filtered("Can't open %s\n",args);
		return;
	}
	while (n=fread(buffer,1,512,fp)) {
		do { n -= write(adapt_desc,buffer,n); } while (n>0);
		if (n<0) { perror("writing ascii coff"); break; }
	}
	fclose(fp);
#endif
  	expect_prompt ();	/* Skip garbage that comes out */
	fprintf (adapt_stream, "\r");
  	expect_prompt ();
}

/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
void
adapt_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;

  if (args && *args)
    error ("Can't pass arguments to remote adapt process.");

  if (execfile == 0 || exec_bfd == 0)
    error ("No exec file specified");

  entry_pt = (int) bfd_get_start_address (exec_bfd);

  if (adapt_stream) {
	adapt_kill(NULL,NULL);	 
	adapt_clear_breakpoints();
	init_wait_for_inferior ();
	/* Clear the input because what the adapt sends back is different
	 * depending on whether it was running or not.
	 */
	slurp_input();	/* After this there should be a prompt */
	fprintf(adapt_stream,"\r"); 
	expect_prompt();
	printf_filtered("Do you want to download '%s' (y/n)? [y] : ",prog_name);
	{	
		char buffer[10];
		gets(buffer);
		if (*buffer != 'n') {
			adapt_load(prog_name,0);
		}
	}

#ifdef NOTDEF
	/* Set the PC and wait for a go/cont */
	  fprintf (adapt_stream, "G %x,N\r",entry_pt);
	  printf_filtered("Now use the 'continue' command to start.\n"); 
  	  expect_prompt ();
#else
  	insert_breakpoints ();  /* Needed to get correct instruction in cache */
  	proceed(entry_pt, TARGET_SIGNAL_DEFAULT, 0);
#endif

  } else {
	printf_filtered("Adapt not open yet.\n");
  }
}

/* Translate baud rates from integers to damn B_codes.  Unix should
   have outgrown this crap years ago, but even POSIX wouldn't buck it.  */

#ifndef B19200
#define B19200 EXTA
#endif
#ifndef B38400
#define B38400 EXTB
#endif

static struct {int rate, damn_b;} baudtab[] = {
	{0, B0},
	{50, B50},
	{75, B75},
	{110, B110},
	{134, B134},
	{150, B150},
	{200, B200},
	{300, B300},
	{600, B600},
	{1200, B1200},
	{1800, B1800},
	{2400, B2400},
	{4800, B4800},
	{9600, B9600},
	{19200, B19200},
	{38400, B38400},
	{-1, -1},
};

static int damn_b (rate)
     int rate;
{
  int i;

  for (i = 0; baudtab[i].rate != -1; i++)
    if (rate == baudtab[i].rate) return baudtab[i].damn_b;
  return B38400;	/* Random */
}


/* Open a connection to a remote debugger.
   NAME is the filename used for communication, then a space,
   then the baud rate.
 */

static int baudrate = 9600;
static void
adapt_open (name, from_tty)
     char *name;
     int from_tty;
{
  TERMINAL sg;
  unsigned int prl;
  char *p;

  /* Find the first whitespace character, it separates dev_name from
     prog_name.  */
  if (name == 0)
    goto erroid;

  for (p = name;
       *p != '\0' && !isspace (*p); p++)
    ;
  if (*p == '\0')
erroid:
    error ("\
Please include the name of the device for the serial port,\n\
the baud rate, and the name of the program to run on the remote system.");
  dev_name = (char*)xmalloc(p - name + 1);
  strncpy (dev_name, name, p - name);
  dev_name[p - name] = '\0';

  /* Skip over the whitespace after dev_name */
  for (; isspace (*p); p++)
    /*EMPTY*/;
  
  if (1 != sscanf (p, "%d ", &baudrate))
    goto erroid;

  /* Skip the number and then the spaces */
  for (; isdigit (*p); p++)
    /*EMPTY*/;
  for (; isspace (*p); p++)
    /*EMPTY*/;
  
  if (prog_name != NULL)
    free (prog_name);
  prog_name = savestring (p, strlen (p));

  adapt_close (0);

  adapt_desc = open (dev_name, O_RDWR);
  if (adapt_desc < 0)
    perror_with_name (dev_name);
  ioctl (adapt_desc, TIOCGETP, &sg);
#ifdef HAVE_TERMIO
  sg.c_cc[VMIN] = 0;		/* read with timeout.  */
  sg.c_cc[VTIME] = timeout * 10;
  sg.c_lflag &= ~(ICANON | ECHO);
  sg.c_cflag = (sg.c_cflag & ~CBAUD) | damn_b (baudrate);
#else
  sg.sg_ispeed = damn_b (baudrate);
  sg.sg_ospeed = damn_b (baudrate);
  sg.sg_flags |= RAW | ANYP;
  sg.sg_flags &= ~ECHO;
#endif

  ioctl (adapt_desc, TIOCSETP, &sg);
  adapt_stream = fdopen (adapt_desc, "r+");

  push_target (&adapt_ops);

#ifndef HAVE_TERMIO
#ifndef NO_SIGINTERRUPT
  /* Cause SIGALRM's to make reads fail with EINTR instead of resuming
     the read.  */
  if (siginterrupt (SIGALRM, 1) != 0)
    perror ("adapt_open: error in siginterrupt");
#endif

  /* Set up read timeout timer.  */
  if ((void (*)) signal (SIGALRM, adapt_timer) == (void (*)) -1)
    perror ("adapt_open: error in signal");
#endif

#if defined (LOG_FILE)
  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    perror_with_name (LOG_FILE);
#endif

  /* Put this port into NORMAL mode, send the 'normal' character */
  write(adapt_desc, "", 1);	/* Control A */
  write(adapt_desc, "\r", 1);	
  expect_prompt ();
  
  /* Hello?  Are you there?  */
  write (adapt_desc, "\r", 1);
 
  expect_prompt ();

  /* Clear any break points */
  adapt_clear_breakpoints();

  /* Print out some stuff, letting the user now what's going on */
  printf_filtered("Connected to an Adapt via %s.\n", dev_name);
    /* FIXME: can this restriction be removed? */
  printf_filtered("Remote debugging using virtual addresses works only\n");
  printf_filtered("\twhen virtual addresses map 1:1 to physical addresses.\n"); 
  if (processor_type != a29k_freeze_mode) {
	fprintf_filtered(stderr,
	"Freeze-mode debugging not available, and can only be done on an A29050.\n");
  }
}

/* Close out all files and local state before this target loses control. */

static void
adapt_close (quitting)
     int quitting;
{

  /* Clear any break points */
  adapt_clear_breakpoints();

  /* Put this port back into REMOTE mode */ 
  if (adapt_stream) {
     fflush(adapt_stream);
     sleep(1);		/* Let any output make it all the way back */
     write(adapt_desc, "R\r", 2);
  }

  /* Due to a bug in Unix, fclose closes not only the stdio stream,
     but also the file descriptor.  So we don't actually close
     adapt_desc.  */
  if (adapt_stream)
    fclose (adapt_stream);	/* This also closes adapt_desc */
  if (adapt_desc >= 0)
    /* close (adapt_desc); */

  /* Do not try to close adapt_desc again, later in the program.  */
  adapt_stream = NULL;
  adapt_desc = -1;

#if defined (LOG_FILE)
  if (log_file) {
    if (ferror (log_file))
      printf_filtered ("Error writing log file.\n");
    if (fclose (log_file) != 0)
      printf_filtered ("Error closing log file.\n");
    log_file = NULL;
  }
#endif
}

/* Attach to the target that is already loaded and possibly running */
static void
adapt_attach (args, from_tty)
     char *args;
     int from_tty;
{

  if (from_tty)
      printf_filtered ("Attaching to remote program %s.\n", prog_name);

  /* Send the adapt a kill. It is ok if it is not already running */
  fprintf(adapt_stream, "K\r"); fflush(adapt_stream);
  expect_prompt();		/* Slurp the echo */
}


/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
void
adapt_detach (args,from_tty)
     char *args;
     int from_tty;
{

  if (adapt_stream) { /* Send it on its way (tell it to continue)  */
	adapt_clear_breakpoints();
  	fprintf(adapt_stream,"G\r");
  }
 
  pop_target();		/* calls adapt_close to do the real work */
  if (from_tty)
    printf_filtered ("Ending remote %s debugging\n", target_shortname);
}
 
/* Tell the remote machine to resume.  */

void
adapt_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  if (step)	
    {
      write (adapt_desc, "t 1,s\r", 6);
      /* Wait for the echo.  */
      expect ("t 1,s\r\n");
      /* Then comes a line containing the instruction we stepped to.  */
      expect ("@");
      /* Then we get the prompt.  */
      expect_prompt ();

      /* Force the next adapt_wait to return a trap.  Not doing anything
         about I/O from the target means that the user has to type
         "continue" to see any.  FIXME, this should be fixed.  */
      need_artificial_trap = 1;
    }
  else
    {
      write (adapt_desc, "G\r", 2);
      /* Swallow the echo.  */
      expect_prompt(); 
    }
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.  */

int
adapt_wait (status)
     struct target_waitstatus *status;
{
  /* Strings to look for.  '?' means match any single character.  
     Note that with the algorithm we use, the initial character
     of the string cannot recur in the string, or we will not
     find some cases of the string in the input.  */
  
  static char bpt[] = "@";
  /* It would be tempting to look for "\n[__exit + 0x8]\n"
     but that requires loading symbols with "yc i" and even if
     we did do that we don't know that the file has symbols.  */
  static char exitmsg[] = "@????????I    JMPTI     GR121,LR0";
  char *bp = bpt;
  char *ep = exitmsg;

  /* Large enough for either sizeof (bpt) or sizeof (exitmsg) chars.  */
  char swallowed[50];
  /* Current position in swallowed.  */
  char *swallowed_p = swallowed;

  int ch;
  int ch_handled;
  int old_timeout = timeout;
  int old_immediate_quit = immediate_quit;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  if (need_artificial_trap != 0)
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
      need_artificial_trap--;
      return 0;
    }

  timeout = 0;		/* Don't time out -- user program is running. */
  immediate_quit = 1;	/* Helps ability to QUIT */
  while (1) {
      QUIT;		/* Let user quit and leave process running */
      ch_handled = 0;
      ch = readchar ();
      if (ch == *bp) {
	  bp++;
	  if (*bp == '\0')
	    break;
	  ch_handled = 1;

	  *swallowed_p++ = ch;
      } else
	bp = bpt;
      if (ch == *ep || *ep == '?') {
	  ep++;
	  if (*ep == '\0')
	    break;

	  if (!ch_handled)
	    *swallowed_p++ = ch;
	  ch_handled = 1;
      } else
	ep = exitmsg;
      if (!ch_handled) {
	  char *p;
	  /* Print out any characters which have been swallowed.  */
	  for (p = swallowed; p < swallowed_p; ++p)
	    putc (*p, stdout);
	  swallowed_p = swallowed;
	  putc (ch, stdout);
      }
  }
  expect_prompt ();
  if (*bp== '\0')
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
    }
  else
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = 0;
    }
  timeout = old_timeout;
  immediate_quit = old_immediate_quit;
  return 0;
}

/* Return the name of register number REGNO
   in the form input and output by adapt.

   Returns a pointer to a static buffer containing the answer.  */
static char *
get_reg_name (regno)
     int regno;
{
  static char buf[80];
  if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32 )
    sprintf (buf, "GR%03d", regno - GR96_REGNUM + 96);
#if defined(GR64_REGNUM)
  else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32 )
    sprintf (buf, "GR%03d", regno - GR64_REGNUM + 64);
#endif
  else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128)
    sprintf (buf, "LR%03d", regno - LR0_REGNUM);
  else if (regno == Q_REGNUM) 
    strcpy (buf, "SR131");
  else if (regno >= BP_REGNUM && regno <= CR_REGNUM)
    sprintf (buf, "SR%03d", regno - BP_REGNUM + 133);
  else if (regno == ALU_REGNUM)
    strcpy (buf, "SR132");
  else if (regno >= IPC_REGNUM && regno <= IPB_REGNUM)
    sprintf (buf, "SR%03d", regno - IPC_REGNUM + 128);
  else if (regno >= VAB_REGNUM && regno <= LRU_REGNUM) {
    /* When a 29050 is in freeze-mode, read shadow pcs instead */
    if ((regno >= NPC_REGNUM && regno <= PC2_REGNUM) && USE_SHADOW_PC)
    	sprintf (buf, "SR%03d", regno - NPC_REGNUM + 20);
    else
    	sprintf (buf, "SR%03d", regno - VAB_REGNUM);
  }
  else if (regno == GR1_REGNUM)
    strcpy (buf, "GR001");
  return buf;
}

/* Read the remote registers.  */

static void
adapt_fetch_registers ()
{
  int reg_index;
  int regnum_index;
  char tempbuf[10];
  int	sreg_buf[16];
  int i,j;

/* 
 * Global registers
 */
#if defined(GR64_REGNUM)
  write (adapt_desc, "dw gr64,gr95\r", 13);
  for (reg_index = 64, regnum_index = GR64_REGNUM;
       reg_index < 96;
       reg_index += 4, regnum_index += 4)
    {
      sprintf (tempbuf, "GR%03d ", reg_index);
      expect (tempbuf);
      get_hex_regs (4, regnum_index);
      expect ("\n");
    }
#endif
  write (adapt_desc, "dw gr96,gr127\r", 14);
  for (reg_index = 96, regnum_index = GR96_REGNUM;
       reg_index < 128;
       reg_index += 4, regnum_index += 4)
    {
      sprintf (tempbuf, "GR%03d ", reg_index);
      expect (tempbuf);
      get_hex_regs (4, regnum_index);
      expect ("\n");
    }

/* 
 * Local registers
 */
  for (i = 0; i < 128; i += 32)
    {
      /* The PC has a tendency to hang if we get these
	 all in one fell swoop ("dw lr0,lr127").  */
      sprintf (tempbuf, "dw lr%d\r", i);
      write (adapt_desc, tempbuf, strlen (tempbuf));
      for (reg_index = i, regnum_index = LR0_REGNUM + i;
	   reg_index < i + 32;
	   reg_index += 4, regnum_index += 4)
	{
	  sprintf (tempbuf, "LR%03d ", reg_index);
	  expect (tempbuf);
	  get_hex_regs (4, regnum_index);
	  expect ("\n");
	}
    }

/* 
 * Special registers
 */
  sprintf (tempbuf, "dw sr0\r");
  write (adapt_desc, tempbuf, strlen (tempbuf));
  for (i=0 ; i<4 ; i++) {			/* SR0 - SR14 */
        sprintf (tempbuf, "SR%3d",i*4);
	expect(tempbuf);
	for (j=0 ; j < (i==3 ? 3 : 4) ; j++)
		sreg_buf[i*4 + j] = get_hex_word();
  }		
  expect_prompt();
  /* 
   * Read the pcs individually if we are in freeze mode.
   * See get_reg_name(), it translates the register names for the pcs to
   * the names of the shadow pcs.
   */ 
  if (USE_SHADOW_PC)  {
	  sreg_buf[10] = read_register(NPC_REGNUM);	/* pc0 */
	  sreg_buf[11] = read_register(PC_REGNUM);	/* pc1 */
	  sreg_buf[12] = read_register(PC2_REGNUM);	/* pc2 */
  }
  for (i=0 ; i<14 ; i++)		/* Supply vab -> lru */
 	supply_register(VAB_REGNUM+i, (char *) &sreg_buf[i]);
  sprintf (tempbuf, "dw sr128\r");
  write (adapt_desc, tempbuf, strlen (tempbuf));
  for (i=0 ; i<2 ; i++) {			/* SR128 - SR135 */
        sprintf (tempbuf, "SR%3d",128 + i*4);
	expect(tempbuf);
	for (j=0 ; j<4 ; j++)
		sreg_buf[i*4 + j] = get_hex_word();
  }		
  expect_prompt();
  supply_register(IPC_REGNUM,(char *) &sreg_buf[0]);
  supply_register(IPA_REGNUM,(char *) &sreg_buf[1]);
  supply_register(IPB_REGNUM,(char *) &sreg_buf[2]);
  supply_register(Q_REGNUM,  (char *) &sreg_buf[3]);
		/* Skip ALU */
  supply_register(BP_REGNUM, (char *) &sreg_buf[5]);
  supply_register(FC_REGNUM, (char *) &sreg_buf[6]);
  supply_register(CR_REGNUM, (char *) &sreg_buf[7]);

  /* There doesn't seem to be any way to get these.  */
  {
    int val = -1;
    supply_register (FPE_REGNUM, (char *) &val);
    supply_register (INTE_REGNUM, (char *) &val);
    supply_register (FPS_REGNUM, (char *) &val);
    supply_register (EXO_REGNUM, (char *) &val);
  }

  write (adapt_desc, "dw gr1,gr1\r", 11);
  expect ("GR001 ");
  get_hex_regs (1, GR1_REGNUM);
  expect_prompt ();
}

/* Fetch register REGNO, or all registers if REGNO is -1.
 */
static void
adapt_fetch_register (regno)
     int regno;
{
  if (regno == -1)
    adapt_fetch_registers ();
  else
    {
      char *name = get_reg_name (regno);
      fprintf (adapt_stream, "dw %s,%s\r", name, name);
      expect (name);
      expect (" ");
      get_hex_regs (1, regno);
      expect_prompt ();
    }
}

/* Store the remote registers from the contents of the block REGS.  */

static void
adapt_store_registers ()
{
  int i, j;

  fprintf (adapt_stream, "s gr1,%x\r", read_register (GR1_REGNUM));
  expect_prompt ();

#if defined(GR64_REGNUM)
  for (j = 0; j < 32; j += 16)
    {
      fprintf (adapt_stream, "s gr%d,", j + 64);
      for (i = 0; i < 15; ++i) 
	fprintf (adapt_stream, "%x,", read_register (GR64_REGNUM + j + i));
      fprintf (adapt_stream, "%x\r", read_register (GR64_REGNUM + j + 15));
      expect_prompt ();
    }
#endif
  for (j = 0; j < 32; j += 16)
    {
      fprintf (adapt_stream, "s gr%d,", j + 96);
      for (i = 0; i < 15; ++i) 
	fprintf (adapt_stream, "%x,", read_register (GR96_REGNUM + j + i));
      fprintf (adapt_stream, "%x\r", read_register (GR96_REGNUM + j + 15));
      expect_prompt ();
    }

  for (j = 0; j < 128; j += 16)
    {
      fprintf (adapt_stream, "s lr%d,", j);
      for (i = 0; i < 15; ++i) 
	fprintf (adapt_stream, "%x,", read_register (LR0_REGNUM + j + i));
      fprintf (adapt_stream, "%x\r", read_register (LR0_REGNUM + j + 15));
      expect_prompt ();
    }

  fprintf (adapt_stream, "s sr128,%x,%x,%x\r", read_register (IPC_REGNUM),
	   read_register (IPA_REGNUM), read_register (IPB_REGNUM));
  expect_prompt ();
  fprintf (adapt_stream, "s sr133,%x,%x,%x\r", read_register (BP_REGNUM),
	   read_register (FC_REGNUM), read_register (CR_REGNUM));
  expect_prompt ();
  fprintf (adapt_stream, "s sr131,%x\r", read_register (Q_REGNUM));
  expect_prompt ();
  fprintf (adapt_stream, "s sr0,");
  for (i=0 ; i<7 ; ++i)
    fprintf (adapt_stream, "%x,", read_register (VAB_REGNUM + i));
  expect_prompt ();
  fprintf (adapt_stream, "s sr7,");
  for (i=7; i<14 ; ++i)
    fprintf (adapt_stream, "%x,", read_register (VAB_REGNUM + i));
  expect_prompt ();
}

/* Store register REGNO, or all if REGNO == -1.
   Return errno value.  */
void
adapt_store_register (regno)
     int regno;
{
  /* printf("adapt_store_register() called.\n"); fflush(stdout); /* */
  if (regno == -1)
    adapt_store_registers ();
  else
    {
      char *name = get_reg_name (regno);
      fprintf (adapt_stream, "s %s,%x\r", name, read_register (regno));
      /* Setting GR1 changes the numbers of all the locals, so
	 invalidate the register cache.  Do this *after* calling
	 read_register, because we want read_register to return the
	 value that write_register has just stuffed into the registers
	 array, not the value of the register fetched from the
	 inferior.  */
      if (regno == GR1_REGNUM)
	registers_changed ();
      expect_prompt ();
    }
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

void
adapt_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static CORE_ADDR 
translate_addr(addr)
CORE_ADDR addr;
{
#if defined(KERNEL_DEBUGGING)
	/* Check for a virtual address in the kernel */
	/* Assume physical address of ublock is in  paddr_u register */
	if (addr >= UVADDR) {
		/* PADDR_U register holds the physical address of the ublock */
		CORE_ADDR i = (CORE_ADDR)read_register(PADDR_U_REGNUM);	
		return(i + addr - (CORE_ADDR)UVADDR);
	} else {
		return(addr);
	}
#else
	return(addr);
#endif
}


/* FIXME!  Merge these two.  */
int
adapt_xfer_inferior_memory (memaddr, myaddr, len, write)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
{

  memaddr = translate_addr(memaddr);

  if (write)
    return adapt_write_inferior_memory (memaddr, myaddr, len);
  else
    return adapt_read_inferior_memory (memaddr, myaddr, len);
}

void
adapt_files_info ()
{
  printf_filtered("\tAttached to %s at %d baud and running program %s\n",
	  dev_name, baudrate, prog_name);
  printf_filtered("\ton an %s processor.\n", processor_name[processor_type]);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.  Returns errno value.  
 * sb/sh instructions don't work on unaligned addresses, when TU=1. 
 */
int
adapt_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i;
  unsigned int cps;

  /* Turn TU bit off so we can do 'sb' commands */
  cps = read_register(CPS_REGNUM);
  if (cps & 0x00000800)
  	write_register(CPS_REGNUM,cps&~(0x00000800));

  for (i = 0; i < len; i++)
    {
      if ((i % 16) == 0)
	fprintf (adapt_stream, "sb %x,", memaddr + i);
      if ((i % 16) == 15 || i == len - 1)
	{
	  fprintf (adapt_stream, "%x\r", ((unsigned char *)myaddr)[i]);
	  expect_prompt ();
	}
      else
	fprintf (adapt_stream, "%x,", ((unsigned char *)myaddr)[i]);
    }
  /* Restore the old value of cps if the TU bit was on */
  if (cps & 0x00000800)
  	write_register(CPS_REGNUM,cps);
  return len;
}

/* Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns errno value.  */
int
adapt_read_inferior_memory(memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i;

  /* Number of bytes read so far.  */
  int count;

  /* Starting address of this pass.  */
  unsigned long startaddr;

  /* Number of bytes to read in this pass.  */
  int len_this_pass;

  /* Note that this code works correctly if startaddr is just less
     than UINT_MAX (well, really CORE_ADDR_MAX if there was such a
     thing).  That is, something like
     adapt_read_bytes (CORE_ADDR_MAX - 4, foo, 4)
     works--it never adds len to memaddr and gets 0.  */
  /* However, something like
     adapt_read_bytes (CORE_ADDR_MAX - 3, foo, 4)
     doesn't need to work.  Detect it and give up if there's an attempt
     to do that.  */

  if (((memaddr - 1) + len) < memaddr)
    return EIO;
  
  startaddr = memaddr;
  count = 0;
  while (count < len)
    {
      len_this_pass = 16;
      if ((startaddr % 16) != 0)
	len_this_pass -= startaddr % 16;
      if (len_this_pass > (len - count))
	len_this_pass = (len - count);

      fprintf (adapt_stream, "db %x,%x\r", startaddr,
	       (startaddr - 1) + len_this_pass);

#ifdef NOTDEF	/* Why do this */
      expect ("\n");
      /* Look for 8 hex digits.  */
      i = 0;
      while (1)
	{
	  if (isxdigit (readchar ()))
	    ++i;
	  else
	    {
	      expect_prompt ();
	      error ("Hex digit expected from remote system.");
	    }
	  if (i >= 8)
	    break;
	}
#endif /* NOTDEF */

      expect ("  ");

      for (i = 0; i < len_this_pass; i++)
	get_hex_byte (&myaddr[count++]);

      expect_prompt ();

      startaddr += len_this_pass;
    }
  return count;
}

#define MAX_BREAKS	8
static int num_brkpts=0;
static int
adapt_insert_breakpoint(addr, save)
CORE_ADDR	addr;
char		*save;	/* Throw away, let adapt save instructions */
{
  if (num_brkpts < MAX_BREAKS) {
  	num_brkpts++;
  	fprintf (adapt_stream, "B %x", addr);
  	fprintf (adapt_stream, "\r");
  	expect_prompt ();
	return(0);	/* Success */
  } else {
	fprintf_filtered(stderr,
		"Too many break points, break point not installed\n");
	return(1);	/* Failure */
  }

}
static int
adapt_remove_breakpoint(addr, save)
CORE_ADDR	addr;
char		*save;	/* Throw away, let adapt save instructions */
{
  if (num_brkpts > 0) {
	  num_brkpts--;
	  fprintf (adapt_stream, "BR %x", addr);
	  fprintf (adapt_stream, "\r");
	  fflush (adapt_stream);
	  expect_prompt ();
  }
  return(0);
}

/* Clear the adapts notion of what the break points are */
static int
adapt_clear_breakpoints() 
{ 
  if (adapt_stream) {
  	fprintf (adapt_stream, "BR");	/* Clear all break points */
  	fprintf (adapt_stream, "\r");
  	fflush(adapt_stream);
  	expect_prompt ();
  }
  num_brkpts = 0;
}
static void
adapt_mourn() 
{ 
  adapt_clear_breakpoints();
  pop_target ();                /* Pop back to no-child state */
  generic_mourn_inferior ();
}

/* Display everthing we read in from the adapt until we match/see the
 * specified string
 */
static int
display_until(str)
char	*str;
{
	int	i=0,j,c;

	while (c=readchar()) {
		if (c==str[i]) {
			i++;
			if (i == strlen(str)) return;
		} else {
			if (i) {
			    for (j=0 ; j<i ; j++) /* Put everthing we matched */
				putchar(str[j]);
			    i=0;
			}
			putchar(c);
		}	
	}

}


/* Put a command string, in args, out to the adapt.  The adapt is assumed to
   be in raw mode, all writing/reading done through adapt_desc.
   Ouput from the adapt is placed on the users terminal until the
   prompt from the adapt is seen.
   FIXME: Can't handle commands that take input.  */

void
adapt_com (args, fromtty)
     char	*args;
     int	fromtty;
{
	if (!adapt_stream) {
		printf_filtered("Adapt not open.  Use the 'target' command to open.\n");
		return;
	}

	/* Clear all input so only command relative output is displayed */
	slurp_input();	

	switch(islower(args[0]) ? toupper(args[0]) : args[0]) {
	default:
		printf_filtered("Unknown/Unimplemented adapt command '%s'\n",args);
		break;
	case 'G':	/* Go, begin execution */
		write(adapt_desc,args,strlen(args));
		write(adapt_desc,"\r",1);
		expect_prompt();
		break;
	case 'B':	/* Break points, B or BR */
	case 'C':	/* Check current 29k status (running/halted) */
	case 'D':	/* Display data/registers */ 
	case 'I':	/* Input from i/o space */
	case 'J':	/* Jam an instruction */
	case 'K':	/* Kill, stop execution */
	case 'L':	/* Disassemble */
	case 'O':	/* Output to i/o space */
	case 'T':	/* Trace */ 
	case 'P':	/* Pulse an input line */ 
	case 'X':	/* Examine special purpose registers */ 
	case 'Z':	/* Display trace buffer */ 
		write(adapt_desc,args,strlen(args));
		write(adapt_desc,"\r",1);
		expect(args);		/* Don't display the command */
		display_until("# ");
		break;
	/* Begin commands that take input in the form 'c x,y[,z...]' */
	case 'S':	/* Set memory or register */
		if (strchr(args,',')) {	/* Assume it is properly formatted */
			write(adapt_desc,args,strlen(args));
			write(adapt_desc,"\r",1);
			expect_prompt();
		}
		break;
	}
}

/* Define the target subroutine names */

struct target_ops adapt_ops = {
	"adapt", "Remote AMD `Adapt' target",
	"Remote debug an AMD 290*0 using an `Adapt' monitor via RS232",
	adapt_open, adapt_close, 
	adapt_attach, adapt_detach, adapt_resume, adapt_wait,
	adapt_fetch_register, adapt_store_register,
	adapt_prepare_to_store,
	adapt_xfer_inferior_memory, 
	adapt_files_info,
	adapt_insert_breakpoint, adapt_remove_breakpoint, /* Breakpoints */
	0, 0, 0, 0, 0,		/* Terminal handling */
	adapt_kill, 		/* FIXME, kill */
	adapt_load, 
	0, 			/* lookup_symbol */
	adapt_create_inferior, 	/* create_inferior */ 
	adapt_mourn, 		/* mourn_inferior FIXME */
	0, /* can_run */
	0, /* notice_signals */
	0,			/* to_stop */
	process_stratum, 0, /* next */
	1, 1, 1, 1, 1,	/* all mem, mem, stack, regs, exec */
	0,0,		/* Section pointers */
	OPS_MAGIC,		/* Always the last thing */
};

void
_initialize_remote_adapt ()
{
  add_target (&adapt_ops);
  add_com ("adapt <command>", class_obscure, adapt_com,
 	"Send a command to the AMD Adapt remote monitor.");
}
