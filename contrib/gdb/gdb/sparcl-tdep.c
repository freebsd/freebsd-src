/* Target dependent code for the Fujitsu SPARClite for GDB, the GNU debugger.
   Copyright 1994, 1995, 1996  Free Software Foundation, Inc.

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

#include "defs.h"
#include "gdbcore.h"
#include "breakpoint.h"
#include "target.h"
#include "serial.h"
#include <sys/types.h>
#include <sys/time.h>

#if defined(__GO32__) || defined(WIN32)
#undef HAVE_SOCKETS
#else
#define HAVE_SOCKETS
#endif


#ifdef HAVE_SOCKETS	 
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

extern struct target_ops sparclite_ops;	/* Forward decl */
extern struct target_ops remote_ops;

static char *remote_target_name = NULL;
static serial_t remote_desc = NULL;
static int serial_flag;
#ifdef HAVE_SOCKETS
static int udp_fd = -1;
#endif

static serial_t open_tty PARAMS ((char *name));
static int send_resp PARAMS ((serial_t desc, char c));
static void close_tty PARAMS ((int ignore));
#ifdef HAVE_SOCKETS
static int recv_udp_buf PARAMS ((int fd, unsigned char *buf, int len, int timeout));
static int send_udp_buf PARAMS ((int fd, unsigned char *buf, int len));
#endif
static void sparclite_open PARAMS ((char *name, int from_tty));
static void sparclite_close PARAMS ((int quitting));
static void download PARAMS ((char *target_name, char *args, int from_tty,
			      void (*write_routine) (bfd *from_bfd,
						     asection *from_sec,
						     file_ptr from_addr,
						     bfd_vma to_addr, int len),
			      void (*start_routine) (bfd_vma entry)));
static void sparclite_serial_start PARAMS ((bfd_vma entry));
static void sparclite_serial_write PARAMS ((bfd *from_bfd, asection *from_sec,
					    file_ptr from_addr,
					    bfd_vma to_addr, int len));
#ifdef HAVE_SOCKETS
static unsigned short calc_checksum PARAMS ((unsigned char *buffer,
					     int count));
static void sparclite_udp_start PARAMS ((bfd_vma entry));
static void sparclite_udp_write PARAMS ((bfd *from_bfd, asection *from_sec,
					 file_ptr from_addr, bfd_vma to_addr,
					 int len));
#endif
static void sparclite_download PARAMS ((char *filename, int from_tty));

#define DDA2_SUP_ASI		0xb000000
#define DDA1_SUP_ASI		0xb0000

#define DDA2_ASI_MASK 		0xff000000
#define DDA1_ASI_MASK 		0xff0000 
#define DIA2_SUP_MODE 		0x8000
#define DIA1_SUP_MODE 		0x4000
#define DDA2_ENABLE 		0x100
#define DDA1_ENABLE 		0x80
#define DIA2_ENABLE 		0x40
#define DIA1_ENABLE 		0x20
#define DSINGLE_STEP 		0x10
#define DDV_TYPE_MASK 		0xc
#define DDV_TYPE_LOAD 		0x0
#define DDV_TYPE_STORE 		0x4
#define DDV_TYPE_ACCESS 	0x8
#define DDV_TYPE_ALWAYS		0xc
#define DDV_COND		0x2
#define DDV_MASK		0x1

