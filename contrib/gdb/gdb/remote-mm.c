/* Remote debugging interface for Am290*0 running MiniMON monitor, for GDB.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Originally written by Daniel Mann at AMD.

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

/* This is like remote.c but ecpects MiniMON to be running on the Am29000 
   target hardware.
 - David Wood (wood@lab.ultra.nyu.edu) at New York University adapted this
	file to gdb 3.95.  I was unable to get this working on sun3os4
	with termio, only with sgtty.  Because we are only attempting to
	use this module to debug our kernel, which is already loaded when
	gdb is started up, I did not code up the file downloading facilities.  
	As a result this module has only the stubs to download files. 
	You should get tagged at compile time if you need to make any 
	changes/additions.  */
 
#include "defs.h"
#include "inferior.h"
#include "wait.h"
#include "value.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "gdb_string.h"
#include "terminal.h"
#include "minimon.h"
#include "target.h"

/* Offset of member MEMBER in a struct of type TYPE.  */
#define offsetof(TYPE, MEMBER) ((int) &((TYPE *)0)->MEMBER)

#define DRAIN_INPUT()	(msg_recv_serial((union msg_t*)0))

extern int stop_soon_quietly;           /* for wait_for_inferior */

static void mm_resume();
static void mm_fetch_registers ();
static int fetch_register ();
static void mm_store_registers ();
static int store_register ();
static int regnum_to_srnum();
static void  mm_close ();
static char* msg_str();
static char* error_msg_str();
static int expect_msg();
static void init_target_mm();
static int mm_memory_space();

#define FREEZE_MODE     (read_register(CPS_REGNUM) && 0x400)
#define USE_SHADOW_PC	((processor_type == a29k_freeze_mode) && FREEZE_MODE)

/* FIXME: Replace with `set remotedebug'.  */
#define LLOG_FILE "minimon.log"
#if defined (LOG_FILE)
FILE *log_file;
#endif

/*  
 * Size of message buffers.  I couldn't get memory reads to work when
 * the byte_count was larger than 512 (it may be a baud rate problem).
 */
#define BUFER_SIZE  512		
/* 
 * Size of data area in message buffer on the TARGET (remote system).
 */
#define MAXDATA_T  (target_config.max_msg_size - \
			offsetof(struct write_r_msg_t,data[0]))
/*		 
 * Size of data area in message buffer on the HOST (gdb). 
 */
#define MAXDATA_H  (BUFER_SIZE - offsetof(struct write_r_msg_t,data[0]))
/* 
 * Defined as the minimum size of data areas of the two message buffers 
 */
#define MAXDATA	   (MAXDATA_H < MAXDATA_T ? MAXDATA_H : MAXDATA_T)

static char out_buf[BUFER_SIZE];
static char  in_buf[BUFER_SIZE];

int msg_recv_serial();
int msg_send_serial();

#define MAX_RETRIES 5000
extern struct target_ops mm_ops;             /* Forward declaration */
struct config_msg_t  target_config;	/* HIF needs this */
union msg_t  *out_msg_buf = (union msg_t*)out_buf;
union msg_t  *in_msg_buf  = (union msg_t*)in_buf;

static int timeout = 5;

/* Descriptor for I/O to remote machine.  Initialize it to -1 so that
   mm_open knows that we don't have a file open when the program
   starts.  */
int mm_desc = -1;

/* stream which is fdopen'd from mm_desc.  Only valid when
   mm_desc != -1.  */
FILE *mm_stream;

/* Called when SIGALRM signal sent due to alarm() timeout.  */
#ifndef HAVE_TERMIO

#ifndef __STDC__
# ifndef volatile
#  define volatile /**/
# endif
#endif
volatile int n_alarms;

static void
mm_timer ()
{
#if 0
  if (kiodebug)
    printf ("mm_timer called\n");
#endif
  n_alarms++;
}
#endif	/* HAVE_TERMIO */

/* malloc'd name of the program on the remote system.  */
static char *prog_name = NULL;


/* Number of SIGTRAPs we need to simulate.  That is, the next
   NEED_ARTIFICIAL_TRAP calls to mm_wait should just return
   SIGTRAP without actually waiting for anything.  */

/**************************************************** REMOTE_CREATE_INFERIOR */
/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */
static void
mm_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
#define MAX_TOKENS 25
#define BUFFER_SIZE 256
   int	token_count;
   int	result;
   char	*token[MAX_TOKENS];
   char	cmd_line[BUFFER_SIZE];

  if (args && *args)
    error ("Can't pass arguments to remote mm process (yet).");

  if (execfile == 0 /* || exec_bfd == 0 */ )
    error ("No exec file specified");

  if (!mm_stream) {
        printf("Minimon not open yet.\n");
	return;
  }

  /* On ultra3 (NYU) we assume the kernel is already running so there is
     no file to download.
     FIXME: Fixed required here -> load your program, possibly with mm_load().
     */
  printf_filtered ("\n\
Assuming you are at NYU debuging a kernel, i.e., no need to download.\n\n");

  /* We will get a task spawn event immediately.  */
  init_wait_for_inferior ();
  clear_proceed_status ();
  stop_soon_quietly = 1;
  proceed (-1, TARGET_SIGNAL_DEFAULT, 0);
  normal_stop ();
}
/**************************************************** REMOTE_MOURN_INFERIOR */
static void
mm_mourn()
{
        pop_target ();                /* Pop back to no-child state */
        generic_mourn_inferior ();
}

/********************************************************************** damn_b
*/
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


/***************************************************************** REMOTE_OPEN
** Open a connection to remote minimon.
   NAME is the filename used for communication, then a space,
   then the baud rate.
   'target adapt /dev/ttya 9600 [prognam]' for example.
 */

