/* Remote utility routines for the remote server for GDB.
   Copyright (C) 1986, 1989, 1993 Free Software Foundation, Inc.

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

#include "server.h"
#include "terminal.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>

int remote_debug = 0;

static int remote_desc;

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

void
remote_open (name)
     char *name;
{
  int save_fcntl_flags;

  if (!strchr (name, ':'))
    {
      remote_desc = open (name, O_RDWR);
      if (remote_desc < 0)
	perror_with_name ("Could not open remote device");

#ifdef HAVE_TERMIOS
      {
	struct termios termios;
	tcgetattr(remote_desc, &termios);

	termios.c_iflag = 0;
	termios.c_oflag = 0;
	termios.c_lflag = 0;
	termios.c_cflag &= ~(CSIZE|PARENB);
	termios.c_cflag |= CLOCAL | CS8;
	termios.c_cc[VMIN] = 0;
	termios.c_cc[VTIME] = 0;

	tcsetattr(remote_desc, TCSANOW, &termios);
      }
#endif

#ifdef HAVE_TERMIO
      {
	struct termio termio;
	ioctl (remote_desc, TCGETA, &termio);

	termio.c_iflag = 0;
	termio.c_oflag = 0;
	termio.c_lflag = 0;
	termio.c_cflag &= ~(CSIZE|PARENB);
	termio.c_cflag |= CLOCAL | CS8;
	termio.c_cc[VMIN] = 0;
	termio.c_cc[VTIME] = 0;

	ioctl (remote_desc, TCSETA, &termio);
      }
#endif

#ifdef HAVE_SGTTY
      {
	struct sgttyb sg;

	ioctl (remote_desc, TIOCGETP, &sg);
	sg.sg_flags = RAW;
	ioctl (remote_desc, TIOCSETP, &sg);
      }
#endif


    }
  else
    {
      char *port_str;
      int port;
      struct sockaddr_in sockaddr;
      int tmp;
      struct protoent *protoent;
      int tmp_desc;

      port_str = strchr (name, ':');

      port = atoi (port_str + 1);

      tmp_desc = socket (PF_INET, SOCK_STREAM, 0);
      if (tmp_desc < 0)
	perror_with_name ("Can't open socket");

      /* Allow rapid reuse of this port. */
      tmp = 1;
      setsockopt (tmp_desc, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp,
		  sizeof(tmp));

      sockaddr.sin_family = PF_INET;
      sockaddr.sin_port = htons(port);
      sockaddr.sin_addr.s_addr = INADDR_ANY;

      if (bind (tmp_desc, (struct sockaddr *)&sockaddr, sizeof (sockaddr))
	  || listen (tmp_desc, 1))
	perror_with_name ("Can't bind address");

      tmp = sizeof (sockaddr);
      remote_desc = accept (tmp_desc, (struct sockaddr *)&sockaddr, &tmp);
      if (remote_desc == -1)
	perror_with_name ("Accept failed");

      protoent = getprotobyname ("tcp");
      if (!protoent)
	perror_with_name ("getprotobyname");

      /* Enable TCP keep alive process. */
      tmp = 1;
      setsockopt (tmp_desc, SOL_SOCKET, SO_KEEPALIVE, (char *)&tmp, sizeof(tmp));

      /* Tell TCP not to delay small packets.  This greatly speeds up
	 interactive response. */
      tmp = 1;
      setsockopt (remote_desc, protoent->p_proto, TCP_NODELAY,
		  (char *)&tmp, sizeof(tmp));

      close (tmp_desc);		/* No longer need this */

      signal (SIGPIPE, SIG_IGN); /* If we don't do this, then gdbserver simply
				    exits when the remote side dies.  */
    }

#if defined(F_SETFL) && defined (FASYNC)
  save_fcntl_flags = fcntl (remote_desc, F_GETFL, 0);
  fcntl (remote_desc, F_SETFL, save_fcntl_flags | FASYNC);
  disable_async_io ();
#endif /* FASYNC */
  fprintf (stderr, "Remote debugging using %s\n", name);
}

