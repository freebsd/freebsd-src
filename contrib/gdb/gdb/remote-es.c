/* Memory-access and commands for remote es1800 processes, for GDB.
   Copyright (C) 1988, 1992 Free Software Foundation, Inc.

   This file is added to GDB to make it possible to do debugging via an
   ES-1800 emulator. The code was originally written by Johan Holmberg
   TT/SJ Ericsson Telecom AB and later modified by Johan Henriksson
   TT/SJ. It was modified for gdb 4.0 by TX/DK Jan Nordenand by TX/DKG
   Harald Johansen.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* Emulator communication protocol.
   All values are encoded in ascii hex digits.

        Request
Command
Reply
	read registers:
DR<cr>
     - 0 -    - 1 -    - 2 -    - 3 -      - 4 -    - 5 -    -- 6 -   - 7 - 
D = XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX   XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX
A = XXXXXXXX XXXXXXXX XXXXXXXX XXXXXXXX   XXXXXXXX XXXXXXXX XXXXXXXX 
    PC = XXXXXX       SSP = XXXXXX    USP = XXXXXX     SR = XXXXXXXX
 >
Each byte of register data is described by two hex digits.

	write regs
D0=XXXXXXXX<cr>
 >D1=XXXXXXXX<cr>
 >D2=XXXXXXXX<cr>
 >D3=XXXXXXXX<cr>
 >D4=XXXXXXXX<cr>
 >D5=XXXXXXXX<cr>
 >D6=XXXXXXXX<cr>
 >D7=XXXXXXXX<cr>
 >A0=XXXXXXXX<cr>
 >A1=XXXXXXXX<cr>
 >A2=XXXXXXXX<cr>
 >A3=XXXXXXXX<cr>
 >A4=XXXXXXXX<cr>
 >A5=XXXXXXXX<cr>
 >A6=XXXXXXXX<cr>
 >A7=XXXXXXXX<cr>
 >SR=XXXXXXXX<cr>
 >PC=XXXXXX<cr>
 >
Each byte of register data is described by two hex digits.

	read mem
@.BAA..AA
$FFFFFFXX
 >
AA..AA is address, XXXXXXX is the contents

	write mem
	@.BAA..AA=$XXXXXXXX
 >
AA..AA is address, XXXXXXXX is data

	cont
PC=$AA..AA
 >RBK
R>
AA..AA is address to resume. If AA..AA is omitted, resume at same address.

	step
PC=$AA..AA
 >STP
R>
AA..AA is address to resume. If AA..AA is omitted, resume at same address.

	kill req
STP
 >
*/


#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <errno.h>
#include <ctype.h>
#include "gdb_string.h"
#include <setjmp.h>
#include <fcntl.h>
#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "command.h"
#include "remote-utils.h"
#include "gdbcore.h"
#include "serial.h"

/* Prototypes for local functions */

static void
es1800_child_detach PARAMS ((char *, int));

static void
es1800_child_open PARAMS ((char *, int));

static void 
es1800_transparent PARAMS ((char *, int));

static void
es1800_create_inferior PARAMS ((char *, char *, char **));

static void
es1800_load PARAMS ((char *, int));

static void
es1800_kill PARAMS ((void));

static int
verify_break PARAMS ((int));

static int
es1800_remove_breakpoint PARAMS ((CORE_ADDR, char *));

static int
es1800_insert_breakpoint PARAMS ((CORE_ADDR, char *));

static void
es1800_files_info PARAMS ((struct target_ops *));

static int
es1800_xfer_inferior_memory PARAMS ((CORE_ADDR, char *, int, int,
				     struct target_ops *));

static void 
es1800_prepare_to_store PARAMS ((void));

static int es1800_wait PARAMS ((int, struct target_waitstatus *));

static void es1800_resume PARAMS ((int, int, enum target_signal));

static void
es1800_detach PARAMS ((char *, int));

static void
es1800_attach PARAMS ((char *, int));

static int
damn_b PARAMS ((char *));

static void
es1800_open PARAMS ((char *, int));

static void
es1800_timer PARAMS ((void));

static void
es1800_reset PARAMS ((char *));

static void
es1800_request_quit PARAMS ((void));

static int
readchar PARAMS ((void));

static void
expect PARAMS ((char *, int));

static void
expect_prompt PARAMS ((void));

static void
download PARAMS ((FILE *, int, int));

#if 0
static void
bfd_copy PARAMS ((bfd *, bfd *));
#endif

static void
get_break_addr PARAMS ((int, CORE_ADDR *));

static int
fromhex PARAMS ((int));

static int
tohex PARAMS ((int));

static void
es1800_close PARAMS ((int));

static void
es1800_fetch_registers PARAMS ((void));

static void
es1800_fetch_register PARAMS ((int));

static void
es1800_store_register PARAMS ((int));

static void
es1800_read_bytes PARAMS ((CORE_ADDR, char *, int));

static void
es1800_write_bytes PARAMS ((CORE_ADDR, char *, int));

static void
send_with_reply PARAMS ((char *, char *, int));

static void
send_command PARAMS ((char *));

static void
send PARAMS ((char *));

static void
getmessage PARAMS ((char *, int));

static void
es1800_mourn_inferior PARAMS ((void));

static void
es1800_create_break_insn PARAMS ((char *, int));

static void
es1800_init_break PARAMS ((char *, int));

/* Local variables */

/* FIXME: Convert this to use "set remotedebug" instead.  */
#define LOG_FILE "es1800.log"
#if defined (LOG_FILE)
static FILE *log_file;
#endif

extern struct target_ops es1800_ops;		/* Forward decl */
extern struct target_ops es1800_child_ops;	/* Forward decl */

static int kiodebug;
static int timeout = 100; 
static char *savename;				/* Name of i/o device used */
static serial_ttystate es1800_saved_ttystate;
static int es1800_fc_save;			/* Save fcntl state */

/* indicates that the emulator uses 32-bit data-adress (68020-mode) 
   instead of 24-bit (68000 -mode) */

static int m68020;

#define MODE (m68020 ? "M68020" : "M68000" )
#define ES1800_BREAK_VEC (0xf)

/* Descriptor for I/O to remote machine.  Initialize it to NULL so that
   es1800_open knows that we don't have a file open when the program
   starts.  */

static serial_t es1800_desc = NULL;

#define	PBUFSIZ	1000
#define HDRLEN sizeof("@.BAAAAAAAA=$VV\r")

/* Maximum number of bytes to read/write at once.  The value here
   is chosen to fill up a packet.  */

#define MAXBUFBYTES ((PBUFSIZ-150)*16/75 )

static int es1800_break_vec = 0;
static char es1800_break_insn[2];
static long es1800_break_address;
static void (*old_sigint)();		/* Old signal-handler for sigint */
static jmp_buf interrupt;