int
sparclite_insert_watchpoint (addr, len, type)
     CORE_ADDR addr;
     int len;
     int type;
{
  CORE_ADDR dcr;

  dcr = read_register (DCR_REGNUM);

  if (!(dcr & DDA1_ENABLE))
    {
      write_register (DDA1_REGNUM, addr);
      dcr &= ~(DDA1_ASI_MASK | DDV_TYPE_MASK);
      dcr |= (DDA1_SUP_ASI | DDA1_ENABLE);
      if (type == 1)
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_LOAD & (~DDV_COND & ~DDV_MASK));
	}   
      else if (type == 0)
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_STORE & (~DDV_COND & ~DDV_MASK));
	}
      else
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_ACCESS);
	}
      write_register (DCR_REGNUM, dcr);
    }
  else if (!(dcr & DDA2_ENABLE))
    {
      write_register (DDA2_REGNUM, addr);
      dcr &= ~(DDA2_ASI_MASK & DDV_TYPE_MASK);
      dcr |= (DDA2_SUP_ASI | DDA2_ENABLE);
      if (type == 1)
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_LOAD & ~DDV_COND & ~DDV_MASK);
	}
      else if (type == 0)
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_STORE & ~DDV_COND & ~DDV_MASK);
	}
      else
	{
	  write_register (DDV1_REGNUM, 0);
	  write_register (DDV2_REGNUM, 0xffffffff);
	  dcr |= (DDV_TYPE_ACCESS);
	}
      write_register (DCR_REGNUM, dcr);
    }
  else
    return -1;

  return 0;
} 

int
sparclite_remove_watchpoint (addr, len, type)
     CORE_ADDR addr;
     int len;
     int type;
{
  CORE_ADDR dcr, dda1, dda2;

  dcr = read_register (DCR_REGNUM);
  dda1 = read_register (DDA1_REGNUM);
  dda2 = read_register (DDA2_REGNUM);

  if ((dcr & DDA1_ENABLE) && addr == dda1)
    write_register (DCR_REGNUM, (dcr & ~DDA1_ENABLE));
  else if ((dcr & DDA2_ENABLE) && addr == dda2)
    write_register (DCR_REGNUM, (dcr & ~DDA2_ENABLE));
  else
    return -1;

  return 0;
}

int
sparclite_insert_hw_breakpoint (addr, len)
     CORE_ADDR addr;
     int len;
{
  CORE_ADDR dcr;

  dcr = read_register (DCR_REGNUM);
  
  if (!(dcr & DIA1_ENABLE))
    {
      write_register (DIA1_REGNUM, addr);
      write_register (DCR_REGNUM, (dcr | DIA1_ENABLE | DIA1_SUP_MODE));
    }
  else if (!(dcr & DIA2_ENABLE))
    {
      write_register (DIA2_REGNUM, addr);
      write_register (DCR_REGNUM, (dcr | DIA2_ENABLE | DIA2_SUP_MODE));
    }
  else
    return -1;

  return 0;
}

int
sparclite_remove_hw_breakpoint (addr, shadow)
     CORE_ADDR addr;
     int shadow;
{
  CORE_ADDR dcr, dia1, dia2;

  dcr = read_register (DCR_REGNUM);
  dia1 = read_register (DIA1_REGNUM);
  dia2 = read_register (DIA2_REGNUM);
  
  if ((dcr & DIA1_ENABLE) && addr == dia1)
    write_register (DCR_REGNUM, (dcr & ~DIA1_ENABLE));
  else if ((dcr & DIA2_ENABLE) && addr == dia2)
    write_register (DCR_REGNUM, (dcr & ~DIA2_ENABLE));
  else
    return -1;

  return 0;
}

int
sparclite_check_watch_resources (type, cnt, ot)
     int type;
     int cnt;
     int ot;
{
  if (type == bp_hardware_breakpoint)
    {
      if (TARGET_HW_BREAK_LIMIT == 0)
	return 0;
      else if (cnt <= TARGET_HW_BREAK_LIMIT)
	return 1;
    }
  else
    {
      if (TARGET_HW_WATCH_LIMIT == 0)
	return 0;
      else if (ot)
	return -1;
      else if (cnt <= TARGET_HW_WATCH_LIMIT)
	return 1;
    }
  return -1;
}

CORE_ADDR
sparclite_stopped_data_address ()
{
  CORE_ADDR dsr, dda1, dda2;

  dsr = read_register (DSR_REGNUM);
  dda1 = read_register (DDA1_REGNUM);
  dda2 = read_register (DDA2_REGNUM);

  if (dsr & 0x10)
    return dda1;
  else if (dsr & 0x20)
    return dda2;
  else
    return 0;
}

