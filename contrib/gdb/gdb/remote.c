/* Remote target communications for serial-line targets in custom GDB protocol
   Copyright 1988, 1991, 1992, 1993, 1994, 1995, 1996 Free Software Foundation, Inc.

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

/* Remote communication protocol.

   A debug packet whose contents are <data>
   is encapsulated for transmission in the form:

	$ <data> # CSUM1 CSUM2

	<data> must be ASCII alphanumeric and cannot include characters
	'$' or '#'.  If <data> starts with two characters followed by
	':', then the existing stubs interpret this as a sequence number.

	CSUM1 and CSUM2 are ascii hex representation of an 8-bit 
	checksum of <data>, the most significant nibble is sent first.
	the hex digits 0-9,a-f are used.

   Receiver responds with:

	+	- if CSUM is correct and ready for next packet
	-	- if CSUM is incorrect

   <data> is as follows:
   Most values are encoded in ascii hex digits.  Signal numbers are according
   to the numbering in target.h.

	Request		Packet

	set thread	Hct...		Set thread for subsequent operations.
					c = 'c' for thread used in step and 
					continue; t... can be -1 for all
					threads.
					c = 'g' for thread used in other
					operations.  If zero, pick a thread,
					any thread.
	reply		OK		for success
			ENN		for an error.

	read registers  g
	reply		XX....X		Each byte of register data
					is described by two hex digits.
					Registers are in the internal order
					for GDB, and the bytes in a register
					are in the same order the machine uses.
			or ENN		for an error.

	write regs	GXX..XX		Each byte of register data
					is described by two hex digits.
	reply		OK		for success
			ENN		for an error

        write reg	Pn...=r...	Write register n... with value r...,
					which contains two hex digits for each
					byte in the register (target byte
					order).
	reply		OK		for success
			ENN		for an error
	(not supported by all stubs).

	read mem	mAA..AA,LLLL	AA..AA is address, LLLL is length.
	reply		XX..XX		XX..XX is mem contents
					Can be fewer bytes than requested
					if able to read only part of the data.
			or ENN		NN is errno

	write mem	MAA..AA,LLLL:XX..XX
					AA..AA is address,
					LLLL is number of bytes,
					XX..XX is data
	reply		OK		for success
			ENN		for an error (this includes the case
					where only part of the data was
					written).

	continue	cAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	step		sAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	continue with	Csig;AA		Continue with signal sig (hex signal
	signal				number).

	step with	Ssig;AA		Like 'C' but step not continue.
	signal

	last signal     ?               Reply the current reason for stopping.
                                        This is the same reply as is generated
					for step or cont : SAA where AA is the
					signal number.

	detach          D               Reply OK.

	There is no immediate reply to step or cont.
	The reply comes when the machine stops.
	It is		SAA		AA is the signal number.

	or...		TAAn...:r...;n...:r...;n...:r...;
					AA = signal number
					n... = register number (hex)
					  r... = register contents
					n... = `thread'
					  r... = thread process ID.  This is
						 a hex integer.
					n... = other string not starting 
					    with valid hex digit.
					  gdb should ignore this n,r pair
					  and go on to the next.  This way
					  we can extend the protocol.
	or...		WAA		The process exited, and AA is
					the exit status.  This is only
					applicable for certains sorts of
					targets.
	or...		XAA		The process terminated with signal
					AA.
        or...           OXX..XX	XX..XX  is hex encoding of ASCII data. This
					can happen at any time while the program is
					running and the debugger should
					continue to wait for 'W', 'T', etc.

	thread alive	TXX		Find out if the thread XX is alive.
	reply		OK		thread is still alive
			ENN		thread is dead
	
	remote restart	RXX		Restart the remote server

	extended ops 	!		Use the extended remote protocol.
					Sticky -- only needs to be set once.

	kill request	k

	toggle debug	d		toggle debug flag (see 386 & 68k stubs)
	reset		r		reset -- see sparc stub.
	reserved	<other>		On other requests, the stub should
					ignore the request and send an empty
					response ($#<checksum>).  This way
					we can extend the protocol and GDB
					can tell whether the stub it is
					talking to uses the old or the new.
	search		tAA:PP,MM	Search backwards starting at address
					AA for a match with pattern PP and
					mask MM.  PP and MM are 4 bytes.
					Not supported by all stubs.

	general query	qXXXX		Request info about XXXX.
	general set	QXXXX=yyyy	Set value of XXXX to yyyy.
	query sect offs	qOffsets	Get section offsets.  Reply is
					Text=xxx;Data=yyy;Bss=zzz

	Responses can be run-length encoded to save space.  A '*' means that
	the next character is an ASCII encoding giving a repeat count which
	stands for that many repititions of the character preceding the '*'.
	The encoding is n+29, yielding a printable character where n >=3 
	(which is where rle starts to win).  Don't use an n > 126.

	So 
	"0* " means the same as "0000".  */

#include "defs.h"
#include "gdb_string.h"
#include <fcntl.h>
#include "frame.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "target.h"
#include "wait.h"
/*#include "terminal.h"*/
#include "gdbcmd.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "thread.h"

#include "dcache.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <signal.h>
#include "serial.h"

/* Prototypes for local functions */

static int remote_write_bytes PARAMS ((CORE_ADDR memaddr,
				       char *myaddr, int len));

static int remote_read_bytes PARAMS ((CORE_ADDR memaddr,
				      char *myaddr, int len));

static void remote_files_info PARAMS ((struct target_ops *ignore));

static int remote_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr,
				       int len, int should_write,
				       struct target_ops *target));

static void remote_prepare_to_store PARAMS ((void));

static void remote_fetch_registers PARAMS ((int regno));

static void remote_resume PARAMS ((int pid, int step,
				   enum target_signal siggnal));

static int remote_start_remote PARAMS ((char *dummy));

static void remote_open PARAMS ((char *name, int from_tty));

static void extended_remote_open PARAMS ((char *name, int from_tty));

static void remote_open_1 PARAMS ((char *, int, struct target_ops *));

static void remote_close PARAMS ((int quitting));

static void remote_store_registers PARAMS ((int regno));

static void remote_mourn PARAMS ((void));

static void extended_remote_restart PARAMS ((void));

static void extended_remote_mourn PARAMS ((void));

static void extended_remote_create_inferior PARAMS ((char *, char *, char **));

static void remote_mourn_1 PARAMS ((struct target_ops *));

static void getpkt PARAMS ((char *buf, int forever));

static int putpkt PARAMS ((char *buf));

static void remote_send PARAMS ((char *buf));

static int readchar PARAMS ((int timeout));

static int remote_wait PARAMS ((int pid, struct target_waitstatus *status));

static void remote_kill PARAMS ((void));

static int tohex PARAMS ((int nib));

