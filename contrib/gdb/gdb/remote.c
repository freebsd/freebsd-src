/* Remote target communications for serial-line targets in custom GDB protocol
   Copyright 1988, 91, 92, 93, 94, 95, 96, 97, 1998 
   Free Software Foundation, Inc.

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

        write mem       XAA..AA,LLLL:XX..XX
         (binary)                       AA..AA is address,
                                        LLLL is number of bytes,
                                        XX..XX is binary data
        reply           OK              for success
                        ENN             for an error

	continue	cAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	step		sAA..AA		AA..AA is address to resume
					If AA..AA is omitted,
					resume at same address.

	continue with	Csig;AA..AA	Continue with signal sig (hex signal
	signal				number).  If ;AA..AA is omitted, 
					resume at same address.

	step with	Ssig;AA..AA	Like 'C' but step not continue.
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
					can happen at any time while the 
					program is running and the debugger 
					should continue to wait for 
					'W', 'T', etc.

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
#include <ctype.h>
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
#include "gdbthread.h"

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

static int remote_xfer_memory PARAMS ((CORE_ADDR memaddr, char * myaddr,
				       int len, int should_write,
				       struct target_ops * target));

static void remote_prepare_to_store PARAMS ((void));

static void remote_fetch_registers PARAMS ((int regno));

static void remote_resume PARAMS ((int pid, int step,
				   enum target_signal siggnal));

static int remote_start_remote PARAMS ((PTR));

static void remote_open PARAMS ((char *name, int from_tty));

static void extended_remote_open PARAMS ((char *name, int from_tty));

static void remote_open_1 PARAMS ((char *, int, struct target_ops *,
				   int extended_p));

static void remote_close PARAMS ((int quitting));

static void remote_store_registers PARAMS ((int regno));

static void remote_mourn PARAMS ((void));

static void extended_remote_restart PARAMS ((void));

static void extended_remote_mourn PARAMS ((void));

static void extended_remote_create_inferior PARAMS ((char *, char *, char **));

static void remote_mourn_1 PARAMS ((struct target_ops *));

static void remote_send PARAMS ((char *buf));

static int readchar PARAMS ((int timeout));

static int remote_wait PARAMS ((int pid, struct target_waitstatus * status));

static void remote_kill PARAMS ((void));

static int tohex PARAMS ((int nib));

static void remote_detach PARAMS ((char *args, int from_tty));

static void remote_interrupt PARAMS ((int signo));

static void interrupt_query PARAMS ((void));

static void set_thread PARAMS ((int, int));

static int remote_thread_alive PARAMS ((int));

static void get_offsets PARAMS ((void));

static int read_frame PARAMS ((char *));

static int remote_insert_breakpoint PARAMS ((CORE_ADDR, char *));

static int remote_remove_breakpoint PARAMS ((CORE_ADDR, char *));

static int hexnumlen PARAMS ((ULONGEST num));

static void init_remote_ops PARAMS ((void));

static void init_extended_remote_ops PARAMS ((void));

static void remote_stop PARAMS ((void));

static int ishex PARAMS ((int ch, int *val));

static int stubhex PARAMS ((int ch));

static int remote_query PARAMS ((int/*char*/, char *, char *, int *));

static int hexnumstr PARAMS ((char *, ULONGEST));

static CORE_ADDR remote_address_masked PARAMS ((CORE_ADDR));

static void print_packet PARAMS ((char *));

static unsigned long crc32 PARAMS ((unsigned char *, int, unsigned int));

static void compare_sections_command PARAMS ((char *, int));

static void packet_command PARAMS ((char *, int));

static int stub_unpack_int PARAMS ((char *buff, int fieldlength));

char *unpack_varlen_hex PARAMS ((char *buff, int *result));

static char *unpack_nibble PARAMS ((char *buf, int *val));

static char *pack_nibble PARAMS ((char *buf, int nibble));

static char *pack_hex_byte PARAMS ((char *pkt, int/*unsigned char*/ byte));

static char *unpack_byte PARAMS ((char *buf, int *value));

static char *pack_int PARAMS ((char *buf, int value));

static char *unpack_int PARAMS ((char *buf, int *value));

static char *unpack_string PARAMS ((char *src, char *dest, int length));

static char *pack_threadid PARAMS ((char *pkt, threadref *id));

static char *unpack_threadid PARAMS ((char *inbuf, threadref *id));

void int_to_threadref PARAMS ((threadref *id, int value));

static int threadref_to_int PARAMS ((threadref *ref));

static void copy_threadref PARAMS ((threadref *dest, threadref *src));

static int threadmatch PARAMS ((threadref *dest, threadref *src));

static char *pack_threadinfo_request PARAMS ((char *pkt, int mode,
					      threadref *id));

static int remote_unpack_thread_info_response PARAMS ((char *pkt,
						       threadref *expectedref,
						       struct gdb_ext_thread_info *info));


static int remote_get_threadinfo PARAMS ((threadref *threadid,
					  int fieldset,	/*TAG mask */
					  struct gdb_ext_thread_info *info));

static int adapt_remote_get_threadinfo PARAMS ((gdb_threadref *ref,
						int selection,
						struct gdb_ext_thread_info *info));

static char *pack_threadlist_request PARAMS ((char *pkt, int startflag,
					      int threadcount,
					      threadref *nextthread));

static int parse_threadlist_response PARAMS ((char *pkt,
					      int result_limit,
					      threadref *original_echo,
					      threadref *resultlist,
					      int *doneflag));

static int remote_get_threadlist PARAMS ((int startflag,
					  threadref *nextthread,
					  int result_limit,
					  int *done,
					  int *result_count,
					  threadref *threadlist));

typedef int (*rmt_thread_action) (threadref *ref, void *context);

static int remote_threadlist_iterator PARAMS ((rmt_thread_action stepfunction,
					       void *context, int looplimit));

static int remote_newthread_step PARAMS ((threadref *ref, void *context));

static int remote_current_thread PARAMS ((int oldpid));

int remote_find_new_threads PARAMS ((void));

static void record_currthread PARAMS ((int currthread));

static void init_remote_threads PARAMS ((void));

/* exported functions */

extern int fromhex PARAMS ((int a));

extern void getpkt PARAMS ((char *buf, int forever));

extern int putpkt PARAMS ((char *buf));

static int putpkt_binary PARAMS ((char *buf, int cnt));

void remote_console_output PARAMS ((char *));

static void check_binary_download PARAMS ((CORE_ADDR addr));

/* Define the target subroutine names */

void open_remote_target PARAMS ((char *, int, struct target_ops *, int));

void _initialize_remote PARAMS ((void));

/* */

static struct target_ops remote_ops;

static struct target_ops extended_remote_ops;

static struct target_thread_vector remote_thread_vec;

/* This was 5 seconds, which is a long time to sit and wait.
   Unless this is going though some terminal server or multiplexer or
   other form of hairy serial connection, I would think 2 seconds would
   be plenty.  */

/* Changed to allow option to set timeout value.
   was static int remote_timeout = 2; */
extern int remote_timeout;

/* This variable chooses whether to send a ^C or a break when the user
   requests program interruption.  Although ^C is usually what remote
   systems expect, and that is the default here, sometimes a break is
   preferable instead.  */

static int remote_break;

/* Has the user attempted to interrupt the target? If so, then offer
   the user the opportunity to bail out completely if he interrupts
   again. */
static int interrupted_already = 0;

/* Descriptor for I/O to remote machine.  Initialize it to NULL so that
   remote_open knows that we don't have a file open when the program
   starts.  */
static serial_t remote_desc = NULL;

/* This variable (available to the user via "set remotebinarydownload")
   dictates whether downloads are sent in binary (via the 'X' packet).
   We assume that the stub can, and attempt to do it. This will be cleared if
   the stub does not understand it. This switch is still needed, though
   in cases when the packet is supported in the stub, but the connection
   does not allow it (i.e., 7-bit serial connection only). */
static int remote_binary_download = 1;
 
/* Have we already checked whether binary downloads work? */
static int remote_binary_checked;

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

/* This variable sets the number of bytes to be written to the target
   in a single packet.  Normally PBUFSIZ is satisfactory, but some
   targets need smaller values (perhaps because the receiving end
   is slow).  */