static serial_t
open_tty (name)
     char *name;
{
  serial_t desc;

  desc = SERIAL_OPEN (name);
  if (!desc)
    perror_with_name (name);

  if (baud_rate != -1)
    {
      if (SERIAL_SETBAUDRATE (desc, baud_rate))
	{
	  SERIAL_CLOSE (desc);
	  perror_with_name (name);
	}
    }

  SERIAL_RAW (desc);

  SERIAL_FLUSH_INPUT (desc);

  return desc;
}

/* Read a single character from the remote end, masking it down to 7 bits. */

static int
readchar (desc, timeout)
     serial_t desc;
     int timeout;
{
  int ch;

  ch = SERIAL_READCHAR (desc, timeout);

  switch (ch)
    {
    case SERIAL_EOF:
      error ("SPARClite remote connection closed");
    case SERIAL_ERROR:
      perror_with_name ("SPARClite communication error");
    case SERIAL_TIMEOUT:
      error ("SPARClite remote timeout");
    default:
      return ch;
    }
}

static int
send_resp (desc, c)
     serial_t desc;
     char c;
{
  SERIAL_WRITE (desc, &c, 1);
  return readchar (desc, 2);
}

static void
close_tty (ignore)
     int ignore;
{
  if (!remote_desc)
    return;

  SERIAL_CLOSE (remote_desc);

  remote_desc = NULL;
}

#ifdef HAVE_SOCKETS
static int
recv_udp_buf (fd, buf, len, timeout)
     int fd, len;
     unsigned char *buf;
     int timeout;
{
  int cc;
  fd_set readfds;

  FD_ZERO (&readfds);
  FD_SET (fd, &readfds);

  if (timeout >= 0)
    {
      struct timeval timebuf;

      timebuf.tv_sec = timeout;
      timebuf.tv_usec = 0;
      cc = select (fd + 1, &readfds, 0, 0, &timebuf);
    }
  else
    cc = select (fd + 1, &readfds, 0, 0, 0);

  if (cc == 0)
    return 0;

  if (cc != 1)
    perror_with_name ("recv_udp_buf: Bad return value from select:");

  cc = recv (fd, buf, len, 0);

  if (cc < 0)
    perror_with_name ("Got an error from recv: ");
}

static int
send_udp_buf (fd, buf, len)
     int fd, len;
     unsigned char *buf;
{
  int cc;

  cc = send (fd, buf, len, 0);

  if (cc == len)
    return;

  if (cc < 0)
    perror_with_name ("Got an error from send: ");

  error ("Short count in send: tried %d, sent %d\n", len, cc);
}
#endif /* __GO32__ */