static int fromhex PARAMS ((int a));

static void remote_detach PARAMS ((char *args, int from_tty));

static void remote_interrupt PARAMS ((int signo));

static void remote_interrupt_twice PARAMS ((int signo));

static void interrupt_query PARAMS ((void));

extern struct target_ops remote_ops;	/* Forward decl */
extern struct target_ops extended_remote_ops;	/* Forward decl */

/* This was 5 seconds, which is a long time to sit and wait.
   Unless this is going though some terminal server or multiplexer or
   other form of hairy serial connection, I would think 2 seconds would
   be plenty.  */

static int remote_timeout = 2;

/* This variable chooses whether to send a ^C or a break when the user
   requests program interruption.  Although ^C is usually what remote
   systems expect, and that is the default here, sometimes a break is
   preferable instead.  */

static int remote_break;

/* Descriptor for I/O to remote machine.  Initialize it to NULL so that
   remote_open knows that we don't have a file open when the program
   starts.  */
serial_t remote_desc = NULL;

/* Having this larger than 400 causes us to be incompatible with m68k-stub.c
   and i386-stub.c.  Normally, no one would notice because it only matters
   for writing large chunks of memory (e.g. in downloads).  Also, this needs
   to be more than 400 if required to hold the registers (see below, where
   we round it up based on REGISTER_BYTES).  */
#define	PBUFSIZ	400

/* Maximum number of bytes to read/write at once.  The value here
   is chosen to fill up a packet (the headers account for the 32).  */
#define MAXBUFBYTES ((PBUFSIZ-32)/2)

/* Round up PBUFSIZ to hold all the registers, at least.  */
/* The blank line after the #if seems to be required to work around a
   bug in HP's PA compiler.  */
#if REGISTER_BYTES > MAXBUFBYTES

#undef PBUFSIZ
#define	PBUFSIZ	(REGISTER_BYTES * 2 + 32)
#endif

/* Should we try the 'P' request?  If this is set to one when the stub
   doesn't support 'P', the only consequence is some unnecessary traffic.  */
static int stub_supports_P = 1;


/*
 * Support for quasi-interactive control of device through GDB port.
 * While we're waiting for an event to occur, chat with the running device.
 */
#define	REMOTE_CHAT
#ifdef	REMOTE_CHAT

extern int      quit_flag;

static char     tty_input[256];
static int      escape_count;
static int      echo_check;
static int	remote_chat = 0;

enum read_stat {
    READ_MORE,
    FATAL_ERROR,
    ENTER_DEBUG
};

static enum read_stat
readtarget()
{
    int             j;
    int             data;

    /* Loop until the socket doesn't have any more data */
    while ((data = readchar(0)) >= 0) {

	/* Check for the escape sequence */
	if (data == '|') {
	    /* If this is the fourth escape, get out */
	    if (++escape_count == 4)
		return (ENTER_DEBUG);
	    continue;		/* Not the fourth, continue */

	} else {
	    /*
	     * Not an escape any more, Ensure any pending ones are flushed
	     */
	    for (j = 1; j <= escape_count; j++)
		putchar('|');
	    escape_count = 0;
	}

	if (data == '\r')	/* If this is a return character */
	    continue;		/* just supress it */

	if (echo_check != -1) {	/* If we are checking for an echo */
	    /* If this might be an echo */
	    if (tty_input[echo_check] == data) {
		echo_check++;	/* Note one more character match */
		continue;	/* Go and loop */
	    } else {

		if ((data == '\n') && (tty_input[echo_check] == '\r')) {
		    /* If this is the end of the line */
		    echo_check = -1;	/* No more echo supression */
		    continue;	/* Go and loop */
		}

		/* Not an echo, print out the data */
		for (j = 0; j < echo_check; j++)
		    putchar(tty_input[j]);

		echo_check = -1;/* No more echo checking */
	    }
	}
	putchar(data);		/* Output the character */
    }
    return (READ_MORE);		/* Indicate to read some more */
}

static enum read_stat
readtty()
{
    enum read_stat  status;
    int             tty_bc;

    /* First, read a buffer full from the terminal */
    if ((tty_bc = read(fileno(stdin), tty_input, sizeof(tty_input) - 1)) < 0) {
	perror_with_name("readtty: read failed");
	return (FATAL_ERROR);
    }

    /* Turn trailing newlines into returns */
    if (tty_input[tty_bc - 1] == '\n')
	tty_input[tty_bc - 1] = '\r';

    if ((tty_input[0] == '~') && (tty_bc == 3))
	switch (tty_input[1]) {
	case 'b':				/* ~b\n = send break & gdb */
	    SERIAL_SEND_BREAK (remote_desc);
	    /* fall through */

	case 'c':				/* ~c\n = return to gdb */
	    return (ENTER_DEBUG);
	}
    
    /* Make this a zero terminated string and write it out */
    tty_input[tty_bc] = '\0';

    if (SERIAL_WRITE(remote_desc, tty_input, tty_bc)) {
	perror_with_name("readtty: write failed");
	return (FATAL_ERROR);
    }

    return (READ_MORE);
}

static int
remote_talk()
{
    fd_set          input;
    int             tablesize;
    enum read_stat  status;
    int             panic_flag = 0;
    char            buf[4];

    escape_count = 0;
    echo_check = -1;

    tablesize = getdtablesize();

    for (;;) {

	/*
	 * Check for anything from our socket - doesn't block. Note that this
	 * must be done *before* the select as there may be buffered I/O
	 * waiting to be processed.
	 */
	if ((status = readtarget()) != READ_MORE)
	    return (status);

	fflush(stdout);		/* Flush output before blocking */

	/* Now block on more socket input or TTY input */
	FD_ZERO(&input);
	FD_SET(fileno(stdin), &input);
	FD_SET(remote_desc->fd, &input);

	status = select(tablesize, &input, 0, 0, 0);
	if ((status < 0) && (errno != EINTR)) {
	    perror_with_name("remote_talk: select");
	    return (FATAL_ERROR);
	}

	/* Handle Control-C typed */
	if (quit_flag) {
	    if ((++panic_flag) == 3) {
		printf("\nAre you repeating Control-C to terminate "
		       "the session? (y/n) [n] ");
		fgets(buf, 3, stdin);
		if (buf[0] == 'y') {
		    pop_target();	/* Clean up */
		    error("Debugging terminated by user interrupt");
		}
		panic_flag = 0;
	    }
	    quit_flag = 0;
	    SERIAL_WRITE(remote_desc, "\003", 1);
	    continue;
	}

	/* Handle terminal input */
	if (FD_ISSET(fileno(stdin), &input)) {
	    panic_flag = 0;
	    status = readtty();
	    if (status != READ_MORE)
		return (status);
	    echo_check = 0;
	}
    }
}