static char *dev_name;
int baudrate = 9600;
static void
mm_open (name, from_tty)
     char *name;
     int from_tty;
{
  TERMINAL sg;
  unsigned int prl;
  char *p;

  /* Find the first whitespace character, it separates dev_name from
     prog_name.  */
  for (p = name;
       p && *p && !isspace (*p); p++)
    ;
  if (p == 0 || *p == '\0')
erroid:
    error ("Usage : <command> <serial-device> <baud-rate> [progname]");
  dev_name = (char*)xmalloc (p - name + 1);
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


  if (mm_desc >= 0)
    close (mm_desc);

  mm_desc = open (dev_name, O_RDWR);
  if (mm_desc < 0)
    perror_with_name (dev_name);
  ioctl (mm_desc, TIOCGETP, &sg);
#ifdef HAVE_TERMIO
  sg.c_cc[VMIN] = 0;		/* read with timeout.  */
  sg.c_cc[VTIME] = timeout * 10;
  sg.c_lflag &= ~(ICANON | ECHO);
  sg.c_cflag = (sg.c_cflag & ~CBAUD) | damn_b (baudrate);
#else
  sg.sg_ispeed = damn_b (baudrate);
  sg.sg_ospeed = damn_b (baudrate);
  sg.sg_flags |= RAW;
  sg.sg_flags |= ANYP;
  sg.sg_flags &= ~ECHO;
#endif


  ioctl (mm_desc, TIOCSETP, &sg);
  mm_stream = fdopen (mm_desc, "r+");

  push_target (&mm_ops);

#ifndef HAVE_TERMIO
#ifndef NO_SIGINTERRUPT
  /* Cause SIGALRM's to make reads fail with EINTR instead of resuming
     the read.  */
  if (siginterrupt (SIGALRM, 1) != 0)
    perror ("mm_open: error in siginterrupt");
#endif

  /* Set up read timeout timer.  */
  if ((void (*)) signal (SIGALRM, mm_timer) == (void (*)) -1)
    perror ("mm_open: error in signal");
#endif

#if defined (LOG_FILE)
  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    perror_with_name (LOG_FILE);
#endif
   /*
   ** Initialize target configuration structure (global)
   */
   DRAIN_INPUT();
   out_msg_buf->config_req_msg.code = CONFIG_REQ;
   out_msg_buf->config_req_msg.length = 4*0;
   msg_send_serial(out_msg_buf); /* send config request message */

   expect_msg(CONFIG,in_msg_buf,1);

  a29k_get_processor_type ();

  /* Print out some stuff, letting the user now what's going on */
  printf_filtered("Connected to MiniMon via %s.\n", dev_name);
    /* FIXME: can this restriction be removed? */
  printf_filtered("Remote debugging using virtual addresses works only\n");
  printf_filtered("\twhen virtual addresses map 1:1 to physical addresses.\n")
;
  if (processor_type != a29k_freeze_mode) {
        fprintf_filtered(stderr,
        "Freeze-mode debugging not available, and can only be done on an A29050.\n");
  }

   target_config.code = CONFIG;
   target_config.length = 0;
   target_config.processor_id = in_msg_buf->config_msg.processor_id;
   target_config.version = in_msg_buf->config_msg.version;
   target_config.I_mem_start = in_msg_buf->config_msg.I_mem_start;
   target_config.I_mem_size = in_msg_buf->config_msg.I_mem_size;
   target_config.D_mem_start = in_msg_buf->config_msg.D_mem_start;
   target_config.D_mem_size = in_msg_buf->config_msg.D_mem_size;
   target_config.ROM_start = in_msg_buf->config_msg.ROM_start;
   target_config.ROM_size =  in_msg_buf->config_msg.ROM_size;
   target_config.max_msg_size = in_msg_buf->config_msg.max_msg_size;
   target_config.max_bkpts = in_msg_buf->config_msg.max_bkpts;
   target_config.coprocessor = in_msg_buf->config_msg.coprocessor;
   target_config.reserved = in_msg_buf->config_msg.reserved;
   if (from_tty) {
   	printf("Connected to MiniMON :\n");
	printf("    Debugcore version            %d.%d\n",
		0x0f & (target_config.version >> 4),
		0x0f & (target_config.version ) );
	printf("    Configuration version        %d.%d\n",
		0x0f & (target_config.version >> 12),
		0x0f & (target_config.version >>  8) );
	printf("    Message system version       %d.%d\n",
		0x0f & (target_config.version >> 20),
		0x0f & (target_config.version >> 16) );
	printf("    Communication driver version %d.%d\n",
		0x0f & (target_config.version >> 28),
		0x0f & (target_config.version >> 24) );
   }

  /* Leave the target running... 
   * The above message stopped the target in the dbg core (MiniMon),  
   * so restart the target out of MiniMon, 
   */
  out_msg_buf->go_msg.code = GO;
  out_msg_buf->go_msg.length = 0;
  msg_send_serial(out_msg_buf);
  /* No message to expect after a GO */
}

/**************************************************************** REMOTE_CLOSE
** Close the open connection to the minimon debugger.
   Use this when you want to detach and do something else
   with your gdb.  */
static void
mm_close (quitting)	/*FIXME: how is quitting used */
     int quitting;
{
  if (mm_desc < 0)
    error ("Can't close remote connection: not debugging remotely.");

  /* We should never get here if there isn't something valid in
     mm_desc and mm_stream.  

     Due to a bug in Unix, fclose closes not only the stdio stream,
     but also the file descriptor.  So we don't actually close
     mm_desc.  */
  DRAIN_INPUT();
  fclose (mm_stream);	
  /* close (mm_desc); */

  /* Do not try to close mm_desc again, later in the program.  */
  mm_stream = NULL;
  mm_desc = -1;

#if defined (LOG_FILE)
  if (ferror (log_file))
    printf ("Error writing log file.\n");
  if (fclose (log_file) != 0)
    printf ("Error closing log file.\n");
#endif

  printf ("Ending remote debugging\n");
} 

/************************************************************* REMOTE_ATACH */
/* Attach to a program that is already loaded and running 
 * Upon exiting the process's execution is stopped.
 */
static void
mm_attach (args, from_tty)
     char *args;
     int from_tty;
{

  if (!mm_stream)
      error ("MiniMon not opened yet, use the 'target minimon' command.\n");
	
  if (from_tty)
      printf ("Attaching to remote program %s...\n", prog_name);

  /* Make sure the target is currently running, it is supposed to be. */
  /* FIXME: is it ok to send MiniMon a BREAK if it is already stopped in 
   * 	the dbg core.  If so, we don't need to send this GO.
   */
  out_msg_buf->go_msg.code = GO;
  out_msg_buf->go_msg.length = 0;
  msg_send_serial(out_msg_buf);
  sleep(2);	/* At the worst it will stop, receive a message, continue */
 
  /* Send the mm a break. */ 
  out_msg_buf->break_msg.code = BREAK;
  out_msg_buf->break_msg.length = 0;
  msg_send_serial(out_msg_buf);
}
/********************************************************** REMOTE_DETACH */
/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.  Leave remote process running (with no breakpoints set). */
static void
mm_detach (args,from_tty)
     char *args;
     int from_tty;
{
  remove_breakpoints();		/* Just in case there were any left in */
  out_msg_buf->go_msg.code = GO;
  out_msg_buf->go_msg.length = 0;
  msg_send_serial(out_msg_buf);
  pop_target();         	/* calls mm_close to do the real work */
}


