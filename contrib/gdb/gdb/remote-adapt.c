/* OBSOLETE /* Remote debugging interface for AMD 290*0 Adapt Monitor Version 2.1d18.  */
/* OBSOLETE    Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, */
/* OBSOLETE    2001 Free Software Foundation, Inc. */
/* OBSOLETE    Contributed by David Wood at New York University (wood@lab.ultra.nyu.edu). */
/* OBSOLETE    Adapted from work done at Cygnus Support in remote-eb.c. */
/* OBSOLETE  */
/* OBSOLETE    This file is part of GDB. */
/* OBSOLETE  */
/* OBSOLETE    This program is free software; you can redistribute it and/or modify */
/* OBSOLETE    it under the terms of the GNU General Public License as published by */
/* OBSOLETE    the Free Software Foundation; either version 2 of the License, or */
/* OBSOLETE    (at your option) any later version. */
/* OBSOLETE  */
/* OBSOLETE    This program is distributed in the hope that it will be useful, */
/* OBSOLETE    but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* OBSOLETE    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* OBSOLETE    GNU General Public License for more details. */
/* OBSOLETE  */
/* OBSOLETE    You should have received a copy of the GNU General Public License */
/* OBSOLETE    along with this program; if not, write to the Free Software */
/* OBSOLETE    Foundation, Inc., 59 Temple Place - Suite 330, */
/* OBSOLETE    Boston, MA 02111-1307, USA.  */ */
/* OBSOLETE  */
/* OBSOLETE /* This is like remote.c but is for an esoteric situation-- */
/* OBSOLETE    having a 29k board attached to an Adapt inline monitor.  */
/* OBSOLETE    The  monitor is connected via serial line to a unix machine  */
/* OBSOLETE    running gdb.  */
/* OBSOLETE  */
/* OBSOLETE    3/91 -  developed on Sun3 OS 4.1, by David Wood */
/* OBSOLETE    o - I can't get binary coff to load.  */
/* OBSOLETE    o - I can't get 19200 baud rate to work.  */
/* OBSOLETE    7/91 o - Freeze mode tracing can be done on a 29050.  */ */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE #include "defs.h" */
/* OBSOLETE #include "gdb_string.h" */
/* OBSOLETE #include "inferior.h" */
/* OBSOLETE #include "value.h" */
/* OBSOLETE #include <ctype.h> */
/* OBSOLETE #include <fcntl.h> */
/* OBSOLETE #include <signal.h> */
/* OBSOLETE #include <errno.h> */
/* OBSOLETE #include "terminal.h" */
/* OBSOLETE #include "target.h" */
/* OBSOLETE #include "gdbcore.h" */
/* OBSOLETE #include "regcache.h" */
/* OBSOLETE  */
/* OBSOLETE /* This processor is getting rusty but I am trying to keep it */
/* OBSOLETE    up to date at least with data structure changes. */
/* OBSOLETE    Activate this block to compile just this file. */
/* OBSOLETE  */ */
/* OBSOLETE #define COMPILE_CHECK 0 */
/* OBSOLETE #if COMPILE_CHECK */
/* OBSOLETE #define Q_REGNUM 0 */
/* OBSOLETE #define VAB_REGNUM 0 */
/* OBSOLETE #define CPS_REGNUM 0 */
/* OBSOLETE #define IPA_REGNUM 0 */
/* OBSOLETE #define IPB_REGNUM 0 */
/* OBSOLETE #define GR1_REGNUM 0 */
/* OBSOLETE #define LR0_REGNUM 0 */
/* OBSOLETE #define IPC_REGNUM 0 */
/* OBSOLETE #define CR_REGNUM 0 */
/* OBSOLETE #define BP_REGNUM 0 */
/* OBSOLETE #define FC_REGNUM 0 */
/* OBSOLETE #define INTE_REGNUM 0 */
/* OBSOLETE #define EXO_REGNUM 0 */
/* OBSOLETE #define GR96_REGNUM 0 */
/* OBSOLETE #define NPC_REGNUM */
/* OBSOLETE #define FPE_REGNUM 0 */
/* OBSOLETE #define PC2_REGNUM 0 */
/* OBSOLETE #define FPS_REGNUM 0 */
/* OBSOLETE #define ALU_REGNUM 0 */
/* OBSOLETE #define LRU_REGNUM 0 */
/* OBSOLETE #define TERMINAL int */
/* OBSOLETE #define RAW 1 */
/* OBSOLETE #define ANYP 1 */
/* OBSOLETE extern int a29k_freeze_mode; */
/* OBSOLETE extern int processor_type; */
/* OBSOLETE extern char *processor_name; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /* External data declarations */ */
/* OBSOLETE extern int stop_soon_quietly;	/* for wait_for_inferior */ */
/* OBSOLETE  */
/* OBSOLETE /* Forward data declarations */ */
/* OBSOLETE extern struct target_ops adapt_ops;	/* Forward declaration */ */
/* OBSOLETE  */
/* OBSOLETE /* Forward function declarations */ */
/* OBSOLETE static void adapt_fetch_registers (); */
/* OBSOLETE static void adapt_store_registers (); */
/* OBSOLETE static void adapt_close (); */
/* OBSOLETE static int adapt_clear_breakpoints (); */
/* OBSOLETE  */
/* OBSOLETE #define FREEZE_MODE 	(read_register(CPS_REGNUM) && 0x400) */
/* OBSOLETE #define USE_SHADOW_PC	((processor_type == a29k_freeze_mode) && FREEZE_MODE) */
/* OBSOLETE  */
/* OBSOLETE /* Can't seem to get binary coff working */ */
/* OBSOLETE #define ASCII_COFF		/* Adapt will be downloaded with ascii coff */ */
/* OBSOLETE  */
/* OBSOLETE /* FIXME: Replace with `set remotedebug'.  */ */
/* OBSOLETE #define LOG_FILE "adapt.log" */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE FILE *log_file = NULL; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE static int timeout = 5; */
/* OBSOLETE static char *dev_name; */
/* OBSOLETE  */
/* OBSOLETE /* Descriptor for I/O to remote machine.  Initialize it to -1 so that */
/* OBSOLETE    adapt_open knows that we don't have a file open when the program */
/* OBSOLETE    starts.  */ */
/* OBSOLETE int adapt_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE /* stream which is fdopen'd from adapt_desc.  Only valid when */
/* OBSOLETE    adapt_desc != -1.  */ */
/* OBSOLETE FILE *adapt_stream; */
/* OBSOLETE  */
/* OBSOLETE #define ON	1 */
/* OBSOLETE #define OFF	0 */
/* OBSOLETE static void */
/* OBSOLETE rawmode (int desc, int turnon) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   TERMINAL sg; */
/* OBSOLETE  */
/* OBSOLETE   if (desc < 0) */
/* OBSOLETE     return; */
/* OBSOLETE  */
/* OBSOLETE   ioctl (desc, TIOCGETP, &sg); */
/* OBSOLETE  */
/* OBSOLETE   if (turnon) */
/* OBSOLETE     { */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE       sg.c_lflag &= ~(ICANON); */
/* OBSOLETE #else */
/* OBSOLETE       sg.sg_flags |= RAW; */
/* OBSOLETE #endif */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE       sg.c_lflag |= ICANON; */
/* OBSOLETE #else */
/* OBSOLETE       sg.sg_flags &= ~(RAW); */
/* OBSOLETE #endif */
/* OBSOLETE     } */
/* OBSOLETE   ioctl (desc, TIOCSETP, &sg); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Suck up all the input from the adapt */ */
/* OBSOLETE slurp_input (void) */
/* OBSOLETE { */
/* OBSOLETE   char buf[8]; */
/* OBSOLETE  */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   /* termio does the timeout for us.  */ */
/* OBSOLETE   while (read (adapt_desc, buf, 8) > 0); */
/* OBSOLETE #else */
/* OBSOLETE   alarm (timeout); */
/* OBSOLETE   while (read (adapt_desc, buf, 8) > 0); */
/* OBSOLETE   alarm (0); */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read a character from the remote system, doing all the fancy */
/* OBSOLETE    timeout stuff.  */ */
/* OBSOLETE static int */
/* OBSOLETE readchar (void) */
/* OBSOLETE { */
/* OBSOLETE   char buf; */
/* OBSOLETE  */
/* OBSOLETE   buf = '\0'; */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   /* termio does the timeout for us.  */ */
/* OBSOLETE   read (adapt_desc, &buf, 1); */
/* OBSOLETE #else */
/* OBSOLETE   alarm (timeout); */
/* OBSOLETE   if (read (adapt_desc, &buf, 1) < 0) */
/* OBSOLETE     { */
/* OBSOLETE       if (errno == EINTR) */
/* OBSOLETE 	error ("Timeout reading from remote system."); */
/* OBSOLETE       else */
/* OBSOLETE 	perror_with_name ("remote"); */
/* OBSOLETE     } */
/* OBSOLETE   alarm (0); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   if (buf == '\0') */
/* OBSOLETE     error ("Timeout reading from remote system."); */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   putc (buf & 0x7f, log_file); */
/* OBSOLETE #endif */
/* OBSOLETE   return buf & 0x7f; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Keep discarding input from the remote system, until STRING is found.  */
/* OBSOLETE    Let the user break out immediately.  */ */
/* OBSOLETE static void */
/* OBSOLETE expect (char *string) */
/* OBSOLETE { */
/* OBSOLETE   char *p = string; */
/* OBSOLETE  */
/* OBSOLETE   fflush (adapt_stream); */
/* OBSOLETE   immediate_quit++; */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
/* OBSOLETE       if (readchar () == *p) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  p++; */
/* OBSOLETE 	  if (*p == '\0') */
/* OBSOLETE 	    { */
/* OBSOLETE 	      immediate_quit--; */
/* OBSOLETE 	      return; */
/* OBSOLETE 	    } */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	p = string; */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Keep discarding input until we see the adapt prompt. */
/* OBSOLETE  */
/* OBSOLETE    The convention for dealing with the prompt is that you */
/* OBSOLETE    o give your command */
/* OBSOLETE    o *then* wait for the prompt. */
/* OBSOLETE  */
/* OBSOLETE    Thus the last thing that a procedure does with the serial line */
/* OBSOLETE    will be an expect_prompt().  Exception:  adapt_resume does not */
/* OBSOLETE    wait for the prompt, because the terminal is being handed over */
/* OBSOLETE    to the inferior.  However, the next thing which happens after that */
/* OBSOLETE    is a adapt_wait which does wait for the prompt. */
/* OBSOLETE    Note that this includes abnormal exit, e.g. error().  This is */
/* OBSOLETE    necessary to prevent getting into states from which we can't */
/* OBSOLETE    recover.  */ */
/* OBSOLETE static void */
/* OBSOLETE expect_prompt (void) */
/* OBSOLETE { */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   /* This is a convenient place to do this.  The idea is to do it often */
/* OBSOLETE      enough that we never lose much data if we terminate abnormally.  */ */
/* OBSOLETE   fflush (log_file); */
/* OBSOLETE #endif */
/* OBSOLETE   fflush (adapt_stream); */
/* OBSOLETE   expect ("\n# "); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Get a hex digit from the remote system & return its value. */
/* OBSOLETE    If ignore_space is nonzero, ignore spaces (not newline, tab, etc).  */ */
/* OBSOLETE static int */
/* OBSOLETE get_hex_digit (int ignore_space) */
/* OBSOLETE { */
/* OBSOLETE   int ch; */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
/* OBSOLETE       ch = readchar (); */
/* OBSOLETE       if (ch >= '0' && ch <= '9') */
/* OBSOLETE 	return ch - '0'; */
/* OBSOLETE       else if (ch >= 'A' && ch <= 'F') */
/* OBSOLETE 	return ch - 'A' + 10; */
/* OBSOLETE       else if (ch >= 'a' && ch <= 'f') */
/* OBSOLETE 	return ch - 'a' + 10; */
/* OBSOLETE       else if (ch == ' ' && ignore_space) */
/* OBSOLETE 	; */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  expect_prompt (); */
/* OBSOLETE 	  error ("Invalid hex digit from remote system."); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Get a byte from adapt_desc and put it in *BYT.  Accept any number */
/* OBSOLETE    leading spaces.  */ */
/* OBSOLETE static void */
/* OBSOLETE get_hex_byte (char *byt) */
/* OBSOLETE { */
/* OBSOLETE   int val; */
/* OBSOLETE  */
/* OBSOLETE   val = get_hex_digit (1) << 4; */
/* OBSOLETE   val |= get_hex_digit (0); */
/* OBSOLETE   *byt = val; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read a 32-bit hex word from the adapt, preceded by a space  */ */
/* OBSOLETE static long */
/* OBSOLETE get_hex_word (void) */
/* OBSOLETE { */
/* OBSOLETE   long val; */
/* OBSOLETE   int j; */
/* OBSOLETE  */
/* OBSOLETE   val = 0; */
/* OBSOLETE   for (j = 0; j < 8; j++) */
/* OBSOLETE     val = (val << 4) + get_hex_digit (j == 0); */
/* OBSOLETE   return val; */
/* OBSOLETE } */
/* OBSOLETE /* Get N 32-bit hex words from remote, each preceded by a space  */
/* OBSOLETE    and put them in registers starting at REGNO.  */ */
/* OBSOLETE static void */
/* OBSOLETE get_hex_regs (int n, int regno) */
/* OBSOLETE { */
/* OBSOLETE   long val; */
/* OBSOLETE   while (n--) */
/* OBSOLETE     { */
/* OBSOLETE       val = get_hex_word (); */
/* OBSOLETE       supply_register (regno++, (char *) &val); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE /* Called when SIGALRM signal sent due to alarm() timeout.  */ */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE  */
/* OBSOLETE volatile int n_alarms; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_timer (void) */
/* OBSOLETE { */
/* OBSOLETE #if 0 */
/* OBSOLETE   if (kiodebug) */
/* OBSOLETE     printf ("adapt_timer called\n"); */
/* OBSOLETE #endif */
/* OBSOLETE   n_alarms++; */
/* OBSOLETE } */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /* malloc'd name of the program on the remote system.  */ */
/* OBSOLETE static char *prog_name = NULL; */
/* OBSOLETE  */
/* OBSOLETE /* Number of SIGTRAPs we need to simulate.  That is, the next */
/* OBSOLETE    NEED_ARTIFICIAL_TRAP calls to adapt_wait should just return */
/* OBSOLETE    SIGTRAP without actually waiting for anything.  */ */
/* OBSOLETE  */
/* OBSOLETE static int need_artificial_trap = 0; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_kill (char *arg, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   fprintf (adapt_stream, "K"); */
/* OBSOLETE   fprintf (adapt_stream, "\r"); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE /* */
/* OBSOLETE  * Download a file specified in 'args', to the adapt.  */
/* OBSOLETE  * FIXME: Assumes the file to download is a binary coff file. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE adapt_load (char *args, int fromtty) */
/* OBSOLETE { */
/* OBSOLETE   FILE *fp; */
/* OBSOLETE   int n; */
/* OBSOLETE   char buffer[1024]; */
/* OBSOLETE  */
/* OBSOLETE   if (!adapt_stream) */
/* OBSOLETE     { */
/* OBSOLETE       printf_filtered ("Adapt not open. Use 'target' command to open adapt\n"); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* OK, now read in the file.  Y=read, C=COFF, T=dTe port */
/* OBSOLETE      0=start address.  */ */
/* OBSOLETE  */
/* OBSOLETE #ifdef ASCII_COFF		/* Ascii coff */ */
/* OBSOLETE   fprintf (adapt_stream, "YA T,0\r"); */
/* OBSOLETE   fflush (adapt_stream);	/* Just in case */ */
/* OBSOLETE   /* FIXME: should check args for only 1 argument */ */
/* OBSOLETE   sprintf (buffer, "cat %s | btoa > /tmp/#adapt-btoa", args); */
/* OBSOLETE   system (buffer); */
/* OBSOLETE   fp = fopen ("/tmp/#adapt-btoa", "r"); */
/* OBSOLETE   rawmode (adapt_desc, OFF); */
/* OBSOLETE   while (n = fread (buffer, 1, 1024, fp)) */
/* OBSOLETE     { */
/* OBSOLETE       do */
/* OBSOLETE 	{ */
/* OBSOLETE 	  n -= write (adapt_desc, buffer, n); */
/* OBSOLETE 	} */
/* OBSOLETE       while (n > 0); */
/* OBSOLETE       if (n < 0) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  perror ("writing ascii coff"); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   fclose (fp); */
/* OBSOLETE   rawmode (adapt_desc, ON); */
/* OBSOLETE   system ("rm /tmp/#adapt-btoa"); */
/* OBSOLETE #else /* Binary coff - can't get it to work . */ */
/* OBSOLETE   fprintf (adapt_stream, "YC T,0\r"); */
/* OBSOLETE   fflush (adapt_stream);	/* Just in case */ */
/* OBSOLETE   if (!(fp = fopen (args, "r"))) */
/* OBSOLETE     { */
/* OBSOLETE       printf_filtered ("Can't open %s\n", args); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE   while (n = fread (buffer, 1, 512, fp)) */
/* OBSOLETE     { */
/* OBSOLETE       do */
/* OBSOLETE 	{ */
/* OBSOLETE 	  n -= write (adapt_desc, buffer, n); */
/* OBSOLETE 	} */
/* OBSOLETE       while (n > 0); */
/* OBSOLETE       if (n < 0) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  perror ("writing ascii coff"); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   fclose (fp); */
/* OBSOLETE #endif */
/* OBSOLETE   expect_prompt ();		/* Skip garbage that comes out */ */
/* OBSOLETE   fprintf (adapt_stream, "\r"); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* This is called not only when we first attach, but also when the */
/* OBSOLETE    user types "run" after having attached.  */ */
/* OBSOLETE void */
/* OBSOLETE adapt_create_inferior (char *execfile, char *args, char **env) */
/* OBSOLETE { */
/* OBSOLETE   int entry_pt; */
/* OBSOLETE  */
/* OBSOLETE   if (args && *args) */
/* OBSOLETE     error ("Can't pass arguments to remote adapt process."); */
/* OBSOLETE  */
/* OBSOLETE   if (execfile == 0 || exec_bfd == 0) */
/* OBSOLETE     error ("No executable file specified"); */
/* OBSOLETE  */
/* OBSOLETE   entry_pt = (int) bfd_get_start_address (exec_bfd); */
/* OBSOLETE  */
/* OBSOLETE   if (adapt_stream) */
/* OBSOLETE     { */
/* OBSOLETE       adapt_kill (NULL, NULL); */
/* OBSOLETE       adapt_clear_breakpoints (); */
/* OBSOLETE       init_wait_for_inferior (); */
/* OBSOLETE       /* Clear the input because what the adapt sends back is different */
/* OBSOLETE        * depending on whether it was running or not. */
/* OBSOLETE        */ */
/* OBSOLETE       slurp_input ();		/* After this there should be a prompt */ */
/* OBSOLETE       fprintf (adapt_stream, "\r"); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE       printf_filtered ("Do you want to download '%s' (y/n)? [y] : ", prog_name); */
/* OBSOLETE       { */
/* OBSOLETE 	char buffer[10]; */
/* OBSOLETE 	gets (buffer); */
/* OBSOLETE 	if (*buffer != 'n') */
/* OBSOLETE 	  { */
/* OBSOLETE 	    adapt_load (prog_name, 0); */
/* OBSOLETE 	  } */
/* OBSOLETE       } */
/* OBSOLETE  */
/* OBSOLETE #ifdef NOTDEF */
/* OBSOLETE       /* Set the PC and wait for a go/cont */ */
/* OBSOLETE       fprintf (adapt_stream, "G %x,N\r", entry_pt); */
/* OBSOLETE       printf_filtered ("Now use the 'continue' command to start.\n"); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE #else */
/* OBSOLETE       insert_breakpoints ();	/* Needed to get correct instruction in cache */ */
/* OBSOLETE       proceed (entry_pt, TARGET_SIGNAL_DEFAULT, 0); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       printf_filtered ("Adapt not open yet.\n"); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Translate baud rates from integers to damn B_codes.  Unix should */
/* OBSOLETE    have outgrown this crap years ago, but even POSIX wouldn't buck it.  */ */
/* OBSOLETE  */
/* OBSOLETE #ifndef B19200 */
/* OBSOLETE #define B19200 EXTA */
/* OBSOLETE #endif */
/* OBSOLETE #ifndef B38400 */
/* OBSOLETE #define B38400 EXTB */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE static struct */
/* OBSOLETE { */
/* OBSOLETE   int rate, damn_b; */
/* OBSOLETE } */
/* OBSOLETE baudtab[] = */
/* OBSOLETE { */
/* OBSOLETE   { */
/* OBSOLETE     0, B0 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     50, B50 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     75, B75 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     110, B110 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     134, B134 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     150, B150 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     200, B200 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     300, B300 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     600, B600 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     1200, B1200 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     1800, B1800 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     2400, B2400 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     4800, B4800 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     9600, B9600 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     19200, B19200 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     38400, B38400 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE   { */
/* OBSOLETE     -1, -1 */
/* OBSOLETE   } */
/* OBSOLETE   , */
/* OBSOLETE }; */
/* OBSOLETE  */
/* OBSOLETE static int */
/* OBSOLETE damn_b (int rate) */
/* OBSOLETE { */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   for (i = 0; baudtab[i].rate != -1; i++) */
/* OBSOLETE     if (rate == baudtab[i].rate) */
/* OBSOLETE       return baudtab[i].damn_b; */
/* OBSOLETE   return B38400;		/* Random */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /* Open a connection to a remote debugger. */
/* OBSOLETE    NAME is the filename used for communication, then a space, */
/* OBSOLETE    then the baud rate. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE static int baudrate = 9600; */
/* OBSOLETE static void */
/* OBSOLETE adapt_open (char *name, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   TERMINAL sg; */
/* OBSOLETE   unsigned int prl; */
/* OBSOLETE   char *p; */
/* OBSOLETE  */
/* OBSOLETE   /* Find the first whitespace character, it separates dev_name from */
/* OBSOLETE      prog_name.  */ */
/* OBSOLETE   if (name == 0) */
/* OBSOLETE     goto erroid; */
/* OBSOLETE  */
/* OBSOLETE   for (p = name; */
/* OBSOLETE        *p != '\0' && !isspace (*p); p++) */
/* OBSOLETE     ; */
/* OBSOLETE   if (*p == '\0') */
/* OBSOLETE   erroid: */
/* OBSOLETE     error ("\ */
/* OBSOLETE Please include the name of the device for the serial port,\n\ */
/* OBSOLETE the baud rate, and the name of the program to run on the remote system."); */
/* OBSOLETE   dev_name = (char *) xmalloc (p - name + 1); */
/* OBSOLETE   strncpy (dev_name, name, p - name); */
/* OBSOLETE   dev_name[p - name] = '\0'; */
/* OBSOLETE  */
/* OBSOLETE   /* Skip over the whitespace after dev_name */ */
/* OBSOLETE   for (; isspace (*p); p++) */
/* OBSOLETE     /*EMPTY */ ; */
/* OBSOLETE  */
/* OBSOLETE   if (1 != sscanf (p, "%d ", &baudrate)) */
/* OBSOLETE     goto erroid; */
/* OBSOLETE  */
/* OBSOLETE   /* Skip the number and then the spaces */ */
/* OBSOLETE   for (; isdigit (*p); p++) */
/* OBSOLETE     /*EMPTY */ ; */
/* OBSOLETE   for (; isspace (*p); p++) */
/* OBSOLETE     /*EMPTY */ ; */
/* OBSOLETE  */
/* OBSOLETE   if (prog_name != NULL) */
/* OBSOLETE     xfree (prog_name); */
/* OBSOLETE   prog_name = savestring (p, strlen (p)); */
/* OBSOLETE  */
/* OBSOLETE   adapt_close (0); */
/* OBSOLETE  */
/* OBSOLETE   adapt_desc = open (dev_name, O_RDWR); */
/* OBSOLETE   if (adapt_desc < 0) */
/* OBSOLETE     perror_with_name (dev_name); */
/* OBSOLETE   ioctl (adapt_desc, TIOCGETP, &sg); */
/* OBSOLETE #if ! defined(COMPILE_CHECK) */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   sg.c_cc[VMIN] = 0;		/* read with timeout.  */ */
/* OBSOLETE   sg.c_cc[VTIME] = timeout * 10; */
/* OBSOLETE   sg.c_lflag &= ~(ICANON | ECHO); */
/* OBSOLETE   sg.c_cflag = (sg.c_cflag & ~CBAUD) | damn_b (baudrate); */
/* OBSOLETE #else */
/* OBSOLETE   sg.sg_ispeed = damn_b (baudrate); */
/* OBSOLETE   sg.sg_ospeed = damn_b (baudrate); */
/* OBSOLETE   sg.sg_flags |= RAW | ANYP; */
/* OBSOLETE   sg.sg_flags &= ~ECHO; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   ioctl (adapt_desc, TIOCSETP, &sg); */
/* OBSOLETE   adapt_stream = fdopen (adapt_desc, "r+"); */
/* OBSOLETE #endif /* compile_check */ */
/* OBSOLETE   push_target (&adapt_ops); */
/* OBSOLETE  */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE #ifndef NO_SIGINTERRUPT */
/* OBSOLETE   /* Cause SIGALRM's to make reads fail with EINTR instead of resuming */
/* OBSOLETE      the read.  */ */
/* OBSOLETE   if (siginterrupt (SIGALRM, 1) != 0) */
/* OBSOLETE     perror ("adapt_open: error in siginterrupt"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Set up read timeout timer.  */ */
/* OBSOLETE   if ((void (*)) signal (SIGALRM, adapt_timer) == (void (*)) -1) */
/* OBSOLETE     perror ("adapt_open: error in signal"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   log_file = fopen (LOG_FILE, "w"); */
/* OBSOLETE   if (log_file == NULL) */
/* OBSOLETE     perror_with_name (LOG_FILE); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Put this port into NORMAL mode, send the 'normal' character */ */
/* OBSOLETE   write (adapt_desc, "", 1);	/* Control A */ */
/* OBSOLETE   write (adapt_desc, "\r", 1); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE   /* Hello?  Are you there?  */ */
/* OBSOLETE   write (adapt_desc, "\r", 1); */
/* OBSOLETE  */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE   /* Clear any break points */ */
/* OBSOLETE   adapt_clear_breakpoints (); */
/* OBSOLETE  */
/* OBSOLETE   /* Print out some stuff, letting the user now what's going on */ */
/* OBSOLETE   printf_filtered ("Connected to an Adapt via %s.\n", dev_name); */
/* OBSOLETE   /* FIXME: can this restriction be removed? */ */
/* OBSOLETE   printf_filtered ("Remote debugging using virtual addresses works only\n"); */
/* OBSOLETE   printf_filtered ("\twhen virtual addresses map 1:1 to physical addresses.\n"); */
/* OBSOLETE   if (processor_type != a29k_freeze_mode) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf_filtered (gdb_stderr, */
/* OBSOLETE 			"Freeze-mode debugging not available, and can only be done on an A29050.\n"); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Close out all files and local state before this target loses control. */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE adapt_close (int quitting) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   /* Clear any break points */ */
/* OBSOLETE   adapt_clear_breakpoints (); */
/* OBSOLETE  */
/* OBSOLETE   /* Put this port back into REMOTE mode */ */
/* OBSOLETE   if (adapt_stream) */
/* OBSOLETE     { */
/* OBSOLETE       fflush (adapt_stream); */
/* OBSOLETE       sleep (1);		/* Let any output make it all the way back */ */
/* OBSOLETE       write (adapt_desc, "R\r", 2); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Due to a bug in Unix, fclose closes not only the stdio stream, */
/* OBSOLETE      but also the file descriptor.  So we don't actually close */
/* OBSOLETE      adapt_desc.  */ */
/* OBSOLETE   if (adapt_stream) */
/* OBSOLETE     fclose (adapt_stream);	/* This also closes adapt_desc */ */
/* OBSOLETE   if (adapt_desc >= 0) */
/* OBSOLETE     /* close (adapt_desc); */ */
/* OBSOLETE  */
/* OBSOLETE     /* Do not try to close adapt_desc again, later in the program.  */ */
/* OBSOLETE     adapt_stream = NULL; */
/* OBSOLETE   adapt_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   if (log_file) */
/* OBSOLETE     { */
/* OBSOLETE       if (ferror (log_file)) */
/* OBSOLETE 	printf_filtered ("Error writing log file.\n"); */
/* OBSOLETE       if (fclose (log_file) != 0) */
/* OBSOLETE 	printf_filtered ("Error closing log file.\n"); */
/* OBSOLETE       log_file = NULL; */
/* OBSOLETE     } */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Attach to the target that is already loaded and possibly running */ */
/* OBSOLETE static void */
/* OBSOLETE adapt_attach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf_filtered ("Attaching to remote program %s.\n", prog_name); */
/* OBSOLETE  */
/* OBSOLETE   /* Send the adapt a kill. It is ok if it is not already running */ */
/* OBSOLETE   fprintf (adapt_stream, "K\r"); */
/* OBSOLETE   fflush (adapt_stream); */
/* OBSOLETE   expect_prompt ();		/* Slurp the echo */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /* Terminate the open connection to the remote debugger. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  */ */
/* OBSOLETE void */
/* OBSOLETE adapt_detach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   if (adapt_stream) */
/* OBSOLETE     {				/* Send it on its way (tell it to continue)  */ */
/* OBSOLETE       adapt_clear_breakpoints (); */
/* OBSOLETE       fprintf (adapt_stream, "G\r"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   pop_target ();		/* calls adapt_close to do the real work */ */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf_filtered ("Ending remote %s debugging\n", target_shortname); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Tell the remote machine to resume.  */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_resume (ptid_t ptid, int step, enum target_signal sig) */
/* OBSOLETE { */
/* OBSOLETE   if (step) */
/* OBSOLETE     { */
/* OBSOLETE       write (adapt_desc, "t 1,s\r", 6); */
/* OBSOLETE       /* Wait for the echo.  */ */
/* OBSOLETE       expect ("t 1,s\r\n"); */
/* OBSOLETE       /* Then comes a line containing the instruction we stepped to.  */ */
/* OBSOLETE       expect ("@"); */
/* OBSOLETE       /* Then we get the prompt.  */ */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE       /* Force the next adapt_wait to return a trap.  Not doing anything */
/* OBSOLETE          about I/O from the target means that the user has to type */
/* OBSOLETE          "continue" to see any.  FIXME, this should be fixed.  */ */
/* OBSOLETE       need_artificial_trap = 1; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       write (adapt_desc, "G\r", 2); */
/* OBSOLETE       /* Swallow the echo.  */ */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Wait until the remote machine stops, then return, */
/* OBSOLETE    storing status in STATUS just as `wait' would.  */ */
/* OBSOLETE  */
/* OBSOLETE ptid_t */
/* OBSOLETE adapt_wait (ptid_t ptid, struct target_waitstatus *status) */
/* OBSOLETE { */
/* OBSOLETE   /* Strings to look for.  '?' means match any single character.   */
/* OBSOLETE      Note that with the algorithm we use, the initial character */
/* OBSOLETE      of the string cannot recur in the string, or we will not */
/* OBSOLETE      find some cases of the string in the input.  */ */
/* OBSOLETE  */
/* OBSOLETE   static char bpt[] = "@"; */
/* OBSOLETE   /* It would be tempting to look for "\n[__exit + 0x8]\n" */
/* OBSOLETE      but that requires loading symbols with "yc i" and even if */
/* OBSOLETE      we did do that we don't know that the file has symbols.  */ */
/* OBSOLETE   static char exitmsg[] = "@????????I    JMPTI     GR121,LR0"; */
/* OBSOLETE   char *bp = bpt; */
/* OBSOLETE   char *ep = exitmsg; */
/* OBSOLETE  */
/* OBSOLETE   /* Large enough for either sizeof (bpt) or sizeof (exitmsg) chars.  */ */
/* OBSOLETE   char swallowed[50]; */
/* OBSOLETE   /* Current position in swallowed.  */ */
/* OBSOLETE   char *swallowed_p = swallowed; */
/* OBSOLETE  */
/* OBSOLETE   int ch; */
/* OBSOLETE   int ch_handled; */
/* OBSOLETE   int old_timeout = timeout; */
/* OBSOLETE   int old_immediate_quit = immediate_quit; */
/* OBSOLETE  */
/* OBSOLETE   status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE   status->value.integer = 0; */
/* OBSOLETE  */
/* OBSOLETE   if (need_artificial_trap != 0) */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE       need_artificial_trap--; */
/* OBSOLETE       return inferior_ptid; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   timeout = 0;			/* Don't time out -- user program is running. */ */
/* OBSOLETE   immediate_quit = 1;		/* Helps ability to QUIT */ */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
/* OBSOLETE       QUIT;			/* Let user quit and leave process running */ */
/* OBSOLETE       ch_handled = 0; */
/* OBSOLETE       ch = readchar (); */
/* OBSOLETE       if (ch == *bp) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  bp++; */
/* OBSOLETE 	  if (*bp == '\0') */
/* OBSOLETE 	    break; */
/* OBSOLETE 	  ch_handled = 1; */
/* OBSOLETE  */
/* OBSOLETE 	  *swallowed_p++ = ch; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	bp = bpt; */
/* OBSOLETE       if (ch == *ep || *ep == '?') */
/* OBSOLETE 	{ */
/* OBSOLETE 	  ep++; */
/* OBSOLETE 	  if (*ep == '\0') */
/* OBSOLETE 	    break; */
/* OBSOLETE  */
/* OBSOLETE 	  if (!ch_handled) */
/* OBSOLETE 	    *swallowed_p++ = ch; */
/* OBSOLETE 	  ch_handled = 1; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	ep = exitmsg; */
/* OBSOLETE       if (!ch_handled) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  char *p; */
/* OBSOLETE 	  /* Print out any characters which have been swallowed.  */ */
/* OBSOLETE 	  for (p = swallowed; p < swallowed_p; ++p) */
/* OBSOLETE 	    putc (*p, stdout); */
/* OBSOLETE 	  swallowed_p = swallowed; */
/* OBSOLETE 	  putc (ch, stdout); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   if (*bp == '\0') */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE       status->value.integer = 0; */
/* OBSOLETE     } */
/* OBSOLETE   timeout = old_timeout; */
/* OBSOLETE   immediate_quit = old_immediate_quit; */
/* OBSOLETE   return inferior_ptid; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Return the name of register number REGNO */
/* OBSOLETE    in the form input and output by adapt. */
/* OBSOLETE  */
/* OBSOLETE    Returns a pointer to a static buffer containing the answer.  */ */
/* OBSOLETE static char * */
/* OBSOLETE get_reg_name (int regno) */
/* OBSOLETE { */
/* OBSOLETE   static char buf[80]; */
/* OBSOLETE   if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     sprintf (buf, "GR%03d", regno - GR96_REGNUM + 96); */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32) */
/* OBSOLETE     sprintf (buf, "GR%03d", regno - GR64_REGNUM + 64); */
/* OBSOLETE #endif */
/* OBSOLETE   else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128) */
/* OBSOLETE     sprintf (buf, "LR%03d", regno - LR0_REGNUM); */
/* OBSOLETE   else if (regno == Q_REGNUM) */
/* OBSOLETE     strcpy (buf, "SR131"); */
/* OBSOLETE   else if (regno >= BP_REGNUM && regno <= CR_REGNUM) */
/* OBSOLETE     sprintf (buf, "SR%03d", regno - BP_REGNUM + 133); */
/* OBSOLETE   else if (regno == ALU_REGNUM) */
/* OBSOLETE     strcpy (buf, "SR132"); */
/* OBSOLETE   else if (regno >= IPC_REGNUM && regno <= IPB_REGNUM) */
/* OBSOLETE     sprintf (buf, "SR%03d", regno - IPC_REGNUM + 128); */
/* OBSOLETE   else if (regno >= VAB_REGNUM && regno <= LRU_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       /* When a 29050 is in freeze-mode, read shadow pcs instead */ */
/* OBSOLETE       if ((regno >= NPC_REGNUM && regno <= PC2_REGNUM) && USE_SHADOW_PC) */
/* OBSOLETE 	sprintf (buf, "SR%03d", regno - NPC_REGNUM + 20); */
/* OBSOLETE       else */
/* OBSOLETE 	sprintf (buf, "SR%03d", regno - VAB_REGNUM); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno == GR1_REGNUM) */
/* OBSOLETE     strcpy (buf, "GR001"); */
/* OBSOLETE   return buf; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read the remote registers.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE adapt_fetch_registers (void) */
/* OBSOLETE { */
/* OBSOLETE   int reg_index; */
/* OBSOLETE   int regnum_index; */
/* OBSOLETE   char tempbuf[10]; */
/* OBSOLETE   int sreg_buf[16]; */
/* OBSOLETE   int i, j; */
/* OBSOLETE  */
/* OBSOLETE /*  */
/* OBSOLETE  * Global registers */
/* OBSOLETE  */ */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   write (adapt_desc, "dw gr64,gr95\r", 13); */
/* OBSOLETE   for (reg_index = 64, regnum_index = GR64_REGNUM; */
/* OBSOLETE        reg_index < 96; */
/* OBSOLETE        reg_index += 4, regnum_index += 4) */
/* OBSOLETE     { */
/* OBSOLETE       sprintf (tempbuf, "GR%03d ", reg_index); */
/* OBSOLETE       expect (tempbuf); */
/* OBSOLETE       get_hex_regs (4, regnum_index); */
/* OBSOLETE       expect ("\n"); */
/* OBSOLETE     } */
/* OBSOLETE #endif */
/* OBSOLETE   write (adapt_desc, "dw gr96,gr127\r", 14); */
/* OBSOLETE   for (reg_index = 96, regnum_index = GR96_REGNUM; */
/* OBSOLETE        reg_index < 128; */
/* OBSOLETE        reg_index += 4, regnum_index += 4) */
/* OBSOLETE     { */
/* OBSOLETE       sprintf (tempbuf, "GR%03d ", reg_index); */
/* OBSOLETE       expect (tempbuf); */
/* OBSOLETE       get_hex_regs (4, regnum_index); */
/* OBSOLETE       expect ("\n"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /*  */
/* OBSOLETE  * Local registers */
/* OBSOLETE  */ */
/* OBSOLETE   for (i = 0; i < 128; i += 32) */
/* OBSOLETE     { */
/* OBSOLETE       /* The PC has a tendency to hang if we get these */
/* OBSOLETE          all in one fell swoop ("dw lr0,lr127").  */ */
/* OBSOLETE       sprintf (tempbuf, "dw lr%d\r", i); */
/* OBSOLETE       write (adapt_desc, tempbuf, strlen (tempbuf)); */
/* OBSOLETE       for (reg_index = i, regnum_index = LR0_REGNUM + i; */
/* OBSOLETE 	   reg_index < i + 32; */
/* OBSOLETE 	   reg_index += 4, regnum_index += 4) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  sprintf (tempbuf, "LR%03d ", reg_index); */
/* OBSOLETE 	  expect (tempbuf); */
/* OBSOLETE 	  get_hex_regs (4, regnum_index); */
/* OBSOLETE 	  expect ("\n"); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /*  */
/* OBSOLETE  * Special registers */
/* OBSOLETE  */ */
/* OBSOLETE   sprintf (tempbuf, "dw sr0\r"); */
/* OBSOLETE   write (adapt_desc, tempbuf, strlen (tempbuf)); */
/* OBSOLETE   for (i = 0; i < 4; i++) */
/* OBSOLETE     {				/* SR0 - SR14 */ */
/* OBSOLETE       sprintf (tempbuf, "SR%3d", i * 4); */
/* OBSOLETE       expect (tempbuf); */
/* OBSOLETE       for (j = 0; j < (i == 3 ? 3 : 4); j++) */
/* OBSOLETE 	sreg_buf[i * 4 + j] = get_hex_word (); */
/* OBSOLETE     } */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   /*  */
/* OBSOLETE    * Read the pcs individually if we are in freeze mode. */
/* OBSOLETE    * See get_reg_name(), it translates the register names for the pcs to */
/* OBSOLETE    * the names of the shadow pcs. */
/* OBSOLETE    */ */
/* OBSOLETE   if (USE_SHADOW_PC) */
/* OBSOLETE     { */
/* OBSOLETE       sreg_buf[10] = read_register (NPC_REGNUM);	/* pc0 */ */
/* OBSOLETE       sreg_buf[11] = read_register (PC_REGNUM);		/* pc1 */ */
/* OBSOLETE       sreg_buf[12] = read_register (PC2_REGNUM);	/* pc2 */ */
/* OBSOLETE     } */
/* OBSOLETE   for (i = 0; i < 14; i++)	/* Supply vab -> lru */ */
/* OBSOLETE     supply_register (VAB_REGNUM + i, (char *) &sreg_buf[i]); */
/* OBSOLETE   sprintf (tempbuf, "dw sr128\r"); */
/* OBSOLETE   write (adapt_desc, tempbuf, strlen (tempbuf)); */
/* OBSOLETE   for (i = 0; i < 2; i++) */
/* OBSOLETE     {				/* SR128 - SR135 */ */
/* OBSOLETE       sprintf (tempbuf, "SR%3d", 128 + i * 4); */
/* OBSOLETE       expect (tempbuf); */
/* OBSOLETE       for (j = 0; j < 4; j++) */
/* OBSOLETE 	sreg_buf[i * 4 + j] = get_hex_word (); */
/* OBSOLETE     } */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   supply_register (IPC_REGNUM, (char *) &sreg_buf[0]); */
/* OBSOLETE   supply_register (IPA_REGNUM, (char *) &sreg_buf[1]); */
/* OBSOLETE   supply_register (IPB_REGNUM, (char *) &sreg_buf[2]); */
/* OBSOLETE   supply_register (Q_REGNUM, (char *) &sreg_buf[3]); */
/* OBSOLETE   /* Skip ALU */ */
/* OBSOLETE   supply_register (BP_REGNUM, (char *) &sreg_buf[5]); */
/* OBSOLETE   supply_register (FC_REGNUM, (char *) &sreg_buf[6]); */
/* OBSOLETE   supply_register (CR_REGNUM, (char *) &sreg_buf[7]); */
/* OBSOLETE  */
/* OBSOLETE   /* There doesn't seem to be any way to get these.  */ */
/* OBSOLETE   { */
/* OBSOLETE     int val = -1; */
/* OBSOLETE     supply_register (FPE_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (INTE_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (FPS_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (EXO_REGNUM, (char *) &val); */
/* OBSOLETE   } */
/* OBSOLETE  */
/* OBSOLETE   write (adapt_desc, "dw gr1,gr1\r", 11); */
/* OBSOLETE   expect ("GR001 "); */
/* OBSOLETE   get_hex_regs (1, GR1_REGNUM); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Fetch register REGNO, or all registers if REGNO is -1. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE adapt_fetch_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   if (regno == -1) */
/* OBSOLETE     adapt_fetch_registers (); */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       char *name = get_reg_name (regno); */
/* OBSOLETE       fprintf (adapt_stream, "dw %s,%s\r", name, name); */
/* OBSOLETE       expect (name); */
/* OBSOLETE       expect (" "); */
/* OBSOLETE       get_hex_regs (1, regno); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Store the remote registers from the contents of the block REGS.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE adapt_store_registers (void) */
/* OBSOLETE { */
/* OBSOLETE   int i, j; */
/* OBSOLETE  */
/* OBSOLETE   fprintf (adapt_stream, "s gr1,%x\r", read_register (GR1_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   for (j = 0; j < 32; j += 16) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (adapt_stream, "s gr%d,", j + 64); */
/* OBSOLETE       for (i = 0; i < 15; ++i) */
/* OBSOLETE 	fprintf (adapt_stream, "%x,", read_register (GR64_REGNUM + j + i)); */
/* OBSOLETE       fprintf (adapt_stream, "%x\r", read_register (GR64_REGNUM + j + 15)); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE #endif */
/* OBSOLETE   for (j = 0; j < 32; j += 16) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (adapt_stream, "s gr%d,", j + 96); */
/* OBSOLETE       for (i = 0; i < 15; ++i) */
/* OBSOLETE 	fprintf (adapt_stream, "%x,", read_register (GR96_REGNUM + j + i)); */
/* OBSOLETE       fprintf (adapt_stream, "%x\r", read_register (GR96_REGNUM + j + 15)); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   for (j = 0; j < 128; j += 16) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (adapt_stream, "s lr%d,", j); */
/* OBSOLETE       for (i = 0; i < 15; ++i) */
/* OBSOLETE 	fprintf (adapt_stream, "%x,", read_register (LR0_REGNUM + j + i)); */
/* OBSOLETE       fprintf (adapt_stream, "%x\r", read_register (LR0_REGNUM + j + 15)); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   fprintf (adapt_stream, "s sr128,%x,%x,%x\r", read_register (IPC_REGNUM), */
/* OBSOLETE 	   read_register (IPA_REGNUM), read_register (IPB_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (adapt_stream, "s sr133,%x,%x,%x\r", read_register (BP_REGNUM), */
/* OBSOLETE 	   read_register (FC_REGNUM), read_register (CR_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (adapt_stream, "s sr131,%x\r", read_register (Q_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (adapt_stream, "s sr0,"); */
/* OBSOLETE   for (i = 0; i < 7; ++i) */
/* OBSOLETE     fprintf (adapt_stream, "%x,", read_register (VAB_REGNUM + i)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (adapt_stream, "s sr7,"); */
/* OBSOLETE   for (i = 7; i < 14; ++i) */
/* OBSOLETE     fprintf (adapt_stream, "%x,", read_register (VAB_REGNUM + i)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Store register REGNO, or all if REGNO == -1. */
/* OBSOLETE    Return errno value.  */ */
/* OBSOLETE void */
/* OBSOLETE adapt_store_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   /* printf("adapt_store_register() called.\n"); fflush(stdout); /* */ */
/* OBSOLETE   if (regno == -1) */
/* OBSOLETE     adapt_store_registers (); */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       char *name = get_reg_name (regno); */
/* OBSOLETE       fprintf (adapt_stream, "s %s,%x\r", name, read_register (regno)); */
/* OBSOLETE       /* Setting GR1 changes the numbers of all the locals, so */
/* OBSOLETE          invalidate the register cache.  Do this *after* calling */
/* OBSOLETE          read_register, because we want read_register to return the */
/* OBSOLETE          value that write_register has just stuffed into the registers */
/* OBSOLETE          array, not the value of the register fetched from the */
/* OBSOLETE          inferior.  */ */
/* OBSOLETE       if (regno == GR1_REGNUM) */
/* OBSOLETE 	registers_changed (); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Get ready to modify the registers array.  On machines which store */
/* OBSOLETE    individual registers, this doesn't need to do anything.  On machines */
/* OBSOLETE    which store all the registers in one fell swoop, this makes sure */
/* OBSOLETE    that registers contains all the registers from the program being */
/* OBSOLETE    debugged.  */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_prepare_to_store (void) */
/* OBSOLETE { */
/* OBSOLETE   /* Do nothing, since we can store individual regs */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE static CORE_ADDR */
/* OBSOLETE translate_addr (CORE_ADDR addr) */
/* OBSOLETE { */
/* OBSOLETE #if defined(KERNEL_DEBUGGING) */
/* OBSOLETE   /* Check for a virtual address in the kernel */ */
/* OBSOLETE   /* Assume physical address of ublock is in  paddr_u register */ */
/* OBSOLETE   if (addr >= UVADDR) */
/* OBSOLETE     { */
/* OBSOLETE       /* PADDR_U register holds the physical address of the ublock */ */
/* OBSOLETE       CORE_ADDR i = (CORE_ADDR) read_register (PADDR_U_REGNUM); */
/* OBSOLETE       return (i + addr - (CORE_ADDR) UVADDR); */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       return (addr); */
/* OBSOLETE     } */
/* OBSOLETE #else */
/* OBSOLETE   return (addr); */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /* FIXME!  Merge these two.  */ */
/* OBSOLETE int */
/* OBSOLETE adapt_xfer_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len, int write, */
/* OBSOLETE 			    struct mem_attrib *attrib ATTRIBUTE_UNUSED, */
/* OBSOLETE 			    struct target_ops *target ATTRIBUTE_UNUSED) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   memaddr = translate_addr (memaddr); */
/* OBSOLETE  */
/* OBSOLETE   if (write) */
/* OBSOLETE     return adapt_write_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE   else */
/* OBSOLETE     return adapt_read_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_files_info (void) */
/* OBSOLETE { */
/* OBSOLETE   printf_filtered ("\tAttached to %s at %d baud and running program %s\n", */
/* OBSOLETE 		   dev_name, baudrate, prog_name); */
/* OBSOLETE   printf_filtered ("\ton an %s processor.\n", processor_name[processor_type]); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Copy LEN bytes of data from debugger memory at MYADDR */
/* OBSOLETE    to inferior's memory at MEMADDR.  Returns errno value.   */
/* OBSOLETE    * sb/sh instructions don't work on unaligned addresses, when TU=1.  */
/* OBSOLETE  */ */
/* OBSOLETE int */
/* OBSOLETE adapt_write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int i; */
/* OBSOLETE   unsigned int cps; */
/* OBSOLETE  */
/* OBSOLETE   /* Turn TU bit off so we can do 'sb' commands */ */
/* OBSOLETE   cps = read_register (CPS_REGNUM); */
/* OBSOLETE   if (cps & 0x00000800) */
/* OBSOLETE     write_register (CPS_REGNUM, cps & ~(0x00000800)); */
/* OBSOLETE  */
/* OBSOLETE   for (i = 0; i < len; i++) */
/* OBSOLETE     { */
/* OBSOLETE       if ((i % 16) == 0) */
/* OBSOLETE 	fprintf (adapt_stream, "sb %x,", memaddr + i); */
/* OBSOLETE       if ((i % 16) == 15 || i == len - 1) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  fprintf (adapt_stream, "%x\r", ((unsigned char *) myaddr)[i]); */
/* OBSOLETE 	  expect_prompt (); */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	fprintf (adapt_stream, "%x,", ((unsigned char *) myaddr)[i]); */
/* OBSOLETE     } */
/* OBSOLETE   /* Restore the old value of cps if the TU bit was on */ */
/* OBSOLETE   if (cps & 0x00000800) */
/* OBSOLETE     write_register (CPS_REGNUM, cps); */
/* OBSOLETE   return len; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read LEN bytes from inferior memory at MEMADDR.  Put the result */
/* OBSOLETE    at debugger address MYADDR.  Returns errno value.  */ */
/* OBSOLETE int */
/* OBSOLETE adapt_read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   /* Number of bytes read so far.  */ */
/* OBSOLETE   int count; */
/* OBSOLETE  */
/* OBSOLETE   /* Starting address of this pass.  */ */
/* OBSOLETE   unsigned long startaddr; */
/* OBSOLETE  */
/* OBSOLETE   /* Number of bytes to read in this pass.  */ */
/* OBSOLETE   int len_this_pass; */
/* OBSOLETE  */
/* OBSOLETE   /* Note that this code works correctly if startaddr is just less */
/* OBSOLETE      than UINT_MAX (well, really CORE_ADDR_MAX if there was such a */
/* OBSOLETE      thing).  That is, something like */
/* OBSOLETE      adapt_read_bytes (CORE_ADDR_MAX - 4, foo, 4) */
/* OBSOLETE      works--it never adds len to memaddr and gets 0.  */ */
/* OBSOLETE   /* However, something like */
/* OBSOLETE      adapt_read_bytes (CORE_ADDR_MAX - 3, foo, 4) */
/* OBSOLETE      doesn't need to work.  Detect it and give up if there's an attempt */
/* OBSOLETE      to do that.  */ */
/* OBSOLETE  */
/* OBSOLETE   if (((memaddr - 1) + len) < memaddr) */
/* OBSOLETE     return EIO; */
/* OBSOLETE  */
/* OBSOLETE   startaddr = memaddr; */
/* OBSOLETE   count = 0; */
/* OBSOLETE   while (count < len) */
/* OBSOLETE     { */
/* OBSOLETE       len_this_pass = 16; */
/* OBSOLETE       if ((startaddr % 16) != 0) */
/* OBSOLETE 	len_this_pass -= startaddr % 16; */
/* OBSOLETE       if (len_this_pass > (len - count)) */
/* OBSOLETE 	len_this_pass = (len - count); */
/* OBSOLETE  */
/* OBSOLETE       fprintf (adapt_stream, "db %x,%x\r", startaddr, */
/* OBSOLETE 	       (startaddr - 1) + len_this_pass); */
/* OBSOLETE  */
/* OBSOLETE #ifdef NOTDEF			/* Why do this */ */
/* OBSOLETE       expect ("\n"); */
/* OBSOLETE       /* Look for 8 hex digits.  */ */
/* OBSOLETE       i = 0; */
/* OBSOLETE       while (1) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  if (isxdigit (readchar ())) */
/* OBSOLETE 	    ++i; */
/* OBSOLETE 	  else */
/* OBSOLETE 	    { */
/* OBSOLETE 	      expect_prompt (); */
/* OBSOLETE 	      error ("Hex digit expected from remote system."); */
/* OBSOLETE 	    } */
/* OBSOLETE 	  if (i >= 8) */
/* OBSOLETE 	    break; */
/* OBSOLETE 	} */
/* OBSOLETE #endif /* NOTDEF */ */
/* OBSOLETE  */
/* OBSOLETE       expect ("  "); */
/* OBSOLETE  */
/* OBSOLETE       for (i = 0; i < len_this_pass; i++) */
/* OBSOLETE 	get_hex_byte (&myaddr[count++]); */
/* OBSOLETE  */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE       startaddr += len_this_pass; */
/* OBSOLETE     } */
/* OBSOLETE   return count; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE #define MAX_BREAKS	8 */
/* OBSOLETE static int num_brkpts = 0; */
/* OBSOLETE  */
/* OBSOLETE /* Insert a breakpoint at ADDR.  SAVE is normally the address of the */
/* OBSOLETE    pattern buffer where the instruction that the breakpoint overwrites */
/* OBSOLETE    is saved.  It is unused here since the Adapt Monitor is responsible */
/* OBSOLETE    for saving/restoring the original instruction. */ */
/* OBSOLETE  */
/* OBSOLETE static int */
/* OBSOLETE adapt_insert_breakpoint (CORE_ADDR addr, char *save) */
/* OBSOLETE { */
/* OBSOLETE   if (num_brkpts < MAX_BREAKS) */
/* OBSOLETE     { */
/* OBSOLETE       num_brkpts++; */
/* OBSOLETE       fprintf (adapt_stream, "B %x", addr); */
/* OBSOLETE       fprintf (adapt_stream, "\r"); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE       return (0);		/* Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       fprintf_filtered (gdb_stderr, */
/* OBSOLETE 		      "Too many break points, break point not installed\n"); */
/* OBSOLETE       return (1);		/* Failure */ */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Remove a breakpoint at ADDR.  SAVE is normally the previously */
/* OBSOLETE    saved pattern, but is unused here as the Adapt Monitor is */
/* OBSOLETE    responsible for saving/restoring instructions. */ */
/* OBSOLETE  */
/* OBSOLETE static int */
/* OBSOLETE adapt_remove_breakpoint (CORE_ADDR addr, char *save) */
/* OBSOLETE { */
/* OBSOLETE   if (num_brkpts > 0) */
/* OBSOLETE     { */
/* OBSOLETE       num_brkpts--; */
/* OBSOLETE       fprintf (adapt_stream, "BR %x", addr); */
/* OBSOLETE       fprintf (adapt_stream, "\r"); */
/* OBSOLETE       fflush (adapt_stream); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE   return (0); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Clear the adapts notion of what the break points are */ */
/* OBSOLETE static int */
/* OBSOLETE adapt_clear_breakpoints (void) */
/* OBSOLETE { */
/* OBSOLETE   if (adapt_stream) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (adapt_stream, "BR");	/* Clear all break points */ */
/* OBSOLETE       fprintf (adapt_stream, "\r"); */
/* OBSOLETE       fflush (adapt_stream); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE   num_brkpts = 0; */
/* OBSOLETE } */
/* OBSOLETE static void */
/* OBSOLETE adapt_mourn (void) */
/* OBSOLETE { */
/* OBSOLETE   adapt_clear_breakpoints (); */
/* OBSOLETE   pop_target ();		/* Pop back to no-child state */ */
/* OBSOLETE   generic_mourn_inferior (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Display everthing we read in from the adapt until we match/see the */
/* OBSOLETE  * specified string */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE display_until (char *str) */
/* OBSOLETE { */
/* OBSOLETE   int i = 0, j, c; */
/* OBSOLETE  */
/* OBSOLETE   while (c = readchar ()) */
/* OBSOLETE     { */
/* OBSOLETE       if (c == str[i]) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  i++; */
/* OBSOLETE 	  if (i == strlen (str)) */
/* OBSOLETE 	    return; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  if (i) */
/* OBSOLETE 	    { */
/* OBSOLETE 	      for (j = 0; j < i; j++)	/* Put everthing we matched */ */
/* OBSOLETE 		putchar (str[j]); */
/* OBSOLETE 	      i = 0; */
/* OBSOLETE 	    } */
/* OBSOLETE 	  putchar (c); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /* Put a command string, in args, out to the adapt.  The adapt is assumed to */
/* OBSOLETE    be in raw mode, all writing/reading done through adapt_desc. */
/* OBSOLETE    Ouput from the adapt is placed on the users terminal until the */
/* OBSOLETE    prompt from the adapt is seen. */
/* OBSOLETE    FIXME: Can't handle commands that take input.  */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE adapt_com (char *args, int fromtty) */
/* OBSOLETE { */
/* OBSOLETE   if (!adapt_stream) */
/* OBSOLETE     { */
/* OBSOLETE       printf_filtered ("Adapt not open.  Use the 'target' command to open.\n"); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Clear all input so only command relative output is displayed */ */
/* OBSOLETE   slurp_input (); */
/* OBSOLETE  */
/* OBSOLETE   switch (islower (args[0]) ? toupper (args[0]) : args[0]) */
/* OBSOLETE     { */
/* OBSOLETE     default: */
/* OBSOLETE       printf_filtered ("Unknown/Unimplemented adapt command '%s'\n", args); */
/* OBSOLETE       break; */
/* OBSOLETE     case 'G':			/* Go, begin execution */ */
/* OBSOLETE       write (adapt_desc, args, strlen (args)); */
/* OBSOLETE       write (adapt_desc, "\r", 1); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE       break; */
/* OBSOLETE     case 'B':			/* Break points, B or BR */ */
/* OBSOLETE     case 'C':			/* Check current 29k status (running/halted) */ */
/* OBSOLETE     case 'D':			/* Display data/registers */ */
/* OBSOLETE     case 'I':			/* Input from i/o space */ */
/* OBSOLETE     case 'J':			/* Jam an instruction */ */
/* OBSOLETE     case 'K':			/* Kill, stop execution */ */
/* OBSOLETE     case 'L':			/* Disassemble */ */
/* OBSOLETE     case 'O':			/* Output to i/o space */ */
/* OBSOLETE     case 'T':			/* Trace */ */
/* OBSOLETE     case 'P':			/* Pulse an input line */ */
/* OBSOLETE     case 'X':			/* Examine special purpose registers */ */
/* OBSOLETE     case 'Z':			/* Display trace buffer */ */
/* OBSOLETE       write (adapt_desc, args, strlen (args)); */
/* OBSOLETE       write (adapt_desc, "\r", 1); */
/* OBSOLETE       expect (args);		/* Don't display the command */ */
/* OBSOLETE       display_until ("# "); */
/* OBSOLETE       break; */
/* OBSOLETE       /* Begin commands that take input in the form 'c x,y[,z...]' */ */
/* OBSOLETE     case 'S':			/* Set memory or register */ */
/* OBSOLETE       if (strchr (args, ',')) */
/* OBSOLETE 	{			/* Assume it is properly formatted */ */
/* OBSOLETE 	  write (adapt_desc, args, strlen (args)); */
/* OBSOLETE 	  write (adapt_desc, "\r", 1); */
/* OBSOLETE 	  expect_prompt (); */
/* OBSOLETE 	} */
/* OBSOLETE       break; */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Define the target subroutine names */ */
/* OBSOLETE  */
/* OBSOLETE struct target_ops adapt_ops; */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE init_adapt_ops (void) */
/* OBSOLETE { */
/* OBSOLETE   adapt_ops.to_shortname = "adapt"; */
/* OBSOLETE   adapt_ops.to_longname = "Remote AMD `Adapt' target"; */
/* OBSOLETE   adapt_ops.to_doc = "Remote debug an AMD 290*0 using an `Adapt' monitor via RS232"; */
/* OBSOLETE   adapt_ops.to_open = adapt_open; */
/* OBSOLETE   adapt_ops.to_close = adapt_close; */
/* OBSOLETE   adapt_ops.to_attach = adapt_attach; */
/* OBSOLETE   adapt_ops.to_post_attach = NULL; */
/* OBSOLETE   adapt_ops.to_require_attach = NULL; */
/* OBSOLETE   adapt_ops.to_detach = adapt_detach; */
/* OBSOLETE   adapt_ops.to_require_detach = NULL; */
/* OBSOLETE   adapt_ops.to_resume = adapt_resume; */
/* OBSOLETE   adapt_ops.to_wait = adapt_wait; */
/* OBSOLETE   adapt_ops.to_post_wait = NULL; */
/* OBSOLETE   adapt_ops.to_fetch_registers = adapt_fetch_register; */
/* OBSOLETE   adapt_ops.to_store_registers = adapt_store_register; */
/* OBSOLETE   adapt_ops.to_prepare_to_store = adapt_prepare_to_store; */
/* OBSOLETE   adapt_ops.to_xfer_memory = adapt_xfer_inferior_memory; */
/* OBSOLETE   adapt_ops.to_files_info = adapt_files_info; */
/* OBSOLETE   adapt_ops.to_insert_breakpoint = adapt_insert_breakpoint; */
/* OBSOLETE   adapt_ops.to_remove_breakpoint = adapt_remove_breakpoint; */
/* OBSOLETE   adapt_ops.to_terminal_init = 0; */
/* OBSOLETE   adapt_ops.to_terminal_inferior = 0; */
/* OBSOLETE   adapt_ops.to_terminal_ours_for_output = 0; */
/* OBSOLETE   adapt_ops.to_terminal_ours = 0; */
/* OBSOLETE   adapt_ops.to_terminal_info = 0; */
/* OBSOLETE   adapt_ops.to_kill = adapt_kill; */
/* OBSOLETE   adapt_ops.to_load = adapt_load; */
/* OBSOLETE   adapt_ops.to_lookup_symbol = 0; */
/* OBSOLETE   adapt_ops.to_create_inferior = adapt_create_inferior; */
/* OBSOLETE   adapt_ops.to_post_startup_inferior = NULL; */
/* OBSOLETE   adapt_ops.to_acknowledge_created_inferior = NULL; */
/* OBSOLETE   adapt_ops.to_clone_and_follow_inferior = NULL; */
/* OBSOLETE   adapt_ops.to_post_follow_inferior_by_clone = NULL; */
/* OBSOLETE   adapt_ops.to_insert_fork_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_remove_fork_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_insert_vfork_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_remove_vfork_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_has_forked = NULL; */
/* OBSOLETE   adapt_ops.to_has_vforked = NULL; */
/* OBSOLETE   adapt_ops.to_can_follow_vfork_prior_to_exec = NULL; */
/* OBSOLETE   adapt_ops.to_post_follow_vfork = NULL; */
/* OBSOLETE   adapt_ops.to_insert_exec_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_remove_exec_catchpoint = NULL; */
/* OBSOLETE   adapt_ops.to_has_execd = NULL; */
/* OBSOLETE   adapt_ops.to_reported_exec_events_per_exec_call = NULL; */
/* OBSOLETE   adapt_ops.to_has_exited = NULL; */
/* OBSOLETE   adapt_ops.to_mourn_inferior = adapt_mourn; */
/* OBSOLETE   adapt_ops.to_can_run = 0; */
/* OBSOLETE   adapt_ops.to_notice_signals = 0; */
/* OBSOLETE   adapt_ops.to_thread_alive = 0; */
/* OBSOLETE   adapt_ops.to_stop = 0;	/* process_stratum; */ */
/* OBSOLETE   adapt_ops.to_pid_to_exec_file = NULL; */
/* OBSOLETE   adapt_ops.to_stratum = 0; */
/* OBSOLETE   adapt_ops.DONT_USE = 0; */
/* OBSOLETE   adapt_ops.to_has_all_memory = 1; */
/* OBSOLETE   adapt_ops.to_has_memory = 1; */
/* OBSOLETE   adapt_ops.to_has_stack = 1; */
/* OBSOLETE   adapt_ops.to_has_registers = 1; */
/* OBSOLETE   adapt_ops.to_has_execution = 0; */
/* OBSOLETE   adapt_ops.to_sections = 0; */
/* OBSOLETE   adapt_ops.to_sections_end = 0; */
/* OBSOLETE   adapt_ops.to_magic = OPS_MAGIC; */
/* OBSOLETE }				/* init_adapt_ops */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE _initialize_remote_adapt (void) */
/* OBSOLETE { */
/* OBSOLETE   init_adapt_ops (); */
/* OBSOLETE   add_target (&adapt_ops); */
/* OBSOLETE   add_com ("adapt <command>", class_obscure, adapt_com, */
/* OBSOLETE 	   "Send a command to the AMD Adapt remote monitor."); */
/* OBSOLETE } */