#endif /* REMOTE_CHAT */

/* These are the threads which we last sent to the remote system.  -1 for all
   or -2 for not sent yet.  */
int general_thread;
int cont_thread;

static void
set_thread (th, gen)
     int th;
     int gen;
{
  char buf[PBUFSIZ];
  int state = gen ? general_thread : cont_thread;
  if (state == th)
    return;
  buf[0] = 'H';
  buf[1] = gen ? 'g' : 'c';
  if (th == 42000)
    {
      buf[2] = '0';
      buf[3] = '\0';
    }
  else if (th < 0)
    sprintf (&buf[2], "-%x", -th);
  else
    sprintf (&buf[2], "%x", th);
  putpkt (buf);
  getpkt (buf, 0);
  if (gen)
    general_thread = th;
  else
    cont_thread = th;
}

/*  Return nonzero if the thread TH is still alive on the remote system.  */

static int
remote_thread_alive (th)
     int th;
{
  char buf[PBUFSIZ];

  buf[0] = 'T';
  if (th < 0)
    sprintf (&buf[1], "-%x", -th);
  else
    sprintf (&buf[1], "%x", th);
  putpkt (buf);
  getpkt (buf, 0);
  return (buf[0] == 'O' && buf[1] == 'K');
}

/*  Restart the remote side; this is an extended protocol operation.  */

static void
extended_remote_restart ()
{
  char buf[PBUFSIZ];

  /* Send the restart command; for reasons I don't understand the
     remote side really expects a number after the "R".  */
  buf[0] = 'R';
  sprintf (&buf[1], "%x", 0);
  putpkt (buf);

  /* Now query for status so this looks just like we restarted
     gdbserver from scratch.  */
  putpkt ("?");
  getpkt (buf, 0);
}

/* Clean up connection to a remote debugger.  */

/* ARGSUSED */
static void
remote_close (quitting)
     int quitting;
{
  if (remote_desc)
    SERIAL_CLOSE (remote_desc);
  remote_desc = NULL;
}

/* Query the remote side for the text, data and bss offsets. */

static void
get_offsets ()
{
  char buf[PBUFSIZ];
  int nvals;
  CORE_ADDR text_addr, data_addr, bss_addr;
  struct section_offsets *offs;

#ifdef	REMOTE_CHAT
      if (remote_chat)
	  (void) remote_talk();
#endif	/* REMOTE_CHAT */

  putpkt ("qOffsets");

  getpkt (buf, 0);

  if (buf[0] == '\000')
    return;			/* Return silently.  Stub doesn't support this
				   command. */
  if (buf[0] == 'E')
    {
      warning ("Remote failure reply: %s", buf);
      return;
    }

  nvals = sscanf (buf, "Text=%lx;Data=%lx;Bss=%lx", &text_addr, &data_addr,
		  &bss_addr);
  if (nvals != 3)
    error ("Malformed response to offset query, %s", buf);

  if (symfile_objfile == NULL)
    return;

  offs = (struct section_offsets *) alloca (sizeof (struct section_offsets)
					    + symfile_objfile->num_sections
					    * sizeof (offs->offsets));
  memcpy (offs, symfile_objfile->section_offsets,
	  sizeof (struct section_offsets)
	  + symfile_objfile->num_sections
	  * sizeof (offs->offsets));

  ANOFFSET (offs, SECT_OFF_TEXT) = text_addr;

  /* This is a temporary kludge to force data and bss to use the same offsets
     because that's what nlmconv does now.  The real solution requires changes
     to the stub and remote.c that I don't have time to do right now.  */

  ANOFFSET (offs, SECT_OFF_DATA) = data_addr;
  ANOFFSET (offs, SECT_OFF_BSS) = data_addr;

  objfile_relocate (symfile_objfile, offs);
}

/* Stub for catch_errors.  */

static int
remote_start_remote (dummy)
     char *dummy;
{
  immediate_quit = 1;		/* Allow user to interrupt it */

  /* Ack any packet which the remote side has already sent.  */
  SERIAL_WRITE (remote_desc, "+", 1);

  /* Let the stub know that we want it to return the thread.  */
  set_thread (-1, 0);

  get_offsets ();		/* Get text, data & bss offsets */

  putpkt ("?");			/* initiate a query from remote machine */
  immediate_quit = 0;

  start_remote ();		/* Initialize gdb process mechanisms */
  return 1;
}

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static void
remote_open (name, from_tty)
     char *name;
     int from_tty;
{
  remote_open_1 (name, from_tty, &remote_ops);
}

/* Open a connection to a remote debugger using the extended
   remote gdb protocol.  NAME is the filename used for communication.  */

static void
extended_remote_open (name, from_tty)
     char *name;
     int from_tty;
{
  char buf[PBUFSIZ];

  /* Do the basic remote open stuff.  */
  remote_open_1 (name, from_tty, &extended_remote_ops);

  /* Now tell the remote that we're using the extended protocol.  */
  putpkt ("!");
  getpkt (buf, 0);

}

/* Generic code for opening a connection to a remote target.  */
static DCACHE *remote_dcache;

static void
remote_open_1 (name, from_tty, target)
     char *name;
     int from_tty;
     struct target_ops *target;
{
  if (name == 0)
    error ("To open a remote debug connection, you need to specify what serial\n\
device is attached to the remote system (e.g. /dev/ttya).");

  target_preopen (from_tty);

  unpush_target (target);

  remote_dcache = dcache_init (remote_read_bytes, remote_write_bytes);

  remote_desc = SERIAL_OPEN (name);
  if (!remote_desc)
    perror_with_name (name);

  if (baud_rate != -1)
    {
      if (SERIAL_SETBAUDRATE (remote_desc, baud_rate))
	{
	  SERIAL_CLOSE (remote_desc);
	  perror_with_name (name);
	}
    }


  SERIAL_RAW (remote_desc);

  /* If there is something sitting in the buffer we might take it as a
     response to a command, which would be bad.  */
  SERIAL_FLUSH_INPUT (remote_desc);

  if (from_tty)
    {
      puts_filtered ("Remote debugging using ");
      puts_filtered (name);
      puts_filtered ("\n");
    }
  push_target (target);	/* Switch to using remote target now */

  /* Start out by trying the 'P' request to set registers.  We set this each
     time that we open a new target so that if the user switches from one
     stub to another, we can (if the target is closed and reopened) cope.  */
  stub_supports_P = 1;

  general_thread = -2;
  cont_thread = -2;

  /* Without this, some commands which require an active target (such as kill)
     won't work.  This variable serves (at least) double duty as both the pid
     of the target process (if it has such), and as a flag indicating that a
     target is active.  These functions should be split out into seperate
     variables, especially since GDB will someday have a notion of debugging
     several processes.  */