static void
sparclite_open (name, from_tty)
     char *name;
     int from_tty;
{
  struct cleanup *old_chain;
  int c;
  char *p;

  if (!name)
    error ("You need to specify what device or hostname is associated with the SparcLite board.");

  target_preopen (from_tty);

  unpush_target (&sparclite_ops);

  if (remote_target_name)
    free (remote_target_name);

  remote_target_name = strsave (name);

  /* We need a 'serial' or 'udp' keyword to disambiguate host:port, which can
     mean either a serial port on a terminal server, or the IP address of a
     SPARClite demo board.  If there's no colon, then it pretty much has to be
     a local device (except for DOS... grrmble) */

  p = strchr (name, ' ');

  if (p)
    {
      *p++ = '\000';
      while ((*p != '\000') && isspace (*p)) p++;

      if (strncmp (name, "serial", strlen (name)) == 0)
	serial_flag = 1;
      else if (strncmp (name, "udp", strlen (name)) == 0)
	serial_flag = 0;
      else
	error ("Must specify either `serial' or `udp'.");
    }
  else
    {
      p = name;

      if (!strchr (name, ':'))
	serial_flag = 1;	/* No colon is unambiguous (local device) */
      else
	error ("Usage: target sparclite serial /dev/ttyb\n\
or: target sparclite udp host");
    }

  if (serial_flag)
    {
      remote_desc = open_tty (p);

      old_chain = make_cleanup (close_tty, 0);

      c = send_resp (remote_desc, 0x00);

      if (c != 0xaa)
	error ("Unknown response (0x%x) from SparcLite.  Try resetting the board.",
	       c);

      c = send_resp (remote_desc, 0x55);

      if (c != 0x55)
	error ("Sparclite appears to be ill.");
    }
  else
    {
#ifdef HAVE_SOCKETS
      struct hostent *he;
      struct sockaddr_in sockaddr;
      unsigned char buffer[100];
      int cc;

      /* Setup the socket.  Must be raw UDP. */

      he = gethostbyname (p);

      if (!he)
	error ("No such host %s.", p);

      udp_fd = socket (PF_INET, SOCK_DGRAM, 0);

      old_chain = make_cleanup (close, udp_fd);

      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons(7000);
      memcpy (&sockaddr.sin_addr.s_addr, he->h_addr, sizeof (struct in_addr));

      if (connect (udp_fd, &sockaddr, sizeof(sockaddr)))
	perror_with_name ("Connect failed");

      buffer[0] = 0x5;
      buffer[1] = 0;

      send_udp_buf (udp_fd, buffer, 2);	/* Request version */
      cc = recv_udp_buf (udp_fd, buffer, sizeof(buffer), 5); /* Get response */
      if (cc == 0)
	error ("SPARClite isn't responding.");

      if (cc < 3)
	error ("SPARClite appears to be ill.");
#else
      error ("UDP downloading is not supported for DOS hosts.");
#endif /* __GO32__ */
    }

  printf_unfiltered ("[SPARClite appears to be alive]\n");

  push_target (&sparclite_ops);

  discard_cleanups (old_chain);

  return;
}

static void
sparclite_close (quitting)
     int quitting;
{
  if (serial_flag)
    close_tty (0);
#ifdef HAVE_SOCKETS
  else
    if (udp_fd != -1)
      close (udp_fd);
#endif
}

#define LOAD_ADDRESS 0x40000000

static void
download (target_name, args, from_tty, write_routine, start_routine)
     char *target_name;
     char *args;
     int from_tty;
     void (*write_routine)();
     void (*start_routine)();
{
  struct cleanup *old_chain;
  asection *section;
  bfd *pbfd;
  bfd_vma entry;
  int i;
#define WRITESIZE 1024
  char *filename;
  int quiet;
  int nostart;

  quiet = 0;
  nostart = 0;
  filename = NULL;

  while (*args != '\000')
    {
      char *arg;

      while (isspace (*args)) args++;

      arg = args;

      while ((*args != '\000') && !isspace (*args)) args++;

      if (*args != '\000')
	*args++ = '\000';

      if (*arg != '-')
	filename = arg;
      else if (strncmp (arg, "-quiet", strlen (arg)) == 0)
	quiet = 1;
      else if (strncmp (arg, "-nostart", strlen (arg)) == 0)
	nostart = 1;
      else
	error ("unknown option `%s'", arg);
    }

  if (!filename)
    filename = get_exec_file (1);

  pbfd = bfd_openr (filename, gnutarget);
  if (pbfd == NULL)
    {
      perror_with_name (filename);
      return;
    }
  old_chain = make_cleanup (bfd_close, pbfd);

  if (!bfd_check_format (pbfd, bfd_object)) 
    error ("\"%s\" is not an object file: %s", filename,
	   bfd_errmsg (bfd_get_error ()));

  for (section = pbfd->sections; section; section = section->next) 
    {
      if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
	{
	  bfd_vma section_address;
	  bfd_size_type section_size;
	  file_ptr fptr;

	  section_address = bfd_get_section_vma (pbfd, section);
	  /* Adjust sections from a.out files, since they don't
	     carry their addresses with.  */
	  if (bfd_get_flavour (pbfd) == bfd_target_aout_flavour)
	    section_address += LOAD_ADDRESS;

	  section_size = bfd_get_section_size_before_reloc (section);

	  if (!quiet)
	    printf_filtered ("[Loading section %s at 0x%x (%d bytes)]\n",
			     bfd_get_section_name (pbfd, section),
			     section_address,
			     section_size);

	  fptr = 0;
	  while (section_size > 0)
	    {
	      int count;
	      static char inds[] = "|/-\\";
	      static int k = 0;

	      QUIT;

	      count = min (section_size, WRITESIZE);

	      write_routine (pbfd, section, fptr, section_address, count);

	      if (!quiet)
		{
		  printf_unfiltered ("\r%c", inds[k++ % 4]);
		  gdb_flush (gdb_stdout);
		}

	      section_address += count;
	      fptr += count;
	      section_size -= count;
	    }
	}
    }

  if (!nostart)
    {
      entry = bfd_get_start_address (pbfd);

      if (!quiet)
	printf_unfiltered ("[Starting %s at 0x%x]\n", filename, entry);

      start_routine (entry);
    }

  do_cleanups (old_chain);
}