static int remote_write_size = PBUFSIZ;

/* This variable sets the number of bits in an address that are to be
   sent in a memory ("M" or "m") packet.  Normally, after stripping
   leading zeros, the entire address would be sent. This variable
   restricts the address to REMOTE_ADDRESS_SIZE bits.  HISTORY: The
   initial implementation of remote.c restricted the address sent in
   memory packets to ``host::sizeof long'' bytes - (typically 32
   bits).  Consequently, for 64 bit targets, the upper 32 bits of an
   address was never sent.  Since fixing this bug may cause a break in
   some remote targets this variable is principly provided to
   facilitate backward compatibility. */

static int remote_address_size;

/* This is the size (in chars) of the first response to the `g' command.  This
   is used to limit the size of the memory read and write commands to prevent
   stub buffers from overflowing.  The size does not include headers and
   trailers, it is only the payload size. */

static int remote_register_buf_size = 0;

/* Should we try the 'P' request?  If this is set to one when the stub
   doesn't support 'P', the only consequence is some unnecessary traffic.  */
static int stub_supports_P = 1;

/* These are pointers to hook functions that may be set in order to
   modify resume/wait behavior for a particular architecture.  */

void (*target_resume_hook) PARAMS ((void));
void (*target_wait_loop_hook) PARAMS ((void));


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

/* These are the threads which we last sent to the remote system.
   -1 for all or -2 for not sent yet.  */
static int general_thread;
static int cont_thread;

/* Call this function as a result of
   1) A halt indication (T packet) containing a thread id
   2) A direct query of currthread
   3) Successful execution of set thread
 */

static void
record_currthread (currthread)
     int currthread;
{
#if 0	/* target_wait must not modify inferior_pid! */
  inferior_pid = currthread;
#endif
  general_thread = currthread;
#if 0	/* setting cont_thread has a different meaning 
	   from having the target report its thread id.  */
  cont_thread = currthread;
#endif
  /* If this is a new thread, add it to GDB's thread list.
     If we leave it up to WFI to do this, bad things will happen.  */
  if (!in_thread_list (currthread))
    add_thread (currthread);
}

#define MAGIC_NULL_PID 42000

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
  if (th == MAGIC_NULL_PID)
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
    sprintf (&buf[1], "-%08x", -th);
  else
    sprintf (&buf[1], "%08x", th);
  putpkt (buf);
  getpkt (buf, 0);
  return (buf[0] == 'O' && buf[1] == 'K');
}

/* About these extended threadlist and threadinfo packets.  They are
   variable length packets but, the fields within them are often fixed
   length.  They are redundent enough to send over UDP as is the
   remote protocol in general.  There is a matching unit test module
   in libstub.  */

#define BUF_THREAD_ID_SIZE (OPAQUETHREADBYTES*2)

/* encode 64 bits in 16 chars of hex */

static const char hexchars[] = "0123456789abcdef";

static int
ishex (ch, val)
     int ch;
     int *val;
{
  if ((ch >= 'a') && (ch <= 'f'))
    {
      *val = ch - 'a' + 10;
      return 1;
    }
  if ((ch >= 'A') && (ch <= 'F'))
    {
      *val = ch - 'A' + 10;
      return 1;
    }
  if ((ch >= '0') && (ch <= '9'))
    {
      *val = ch - '0';
      return 1;
    }
  return 0;
}

static int
stubhex (ch)
     int ch;
{
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  return -1;
}

static int
stub_unpack_int (buff, fieldlength)
     char *buff;
     int fieldlength;
{
  int nibble;
  int retval = 0;

  while (fieldlength)
    {
      nibble = stubhex (*buff++);
      retval |= nibble;
      fieldlength--;
      if (fieldlength)
	retval = retval << 4;
    }
  return retval;
}

char *
unpack_varlen_hex (buff, result)
     char *buff;		/* packet to parse */
     int *result;
{
  int nibble;
  int retval = 0;

  while (ishex (*buff, &nibble))
    {
      buff++;
      retval = retval << 4;
      retval |= nibble & 0x0f;
    }
  *result = retval;
  return buff;
}

static char *
unpack_nibble (buf, val)
     char *buf;
     int *val;
{
  ishex (*buf++, val);
  return buf;
}

static char *
pack_nibble (buf, nibble)
     char *buf;
     int nibble;
{
  *buf++ = hexchars[(nibble & 0x0f)];
  return buf;
}

static char *
pack_hex_byte (pkt, byte)
     char *pkt;
     int byte;
{
  *pkt++ = hexchars[(byte >> 4) & 0xf];
  *pkt++ = hexchars[(byte & 0xf)];
  return pkt;
}

static char *
unpack_byte (buf, value)
     char *buf;
     int *value;
{
  *value = stub_unpack_int (buf, 2);
  return buf + 2;
}

static char *
pack_int (buf, value)
     char *buf;
     int value;
{
  buf = pack_hex_byte (buf, (value >> 24) & 0xff);
  buf = pack_hex_byte (buf, (value >> 16) & 0xff);
  buf = pack_hex_byte (buf, (value >> 8) & 0x0ff);
  buf = pack_hex_byte (buf, (value & 0xff));
  return buf;
}

static char *
unpack_int (buf, value)
     char *buf;
     int *value;
{
  *value = stub_unpack_int (buf, 8);
  return buf + 8;
}

#if 0 /* currently unused, uncomment when needed */
static char *pack_string PARAMS ((char *pkt, char *string));

static char *
pack_string (pkt, string)
     char *pkt;
     char *string;
{
  char ch;
  int len;

  len = strlen (string);
  if (len > 200)
    len = 200;			/* Bigger than most GDB packets, junk??? */
  pkt = pack_hex_byte (pkt, len);
  while (len-- > 0)
    {
      ch = *string++;
      if ((ch == '\0') || (ch == '#'))
	ch = '*';		/* Protect encapsulation */
      *pkt++ = ch;
    }
  return pkt;
}
#endif /* 0 (unused) */

static char *
unpack_string (src, dest, length)
     char *src;
     char *dest;
     int length;
{
  while (length--)
    *dest++ = *src++;
  *dest = '\0';
  return src;
}

static char *
pack_threadid (pkt, id)
     char *pkt;
     threadref *id;
{
  char *limit;
  unsigned char *altid;

  altid = (unsigned char *) id;
  limit = pkt + BUF_THREAD_ID_SIZE;
  while (pkt < limit)
    pkt = pack_hex_byte (pkt, *altid++);
  return pkt;
}


static char *
unpack_threadid (inbuf, id)
     char *inbuf;
     threadref *id;
{
  char *altref;
  char *limit = inbuf + BUF_THREAD_ID_SIZE;
  int x, y;

  altref = (char *) id;

  while (inbuf < limit)
    {
      x = stubhex (*inbuf++);
      y = stubhex (*inbuf++);
      *altref++ = (x << 4) | y;
    }
  return inbuf;
}

/* Externally, threadrefs are 64 bits but internally, they are still
   ints. This is due to a mismatch of specifications.  We would like
   to use 64bit thread references internally.  This is an adapter
   function.  */

void
int_to_threadref (id, value)
     threadref *id;
     int value;
{
  unsigned char *scan;

  scan = (unsigned char *) id;
  {
    int i = 4;
    while (i--)
      *scan++ = 0;
  }
  *scan++ = (value >> 24) & 0xff;
  *scan++ = (value >> 16) & 0xff;
  *scan++ = (value >> 8) & 0xff;
  *scan++ = (value & 0xff);
}

static int
threadref_to_int (ref)
     threadref *ref;
{
  int i, value = 0;
  unsigned char *scan;

  scan = (char *) ref;
  scan += 4;
  i = 4;
  while (i-- > 0)
    value = (value << 8) | ((*scan++) & 0xff);
  return value;
}

static void
copy_threadref (dest, src)
     threadref *dest;
     threadref *src;
{
  int i;
  unsigned char *csrc, *cdest;

  csrc = (unsigned char *) src;
  cdest = (unsigned char *) dest;
  i = 8;
  while (i--)
    *cdest++ = *csrc++;
}