/* Local signalhandler to allow breaking tranfers or program run.
   Rely on global variables: old_sigint(), interrupt */

static void
es1800_request_quit ()
{
  /* restore original signalhandler */
  signal (SIGINT, old_sigint);
  longjmp (interrupt, 1);
}


/* Reset emulator.
   Sending reset character(octal 32) to emulator.
   quit - return to '(esgdb)' prompt or continue	*/

static void
es1800_reset (quit)
     char *quit;
{
  char buf[80];

  if (quit)
    {
      printf ("\nResetting emulator...  ");
    }
  strcpy (buf, "\032");
  send (buf);
  expect_prompt ();
  if (quit)
    {
      error ("done\n");
    }
}


/* Open a connection to a remote debugger and push the new target
   onto the stack. Check if the emulator is responding and find out
   what kind of processor the emulator is connected to.
   Initiate the breakpoint handling in the emulator.

   name     - the filename used for communication (ex. '/dev/tta')
   from_tty - says whether to be verbose or not */

static void
es1800_open (name, from_tty)
     char *name;
     int from_tty;
{
  char buf[PBUFSIZ];
  char *p;
  int i, fcflag;

  m68020 = 0;

  if (!name)           /* no device name given in target command */
    {
      error_no_arg ("serial port device name");
    }

  target_preopen (from_tty);
  es1800_close (0);

  /* open the device and configure it for communication */

#ifndef DEBUG_STDIN

  es1800_desc = SERIAL_OPEN (name);
  if (es1800_desc == NULL)
    {
      perror_with_name (name);
    }
  savename = savestring (name, strlen (name));

  es1800_saved_ttystate = SERIAL_GET_TTY_STATE (es1800_desc);

  if ((fcflag = fcntl (es1800_desc->fd, F_GETFL, 0)) == -1)
    {
      perror_with_name ("fcntl serial");
    }
  es1800_fc_save = fcflag;

  fcflag = (fcflag & (FREAD | FWRITE)); /* mask out any funny stuff */
  if (fcntl (es1800_desc->fd, F_SETFL, fcflag) == -1)
    {
      perror_with_name ("fcntl serial");
    }

  if (baud_rate != -1)
    {
      if (SERIAL_SETBAUDRATE (es1800_desc, baud_rate))
	{
	  SERIAL_CLOSE (es1800_desc);
	  perror_with_name (name);
	}
    }

  SERIAL_RAW (es1800_desc);

  /* If there is something sitting in the buffer we might take it as a
     response to a command, which would be bad.  */
  SERIAL_FLUSH_INPUT (es1800_desc);

#endif	/* DEBUG_STDIN */

  push_target (&es1800_ops);	/* Switch to using remote target now */
  if (from_tty)
    {
      printf ("Remote ES1800 debugging using %s\n", name);
    }

#if defined (LOG_FILE)

  log_file = fopen (LOG_FILE, "w");
  if (log_file == NULL)
    {
      perror_with_name (LOG_FILE);
    }

#endif	/* LOG_FILE */

  /* Hello?  Are you there?, also check mode  */

  /*  send_with_reply( "DB 0 TO 1", buf, sizeof(buf)); */
  /*  for (p = buf, i = 0; *p++ =='0';)  */   /* count the number of zeros */
  /*      i++; */

  send ("\032");
  getmessage (buf, sizeof (buf));  /* send reset character */

  if (from_tty)
    {
      printf ("Checking mode.... ");
    }
  /*  m68020 = (i==8); */   /* if eight zeros then we are in m68020 mode */

  /* What kind of processor am i talking to ?*/
  p = buf;
  while (*p++ != '\n') {;}
  while (*p++ != '\n') {;}
  while (*p++ != '\n') {;}
  for (i = 0; i < 20; i++, p++) {;}
  m68020 = !strncmp (p, "68020", 5);
  if (from_tty)
    {
      printf ("You are in %s(%c%c%c%c%c)-mode\n", MODE, p[0], p[1], p[2],
	      p[3], p[4]);
    }

  /* if no init_break statement is present in .gdb file we have to check 
     whether to download a breakpoint routine or not */

#if 0
  if ((es1800_break_vec == 0) || (verify_break (es1800_break_vec) != 0)
      && query ("No breakpoint routine in ES 1800 emulator!\nDownload a breakpoint routine to the emulator? "))
    {
      CORE_ADDR memaddress;
      printf ("Give the start address of the breakpoint routine: ");
      scanf ("%li", &memaddress);
      es1800_init_break ((es1800_break_vec ? es1800_break_vec :
			  ES1800_BREAK_VEC), memaddress);
    }
#endif

}

/*  Close out all files and local state before this target loses control.
    quitting - are we quitting gdb now? */

static void
es1800_close (quitting)
     int quitting;
{
  if (es1800_desc != NULL)
    {
      printf ("\nClosing connection to emulator...\n");
      if (SERIAL_SET_TTY_STATE (es1800_desc, es1800_saved_ttystate) < 0)
	print_sys_errmsg ("warning: unable to restore tty state", errno);
      fcntl (es1800_desc->fd, F_SETFL, es1800_fc_save);
      SERIAL_CLOSE (es1800_desc);
      es1800_desc = NULL;
    }
  if (savename != NULL)
    {
      free (savename);
    }
  savename = NULL;

#if defined (LOG_FILE)

  if (log_file != NULL)
    {
      if (ferror (log_file))
	{
	  printf ("Error writing log file.\n");
	}
      if (fclose (log_file) != 0)
	{
	  printf ("Error closing log file.\n");
	}
      log_file = NULL;
    }

#endif	/* LOG_FILE */

}

/*  Attaches to a process on the target side
    proc_id  - the id of the process to be attached.
    from_tty - says whether to be verbose or not */

static void
es1800_attach (args, from_tty)
    char *args;
    int from_tty;
{
  error ("Cannot attach to pid %s, this feature is not implemented yet.",
	 args);
}


/* Takes a program previously attached to and detaches it.
   We better not have left any breakpoints
   in the program or it'll die when it hits one.
   Close the open connection to the remote debugger.
   Use this when you want to detach and do something else
   with your gdb.
 
   args     - arguments given to the 'detach' command
   from_tty - says whether to be verbose or not */

static void
es1800_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    {
      error ("Argument given to \"detach\" when remotely debugging.");
    }
  pop_target ();
  if (from_tty)
    {
      printf ("Ending es1800 remote debugging.\n");
    }
}


/* Tell the remote machine to resume.
   step    - single-step or run free
   siggnal - the signal value to be given to the target (0 = no signal) */