static void
sparclite_serial_start (entry)
     bfd_vma entry;
{
  char buffer[5];
  int i;

  buffer[0] = 0x03;
  store_unsigned_integer (buffer + 1, 4, entry);

  SERIAL_WRITE (remote_desc, buffer, 1 + 4);
  i = readchar (remote_desc, 2);
  if (i != 0x55)
    error ("Can't start SparcLite.  Error code %d\n", i);
}

static void
sparclite_serial_write (from_bfd, from_sec, from_addr, to_addr, len)
     bfd *from_bfd;
     asection *from_sec;
     file_ptr from_addr;
     bfd_vma to_addr;
     int len;
{
  char buffer[4 + 4 + WRITESIZE]; /* addr + len + data */
  unsigned char checksum;
  int i;

  store_unsigned_integer (buffer, 4, to_addr); /* Address */
  store_unsigned_integer (buffer + 4, 4, len); /* Length */

  bfd_get_section_contents (from_bfd, from_sec, buffer + 8, from_addr, len);

  checksum = 0;
  for (i = 0; i < len; i++)
    checksum += buffer[8 + i];

  i = send_resp (remote_desc, 0x01);

  if (i != 0x5a)
    error ("Bad response from load command (0x%x)", i);

  SERIAL_WRITE (remote_desc, buffer, 4 + 4 + len);
  i = readchar (remote_desc, 2);

  if (i != checksum)
    error ("Bad checksum from load command (0x%x)", i);
}

#ifdef HAVE_SOCKETS

static unsigned short
calc_checksum (buffer, count)
     unsigned char *buffer;
     int count;
{
  unsigned short checksum;

  checksum = 0;
  for (; count > 0; count -= 2, buffer += 2)
    checksum += (*buffer << 8) | *(buffer + 1);

  if (count != 0)
    checksum += *buffer << 8;

  return checksum;
}

static void
sparclite_udp_start (entry)
     bfd_vma entry;
{
  unsigned char buffer[6];
  int i;

  buffer[0] = 0x3;
  buffer[1] = 0;
  buffer[2] = entry >> 24;
  buffer[3] = entry >> 16;
  buffer[4] = entry >> 8;
  buffer[5] = entry;

  send_udp_buf (udp_fd, buffer, 6); /* Send start addr */
  i = recv_udp_buf (udp_fd, buffer, sizeof(buffer), -1); /* Get response */

  if (i < 1 || buffer[0] != 0x55)
    error ("Failed to take start address.");
}