/*************************************************************** REMOTE_RESUME
** Tell the remote machine to resume.  */

static void
mm_resume (pid, step, sig)
     int pid, step;
     enum target_signal sig;
{
  if (sig != TARGET_SIGNAL_0)
    warning ("Can't send signals to a remote MiniMon system.");

  if (step) {
      out_msg_buf->step_msg.code= STEP;
      out_msg_buf->step_msg.length = 1*4;
      out_msg_buf->step_msg.count = 1;		/* step 1 instruction */
      msg_send_serial(out_msg_buf);
  } else {
      out_msg_buf->go_msg.code= GO;
      out_msg_buf->go_msg.length = 0;
      msg_send_serial(out_msg_buf);
  }
}

/***************************************************************** REMOTE_WAIT
** Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.  */

static int
mm_wait (status)
     struct target_waitstatus *status;
{
  int i, result;
  int old_timeout = timeout;
  int old_immediate_quit = immediate_quit;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

/* wait for message to arrive. It should be:
	- A HIF service request.
	- A HIF exit service request.
	- A CHANNEL0_ACK.
	- A CHANNEL1 request.
	- a debugcore HALT message.
  HIF services must be responded too, and while-looping continued.
  If the target stops executing, mm_wait() should return.
*/
  timeout = 0;	/* Wait indefinetly for a message */
  immediate_quit = 1;   /* Helps ability to QUIT */
  while(1)
  {
    while(msg_recv_serial(in_msg_buf)) {
    	QUIT;	/* Let user quit if they want */
    }
    switch (in_msg_buf->halt_msg.code)
    {
    case HIF_CALL:
	i = in_msg_buf->hif_call_rtn_msg.service_number;
	result=service_HIF(in_msg_buf);
   	if(i == 1) /* EXIT */
	    goto exit;
	if(result)
	    printf("Warning: failure during HIF service %d\n", i);
	break;
    case CHANNEL0_ACK:
	service_HIF(in_msg_buf);
	break;
    case CHANNEL1:
        i=in_msg_buf->channel1_msg.length;
        in_msg_buf->channel1_msg.data[i] = '\0';
        printf("%s", in_msg_buf->channel1_msg.data);
	gdb_flush(stdout);
        /* Send CHANNEL1_ACK message */
        out_msg_buf->channel1_ack_msg.code = CHANNEL1_ACK;
        out_msg_buf->channel1_ack_msg.length = 0;
        result = msg_send_serial(out_msg_buf);
	break;
    case HALT:
	goto halted;
    default:
	goto halted;
    }
  }
halted:
  /* FIXME, these printfs should not be here.  This is a source level 
     debugger, guys!  */
  if (in_msg_buf->halt_msg.trap_number== 0)
  { printf("Am290*0 received vector number %d (break point)\n",
	in_msg_buf->halt_msg.trap_number);
    status->kind = TARGET_WAITKIND_STOPPED;
    status->value.sig = TARGET_SIGNAL_TRAP;
  }
  else if (in_msg_buf->halt_msg.trap_number== 1)
    {
      printf("Am290*0 received vector number %d\n",
	     in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_BUS;
    }
  else if (in_msg_buf->halt_msg.trap_number== 3
        || in_msg_buf->halt_msg.trap_number== 4)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_FPE;
  }
  else if (in_msg_buf->halt_msg.trap_number== 5)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_ILL;
  }
  else if (in_msg_buf->halt_msg.trap_number >= 6
        && in_msg_buf->halt_msg.trap_number <= 11)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_SEGV;
  }
  else if (in_msg_buf->halt_msg.trap_number== 12
        || in_msg_buf->halt_msg.trap_number== 13)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_ILL;
  }
  else if (in_msg_buf->halt_msg.trap_number== 14)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_ALRM;
  }
  else if (in_msg_buf->halt_msg.trap_number== 15)
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
    }
  else if (in_msg_buf->halt_msg.trap_number >= 16
        && in_msg_buf->halt_msg.trap_number <= 21)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_INT;
  }
  else if (in_msg_buf->halt_msg.trap_number== 22)
  { printf("Am290*0 received vector number %d\n",
	in_msg_buf->halt_msg.trap_number);
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_ILL;
  } /* BREAK message was sent */
  else if (in_msg_buf->halt_msg.trap_number== 75)
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
    }
  else
exit:
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = 0;
    }

  timeout = old_timeout;	/* Restore original timeout value */
  immediate_quit = old_immediate_quit;
  return 0;
}

/******************************************************* REMOTE_FETCH_REGISTERS
 * Read a remote register 'regno'. 
 * If regno==-1 then read all the registers.
 */