  inferior_pid = 42000;
  /* Start the remote connection; if error (0), discard this target.
     In particular, if the user quits, be sure to discard it
     (we'd be in an inconsistent state otherwise).  */
  if (!catch_errors (remote_start_remote, (char *)0, 
		     "Couldn't establish connection to remote target\n", RETURN_MASK_ALL))
    pop_target();
}

/* This takes a program previously attached to and detaches it.  After
   this is done, GDB can be used to debug some other program.  We
   better not have left any breakpoints in the target program or it'll
   die when it hits one.  */

static void
remote_detach (args, from_tty)
     char *args;
     int from_tty;
{
  char buf[PBUFSIZ];

  if (args)
    error ("Argument given to \"detach\" when remotely debugging.");

  /* Tell the remote target to detach.  */
  strcpy (buf, "D");
  remote_send (buf);

  pop_target ();
  if (from_tty)
    puts_filtered ("Ending remote debugging.\n");
}

/* Convert hex digit A to a number.  */

static int
fromhex (a)
     int a;
{
  if (a >= '0' && a <= '9')
    return a - '0';
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else 
    error ("Reply contains invalid hex digit %d", a);
}

/* Convert number NIB to a hex digit.  */

static int
tohex (nib)
     int nib;
{
  if (nib < 10)
    return '0'+nib;
  else
    return 'a'+nib-10;
}

/* Tell the remote machine to resume.  */

static enum target_signal last_sent_signal = TARGET_SIGNAL_0;
int last_sent_step;

static void
remote_resume (pid, step, siggnal)
     int pid, step;
     enum target_signal siggnal;
{
  char buf[PBUFSIZ];

  if (pid == -1)
    set_thread (inferior_pid, 0);
  else
    set_thread (pid, 0);

  dcache_flush (remote_dcache);

  last_sent_signal = siggnal;
  last_sent_step = step;

  if (siggnal != TARGET_SIGNAL_0)
    {
      buf[0] = step ? 'S' : 'C';
      buf[1] = tohex (((int)siggnal >> 4) & 0xf);
      buf[2] = tohex ((int)siggnal & 0xf);
      buf[3] = '\0';
    }
  else
    strcpy (buf, step ? "s": "c");

  putpkt (buf);
}

/* Send ^C to target to halt it.  Target will respond, and send us a
   packet.  */

static void
remote_interrupt (signo)
     int signo;
{
  /* If this doesn't work, try more severe steps.  */
  signal (signo, remote_interrupt_twice);
  
  if (remote_debug)
    printf_unfiltered ("remote_interrupt called\n");

  /* Send a break or a ^C, depending on user preference.  */
  if (remote_break)
    SERIAL_SEND_BREAK (remote_desc);
  else
    SERIAL_WRITE (remote_desc, "\003", 1);
}

static void (*ofunc)();

/* The user typed ^C twice.  */
static void
remote_interrupt_twice (signo)
     int signo;
{
  signal (signo, ofunc);
  
  interrupt_query ();

  signal (signo, remote_interrupt);
}

/* Ask the user what to do when an interrupt is received.  */

static void
interrupt_query ()
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      return_to_top_level (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

/* If nonzero, ignore the next kill.  */
int kill_kludge;

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.
   Returns "pid" (though it's not clear what, if anything, that
   means in the case of this target).  */

static int
remote_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  unsigned char buf[PBUFSIZ];
  int thread_num = -1;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  while (1)
    {
      unsigned char *p;

#ifdef	REMOTE_CHAT
      if (remote_chat)
	  (void) remote_talk();
#endif	/* REMOTE_CHAT */

      ofunc = (void (*)()) signal (SIGINT, remote_interrupt);
      getpkt ((char *) buf, 1);
      signal (SIGINT, ofunc);

      switch (buf[0])
	{
	case 'E':		/* Error of some sort */
	  warning ("Remote failure reply: %s", buf);
	  continue;
	case 'T':		/* Status with PC, SP, FP, ... */
	  {
	    int i;
	    long regno;
	    char regs[MAX_REGISTER_RAW_SIZE];

	    /* Expedited reply, containing Signal, {regno, reg} repeat */
	    /*  format is:  'Tssn...:r...;n...:r...;n...:r...;#cc', where
		ss = signal number
		n... = register number
		r... = register contents
		*/

	    p = &buf[3];	/* after Txx */

	    while (*p)
	      {
		unsigned char *p1;
		char *p_temp;

		regno = strtol ((const char *) p, &p_temp, 16); /* Read the register number */
		p1 = (unsigned char *)p_temp;

		if (p1 == p)
		  {
		    p1 = (unsigned char *) strchr ((const char *) p, ':');
		    if (p1 == NULL)
		      warning ("Malformed packet (missing colon): %s\n\
Packet: '%s'\n",
			       p, buf);
		    if (strncmp ((const char *) p, "thread", p1 - p) == 0)
		      {
			thread_num = strtol ((const char *) ++p1, &p_temp, 16);
			p = (unsigned char *)p_temp;
		      }
		  }
		else
		  {
		    p = p1;

		    if (*p++ != ':')
		      warning ("Malformed packet (missing colon): %s\n\
Packet: '%s'\n",
			       p, buf);

		    if (regno >= NUM_REGS)
		      warning ("Remote sent bad register number %ld: %s\n\
Packet: '%s'\n",
			       regno, p, buf);

		    for (i = 0; i < REGISTER_RAW_SIZE (regno); i++)
		      {
			if (p[0] == 0 || p[1] == 0)
			  warning ("Remote reply is too short: %s", buf);
			regs[i] = fromhex (p[0]) * 16 + fromhex (p[1]);
			p += 2;
		      }
		    supply_register (regno, regs);
		  }

		if (*p++ != ';')
		  warning ("Remote register badly formatted: %s", buf);
	      }
	  }
	  /* fall through */
	case 'S':		/* Old style status, just signal only */
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = (enum target_signal)
	    (((fromhex (buf[1])) << 4) + (fromhex (buf[2])));

	  goto got_status;
	case 'W':		/* Target exited */
	  {
	    /* The remote process exited.  */
	    status->kind = TARGET_WAITKIND_EXITED;
	    status->value.integer = (fromhex (buf[1]) << 4) + fromhex (buf[2]);
	    goto got_status;
	  }
	case 'X':
	  status->kind = TARGET_WAITKIND_SIGNALLED;
	  status->value.sig = (enum target_signal)
	    (((fromhex (buf[1])) << 4) + (fromhex (buf[2])));
	  kill_kludge = 1;

	  goto got_status;
	case 'O':		/* Console output */
 	  for (p = buf + 1; *p; p +=2) 
 	    {
 	      char tb[2];
 	      char c = fromhex (p[0]) * 16 + fromhex (p[1]);
 	      tb[0] = c;
 	      tb[1] = 0;
 	      if (target_output_hook)
 		target_output_hook (tb);
 	      else 
 		fputs_filtered (tb, gdb_stdout);
 	    }
	  continue;
	case '\0':
	  if (last_sent_signal != TARGET_SIGNAL_0)
	    {
	      /* Zero length reply means that we tried 'S' or 'C' and
		 the remote system doesn't support it.  */
	      target_terminal_ours_for_output ();
	      printf_filtered
		("Can't send signals to this remote system.  %s not sent.\n",
		 target_signal_to_name (last_sent_signal));
	      last_sent_signal = TARGET_SIGNAL_0;
	      target_terminal_inferior ();

	      strcpy ((char *) buf, last_sent_step ? "s" : "c");
	      putpkt ((char *) buf);
	      continue;
	    }
	  /* else fallthrough */
	default:
	  warning ("Invalid remote reply: %s", buf);
	  continue;
	}
    }
 got_status:
  if (thread_num != -1)
    {
      /* Initial thread value can only be acquired via wait, so deal with
	 this marker which is used before the first thread value is
	 acquired.  */
      if (inferior_pid == 42000)
	{
	  inferior_pid = thread_num;
	  add_thread (inferior_pid);
	}
      return thread_num;
    }
  return inferior_pid;
}