static int
threadmatch (dest, src)
     threadref *dest;
     threadref *src;
{
  /* things are broken right now, so just assume we got a match */
#if 0
  unsigned char *srcp, *destp;
  int i, result;
  srcp = (char *) src;
  destp = (char *) dest;

  result = 1;
  while (i-- > 0)
    result &= (*srcp++ == *destp++) ? 1 : 0;
  return result;
#endif
  return 1;
}

/*
  threadid:1,        # always request threadid
  context_exists:2,
  display:4,
  unique_name:8,
  more_display:16
*/

/* Encoding:  'Q':8,'P':8,mask:32,threadid:64 */

static char *
pack_threadinfo_request (pkt, mode, id)
     char *pkt;
     int mode;
     threadref *id;
{
  *pkt++ = 'q';			/* Info Query */
  *pkt++ = 'P';			/* process or thread info */
  pkt = pack_int (pkt, mode);	/* mode */
  pkt = pack_threadid (pkt, id);	/* threadid */
  *pkt = '\0';			/* terminate */
  return pkt;
}

/* These values tag the fields in a thread info response packet */
/* Tagging the fields allows us to request specific fields and to
   add more fields as time goes by */

#define TAG_THREADID 1      /* Echo the thread identifier */
#define TAG_EXISTS 2        /* Is this process defined enough to
			       fetch registers and its stack */
#define TAG_DISPLAY 4       /* A short thing maybe to put on a window */
#define TAG_THREADNAME 8    /* string, maps 1-to-1 with a thread is */
#define TAG_MOREDISPLAY 16  /* Whatever the kernel wants to say about 
			       the process*/

static int
remote_unpack_thread_info_response (pkt, expectedref, info)
     char *pkt;
     threadref *expectedref;
     struct gdb_ext_thread_info *info;
{
  int mask, length;
  unsigned int tag;
  threadref ref;
  char *limit = pkt + PBUFSIZ;  /* plausable parsing limit */
  int retval = 1;

  /* info->threadid = 0; FIXME: implement zero_threadref */
  info->active = 0;
  info->display[0] = '\0';
  info->shortname[0] = '\0';
  info->more_display[0] = '\0';

  /* Assume the characters indicating the packet type have been stripped */
  pkt = unpack_int (pkt, &mask);	/* arg mask */
  pkt = unpack_threadid (pkt, &ref);

  if (mask == 0)
    warning ("Incomplete response to threadinfo request\n");
  if (!threadmatch (&ref, expectedref))
    {				/* This is an answer to a different request */
      warning ("ERROR RMT Thread info mismatch\n");
      return 0;
    }
  copy_threadref (&info->threadid, &ref);

  /* Loop on tagged fields , try to bail if somthing goes wrong */

  while ((pkt < limit) && mask && *pkt)	/* packets are terminated with nulls */
    {
      pkt = unpack_int (pkt, &tag);	/* tag */
      pkt = unpack_byte (pkt, &length);		/* length */
      if (!(tag & mask))	/* tags out of synch with mask */
	{
	  warning ("ERROR RMT: threadinfo tag mismatch\n");
	  retval = 0;
	  break;
	}
      if (tag == TAG_THREADID)
	{
	  if (length != 16)
	    {
	      warning ("ERROR RMT: length of threadid is not 16\n");
	      retval = 0;
	      break;
	    }
	  pkt = unpack_threadid (pkt, &ref);
	  mask = mask & ~TAG_THREADID;
	  continue;
	}
      if (tag == TAG_EXISTS)
	{
	  info->active = stub_unpack_int (pkt, length);
	  pkt += length;
	  mask = mask & ~(TAG_EXISTS);
	  if (length > 8)
	    {
	      warning ("ERROR RMT: 'exists' length too long\n");
	      retval = 0;
	      break;
	    }
	  continue;
	}
      if (tag == TAG_THREADNAME)
	{
	  pkt = unpack_string (pkt, &info->shortname[0], length);
	  mask = mask & ~TAG_THREADNAME;
	  continue;
	}
      if (tag == TAG_DISPLAY)
	{
	  pkt = unpack_string (pkt, &info->display[0], length);
	  mask = mask & ~TAG_DISPLAY;
	  continue;
	}
      if (tag == TAG_MOREDISPLAY)
	{
	  pkt = unpack_string (pkt, &info->more_display[0], length);
	  mask = mask & ~TAG_MOREDISPLAY;
	  continue;
	}
      warning ("ERROR RMT: unknown thread info tag\n");
      break;			/* Not a tag we know about */
    }
  return retval;
}

static int
remote_get_threadinfo (threadid, fieldset, info)
     threadref *threadid;
     int fieldset;		/* TAG mask */
     struct gdb_ext_thread_info *info;
{
  int result;
  char threadinfo_pkt[PBUFSIZ];

  pack_threadinfo_request (threadinfo_pkt, fieldset, threadid);
  putpkt (threadinfo_pkt);
  getpkt (threadinfo_pkt, 0);
  result = remote_unpack_thread_info_response (threadinfo_pkt + 2, threadid,
					       info);
  return result;
}

/* Unfortunately, 61 bit thread-ids are bigger than the internal
   representation of a threadid.  */

static int
adapt_remote_get_threadinfo (ref, selection, info)
     gdb_threadref *ref;
     int selection;
     struct gdb_ext_thread_info *info;
{
  threadref lclref;

  int_to_threadref (&lclref, *ref);
  return remote_get_threadinfo (&lclref, selection, info);
}

/*    Format: i'Q':8,i"L":8,initflag:8,batchsize:16,lastthreadid:32   */

static char *
pack_threadlist_request (pkt, startflag, threadcount, nextthread)
     char *pkt;
     int startflag;
     int threadcount;
     threadref *nextthread;
{
  *pkt++ = 'q';			/* info query packet */
  *pkt++ = 'L';			/* Process LIST or threadLIST request */
  pkt = pack_nibble (pkt, startflag);	/* initflag 1 bytes */
  pkt = pack_hex_byte (pkt, threadcount);	/* threadcount 2 bytes */
  pkt = pack_threadid (pkt, nextthread);	/* 64 bit thread identifier */
  *pkt = '\0';
  return pkt;
}

/* Encoding:   'q':8,'M':8,count:16,done:8,argthreadid:64,(threadid:64)* */

static int
parse_threadlist_response (pkt, result_limit, original_echo, resultlist,
			   doneflag)
     char *pkt;
     int result_limit;
     threadref *original_echo;
     threadref *resultlist;
     int *doneflag;
{
  char *limit;
  int count, resultcount, done;

  resultcount = 0;
  /* Assume the 'q' and 'M chars have been stripped.  */
  limit = pkt + (PBUFSIZ - BUF_THREAD_ID_SIZE);	/* done parse past here */
  pkt = unpack_byte (pkt, &count);	/* count field */
  pkt = unpack_nibble (pkt, &done);
  /* The first threadid is the argument threadid.  */
  pkt = unpack_threadid (pkt, original_echo);	/* should match query packet */
  while ((count-- > 0) && (pkt < limit))
    {
      pkt = unpack_threadid (pkt, resultlist++);
      if (resultcount++ >= result_limit)
	break;
    }
  if (doneflag)
    *doneflag = done;
  return resultcount;
}

static int
remote_get_threadlist (startflag, nextthread, result_limit,
		       done, result_count, threadlist)
     int startflag;
     threadref *nextthread;
     int result_limit;
     int *done;
     int *result_count;
     threadref *threadlist;

{
  static threadref echo_nextthread;
  char threadlist_packet[PBUFSIZ];
  char t_response[PBUFSIZ];
  int result = 1;

  /* Trancate result limit to be smaller than the packet size */
  if ((((result_limit + 1) * BUF_THREAD_ID_SIZE) + 10) >= PBUFSIZ)
    result_limit = (PBUFSIZ / BUF_THREAD_ID_SIZE) - 2;

  pack_threadlist_request (threadlist_packet,
			   startflag, result_limit, nextthread);
  putpkt (threadlist_packet);
  getpkt (t_response, 0);

  *result_count =
    parse_threadlist_response (t_response + 2, result_limit, &echo_nextthread,
			       threadlist, done);

  if (!threadmatch (&echo_nextthread, nextthread))
    {
      /* FIXME: This is a good reason to drop the packet */
      /* Possably, there is a duplicate response */
      /* Possabilities :
         retransmit immediatly - race conditions
         retransmit after timeout - yes
         exit
         wait for packet, then exit
       */
      warning ("HMM: threadlist did not echo arg thread, dropping it\n");
      return 0;			/* I choose simply exiting */
    }
  if (*result_count <= 0)
    {
      if (*done != 1)
	{
	  warning ("RMT ERROR : failed to get remote thread list\n");
	  result = 0;
	}
      return result;		/* break; */
    }
  if (*result_count > result_limit)
    {
      *result_count = 0;
      warning ("RMT ERROR: threadlist response longer than requested\n");
      return 0;
    }
  return result;
}