void
remote_close()
{
  close (remote_desc);
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
    error ("Reply contains invalid hex digit");
}

/* Convert number NIB to a hex digit.  */

static int
tohex (nib)
     int nib;
{
  if (nib < 10)
    return '0' + nib;
  else
    return 'a' + nib - 10;
}

/* Send a packet to the remote machine, with error checking.
   The data of the packet is in BUF.  Returns >= 0 on success, -1 otherwise. */

int
putpkt (buf)
     char *buf;
{
  int i;
  unsigned char csum = 0;
  char buf2[2000];
  char buf3[1];
  int cnt = strlen (buf);
  char *p;

  /* Copy the packet into buffer BUF2, encapsulating it
     and giving it a checksum.  */

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

  *p = '\0';

  /* Send it over and over until we get a positive ack.  */

  do
    {
      int cc;

      if (write (remote_desc, buf2, p - buf2) != p - buf2)
	{
	  perror ("putpkt(write)");
	  return -1;
	}

      if (remote_debug)
	printf ("putpkt (\"%s\"); [looking for ack]\n", buf2);
      cc = read (remote_desc, buf3, 1);
      if (remote_debug)
	printf ("[received '%c' (0x%x)]\n", buf3[0], buf3[0]);
      if (cc <= 0)
	{
	  if (cc == 0)
	    fprintf (stderr, "putpkt(read): Got EOF\n");
	  else
	    perror ("putpkt(read)");

	  return -1;
	}
    }
  while (buf3[0] != '+');

  return 1;			/* Success! */
}

/* Come here when we get an input interrupt from the remote side.  This
   interrupt should only be active while we are waiting for the child to do
   something.  About the only thing that should come through is a ^C, which
   will cause us to send a SIGINT to the child.  */

static void
input_interrupt()
{
  int cc;
  char c;

  cc = read (remote_desc, &c, 1);

  if (cc != 1 || c != '\003')
    {
      fprintf(stderr, "input_interrupt, cc = %d c = %d\n", cc, c);
      return;
    }

  kill (inferior_pid, SIGINT);
}

void
enable_async_io()
{
  signal (SIGIO, input_interrupt);
}

void
disable_async_io()
{
  signal (SIGIO, SIG_IGN);
}

/* Returns next char from remote GDB.  -1 if error.  */

static int
readchar ()
{
  static char buf[BUFSIZ];
  static int bufcnt = 0;
  static char *bufp;

  if (bufcnt-- > 0)
    return *bufp++ & 0x7f;

  bufcnt = read (remote_desc, buf, sizeof (buf));

  if (bufcnt <= 0)
    {
      if (bufcnt == 0)
	fprintf (stderr, "readchar: Got EOF\n");
      else
	perror ("readchar");

      return -1;
    }

  bufp = buf;
  bufcnt--;
  return *bufp++ & 0x7f;
}

/* Read a packet from the remote machine, with error checking,
   and store it in BUF.  Returns length of packet, or negative if error. */

int
getpkt (buf)
     char *buf;
{
  char *bp;
  unsigned char csum, c1, c2;
  int c;

  while (1)
    {
      csum = 0;

      while (1)
	{
	  c = readchar ();
	  if (c == '$')
	    break;
	  if (remote_debug)
	    printf ("[getpkt: discarding char '%c']\n", c);
	  if (c < 0)
	    return -1;
	}

      bp = buf;
      while (1)
	{
	  c = readchar ();
	  if (c < 0)
	    return -1;
	  if (c == '#')
	    break;
	  *bp++ = c;
	  csum += c;
	}
      *bp = 0;

      c1 = fromhex (readchar ());
      c2 = fromhex (readchar ());
      
      if (csum == (c1 << 4) + c2)
	break;

      fprintf (stderr, "Bad checksum, sentsum=0x%x, csum=0x%x, buf=%s\n",
	       (c1 << 4) + c2, csum, buf);
      write (remote_desc, "-", 1);
    }

  if (remote_debug)
    printf ("getpkt (\"%s\");  [sending ack] \n", buf);

  write (remote_desc, "+", 1);

  if (remote_debug)
    printf ("[sent ack]\n");
  return bp - buf;
}