/* Number of bytes of registers this stub implements.  */
static int register_bytes_found;

/* Read the remote registers into the block REGS.  */
/* Currently we just read all the registers, so we don't use regno.  */
/* ARGSUSED */
static void
remote_fetch_registers (regno)
     int regno;
{
  char buf[PBUFSIZ];
  int i;
  char *p;
  char regs[REGISTER_BYTES];

  set_thread (inferior_pid, 1);

  sprintf (buf, "g");
  remote_send (buf);

  /* Unimplemented registers read as all bits zero.  */
  memset (regs, 0, REGISTER_BYTES);

  /* We can get out of synch in various cases.  If the first character
     in the buffer is not a hex character, assume that has happened
     and try to fetch another packet to read.  */
  while ((buf[0] < '0' || buf[0] > '9')
	 && (buf[0] < 'a' || buf[0] > 'f'))
    {
      if (remote_debug)
	printf_unfiltered ("Bad register packet; fetching a new packet\n");
      getpkt (buf, 0);
    }

  /* Reply describes registers byte by byte, each byte encoded as two
     hex characters.  Suck them all up, then supply them to the
     register cacheing/storage mechanism.  */

  p = buf;
  for (i = 0; i < REGISTER_BYTES; i++)
    {
      if (p[0] == 0)
	break;
      if (p[1] == 0)
	{
	  warning ("Remote reply is of odd length: %s", buf);
	  /* Don't change register_bytes_found in this case, and don't
	     print a second warning.  */
	  goto supply_them;
	}
      regs[i] = fromhex (p[0]) * 16 + fromhex (p[1]);
      p += 2;
    }

  if (i != register_bytes_found)
    {
      register_bytes_found = i;
#ifdef REGISTER_BYTES_OK
      if (!REGISTER_BYTES_OK (i))
	warning ("Remote reply is too short: %s", buf);
#endif
    }

 supply_them:
  for (i = 0; i < NUM_REGS; i++)
    supply_register (i, &regs[REGISTER_BYTE(i)]);
}

/* Prepare to store registers.  Since we may send them all (using a
   'G' request), we have to read out the ones we don't want to change
   first.  */

static void 
remote_prepare_to_store ()
{
  /* Make sure the entire registers array is valid.  */
  read_register_bytes (0, (char *)NULL, REGISTER_BYTES);
}

/* Store register REGNO, or all registers if REGNO == -1, from the contents
   of REGISTERS.  FIXME: ignores errors.  */

static void
remote_store_registers (regno)
     int regno;
{
  char buf[PBUFSIZ];
  int i;
  char *p;

  set_thread (inferior_pid, 1);

  if (regno >= 0 && stub_supports_P)
    {
      /* Try storing a single register.  */
      char *regp;

      sprintf (buf, "P%x=", regno);
      p = buf + strlen (buf);
      regp = &registers[REGISTER_BYTE (regno)];
      for (i = 0; i < REGISTER_RAW_SIZE (regno); ++i)
	{
	  *p++ = tohex ((regp[i] >> 4) & 0xf);
	  *p++ = tohex (regp[i] & 0xf);
	}
      *p = '\0';
      remote_send (buf);
      if (buf[0] != '\0')
	{
	  /* The stub understands the 'P' request.  We are done.  */
	  return;
	}

      /* The stub does not support the 'P' request.  Use 'G' instead,
	 and don't try using 'P' in the future (it will just waste our
	 time).  */
      stub_supports_P = 0;
    }

  buf[0] = 'G';

  /* Command describes registers byte by byte,
     each byte encoded as two hex characters.  */

  p = buf + 1;
  /* remote_prepare_to_store insures that register_bytes_found gets set.  */
  for (i = 0; i < register_bytes_found; i++)
    {
      *p++ = tohex ((registers[i] >> 4) & 0xf);
      *p++ = tohex (registers[i] & 0xf);
    }
  *p = '\0';

  remote_send (buf);
}

/* 
   Use of the data cache *used* to be disabled because it loses for looking at
   and changing hardware I/O ports and the like.  Accepting `volatile'
   would perhaps be one way to fix it.  Another idea would be to use the
   executable file for the text segment (for all SEC_CODE sections?
   For all SEC_READONLY sections?).  This has problems if you want to
   actually see what the memory contains (e.g. self-modifying code,
   clobbered memory, user downloaded the wrong thing).  

   Because it speeds so much up, it's now enabled, if you're playing
   with registers you turn it of (set remotecache 0)
*/

/* Read a word from remote address ADDR and return it.
   This goes through the data cache.  */

#if 0	/* unused? */
static int
remote_fetch_word (addr)
     CORE_ADDR addr;
{
  return dcache_fetch (remote_dcache, addr);
}

/* Write a word WORD into remote address ADDR.
   This goes through the data cache.  */

static void
remote_store_word (addr, word)
     CORE_ADDR addr;
     int word;
{
  dcache_poke (remote_dcache, addr, word);
}
#endif	/* 0 (unused?) */


/* Write memory data directly to the remote machine.
   This does not inform the data cache; the data cache uses this.
   MEMADDR is the address in the remote memory space.
   MYADDR is the address of the buffer in our space.
   LEN is the number of bytes.

   Returns number of bytes transferred, or 0 for error.  */