/* This is the interface between remote and threads, remotes upper interface */

/* remote_find_new_threads retrieves the thread list and for each
   thread in the list, looks up the thread in GDB's internal list,
   ading the thread if it does not already exist.  This involves
   getting partial thread lists from the remote target so, polling the
   quit_flag is required.  */


/* About this many threadisds fit in a packet. */

#define MAXTHREADLISTRESULTS 32

static int
remote_threadlist_iterator (stepfunction, context, looplimit)
     rmt_thread_action stepfunction;
     void *context;
     int looplimit;
{
  int done, i, result_count;
  int startflag = 1;
  int result = 1;
  int loopcount = 0;
  static threadref nextthread;
  static threadref resultthreadlist[MAXTHREADLISTRESULTS];

  done = 0;
  while (!done)
    {
      if (loopcount++ > looplimit)
	{
	  result = 0;
	  warning ("Remote fetch threadlist -infinite loop-\n");
	  break;
	}
      if (!remote_get_threadlist (startflag, &nextthread, MAXTHREADLISTRESULTS,
				  &done, &result_count, resultthreadlist))
	{
	  result = 0;
	  break;
	}
      /* clear for later iterations */
      startflag = 0;
      /* Setup to resume next batch of thread references, set nextthread.  */
      if (result_count >= 1)
	copy_threadref (&nextthread, &resultthreadlist[result_count - 1]);
      i = 0;
      while (result_count--)
	if (!(result = (*stepfunction) (&resultthreadlist[i++], context)))
	  break;
    }
  return result;
}

static int
remote_newthread_step (ref, context)
     threadref *ref;
     void *context;
{
  int pid;

  pid = threadref_to_int (ref);
  if (!in_thread_list (pid))
    add_thread (pid);
  return 1;			/* continue iterator */
}

#define CRAZY_MAX_THREADS 1000

static int
remote_current_thread (oldpid)
     int oldpid;
{
  char buf[PBUFSIZ];

  putpkt ("qC");
  getpkt (buf, 0);
  if (buf[0] == 'Q' && buf[1] == 'C')
    return strtol (&buf[2], NULL, 16);
  else
    return oldpid;
}

int
remote_find_new_threads ()
{
  int ret;

  ret = remote_threadlist_iterator (remote_newthread_step, 0, 
				    CRAZY_MAX_THREADS);
  if (inferior_pid == MAGIC_NULL_PID)	/* ack ack ack */
    inferior_pid = remote_current_thread (inferior_pid);
  return ret;
}

/* Initialize the thread vector which is used by threads.c */
/* The thread stub is a package, it has an initializer */

