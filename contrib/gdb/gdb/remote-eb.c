/* OBSOLETE /* Remote debugging interface for AMD 29000 EBMON on IBM PC, for GDB. */
/* OBSOLETE    Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1998, 1999, 2000, 2001 */
/* OBSOLETE    Free Software Foundation, Inc. */
/* OBSOLETE    Contributed by Cygnus Support.  Written by Jim Kingdon for Cygnus. */
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
/* OBSOLETE    having a a29k board in a PC hooked up to a unix machine with */
/* OBSOLETE    a serial line, and running ctty com1 on the PC, through which */
/* OBSOLETE    the unix machine can run ebmon.  Not to mention that the PC */
/* OBSOLETE    has PC/NFS, so it can access the same executables that gdb can, */
/* OBSOLETE    over the net in real time.  */ */
/* OBSOLETE  */
/* OBSOLETE #include "defs.h" */
/* OBSOLETE #include "gdb_string.h" */
/* OBSOLETE #include "regcache.h" */
/* OBSOLETE  */
/* OBSOLETE #include "inferior.h" */
/* OBSOLETE #include "bfd.h" */
/* OBSOLETE #include "symfile.h" */
/* OBSOLETE #include "value.h" */
/* OBSOLETE #include <ctype.h> */
/* OBSOLETE #include <fcntl.h> */
/* OBSOLETE #include <signal.h> */
/* OBSOLETE #include <errno.h> */
/* OBSOLETE #include "terminal.h" */
/* OBSOLETE #include "target.h" */
/* OBSOLETE #include "gdbcore.h" */
/* OBSOLETE  */
/* OBSOLETE extern struct target_ops eb_ops;	/* Forward declaration */ */
/* OBSOLETE  */
/* OBSOLETE static void eb_close (); */
/* OBSOLETE  */
/* OBSOLETE #define LOG_FILE "eb.log" */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE FILE *log_file; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE static int timeout = 24; */
/* OBSOLETE  */
/* OBSOLETE /* Descriptor for I/O to remote machine.  Initialize it to -1 so that */
/* OBSOLETE    eb_open knows that we don't have a file open when the program */
/* OBSOLETE    starts.  */ */
/* OBSOLETE int eb_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE /* stream which is fdopen'd from eb_desc.  Only valid when */
/* OBSOLETE    eb_desc != -1.  */ */
/* OBSOLETE FILE *eb_stream; */
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
/* OBSOLETE   read (eb_desc, &buf, 1); */
/* OBSOLETE #else */
/* OBSOLETE   alarm (timeout); */
/* OBSOLETE   if (read (eb_desc, &buf, 1) < 0) */
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
/* OBSOLETE /* Keep discarding input until we see the ebmon prompt. */
/* OBSOLETE  */
/* OBSOLETE    The convention for dealing with the prompt is that you */
/* OBSOLETE    o give your command */
/* OBSOLETE    o *then* wait for the prompt. */
/* OBSOLETE  */
/* OBSOLETE    Thus the last thing that a procedure does with the serial line */
/* OBSOLETE    will be an expect_prompt().  Exception:  eb_resume does not */
/* OBSOLETE    wait for the prompt, because the terminal is being handed over */
/* OBSOLETE    to the inferior.  However, the next thing which happens after that */
/* OBSOLETE    is a eb_wait which does wait for the prompt. */
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
/* OBSOLETE /* Get a byte from eb_desc and put it in *BYT.  Accept any number */
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
/* OBSOLETE /* Get N 32-bit words from remote, each preceded by a space, */
/* OBSOLETE    and put them in registers starting at REGNO.  */ */
/* OBSOLETE static void */
/* OBSOLETE get_hex_regs (int n, int regno) */
/* OBSOLETE { */
/* OBSOLETE   long val; */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   for (i = 0; i < n; i++) */
/* OBSOLETE     { */
/* OBSOLETE       int j; */
/* OBSOLETE  */
/* OBSOLETE       val = 0; */
/* OBSOLETE       for (j = 0; j < 8; j++) */
/* OBSOLETE 	val = (val << 4) + get_hex_digit (j == 0); */
/* OBSOLETE       supply_register (regno++, (char *) &val); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Called when SIGALRM signal sent due to alarm() timeout.  */ */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE  */
/* OBSOLETE volatile int n_alarms; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE eb_timer (void) */
/* OBSOLETE { */
/* OBSOLETE #if 0 */
/* OBSOLETE   if (kiodebug) */
/* OBSOLETE     printf ("eb_timer called\n"); */
/* OBSOLETE #endif */
/* OBSOLETE   n_alarms++; */
/* OBSOLETE } */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /* malloc'd name of the program on the remote system.  */ */
/* OBSOLETE static char *prog_name = NULL; */
/* OBSOLETE  */
/* OBSOLETE /* Nonzero if we have loaded the file ("yc") and not yet issued a "gi" */
/* OBSOLETE    command.  "gi" is supposed to happen exactly once for each "yc".  */ */
/* OBSOLETE static int need_gi = 0; */
/* OBSOLETE  */
/* OBSOLETE /* Number of SIGTRAPs we need to simulate.  That is, the next */
/* OBSOLETE    NEED_ARTIFICIAL_TRAP calls to eb_wait should just return */
/* OBSOLETE    SIGTRAP without actually waiting for anything.  */ */
/* OBSOLETE  */
/* OBSOLETE static int need_artificial_trap = 0; */
/* OBSOLETE  */
/* OBSOLETE /* This is called not only when we first attach, but also when the */
/* OBSOLETE    user types "run" after having attached.  */ */
/* OBSOLETE static void */
/* OBSOLETE eb_create_inferior (char *execfile, char *args, char **env) */
/* OBSOLETE { */
/* OBSOLETE   int entry_pt; */
/* OBSOLETE  */
/* OBSOLETE   if (args && *args) */
/* OBSOLETE     error ("Can't pass arguments to remote EBMON process"); */
/* OBSOLETE  */
/* OBSOLETE   if (execfile == 0 || exec_bfd == 0) */
/* OBSOLETE     error ("No executable file specified"); */
/* OBSOLETE  */
/* OBSOLETE   entry_pt = (int) bfd_get_start_address (exec_bfd); */
/* OBSOLETE  */
/* OBSOLETE   { */
/* OBSOLETE     /* OK, now read in the file.  Y=read, C=COFF, D=no symbols */
/* OBSOLETE        0=start address, %s=filename.  */ */
/* OBSOLETE  */
/* OBSOLETE     fprintf (eb_stream, "YC D,0:%s", prog_name); */
/* OBSOLETE  */
/* OBSOLETE     if (args != NULL) */
/* OBSOLETE       fprintf (eb_stream, " %s", args); */
/* OBSOLETE  */
/* OBSOLETE     fprintf (eb_stream, "\n"); */
/* OBSOLETE     fflush (eb_stream); */
/* OBSOLETE  */
/* OBSOLETE     expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE     need_gi = 1; */
/* OBSOLETE   } */
/* OBSOLETE  */
/* OBSOLETE /* The "process" (board) is already stopped awaiting our commands, and */
/* OBSOLETE    the program is already downloaded.  We just set its PC and go.  */ */
/* OBSOLETE  */
/* OBSOLETE   clear_proceed_status (); */
/* OBSOLETE  */
/* OBSOLETE   /* Tell wait_for_inferior that we've started a new process.  */ */
/* OBSOLETE   init_wait_for_inferior (); */
/* OBSOLETE  */
/* OBSOLETE   /* Set up the "saved terminal modes" of the inferior */
/* OBSOLETE      based on what modes we are starting it with.  */ */
/* OBSOLETE   target_terminal_init (); */
/* OBSOLETE  */
/* OBSOLETE   /* Install inferior's terminal modes.  */ */
/* OBSOLETE   target_terminal_inferior (); */
/* OBSOLETE  */
/* OBSOLETE   /* insert_step_breakpoint ();  FIXME, do we need this?  */ */
/* OBSOLETE   proceed ((CORE_ADDR) entry_pt, TARGET_SIGNAL_DEFAULT, 0);	/* Let 'er rip... */ */
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
/* OBSOLETE struct */
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
/* OBSOLETE int */
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
/* OBSOLETE    then the name of the program as we should name it to EBMON.  */ */
/* OBSOLETE  */
/* OBSOLETE static int baudrate = 9600; */
/* OBSOLETE static char *dev_name; */
/* OBSOLETE void */
/* OBSOLETE eb_open (char *name, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   TERMINAL sg; */
/* OBSOLETE  */
/* OBSOLETE   char *p; */
/* OBSOLETE  */
/* OBSOLETE   target_preopen (from_tty); */
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
/* OBSOLETE   dev_name = alloca (p - name + 1); */
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
/* OBSOLETE   eb_close (0); */
/* OBSOLETE  */
/* OBSOLETE   eb_desc = open (dev_name, O_RDWR); */
/* OBSOLETE   if (eb_desc < 0) */
/* OBSOLETE     perror_with_name (dev_name); */
/* OBSOLETE   ioctl (eb_desc, TIOCGETP, &sg); */
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
/* OBSOLETE   ioctl (eb_desc, TIOCSETP, &sg); */
/* OBSOLETE   eb_stream = fdopen (eb_desc, "r+"); */
/* OBSOLETE  */
/* OBSOLETE   push_target (&eb_ops); */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf ("Remote %s debugging %s using %s\n", target_shortname, */
/* OBSOLETE 	    prog_name, dev_name); */
/* OBSOLETE  */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE #ifndef NO_SIGINTERRUPT */
/* OBSOLETE   /* Cause SIGALRM's to make reads fail with EINTR instead of resuming */
/* OBSOLETE      the read.  */ */
/* OBSOLETE   if (siginterrupt (SIGALRM, 1) != 0) */
/* OBSOLETE     perror ("eb_open: error in siginterrupt"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Set up read timeout timer.  */ */
/* OBSOLETE   if ((void (*)) signal (SIGALRM, eb_timer) == (void (*)) -1) */
/* OBSOLETE     perror ("eb_open: error in signal"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   log_file = fopen (LOG_FILE, "w"); */
/* OBSOLETE   if (log_file == NULL) */
/* OBSOLETE     perror_with_name (LOG_FILE); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Hello?  Are you there?  */ */
/* OBSOLETE   write (eb_desc, "\n", 1); */
/* OBSOLETE  */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Close out all files and local state before this target loses control. */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE eb_close (int quitting) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   /* Due to a bug in Unix, fclose closes not only the stdio stream, */
/* OBSOLETE      but also the file descriptor.  So we don't actually close */
/* OBSOLETE      eb_desc.  */ */
/* OBSOLETE   if (eb_stream) */
/* OBSOLETE     fclose (eb_stream);		/* This also closes eb_desc */ */
/* OBSOLETE   if (eb_desc >= 0) */
/* OBSOLETE     /* close (eb_desc); */ */
/* OBSOLETE  */
/* OBSOLETE     /* Do not try to close eb_desc again, later in the program.  */ */
/* OBSOLETE     eb_stream = NULL; */
/* OBSOLETE   eb_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   if (log_file) */
/* OBSOLETE     { */
/* OBSOLETE       if (ferror (log_file)) */
/* OBSOLETE 	printf ("Error writing log file.\n"); */
/* OBSOLETE       if (fclose (log_file) != 0) */
/* OBSOLETE 	printf ("Error closing log file.\n"); */
/* OBSOLETE     } */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Terminate the open connection to the remote debugger. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  */ */
/* OBSOLETE void */
/* OBSOLETE eb_detach (int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   pop_target ();		/* calls eb_close to do the real work */ */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf ("Ending remote %s debugging\n", target_shortname); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Tell the remote machine to resume.  */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE eb_resume (ptid_t ptid, int step, enum target_signal sig) */
/* OBSOLETE { */
/* OBSOLETE   if (step) */
/* OBSOLETE     { */
/* OBSOLETE       write (eb_desc, "t 1,s\n", 6); */
/* OBSOLETE       /* Wait for the echo.  */ */
/* OBSOLETE       expect ("t 1,s\r"); */
/* OBSOLETE       /* Then comes a line containing the instruction we stepped to.  */ */
/* OBSOLETE       expect ("\n@"); */
/* OBSOLETE       /* Then we get the prompt.  */ */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE       /* Force the next eb_wait to return a trap.  Not doing anything */
/* OBSOLETE          about I/O from the target means that the user has to type */
/* OBSOLETE          "continue" to see any.  This should be fixed.  */ */
/* OBSOLETE       need_artificial_trap = 1; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       if (need_gi) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  need_gi = 0; */
/* OBSOLETE 	  write (eb_desc, "gi\n", 3); */
/* OBSOLETE  */
/* OBSOLETE 	  /* Swallow the echo of "gi".  */ */
/* OBSOLETE 	  expect ("gi\r"); */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  write (eb_desc, "GR\n", 3); */
/* OBSOLETE 	  /* Swallow the echo.  */ */
/* OBSOLETE 	  expect ("GR\r"); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Wait until the remote machine stops, then return, */
/* OBSOLETE    storing status in STATUS just as `wait' would.  */ */
/* OBSOLETE  */
/* OBSOLETE ptid_t */
/* OBSOLETE eb_wait (ptid_t ptid, struct target_waitstatus *status) */
/* OBSOLETE { */
/* OBSOLETE   /* Strings to look for.  '?' means match any single character.   */
/* OBSOLETE      Note that with the algorithm we use, the initial character */
/* OBSOLETE      of the string cannot recur in the string, or we will not */
/* OBSOLETE      find some cases of the string in the input.  */ */
/* OBSOLETE  */
/* OBSOLETE   static char bpt[] = "Invalid interrupt taken - #0x50 - "; */
/* OBSOLETE   /* It would be tempting to look for "\n[__exit + 0x8]\n" */
/* OBSOLETE      but that requires loading symbols with "yc i" and even if */
/* OBSOLETE      we did do that we don't know that the file has symbols.  */ */
/* OBSOLETE   static char exitmsg[] = "\n@????????I    JMPTI     GR121,LR0"; */
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
/* OBSOLETE  */
/* OBSOLETE   int old_timeout = timeout; */
/* OBSOLETE  */
/* OBSOLETE   status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE   status->value.integer = 0; */
/* OBSOLETE  */
/* OBSOLETE   if (need_artificial_trap != 0) */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE       need_artificial_trap--; */
/* OBSOLETE       return 0; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   timeout = 0;			/* Don't time out -- user program is running. */ */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
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
/* OBSOLETE  */
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
/* OBSOLETE  */
/* OBSOLETE       if (!ch_handled) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  char *p; */
/* OBSOLETE  */
/* OBSOLETE 	  /* Print out any characters which have been swallowed.  */ */
/* OBSOLETE 	  for (p = swallowed; p < swallowed_p; ++p) */
/* OBSOLETE 	    putc (*p, stdout); */
/* OBSOLETE 	  swallowed_p = swallowed; */
/* OBSOLETE  */
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
/* OBSOLETE  */
/* OBSOLETE   return 0; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Return the name of register number REGNO */
/* OBSOLETE    in the form input and output by EBMON. */
/* OBSOLETE  */
/* OBSOLETE    Returns a pointer to a static buffer containing the answer.  */ */
/* OBSOLETE static char * */
/* OBSOLETE get_reg_name (int regno) */
/* OBSOLETE { */
/* OBSOLETE   static char buf[80]; */
/* OBSOLETE   if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     sprintf (buf, "GR%03d", regno - GR96_REGNUM + 96); */
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
/* OBSOLETE     sprintf (buf, "SR%03d", regno - VAB_REGNUM); */
/* OBSOLETE   else if (regno == GR1_REGNUM) */
/* OBSOLETE     strcpy (buf, "GR001"); */
/* OBSOLETE   return buf; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read the remote registers into the block REGS.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE eb_fetch_registers (void) */
/* OBSOLETE { */
/* OBSOLETE   int reg_index; */
/* OBSOLETE   int regnum_index; */
/* OBSOLETE   char tempbuf[10]; */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE #if 0 */
/* OBSOLETE   /* This should not be necessary, because one is supposed to read the */
/* OBSOLETE      registers only when the inferior is stopped (at least with */
/* OBSOLETE      ptrace() and why not make it the same for remote?).  */ */
/* OBSOLETE   /* ^A is the "normal character" used to make sure we are talking to EBMON */
/* OBSOLETE      and not to the program being debugged.  */ */
/* OBSOLETE   write (eb_desc, "\001\n"); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   write (eb_desc, "dw gr96,gr127\n", 14); */
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
/* OBSOLETE   for (i = 0; i < 128; i += 32) */
/* OBSOLETE     { */
/* OBSOLETE       /* The PC has a tendency to hang if we get these */
/* OBSOLETE          all in one fell swoop ("dw lr0,lr127").  */ */
/* OBSOLETE       sprintf (tempbuf, "dw lr%d\n", i); */
/* OBSOLETE       write (eb_desc, tempbuf, strlen (tempbuf)); */
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
/* OBSOLETE   write (eb_desc, "dw sr133,sr133\n", 15); */
/* OBSOLETE   expect ("SR133          "); */
/* OBSOLETE   get_hex_regs (1, BP_REGNUM); */
/* OBSOLETE   expect ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   write (eb_desc, "dw sr134,sr134\n", 15); */
/* OBSOLETE   expect ("SR134                   "); */
/* OBSOLETE   get_hex_regs (1, FC_REGNUM); */
/* OBSOLETE   expect ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   write (eb_desc, "dw sr135,sr135\n", 15); */
/* OBSOLETE   expect ("SR135                            "); */
/* OBSOLETE   get_hex_regs (1, CR_REGNUM); */
/* OBSOLETE   expect ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   write (eb_desc, "dw sr131,sr131\n", 15); */
/* OBSOLETE   expect ("SR131                            "); */
/* OBSOLETE   get_hex_regs (1, Q_REGNUM); */
/* OBSOLETE   expect ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   write (eb_desc, "dw sr0,sr14\n", 12); */
/* OBSOLETE   for (reg_index = 0, regnum_index = VAB_REGNUM; */
/* OBSOLETE        regnum_index <= LRU_REGNUM; */
/* OBSOLETE        regnum_index += 4, reg_index += 4) */
/* OBSOLETE     { */
/* OBSOLETE       sprintf (tempbuf, "SR%03d ", reg_index); */
/* OBSOLETE       expect (tempbuf); */
/* OBSOLETE       get_hex_regs (reg_index == 12 ? 3 : 4, regnum_index); */
/* OBSOLETE       expect ("\n"); */
/* OBSOLETE     } */
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
/* OBSOLETE   write (eb_desc, "dw gr1,gr1\n", 11); */
/* OBSOLETE   expect ("GR001 "); */
/* OBSOLETE   get_hex_regs (1, GR1_REGNUM); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Fetch register REGNO, or all registers if REGNO is -1. */
/* OBSOLETE    Returns errno value.  */ */
/* OBSOLETE void */
/* OBSOLETE eb_fetch_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   if (regno == -1) */
/* OBSOLETE     eb_fetch_registers (); */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       char *name = get_reg_name (regno); */
/* OBSOLETE       fprintf (eb_stream, "dw %s,%s\n", name, name); */
/* OBSOLETE       expect (name); */
/* OBSOLETE       expect (" "); */
/* OBSOLETE       get_hex_regs (1, regno); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE   return; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Store the remote registers from the contents of the block REGS.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE eb_store_registers (void) */
/* OBSOLETE { */
/* OBSOLETE   int i, j; */
/* OBSOLETE   fprintf (eb_stream, "s gr1,%x\n", read_register (GR1_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE  */
/* OBSOLETE   for (j = 0; j < 32; j += 16) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (eb_stream, "s gr%d,", j + 96); */
/* OBSOLETE       for (i = 0; i < 15; ++i) */
/* OBSOLETE 	fprintf (eb_stream, "%x,", read_register (GR96_REGNUM + j + i)); */
/* OBSOLETE       fprintf (eb_stream, "%x\n", read_register (GR96_REGNUM + j + 15)); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   for (j = 0; j < 128; j += 16) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf (eb_stream, "s lr%d,", j); */
/* OBSOLETE       for (i = 0; i < 15; ++i) */
/* OBSOLETE 	fprintf (eb_stream, "%x,", read_register (LR0_REGNUM + j + i)); */
/* OBSOLETE       fprintf (eb_stream, "%x\n", read_register (LR0_REGNUM + j + 15)); */
/* OBSOLETE       expect_prompt (); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   fprintf (eb_stream, "s sr133,%x,%x,%x\n", read_register (BP_REGNUM), */
/* OBSOLETE 	   read_register (FC_REGNUM), read_register (CR_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (eb_stream, "s sr131,%x\n", read_register (Q_REGNUM)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE   fprintf (eb_stream, "s sr0,"); */
/* OBSOLETE   for (i = 0; i < 11; ++i) */
/* OBSOLETE     fprintf (eb_stream, "%x,", read_register (VAB_REGNUM + i)); */
/* OBSOLETE   fprintf (eb_stream, "%x\n", read_register (VAB_REGNUM + 11)); */
/* OBSOLETE   expect_prompt (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Store register REGNO, or all if REGNO == 0. */
/* OBSOLETE    Return errno value.  */ */
/* OBSOLETE void */
/* OBSOLETE eb_store_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   if (regno == -1) */
/* OBSOLETE     eb_store_registers (); */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       char *name = get_reg_name (regno); */
/* OBSOLETE       fprintf (eb_stream, "s %s,%x\n", name, read_register (regno)); */
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
/* OBSOLETE eb_prepare_to_store (void) */
/* OBSOLETE { */
/* OBSOLETE   /* Do nothing, since we can store individual regs */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Transfer LEN bytes between GDB address MYADDR and target address */
/* OBSOLETE    MEMADDR.  If WRITE is non-zero, transfer them to the target, */
/* OBSOLETE    otherwise transfer them from the target.  TARGET is unused. */
/* OBSOLETE  */
/* OBSOLETE    Returns the number of bytes transferred. */ */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE eb_xfer_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len, int write, */
/* OBSOLETE 			 struct mem_attrib *attrib ATTRIBUTE_UNUSED, */
/* OBSOLETE 			 struct target_ops *target ATTRIBUTE_UNUSED) */
/* OBSOLETE { */
/* OBSOLETE   if (write) */
/* OBSOLETE     return eb_write_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE   else */
/* OBSOLETE     return eb_read_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE eb_files_info (void) */
/* OBSOLETE { */
/* OBSOLETE   printf ("\tAttached to %s at %d baud and running program %s.\n", */
/* OBSOLETE 	  dev_name, baudrate, prog_name); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Copy LEN bytes of data from debugger memory at MYADDR */
/* OBSOLETE    to inferior's memory at MEMADDR.  Returns length moved.  */ */
/* OBSOLETE int */
/* OBSOLETE eb_write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   for (i = 0; i < len; i++) */
/* OBSOLETE     { */
/* OBSOLETE       if ((i % 16) == 0) */
/* OBSOLETE 	fprintf (eb_stream, "sb %x,", memaddr + i); */
/* OBSOLETE       if ((i % 16) == 15 || i == len - 1) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  fprintf (eb_stream, "%x\n", ((unsigned char *) myaddr)[i]); */
/* OBSOLETE 	  expect_prompt (); */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	fprintf (eb_stream, "%x,", ((unsigned char *) myaddr)[i]); */
/* OBSOLETE     } */
/* OBSOLETE   return len; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Read LEN bytes from inferior memory at MEMADDR.  Put the result */
/* OBSOLETE    at debugger address MYADDR.  Returns length moved.  */ */
/* OBSOLETE int */
/* OBSOLETE eb_read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
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
/* OBSOLETE      eb_read_bytes (CORE_ADDR_MAX - 4, foo, 4) */
/* OBSOLETE      works--it never adds len to memaddr and gets 0.  */ */
/* OBSOLETE   /* However, something like */
/* OBSOLETE      eb_read_bytes (CORE_ADDR_MAX - 3, foo, 4) */
/* OBSOLETE      doesn't need to work.  Detect it and give up if there's an attempt */
/* OBSOLETE      to do that.  */ */
/* OBSOLETE   if (((memaddr - 1) + len) < memaddr) */
/* OBSOLETE     { */
/* OBSOLETE       errno = EIO; */
/* OBSOLETE       return 0; */
/* OBSOLETE     } */
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
/* OBSOLETE       fprintf (eb_stream, "db %x,%x\n", startaddr, */
/* OBSOLETE 	       (startaddr - 1) + len_this_pass); */
/* OBSOLETE       expect ("\n"); */
/* OBSOLETE  */
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
/* OBSOLETE   return len; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE eb_kill (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   return;			/* Ignore attempts to kill target system */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Clean up when a program exits. */
/* OBSOLETE  */
/* OBSOLETE    The program actually lives on in the remote processor's RAM, and may be */
/* OBSOLETE    run again without a download.  Don't leave it full of breakpoint */
/* OBSOLETE    instructions.  */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE eb_mourn_inferior (void) */
/* OBSOLETE { */
/* OBSOLETE   remove_breakpoints (); */
/* OBSOLETE   unpush_target (&eb_ops); */
/* OBSOLETE   generic_mourn_inferior ();	/* Do all the proper things now */ */
/* OBSOLETE } */
/* OBSOLETE /* Define the target subroutine names */ */
/* OBSOLETE  */
/* OBSOLETE struct target_ops eb_ops; */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE init_eb_ops (void) */
/* OBSOLETE { */
/* OBSOLETE   eb_ops.to_shortname = "amd-eb"; */
/* OBSOLETE   eb_ops.to_longname = "Remote serial AMD EBMON target"; */
/* OBSOLETE   eb_ops.to_doc = "Use a remote computer running EBMON connected by a serial line.\n\ */
/* OBSOLETE Arguments are the name of the device for the serial line,\n\ */
/* OBSOLETE the speed to connect at in bits per second, and the filename of the\n\ */
/* OBSOLETE executable as it exists on the remote computer.  For example,\n\ */
/* OBSOLETE target amd-eb /dev/ttya 9600 demo", */
/* OBSOLETE     eb_ops.to_open = eb_open; */
/* OBSOLETE   eb_ops.to_close = eb_close; */
/* OBSOLETE   eb_ops.to_attach = 0; */
/* OBSOLETE   eb_ops.to_post_attach = NULL; */
/* OBSOLETE   eb_ops.to_require_attach = NULL; */
/* OBSOLETE   eb_ops.to_detach = eb_detach; */
/* OBSOLETE   eb_ops.to_require_detach = NULL; */
/* OBSOLETE   eb_ops.to_resume = eb_resume; */
/* OBSOLETE   eb_ops.to_wait = eb_wait; */
/* OBSOLETE   eb_ops.to_post_wait = NULL; */
/* OBSOLETE   eb_ops.to_fetch_registers = eb_fetch_register; */
/* OBSOLETE   eb_ops.to_store_registers = eb_store_register; */
/* OBSOLETE   eb_ops.to_prepare_to_store = eb_prepare_to_store; */
/* OBSOLETE   eb_ops.to_xfer_memory = eb_xfer_inferior_memory; */
/* OBSOLETE   eb_ops.to_files_info = eb_files_info; */
/* OBSOLETE   eb_ops.to_insert_breakpoint = 0; */
/* OBSOLETE   eb_ops.to_remove_breakpoint = 0;	/* Breakpoints */ */
/* OBSOLETE   eb_ops.to_terminal_init = 0; */
/* OBSOLETE   eb_ops.to_terminal_inferior = 0; */
/* OBSOLETE   eb_ops.to_terminal_ours_for_output = 0; */
/* OBSOLETE   eb_ops.to_terminal_ours = 0; */
/* OBSOLETE   eb_ops.to_terminal_info = 0;	/* Terminal handling */ */
/* OBSOLETE   eb_ops.to_kill = eb_kill; */
/* OBSOLETE   eb_ops.to_load = generic_load;	/* load */ */
/* OBSOLETE   eb_ops.to_lookup_symbol = 0;	/* lookup_symbol */ */
/* OBSOLETE   eb_ops.to_create_inferior = eb_create_inferior; */
/* OBSOLETE   eb_ops.to_post_startup_inferior = NULL; */
/* OBSOLETE   eb_ops.to_acknowledge_created_inferior = NULL; */
/* OBSOLETE   eb_ops.to_clone_and_follow_inferior = NULL; */
/* OBSOLETE   eb_ops.to_post_follow_inferior_by_clone = NULL; */
/* OBSOLETE   eb_ops.to_insert_fork_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_remove_fork_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_insert_vfork_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_remove_vfork_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_has_forked = NULL; */
/* OBSOLETE   eb_ops.to_has_vforked = NULL; */
/* OBSOLETE   eb_ops.to_can_follow_vfork_prior_to_exec = NULL; */
/* OBSOLETE   eb_ops.to_post_follow_vfork = NULL; */
/* OBSOLETE   eb_ops.to_insert_exec_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_remove_exec_catchpoint = NULL; */
/* OBSOLETE   eb_ops.to_has_execd = NULL; */
/* OBSOLETE   eb_ops.to_reported_exec_events_per_exec_call = NULL; */
/* OBSOLETE   eb_ops.to_has_exited = NULL; */
/* OBSOLETE   eb_ops.to_mourn_inferior = eb_mourn_inferior; */
/* OBSOLETE   eb_ops.to_can_run = 0;	/* can_run */ */
/* OBSOLETE   eb_ops.to_notice_signals = 0;	/* notice_signals */ */
/* OBSOLETE   eb_ops.to_thread_alive = 0;	/* thread-alive */ */
/* OBSOLETE   eb_ops.to_stop = 0;		/* to_stop */ */
/* OBSOLETE   eb_ops.to_pid_to_exec_file = NULL; */
/* OBSOLETE   eb_ops.to_stratum = process_stratum; */
/* OBSOLETE   eb_ops.DONT_USE = 0;		/* next */ */
/* OBSOLETE   eb_ops.to_has_all_memory = 1; */
/* OBSOLETE   eb_ops.to_has_memory = 1; */
/* OBSOLETE   eb_ops.to_has_stack = 1; */
/* OBSOLETE   eb_ops.to_has_registers = 1; */
/* OBSOLETE   eb_ops.to_has_execution = 1;	/* all mem, mem, stack, regs, exec */ */
/* OBSOLETE   eb_ops.to_sections = 0;	/* sections */ */
/* OBSOLETE   eb_ops.to_sections_end = 0;	/* sections end */ */
/* OBSOLETE   eb_ops.to_magic = OPS_MAGIC;	/* Always the last thing */ */
/* OBSOLETE }; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE _initialize_remote_eb (void) */
/* OBSOLETE { */
/* OBSOLETE   init_eb_ops (); */
/* OBSOLETE   add_target (&eb_ops); */
/* OBSOLETE } */