static void 
mm_fetch_registers (regno)
int	regno;
{
  INT32 *data_p;

  if (regno >= 0)  {
	fetch_register(regno);
	return;
  }

/* Gr1/rsp */
  out_msg_buf->read_req_msg.byte_count = 4*1;
  out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
  out_msg_buf->read_req_msg.address = 1;
  msg_send_serial(out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);
  supply_register (GR1_REGNUM , data_p);

#if defined(GR64_REGNUM)	/* Read gr64-127 */
/* Global Registers gr64-gr95 */ 
  out_msg_buf->read_req_msg.code= READ_REQ;
  out_msg_buf->read_req_msg.length = 4*3;
  out_msg_buf->read_req_msg.byte_count = 4*32;
  out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
  out_msg_buf->read_req_msg.address = 64;
  msg_send_serial(out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);

  for (regno=GR64_REGNUM; regno<GR64_REGNUM+32; regno++) {
      supply_register (regno, data_p++);
  }
#endif	/*  GR64_REGNUM */

/* Global Registers gr96-gr127 */ 
  out_msg_buf->read_req_msg.code= READ_REQ;
  out_msg_buf->read_req_msg.length = 4*3;
  out_msg_buf->read_req_msg.byte_count = 4 * 32;
  out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
  out_msg_buf->read_req_msg.address = 96;
  msg_send_serial(out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);

  for (regno=GR96_REGNUM; regno<GR96_REGNUM+32; regno++) {
      supply_register (regno, data_p++);
  }

/* Local Registers */ 
  out_msg_buf->read_req_msg.byte_count = 4 * (128);
  out_msg_buf->read_req_msg.memory_space = LOCAL_REG;
  out_msg_buf->read_req_msg.address = 0;
  msg_send_serial(out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);

  for (regno=LR0_REGNUM; regno<LR0_REGNUM+128; regno++) {
      supply_register (regno, data_p++);
  }

/* Protected Special Registers */ 
  out_msg_buf->read_req_msg.byte_count = 4*15;
  out_msg_buf->read_req_msg.memory_space = SPECIAL_REG;
  out_msg_buf->read_req_msg.address = 0;
  msg_send_serial( out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);

  for (regno=0; regno<=14; regno++) {
      supply_register (SR_REGNUM(regno), data_p++);
  }
  if (USE_SHADOW_PC) {	/* Let regno_to_srnum() handle the register number */
	fetch_register(NPC_REGNUM);
	fetch_register(PC_REGNUM);
	fetch_register(PC2_REGNUM);
  }

/* Unprotected Special Registers */ 
  out_msg_buf->read_req_msg.byte_count = 4*8;
  out_msg_buf->read_req_msg.memory_space = SPECIAL_REG;
  out_msg_buf->read_req_msg.address = 128;
  msg_send_serial( out_msg_buf);
  expect_msg(READ_ACK,in_msg_buf,1);
  data_p = &(in_msg_buf->read_r_ack_msg.data[0]);

  for (regno=128; regno<=135; regno++) {
      supply_register (SR_REGNUM(regno), data_p++);
  }

  /* There doesn't seem to be any way to get these.  */
  {
    int val = -1;
    supply_register (FPE_REGNUM, &val);
    supply_register (INTE_REGNUM, &val);
    supply_register (FPS_REGNUM, &val);
    supply_register (EXO_REGNUM, &val);
  }
}


/****************************************************** REMOTE_STORE_REGISTERS
 * Store register regno into the target.  
 * If regno==-1 then store all the registers.
 * Result is 0 for success, -1 for failure.
 */

static void
mm_store_registers (regno)
int regno;
{
  int result;
  
  if (regno >= 0) {
    store_register(regno);
    return;
  }

  result = 0;

  out_msg_buf->write_r_msg.code= WRITE_REQ;

/* Gr1/rsp */
  out_msg_buf->write_r_msg.byte_count = 4*1;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.memory_space = GLOBAL_REG;
  out_msg_buf->write_r_msg.address = 1;
  out_msg_buf->write_r_msg.data[0] = read_register (GR1_REGNUM);

  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

#if defined(GR64_REGNUM)
/* Global registers gr64-gr95 */
  out_msg_buf->write_r_msg.byte_count = 4* (32);
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 64;

  for (regno=GR64_REGNUM ; regno<GR64_REGNUM+32 ; regno++)
    {
      out_msg_buf->write_r_msg.data[regno-GR64_REGNUM] = read_register (regno);
    }
  msg_send_serial(out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }
#endif	/* GR64_REGNUM */

/* Global registers gr96-gr127 */
  out_msg_buf->write_r_msg.byte_count = 4* (32);
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 96;
  for (regno=GR96_REGNUM ; regno<GR96_REGNUM+32 ; regno++)
    {
      out_msg_buf->write_r_msg.data[regno-GR96_REGNUM] = read_register (regno);
    }
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

/* Local Registers */
  out_msg_buf->write_r_msg.memory_space = LOCAL_REG;
  out_msg_buf->write_r_msg.byte_count = 4*128;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 0;

  for (regno = LR0_REGNUM ; regno < LR0_REGNUM+128 ; regno++)
    {
      out_msg_buf->write_r_msg.data[regno-LR0_REGNUM] = read_register (regno);
    }
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

/* Protected Special Registers */ 
  /* VAB through TMR */
  out_msg_buf->write_r_msg.memory_space = SPECIAL_REG;
  out_msg_buf->write_r_msg.byte_count = 4* 10;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 0;
  for (regno = 0 ; regno<=9 ; regno++)	/* VAB through TMR */
    out_msg_buf->write_r_msg.data[regno] = read_register (SR_REGNUM(regno));
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

  /* PC0, PC1, PC2 possibly as shadow registers */
  out_msg_buf->write_r_msg.byte_count = 4* 3;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  for (regno=10 ; regno<=12 ; regno++)	/* LRU and MMU */
    out_msg_buf->write_r_msg.data[regno-10] = read_register (SR_REGNUM(regno));
  if (USE_SHADOW_PC) 
    out_msg_buf->write_r_msg.address = 20;	/* SPC0 */
  else 
    out_msg_buf->write_r_msg.address = 10;	/* PC0 */
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

  /* LRU and MMU */
  out_msg_buf->write_r_msg.byte_count = 4* 2;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 13;
  for (regno=13 ; regno<=14 ; regno++)	/* LRU and MMU */
    out_msg_buf->write_r_msg.data[regno-13] = read_register (SR_REGNUM(regno));
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }

/* Unprotected Special Registers */ 
  out_msg_buf->write_r_msg.byte_count = 4*8;
  out_msg_buf->write_r_msg.length = 3*4 + out_msg_buf->write_r_msg.byte_count;
  out_msg_buf->write_r_msg.address = 128;
  for (regno = 128 ; regno<=135 ; regno++)
    out_msg_buf->write_r_msg.data[regno-128] = read_register(SR_REGNUM(regno));
  msg_send_serial( out_msg_buf);
  if (!expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = -1;
  }
 
  registers_changed ();
}

/*************************************************** REMOTE_PREPARE_TO_STORE */
/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
mm_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

