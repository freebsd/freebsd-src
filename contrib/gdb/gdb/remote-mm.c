/* OBSOLETE /* Remote debugging interface for Am290*0 running MiniMON monitor, for GDB. */
/* OBSOLETE    Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, */
/* OBSOLETE    2001 Free Software Foundation, Inc. */
/* OBSOLETE    Originally written by Daniel Mann at AMD. */
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
/* OBSOLETE /* This is like remote.c but ecpects MiniMON to be running on the Am29000  */
/* OBSOLETE    target hardware. */
/* OBSOLETE    - David Wood (wood@lab.ultra.nyu.edu) at New York University adapted this */
/* OBSOLETE    file to gdb 3.95.  I was unable to get this working on sun3os4 */
/* OBSOLETE    with termio, only with sgtty.  Because we are only attempting to */
/* OBSOLETE    use this module to debug our kernel, which is already loaded when */
/* OBSOLETE    gdb is started up, I did not code up the file downloading facilities.   */
/* OBSOLETE    As a result this module has only the stubs to download files.  */
/* OBSOLETE    You should get tagged at compile time if you need to make any  */
/* OBSOLETE    changes/additions.  */ */
/* OBSOLETE  */
/* OBSOLETE #include "defs.h" */
/* OBSOLETE #include "inferior.h" */
/* OBSOLETE #include "value.h" */
/* OBSOLETE #include <ctype.h> */
/* OBSOLETE #include <fcntl.h> */
/* OBSOLETE #include <signal.h> */
/* OBSOLETE #include <errno.h> */
/* OBSOLETE #include "gdb_string.h" */
/* OBSOLETE #include "terminal.h" */
/* OBSOLETE #include "minimon.h" */
/* OBSOLETE #include "target.h" */
/* OBSOLETE #include "regcache.h" */
/* OBSOLETE  */
/* OBSOLETE /* Offset of member MEMBER in a struct of type TYPE.  */ */
/* OBSOLETE #define offsetof(TYPE, MEMBER) ((int) &((TYPE *)0)->MEMBER) */
/* OBSOLETE  */
/* OBSOLETE #define DRAIN_INPUT()	(msg_recv_serial((union msg_t*)0)) */
/* OBSOLETE  */
/* OBSOLETE extern int stop_soon_quietly;	/* for wait_for_inferior */ */
/* OBSOLETE  */
/* OBSOLETE static void mm_resume (ptid_t ptid, int step, enum target_signal sig) */
/* OBSOLETE static void mm_fetch_registers (); */
/* OBSOLETE static int fetch_register (); */
/* OBSOLETE static void mm_store_registers (); */
/* OBSOLETE static int store_register (); */
/* OBSOLETE static int regnum_to_srnum (); */
/* OBSOLETE static void mm_close (); */
/* OBSOLETE static char *msg_str (); */
/* OBSOLETE static char *error_msg_str (); */
/* OBSOLETE static int expect_msg (); */
/* OBSOLETE static void init_target_mm (); */
/* OBSOLETE static int mm_memory_space (); */
/* OBSOLETE  */
/* OBSOLETE #define FREEZE_MODE     (read_register(CPS_REGNUM) && 0x400) */
/* OBSOLETE #define USE_SHADOW_PC	((processor_type == a29k_freeze_mode) && FREEZE_MODE) */
/* OBSOLETE  */
/* OBSOLETE /* FIXME: Replace with `set remotedebug'.  */ */
/* OBSOLETE #define LLOG_FILE "minimon.log" */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE FILE *log_file; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /*   */
/* OBSOLETE  * Size of message buffers.  I couldn't get memory reads to work when */
/* OBSOLETE  * the byte_count was larger than 512 (it may be a baud rate problem). */
/* OBSOLETE  */ */
/* OBSOLETE #define BUFER_SIZE  512 */
/* OBSOLETE /*  */
/* OBSOLETE  * Size of data area in message buffer on the TARGET (remote system). */
/* OBSOLETE  */ */
/* OBSOLETE #define MAXDATA_T  (target_config.max_msg_size - \ */
/* OBSOLETE 			offsetof(struct write_r_msg_t,data[0])) */
/* OBSOLETE /*                */
/* OBSOLETE  * Size of data area in message buffer on the HOST (gdb).  */
/* OBSOLETE  */ */
/* OBSOLETE #define MAXDATA_H  (BUFER_SIZE - offsetof(struct write_r_msg_t,data[0])) */
/* OBSOLETE /*  */
/* OBSOLETE  * Defined as the minimum size of data areas of the two message buffers  */
/* OBSOLETE  */ */
/* OBSOLETE #define MAXDATA	   (MAXDATA_H < MAXDATA_T ? MAXDATA_H : MAXDATA_T) */
/* OBSOLETE  */
/* OBSOLETE static char out_buf[BUFER_SIZE]; */
/* OBSOLETE static char in_buf[BUFER_SIZE]; */
/* OBSOLETE  */
/* OBSOLETE int msg_recv_serial (); */
/* OBSOLETE int msg_send_serial (); */
/* OBSOLETE  */
/* OBSOLETE #define MAX_RETRIES 5000 */
/* OBSOLETE extern struct target_ops mm_ops;	/* Forward declaration */ */
/* OBSOLETE struct config_msg_t target_config;	/* HIF needs this */ */
/* OBSOLETE union msg_t *out_msg_buf = (union msg_t *) out_buf; */
/* OBSOLETE union msg_t *in_msg_buf = (union msg_t *) in_buf; */
/* OBSOLETE  */
/* OBSOLETE static int timeout = 5; */
/* OBSOLETE  */
/* OBSOLETE /* Descriptor for I/O to remote machine.  Initialize it to -1 so that */
/* OBSOLETE    mm_open knows that we don't have a file open when the program */
/* OBSOLETE    starts.  */ */
/* OBSOLETE int mm_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE /* stream which is fdopen'd from mm_desc.  Only valid when */
/* OBSOLETE    mm_desc != -1.  */ */
/* OBSOLETE FILE *mm_stream; */
/* OBSOLETE  */
/* OBSOLETE /* Called when SIGALRM signal sent due to alarm() timeout.  */ */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE  */
/* OBSOLETE volatile int n_alarms; */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE mm_timer (void) */
/* OBSOLETE { */
/* OBSOLETE #if 0 */
/* OBSOLETE   if (kiodebug) */
/* OBSOLETE     printf ("mm_timer called\n"); */
/* OBSOLETE #endif */
/* OBSOLETE   n_alarms++; */
/* OBSOLETE } */
/* OBSOLETE #endif	/* HAVE_TERMIO */ */
/* OBSOLETE  */
/* OBSOLETE /* malloc'd name of the program on the remote system.  */ */
/* OBSOLETE static char *prog_name = NULL; */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /* Number of SIGTRAPs we need to simulate.  That is, the next */
/* OBSOLETE    NEED_ARTIFICIAL_TRAP calls to mm_wait should just return */
/* OBSOLETE    SIGTRAP without actually waiting for anything.  */ */
/* OBSOLETE  */
/* OBSOLETE /**************************************************** REMOTE_CREATE_INFERIOR */ */
/* OBSOLETE /* This is called not only when we first attach, but also when the */
/* OBSOLETE    user types "run" after having attached.  */ */
/* OBSOLETE static void */
/* OBSOLETE mm_create_inferior (char *execfile, char *args, char **env) */
/* OBSOLETE { */
/* OBSOLETE #define MAX_TOKENS 25 */
/* OBSOLETE #define BUFFER_SIZE 256 */
/* OBSOLETE   int token_count; */
/* OBSOLETE   int result; */
/* OBSOLETE   char *token[MAX_TOKENS]; */
/* OBSOLETE   char cmd_line[BUFFER_SIZE]; */
/* OBSOLETE  */
/* OBSOLETE   if (args && *args) */
/* OBSOLETE     error ("Can't pass arguments to remote mm process (yet)."); */
/* OBSOLETE  */
/* OBSOLETE   if (execfile == 0 /* || exec_bfd == 0 */ ) */
/* OBSOLETE     error ("No executable file specified"); */
/* OBSOLETE  */
/* OBSOLETE   if (!mm_stream) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Minimon not open yet.\n"); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* On ultra3 (NYU) we assume the kernel is already running so there is */
/* OBSOLETE      no file to download. */
/* OBSOLETE      FIXME: Fixed required here -> load your program, possibly with mm_load(). */
/* OBSOLETE    */ */
/* OBSOLETE   printf_filtered ("\n\ */
/* OBSOLETE Assuming you are at NYU debuging a kernel, i.e., no need to download.\n\n"); */
/* OBSOLETE  */
/* OBSOLETE   /* We will get a task spawn event immediately.  */ */
/* OBSOLETE   init_wait_for_inferior (); */
/* OBSOLETE   clear_proceed_status (); */
/* OBSOLETE   stop_soon_quietly = 1; */
/* OBSOLETE   proceed (-1, TARGET_SIGNAL_DEFAULT, 0); */
/* OBSOLETE   normal_stop (); */
/* OBSOLETE } */
/* OBSOLETE /**************************************************** REMOTE_MOURN_INFERIOR */ */
/* OBSOLETE static void */
/* OBSOLETE mm_mourn (void) */
/* OBSOLETE { */
/* OBSOLETE   pop_target ();		/* Pop back to no-child state */ */
/* OBSOLETE   generic_mourn_inferior (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /********************************************************************** damn_b */
/* OBSOLETE */ */
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
/* OBSOLETE /***************************************************************** REMOTE_OPEN */
/* OBSOLETE ** Open a connection to remote minimon. */
/* OBSOLETE    NAME is the filename used for communication, then a space, */
/* OBSOLETE    then the baud rate. */
/* OBSOLETE    'target adapt /dev/ttya 9600 [prognam]' for example. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE static char *dev_name; */
/* OBSOLETE int baudrate = 9600; */
/* OBSOLETE static void */
/* OBSOLETE mm_open (char *name, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   TERMINAL sg; */
/* OBSOLETE   unsigned int prl; */
/* OBSOLETE   char *p; */
/* OBSOLETE  */
/* OBSOLETE   /* Find the first whitespace character, it separates dev_name from */
/* OBSOLETE      prog_name.  */ */
/* OBSOLETE   for (p = name; */
/* OBSOLETE        p && *p && !isspace (*p); p++) */
/* OBSOLETE     ; */
/* OBSOLETE   if (p == 0 || *p == '\0') */
/* OBSOLETE   erroid: */
/* OBSOLETE     error ("Usage : <command> <serial-device> <baud-rate> [progname]"); */
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
/* OBSOLETE  */
/* OBSOLETE   if (mm_desc >= 0) */
/* OBSOLETE     close (mm_desc); */
/* OBSOLETE  */
/* OBSOLETE   mm_desc = open (dev_name, O_RDWR); */
/* OBSOLETE   if (mm_desc < 0) */
/* OBSOLETE     perror_with_name (dev_name); */
/* OBSOLETE   ioctl (mm_desc, TIOCGETP, &sg); */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   sg.c_cc[VMIN] = 0;		/* read with timeout.  */ */
/* OBSOLETE   sg.c_cc[VTIME] = timeout * 10; */
/* OBSOLETE   sg.c_lflag &= ~(ICANON | ECHO); */
/* OBSOLETE   sg.c_cflag = (sg.c_cflag & ~CBAUD) | damn_b (baudrate); */
/* OBSOLETE #else */
/* OBSOLETE   sg.sg_ispeed = damn_b (baudrate); */
/* OBSOLETE   sg.sg_ospeed = damn_b (baudrate); */
/* OBSOLETE   sg.sg_flags |= RAW; */
/* OBSOLETE   sg.sg_flags |= ANYP; */
/* OBSOLETE   sg.sg_flags &= ~ECHO; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE   ioctl (mm_desc, TIOCSETP, &sg); */
/* OBSOLETE   mm_stream = fdopen (mm_desc, "r+"); */
/* OBSOLETE  */
/* OBSOLETE   push_target (&mm_ops); */
/* OBSOLETE  */
/* OBSOLETE #ifndef HAVE_TERMIO */
/* OBSOLETE #ifndef NO_SIGINTERRUPT */
/* OBSOLETE   /* Cause SIGALRM's to make reads fail with EINTR instead of resuming */
/* OBSOLETE      the read.  */ */
/* OBSOLETE   if (siginterrupt (SIGALRM, 1) != 0) */
/* OBSOLETE     perror ("mm_open: error in siginterrupt"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Set up read timeout timer.  */ */
/* OBSOLETE   if ((void (*)) signal (SIGALRM, mm_timer) == (void (*)) -1) */
/* OBSOLETE     perror ("mm_open: error in signal"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   log_file = fopen (LOG_FILE, "w"); */
/* OBSOLETE   if (log_file == NULL) */
/* OBSOLETE     perror_with_name (LOG_FILE); */
/* OBSOLETE #endif */
/* OBSOLETE   /* */
/* OBSOLETE      ** Initialize target configuration structure (global) */
/* OBSOLETE    */ */
/* OBSOLETE   DRAIN_INPUT (); */
/* OBSOLETE   out_msg_buf->config_req_msg.code = CONFIG_REQ; */
/* OBSOLETE   out_msg_buf->config_req_msg.length = 4 * 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf);	/* send config request message */ */
/* OBSOLETE  */
/* OBSOLETE   expect_msg (CONFIG, in_msg_buf, 1); */
/* OBSOLETE  */
/* OBSOLETE   a29k_get_processor_type (); */
/* OBSOLETE  */
/* OBSOLETE   /* Print out some stuff, letting the user now what's going on */ */
/* OBSOLETE   printf_filtered ("Connected to MiniMon via %s.\n", dev_name); */
/* OBSOLETE   /* FIXME: can this restriction be removed? */ */
/* OBSOLETE   printf_filtered ("Remote debugging using virtual addresses works only\n"); */
/* OBSOLETE   printf_filtered ("\twhen virtual addresses map 1:1 to physical addresses.\n") */
/* OBSOLETE     ; */
/* OBSOLETE   if (processor_type != a29k_freeze_mode) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf_filtered (gdb_stderr, */
/* OBSOLETE 			"Freeze-mode debugging not available, and can only be done on an A29050.\n"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   target_config.code = CONFIG; */
/* OBSOLETE   target_config.length = 0; */
/* OBSOLETE   target_config.processor_id = in_msg_buf->config_msg.processor_id; */
/* OBSOLETE   target_config.version = in_msg_buf->config_msg.version; */
/* OBSOLETE   target_config.I_mem_start = in_msg_buf->config_msg.I_mem_start; */
/* OBSOLETE   target_config.I_mem_size = in_msg_buf->config_msg.I_mem_size; */
/* OBSOLETE   target_config.D_mem_start = in_msg_buf->config_msg.D_mem_start; */
/* OBSOLETE   target_config.D_mem_size = in_msg_buf->config_msg.D_mem_size; */
/* OBSOLETE   target_config.ROM_start = in_msg_buf->config_msg.ROM_start; */
/* OBSOLETE   target_config.ROM_size = in_msg_buf->config_msg.ROM_size; */
/* OBSOLETE   target_config.max_msg_size = in_msg_buf->config_msg.max_msg_size; */
/* OBSOLETE   target_config.max_bkpts = in_msg_buf->config_msg.max_bkpts; */
/* OBSOLETE   target_config.coprocessor = in_msg_buf->config_msg.coprocessor; */
/* OBSOLETE   target_config.reserved = in_msg_buf->config_msg.reserved; */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Connected to MiniMON :\n"); */
/* OBSOLETE       printf ("    Debugcore version            %d.%d\n", */
/* OBSOLETE 	      0x0f & (target_config.version >> 4), */
/* OBSOLETE 	      0x0f & (target_config.version)); */
/* OBSOLETE       printf ("    Configuration version        %d.%d\n", */
/* OBSOLETE 	      0x0f & (target_config.version >> 12), */
/* OBSOLETE 	      0x0f & (target_config.version >> 8)); */
/* OBSOLETE       printf ("    Message system version       %d.%d\n", */
/* OBSOLETE 	      0x0f & (target_config.version >> 20), */
/* OBSOLETE 	      0x0f & (target_config.version >> 16)); */
/* OBSOLETE       printf ("    Communication driver version %d.%d\n", */
/* OBSOLETE 	      0x0f & (target_config.version >> 28), */
/* OBSOLETE 	      0x0f & (target_config.version >> 24)); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Leave the target running...  */
/* OBSOLETE    * The above message stopped the target in the dbg core (MiniMon),   */
/* OBSOLETE    * so restart the target out of MiniMon,  */
/* OBSOLETE    */ */
/* OBSOLETE   out_msg_buf->go_msg.code = GO; */
/* OBSOLETE   out_msg_buf->go_msg.length = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   /* No message to expect after a GO */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /**************************************************************** REMOTE_CLOSE */
/* OBSOLETE ** Close the open connection to the minimon debugger. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  */ */
/* OBSOLETE static void */
/* OBSOLETE mm_close (			/*FIXME: how is quitting used */ */
/* OBSOLETE 	   int quitting) */
/* OBSOLETE { */
/* OBSOLETE   if (mm_desc < 0) */
/* OBSOLETE     error ("Can't close remote connection: not debugging remotely."); */
/* OBSOLETE  */
/* OBSOLETE   /* We should never get here if there isn't something valid in */
/* OBSOLETE      mm_desc and mm_stream.   */
/* OBSOLETE  */
/* OBSOLETE      Due to a bug in Unix, fclose closes not only the stdio stream, */
/* OBSOLETE      but also the file descriptor.  So we don't actually close */
/* OBSOLETE      mm_desc.  */ */
/* OBSOLETE   DRAIN_INPUT (); */
/* OBSOLETE   fclose (mm_stream); */
/* OBSOLETE   /* close (mm_desc); */ */
/* OBSOLETE  */
/* OBSOLETE   /* Do not try to close mm_desc again, later in the program.  */ */
/* OBSOLETE   mm_stream = NULL; */
/* OBSOLETE   mm_desc = -1; */
/* OBSOLETE  */
/* OBSOLETE #if defined (LOG_FILE) */
/* OBSOLETE   if (ferror (log_file)) */
/* OBSOLETE     printf ("Error writing log file.\n"); */
/* OBSOLETE   if (fclose (log_file) != 0) */
/* OBSOLETE     printf ("Error closing log file.\n"); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   printf ("Ending remote debugging\n"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /************************************************************* REMOTE_ATACH */ */
/* OBSOLETE /* Attach to a program that is already loaded and running  */
/* OBSOLETE  * Upon exiting the process's execution is stopped. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE mm_attach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   if (!mm_stream) */
/* OBSOLETE     error ("MiniMon not opened yet, use the 'target minimon' command.\n"); */
/* OBSOLETE  */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf ("Attaching to remote program %s...\n", prog_name); */
/* OBSOLETE  */
/* OBSOLETE   /* Make sure the target is currently running, it is supposed to be. */ */
/* OBSOLETE   /* FIXME: is it ok to send MiniMon a BREAK if it is already stopped in  */
/* OBSOLETE    *  the dbg core.  If so, we don't need to send this GO. */
/* OBSOLETE    */ */
/* OBSOLETE   out_msg_buf->go_msg.code = GO; */
/* OBSOLETE   out_msg_buf->go_msg.length = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   sleep (2);			/* At the worst it will stop, receive a message, continue */ */
/* OBSOLETE  */
/* OBSOLETE   /* Send the mm a break. */ */
/* OBSOLETE   out_msg_buf->break_msg.code = BREAK; */
/* OBSOLETE   out_msg_buf->break_msg.length = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE } */
/* OBSOLETE /********************************************************** REMOTE_DETACH */ */
/* OBSOLETE /* Terminate the open connection to the remote debugger. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  Leave remote process running (with no breakpoints set). */ */
/* OBSOLETE static void */
/* OBSOLETE mm_detach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   remove_breakpoints ();	/* Just in case there were any left in */ */
/* OBSOLETE   out_msg_buf->go_msg.code = GO; */
/* OBSOLETE   out_msg_buf->go_msg.length = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   pop_target ();		/* calls mm_close to do the real work */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /*************************************************************** REMOTE_RESUME */
/* OBSOLETE ** Tell the remote machine to resume.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE mm_resume (ptid_t ptid, int step, enum target_signal sig) */
/* OBSOLETE { */
/* OBSOLETE   if (sig != TARGET_SIGNAL_0) */
/* OBSOLETE     warning ("Can't send signals to a remote MiniMon system."); */
/* OBSOLETE  */
/* OBSOLETE   if (step) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->step_msg.code = STEP; */
/* OBSOLETE       out_msg_buf->step_msg.length = 1 * 4; */
/* OBSOLETE       out_msg_buf->step_msg.count = 1;	/* step 1 instruction */ */
/* OBSOLETE       msg_send_serial (out_msg_buf); */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->go_msg.code = GO; */
/* OBSOLETE       out_msg_buf->go_msg.length = 0; */
/* OBSOLETE       msg_send_serial (out_msg_buf); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /***************************************************************** REMOTE_WAIT */
/* OBSOLETE ** Wait until the remote machine stops, then return, */
/* OBSOLETE    storing status in STATUS just as `wait' would.  */ */
/* OBSOLETE  */
/* OBSOLETE static ptid_t */
/* OBSOLETE mm_wait (ptid_t ptid, struct target_waitstatus *status) */
/* OBSOLETE { */
/* OBSOLETE   int i, result; */
/* OBSOLETE   int old_timeout = timeout; */
/* OBSOLETE   int old_immediate_quit = immediate_quit; */
/* OBSOLETE  */
/* OBSOLETE   status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE   status->value.integer = 0; */
/* OBSOLETE  */
/* OBSOLETE /* wait for message to arrive. It should be: */
/* OBSOLETE    - A HIF service request. */
/* OBSOLETE    - A HIF exit service request. */
/* OBSOLETE    - A CHANNEL0_ACK. */
/* OBSOLETE    - A CHANNEL1 request. */
/* OBSOLETE    - a debugcore HALT message. */
/* OBSOLETE    HIF services must be responded too, and while-looping continued. */
/* OBSOLETE    If the target stops executing, mm_wait() should return. */
/* OBSOLETE  */ */
/* OBSOLETE   timeout = 0;			/* Wait indefinetly for a message */ */
/* OBSOLETE   immediate_quit = 1;		/* Helps ability to QUIT */ */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
/* OBSOLETE       while (msg_recv_serial (in_msg_buf)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  QUIT;			/* Let user quit if they want */ */
/* OBSOLETE 	} */
/* OBSOLETE       switch (in_msg_buf->halt_msg.code) */
/* OBSOLETE 	{ */
/* OBSOLETE 	case HIF_CALL: */
/* OBSOLETE 	  i = in_msg_buf->hif_call_rtn_msg.service_number; */
/* OBSOLETE 	  result = service_HIF (in_msg_buf); */
/* OBSOLETE 	  if (i == 1)		/* EXIT */ */
/* OBSOLETE 	    goto exit; */
/* OBSOLETE 	  if (result) */
/* OBSOLETE 	    printf ("Warning: failure during HIF service %d\n", i); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case CHANNEL0_ACK: */
/* OBSOLETE 	  service_HIF (in_msg_buf); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case CHANNEL1: */
/* OBSOLETE 	  i = in_msg_buf->channel1_msg.length; */
/* OBSOLETE 	  in_msg_buf->channel1_msg.data[i] = '\0'; */
/* OBSOLETE 	  printf ("%s", in_msg_buf->channel1_msg.data); */
/* OBSOLETE 	  gdb_flush (gdb_stdout); */
/* OBSOLETE 	  /* Send CHANNEL1_ACK message */ */
/* OBSOLETE 	  out_msg_buf->channel1_ack_msg.code = CHANNEL1_ACK; */
/* OBSOLETE 	  out_msg_buf->channel1_ack_msg.length = 0; */
/* OBSOLETE 	  result = msg_send_serial (out_msg_buf); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case HALT: */
/* OBSOLETE 	  goto halted; */
/* OBSOLETE 	default: */
/* OBSOLETE 	  goto halted; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE halted: */
/* OBSOLETE   /* FIXME, these printfs should not be here.  This is a source level  */
/* OBSOLETE      debugger, guys!  */ */
/* OBSOLETE   if (in_msg_buf->halt_msg.trap_number == 0) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d (break point)\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 1) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_BUS; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 3 */
/* OBSOLETE 	   || in_msg_buf->halt_msg.trap_number == 4) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_FPE; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 5) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number >= 6 */
/* OBSOLETE 	   && in_msg_buf->halt_msg.trap_number <= 11) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_SEGV; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 12 */
/* OBSOLETE 	   || in_msg_buf->halt_msg.trap_number == 13) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 14) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_ALRM; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 15) */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number >= 16 */
/* OBSOLETE 	   && in_msg_buf->halt_msg.trap_number <= 21) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_INT; */
/* OBSOLETE     } */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 22) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Am290*0 received vector number %d\n", */
/* OBSOLETE 	      in_msg_buf->halt_msg.trap_number); */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE     }				/* BREAK message was sent */ */
/* OBSOLETE   else if (in_msg_buf->halt_msg.trap_number == 75) */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE   exit: */
/* OBSOLETE     { */
/* OBSOLETE       status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE       status->value.integer = 0; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   timeout = old_timeout;	/* Restore original timeout value */ */
/* OBSOLETE   immediate_quit = old_immediate_quit; */
/* OBSOLETE   return inferior_ptid; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /******************************************************* REMOTE_FETCH_REGISTERS */
/* OBSOLETE  * Read a remote register 'regno'.  */
/* OBSOLETE  * If regno==-1 then read all the registers. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE mm_fetch_registers (int regno) */
/* OBSOLETE { */
/* OBSOLETE   INT32 *data_p; */
/* OBSOLETE  */
/* OBSOLETE   if (regno >= 0) */
/* OBSOLETE     { */
/* OBSOLETE       fetch_register (regno); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Gr1/rsp */ */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * 1; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 1; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE   supply_register (GR1_REGNUM, data_p); */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM)	/* Read gr64-127 */ */
/* OBSOLETE /* Global Registers gr64-gr95 */ */
/* OBSOLETE   out_msg_buf->read_req_msg.code = READ_REQ; */
/* OBSOLETE   out_msg_buf->read_req_msg.length = 4 * 3; */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * 32; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 64; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = GR64_REGNUM; regno < GR64_REGNUM + 32; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (regno, data_p++); */
/* OBSOLETE     } */
/* OBSOLETE #endif /*  GR64_REGNUM */ */
/* OBSOLETE  */
/* OBSOLETE /* Global Registers gr96-gr127 */ */
/* OBSOLETE   out_msg_buf->read_req_msg.code = READ_REQ; */
/* OBSOLETE   out_msg_buf->read_req_msg.length = 4 * 3; */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * 32; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 96; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = GR96_REGNUM; regno < GR96_REGNUM + 32; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (regno, data_p++); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Local Registers */ */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * (128); */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = LOCAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = LR0_REGNUM; regno < LR0_REGNUM + 128; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (regno, data_p++); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Protected Special Registers */ */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * 15; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = SPECIAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 0; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = 0; regno <= 14; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (SR_REGNUM (regno), data_p++); */
/* OBSOLETE     } */
/* OBSOLETE   if (USE_SHADOW_PC) */
/* OBSOLETE     {				/* Let regno_to_srnum() handle the register number */ */
/* OBSOLETE       fetch_register (NPC_REGNUM); */
/* OBSOLETE       fetch_register (PC_REGNUM); */
/* OBSOLETE       fetch_register (PC2_REGNUM); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Unprotected Special Registers */ */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4 * 8; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = SPECIAL_REG; */
/* OBSOLETE   out_msg_buf->read_req_msg.address = 128; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (READ_ACK, in_msg_buf, 1); */
/* OBSOLETE   data_p = &(in_msg_buf->read_r_ack_msg.data[0]); */
/* OBSOLETE  */
/* OBSOLETE   for (regno = 128; regno <= 135; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (SR_REGNUM (regno), data_p++); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* There doesn't seem to be any way to get these.  */ */
/* OBSOLETE   { */
/* OBSOLETE     int val = -1; */
/* OBSOLETE     supply_register (FPE_REGNUM, &val); */
/* OBSOLETE     supply_register (INTE_REGNUM, &val); */
/* OBSOLETE     supply_register (FPS_REGNUM, &val); */
/* OBSOLETE     supply_register (EXO_REGNUM, &val); */
/* OBSOLETE   } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /****************************************************** REMOTE_STORE_REGISTERS */
/* OBSOLETE  * Store register regno into the target.   */
/* OBSOLETE  * If regno==-1 then store all the registers. */
/* OBSOLETE  * Result is 0 for success, -1 for failure. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE mm_store_registers (int regno) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE  */
/* OBSOLETE   if (regno >= 0) */
/* OBSOLETE     { */
/* OBSOLETE       store_register (regno); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   result = 0; */
/* OBSOLETE  */
/* OBSOLETE   out_msg_buf->write_r_msg.code = WRITE_REQ; */
/* OBSOLETE  */
/* OBSOLETE /* Gr1/rsp */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 1; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 1; */
/* OBSOLETE   out_msg_buf->write_r_msg.data[0] = read_register (GR1_REGNUM); */
/* OBSOLETE  */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE /* Global registers gr64-gr95 */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * (32); */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 64; */
/* OBSOLETE  */
/* OBSOLETE   for (regno = GR64_REGNUM; regno < GR64_REGNUM + 32; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_r_msg.data[regno - GR64_REGNUM] = read_register (regno); */
/* OBSOLETE     } */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE  */
/* OBSOLETE /* Global registers gr96-gr127 */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * (32); */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 96; */
/* OBSOLETE   for (regno = GR96_REGNUM; regno < GR96_REGNUM + 32; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_r_msg.data[regno - GR96_REGNUM] = read_register (regno); */
/* OBSOLETE     } */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Local Registers */ */
/* OBSOLETE   out_msg_buf->write_r_msg.memory_space = LOCAL_REG; */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 128; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 0; */
/* OBSOLETE  */
/* OBSOLETE   for (regno = LR0_REGNUM; regno < LR0_REGNUM + 128; regno++) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_r_msg.data[regno - LR0_REGNUM] = read_register (regno); */
/* OBSOLETE     } */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Protected Special Registers */ */
/* OBSOLETE   /* VAB through TMR */ */
/* OBSOLETE   out_msg_buf->write_r_msg.memory_space = SPECIAL_REG; */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 10; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 0; */
/* OBSOLETE   for (regno = 0; regno <= 9; regno++)	/* VAB through TMR */ */
/* OBSOLETE     out_msg_buf->write_r_msg.data[regno] = read_register (SR_REGNUM (regno)); */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* PC0, PC1, PC2 possibly as shadow registers */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 3; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   for (regno = 10; regno <= 12; regno++)	/* LRU and MMU */ */
/* OBSOLETE     out_msg_buf->write_r_msg.data[regno - 10] = read_register (SR_REGNUM (regno)); */
/* OBSOLETE   if (USE_SHADOW_PC) */
/* OBSOLETE     out_msg_buf->write_r_msg.address = 20;	/* SPC0 */ */
/* OBSOLETE   else */
/* OBSOLETE     out_msg_buf->write_r_msg.address = 10;	/* PC0 */ */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* LRU and MMU */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 2; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 13; */
/* OBSOLETE   for (regno = 13; regno <= 14; regno++)	/* LRU and MMU */ */
/* OBSOLETE     out_msg_buf->write_r_msg.data[regno - 13] = read_register (SR_REGNUM (regno)); */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Unprotected Special Registers */ */
/* OBSOLETE   out_msg_buf->write_r_msg.byte_count = 4 * 8; */
/* OBSOLETE   out_msg_buf->write_r_msg.length = 3 * 4 + out_msg_buf->write_r_msg.byte_count; */
/* OBSOLETE   out_msg_buf->write_r_msg.address = 128; */
/* OBSOLETE   for (regno = 128; regno <= 135; regno++) */
/* OBSOLETE     out_msg_buf->write_r_msg.data[regno - 128] = read_register (SR_REGNUM (regno)); */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (!expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   registers_changed (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /*************************************************** REMOTE_PREPARE_TO_STORE */ */
/* OBSOLETE /* Get ready to modify the registers array.  On machines which store */
/* OBSOLETE    individual registers, this doesn't need to do anything.  On machines */
/* OBSOLETE    which store all the registers in one fell swoop, this makes sure */
/* OBSOLETE    that registers contains all the registers from the program being */
/* OBSOLETE    debugged.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE mm_prepare_to_store (void) */
/* OBSOLETE { */
/* OBSOLETE   /* Do nothing, since we can store individual regs */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /******************************************************* REMOTE_XFER_MEMORY */ */
/* OBSOLETE static CORE_ADDR */
/* OBSOLETE translate_addr (CORE_ADDR addr) */
/* OBSOLETE { */
/* OBSOLETE #if defined(KERNEL_DEBUGGING) */
/* OBSOLETE   /* Check for a virtual address in the kernel */ */
/* OBSOLETE   /* Assume physical address of ublock is in  paddr_u register */ */
/* OBSOLETE   /* FIXME: doesn't work for user virtual addresses */ */
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
/* OBSOLETE /******************************************************* REMOTE_FILES_INFO */ */
/* OBSOLETE static void */
/* OBSOLETE mm_files_info (void) */
/* OBSOLETE { */
/* OBSOLETE   printf ("\tAttached to %s at %d baud and running program %s.\n", */
/* OBSOLETE 	  dev_name, baudrate, prog_name); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /************************************************* REMOTE_INSERT_BREAKPOINT */ */
/* OBSOLETE static int */
/* OBSOLETE mm_insert_breakpoint (CORE_ADDR addr, char *contents_cache) */
/* OBSOLETE { */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.code = BKPT_SET; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.length = 4 * 4; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.memory_space = I_MEM; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.bkpt_addr = (ADDR32) addr; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.pass_count = 1; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.bkpt_type = -1;	/* use illop for 29000 */ */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (expect_msg (BKPT_SET_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       return 0;			/* Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       return 1;			/* Failure */ */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /************************************************* REMOTE_DELETE_BREAKPOINT */ */
/* OBSOLETE static int */
/* OBSOLETE mm_remove_breakpoint (CORE_ADDR addr, char *contents_cache) */
/* OBSOLETE { */
/* OBSOLETE   out_msg_buf->bkpt_rm_msg.code = BKPT_RM; */
/* OBSOLETE   out_msg_buf->bkpt_rm_msg.length = 4 * 3; */
/* OBSOLETE   out_msg_buf->bkpt_rm_msg.memory_space = I_MEM; */
/* OBSOLETE   out_msg_buf->bkpt_rm_msg.bkpt_addr = (ADDR32) addr; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   if (expect_msg (BKPT_RM_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       return 0;			/* Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       return 1;			/* Failure */ */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /******************************************************* REMOTE_KILL */ */
/* OBSOLETE static void */
/* OBSOLETE mm_kill (char *arg, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   char buf[4]; */
/* OBSOLETE  */
/* OBSOLETE #if defined(KERNEL_DEBUGGING) */
/* OBSOLETE   /* We don't ever kill the kernel */ */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Kernel not killed, but left in current state.\n"); */
/* OBSOLETE       printf ("Use detach to leave kernel running.\n"); */
/* OBSOLETE     } */
/* OBSOLETE #else */
/* OBSOLETE   out_msg_buf->break_msg.code = BREAK; */
/* OBSOLETE   out_msg_buf->bkpt_set_msg.length = 4 * 0; */
/* OBSOLETE   expect_msg (HALT, in_msg_buf, from_tty); */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Target has been stopped."); */
/* OBSOLETE       printf ("Would you like to do a hardware reset (y/n) [n] "); */
/* OBSOLETE       fgets (buf, 3, stdin); */
/* OBSOLETE       if (buf[0] == 'y') */
/* OBSOLETE 	{ */
/* OBSOLETE 	  out_msg_buf->reset_msg.code = RESET; */
/* OBSOLETE 	  out_msg_buf->bkpt_set_msg.length = 4 * 0; */
/* OBSOLETE 	  expect_msg (RESET_ACK, in_msg_buf, from_tty); */
/* OBSOLETE 	  printf ("Target has been reset."); */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   pop_target (); */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /***************************************************************************/ */
/* OBSOLETE /*  */
/* OBSOLETE  * Load a program into the target. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE mm_load (char *arg_string, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   dont_repeat (); */
/* OBSOLETE  */
/* OBSOLETE #if defined(KERNEL_DEBUGGING) */
/* OBSOLETE   printf ("The kernel had better be loaded already!  Loading not done.\n"); */
/* OBSOLETE #else */
/* OBSOLETE   if (arg_string == 0) */
/* OBSOLETE     error ("The load command takes a file name"); */
/* OBSOLETE  */
/* OBSOLETE   arg_string = tilde_expand (arg_string); */
/* OBSOLETE   make_cleanup (xfree, arg_string); */
/* OBSOLETE   QUIT; */
/* OBSOLETE   immediate_quit++; */
/* OBSOLETE   error ("File loading is not yet supported for MiniMon."); */
/* OBSOLETE   /* FIXME, code to load your file here... */ */
/* OBSOLETE   /* You may need to do an init_target_mm() */ */
/* OBSOLETE   /* init_target_mm(?,?,?,?,?,?,?,?); */ */
/* OBSOLETE   immediate_quit--; */
/* OBSOLETE   /* symbol_file_add (arg_string, from_tty, text_addr, 0, 0); */ */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /************************************************ REMOTE_WRITE_INFERIOR_MEMORY */
/* OBSOLETE ** Copy LEN bytes of data from debugger memory at MYADDR */
/* OBSOLETE    to inferior's memory at MEMADDR.  Returns number of bytes written.  */ */
/* OBSOLETE static int */
/* OBSOLETE mm_write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int i, nwritten; */
/* OBSOLETE  */
/* OBSOLETE   out_msg_buf->write_req_msg.code = WRITE_REQ; */
/* OBSOLETE   out_msg_buf->write_req_msg.memory_space = mm_memory_space (memaddr); */
/* OBSOLETE  */
/* OBSOLETE   nwritten = 0; */
/* OBSOLETE   while (nwritten < len) */
/* OBSOLETE     { */
/* OBSOLETE       int num_to_write = len - nwritten; */
/* OBSOLETE       if (num_to_write > MAXDATA) */
/* OBSOLETE 	num_to_write = MAXDATA; */
/* OBSOLETE       for (i = 0; i < num_to_write; i++) */
/* OBSOLETE 	out_msg_buf->write_req_msg.data[i] = myaddr[i + nwritten]; */
/* OBSOLETE       out_msg_buf->write_req_msg.byte_count = num_to_write; */
/* OBSOLETE       out_msg_buf->write_req_msg.length = 3 * 4 + num_to_write; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = memaddr + nwritten; */
/* OBSOLETE       msg_send_serial (out_msg_buf); */
/* OBSOLETE  */
/* OBSOLETE       if (expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  nwritten += in_msg_buf->write_ack_msg.byte_count; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   return (nwritten); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /************************************************* REMOTE_READ_INFERIOR_MEMORY */
/* OBSOLETE ** Read LEN bytes from inferior memory at MEMADDR.  Put the result */
/* OBSOLETE    at debugger address MYADDR.  Returns number of bytes read.  */ */
/* OBSOLETE static int */
/* OBSOLETE mm_read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int i, nread; */
/* OBSOLETE  */
/* OBSOLETE   out_msg_buf->read_req_msg.code = READ_REQ; */
/* OBSOLETE   out_msg_buf->read_req_msg.memory_space = mm_memory_space (memaddr); */
/* OBSOLETE  */
/* OBSOLETE   nread = 0; */
/* OBSOLETE   while (nread < len) */
/* OBSOLETE     { */
/* OBSOLETE       int num_to_read = (len - nread); */
/* OBSOLETE       if (num_to_read > MAXDATA) */
/* OBSOLETE 	num_to_read = MAXDATA; */
/* OBSOLETE       out_msg_buf->read_req_msg.byte_count = num_to_read; */
/* OBSOLETE       out_msg_buf->read_req_msg.length = 3 * 4 + num_to_read; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = memaddr + nread; */
/* OBSOLETE       msg_send_serial (out_msg_buf); */
/* OBSOLETE  */
/* OBSOLETE       if (expect_msg (READ_ACK, in_msg_buf, 1)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  for (i = 0; i < in_msg_buf->read_ack_msg.byte_count; i++) */
/* OBSOLETE 	    myaddr[i + nread] = in_msg_buf->read_ack_msg.data[i]; */
/* OBSOLETE 	  nread += in_msg_buf->read_ack_msg.byte_count; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   return (nread); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* FIXME!  Merge these two.  */ */
/* OBSOLETE static int */
/* OBSOLETE mm_xfer_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len, int write, */
/* OBSOLETE 			 struct mem_attrib *attrib ATTRIBUTE_UNUSED, */
/* OBSOLETE 			 struct target_ops *target ATTRIBUTE_UNUSED) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   memaddr = translate_addr (memaddr); */
/* OBSOLETE  */
/* OBSOLETE   if (write) */
/* OBSOLETE     return mm_write_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE   else */
/* OBSOLETE     return mm_read_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /********************************************************** MSG_SEND_SERIAL */
/* OBSOLETE ** This function is used to send a message over the */
/* OBSOLETE ** serial line. */
/* OBSOLETE ** */
/* OBSOLETE ** If the message is successfully sent, a zero is */
/* OBSOLETE ** returned.  If the message was not sendable, a -1 */
/* OBSOLETE ** is returned.  This function blocks.  That is, it */
/* OBSOLETE ** does not return until the message is completely */
/* OBSOLETE ** sent, or until an error is encountered. */
/* OBSOLETE ** */
/* OBSOLETE */ */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE msg_send_serial (union msg_t *msg_ptr) */
/* OBSOLETE { */
/* OBSOLETE   INT32 message_size; */
/* OBSOLETE   int byte_count; */
/* OBSOLETE   int result; */
/* OBSOLETE   char c; */
/* OBSOLETE  */
/* OBSOLETE   /* Send message header */ */
/* OBSOLETE   byte_count = 0; */
/* OBSOLETE   message_size = msg_ptr->generic_msg.length + (2 * sizeof (INT32)); */
/* OBSOLETE   do */
/* OBSOLETE     { */
/* OBSOLETE       c = *((char *) msg_ptr + byte_count); */
/* OBSOLETE       result = write (mm_desc, &c, 1); */
/* OBSOLETE       if (result == 1) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  byte_count = byte_count + 1; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   while ((byte_count < message_size)); */
/* OBSOLETE  */
/* OBSOLETE   return (0); */
/* OBSOLETE }				/* end msg_send_serial() */ */
/* OBSOLETE  */
/* OBSOLETE /********************************************************** MSG_RECV_SERIAL */
/* OBSOLETE ** This function is used to receive a message over a */
/* OBSOLETE ** serial line. */
/* OBSOLETE ** */
/* OBSOLETE ** If the message is waiting in the buffer, a zero is */
/* OBSOLETE ** returned and the buffer pointed to by msg_ptr is filled */
/* OBSOLETE ** in.  If no message was available, a -1 is returned. */
/* OBSOLETE ** If timeout==0, wait indefinetly for a character. */
/* OBSOLETE ** */
/* OBSOLETE */ */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE msg_recv_serial (union msg_t *msg_ptr) */
/* OBSOLETE { */
/* OBSOLETE   static INT32 length = 0; */
/* OBSOLETE   static INT32 byte_count = 0; */
/* OBSOLETE   int result; */
/* OBSOLETE   char c; */
/* OBSOLETE   if (msg_ptr == 0)		/* re-sync request */ */
/* OBSOLETE     { */
/* OBSOLETE       length = 0; */
/* OBSOLETE       byte_count = 0; */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE       /* The timeout here is the prevailing timeout set with VTIME */ */
/* OBSOLETE       ->"timeout==0 semantics not supported" */
/* OBSOLETE 	read (mm_desc, in_buf, BUFER_SIZE); */
/* OBSOLETE #else */
/* OBSOLETE       alarm (1); */
/* OBSOLETE       read (mm_desc, in_buf, BUFER_SIZE); */
/* OBSOLETE       alarm (0); */
/* OBSOLETE #endif */
/* OBSOLETE       return (0); */
/* OBSOLETE     } */
/* OBSOLETE   /* Receive message */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE /* Timeout==0, help support the mm_wait() routine */ */
/* OBSOLETE   ->"timeout==0 semantics not supported (and its nice if they are)" */
/* OBSOLETE     result = read (mm_desc, &c, 1); */
/* OBSOLETE #else */
/* OBSOLETE   alarm (timeout); */
/* OBSOLETE   result = read (mm_desc, &c, 1); */
/* OBSOLETE   alarm (0); */
/* OBSOLETE #endif */
/* OBSOLETE   if (result < 0) */
/* OBSOLETE     { */
/* OBSOLETE       if (errno == EINTR) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  error ("Timeout reading from remote system."); */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	perror_with_name ("remote"); */
/* OBSOLETE     } */
/* OBSOLETE   else if (result == 1) */
/* OBSOLETE     { */
/* OBSOLETE       *((char *) msg_ptr + byte_count) = c; */
/* OBSOLETE       byte_count = byte_count + 1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Message header received.  Save message length. */ */
/* OBSOLETE   if (byte_count == (2 * sizeof (INT32))) */
/* OBSOLETE     length = msg_ptr->generic_msg.length; */
/* OBSOLETE  */
/* OBSOLETE   if (byte_count >= (length + (2 * sizeof (INT32)))) */
/* OBSOLETE     { */
/* OBSOLETE       /* Message received */ */
/* OBSOLETE       byte_count = 0; */
/* OBSOLETE       return (0); */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     return (-1); */
/* OBSOLETE  */
/* OBSOLETE }				/* end msg_recv_serial() */ */
/* OBSOLETE  */
/* OBSOLETE /********************************************************************* KBD_RAW */
/* OBSOLETE ** This function is used to put the keyboard in "raw" */
/* OBSOLETE ** mode for BSD Unix.  The original status is saved */
/* OBSOLETE ** so that it may be restored later. */
/* OBSOLETE */ */
/* OBSOLETE TERMINAL kbd_tbuf; */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE kbd_raw (void) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE   TERMINAL tbuf; */
/* OBSOLETE  */
/* OBSOLETE   /* Get keyboard termio (to save to restore original modes) */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   result = ioctl (0, TCGETA, &kbd_tbuf); */
/* OBSOLETE #else */
/* OBSOLETE   result = ioctl (0, TIOCGETP, &kbd_tbuf); */
/* OBSOLETE #endif */
/* OBSOLETE   if (result == -1) */
/* OBSOLETE     return (errno); */
/* OBSOLETE  */
/* OBSOLETE   /* Get keyboard TERMINAL (for modification) */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   result = ioctl (0, TCGETA, &tbuf); */
/* OBSOLETE #else */
/* OBSOLETE   result = ioctl (0, TIOCGETP, &tbuf); */
/* OBSOLETE #endif */
/* OBSOLETE   if (result == -1) */
/* OBSOLETE     return (errno); */
/* OBSOLETE  */
/* OBSOLETE   /* Set up new parameters */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   tbuf.c_iflag = tbuf.c_iflag & */
/* OBSOLETE     ~(INLCR | ICRNL | IUCLC | ISTRIP | IXON | BRKINT); */
/* OBSOLETE   tbuf.c_lflag = tbuf.c_lflag & ~(ICANON | ISIG | ECHO); */
/* OBSOLETE   tbuf.c_cc[4] = 0;		/* MIN */ */
/* OBSOLETE   tbuf.c_cc[5] = 0;		/* TIME */ */
/* OBSOLETE #else */
/* OBSOLETE   /* FIXME: not sure if this is correct (matches HAVE_TERMIO). */ */
/* OBSOLETE   tbuf.sg_flags |= RAW; */
/* OBSOLETE   tbuf.sg_flags |= ANYP; */
/* OBSOLETE   tbuf.sg_flags &= ~ECHO; */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   /* Set keyboard termio to new mode (RAW) */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   result = ioctl (0, TCSETAF, &tbuf); */
/* OBSOLETE #else */
/* OBSOLETE   result = ioctl (0, TIOCSETP, &tbuf); */
/* OBSOLETE #endif */
/* OBSOLETE   if (result == -1) */
/* OBSOLETE     return (errno); */
/* OBSOLETE  */
/* OBSOLETE   return (0); */
/* OBSOLETE }				/* end kbd_raw() */ */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /***************************************************************** KBD_RESTORE */
/* OBSOLETE ** This function is used to put the keyboard back in the */
/* OBSOLETE ** mode it was in before kbk_raw was called.  Note that */
/* OBSOLETE ** kbk_raw() must have been called at least once before */
/* OBSOLETE ** kbd_restore() is called. */
/* OBSOLETE */ */
/* OBSOLETE  */
/* OBSOLETE int */
/* OBSOLETE kbd_restore (void) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE  */
/* OBSOLETE   /* Set keyboard termio to original mode */ */
/* OBSOLETE #ifdef HAVE_TERMIO */
/* OBSOLETE   result = ioctl (0, TCSETAF, &kbd_tbuf); */
/* OBSOLETE #else */
/* OBSOLETE   result = ioctl (0, TIOCGETP, &kbd_tbuf); */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE   if (result == -1) */
/* OBSOLETE     return (errno); */
/* OBSOLETE  */
/* OBSOLETE   return (0); */
/* OBSOLETE }				/* end kbd_cooked() */ */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /*****************************************************************************/ */
/* OBSOLETE /* Fetch a single register indicatated by 'regno'.  */
/* OBSOLETE  * Returns 0/-1 on success/failure.   */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE fetch_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE   out_msg_buf->read_req_msg.code = READ_REQ; */
/* OBSOLETE   out_msg_buf->read_req_msg.length = 4 * 3; */
/* OBSOLETE   out_msg_buf->read_req_msg.byte_count = 4; */
/* OBSOLETE  */
/* OBSOLETE   if (regno == GR1_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = 1; */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = (regno - GR96_REGNUM) + 96; */
/* OBSOLETE     } */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->read_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = (regno - GR64_REGNUM) + 64; */
/* OBSOLETE     } */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE   else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->read_req_msg.memory_space = LOCAL_REG; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = (regno - LR0_REGNUM); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= FPE_REGNUM && regno <= EXO_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       int val = -1; */
/* OBSOLETE       supply_register (160 + (regno - FPE_REGNUM), &val); */
/* OBSOLETE       return 0;			/* Pretend Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->read_req_msg.memory_space = SPECIAL_REG; */
/* OBSOLETE       out_msg_buf->read_req_msg.address = regnum_to_srnum (regno); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE  */
/* OBSOLETE   if (expect_msg (READ_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       supply_register (regno, &(in_msg_buf->read_r_ack_msg.data[0])); */
/* OBSOLETE       result = 0; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE   return result; */
/* OBSOLETE } */
/* OBSOLETE /*****************************************************************************/ */
/* OBSOLETE /* Store a single register indicated by 'regno'.  */
/* OBSOLETE  * Returns 0/-1 on success/failure.   */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE store_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE  */
/* OBSOLETE   out_msg_buf->write_req_msg.code = WRITE_REQ; */
/* OBSOLETE   out_msg_buf->write_req_msg.length = 4 * 4; */
/* OBSOLETE   out_msg_buf->write_req_msg.byte_count = 4; */
/* OBSOLETE   out_msg_buf->write_r_msg.data[0] = read_register (regno); */
/* OBSOLETE  */
/* OBSOLETE   if (regno == GR1_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = 1; */
/* OBSOLETE       /* Setting GR1 changes the numbers of all the locals, so invalidate the  */
/* OBSOLETE        * register cache.  Do this *after* calling read_register, because we want  */
/* OBSOLETE        * read_register to return the value that write_register has just stuffed  */
/* OBSOLETE        * into the registers array, not the value of the register fetched from  */
/* OBSOLETE        * the inferior.   */
/* OBSOLETE        */ */
/* OBSOLETE       registers_changed (); */
/* OBSOLETE     } */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = (regno - GR64_REGNUM) + 64; */
/* OBSOLETE     } */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE   else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_req_msg.memory_space = GLOBAL_REG; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = (regno - GR96_REGNUM) + 96; */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128) */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_req_msg.memory_space = LOCAL_REG; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = (regno - LR0_REGNUM); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= FPE_REGNUM && regno <= EXO_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       return 0;			/* Pretend Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     /* An unprotected or protected special register */ */
/* OBSOLETE     { */
/* OBSOLETE       out_msg_buf->write_req_msg.memory_space = SPECIAL_REG; */
/* OBSOLETE       out_msg_buf->write_req_msg.address = regnum_to_srnum (regno); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE  */
/* OBSOLETE   if (expect_msg (WRITE_ACK, in_msg_buf, 1)) */
/* OBSOLETE     { */
/* OBSOLETE       result = 0; */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       result = -1; */
/* OBSOLETE     } */
/* OBSOLETE   return result; */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /*  */
/* OBSOLETE  * Convert a gdb special register number to a 29000 special register number. */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE regnum_to_srnum (int regno) */
/* OBSOLETE { */
/* OBSOLETE   switch (regno) */
/* OBSOLETE     { */
/* OBSOLETE     case VAB_REGNUM: */
/* OBSOLETE       return (0); */
/* OBSOLETE     case OPS_REGNUM: */
/* OBSOLETE       return (1); */
/* OBSOLETE     case CPS_REGNUM: */
/* OBSOLETE       return (2); */
/* OBSOLETE     case CFG_REGNUM: */
/* OBSOLETE       return (3); */
/* OBSOLETE     case CHA_REGNUM: */
/* OBSOLETE       return (4); */
/* OBSOLETE     case CHD_REGNUM: */
/* OBSOLETE       return (5); */
/* OBSOLETE     case CHC_REGNUM: */
/* OBSOLETE       return (6); */
/* OBSOLETE     case RBP_REGNUM: */
/* OBSOLETE       return (7); */
/* OBSOLETE     case TMC_REGNUM: */
/* OBSOLETE       return (8); */
/* OBSOLETE     case TMR_REGNUM: */
/* OBSOLETE       return (9); */
/* OBSOLETE     case NPC_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (20) : (10)); */
/* OBSOLETE     case PC_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (21) : (11)); */
/* OBSOLETE     case PC2_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (22) : (12)); */
/* OBSOLETE     case MMU_REGNUM: */
/* OBSOLETE       return (13); */
/* OBSOLETE     case LRU_REGNUM: */
/* OBSOLETE       return (14); */
/* OBSOLETE     case IPC_REGNUM: */
/* OBSOLETE       return (128); */
/* OBSOLETE     case IPA_REGNUM: */
/* OBSOLETE       return (129); */
/* OBSOLETE     case IPB_REGNUM: */
/* OBSOLETE       return (130); */
/* OBSOLETE     case Q_REGNUM: */
/* OBSOLETE       return (131); */
/* OBSOLETE     case ALU_REGNUM: */
/* OBSOLETE       return (132); */
/* OBSOLETE     case BP_REGNUM: */
/* OBSOLETE       return (133); */
/* OBSOLETE     case FC_REGNUM: */
/* OBSOLETE       return (134); */
/* OBSOLETE     case CR_REGNUM: */
/* OBSOLETE       return (135); */
/* OBSOLETE     case FPE_REGNUM: */
/* OBSOLETE       return (160); */
/* OBSOLETE     case INTE_REGNUM: */
/* OBSOLETE       return (161); */
/* OBSOLETE     case FPS_REGNUM: */
/* OBSOLETE       return (162); */
/* OBSOLETE     case EXO_REGNUM: */
/* OBSOLETE       return (164); */
/* OBSOLETE     default: */
/* OBSOLETE       return (255);		/* Failure ? */ */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /*  */
/* OBSOLETE  * Initialize the target debugger (minimon only). */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE init_target_mm (ADDR32 tstart, ADDR32 tend, ADDR32 dstart, ADDR32 dend, */
/* OBSOLETE 		ADDR32 entry, INT32 ms_size, INT32 rs_size, ADDR32 arg_start) */
/* OBSOLETE { */
/* OBSOLETE   out_msg_buf->init_msg.code = INIT; */
/* OBSOLETE   out_msg_buf->init_msg.length = sizeof (struct init_msg_t) - 2 * sizeof (INT32); */
/* OBSOLETE   out_msg_buf->init_msg.text_start = tstart; */
/* OBSOLETE   out_msg_buf->init_msg.text_end = tend; */
/* OBSOLETE   out_msg_buf->init_msg.data_start = dstart; */
/* OBSOLETE   out_msg_buf->init_msg.data_end = dend; */
/* OBSOLETE   out_msg_buf->init_msg.entry_point = entry; */
/* OBSOLETE   out_msg_buf->init_msg.mem_stack_size = ms_size; */
/* OBSOLETE   out_msg_buf->init_msg.reg_stack_size = rs_size; */
/* OBSOLETE   out_msg_buf->init_msg.arg_start = arg_start; */
/* OBSOLETE   msg_send_serial (out_msg_buf); */
/* OBSOLETE   expect_msg (INIT_ACK, in_msg_buf, 1); */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /*  */
/* OBSOLETE  * Return a pointer to a string representing the given message code. */
/* OBSOLETE  * Not all messages are represented here, only the ones that we expect */
/* OBSOLETE  * to be called with. */
/* OBSOLETE  */ */
/* OBSOLETE static char * */
/* OBSOLETE msg_str (INT32 code) */
/* OBSOLETE { */
/* OBSOLETE   static char cbuf[32]; */
/* OBSOLETE  */
/* OBSOLETE   switch (code) */
/* OBSOLETE     { */
/* OBSOLETE     case BKPT_SET_ACK: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "BKPT_SET_ACK", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case BKPT_RM_ACK: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "BKPT_RM_ACK", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case INIT_ACK: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "INIT_ACK", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case READ_ACK: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "READ_ACK", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case WRITE_ACK: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "WRITE_ACK", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case ERROR: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "ERROR", code); */
/* OBSOLETE       break; */
/* OBSOLETE     case HALT: */
/* OBSOLETE       sprintf (cbuf, "%s (%d)", "HALT", code); */
/* OBSOLETE       break; */
/* OBSOLETE     default: */
/* OBSOLETE       sprintf (cbuf, "UNKNOWN (%d)", code); */
/* OBSOLETE       break; */
/* OBSOLETE     } */
/* OBSOLETE   return (cbuf); */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /* */
/* OBSOLETE  * Selected (not all of them) error codes that we might get. */
/* OBSOLETE  */ */
/* OBSOLETE static char * */
/* OBSOLETE error_msg_str (INT32 code) */
/* OBSOLETE { */
/* OBSOLETE   static char cbuf[50]; */
/* OBSOLETE  */
/* OBSOLETE   switch (code) */
/* OBSOLETE     { */
/* OBSOLETE     case EMFAIL: */
/* OBSOLETE       return ("EMFAIL: unrecoverable error"); */
/* OBSOLETE     case EMBADADDR: */
/* OBSOLETE       return ("EMBADADDR: Illegal address"); */
/* OBSOLETE     case EMBADREG: */
/* OBSOLETE       return ("EMBADREG: Illegal register "); */
/* OBSOLETE     case EMACCESS: */
/* OBSOLETE       return ("EMACCESS: Could not access memory"); */
/* OBSOLETE     case EMBADMSG: */
/* OBSOLETE       return ("EMBADMSG: Unknown message type"); */
/* OBSOLETE     case EMMSG2BIG: */
/* OBSOLETE       return ("EMMSG2BIG: Message to large"); */
/* OBSOLETE     case EMNOSEND: */
/* OBSOLETE       return ("EMNOSEND: Could not send message"); */
/* OBSOLETE     case EMNORECV: */
/* OBSOLETE       return ("EMNORECV: Could not recv message"); */
/* OBSOLETE     case EMRESET: */
/* OBSOLETE       return ("EMRESET: Could not RESET target"); */
/* OBSOLETE     case EMCONFIG: */
/* OBSOLETE       return ("EMCONFIG: Could not get target CONFIG"); */
/* OBSOLETE     case EMSTATUS: */
/* OBSOLETE       return ("EMSTATUS: Could not get target STATUS"); */
/* OBSOLETE     case EMREAD: */
/* OBSOLETE       return ("EMREAD: Could not READ target memory"); */
/* OBSOLETE     case EMWRITE: */
/* OBSOLETE       return ("EMWRITE: Could not WRITE target memory"); */
/* OBSOLETE     case EMBKPTSET: */
/* OBSOLETE       return ("EMBKPTSET: Could not set breakpoint"); */
/* OBSOLETE     case EMBKPTRM: */
/* OBSOLETE       return ("EMBKPTRM: Could not remove breakpoint"); */
/* OBSOLETE     case EMBKPTSTAT: */
/* OBSOLETE       return ("EMBKPTSTAT: Could not get breakpoint status"); */
/* OBSOLETE     case EMBKPTNONE: */
/* OBSOLETE       return ("EMBKPTNONE: All breakpoints in use"); */
/* OBSOLETE     case EMBKPTUSED: */
/* OBSOLETE       return ("EMBKPTUSED: Breakpoints already in use"); */
/* OBSOLETE     case EMINIT: */
/* OBSOLETE       return ("EMINIT: Could not init target memory"); */
/* OBSOLETE     case EMGO: */
/* OBSOLETE       return ("EMGO: Could not start execution"); */
/* OBSOLETE     case EMSTEP: */
/* OBSOLETE       return ("EMSTEP: Could not single step"); */
/* OBSOLETE     case EMBREAK: */
/* OBSOLETE       return ("EMBREAK: Could not BREAK"); */
/* OBSOLETE     case EMCOMMERR: */
/* OBSOLETE       return ("EMCOMMERR: Communication error"); */
/* OBSOLETE     default: */
/* OBSOLETE       sprintf (cbuf, "error number %d", code); */
/* OBSOLETE       break; */
/* OBSOLETE     }				/* end switch */ */
/* OBSOLETE  */
/* OBSOLETE   return (cbuf); */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE  */
/* OBSOLETE /* Receive a message, placing it in MSG_BUF, and expect it to be of */
/* OBSOLETE    type MSGCODE.  If an error occurs, a non-zero FROM_TTY indicates */
/* OBSOLETE    that the message should be printed. */
/* OBSOLETE     */
/* OBSOLETE    Return 0 for failure, 1 for success.  */ */
/* OBSOLETE  */
/* OBSOLETE static int */
/* OBSOLETE expect_msg (INT32 msgcode, union msg_t *msg_buf, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   int retries = 0; */
/* OBSOLETE   while (msg_recv_serial (msg_buf) && (retries++ < MAX_RETRIES)); */
/* OBSOLETE   if (retries >= MAX_RETRIES) */
/* OBSOLETE     { */
/* OBSOLETE       printf ("Expected msg %s, ", msg_str (msgcode)); */
/* OBSOLETE       printf ("no message received!\n"); */
/* OBSOLETE       return (0);		/* Failure */ */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (msg_buf->generic_msg.code != msgcode) */
/* OBSOLETE     { */
/* OBSOLETE       if (from_tty) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  printf ("Expected msg %s, ", msg_str (msgcode)); */
/* OBSOLETE 	  printf ("got msg %s\n", msg_str (msg_buf->generic_msg.code)); */
/* OBSOLETE 	  if (msg_buf->generic_msg.code == ERROR) */
/* OBSOLETE 	    printf ("%s\n", error_msg_str (msg_buf->error_msg.error_code)); */
/* OBSOLETE 	} */
/* OBSOLETE       return (0);		/* Failure */ */
/* OBSOLETE     } */
/* OBSOLETE   return (1);			/* Success */ */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /* */
/* OBSOLETE  * Determine the MiniMon memory space qualifier based on the addr.  */
/* OBSOLETE  * FIXME: Can't distinguis I_ROM/D_ROM.   */
/* OBSOLETE  * FIXME: Doesn't know anything about I_CACHE/D_CACHE. */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE mm_memory_space (CORE_ADDR *addr) */
/* OBSOLETE { */
/* OBSOLETE   ADDR32 tstart = target_config.I_mem_start; */
/* OBSOLETE   ADDR32 tend = tstart + target_config.I_mem_size; */
/* OBSOLETE   ADDR32 dstart = target_config.D_mem_start; */
/* OBSOLETE   ADDR32 dend = tstart + target_config.D_mem_size; */
/* OBSOLETE   ADDR32 rstart = target_config.ROM_start; */
/* OBSOLETE   ADDR32 rend = tstart + target_config.ROM_size; */
/* OBSOLETE  */
/* OBSOLETE   if (((ADDR32) addr >= tstart) && ((ADDR32) addr < tend)) */
/* OBSOLETE     { */
/* OBSOLETE       return I_MEM; */
/* OBSOLETE     } */
/* OBSOLETE   else if (((ADDR32) addr >= dstart) && ((ADDR32) addr < dend)) */
/* OBSOLETE     { */
/* OBSOLETE       return D_MEM; */
/* OBSOLETE     } */
/* OBSOLETE   else if (((ADDR32) addr >= rstart) && ((ADDR32) addr < rend)) */
/* OBSOLETE     { */
/* OBSOLETE       /* FIXME: how do we determine between D_ROM and I_ROM */ */
/* OBSOLETE       return D_ROM; */
/* OBSOLETE     } */
/* OBSOLETE   else				/* FIXME: what do me do now? */ */
/* OBSOLETE     return D_MEM;		/* Hmmm! */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /*  */
/* OBSOLETE  *  Define the target subroutine names */
/* OBSOLETE  */ */
/* OBSOLETE struct target_ops mm_ops; */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE init_mm_ops (void) */
/* OBSOLETE { */
/* OBSOLETE   mm_ops.to_shortname = "minimon"; */
/* OBSOLETE   mm_ops.to_longname = "Remote AMD/Minimon target"; */
/* OBSOLETE   mm_ops.to_doc = "Remote debug an AMD 290*0 using the MiniMon dbg core on the target"; */
/* OBSOLETE   mm_ops.to_open = mm_open; */
/* OBSOLETE   mm_ops.to_close = mm_close; */
/* OBSOLETE   mm_ops.to_attach = mm_attach; */
/* OBSOLETE   mm_ops.to_post_attach = NULL; */
/* OBSOLETE   mm_ops.to_require_attach = NULL; */
/* OBSOLETE   mm_ops.to_detach = mm_detach; */
/* OBSOLETE   mm_ops.to_require_detach = NULL; */
/* OBSOLETE   mm_ops.to_resume = mm_resume; */
/* OBSOLETE   mm_ops.to_wait = mm_wait; */
/* OBSOLETE   mm_ops.to_post_wait = NULL; */
/* OBSOLETE   mm_ops.to_fetch_registers = mm_fetch_registers; */
/* OBSOLETE   mm_ops.to_store_registers = mm_store_registers; */
/* OBSOLETE   mm_ops.to_prepare_to_store = mm_prepare_to_store; */
/* OBSOLETE   mm_ops.to_xfer_memory = mm_xfer_inferior_memory; */
/* OBSOLETE   mm_ops.to_files_info = mm_files_info; */
/* OBSOLETE   mm_ops.to_insert_breakpoint = mm_insert_breakpoint; */
/* OBSOLETE   mm_ops.to_remove_breakpoint = mm_remove_breakpoint; */
/* OBSOLETE   mm_ops.to_terminal_init = 0; */
/* OBSOLETE   mm_ops.to_terminal_inferior = 0; */
/* OBSOLETE   mm_ops.to_terminal_ours_for_output = 0; */
/* OBSOLETE   mm_ops.to_terminal_ours = 0; */
/* OBSOLETE   mm_ops.to_terminal_info = 0; */
/* OBSOLETE   mm_ops.to_kill = mm_kill; */
/* OBSOLETE   mm_ops.to_load = mm_load; */
/* OBSOLETE   mm_ops.to_lookup_symbol = 0; */
/* OBSOLETE   mm_ops.to_create_inferior = mm_create_inferior; */
/* OBSOLETE   mm_ops.to_post_startup_inferior = NULL; */
/* OBSOLETE   mm_ops.to_acknowledge_created_inferior = NULL; */
/* OBSOLETE   mm_ops.to_clone_and_follow_inferior = NULL; */
/* OBSOLETE   mm_ops.to_post_follow_inferior_by_clone = NULL; */
/* OBSOLETE   mm_ops.to_insert_fork_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_remove_fork_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_insert_vfork_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_remove_vfork_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_has_forked = NULL; */
/* OBSOLETE   mm_ops.to_has_vforked = NULL; */
/* OBSOLETE   mm_ops.to_can_follow_vfork_prior_to_exec = NULL; */
/* OBSOLETE   mm_ops.to_post_follow_vfork = NULL; */
/* OBSOLETE   mm_ops.to_insert_exec_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_remove_exec_catchpoint = NULL; */
/* OBSOLETE   mm_ops.to_has_execd = NULL; */
/* OBSOLETE   mm_ops.to_reported_exec_events_per_exec_call = NULL; */
/* OBSOLETE   mm_ops.to_has_exited = NULL; */
/* OBSOLETE   mm_ops.to_mourn_inferior = mm_mourn; */
/* OBSOLETE   mm_ops.to_can_run = 0; */
/* OBSOLETE   mm_ops.to_notice_signals = 0; */
/* OBSOLETE   mm_ops.to_thread_alive = 0; */
/* OBSOLETE   mm_ops.to_stop = 0; */
/* OBSOLETE   mm_ops.to_pid_to_exec_file = NULL; */
/* OBSOLETE   mm_ops.to_stratum = process_stratum; */
/* OBSOLETE   mm_ops.DONT_USE = 0; */
/* OBSOLETE   mm_ops.to_has_all_memory = 1; */
/* OBSOLETE   mm_ops.to_has_memory = 1; */
/* OBSOLETE   mm_ops.to_has_stack = 1; */
/* OBSOLETE   mm_ops.to_has_registers = 1; */
/* OBSOLETE   mm_ops.to_has_execution = 1; */
/* OBSOLETE   mm_ops.to_sections = 0; */
/* OBSOLETE   mm_ops.to_sections_end = 0; */
/* OBSOLETE   mm_ops.to_magic = OPS_MAGIC; */
/* OBSOLETE }; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE _initialize_remote_mm (void) */
/* OBSOLETE { */
/* OBSOLETE   init_mm_ops (); */
/* OBSOLETE   add_target (&mm_ops); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE #ifdef NO_HIF_SUPPORT */
/* OBSOLETE service_HIF (union msg_t *msg) */
/* OBSOLETE { */
/* OBSOLETE   return (0);			/* Emulate a failure */ */
/* OBSOLETE } */
/* OBSOLETE #endif */