static void
es1800_resume (pid, step, siggnal)
     int pid;
     int step;
     enum target_signal siggnal;
{
  char buf[PBUFSIZ];

  if (siggnal)
    {
      error ("Can't send signals to a remote system.");
    }
  if (step)
    {
      strcpy (buf,"STP\r");
      send (buf);
    }
  else
    {
      send_command ("RBK");
    }
}

/* Wait until the remote machine stops, then return,
   storing status in STATUS just as `wait' would.
   status -  */
 
static int
es1800_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  unsigned char buf[PBUFSIZ];
  int old_timeout = timeout;

  status->kind = TARGET_WAITKIND_EXITED;
  status->value.integer = 0;

  timeout = 0;		/* Don't time out -- user program is running. */
  if (!setjmp (interrupt))
    {
      old_sigint = signal (SIGINT, es1800_request_quit);
      while (1)
        {
	  getmessage (buf, sizeof(buf));
	  if (strncmp ( buf, "\r\n* BREAK *", 11) == 0) 
	    {
	      status->kind = TARGET_WAITKIND_STOPPED;
	      status->value.sig = TARGET_SIGNAL_TRAP;
	      send_command ("STP");	/* Restore stack and PC and such */
	      if (m68020)
		{
		  send_command ("STP");
		}
	      break;
	    }
	  if (strncmp (buf, "STP\r\n ", 6) == 0)
	    {
	      status->kind = TARGET_WAITKIND_STOPPED;
	      status->value.sig = TARGET_SIGNAL_TRAP;
	      break;
	    }
	  if (buf[strlen (buf) - 2] == 'R')
	    {
	      printf ("Unexpected emulator reply: \n%s\n", buf);
	    }
	  else
	    {
	      printf ("Unexpected stop: \n%s\n", buf);
	      status->kind = TARGET_WAITKIND_STOPPED;
	      status->value.sig = TARGET_SIGNAL_QUIT;
	      break;
	    }
        }
    }
  else
    {
      fflush (stdin);
      printf ("\nStopping emulator...");
      if (!setjmp (interrupt))
	{
	  old_sigint = signal (SIGINT, es1800_request_quit);
	  send_command ("STP");
	  printf (" emulator stopped\n");
	  status->kind = TARGET_WAITKIND_STOPPED;
	  status->value.sig = TARGET_SIGNAL_INT;
        }
      else
	{
	  fflush (stdin);
	  es1800_reset ((char*) 1);
	}
    }
  signal (SIGINT, old_sigint);
  timeout = old_timeout;
  return (0);
}


/* Fetch register values from remote machine.
   regno - the register to be fetched (fetch all registers if -1) */

static void
es1800_fetch_register (regno)
     int regno;
{
  char buf[PBUFSIZ];
  int k;
  int r;
  char *p;
  static char regtab[18][4] = 
    {
      "D0 ", "D1 ", "D2 ", "D3 ", "D4 ", "D5 ", "D6 ", "D7 ",
      "A0 ", "A1 ", "A2 ", "A3 ", "A4 ", "A5 ", "A6 ", "SSP",
      "SR ", "PC "
    };

  if ((regno < 15) || (regno == 16) || (regno == 17))
    {
      r = regno * 4;
      send_with_reply (regtab[regno], buf, sizeof (buf));
      p = buf;
      for (k = 0; k < 4; k++)
	{
	  if ((p[k*2 + 1] == 0) || (p[k*2 + 2] == 0))
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = (fromhex (p[k*2 + 1]) * 16) + fromhex (p[k*2 + 2]);
	}
    }
  else
    {
      es1800_fetch_registers ();
    }
}

/* Read the remote registers into REGISTERS.
   Always fetches all registers. */