/******************************************************* REMOTE_XFER_MEMORY */
static CORE_ADDR
translate_addr(addr)
CORE_ADDR addr;
{
#if defined(KERNEL_DEBUGGING)
        /* Check for a virtual address in the kernel */
        /* Assume physical address of ublock is in  paddr_u register */
	/* FIXME: doesn't work for user virtual addresses */
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

/******************************************************* REMOTE_FILES_INFO */
static void
mm_files_info ()
{
  printf ("\tAttached to %s at %d baud and running program %s.\n",
          dev_name, baudrate, prog_name);
}

/************************************************* REMOTE_INSERT_BREAKPOINT */
static int
mm_insert_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  out_msg_buf->bkpt_set_msg.code = BKPT_SET;
  out_msg_buf->bkpt_set_msg.length = 4*4;
  out_msg_buf->bkpt_set_msg.memory_space = I_MEM;
  out_msg_buf->bkpt_set_msg.bkpt_addr = (ADDR32) addr;
  out_msg_buf->bkpt_set_msg.pass_count = 1;
  out_msg_buf->bkpt_set_msg.bkpt_type = -1;	/* use illop for 29000 */
  msg_send_serial( out_msg_buf);
  if (expect_msg(BKPT_SET_ACK,in_msg_buf,1)) {
	return 0;		/* Success */
  } else {
	return 1;		/* Failure */
  }
}

/************************************************* REMOTE_DELETE_BREAKPOINT */
static int
mm_remove_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  out_msg_buf->bkpt_rm_msg.code = BKPT_RM;
  out_msg_buf->bkpt_rm_msg.length = 4*3;
  out_msg_buf->bkpt_rm_msg.memory_space = I_MEM;
  out_msg_buf->bkpt_rm_msg.bkpt_addr = (ADDR32) addr;
  msg_send_serial( out_msg_buf);
  if (expect_msg(BKPT_RM_ACK,in_msg_buf,1)) {
	return 0;		/* Success */
  } else {
	return 1;		/* Failure */
  }
}


/******************************************************* REMOTE_KILL */
static void
mm_kill(arg,from_tty)
char    *arg;
int     from_tty;
{
	char	buf[4];

#if defined(KERNEL_DEBUGGING)
	/* We don't ever kill the kernel */
	if (from_tty) {
		printf("Kernel not killed, but left in current state.\n");
	 	printf("Use detach to leave kernel running.\n");
	}
#else
  	out_msg_buf->break_msg.code = BREAK;
  	out_msg_buf->bkpt_set_msg.length = 4*0;
	expect_msg(HALT,in_msg_buf,from_tty);
	if (from_tty) {
		printf("Target has been stopped.");
		printf("Would you like to do a hardware reset (y/n) [n] ");
		fgets(buf,3,stdin);	
		if (buf[0] == 'y') {
			out_msg_buf->reset_msg.code = RESET;
			out_msg_buf->bkpt_set_msg.length = 4*0;
			expect_msg(RESET_ACK,in_msg_buf,from_tty);
			printf("Target has been reset.");
		}
	}
	pop_target();
#endif 
}



/***************************************************************************/
/* 
 * Load a program into the target.
 */
static void
mm_load(arg_string,from_tty)
char	*arg_string;
int	from_tty;
{
  dont_repeat ();

#if defined(KERNEL_DEBUGGING)
  printf("The kernel had better be loaded already!  Loading not done.\n");
#else
  if (arg_string == 0)
    error ("The load command takes a file name");

  arg_string = tilde_expand (arg_string);
  make_cleanup (free, arg_string);
  QUIT;
  immediate_quit++;
  error("File loading is not yet supported for MiniMon.");
  /* FIXME, code to load your file here... */
  /* You may need to do an init_target_mm() */
  /* init_target_mm(?,?,?,?,?,?,?,?); */
  immediate_quit--;
  /* symbol_file_add (arg_string, from_tty, text_addr, 0, 0); */
#endif

}

/************************************************ REMOTE_WRITE_INFERIOR_MEMORY
** Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.  Returns number of bytes written.  */
static int
mm_write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i,nwritten;

  out_msg_buf->write_req_msg.code= WRITE_REQ;
  out_msg_buf->write_req_msg.memory_space = mm_memory_space(memaddr);	

  nwritten=0;
  while (nwritten < len) {
	int num_to_write = len - nwritten;
	if (num_to_write > MAXDATA) num_to_write = MAXDATA;
  	for (i=0 ; i < num_to_write ; i++)
      		out_msg_buf->write_req_msg.data[i] = myaddr[i+nwritten];
  	out_msg_buf->write_req_msg.byte_count = num_to_write;
  	out_msg_buf->write_req_msg.length = 3*4 + num_to_write; 
  	out_msg_buf->write_req_msg.address = memaddr + nwritten;
  	msg_send_serial(out_msg_buf);

  	if (expect_msg(WRITE_ACK,in_msg_buf,1)) {
  		nwritten += in_msg_buf->write_ack_msg.byte_count;
  	} else {
		break;	
  	}
  }
  return(nwritten);
}

/************************************************* REMOTE_READ_INFERIOR_MEMORY
** Read LEN bytes from inferior memory at MEMADDR.  Put the result
   at debugger address MYADDR.  Returns number of bytes read.  */
static int
mm_read_inferior_memory(memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int i,nread;

  out_msg_buf->read_req_msg.code= READ_REQ;
  out_msg_buf->read_req_msg.memory_space = mm_memory_space(memaddr);

  nread=0;
  while (nread < len) {
	int num_to_read = (len - nread);
	if (num_to_read > MAXDATA) num_to_read = MAXDATA;
  	out_msg_buf->read_req_msg.byte_count = num_to_read; 
  	out_msg_buf->read_req_msg.length = 3*4 + num_to_read; 
  	out_msg_buf->read_req_msg.address = memaddr + nread;
  	msg_send_serial(out_msg_buf);

  	if (expect_msg(READ_ACK,in_msg_buf,1)) {
  		for (i=0 ; i<in_msg_buf->read_ack_msg.byte_count ; i++)
      			myaddr[i+nread] = in_msg_buf->read_ack_msg.data[i];
  		nread += in_msg_buf->read_ack_msg.byte_count;
  	} else {
		break;	
  	}
  }
  return(nread);
}

/* FIXME!  Merge these two.  */
static int
mm_xfer_inferior_memory (memaddr, myaddr, len, write)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
{

  memaddr = translate_addr(memaddr);

  if (write)
    return mm_write_inferior_memory (memaddr, myaddr, len);
  else
    return mm_read_inferior_memory (memaddr, myaddr, len);
}