static int
remote_write_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  char buf[PBUFSIZ];
  int i;
  char *p;
  int done;
  /* Chop the transfer down if necessary */

  done = 0;
  while (done < len)
    {
      int todo = len - done;
      int cando = PBUFSIZ /2 - 32; /* number of bytes that will fit. */
      if (todo > cando)
	todo = cando;

      /* FIXME-32x64: Need a version of print_address_numeric which puts the
	 result in a buffer like sprintf.  */
      sprintf (buf, "M%lx,%x:", (unsigned long) memaddr + done, todo);

      /* We send target system values byte by byte, in increasing byte addresses,
	 each byte encoded as two hex characters.  */

      p = buf + strlen (buf);
      for (i = 0; i < todo; i++)
	{
	  *p++ = tohex ((myaddr[i + done] >> 4) & 0xf);
	  *p++ = tohex (myaddr[i + done] & 0xf);
	}
      *p = '\0';

      putpkt (buf);
      getpkt (buf, 0);

      if (buf[0] == 'E')
	{
	  /* There is no correspondance between what the remote protocol uses
	     for errors and errno codes.  We would like a cleaner way of
	     representing errors (big enough to include errno codes, bfd_error
	     codes, and others).  But for now just return EIO.  */
	  errno = EIO;
	  return 0;
	}
      done += todo;
    }
  return len;
}

/* Read memory data directly from the remote machine.
   This does not use the data cache; the data cache uses this.
   MEMADDR is the address in the remote memory space.
   MYADDR is the address of the buffer in our space.
   LEN is the number of bytes.

   Returns number of bytes transferred, or 0 for error.  */

static int
remote_read_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  char buf[PBUFSIZ];
  int i;
  char *p;
  int done;
  /* Chop transfer down if neccessary */

#if 0
  /* FIXME: This is wrong for larger packets */
  if (len > PBUFSIZ / 2 - 1)
    abort ();
#endif
  done = 0;
  while (done < len)
    {
      int todo = len - done;
      int cando = PBUFSIZ / 2 - 32; /* number of bytes that will fit. */
      if (todo > cando)
	todo = cando;

      /* FIXME-32x64: Need a version of print_address_numeric which puts the
	 result in a buffer like sprintf.  */
      sprintf (buf, "m%lx,%x", (unsigned long) memaddr + done, todo);
      putpkt (buf);
      getpkt (buf, 0);

      if (buf[0] == 'E')
	{
	  /* There is no correspondance between what the remote protocol uses
	     for errors and errno codes.  We would like a cleaner way of
	     representing errors (big enough to include errno codes, bfd_error
	     codes, and others).  But for now just return EIO.  */
	  errno = EIO;
	  return 0;
	}

  /* Reply describes memory byte by byte,
     each byte encoded as two hex characters.  */

      p = buf;
      for (i = 0; i < todo; i++)
	{
	  if (p[0] == 0 || p[1] == 0)
	    /* Reply is short.  This means that we were able to read only part
	       of what we wanted to.  */
	    return i + done;
	  myaddr[i + done] = fromhex (p[0]) * 16 + fromhex (p[1]);
	  p += 2;
	}
      done += todo;
    }
  return len;
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if SHOULD_WRITE is
   nonzero.  Returns length of data written or read; 0 for error.  */