static void
sparclite_udp_write (from_bfd, from_sec, from_addr, to_addr, len)
     bfd *from_bfd;
     asection *from_sec;
     file_ptr from_addr;
     bfd_vma to_addr;
     int len;
{
  unsigned char buffer[2000];
  unsigned short checksum;
  static int pkt_num = 0;
  static unsigned long old_addr = -1;
  int i;

  while (1)
    {
      if (to_addr != old_addr)
	{
	  buffer[0] = 0x1;	/* Load command */
	  buffer[1] = 0x1;	/* Loading address */
	  buffer[2] = to_addr >> 24;
	  buffer[3] = to_addr >> 16;
	  buffer[4] = to_addr >> 8;
	  buffer[5] = to_addr;

	  checksum = 0;
	  for (i = 0; i < 6; i++)
	    checksum += buffer[i];
	  checksum &= 0xff;

	  send_udp_buf (udp_fd, buffer, 6);
	  i = recv_udp_buf (udp_fd, buffer, sizeof buffer, -1);

	  if (i < 1)
	    error ("Got back short checksum for load addr.");

	  if (checksum != buffer[0])
	    error ("Got back bad checksum for load addr.");

	  pkt_num = 0;		/* Load addr resets packet seq # */
	  old_addr = to_addr;
	}

      bfd_get_section_contents (from_bfd, from_sec, buffer + 6, from_addr,
				len);

      checksum = calc_checksum (buffer + 6, len);

      buffer[0] = 0x1;		/* Load command */
      buffer[1] = 0x2;		/* Loading data */
      buffer[2] = pkt_num >> 8;
      buffer[3] = pkt_num;
      buffer[4] = checksum >> 8;
      buffer[5] = checksum;

      send_udp_buf (udp_fd, buffer, len + 6);
      i = recv_udp_buf (udp_fd, buffer, sizeof buffer, 3);

      if (i == 0)
	{
	  fprintf_unfiltered (gdb_stderr, "send_data: timeout sending %d bytes to address 0x%x retrying\n", len, to_addr);
	  continue;
	}

      if (buffer[0] != 0xff)
	error ("Got back bad response for load data.");

      old_addr += len;
      pkt_num++;

      return;
    }
}

#endif /* __GO32__ */

static void
sparclite_download (filename, from_tty)
     char *filename;
     int from_tty;
{
  if (!serial_flag)
#ifdef HAVE_SOCKETS
    download (remote_target_name, filename, from_tty, sparclite_udp_write,
	      sparclite_udp_start);
#else
    abort ();			/* sparclite_open should prevent this! */
#endif
  else
    download (remote_target_name, filename, from_tty, sparclite_serial_write,
	      sparclite_serial_start);
}

/* Define the target subroutine names */

static struct target_ops sparclite_ops =
{
  "sparclite",			/* to_shortname */
  "SPARClite remote target",	/* to_longname */
  "Use a remote SPARClite target board via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).",  /* to_doc */
  sparclite_open,		/* to_open */
  sparclite_close,		/* to_close */
  0,				/* to_attach */
  0,				/* to_detach */
  0,				/* to_resume */
  0,				/* to_wait */
  0,				/* to_fetch_registers */
  0,				/* to_store_registers */
  0,				/* to_prepare_to_store */
  0,				/* to_xfer_memory */
  0,				/* to_files_info */
  0,				/* to_insert_breakpoint */
  0,				/* to_remove_breakpoint */
  0,				/* to_terminal_init */
  0,				/* to_terminal_inferior */
  0,				/* to_terminal_ours_for_output */
  0,				/* to_terminal_ours */
  0,				/* to_terminal_info */
  0,				/* to_kill */
  sparclite_download,		/* to_load */
  0,				/* to_lookup_symbol */
  0,				/* to_create_inferior */
  0,				/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  download_stratum,		/* to_stratum */
  0,				/* to_next */
  0,				/* to_has_all_memory */
  0,				/* to_has_memory */
  0,				/* to_has_stack */
  0,				/* to_has_registers */
  0,				/* to_has_execution */
  0,				/* sections */
  0,				/* sections_end */
  OPS_MAGIC			/* to_magic */
  };

void
_initialize_sparcl_tdep ()
{
  add_target (&sparclite_ops);
}