/********************************************************** MSG_SEND_SERIAL
** This function is used to send a message over the
** serial line.
**
** If the message is successfully sent, a zero is
** returned.  If the message was not sendable, a -1
** is returned.  This function blocks.  That is, it
** does not return until the message is completely
** sent, or until an error is encountered.
**
*/

int
msg_send_serial(msg_ptr)
   union  msg_t  *msg_ptr;
{
   INT32  message_size;
   int    byte_count;
   int    result;
   char   c;

   /* Send message header */
   byte_count = 0;
   message_size = msg_ptr->generic_msg.length + (2 * sizeof(INT32));
   do {
      c = *((char *)msg_ptr+byte_count);
      result = write(mm_desc, &c, 1);
      if (result == 1) {
         byte_count = byte_count + 1;
      }
   } while ((byte_count < message_size) );

   return(0);
}  /* end msg_send_serial() */

/********************************************************** MSG_RECV_SERIAL
** This function is used to receive a message over a
** serial line.
**
** If the message is waiting in the buffer, a zero is
** returned and the buffer pointed to by msg_ptr is filled
** in.  If no message was available, a -1 is returned.
** If timeout==0, wait indefinetly for a character.
**
*/

int
msg_recv_serial(msg_ptr)
union  msg_t  *msg_ptr;
{
   static INT32  length=0;
   static INT32  byte_count=0;
   int    result;
   char   c;
  if(msg_ptr == 0)		/* re-sync request */
  {  length=0;
     byte_count=0;
#ifdef HAVE_TERMIO
     /* The timeout here is the prevailing timeout set with VTIME */
     ->"timeout==0 semantics not supported"
     read(mm_desc, in_buf, BUFER_SIZE);
#else
     alarm (1);
     read(mm_desc, in_buf, BUFER_SIZE);
     alarm (0);
#endif
     return(0);
  }
   /* Receive message */
#ifdef HAVE_TERMIO
/* Timeout==0, help support the mm_wait() routine */
   ->"timeout==0 semantics not supported (and its nice if they are)"
   result = read(mm_desc, &c, 1);
#else
  alarm(timeout);
  result = read(mm_desc, &c, 1);
  alarm (0);
#endif
  if ( result < 0) {
      if (errno == EINTR) {
      	error ("Timeout reading from remote system.");
      } else
	perror_with_name ("remote");
  } else if (result == 1) {
      *((char *)msg_ptr+byte_count) = c;
      byte_count = byte_count + 1;
  }

   /* Message header received.  Save message length. */
  if (byte_count == (2 * sizeof(INT32)))
      length = msg_ptr->generic_msg.length;

  if (byte_count >= (length + (2 * sizeof(INT32)))) {
      /* Message received */
      byte_count = 0;
      return(0);
  } else
      return (-1);

}  /* end msg_recv_serial() */

/********************************************************************* KBD_RAW
** This function is used to put the keyboard in "raw"
** mode for BSD Unix.  The original status is saved
** so that it may be restored later.
*/
TERMINAL kbd_tbuf;

int
kbd_raw() {
   int    result;
   TERMINAL tbuf;

   /* Get keyboard termio (to save to restore original modes) */
#ifdef HAVE_TERMIO
   result = ioctl(0, TCGETA, &kbd_tbuf);
#else
   result = ioctl(0, TIOCGETP, &kbd_tbuf);
#endif
   if (result == -1)
      return (errno);

   /* Get keyboard TERMINAL (for modification) */
#ifdef HAVE_TERMIO
   result = ioctl(0, TCGETA, &tbuf);
#else
   result = ioctl(0, TIOCGETP, &tbuf);
#endif
   if (result == -1)
      return (errno);

   /* Set up new parameters */
#ifdef HAVE_TERMIO
   tbuf.c_iflag = tbuf.c_iflag &
      ~(INLCR | ICRNL | IUCLC | ISTRIP | IXON | BRKINT);
   tbuf.c_lflag = tbuf.c_lflag & ~(ICANON | ISIG | ECHO);
   tbuf.c_cc[4] = 0;  /* MIN */
   tbuf.c_cc[5] = 0;  /* TIME */
#else
   /* FIXME: not sure if this is correct (matches HAVE_TERMIO). */
   tbuf.sg_flags |= RAW;
   tbuf.sg_flags |= ANYP;
   tbuf.sg_flags &= ~ECHO;
#endif

   /* Set keyboard termio to new mode (RAW) */
#ifdef HAVE_TERMIO
   result = ioctl(0, TCSETAF, &tbuf);
#else
   result = ioctl(0, TIOCSETP, &tbuf);
#endif 
   if (result == -1)
      return (errno);

   return (0);
}  /* end kbd_raw() */



/***************************************************************** KBD_RESTORE
** This function is used to put the keyboard back in the
** mode it was in before kbk_raw was called.  Note that
** kbk_raw() must have been called at least once before
** kbd_restore() is called.
*/

int
kbd_restore() {
   int result;

   /* Set keyboard termio to original mode */
#ifdef HAVE_TERMIO
   result = ioctl(0, TCSETAF, &kbd_tbuf);
#else
   result = ioctl(0, TIOCGETP, &kbd_tbuf);
#endif

   if (result == -1)
      return (errno);

   return(0);
}  /* end kbd_cooked() */


/*****************************************************************************/ 
/* Fetch a single register indicatated by 'regno'. 
 * Returns 0/-1 on success/failure.  
 */
static int
fetch_register (regno)
     int regno;
{
     int  result;
  out_msg_buf->read_req_msg.code= READ_REQ;
  out_msg_buf->read_req_msg.length = 4*3;
  out_msg_buf->read_req_msg.byte_count = 4;

  if (regno == GR1_REGNUM)
  { out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->read_req_msg.address = 1;
  }
  else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32)
  { out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->read_req_msg.address = (regno - GR96_REGNUM) + 96;
  }
#if defined(GR64_REGNUM)
  else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32 )
  { out_msg_buf->read_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->read_req_msg.address = (regno - GR64_REGNUM) + 64;
  }