void
write_ok (buf)
     char *buf;
{
  buf[0] = 'O';
  buf[1] = 'K';
  buf[2] = '\0';
}

void
write_enn (buf)
     char *buf;
{
  buf[0] = 'E';
  buf[1] = 'N';
  buf[2] = 'N';
  buf[3] = '\0';
}

void
convert_int_to_ascii (from, to, n)
     char *from, *to;
     int n;
{
  int nib;
  char ch;
  while (n--)
    {
      ch = *from++;
      nib = ((ch & 0xf0) >> 4) & 0x0f;
      *to++ = tohex (nib);
      nib = ch & 0x0f;
      *to++ = tohex (nib);
    }
  *to++ = 0;
}


void
convert_ascii_to_int (from, to, n)
     char *from, *to;
     int n;
{
  int nib1, nib2;
  while (n--)
    {
      nib1 = fromhex (*from++);
      nib2 = fromhex (*from++);
      *to++ = (((nib1 & 0x0f) << 4) & 0xf0) | (nib2 & 0x0f);
    }
}

static char *
outreg(regno, buf)
     int regno;
     char *buf;
{
  extern char registers[];
  int regsize = REGISTER_RAW_SIZE (regno);

  *buf++ = tohex (regno >> 4);
  *buf++ = tohex (regno & 0xf);
  *buf++ = ':';
  convert_int_to_ascii (&registers[REGISTER_BYTE (regno)], buf, regsize);
  buf += 2 * regsize;
  *buf++ = ';';

  return buf;
}

void
prepare_resume_reply (buf, status, signo)
     char *buf;
     char status;
     unsigned char signo;
{
  int nib;

  *buf++ = status;

  /* FIXME!  Should be converting this signal number (numbered
     according to the signal numbering of the system we are running on)
     to the signal numbers used by the gdb protocol (see enum target_signal
     in gdb/target.h).  */
  nib = ((signo & 0xf0) >> 4);
  *buf++ = tohex (nib);
  nib = signo & 0x0f;
  *buf++ = tohex (nib);

  if (status == 'T')
    {
      buf = outreg (PC_REGNUM, buf);
      buf = outreg (FP_REGNUM, buf);
      buf = outreg (SP_REGNUM, buf);
#ifdef NPC_REGNUM
      buf = outreg (NPC_REGNUM, buf);
#endif
#ifdef O7_REGNUM
      buf = outreg (O7_REGNUM, buf);
#endif

      /* If the debugger hasn't used any thread features, don't burden it with
	 threads.  If we didn't check this, GDB 4.13 and older would choke.  */
      if (cont_thread != 0)
	{
	  if (old_thread_from_wait != thread_from_wait)
	    {
	      sprintf (buf, "thread:%x;", thread_from_wait);
	      buf += strlen (buf);
	      old_thread_from_wait = thread_from_wait;
	    }
	}
    }
  /* For W and X, we're done.  */
  *buf++ = 0;
}

void
decode_m_packet (from, mem_addr_ptr, len_ptr)
     char *from;
     CORE_ADDR *mem_addr_ptr;
     unsigned int *len_ptr;
{
  int i = 0, j = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  for (j = 0; j < 4; j++)
    {
      if ((ch = from[i++]) == 0)
	break;
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }
}

void
decode_M_packet (from, mem_addr_ptr, len_ptr, to)
     char *from, *to;
     CORE_ADDR *mem_addr_ptr;
     unsigned int *len_ptr;
{
  int i = 0;
  char ch;
  *mem_addr_ptr = *len_ptr = 0;

  while ((ch = from[i++]) != ',')
    {
      *mem_addr_ptr = *mem_addr_ptr << 4;
      *mem_addr_ptr |= fromhex (ch) & 0x0f;
    }

  while ((ch = from[i++]) != ':')
    {
      *len_ptr = *len_ptr << 4;
      *len_ptr |= fromhex (ch) & 0x0f;
    }

  convert_ascii_to_int (&from[i++], to, *len_ptr);
}