static void
init_remote_threads ()
{
  remote_thread_vec.find_new_threads = remote_find_new_threads;
  remote_thread_vec.get_thread_info = adapt_remote_get_threadinfo;
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
  char buf[PBUFSIZ], *ptr;
  int lose;
  CORE_ADDR text_addr, data_addr, bss_addr;
  struct section_offsets *offs;

#ifdef	REMOTE_CHAT
      if (remote_chat)
	  (void) remote_talk();
#endif	/* REMOTE_CHAT */

  putpkt ("qOffsets");

  getpkt (buf, 0);

  if (buf[0] == '\000')
    return;			/* Return silently.  Stub doesn't support
				   this command. */
  if (buf[0] == 'E')
    {
      warning ("Remote failure reply: %s", buf);
      return;
    }

  /* Pick up each field in turn.  This used to be done with scanf, but
     scanf will make trouble if CORE_ADDR size doesn't match
     conversion directives correctly.  The following code will work
     with any size of CORE_ADDR.  */
  text_addr = data_addr = bss_addr = 0;
  ptr = buf;
  lose = 0;

  if (strncmp (ptr, "Text=", 5) == 0)
    {
      ptr += 5;
      /* Don't use strtol, could lose on big values.  */
      while (*ptr && *ptr != ';')
	text_addr = (text_addr << 4) + fromhex (*ptr++);
    }
  else
    lose = 1;

  if (!lose && strncmp (ptr, ";Data=", 6) == 0)
    {
      ptr += 6;
      while (*ptr && *ptr != ';')
	data_addr = (data_addr << 4) + fromhex (*ptr++);
    }
  else
    lose = 1;

  if (!lose && strncmp (ptr, ";Bss=", 5) == 0)
    {
      ptr += 5;
      while (*ptr && *ptr != ';')
	bss_addr = (bss_addr << 4) + fromhex (*ptr++);
    }
  else
    lose = 1;

  if (lose)
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
     PTR dummy;
{
  immediate_quit = 1;		/* Allow user to interrupt it */

  /* Ack any packet which the remote side has already sent.  */
  SERIAL_WRITE (remote_desc, "+", 1);

  /* Let the stub know that we want it to return the thread.  */
  set_thread (-1, 0);

  inferior_pid = remote_current_thread (inferior_pid);

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
  remote_open_1 (name, from_tty, &remote_ops, 0);
}

/* Open a connection to a remote debugger using the extended
   remote gdb protocol.  NAME is the filename used for communication.  */

static void
extended_remote_open (name, from_tty)
     char *name;
     int from_tty;
{
  remote_open_1 (name, from_tty, &extended_remote_ops, 1/*extended_p*/);
}

/* Generic code for opening a connection to a remote target.  */

static DCACHE *remote_dcache;

static void
remote_open_1 (name, from_tty, target, extended_p)
     char *name;
     int from_tty;
     struct target_ops *target;
     int extended_p;
{
  if (name == 0)
    error ("To open a remote debug connection, you need to specify what\n\
serial device is attached to the remote system (e.g. /dev/ttya).");

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

  /* The target vector does not have the thread functions in it yet,
     so we use this function to call back into the thread module and
     register the thread vector and its contained functions. */
  bind_target_thread_vector (&remote_thread_vec);

  /* Start out by trying the 'P' request to set registers.  We set
     this each time that we open a new target so that if the user
     switches from one stub to another, we can (if the target is
     closed and reopened) cope.  */
  stub_supports_P = 1;

  general_thread = -2;
  cont_thread = -2;

  /* Force remote_write_bytes to check whether target supports
     binary downloading. */
  remote_binary_checked = 0;

  /* Without this, some commands which require an active target (such
     as kill) won't work.  This variable serves (at least) double duty
     as both the pid of the target process (if it has such), and as a
     flag indicating that a target is active.  These functions should
     be split out into seperate variables, especially since GDB will
     someday have a notion of debugging several processes.  */

  inferior_pid = MAGIC_NULL_PID;
  /* Start the remote connection; if error (0), discard this target.
     In particular, if the user quits, be sure to discard it
     (we'd be in an inconsistent state otherwise).  */
  if (!catch_errors (remote_start_remote, NULL, 
		     "Couldn't establish connection to remote target\n", 
		     RETURN_MASK_ALL))
    {
      pop_target ();
      return;
    }

  if (extended_p)
    {
      /* tell the remote that we're using the extended protocol.  */
      char buf[PBUFSIZ];
      putpkt ("!");
      getpkt (buf, 0);
    }
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

int
fromhex (a)
     int a;
{
  if (a >= '0' && a <= '9')
    return a - '0';
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
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

static int last_sent_step;

static void
remote_resume (pid, step, siggnal)
     int pid, step;
     enum target_signal siggnal;
{
  char buf[PBUFSIZ];

  if (pid == -1)
    set_thread (0, 0);		/* run any thread */
  else
    set_thread (pid, 0);	/* run this thread */

  dcache_flush (remote_dcache);

  last_sent_signal = siggnal;
  last_sent_step = step;

  /* A hook for when we need to do something at the last moment before
     resumption.  */
  if (target_resume_hook)
    (*target_resume_hook) ();

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

static void (*ofunc) PARAMS ((int));

static void
remote_interrupt (signo)
     int signo;
{
  remote_stop ();
  signal (signo, remote_interrupt);
}
  
static void
remote_stop ()
{
  if (!interrupted_already)
    {
      /* Send a break or a ^C, depending on user preference.  */
      interrupted_already = 1;

      if (remote_debug)
        printf_unfiltered ("remote_stop called\n");

      if (remote_break)
        SERIAL_SEND_BREAK (remote_desc);
      else
        SERIAL_WRITE (remote_desc, "\003", 1);
    }
  else
    {
      signal (SIGINT, ofunc);
      interrupt_query ();
      signal (SIGINT, remote_interrupt);
      interrupted_already = 0;
    }
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

void
remote_console_output (msg)
     char *msg;
{
  char *p;

  for (p = msg; *p; p +=2) 
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
}

/* Wait until the remote machine stops, then return, storing status in
   STATUS just as `wait' would.  Returns "pid" (though it's not clear
   what, if anything, that means in the case of this target).  */

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

      interrupted_already = 0;
      ofunc = signal (SIGINT, remote_interrupt);
      getpkt ((char *) buf, 1);
      signal (SIGINT, ofunc);

      /* This is a hook for when we need to do something (perhaps the
	 collection of trace data) every time the target stops.  */
      if (target_wait_loop_hook)
	(*target_wait_loop_hook) ();

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

		/* Read the register number */
		regno = strtol ((const char *) p, &p_temp, 16);
		p1 = (unsigned char *)p_temp;

		if (p1 == p) /* No register number present here */
		  {
		    p1 = (unsigned char *) strchr ((const char *) p, ':');
		    if (p1 == NULL)
		      warning ("Malformed packet(a) (missing colon): %s\n\
Packet: '%s'\n",
			       p, buf);
		    if (strncmp ((const char *) p, "thread", p1 - p) == 0)
		      {
			p_temp = unpack_varlen_hex (++p1, &thread_num);
			record_currthread (thread_num);
			p = (unsigned char *) p_temp;
		      }
		  }
		else
		  {
		    p = p1;

		    if (*p++ != ':')
		      warning ("Malformed packet(b) (missing colon): %s\n\
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
		  {
		    warning ("Remote register badly formatted: %s", buf);
		    warning ("            here: %s",p);
		  }
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
	  remote_console_output (buf + 1);
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
      if (inferior_pid == MAGIC_NULL_PID)
	{
	  inferior_pid = thread_num;
	  if (!in_thread_list (inferior_pid))
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

  if (remote_register_buf_size == 0)
    remote_register_buf_size = strlen (buf);

  /* Unimplemented registers read as all bits zero.  */
  memset (regs, 0, REGISTER_BYTES);

  /* We can get out of synch in various cases.  If the first character
     in the buffer is not a hex character, assume that has happened
     and try to fetch another packet to read.  */
  while ((buf[0] < '0' || buf[0] > '9')
	 && (buf[0] < 'a' || buf[0] > 'f')
	 && buf[0] != 'x')	/* New: unavailable register value */
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
      if (p[0] == 'x' && p[1] == 'x')
	regs[i] = 0;	/* 'x' */
      else
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
  {
    supply_register (i, &regs[REGISTER_BYTE(i)]);
    if (buf[REGISTER_BYTE(i) * 2] == 'x')
      register_valid[i] = -1;	/* register value not available */
  }
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

/* Use of the data cache *used* to be disabled because it loses for looking
   at and changing hardware I/O ports and the like.  Accepting `volatile'
   would perhaps be one way to fix it.  Another idea would be to use the
   executable file for the text segment (for all SEC_CODE sections?
   For all SEC_READONLY sections?).  This has problems if you want to
   actually see what the memory contains (e.g. self-modifying code,
   clobbered memory, user downloaded the wrong thing).  

   Because it speeds so much up, it's now enabled, if you're playing
   with registers you turn it of (set remotecache 0).  */

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



/* Return the number of hex digits in num.  */

static int
hexnumlen (num)
     ULONGEST num;
{
  int i;

  for (i = 0; num != 0; i++)
    num >>= 4;

  return max (i, 1);
}

/* Set BUF to the hex digits representing NUM.  */

static int
hexnumstr (buf, num)
     char *buf;
     ULONGEST num;
{
  int i;
  int len = hexnumlen (num);

  buf[len] = '\0';

  for (i = len - 1; i >= 0; i--)
    {
      buf[i] = "0123456789abcdef" [(num & 0xf)];
      num >>= 4;
    }

  return len;
}

/* Mask all but the least significant REMOTE_ADDRESS_SIZE bits. */

static CORE_ADDR
remote_address_masked (addr)
     CORE_ADDR addr;
{
  if (remote_address_size > 0
      && remote_address_size < (sizeof (ULONGEST) * 8))
    {
      /* Only create a mask when that mask can safely be constructed
         in a ULONGEST variable. */
      ULONGEST mask = 1;
      mask = (mask << remote_address_size) - 1;
      addr &= mask;
    }
  return addr;
}

/* Determine whether the remote target supports binary downloading.
   This is accomplished by sending a no-op memory write of zero length
   to the target at the specified address. It does not suffice to send
   the whole packet, since many stubs strip the eighth bit and subsequently
   compute a wrong checksum, which causes real havoc with remote_write_bytes. */
static void
check_binary_download (addr)
     CORE_ADDR addr;
{
  if (remote_binary_download && !remote_binary_checked)
    {
      char buf[PBUFSIZ], *p;
      remote_binary_checked = 1;

      p = buf;
      *p++ = 'X';
      p += hexnumstr (p, (ULONGEST) addr);
      *p++ = ',';
      p += hexnumstr (p, (ULONGEST) 0);
      *p++ = ':';
      *p = '\0';

      putpkt_binary (buf, (int) (p - buf));
      getpkt (buf, 0);

      if (buf[0] == '\0')
	remote_binary_download = 0;
    }

  if (remote_debug)
    {
      if (remote_binary_download)
	printf_unfiltered ("binary downloading suppported by target\n");
      else
	printf_unfiltered ("binary downloading NOT suppported by target\n");
    }
}

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
  int max_buf_size;		/* Max size of packet output buffer */
  int origlen;

  /* Verify that the target can support a binary download */
  check_binary_download (memaddr);

  /* Chop the transfer down if necessary */

  max_buf_size = min (remote_write_size, PBUFSIZ);
  if (remote_register_buf_size != 0)
    max_buf_size = min (max_buf_size, remote_register_buf_size);

  /* Subtract header overhead from max payload size - $M<memaddr>,<len>:#nn */
  max_buf_size -= 2 + hexnumlen (memaddr + len - 1) + 1 + hexnumlen (len) + 4;

  origlen = len;
  while (len > 0)
    {
      unsigned char buf[PBUFSIZ];
      unsigned char *p, *plen;
      int todo;
      int i;

      /* construct "M"<memaddr>","<len>":" */
      /* sprintf (buf, "M%lx,%x:", (unsigned long) memaddr, todo); */
      memaddr = remote_address_masked (memaddr);
      p = buf;
      if (remote_binary_download)
	{
	  *p++ = 'X';
	  todo = min (len, max_buf_size);
	}
      else
	{
	  *p++ = 'M';
	  todo = min (len, max_buf_size / 2); /* num bytes that will fit */
	}

      p += hexnumstr (p, (ULONGEST) memaddr);
      *p++ = ',';

      plen = p;		/* remember where len field goes */
      p += hexnumstr (p, (ULONGEST) todo);
      *p++ = ':';
      *p = '\0';

      /* We send target system values byte by byte, in increasing byte
	 addresses, each byte encoded as two hex characters (or one
	 binary character).  */
      if (remote_binary_download)
	{
          int escaped = 0;
          for (i = 0; 
	       (i < todo) && (i + escaped) < (max_buf_size - 2);
	       i++) 
            {
              switch (myaddr[i] & 0xff)
                {
                case '$':
                case '#':
                case 0x7d:
                  /* These must be escaped */
                  escaped++;
                  *p++ = 0x7d;
                  *p++ = (myaddr[i] & 0xff) ^ 0x20;
                  break;
                default:
                  *p++ = myaddr[i] & 0xff;
                  break;
                }
            }

	  if (i < todo)
	    {
	      /* Escape chars have filled up the buffer prematurely, 
		 and we have actually sent fewer bytes than planned.
		 Fix-up the length field of the packet.  */

	      /* FIXME: will fail if new len is a shorter string than 
		 old len.  */

	      plen += hexnumstr (plen, (ULONGEST) i);
	      *plen++ = ':';
	    }
	}
      else
	{
	  for (i = 0; i < todo; i++)
	    {
	      *p++ = tohex ((myaddr[i] >> 4) & 0xf);
	      *p++ = tohex (myaddr[i] & 0xf);
	    }
	  *p = '\0';
	}

      putpkt_binary (buf, (int) (p - buf));
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

      /* Increment by i, not by todo, in case escape chars 
	 caused us to send fewer bytes than we'd planned.  */
      myaddr  += i;
      memaddr += i;
      len     -= i;
    }
  return origlen;
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
  int max_buf_size;		/* Max size of packet output buffer */
  int origlen;

  /* Chop the transfer down if necessary */

  max_buf_size = min (remote_write_size, PBUFSIZ);
  if (remote_register_buf_size != 0)
    max_buf_size = min (max_buf_size, remote_register_buf_size);

  origlen = len;
  while (len > 0)
    {
      char buf[PBUFSIZ];
      char *p;
      int todo;
      int i;

      todo = min (len, max_buf_size / 2); /* num bytes that will fit */

      /* construct "m"<memaddr>","<len>" */
      /* sprintf (buf, "m%lx,%x", (unsigned long) memaddr, todo); */
      memaddr = remote_address_masked (memaddr);
      p = buf;
      *p++ = 'm';
      p += hexnumstr (p, (ULONGEST) memaddr);
      *p++ = ',';
      p += hexnumstr (p, (ULONGEST) todo);
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

  /* Reply describes memory byte by byte,
     each byte encoded as two hex characters.  */

      p = buf;
      for (i = 0; i < todo; i++)
	{
	  if (p[0] == 0 || p[1] == 0)
	    /* Reply is short.  This means that we were able to read
	       only part of what we wanted to.  */
	    return i + (origlen - len);
	  myaddr[i] = fromhex (p[0]) * 16 + fromhex (p[1]);
	  p += 2;
	}
      myaddr += todo;
      memaddr += todo;
      len -= todo;
    }
  return origlen;
}

/* Read or write LEN bytes from inferior memory at MEMADDR,
   transferring to or from debugger address MYADDR.  Write to inferior
   if SHOULD_WRITE is nonzero.  Returns length of data written or
   read; 0 for error.  */

/* ARGSUSED */
static int
remote_xfer_memory (memaddr, myaddr, len, should_write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
     struct target_ops *target;			/* ignored */
{
#ifdef REMOTE_TRANSLATE_XFER_ADDRESS
  CORE_ADDR targaddr;
  int targlen;
  REMOTE_TRANSLATE_XFER_ADDRESS (memaddr, len, targaddr, targlen);
  if (targlen == 0)
    return 0;
  memaddr = targaddr;
  len = targlen;
#endif

  return dcache_xfer_memory (remote_dcache, memaddr, myaddr,
			     len, should_write);
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

/* Send the command in BUF to the remote machine, and read the reply
   into BUF.  Report an error if we get an error reply.  */

static void
remote_send (buf)
     char *buf;
{
  putpkt (buf);
  getpkt (buf, 0);

  if (buf[0] == 'E')
    error ("Remote failure reply: %s", buf);
}

/* Display a null-terminated packet on stdout, for debugging, using C
   string notation.  */

static void
print_packet (buf)
     char *buf;
{
  puts_filtered ("\"");
  while (*buf)
    gdb_printchar (*buf++, gdb_stdout, '"');
  puts_filtered ("\"");
}

int
putpkt (buf)
     char *buf;
{
  return putpkt_binary (buf, strlen (buf));
}

/* Send a packet to the remote machine, with error checking.  The data
   of the packet is in BUF.  The string in BUF can be at most  PBUFSIZ - 5
   to account for the $, # and checksum, and for a possible /0 if we are
   debugging (remote_debug) and want to print the sent packet as a string */

static int
putpkt_binary (buf, cnt)
     char *buf;
     int cnt;
{
  int i;
  unsigned char csum = 0;
  char buf2[PBUFSIZ];
  int ch;
  int tcount = 0;
  char *p;

  /* Copy the packet into buffer BUF2, encapsulating it
     and giving it a checksum.  */

  if (cnt > (int) sizeof (buf2) - 5)		/* Prosanity check */
    abort ();

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
	  gdb_flush (gdb_stdout);
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
		printf_unfiltered ("Ack\n");
	      return 1;
	    case SERIAL_TIMEOUT:
	      tcount ++;
	      if (tcount > 3)
		return 0;
	      break;		/* Retransmit buffer */
	    case '$':
	      {
		char junkbuf[PBUFSIZ];

	      /* It's probably an old response, and we're out of sync.
		 Just gobble up the packet and ignore it.  */
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
	 able to get out next time we call QUIT, without anything as
	 violent as interrupt_query.  If we want to provide a way out of
	 here without getting to the next QUIT, it should be based on
	 hitting ^C twice as in remote_wait.  */
      if (quit_flag)
	{
	  quit_flag = 0;
	  interrupt_query ();
	}
#endif
    }
}

/* Come here after finding the start of the frame.  Collect the rest
   into BUF, verifying the checksum, length, and handling run-length
   compression.  Returns 0 on any error, 1 on success.  */

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

/* Read a packet from the remote machine, with error checking, and
   store it in BUF.  BUF is expected to be of size PBUFSIZ.  If
   FOREVER, wait forever rather than timing out; this is used while
   the target is executing user code.  */

void
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
	    fprintf_unfiltered (gdb_stdout, "Packet received: %s\n", buf);
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
  catch_errors ((catch_errors_ftype *) putpkt, "k", "", RETURN_MASK_ERROR);

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


/* On some machines, e.g. 68k, we may use a different breakpoint instruction
   than other targets; in those use REMOTE_BREAKPOINT instead of just
   BREAKPOINT.  Also, bi-endian targets may define LITTLE_REMOTE_BREAKPOINT
   and BIG_REMOTE_BREAKPOINT.  If none of these are defined, we just call
   the standard routines that are in mem-break.c.  */

/* FIXME, these ought to be done in a more dynamic fashion.  For instance,
   the choice of breakpoint instruction affects target program design and
   vice versa, and by making it user-tweakable, the special code here
   goes away and we need fewer special GDB configurations.  */

#if defined (LITTLE_REMOTE_BREAKPOINT) && defined (BIG_REMOTE_BREAKPOINT) && !defined(REMOTE_BREAKPOINT)
#define REMOTE_BREAKPOINT
#endif

#ifdef REMOTE_BREAKPOINT

/* If the target isn't bi-endian, just pretend it is.  */
#if !defined (LITTLE_REMOTE_BREAKPOINT) && !defined (BIG_REMOTE_BREAKPOINT)
#define LITTLE_REMOTE_BREAKPOINT REMOTE_BREAKPOINT
#define BIG_REMOTE_BREAKPOINT REMOTE_BREAKPOINT
#endif

static unsigned char big_break_insn[] = BIG_REMOTE_BREAKPOINT;
static unsigned char little_break_insn[] = LITTLE_REMOTE_BREAKPOINT;

#endif /* REMOTE_BREAKPOINT */

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
#ifdef REMOTE_BREAKPOINT
  int val;

  val = target_read_memory (addr, contents_cache, sizeof big_break_insn);

  if (val == 0)
    {
      if (TARGET_BYTE_ORDER == BIG_ENDIAN)
	val = target_write_memory (addr, (char *) big_break_insn,
				   sizeof big_break_insn);
      else
	val = target_write_memory (addr, (char *) little_break_insn,
				   sizeof little_break_insn);
    }

  return val;
#else
  return memory_insert_breakpoint (addr, contents_cache);
#endif /* REMOTE_BREAKPOINT */
}

static int
remote_remove_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
#ifdef REMOTE_BREAKPOINT
  return target_write_memory (addr, contents_cache, sizeof big_break_insn);
#else
  return memory_remove_breakpoint (addr, contents_cache);
#endif /* REMOTE_BREAKPOINT */
}

/* Some targets are only capable of doing downloads, and afterwards
   they switch to the remote serial protocol.  This function provides
   a clean way to get from the download target to the remote target.
   It's basically just a wrapper so that we don't have to expose any
   of the internal workings of remote.c.

   Prior to calling this routine, you should shutdown the current
   target code, else you will get the "A program is being debugged
   already..." message.  Usually a call to pop_target() suffices.  */

void
push_remote_target (name, from_tty)
     char *name;
     int from_tty;
{
  printf_filtered ("Switching to remote protocol\n");
  remote_open (name, from_tty);
}

/* Other targets want to use the entire remote serial module but with
   certain remote_ops overridden. */

void
open_remote_target (name, from_tty, target, extended_p)
     char *name;
     int from_tty;
     struct target_ops *target;
     int extended_p;
{
  printf_filtered ("Selecting the %sremote protocol\n",
		   (extended_p ? "extended-" : ""));
  remote_open_1 (name, from_tty, target, extended_p);
}

/* Table used by the crc32 function to calcuate the checksum. */

static unsigned long crc32_table[256] = {0, 0};

static unsigned long
crc32 (buf, len, crc)
     unsigned char *buf;
     int len;
     unsigned int crc;
{
  if (! crc32_table[1])
    {
      /* Initialize the CRC table and the decoding table. */
      int i, j;
      unsigned int c;

      for (i = 0; i < 256; i++)
        {
          for (c = i << 24, j = 8; j > 0; --j)
            c = c & 0x80000000 ? (c << 1) ^ 0x04c11db7 : (c << 1);
          crc32_table[i] = c;
        }
    }

  while (len--)
    {
      crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf) & 255];
      buf++;
    }
  return crc;
}

/* compare-sections command

   With no arguments, compares each loadable section in the exec bfd
   with the same memory range on the target, and reports mismatches.
   Useful for verifying the image on the target against the exec file.
   Depends on the target understanding the new "qCRC:" request.  */

static void
compare_sections_command (args, from_tty)
     char *args;
     int from_tty;
{
  asection *s;
  unsigned long host_crc, target_crc;
  extern bfd *exec_bfd;
  struct cleanup *old_chain;
  char *tmp, *sectdata, *sectname, buf[PBUFSIZ];
  bfd_size_type size;
  bfd_vma lma;
  int matched = 0;
  int mismatched = 0;

  if (!exec_bfd)
    error ("command cannot be used without an exec file");
  if (!current_target.to_shortname ||
      strcmp (current_target.to_shortname, "remote") != 0)
    error ("command can only be used with remote target");

  for (s = exec_bfd->sections; s; s = s->next) 
    {
      if (!(s->flags & SEC_LOAD))
	continue;	/* skip non-loadable section */

      size = bfd_get_section_size_before_reloc (s);
      if (size == 0)
	continue;	/* skip zero-length section */

      sectname = (char *) bfd_get_section_name (exec_bfd, s);
      if (args && strcmp (args, sectname) != 0)
	continue;	/* not the section selected by user */

      matched = 1;	/* do this section */
      lma = s->lma;
      /* FIXME: assumes lma can fit into long */
      sprintf (buf, "qCRC:%lx,%lx", (long) lma, (long) size);
      putpkt (buf);

      /* be clever; compute the host_crc before waiting for target reply */
      sectdata = xmalloc (size);
      old_chain = make_cleanup (free, sectdata);
      bfd_get_section_contents (exec_bfd, s, sectdata, 0, size);
      host_crc = crc32 ((unsigned char *) sectdata, size, 0xffffffff);

      getpkt (buf, 0);
      if (buf[0] == 'E')
	error ("target memory fault, section %s, range 0x%08x -- 0x%08x",
	       sectname, lma, lma + size);
      if (buf[0] != 'C')
	error ("remote target does not support this operation");

      for (target_crc = 0, tmp = &buf[1]; *tmp; tmp++)
	target_crc = target_crc * 16 + fromhex (*tmp);

      printf_filtered ("Section %s, range 0x%08x -- 0x%08x: ",
		       sectname, lma, lma + size);
      if (host_crc == target_crc)
	printf_filtered ("matched.\n");
      else
       {
	 printf_filtered ("MIS-MATCHED!\n");
	 mismatched++;
       }

      do_cleanups (old_chain);
    }
  if (mismatched > 0)
    warning ("One or more sections of the remote executable does not match\n\
the loaded file\n");
  if (args && !matched)
    printf_filtered ("No loaded section named '%s'.\n", args);
}

static int
remote_query (query_type, buf, outbuf, bufsiz)
     int query_type;
     char *buf;
     char *outbuf;
     int *bufsiz;
{
  int i;
  char buf2[PBUFSIZ];
  char *p2 = &buf2[0];
  char *p = buf;

  if (! bufsiz)
    error ("null pointer to remote bufer size specified");

  /* minimum outbuf size is PBUFSIZE - if bufsiz is not large enough let 
     the caller know and return what the minimum size is   */
  /* Note: a zero bufsiz can be used to query the minimum buffer size */
  if ( *bufsiz < PBUFSIZ )
    {
      *bufsiz = PBUFSIZ;
      return -1;
    }

  /* except for querying the minimum buffer size, target must be open */
  if (! remote_desc)
    error ("remote query is only available after target open");

  /* we only take uppercase letters as query types, at least for now */
  if ( (query_type < 'A') || (query_type > 'Z') )
    error ("invalid remote query type");

  if (! buf)
    error ("null remote query specified");

  if (! outbuf)
    error ("remote query requires a buffer to receive data");

  outbuf[0] = '\0';

  *p2++ = 'q';
  *p2++ = query_type;

  /* we used one buffer char for the remote protocol q command and another
     for the query type.  As the remote protocol encapsulation uses 4 chars
     plus one extra in case we are debugging (remote_debug),
     we have PBUFZIZ - 7 left to pack the query string */
  i = 0;
  while ( buf[i] && (i < (PBUFSIZ - 8)) )
    {
      /* bad caller may have sent forbidden characters */
      if ( (!isprint(buf[i])) || (buf[i] == '$') || (buf[i] == '#') )
        error ("illegal characters in query string");

      *p2++ = buf[i];
      i++;
    }
  *p2 = buf[i];

  if ( buf[i] )
    error ("query larger than available buffer");

  i = putpkt (buf2);
  if ( i < 0 ) return i;

  getpkt (outbuf, 0);

  return 0;
}

static void
packet_command (args, from_tty)
     char *args;
     int from_tty;
{
  char buf[PBUFSIZ];

  if (! remote_desc)
    error ("command can only be used with remote target");

  if (! args)
    error ("remote-packet command requires packet text as argument");

  puts_filtered ("sending: ");
  print_packet (args);
  puts_filtered ("\n");
  putpkt (args);

  getpkt (buf, 0);
  puts_filtered ("received: ");
  print_packet (buf);
  puts_filtered ("\n");
}

#if 0
/* --------- UNIT_TEST for THREAD oriented PACKETS ------------------------- */

static void display_thread_info PARAMS ((struct gdb_ext_thread_info *info));

static void threadset_test_cmd PARAMS ((char *cmd, int tty));

static void threadalive_test PARAMS ((char *cmd, int tty));

static void threadlist_test_cmd PARAMS ((char *cmd, int tty));

int get_and_display_threadinfo PARAMS ((threadref *ref));

static void threadinfo_test_cmd PARAMS ((char *cmd, int tty));

static int thread_display_step PARAMS ((threadref *ref, void *context));

static void threadlist_update_test_cmd PARAMS ((char *cmd, int tty));

static void init_remote_threadtests PARAMS ((void));

#define SAMPLE_THREAD  0x05060708  /* Truncated 64 bit threadid */

static void
threadset_test_cmd (cmd, tty)
     char *cmd;
     int tty;
{
  int sample_thread = SAMPLE_THREAD;

  printf_filtered ("Remote threadset test\n");
  set_thread (sample_thread, 1);
}


static void
threadalive_test (cmd, tty)
     char *cmd;
     int tty;
{
  int sample_thread = SAMPLE_THREAD;

  if (remote_thread_alive (sample_thread))
    printf_filtered ("PASS: Thread alive test\n");
  else
    printf_filtered ("FAIL: Thread alive test\n");
}

void output_threadid PARAMS ((char *title, threadref * ref));

void
output_threadid (title, ref)
     char *title;
     threadref *ref;
{
  char hexid[20];

  pack_threadid (&hexid[0], ref);	/* Convert threead id into hex */
  hexid[16] = 0;
  printf_filtered ("%s  %s\n", title, (&hexid[0]));
}

static void
threadlist_test_cmd (cmd, tty)
     char *cmd;
     int tty;
{
  int startflag = 1;
  threadref nextthread;
  int done, result_count;
  threadref threadlist[3];

  printf_filtered ("Remote Threadlist test\n");
  if (!remote_get_threadlist (startflag, &nextthread, 3, &done,
			      &result_count, &threadlist[0]))
    printf_filtered ("FAIL: threadlist test\n");
  else
    {
      threadref *scan = threadlist;
      threadref *limit = scan + result_count;

      while (scan < limit)
	output_threadid (" thread ", scan++);
    }
}

void
display_thread_info (info)
     struct gdb_ext_thread_info *info;
{
  output_threadid ("Threadid: ", &info->threadid);
  printf_filtered ("Name: %s\n ", info->shortname);
  printf_filtered ("State: %s\n", info->display);
  printf_filtered ("other: %s\n\n", info->more_display);
}

int
get_and_display_threadinfo (ref)
     threadref *ref;
{
  int result;
  int set;
  struct gdb_ext_thread_info threadinfo;

  set = TAG_THREADID | TAG_EXISTS | TAG_THREADNAME
    | TAG_MOREDISPLAY | TAG_DISPLAY;
  if (0 != (result = remote_get_threadinfo (ref, set, &threadinfo)))
    display_thread_info (&threadinfo);
  return result;
}

static void
threadinfo_test_cmd (cmd, tty)
     char *cmd;
     int tty;
{
  int athread = SAMPLE_THREAD;
  threadref thread;
  int set;

  int_to_threadref (&thread, athread);
  printf_filtered ("Remote Threadinfo test\n");
  if (!get_and_display_threadinfo (&thread))
    printf_filtered ("FAIL cannot get thread info\n");
}

static int
thread_display_step (ref, context)
     threadref *ref;
     void *context;
{
  /* output_threadid(" threadstep ",ref); *//* simple test */
  return get_and_display_threadinfo (ref);
}

static void
threadlist_update_test_cmd (cmd, tty)
     char *cmd;
     int tty;
{
  printf_filtered ("Remote Threadlist update test\n");
  remote_threadlist_iterator (thread_display_step, 0, CRAZY_MAX_THREADS);
}

static void
init_remote_threadtests (void)
{
  add_com ("tlist", class_obscure, threadlist_test_cmd,
     "Fetch and print the remote list of thread identifiers, one pkt only");
  add_com ("tinfo", class_obscure, threadinfo_test_cmd,
	   "Fetch and display info about one thread");
  add_com ("tset", class_obscure, threadset_test_cmd,
	   "Test setting to a different thread");
  add_com ("tupd", class_obscure, threadlist_update_test_cmd,
	   "Iterate through updating all remote thread info");
  add_com ("talive", class_obscure, threadalive_test,
	   " Remote thread alive test ");
}

#endif /* 0 */

static void
init_remote_ops ()
{
  remote_ops.to_shortname = "remote";		
  remote_ops.to_longname = "Remote serial target in gdb-specific protocol";
  remote_ops.to_doc = 
    "Use a remote computer via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";  
  remote_ops.to_open = remote_open;		
  remote_ops.to_close = remote_close;		
  remote_ops.to_detach = remote_detach;
  remote_ops.to_resume = remote_resume;	
  remote_ops.to_wait = remote_wait;
  remote_ops.to_fetch_registers = remote_fetch_registers;
  remote_ops.to_store_registers = remote_store_registers;
  remote_ops.to_prepare_to_store = remote_prepare_to_store;
  remote_ops.to_xfer_memory = remote_xfer_memory;	
  remote_ops.to_files_info = remote_files_info;	
  remote_ops.to_insert_breakpoint = remote_insert_breakpoint;
  remote_ops.to_remove_breakpoint = remote_remove_breakpoint;
  remote_ops.to_kill = remote_kill;		
  remote_ops.to_load = generic_load;		
  remote_ops.to_mourn_inferior = remote_mourn;
  remote_ops.to_thread_alive = remote_thread_alive;
  remote_ops.to_stop = remote_stop;
  remote_ops.to_query = remote_query;
  remote_ops.to_stratum = process_stratum;
  remote_ops.to_has_all_memory = 1;	
  remote_ops.to_has_memory = 1;	
  remote_ops.to_has_stack = 1;	
  remote_ops.to_has_registers = 1;	
  remote_ops.to_has_execution = 1;	
  remote_ops.to_has_thread_control = tc_schedlock; /* can lock scheduler */
  remote_ops.to_magic = OPS_MAGIC;	
}

/* Set up the extended remote vector by making a copy of the standard
   remote vector and adding to it.  */

static void
init_extended_remote_ops ()
{
  extended_remote_ops = remote_ops;

  extended_remote_ops.to_shortname = "extended-remote";	
  extended_remote_ops.to_longname = 
    "Extended remote serial target in gdb-specific protocol";
  extended_remote_ops.to_doc = 
    "Use a remote computer via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",
  extended_remote_ops.to_open = extended_remote_open;	
  extended_remote_ops.to_create_inferior = extended_remote_create_inferior;
  extended_remote_ops.to_mourn_inferior = extended_remote_mourn;
} 

void
_initialize_remote ()
{
  init_remote_ops ();
  add_target (&remote_ops);

  init_extended_remote_ops ();
  add_target (&extended_remote_ops);
  init_remote_threads ();
#if 0
  init_remote_threadtests ();
#endif

  add_cmd ("compare-sections", class_obscure, compare_sections_command, 
	   "Compare section data on target to the exec file.\n\
Argument is a single section name (default: all loaded sections).", 
	   &cmdlist);

  add_cmd ("packet", class_maintenance, packet_command,
	   "Send an arbitrary packet to a remote target.\n\
   maintenance packet TEXT\n\
If GDB is talking to an inferior via the GDB serial protocol, then\n\
this command sends the string TEXT to the inferior, and displays the\n\
response packet.  GDB supplies the initial `$' character, and the\n\
terminating `#' character and checksum.",
	   &maintenancelist);

#ifdef	REMOTE_CHAT
  add_show_from_set
    (add_set_cmd ("remotechat", no_class,
		  var_zinteger, (char *)&remote_chat,
		  "Set remote port interacts with target.\n",
		  &setlist),
     &showlist);
#endif	/* REMOTE_CHAT */

  add_show_from_set 
    (add_set_cmd ("remotetimeout", no_class,
		  var_integer, (char *)&remote_timeout,
		  "Set timeout value for remote read.\n",
		  &setlist),
     &showlist);

  add_show_from_set 
    (add_set_cmd ("remotebreak", no_class,
		  var_integer, (char *)&remote_break,
		  "Set whether to send break if interrupted.\n",
		  &setlist),
     &showlist);

  add_show_from_set 
    (add_set_cmd ("remotewritesize", no_class,
		  var_integer, (char *)&remote_write_size,
		  "Set the maximum number of bytes per memory write packet.\n",
		  &setlist),
     &showlist);

  remote_address_size = TARGET_PTR_BIT;
  add_show_from_set 
    (add_set_cmd ("remoteaddresssize", class_obscure,
		  var_integer, (char *)&remote_address_size,
		  "Set the maximum size of the address (in bits) \
in a memory packet.\n",
		  &setlist),
     &showlist);  

  add_show_from_set (add_set_cmd ("remotebinarydownload", no_class,
				  var_boolean, (char *) &remote_binary_download,
				  "Set binary downloads.\n", &setlist),
		     &showlist);
}