#endif	/* GR64_REGNUM */
  else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128)
  { out_msg_buf->read_req_msg.memory_space = LOCAL_REG;
    out_msg_buf->read_req_msg.address = (regno - LR0_REGNUM);
  }
  else if (regno>=FPE_REGNUM && regno<=EXO_REGNUM)  
  { int val = -1;
    supply_register(160 + (regno - FPE_REGNUM),&val);
    return 0;		/* Pretend Success */
  }
  else 
  { out_msg_buf->read_req_msg.memory_space = SPECIAL_REG;
    out_msg_buf->read_req_msg.address = regnum_to_srnum(regno); 
  } 

  msg_send_serial(out_msg_buf);

  if (expect_msg(READ_ACK,in_msg_buf,1)) {
  	supply_register (regno, &(in_msg_buf->read_r_ack_msg.data[0]));
	result = 0;
  } else {
	result = -1;
  }
  return result;
}
/*****************************************************************************/ 
/* Store a single register indicated by 'regno'. 
 * Returns 0/-1 on success/failure.  
 */
static int
store_register (regno)
     int regno;
{
     int  result;

  out_msg_buf->write_req_msg.code= WRITE_REQ;
  out_msg_buf->write_req_msg.length = 4*4;
  out_msg_buf->write_req_msg.byte_count = 4;
  out_msg_buf->write_r_msg.data[0] = read_register (regno);

  if (regno == GR1_REGNUM)
  { out_msg_buf->write_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->write_req_msg.address = 1;
    /* Setting GR1 changes the numbers of all the locals, so invalidate the 
     * register cache.  Do this *after* calling read_register, because we want 
     * read_register to return the value that write_register has just stuffed 
     * into the registers array, not the value of the register fetched from 
     * the inferior.  
     */
    registers_changed ();
  }
#if defined(GR64_REGNUM)
  else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32 )
  { out_msg_buf->write_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->write_req_msg.address = (regno - GR64_REGNUM) + 64;
  }
#endif	/* GR64_REGNUM */
  else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32)
  { out_msg_buf->write_req_msg.memory_space = GLOBAL_REG;
    out_msg_buf->write_req_msg.address = (regno - GR96_REGNUM) + 96;
  }
  else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128)
  { out_msg_buf->write_req_msg.memory_space = LOCAL_REG;
    out_msg_buf->write_req_msg.address = (regno - LR0_REGNUM);
  }
  else if (regno>=FPE_REGNUM && regno<=EXO_REGNUM)  
  { 
    return 0;		/* Pretend Success */
  }
  else 	/* An unprotected or protected special register */
  { out_msg_buf->write_req_msg.memory_space = SPECIAL_REG;
    out_msg_buf->write_req_msg.address = regnum_to_srnum(regno); 
  } 

  msg_send_serial(out_msg_buf);

  if (expect_msg(WRITE_ACK,in_msg_buf,1)) {
	result = 0;
  } else {
	result = -1;
  }
  return result;
}
/****************************************************************************/
/* 
 * Convert a gdb special register number to a 29000 special register number.
 */
static int
regnum_to_srnum(regno)
int	regno;
{
	switch(regno) {
		case VAB_REGNUM: return(0); 
		case OPS_REGNUM: return(1); 
		case CPS_REGNUM: return(2); 
		case CFG_REGNUM: return(3); 
		case CHA_REGNUM: return(4); 
		case CHD_REGNUM: return(5); 
		case CHC_REGNUM: return(6); 
		case RBP_REGNUM: return(7); 
		case TMC_REGNUM: return(8); 
		case TMR_REGNUM: return(9); 
		case NPC_REGNUM: return(USE_SHADOW_PC ? (20) : (10));
		case PC_REGNUM:  return(USE_SHADOW_PC ? (21) : (11));
		case PC2_REGNUM: return(USE_SHADOW_PC ? (22) : (12));
		case MMU_REGNUM: return(13); 
		case LRU_REGNUM: return(14); 
		case IPC_REGNUM: return(128); 
		case IPA_REGNUM: return(129); 
		case IPB_REGNUM: return(130); 
		case Q_REGNUM:   return(131); 
		case ALU_REGNUM: return(132); 
		case BP_REGNUM:  return(133); 
		case FC_REGNUM:  return(134); 
		case CR_REGNUM:  return(135); 
		case FPE_REGNUM: return(160); 
		case INTE_REGNUM: return(161); 
		case FPS_REGNUM: return(162); 
		case EXO_REGNUM:return(164); 
		default:
			return(255);	/* Failure ? */
	}
}
/****************************************************************************/
/* 
 * Initialize the target debugger (minimon only).
 */
static void
init_target_mm(tstart,tend,dstart,dend,entry,ms_size,rs_size,arg_start)
ADDR32	tstart,tend,dstart,dend,entry;
INT32	ms_size,rs_size;
ADDR32	arg_start;
{
	out_msg_buf->init_msg.code = INIT;
	out_msg_buf->init_msg.length= sizeof(struct init_msg_t)-2*sizeof(INT32);
	out_msg_buf->init_msg.text_start = tstart;
	out_msg_buf->init_msg.text_end = tend;
	out_msg_buf->init_msg.data_start = dstart;
	out_msg_buf->init_msg.data_end = dend;
	out_msg_buf->init_msg.entry_point = entry;
	out_msg_buf->init_msg.mem_stack_size = ms_size;
	out_msg_buf->init_msg.reg_stack_size = rs_size;
	out_msg_buf->init_msg.arg_start = arg_start;
	msg_send_serial(out_msg_buf);
	expect_msg(INIT_ACK,in_msg_buf,1);
}
/****************************************************************************/
/* 
 * Return a pointer to a string representing the given message code.
 * Not all messages are represented here, only the ones that we expect
 * to be called with.
 */
static char*
msg_str(code)
INT32	code;
{
	static char cbuf[32];

	switch (code) {
	case BKPT_SET_ACK: sprintf(cbuf,"%s (%d)","BKPT_SET_ACK",code); break; 
	case BKPT_RM_ACK: sprintf(cbuf,"%s (%d)","BKPT_RM_ACK",code); break; 
	case INIT_ACK: 	  sprintf(cbuf,"%s (%d)","INIT_ACK",code); break; 
	case READ_ACK: 	  sprintf(cbuf,"%s (%d)","READ_ACK",code); break; 
	case WRITE_ACK:	  sprintf(cbuf,"%s (%d)","WRITE_ACK",code); break; 
	case ERROR:       sprintf(cbuf,"%s (%d)","ERROR",code); break; 
	case HALT: 	sprintf(cbuf,"%s (%d)","HALT",code); break; 
	default:	sprintf(cbuf,"UNKNOWN (%d)",code); break; 
	}
	return(cbuf);
}
/****************************************************************************/
/*
 * Selected (not all of them) error codes that we might get.
 */