/* ARGSUSED */
static int
remote_xfer_memory(memaddr, myaddr, len, should_write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
     struct target_ops *target;			/* ignored */
{
  return dcache_xfer_memory (remote_dcache, memaddr, myaddr, len, should_write);
}

   
#if 0
/* Enable after 4.12.  */

void
remote_search (len, data, mask, startaddr, increment, lorange, hirange
	       addr_found, data_found)
     int len;
     char *data;
     char *mask;
     CORE_ADDR startaddr;
     int increment;
     CORE_ADDR lorange;
     CORE_ADDR hirange;
     CORE_ADDR *addr_found;
     char *data_found;
{
  if (increment == -4 && len == 4)
    {
      long mask_long, data_long;
      long data_found_long;
      CORE_ADDR addr_we_found;
      char buf[PBUFSIZ];
      long returned_long[2];
      char *p;

      mask_long = extract_unsigned_integer (mask, len);
      data_long = extract_unsigned_integer (data, len);
      sprintf (buf, "t%x:%x,%x", startaddr, data_long, mask_long);
      putpkt (buf);
      getpkt (buf, 0);
      if (buf[0] == '\0')
	{
	  /* The stub doesn't support the 't' request.  We might want to
	     remember this fact, but on the other hand the stub could be
	     switched on us.  Maybe we should remember it only until
	     the next "target remote".  */
	  generic_search (len, data, mask, startaddr, increment, lorange,
			  hirange, addr_found, data_found);
	  return;
	}

      if (buf[0] == 'E')
	/* There is no correspondance between what the remote protocol uses
	   for errors and errno codes.  We would like a cleaner way of
	   representing errors (big enough to include errno codes, bfd_error
	   codes, and others).  But for now just use EIO.  */
	memory_error (EIO, startaddr);
      p = buf;
      addr_we_found = 0;
      while (*p != '\0' && *p != ',')
	addr_we_found = (addr_we_found << 4) + fromhex (*p++);
      if (*p == '\0')
	error ("Protocol error: short return for search");

      data_found_long = 0;
      while (*p != '\0' && *p != ',')
	data_found_long = (data_found_long << 4) + fromhex (*p++);
      /* Ignore anything after this comma, for future extensions.  */

      if (addr_we_found < lorange || addr_we_found >= hirange)
	{
	  *addr_found = 0;
	  return;
	}

      *addr_found = addr_we_found;
      *data_found = store_unsigned_integer (data_we_found, len);
      return;
    }
  generic_search (len, data, mask, startaddr, increment, lorange,
		  hirange, addr_found, data_found);
}
#endif /* 0 */

static void
remote_files_info (ignore)
     struct target_ops *ignore;
{
  puts_filtered ("Debugging a target over a serial line.\n");
}

/* Stuff for dealing with the packets which are part of this protocol.
   See comment at top of file for details.  */

/* Read a single character from the remote end, masking it down to 7 bits. */

static int
readchar (timeout)
     int timeout;
{
  int ch;

  ch = SERIAL_READCHAR (remote_desc, timeout);

  switch (ch)
    {
    case SERIAL_EOF:
      error ("Remote connection closed");
    case SERIAL_ERROR:
      perror_with_name ("Remote communication error");
    case SERIAL_TIMEOUT:
      return ch;
    default:
      return ch & 0x7f;
    }
}

/* Send the command in BUF to the remote machine,
   and read the reply into BUF.
   Report an error if we get an error reply.  */

static void
remote_send (buf)
     char *buf;
{
  putpkt (buf);
  getpkt (buf, 0);

  if (buf[0] == 'E')
    error ("Remote failure reply: %s", buf);
}

/* Send a packet to the remote machine, with error checking.
   The data of the packet is in BUF.  */

static int
putpkt (buf)
     char *buf;
{
  int i;
  unsigned char csum = 0;
  char buf2[PBUFSIZ];
  int cnt = strlen (buf);
  int ch;
  int tcount = 0;
  char *p;

  /* Copy the packet into buffer BUF2, encapsulating it
     and giving it a checksum.  */

  if (cnt > (int) sizeof (buf2) - 5)		/* Prosanity check */
    abort();

  p = buf2;
  *p++ = '$';

  for (i = 0; i < cnt; i++)
    {
      csum += buf[i];
      *p++ = buf[i];
    }
  *p++ = '#';
  *p++ = tohex ((csum >> 4) & 0xf);
  *p++ = tohex (csum & 0xf);

  /* Send it over and over until we get a positive ack.  */

  while (1)
    {
      int started_error_output = 0;

      if (remote_debug)
	{
	  *p = '\0';
	  printf_unfiltered ("Sending packet: %s...", buf2);
	  gdb_flush(gdb_stdout);
	}
      if (SERIAL_WRITE (remote_desc, buf2, p - buf2))
	perror_with_name ("putpkt: write failed");

      /* read until either a timeout occurs (-2) or '+' is read */
      while (1)
	{
	  ch = readchar (remote_timeout);

 	  if (remote_debug)
	    {
	      switch (ch)
		{
		case '+':
		case SERIAL_TIMEOUT:
		case '$':
		  if (started_error_output)
		    {
		      putchar_unfiltered ('\n');
		      started_error_output = 0;
		    }
		}
	    }

	  switch (ch)
	    {
	    case '+':
	      if (remote_debug)
		printf_unfiltered("Ack\n");
	      return 1;
	    case SERIAL_TIMEOUT:
	      tcount ++;
	      if (tcount > 3)
		return 0;
	      break;		/* Retransmit buffer */
	    case '$':
	      {
		char junkbuf[PBUFSIZ];

	      /* It's probably an old response, and we're out of sync.  Just
		 gobble up the packet and ignore it.  */
		getpkt (junkbuf, 0);
		continue;		/* Now, go look for + */
	      }

#ifdef	REMOTE_CHAT
	    case '|':
	      {
		if (!started_error_output)
		    continue;
		/* else fall through */
	      }
#endif	/* REMOTE_CHAT */

	    default:
	      if (remote_debug)
		{
		  if (!started_error_output)
		    {
		      started_error_output = 1;
		      printf_unfiltered ("putpkt: Junk: ");
		    }
		  putchar_unfiltered (ch & 0177);
		}
	      continue;
	    }
	  break;		/* Here to retransmit */
	}

#if 0
      /* This is wrong.  If doing a long backtrace, the user should be
	 able to get out next time we call QUIT, without anything as violent
	 as interrupt_query.  If we want to provide a way out of here
	 without getting to the next QUIT, it should be based on hitting
	 ^C twice as in remote_wait.  */
      if (quit_flag)
	{
	  quit_flag = 0;
	  interrupt_query ();
	}
#endif
    }
}

/* Come here after finding the start of the frame.  Collect the rest into BUF,
   verifying the checksum, length, and handling run-length compression.
   Returns 0 on any error, 1 on success.  */

static int
read_frame (buf)
     char *buf;
{
  unsigned char csum;
  char *bp;
  int c;

  csum = 0;
  bp = buf;

  while (1)
    {
      c = readchar (remote_timeout);

      switch (c)
	{
	case SERIAL_TIMEOUT:
	  if (remote_debug)
	    puts_filtered ("Timeout in mid-packet, retrying\n");
	  return 0;
	case '$':
	  if (remote_debug)
	    puts_filtered ("Saw new packet start in middle of old one\n");
	  return 0;		/* Start a new packet, count retries */
	case '#':
	  {
	    unsigned char pktcsum;

	    *bp = '\000';

	    pktcsum = fromhex (readchar (remote_timeout)) << 4;
	    pktcsum |= fromhex (readchar (remote_timeout));

	    if (csum == pktcsum)
	      return 1;

	    if (remote_debug) 
	      {
		printf_filtered ("Bad checksum, sentsum=0x%x, csum=0x%x, buf=",
				 pktcsum, csum);
		puts_filtered (buf);
		puts_filtered ("\n");
	      }
	    return 0;
	  }
	case '*':		/* Run length encoding */
	  csum += c;
	  c = readchar (remote_timeout);
	  csum += c;
	  c = c - ' ' + 3;	/* Compute repeat count */


	  if (c > 0 && c < 255 && bp + c - 1 < buf + PBUFSIZ - 1)
	    {
	      memset (bp, *(bp - 1), c);
	      bp += c;
	      continue;
	    }

	  *bp = '\0';
	  printf_filtered ("Repeat count %d too large for buffer: ", c);
	  puts_filtered (buf);
	  puts_filtered ("\n");
	  return 0;

	default:
	  if (bp < buf + PBUFSIZ - 1)
	    {
	      *bp++ = c;
	      csum += c;
	      continue;
	    }

	  *bp = '\0';
	  puts_filtered ("Remote packet too long: ");
	  puts_filtered (buf);
	  puts_filtered ("\n");

	  return 0;
	}
    }
}

/* Read a packet from the remote machine, with error checking,
   and store it in BUF.  BUF is expected to be of size PBUFSIZ.
   If FOREVER, wait forever rather than timing out; this is used
   while the target is executing user code.  */

static void
getpkt (buf, forever)
     char *buf;
     int forever;
{
  int c;
  int tries;
  int timeout;
  int val;

  strcpy (buf,"timeout");

  if (forever)
    {
#ifdef MAINTENANCE_CMDS
      timeout = watchdog > 0 ? watchdog : -1;
#else
      timeout = -1;
#endif
    }

  else
    timeout = remote_timeout;

#define MAX_TRIES 3

  for (tries = 1; tries <= MAX_TRIES; tries++)
    {
      /* This can loop forever if the remote side sends us characters
	 continuously, but if it pauses, we'll get a zero from readchar
	 because of timeout.  Then we'll count that as a retry.  */

      /* Note that we will only wait forever prior to the start of a packet.
	 After that, we expect characters to arrive at a brisk pace.  They
	 should show up within remote_timeout intervals.  */

      do
	{
	  c = readchar (timeout);

	  if (c == SERIAL_TIMEOUT)
	    {
#ifdef MAINTENANCE_CMDS
	      if (forever)	/* Watchdog went off.  Kill the target. */
		{
		  target_mourn_inferior ();
		  error ("Watchdog has expired.  Target detached.\n");
		}
#endif
	      if (remote_debug)
		puts_filtered ("Timed out.\n");
	      goto retry;
	    }
	}
      while (c != '$');

      /* We've found the start of a packet, now collect the data.  */

      val = read_frame (buf);

      if (val == 1)
	{
	  if (remote_debug)
	    fprintf_unfiltered (gdb_stderr, "Packet received: %s\n", buf);
	  SERIAL_WRITE (remote_desc, "+", 1);
	  return;
	}

      /* Try the whole thing again.  */
    retry:
      SERIAL_WRITE (remote_desc, "-", 1);
    }

  /* We have tried hard enough, and just can't receive the packet.  Give up. */

  printf_unfiltered ("Ignoring packet error, continuing...\n");
  SERIAL_WRITE (remote_desc, "+", 1);
}

static void
remote_kill ()
{
  /* For some mysterious reason, wait_for_inferior calls kill instead of
     mourn after it gets TARGET_WAITKIND_SIGNALLED.  Work around it.  */
  if (kill_kludge)
    {
      kill_kludge = 0;
      target_mourn_inferior ();
      return;
    }

  /* Use catch_errors so the user can quit from gdb even when we aren't on
     speaking terms with the remote system.  */
  catch_errors (putpkt, "k", "", RETURN_MASK_ERROR);

  /* Don't wait for it to die.  I'm not really sure it matters whether
     we do or not.  For the existing stubs, kill is a noop.  */
  target_mourn_inferior ();
}

static void
remote_mourn ()
{
  remote_mourn_1 (&remote_ops);
}

static void
extended_remote_mourn ()
{
  /* We do _not_ want to mourn the target like this; this will
     remove the extended remote target  from the target stack,
     and the next time the user says "run" it'll fail. 

     FIXME: What is the right thing to do here?  */
#if 0
  remote_mourn_1 (&extended_remote_ops);
#endif
}

/* Worker function for remote_mourn.  */
static void
remote_mourn_1 (target)
     struct target_ops *target;
{
  unpush_target (target);
  generic_mourn_inferior ();
}

/* In the extended protocol we want to be able to do things like
   "run" and have them basically work as expected.  So we need
   a special create_inferior function. 

   FIXME: One day add support for changing the exec file
   we're debugging, arguments and an environment.  */

static void
extended_remote_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  /* Rip out the breakpoints; we'll reinsert them after restarting
     the remote server.  */
  remove_breakpoints ();

  /* Now restart the remote server.  */
  extended_remote_restart ();

  /* Now put the breakpoints back in.  This way we're safe if the
     restart function works via a unix fork on the remote side.  */
  insert_breakpoints ();

  /* Clean up from the last time we were running.  */
  clear_proceed_status ();

  /* Let the remote process run.  */
  proceed (-1, TARGET_SIGNAL_0, 0);
}