static void
es1800_fetch_registers ()
{
  char buf[PBUFSIZ];
  char SR_buf[PBUFSIZ];
  int i;
  int k;
  int r;
  char *p;

  send_with_reply ("DR", buf, sizeof (buf));

  /* Reply is edited to a string that describes registers byte by byte,
     each byte encoded as two hex characters.  */

  p = buf;
  r = 0;

  /*  parsing row one - D0-D7-registers  */

  while (*p++ != '\n') {;}
  for (i = 4; i < 70; i += (i == 39 ? 3 : 1))
    {
      for (k = 0; k < 4; k++)
	{
	  if (p[i+0] == 0 || p[i+1] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = (fromhex (p[i+0]) * 16) + fromhex (p[i+1]);
	  i += 2;
	}
    }
  p += i;

  /*  parsing row two - A0-A6-registers  */

  while (*p++ != '\n') {;}
  for (i = 4; i < 61; i += (i == 39 ? 3 : 1))
    {
      for (k = 0; k < 4; k++)
	{
	  if (p[i+0] == 0 || p[i+1] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = (fromhex (p[i+0])) * 16 + fromhex (p[i+1]);
	  i += 2;
	}
    }
  p += i;

  while (*p++ != '\n') {;}

  /* fetch SSP-, SR- and PC-registers  */

  /* first - check STATUS-word and decide which stackpointer to use */

  send_with_reply ("SR", SR_buf, sizeof (SR_buf));
  p = SR_buf;
  p += 5;

  if (m68020)
    {
      if (*p == '3')		 /* use masterstackpointer MSP */
	{
	  send_with_reply ("MSP", buf, sizeof (buf));
	}
      else if (*p == '2')	/* use interruptstackpointer ISP  */
	{
	  send_with_reply ("ISP", buf, sizeof (buf));
	}
      else			/* use userstackpointer USP  */
	{
	  send_with_reply ("USP", buf, sizeof (buf)); 
	}
      p = buf;
      for (k = 0; k<4; k++)
	{
	  if (p[k*2+1] == 0 || p[k*2+2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = fromhex (buf[k*2+1]) * 16 + fromhex (buf[k*2+2]);
	}

      p = SR_buf;
      for (k = 0; k < 4; k++)
	{
	  if (p[k*2+1] == 0 || p[k*2+2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] =
	    fromhex (SR_buf[k*2+1]) * 16 + fromhex (SR_buf[k*2+2]);
	}
      send_with_reply ("PC", buf, sizeof (buf));
      p = buf;
      for (k = 0; k<4; k++)
	{
	  if (p[k*2+1] == 0 || p[k*2+2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = fromhex (buf[k*2+1]) * 16 + fromhex (buf[k*2+2]);
	}
    }
  else    /* 68000-mode */
    {                       
      if (*p == '2') /* use supervisorstackpointer SSP  */
	{
	  send_with_reply ("SSP", buf, sizeof (buf)); 
	}
      else  /* use userstackpointer USP  */
	{
	  send_with_reply ("USP", buf, sizeof (buf)); 
	}

      /* fetch STACKPOINTER */

      p = buf;
      for (k = 0; k < 4; k++)
	{
	  if (p[k*2 + 1] == 0 || p[k*2 + 2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = fromhex (buf[k*2+1]) * 16 + fromhex (buf[k*2+2]);
	}

      /* fetch STATUS */

      p = SR_buf;
      for (k = 0; k < 4; k++)
	{
	  if (p[k*2+1] == 0 || p[k*2+2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] =
	    fromhex (SR_buf[k*2+1]) * 16 + fromhex (SR_buf[k*2+2]);
	}

      /* fetch PC */

      send_with_reply ("PC", buf, sizeof (buf)); 
      p = buf;
      for (k = 0; k < 4; k++)
	{
	  if (p[k*2+1] == 0 || p[k*2+2] == 0)
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  registers[r++] = fromhex (buf[k*2+1]) * 16 + fromhex (buf[k*2+2]);
	}    
    }
}

/* Store register value, located in REGISTER, on the target processor.
   regno - the register-number of the register to store
           (-1 means store them all)
   FIXME: Return errno value.  */

static void
es1800_store_register(regno)
     int regno;
{

  static char regtab[18][4] =
    {
      "D0 ", "D1 ", "D2 ", "D3 ", "D4 ", "D5 ", "D6 ", "D7 ",
      "A0 ", "A1 ", "A2 ", "A3 ", "A4 ", "A5 ", "A6 ", "SSP",
      "SR ", "PC "
    };

  char buf[PBUFSIZ];
  char SR_buf[PBUFSIZ];
  char stack_pointer[4];
  char *p;
  int i;
  int j;
  int k;
  unsigned char *r;

  r = (unsigned char *) registers;

  if (regno == -1)  /* write all registers */
    {
      j = 0;
      k = 18;
    }
  else              /* write one register */
    {
      j = regno;
      k = regno+1;
      r += regno * 4; 
    }    

  if ((regno == -1) || (regno == 15))
    {
      /* fetch current status */
      send_with_reply ("SR", SR_buf, sizeof (SR_buf));
      p = SR_buf;
      p += 5;
      if (m68020)
	{
	  if (*p == '3') /* use masterstackpointer MSP */
	    {
	      strcpy (stack_pointer,"MSP");  
	    }
	  else
	    {
	      if (*p == '2') /* use interruptstackpointer ISP  */
		{
		  strcpy (stack_pointer,"ISP");  
		}
	      else
		{
		  strcpy (stack_pointer,"USP");  /* use userstackpointer USP  */
		}
	    }
	}
      else  /* 68000-mode */
	{
	  if (*p == '2') /* use supervisorstackpointer SSP  */
	    {
	      strcpy (stack_pointer,"SSP");  
	    }
	  else
	    {
	      strcpy (stack_pointer,"USP");/* use userstackpointer USP  */  
	    }
	}
      strcpy (regtab[15],stack_pointer);
    }

  for (i = j; i<k; i++)
    {
      buf[0] = regtab[i][0];
      buf[1] = regtab[i][1];
      buf[2] = regtab[i][2];
      buf[3] = '=';
      buf[4] = '$';
      buf[5] = tohex ((*r >> 4) & 0x0f);
      buf[6] = tohex (*r++ & 0x0f);
      buf[7] = tohex ((*r >> 4) & 0x0f);
      buf[8] = tohex (*r++ & 0x0f);
      buf[9] = tohex ((*r >> 4) & 0x0f);
      buf[10] = tohex (*r++ & 0x0f);
      buf[11] = tohex ((*r >> 4) & 0x0f);
      buf[12] = tohex (*r++ & 0x0f);
      buf[13] = 0;

      send_with_reply (buf, buf, sizeof (buf)); /* FIXME, reply not used? */
    }
}


/* Prepare to store registers.  */

static void 
es1800_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

/* Convert hex digit A to a number.  */

static int
fromhex (a)
     int a;
{
  if (a >= '0' && a <= '9')
    {
      return a - '0';
    }
  else if (a >= 'a' && a <= 'f')
    {
      return a - 'a' + 10;
    }
  else if (a >= 'A' && a <= 'F')
    {
      return a - 'A' + 10;
    }
  else
    {
      error ("Reply contains invalid hex digit");
    }
  return (-1);
}


/* Convert number NIB to a hex digit.  */

static int
tohex (nib)
     int nib;
{
  if (nib < 10)
    {
      return ('0' + nib);
    }
  else
    {
      return ('A' + nib - 10);
    }
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if WRITE is
   nonzero.  Returns length of data written or read; 0 for error. 
 
   memaddr - the target's address
   myaddr  - gdb's address
   len     - number of bytes 
   write   - write if != 0 otherwise read	*/

static int
es1800_xfer_inferior_memory (memaddr, myaddr, len, write, tops)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *tops;	/* Unused */
{
  int origlen = len;
  int xfersize;

  while (len > 0)
    {
      xfersize = len > MAXBUFBYTES ? MAXBUFBYTES : len;
      if (write)
	{
	  es1800_write_bytes (memaddr, myaddr, xfersize);
	}
      else
	{
	  es1800_read_bytes (memaddr, myaddr, xfersize);
	}
      memaddr += xfersize;
      myaddr += xfersize;
      len -= xfersize;
    }
  return (origlen); /* no error possible */
}


/* Write memory data directly to the emulator.
   This does not inform the data cache; the data cache uses this.
   MEMADDR is the address in the remote memory space.
   MYADDR is the address of the buffer in our space.
   LEN is the number of bytes.
 
   memaddr - the target's address
   myaddr  - gdb's address
   len     - number of bytes   */

static void
es1800_write_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  char buf[PBUFSIZ];
  int i;
  char *p;

  p = myaddr;
  for (i = 0; i < len; i++)
    {
      sprintf (buf, "@.B$%x=$%x", memaddr+i, (*p++) & 0xff);
      send_with_reply (buf, buf, sizeof (buf));      /* FIXME send_command? */
    }
}


/* Read memory data directly from the emulator.
   This does not use the data cache; the data cache uses this.
 
   memaddr - the target's address
   myaddr  - gdb's address
   len     - number of bytes   */

static void
es1800_read_bytes (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  static int DB_tab[16] = {8,11,14,17,20,23,26,29,34,37,40,43,46,49,52,55};
  char buf[PBUFSIZ];
  int i;
  int low_addr;
  char *p;
  char *b;

  if (len > PBUFSIZ / 2 - 1)
    {
      abort ();
    }

  if (len == 1)	/* The emulator does not like expressions like:  */
    {
      len = 2;	/* DB.B $20018 TO $20018                       */
    }

  /* Reply describes registers byte by byte, each byte encoded as two hex
     characters.  */

  sprintf (buf, "DB.B $%x TO $%x", memaddr, memaddr+len-1);
  send_with_reply (buf, buf, sizeof (buf));
  b = buf;
  low_addr = memaddr&0x0f;
  for (i = low_addr; i < low_addr + len; i++)
    {
      if ((!(i % 16)) && i)
	{   /* if (i = 16,32,48)  */
	  while (*p++!='\n') {;}
	  b = p;
	}
      p = b + DB_tab[i%16] + (m68020 ? 2 : 0);
      if (p[0] == 32 || p[1] == 32)
	{
	  error ("Emulator reply is too short: %s", buf);
	}
      myaddr[i-low_addr] = fromhex (p[0]) * 16 + fromhex (p[1]);
    }
}

/* Information about the current target  */

static void
es1800_files_info (tops)
     struct target_ops *tops;	/* Unused */
{
  printf ("ES1800 Attached to %s at %d baud in %s mode\n", savename, 19200,
	  MODE);
}


/* We read the contents of the target location and stash it,
   then overwrite it with a breakpoint instruction.
 
   addr           - is the target location in the target machine.
   contents_cache - is a pointer to memory allocated for saving the target contents.
                    It is guaranteed by the caller to be long enough to save sizeof 
                    BREAKPOINT bytes.
 
   FIXME: This size is target_arch dependent and should be available in
   the target_arch transfer vector, if we ever have one...  */

static int
es1800_insert_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  int val; 

  val = target_read_memory (addr, contents_cache, sizeof (es1800_break_insn));

  if (val == 0)
    {
      val = target_write_memory (addr, es1800_break_insn,
				 sizeof (es1800_break_insn));
    }

  return (val);
}


/* Write back the stashed instruction
 
   addr           - is the target location in the target machine.
   contents_cache - is a pointer to memory allocated for saving the target contents.
                    It is guaranteed by the caller to be long enough to save sizeof 
                    BREAKPOINT bytes.	*/
 
static int
es1800_remove_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{

  return (target_write_memory (addr, contents_cache,
			       sizeof (es1800_break_insn)));
}

/* create_break_insn ()
   Primitive datastructures containing the es1800 breakpoint instruction  */

static void
es1800_create_break_insn (ins, vec)
     char *ins;
     int vec;
{
  if (vec == 15)
    {
      ins[0] = 0x4e;
      ins[1] = 0x4f;
    }
}


/* verify_break ()
   Seach for breakpoint routine in emulator memory.
   returns non-zero on failure
   vec - trap vector used for breakpoints  */

static int
verify_break (vec)
     int vec;
{
  CORE_ADDR memaddress;
  char buf[8];
  char *instr = "NqNqNqNs";      /* breakpoint routine */
  int status;

  get_break_addr (vec, &memaddress);

  if (memaddress)
    {
      status = target_read_memory (memaddress, buf, 8); 
      if (status != 0)
	{
	  memory_error (status, memaddress);
	}
      return (STRCMP (instr, buf));
    }
    return (-1);
}


/* get_break_addr ()
   find address of breakpint routine
   vec - trap vector used for breakpoints
   addrp - store the address here	*/

static void
get_break_addr (vec, addrp)
     int vec;
     CORE_ADDR *addrp;
{
  CORE_ADDR memaddress = 0;
  int status;
  int k;
  char buf[PBUFSIZ];
  char base_addr[4];
  char *p;

  if (m68020)
    {
      send_with_reply ("VBR ", buf, sizeof (buf));
      p = buf;
      for (k = 0; k < 4; k++)
	{
	  if ((p[k*2 + 1] == 0) || (p[k*2 + 2] == 0))
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  base_addr[k] = (fromhex (p[k*2 + 1]) * 16) + fromhex (p[k*2 + 2]);
	}
      /* base addr of exception vector table */
      memaddress = *((CORE_ADDR *) base_addr);
    }

    memaddress += (vec + 32) * 4;     /* address of trap vector */
    status = target_read_memory (memaddress, (char *) addrp, 4); 
    if (status != 0)                                                   
      {
	memory_error (status, memaddress);
      }
}


/* Kill an inferior process */

static void
es1800_kill ()
{
  if (inferior_pid != 0)
    {
      inferior_pid = 0;
      es1800_mourn_inferior ();
    }
}


/* Load a file to the ES1800 emulator. 
   Converts the file from a.out format into Extended Tekhex format
   before the file is loaded.
   Also loads the trap routine, and sets the ES1800 breakpoint on it
   filename - the a.out to be loaded
   from_tty - says whether to be verbose or not
   FIXME Uses emulator overlay memory for trap routine	*/
 
static void
es1800_load (filename, from_tty)
     char *filename;
     int from_tty;
{

  FILE *instream;
  char loadname[15];
  char buf[160];
  struct cleanup *old_chain;
  int es1800_load_format = 5;

  if (es1800_desc == NULL) 
    {
      printf ("No emulator attached, type emulator-command first\n");
      return;
    }

  filename = tilde_expand (filename);
  make_cleanup (free, filename);

  switch (es1800_load_format)
    {
      case 2:   /* Extended Tekhex  */
        if (from_tty)
	  {
	    printf ("Converting \"%s\" to Extended Tekhex Format\n", filename);
	  }
	sprintf (buf, "tekhex %s", filename);
	system (buf);
	sprintf (loadname, "out.hex");
	break;

      case 5:   /* Motorola S-rec  */
	if (from_tty)
	  {
	    printf ("Converting \"%s\" to Motorola S-record format\n",
		    filename);
	  }
	/* in the future the source code in copy (part of binutils-1.93) will
	   be included in this file */
	sprintf (buf,
		 "copy -s \"a.out-sunos-big\" -d \"srec\" %s /tmp/out.hex",
		 filename);
	system (buf);
	sprintf (loadname, "/tmp/out.hex");
	break;

      default:
	error ("Downloading format not defined\n");
    }
 
  breakpoint_init_inferior ();
  inferior_pid = 0;
  if (from_tty)
    {
      printf ("Downloading \"%s\" to the ES 1800\n",filename);
    }
  if ((instream = fopen (loadname, "r")) == NULL)
    {
      perror_with_name ("fopen:");
    }

  old_chain = make_cleanup (fclose, instream);
  immediate_quit++;

  es1800_reset (0);

  download (instream, from_tty, es1800_load_format);

  /* if breakpoint routine is not present anymore we have to check 
     whether to download a new breakpoint routine or not */

  if ((verify_break (es1800_break_vec) != 0)
      && query ("No breakpoint routine in ES 1800 emulator!\nDownload a breakpoint routine to the emulator? "))
    {
      char buf[128];
      printf ("Using break vector 0x%x\n", es1800_break_vec);
      sprintf (buf, "0x%x ", es1800_break_vec);
      printf ("Give the start address of the breakpoint routine: ");
      fgets (buf + strlen (buf), sizeof (buf) - strlen (buf), stdin);
      es1800_init_break (buf, 0);
    }

  do_cleanups (old_chain);
  expect_prompt ();
  readchar ();  /* FIXME I am getting a ^G = 7 after the prompt  */
  printf ("\n");

  if (fclose (instream) == EOF)
    {
      ;
    }

  if (es1800_load_format != 2)
    {
      sprintf (buf, "/usr/bin/rm %s", loadname);
      system (buf);
    }

  symbol_file_command (filename, from_tty);   /* reading symbol table */
  immediate_quit--;
}

#if 0

#define NUMCPYBYTES 20

static void
bfd_copy (from_bfd, to_bfd)
     bfd *from_bfd;
     bfd *to_bfd;
{
  asection *p, *new;
  int i;
  char buf[NUMCPYBYTES];

  for (p = from_bfd->sections; p != NULL; p = p->next)
    {
      printf ("  Copying section %s. Size = %x.\n", p->name, p->_cooked_size);
      printf ("    vma = %x,  offset = %x,  output_sec = %x\n",
	      p->vma, p->output_offset, p->output_section);
      new = bfd_make_section (to_bfd, p->name);
      if (p->_cooked_size &&
	  !bfd_set_section_size (to_bfd, new, p->_cooked_size))
	{
	  error ("Wrong BFD size!\n");
	}
      if (!bfd_set_section_flags (to_bfd, new, p->flags))
	{
	  error ("bfd_set_section_flags");
	}
      new->vma = p->vma;
      
      for (i = 0; (i + NUMCPYBYTES) < p->_cooked_size ; i += NUMCPYBYTES)
	{
	  if (!bfd_get_section_contents (from_bfd, p, (PTR) buf, (file_ptr) i,
					(bfd_size_type) NUMCPYBYTES))
	    {
	      error ("bfd_get_section_contents\n");
	    }
	if (!bfd_set_section_contents (to_bfd, new, (PTR) buf, (file_ptr) i,
				      (bfd_size_type) NUMCPYBYTES))
	  {
	    error ("bfd_set_section_contents\n");
	  }
      }
      bfd_get_section_contents (from_bfd, p, (PTR) buf, (file_ptr) i,
				(bfd_size_type) (p->_cooked_size - i));
      bfd_set_section_contents (to_bfd, new, (PTR) buf,(file_ptr) i,
				(bfd_size_type) (p->_cooked_size - i));
    }
}

#endif

/* Start an process on the es1800 and set inferior_pid to the new
   process' pid.
   execfile - the file to run
   args     - arguments passed to the program
   env      - the environment vector to pass	*/

static void
es1800_create_inferior (execfile, args, env)
     char *execfile;
     char *args;
     char **env;
{
  int entry_pt;
  int pid;
#if 0
  struct expression *expr;
  register struct cleanup *old_chain = 0;
  register value val;
#endif

  if (args && *args)
    {
      error ("Can't pass arguments to remote ES1800 process");
    }

#if 0
  if (query ("Use 'start' as entry point? "))
    {
      expr = parse_c_expression ("start");
      old_chain = make_cleanup (free_current_contents, &expr);
      val = evaluate_expression (expr);
      entry_pt = (val->location).address;
    }
  else
    {
      printf ("Enter the program's entry point (in hexadecimal): ");
      scanf ("%x", &entry_pt);
    }
#endif

  if (execfile == 0 || exec_bfd == 0)
    {
      error ("No exec file specified");
    }

  entry_pt = (int) bfd_get_start_address (exec_bfd);

  pid = 42;

  /* Now that we have a child process, make it our target.  */

  push_target (&es1800_child_ops);

  /* The "process" (board) is already stopped awaiting our commands, and
     the program is already downloaded.  We just set its PC and go.  */

  inferior_pid = pid;		/* Needed for wait_for_inferior below */

  clear_proceed_status ();

  /* Tell wait_for_inferior that we've started a new process.  */

  init_wait_for_inferior ();

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */

  target_terminal_init ();

  /* Install inferior's terminal modes.  */

  target_terminal_inferior ();

  /* remote_start (args); */
  /* trap_expected = 0; */
  /* insert_step_breakpoint ();  FIXME, do we need this?  */

  /* Let 'er rip... */
  proceed ((CORE_ADDR) entry_pt, TARGET_SIGNAL_DEFAULT, 0);

}


/* The process has died, clean up.  */

static void
es1800_mourn_inferior ()
{
  remove_breakpoints ();
  unpush_target (&es1800_child_ops);
  generic_mourn_inferior ();	/* Do all the proper things now */
}

/* ES1800-protocol specific routines */

/* Keep discarding input from the remote system, until STRING is found. 
   Let the user break out immediately. 
   string - the string to expect
   nowait - break out if string not the emulator's first respond otherwise
            read until string is found (== 0)   */
 
static void
expect (string, nowait)
     char *string;
    int nowait;
{
  char c;
  char *p = string;

  immediate_quit++;
  while (1)
    {
      c = readchar ();
      if (isalpha (c))
	{
	  c = toupper (c);
	}
      if (c == toupper (*p))
	{
	  p++;
	  if (*p == '\0')
	    {
	      immediate_quit--;
	      return;
	    }
	}
      else if (!nowait)
	{
	  p = string;
	}
      else
	{
	  printf ("\'%s\' expected\n" , string);
	  printf ("char %d is %d", p - string, c);
	  error ("\n" );
	}
    }
}

/* Keep discarding input until we see the prompt.  */

static void
expect_prompt ()
{
  expect (">", 0);
}


/* Read one character */

#ifdef DEBUG_STDIN

/* read from stdin */

static int
readchar ()
{
  char buf[1];

  buf[0] = '\0';
  printf ("readchar, give one character\n");
  read (0, buf, 1);

#if defined (LOG_FILE)
  putc (buf[0] & 0x7f, log_file);
#endif

  return (buf[0] & 0x7f);
}

#else	/* !DEBUG_STDIN */

/* Read a character from the remote system, doing all the fancy
   timeout stuff.  */

static int
readchar ()
{
  int ch;

  ch = SERIAL_READCHAR (es1800_desc, timeout);

  /* FIXME: doing an error() here will probably cause trouble, at least if from
     es1800_wait.  */
  if (ch == SERIAL_TIMEOUT)
    error ("Timeout reading from remote system.");
  else if (ch == SERIAL_ERROR)
    perror_with_name ("remote read");

#if defined (LOG_FILE)
  putc (ch & 0x7f, log_file);
  fflush (log_file);
#endif

  return (ch);
}

#endif	/* DEBUG_STDIN */


/* Send a command to the emulator and save the reply.
   Report an error if we get an error reply.
   string - the es1800 command
   buf    - containing the emulator reply on return
   len    - size of buf  */

static void
send_with_reply (string, buf, len)
    char *string, *buf;
    int len;
{
  send (string);
  SERIAL_WRITE (es1800_desc, "\r", 1);

#ifndef DEBUG_STDIN
  expect (string, 1);
  expect ("\r\n", 0);
#endif

  getmessage (buf, len);
}


/* Send the command in STR to the emulator adding \r. check
   the echo for consistency. 
   string - the es1800 command  */
 
static void
send_command (string)
     char *string;
{
  send (string);
  SERIAL_WRITE (es1800_desc, "\r", 1);

#ifndef DEBUG_STDIN
  expect (string, 0);
  expect_prompt ();
#endif

}

/* Send a string
   string - the es1800 command  */

static void
send (string)
     char *string;
{
  if (kiodebug) 
    {
      fprintf (stderr, "Sending: %s\n", string);
    }
  SERIAL_WRITE (es1800_desc, string, strlen (string));
}


/* Read a message from the emulator and store it in BUF. 
   buf    - containing the emulator reply on return
   len    - size of buf  */
 
static void
getmessage (buf, len)
     char *buf;
     int len;
{
  char *bp;
  int c;
  int prompt_found = 0;
  extern kiodebug;

#if defined (LOG_FILE)
  /* This is a convenient place to do this.  The idea is to do it often
     enough that we never lose much data if we terminate abnormally.  */
  fflush (log_file);
#endif

  bp = buf;
  c = readchar ();
  do
    {
      if (c) 
	{
	  if (len-- < 2) /* char and terminaling NULL */
	    {
	      error ("input buffer overrun\n");
	    }
	  *bp++ = c;
	}
      c = readchar ();
      if ((c == '>') && (*(bp - 1) == ' '))
	{
	  prompt_found = 1;
	}
    }
  while (!prompt_found);
  *bp = 0;
  
  if (kiodebug)
    {
      fprintf (stderr,"message received :%s\n", buf);
    }
}

static void
download (instream, from_tty, format)
FILE *instream;
     int from_tty;
     int format;
{
  char c;
  char buf[160];
  int i = 0;

  send_command ("SET #2,$1A");                  /* reset char = ^Z */
  send_command ("SET #3,$11,$13");              /* XON  XOFF */
  if (format == 2)
    {
      send_command ("SET #26,#2");
    }
  else
    {
      send_command ("SET #26,#5");		/* Format=Extended Tekhex */
    }
  send_command ("DFB = $10");
  send_command ("PUR");
  send_command ("CES");
  send ("DNL\r");
  expect ("DNL", 1);
  if (from_tty)
    {
      printf ("    0 records loaded...\r");
    }
  while (fgets (buf, 160, instream))
      {
	send (buf);
	if (from_tty)
	  {
	    printf ("%5d\b\b\b\b\b",++i);
	    fflush (stdout);
	  }
	if ((c = readchar ()) != 006) 
	  {
	    error ("expected ACK");
	  }
      }
  if (from_tty)
    {
      printf ("- All");
    }
}

/* Additional commands */

#if defined (TIOCGETP) && defined (FNDELAY) && defined (EWOULDBLOCK)
#define PROVIDE_TRANSPARENT
#endif

#ifdef PROVIDE_TRANSPARENT
/* Talk directly to the emulator
   FIXME, uses busy wait, and is SUNOS (or at least BSD) specific  */

/*ARGSUSED*/
static void 
es1800_transparent (args, from_tty)
     char *args;
     int from_tty;
{
  int console;
  struct sgttyb modebl;
  int fcflag;
  int cc;
  struct sgttyb console_mode_save;
  int console_fc_save;
  int es1800_fc_save;
  int inputcnt = 80;
  char inputbuf[80];
  int consolecnt = 0;
  char consolebuf[80];
  int es1800_cnt = 0;
  char es1800_buf[80];
  int i;

  dont_repeat ();
  if (es1800_desc == NULL) 
    {
      printf ("No emulator attached, type emulator-command first\n");
      return;
    }

  printf ("\n");
  printf ("You are now communicating directly with the ES 1800 emulator.\n");
  printf ("To leave this mode (transparent mode), press ^E.\n");
  printf ("\n");
  printf (" >");
  fflush (stdout);

  if ((console = open ("/dev/tty", O_RDWR)) == -1) 
    {
      perror_with_name ("/dev/tty:");
    }

  if ((fcflag = fcntl (console, F_GETFL, 0)) == -1)
    {
      perror_with_name ("fcntl console");
    }

  console_fc_save = fcflag;
  fcflag = fcflag | FNDELAY;

  if (fcntl (console, F_SETFL, fcflag) == -1)
    {
      perror_with_name ("fcntl console");
    }

  if (ioctl (console, TIOCGETP, &modebl))
    {
      perror_with_name ("ioctl console");
    }

  console_mode_save = modebl;
  modebl.sg_flags = RAW; 

  if (ioctl (console, TIOCSETP, &modebl))
    {
      perror_with_name ("ioctl console");
    }

  if ((fcflag = fcntl (es1800_desc->fd, F_GETFL, 0)) == -1)
    {
      perror_with_name ("fcntl serial");
    }

  es1800_fc_save = fcflag;
  fcflag = fcflag | FNDELAY;

  if (fcntl (es1800_desc->fd, F_SETFL, fcflag) == -1)
    {
      perror_with_name ("fcntl serial");
    }

  while (1)
    { 
      cc = read (console, inputbuf, inputcnt);
      if (cc != -1)
	{
	  if ((*inputbuf & 0x7f) == 0x05)
	    {
	      break;
	    }
	  for (i = 0; i < cc; )
	    {
	      es1800_buf[es1800_cnt++] = inputbuf[i++];
	    }
	  if ((cc = SERIAL_WRITE (es1800_desc, es1800_buf, es1800_cnt)) == -1)
	    {
	      perror_with_name ("FEL! write:");
	    }
	  es1800_cnt -= cc;
	  if (es1800_cnt && cc) 
	    {
	      for (i = 0; i < es1800_cnt; i++)
		{
		  es1800_buf[i] = es1800_buf[cc+i];
		}
	    }
	}
      else if (errno != EWOULDBLOCK)
	{
	  perror_with_name ("FEL! read:");
	}
      
      cc = read (es1800_desc->fd,inputbuf,inputcnt);
      if (cc != -1)
	{
	  for (i = 0; i < cc; )
	    {
	      consolebuf[consolecnt++] = inputbuf[i++];
	    }
	  if ((cc = write (console,consolebuf,consolecnt)) == -1)
	    {
	      perror_with_name ("FEL! write:");
	    }
	  consolecnt -= cc;
	  if (consolecnt && cc) 
	    {
	      for (i = 0; i < consolecnt; i++)
		{
		  consolebuf[i] = consolebuf[cc+i];
		}
	    }
	}
      else if (errno != EWOULDBLOCK)
	{
	  perror_with_name ("FEL! read:");
	}
    }

  console_fc_save = console_fc_save & !FNDELAY;
  if (fcntl (console, F_SETFL, console_fc_save) == -1)
    {
      perror_with_name ("FEL! fcntl");
    }

  if (ioctl (console, TIOCSETP, &console_mode_save))
    {
      perror_with_name ("FEL! ioctl");
    }

  close (console);

  if (fcntl (es1800_desc->fd, F_SETFL, es1800_fc_save) == -1)
    {
      perror_with_name ("FEL! fcntl");
    }

  printf ("\n");

}
#endif /* PROVIDE_TRANSPARENT */

static void
es1800_init_break (args, from_tty)
     char *args;
     int from_tty;
{
  CORE_ADDR memaddress = 0;
  char buf[PBUFSIZ];
  char base_addr[4];
  char *space_index;
  char *p;
  int k;

  if (args == NULL)
    {
      error_no_arg ("a trap vector");
    }

  if (!(space_index = strchr (args, ' ')))
    {
      error ("Two arguments needed (trap vector and address of break routine).\n");
    }

  *space_index = '\0';

  es1800_break_vec = strtol (args, (char **) NULL, 0);
  es1800_break_address = parse_and_eval_address (space_index + 1);

  es1800_create_break_insn (es1800_break_insn, es1800_break_vec);

  if (m68020)
    {
      send_with_reply ("VBR ", buf, sizeof (buf));
      p = buf;
      for (k = 0; k < 4; k++)
	{
	  if ((p[k*2 + 1] == 0) || (p[k*2 + 2] == 0))
	    {
	      error ("Emulator reply is too short: %s", buf);
	    }
	  base_addr[k] = (fromhex (p[k*2 + 1]) * 16) + fromhex (p[k*2 + 2]);
	}
      /* base addr of exception vector table */
      memaddress =  *((CORE_ADDR *) base_addr);
    }

  memaddress += (es1800_break_vec + 32) * 4;     /* address of trap vector */

  sprintf (buf, "@.L%lx=$%lx", memaddress, es1800_break_address);
  send_command (buf);   /* set the address of the break routine in the */
  		       /* trap vector */
  
  sprintf (buf, "@.L%lx=$4E714E71", es1800_break_address);      /* NOP; NOP */
  send_command (buf);
  sprintf (buf, "@.L%lx=$4E714E73", es1800_break_address + 4);  /* NOP; RTE */
  send_command (buf);
  
  sprintf (buf, "AC2=$%lx", es1800_break_address + 4);
  /* breakpoint at es1800-break_address */
  send_command (buf);
  send_command ("WHEN AC2 THEN BRK");          /* ie in exception routine */

  if (from_tty)
    {
      printf ("Breakpoint (trap $%x) routine at address: %lx\n",
	      es1800_break_vec, es1800_break_address);
    }
}	  

static void
es1800_child_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a child process.");
}

static void
es1800_child_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    {
      error ("Argument given to \"detach\" when remotely debugging.");
    }
  
  pop_target ();
  if (from_tty)
    {
      printf ("Ending debugging the process %d.\n", inferior_pid);
    }
}


/* Define the target subroutine names  */

struct target_ops es1800_ops =
{
  "es1800",			/* to_shortname */
				/* to_longname */
  "Remote serial target in ES1800-emulator protocol",
  				/* to_doc */
  "Remote debugging on the es1800 emulator via a serial line.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",
  es1800_open,			/* to_open */
  es1800_close,			/* to_close */
  es1800_attach,		/* to_attach */
  es1800_detach,		/* to_detach */
  es1800_resume,		/* to_resume */
  NULL,				/* to_wait */
  NULL,				/* to_fetch_registers */
  NULL,				/* to_store_registers */
  es1800_prepare_to_store,	/* to_prepare_to_store */
  es1800_xfer_inferior_memory,	/* to_xfer_memory */
  es1800_files_info,		/* to_files_info */
  es1800_insert_breakpoint,	/* to_insert_breakpoint */
  es1800_remove_breakpoint,	/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  NULL,				/* to_kill */
  es1800_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  es1800_create_inferior,	/* to_create_inferior */
  NULL,				/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  core_stratum,			/* to_stratum */
  0,				/* to_next */
  0,				/* to_has_all_memory */
  1,				/* to_has_memory */
  0,				/* to_has_stack */
  0,				/* to_has_registers */
  0,				/* to_has_execution */
  NULL,				/* to_sections */
  NULL,				/* to_sections_end */
  OPS_MAGIC			/* to_magic (always last) */
};

/* Define the target subroutine names  */

struct target_ops es1800_child_ops =
{
  "es1800_process",		/* to_shortname */
  				/* to_longname */
  "Remote serial target in ES1800-emulator protocol",
				/* to_doc */
  "Remote debugging on the es1800 emulator via a serial line.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",
  es1800_child_open,		/* to_open */
  NULL,				/* to_close */
  es1800_attach,		/* to_attach */
  es1800_child_detach,		/* to_detach */
  es1800_resume,		/* to_resume */
  es1800_wait,			/* to_wait */
  es1800_fetch_register,	/* to_fetch_registers */
  es1800_store_register,	/* to_store_registers */
  es1800_prepare_to_store,	/* to_prepare_to_store */
  es1800_xfer_inferior_memory,	/* to_xfer_memory */
  es1800_files_info,		/* to_files_info */
  es1800_insert_breakpoint,	/* to_insert_breakpoint */
  es1800_remove_breakpoint,	/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  es1800_kill,			/* to_kill */
  es1800_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  es1800_create_inferior,	/* to_create_inferior */
  es1800_mourn_inferior,	/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  NULL,				/* to_sections */
  NULL,				/* to_sections_end */
  OPS_MAGIC			/* to_magic (always last) */
};

void
_initialize_es1800 ()
{
  add_target (&es1800_ops);
  add_target (&es1800_child_ops);
#ifdef PROVIDE_TRANSPARENT
  add_com ("transparent", class_support, es1800_transparent,
	   "Start transparent communication with the ES 1800 emulator.");
#endif /* PROVIDE_TRANSPARENT */
  add_com ("init_break", class_support, es1800_init_break,
	   "Download break routine and initialize break facility on ES 1800");
}