static char* 
error_msg_str(code)
INT32	code;
{
	static char cbuf[50];

	switch (code) {
	case EMFAIL: 	return("EMFAIL: unrecoverable error"); 
	case EMBADADDR: return("EMBADADDR: Illegal address"); 
	case EMBADREG: 	return("EMBADREG: Illegal register "); 
	case EMACCESS: 	return("EMACCESS: Could not access memory");
	case EMBADMSG: 	return("EMBADMSG: Unknown message type"); 
	case EMMSG2BIG: return("EMMSG2BIG: Message to large"); 
	case EMNOSEND: 	return("EMNOSEND: Could not send message"); 
	case EMNORECV: 	return("EMNORECV: Could not recv message"); 
	case EMRESET: 	return("EMRESET: Could not RESET target"); 
	case EMCONFIG: 	return("EMCONFIG: Could not get target CONFIG"); 
	case EMSTATUS: 	return("EMSTATUS: Could not get target STATUS"); 
	case EMREAD: 	return("EMREAD: Could not READ target memory"); 
	case EMWRITE: 	return("EMWRITE: Could not WRITE target memory"); 
	case EMBKPTSET: return("EMBKPTSET: Could not set breakpoint"); 
	case EMBKPTRM:	return("EMBKPTRM: Could not remove breakpoint"); 
	case EMBKPTSTAT:return("EMBKPTSTAT: Could not get breakpoint status"); 
	case EMBKPTNONE:return("EMBKPTNONE: All breakpoints in use"); 
	case EMBKPTUSED:return("EMBKPTUSED: Breakpoints already in use"); 
	case EMINIT: 	return("EMINIT: Could not init target memory"); 
	case EMGO: 	return("EMGO: Could not start execution"); 
	case EMSTEP: 	return("EMSTEP: Could not single step"); 
	case EMBREAK: 	return("EMBREAK: Could not BREAK"); 
	case EMCOMMERR: return("EMCOMMERR: Communication error"); 
	default:     	sprintf(cbuf,"error number %d",code); break;
	} /* end switch */

	return (cbuf);
}
/****************************************************************************/
/* 
 *  Receive a message and expect it to be of type msgcode.
 *  Returns 0/1 on failure/success.
 */
static int
expect_msg(msgcode,msg_buf,from_tty)
INT32	msgcode;		/* Msg code we expect */
union msg_t *msg_buf;		/* Where to put  the message received */
int	from_tty;		/* Print message on error if non-zero */
{
  int	retries=0;
  while(msg_recv_serial(msg_buf) && (retries++<MAX_RETRIES)); 
  if (retries >= MAX_RETRIES) {
	printf("Expected msg %s, ",msg_str(msgcode));
	printf("no message received!\n");
        return(0);		/* Failure */
  }

  if (msg_buf->generic_msg.code != msgcode) {
     if (from_tty) {
	printf("Expected msg %s, ",msg_str(msgcode));
	printf("got msg %s\n",msg_str(msg_buf->generic_msg.code));
        if (msg_buf->generic_msg.code == ERROR) 
		printf("%s\n",error_msg_str(msg_buf->error_msg.error_code));
     }
     return(0);			/* Failure */
  }
  return(1);			/* Success */
}	
/****************************************************************************/
/*
 * Determine the MiniMon memory space qualifier based on the addr. 
 * FIXME: Can't distinguis I_ROM/D_ROM.  
 * FIXME: Doesn't know anything about I_CACHE/D_CACHE.
 */
static int
mm_memory_space(addr)
CORE_ADDR	*addr;
{
	ADDR32 tstart = target_config.I_mem_start;
	ADDR32 tend   = tstart + target_config.I_mem_size;  
	ADDR32 dstart = target_config.D_mem_start;
	ADDR32 dend   = tstart + target_config.D_mem_size;  
	ADDR32 rstart = target_config.ROM_start;
	ADDR32 rend   = tstart + target_config.ROM_size;  

	if (((ADDR32)addr >= tstart) && ((ADDR32)addr < tend)) { 
		return I_MEM;
	} else if (((ADDR32)addr >= dstart) && ((ADDR32)addr < dend)) { 
		return D_MEM;
	} else if (((ADDR32)addr >= rstart) && ((ADDR32)addr < rend)) {
		/* FIXME: how do we determine between D_ROM and I_ROM */
		return D_ROM;
	} else	/* FIXME: what do me do now? */
		return D_MEM;	/* Hmmm! */
}

/****************************************************************************/
/* 
 *  Define the target subroutine names 
 */
struct target_ops mm_ops = {
        "minimon", "Remote AMD/Minimon target",
	"Remote debug an AMD 290*0 using the MiniMon dbg core on the target",
        mm_open, mm_close,
        mm_attach, mm_detach, mm_resume, mm_wait,
        mm_fetch_registers, mm_store_registers,
        mm_prepare_to_store,
        mm_xfer_inferior_memory,
        mm_files_info,
        mm_insert_breakpoint, mm_remove_breakpoint, /* Breakpoints */
        0, 0, 0, 0, 0,          /* Terminal handling */
        mm_kill,             	/* FIXME, kill */
        mm_load, 
        0,                      /* lookup_symbol */
        mm_create_inferior,  /* create_inferior */
        mm_mourn,            /* mourn_inferior FIXME */
	0,			/* can_run */
	0, /* notice_signals */
	0,			/* to_stop */
        process_stratum, 0, /* next */
        1, 1, 1, 1, 1,  /* all mem, mem, stack, regs, exec */
	0,0,		/* sections, sections_end */
        OPS_MAGIC,              /* Always the last thing */
};

void
_initialize_remote_mm()
{
  add_target (&mm_ops);
}

#ifdef NO_HIF_SUPPORT
service_HIF(msg)
union msg_t	*msg;
{
	return(0);	/* Emulate a failure */
}
#endif