#ifdef REMOTE_BREAKPOINT

/* On some machines, e.g. 68k, we may use a different breakpoint instruction
   than other targets.  */
static unsigned char break_insn[] = REMOTE_BREAKPOINT;

#else /* No REMOTE_BREAKPOINT.  */

/* Same old breakpoint instruction.  This code does nothing different
   than mem-break.c.  */
static unsigned char break_insn[] = BREAKPOINT;

#endif /* No REMOTE_BREAKPOINT.  */

/* Insert a breakpoint on targets that don't have any better breakpoint
   support.  We read the contents of the target location and stash it,
   then overwrite it with a breakpoint instruction.  ADDR is the target
   location in the target machine.  CONTENTS_CACHE is a pointer to 
   memory allocated for saving the target contents.  It is guaranteed
   by the caller to be long enough to save sizeof BREAKPOINT bytes (this
   is accomplished via BREAKPOINT_MAX).  */

static int
remote_insert_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  int val;

  val = target_read_memory (addr, contents_cache, sizeof break_insn);

  if (val == 0)
    val = target_write_memory (addr, (char *)break_insn, sizeof break_insn);

  return val;
}

static int
remote_remove_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  return target_write_memory (addr, contents_cache, sizeof break_insn);
}

/* Define the target subroutine names */

struct target_ops remote_ops = {
  "remote",			/* to_shortname */
  "Remote serial target in gdb-specific protocol",	/* to_longname */
  "Use a remote computer via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",  /* to_doc */
  remote_open,			/* to_open */
  remote_close,			/* to_close */
  NULL,				/* to_attach */
  remote_detach,		/* to_detach */
  remote_resume,		/* to_resume */
  remote_wait,			/* to_wait */
  remote_fetch_registers,	/* to_fetch_registers */
  remote_store_registers,	/* to_store_registers */
  remote_prepare_to_store,	/* to_prepare_to_store */
  remote_xfer_memory,		/* to_xfer_memory */
  remote_files_info,		/* to_files_info */
  remote_insert_breakpoint,	/* to_insert_breakpoint */
  remote_remove_breakpoint,	/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  remote_kill,			/* to_kill */
  generic_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  NULL,				/* to_create_inferior */
  remote_mourn,			/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  remote_thread_alive,		/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  NULL,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  NULL,				/* sections */
  NULL,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

struct target_ops extended_remote_ops = {
  "extended-remote",			/* to_shortname */
  "Extended remote serial target in gdb-specific protocol",/* to_longname */
  "Use a remote computer via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",  /* to_doc */
  extended_remote_open,			/* to_open */
  remote_close,			/* to_close */
  NULL,				/* to_attach */
  remote_detach,		/* to_detach */
  remote_resume,		/* to_resume */
  remote_wait,			/* to_wait */
  remote_fetch_registers,	/* to_fetch_registers */
  remote_store_registers,	/* to_store_registers */
  remote_prepare_to_store,	/* to_prepare_to_store */
  remote_xfer_memory,		/* to_xfer_memory */
  remote_files_info,		/* to_files_info */

  remote_insert_breakpoint,	/* to_insert_breakpoint */
  remote_remove_breakpoint,	/* to_remove_breakpoint */

  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  remote_kill,			/* to_kill */
  generic_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  extended_remote_create_inferior,/* to_create_inferior */
  extended_remote_mourn,	/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  remote_thread_alive,		/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  NULL,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  NULL,				/* sections */
  NULL,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

void
_initialize_remote ()
{
  add_target (&remote_ops);
  add_target (&extended_remote_ops);

  add_show_from_set (add_set_cmd ("remotetimeout", no_class,
				  var_integer, (char *)&remote_timeout,
				  "Set timeout value for remote read.\n", &setlist),
		     &showlist);

  add_show_from_set (add_set_cmd ("remotebreak", no_class,
				  var_integer, (char *)&remote_break,
				  "Set whether to send break if interrupted.\n", &setlist),
		     &showlist);

#ifdef	REMOTE_CHAT
  add_show_from_set (add_set_cmd ("remotechat", no_class,
				  var_zinteger, (char *)&remote_chat,
				   "Set remote port interacts with target.\n", &setlist),
		     &showlist);
#endif	/* REMOTE_CHAT */
}

