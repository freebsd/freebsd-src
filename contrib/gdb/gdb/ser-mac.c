/* Remote serial interface for local (hardwired) serial ports for Macintosh.
   Copyright 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Stan Shebs.

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
#include "serial.h"

#include <Types.h>
#include <Devices.h>
/* This is the regular Mac Serial.h, but copied to a different name
   so as not to get confused with the GDB serial.h above.  */
#include "MacSerial.h"

/* This is unused for now.  We just return a placeholder. */

struct mac_ttystate
  {
    int bogus;
  };

static int mac_open PARAMS ((serial_t scb, const char *name));
static void mac_raw PARAMS ((serial_t scb));
static int mac_readchar PARAMS ((serial_t scb, int timeout));
static int mac_setbaudrate PARAMS ((serial_t scb, int rate));
static int mac_write PARAMS ((serial_t scb, const char *str, int len));
static void mac_close PARAMS ((serial_t scb));
static serial_ttystate mac_get_tty_state PARAMS ((serial_t scb));
static int mac_set_tty_state PARAMS ((serial_t scb, serial_ttystate state));
static char *aptr PARAMS ((short p));

short input_refnum;
short output_refnum;

char *mac_input_buffer;
char *mac_output_buffer;

static int
mac_open (scb, name)
     serial_t scb;
     const char *name;
{
  OSErr err;

  /* Alloc buffer space first - that way any allocation failures are
     intercepted before the serial driver gets involved. */
  if (mac_input_buffer == NULL)
    mac_input_buffer = (char *) xmalloc (4096);
  /* Match on a name and open a port. */
  if (strcmp (name, "modem") == 0)
    {
      err = OpenDriver ("\p.AIn", &input_refnum);
      if (err != 0)
	{
	  return (-1);
	}
      err = OpenDriver ("\p.AOut", &output_refnum);
      if (err != 0)
	{
	  CloseDriver (input_refnum);
	  return (-1);
	}
    }
  else if (strcmp (name, "printer") == 0)
    {
      err = OpenDriver ("\p.BIn", &input_refnum);
      if (err != 0)
	{
	  return (-1);
	}
      err = OpenDriver ("\p.BOut", &output_refnum);
      if (err != 0)
	{
	  CloseDriver (input_refnum);
	  return (-1);
	}
      /* fake */
      scb->fd = 1;
      return 0;
    }
  else
    {
      error ("You must specify a valid serial port name; your choices are `modem' or `printer'.");
      errno = ENOENT;
      return (-1);
    }
  /* We got something open. */
  if (1 /* using custom buffer */)
    SerSetBuf (input_refnum, mac_input_buffer, 4096);
  /* Set to a GDB-preferred state. */
  SerReset (input_refnum,  stop10|noParity|data8|baud9600);
  SerReset (output_refnum, stop10|noParity|data8|baud9600);
  {
    CntrlParam cb;
    struct SerShk *handshake;

    cb.ioCRefNum = output_refnum;
    cb.csCode = 14;
    handshake = (struct SerShk *) &cb.csParam[0];
    handshake->fXOn = 0;
    handshake->fCTS = 0;
    handshake->xOn = 0;
    handshake->xOff = 0;
    handshake->errs = 0;
    handshake->evts = 0;
    handshake->fInX = 0;
    handshake->fDTR = 0;
    err = PBControl ((ParmBlkPtr) &cb, 0);
    if (err < 0)
      return (-1);
  }
  /* fake */
  scb->fd = 1;
  return 0;
}

static int
mac_noop (scb)
     serial_t scb;
{
  return 0;
}

static void
mac_raw (scb)
     serial_t scb;
{
  /* Always effectively in raw mode. */
}

/* Read a character with user-specified timeout.  TIMEOUT is number of seconds
   to wait, or -1 to wait forever.  Use timeout of 0 to effect a poll.  Returns
   char if successful.  Returns -2 if timeout expired, EOF if line dropped
   dead, or -3 for any other error (see errno in that case). */

static int
mac_readchar (scb, timeout)
     serial_t scb;
     int timeout;
{
  int status, n;
  /* time_t */ unsigned long start_time, now;
  OSErr err;
  CntrlParam cb;
  IOParam pb;

  if (scb->bufcnt-- > 0)
    return *scb->bufp++;

  time (&start_time);

  while (1)
    {
      cb.ioCRefNum = input_refnum;
      cb.csCode = 2;
      err = PBStatus ((ParmBlkPtr) &cb, 0);
      if (err < 0)
	return SERIAL_ERROR;
      n = *((long *) &cb.csParam[0]);
      if (n > 0)
	{
	  pb.ioRefNum = input_refnum;
	  pb.ioBuffer = (Ptr) (scb->buf);
	  pb.ioReqCount = (n > 64 ? 64 : n);
	  err = PBRead ((ParmBlkPtr) &pb, 0);
	  if (err < 0)
	    return SERIAL_ERROR;
	  scb->bufcnt = pb.ioReqCount;
	  scb->bufcnt--;
	  scb->bufp = scb->buf;
	  return *scb->bufp++;
	}
      else if (timeout == 0)
	return SERIAL_TIMEOUT;
      else if (timeout == -1)
	;
      else
	{
	  time (&now);
	  if (now > start_time + timeout)
	    return SERIAL_TIMEOUT;
	}
      PROGRESS (1);
    }
}

/* mac_{get set}_tty_state() are both dummys to fill out the function
   vector.  Someday, they may do something real... */

static serial_ttystate
mac_get_tty_state (scb)
     serial_t scb;
{
  struct mac_ttystate *state;

  state = (struct mac_ttystate *) xmalloc (sizeof *state);

  return (serial_ttystate) state;
}

static int
mac_set_tty_state (scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  return 0;
}

static int
mac_noflush_set_tty_state (scb, new_ttystate, old_ttystate)
     serial_t scb;
     serial_ttystate new_ttystate;
     serial_ttystate old_ttystate;
{
  return 0;
}

static void
mac_print_tty_state (scb, ttystate)
     serial_t scb;
     serial_ttystate ttystate;
{
  /* Nothing to print.  */
  return;
}

/* If there is a tricky formula to relate real baud rates
   to what the serial driver wants, we should use it.  Until
   we get one, this table will have to do.  */

static struct {
  int real_rate;
  int bits;
} mac_baud_rate_table[] = {
  { 57600, baud57600 },
  { 38400, 1 },
  { 19200, baud19200 },
  { 9600, baud9600 },
  { 7200, baud7200 },
  { 4800, baud4800 },
  { 3600, baud3600 },
  { 2400, baud2400 },
  { 1800, baud1800 },
  { 1200, baud1200 },
  { 600, baud600 },
  { 300, baud300 },
  { 0, 0 }
};

static int
mac_set_baud_rate (scb, rate)
     serial_t scb;
     int rate;
{
  int i, bits;

  for (i = 0; mac_baud_rate_table[i].real_rate != 0; ++i)
    {
      if (mac_baud_rate_table[i].real_rate == rate)
	{
	  bits = mac_baud_rate_table[i].bits;
	  break;
	}
    }
  SerReset (input_refnum,  stop10|noParity|data8|bits);
  SerReset (output_refnum, stop10|noParity|data8|bits);
}

static int
mac_set_stop_bits (scb, num)
     serial_t scb;
     int num;
{
  return 0;
}

int first_mac_write = 0;

static int
mac_write (scb, str, len)
     serial_t scb;
     const char *str;
     int len;
{
  OSErr err;
  IOParam pb;

  if (first_mac_write++ < 4)
    {
      sleep (1);
    }
  pb.ioRefNum = output_refnum;
  pb.ioBuffer = (Ptr) str;
  pb.ioReqCount = len;
  err = PBWrite ((ParmBlkPtr) &pb, 0);
  if (err < 0)
    {
      return 1;
    }
  return 0;
}

static void
mac_close (serial_t scb)
{
  if (input_refnum)
    {
      if (1 /* custom buffer */)
	SerSetBuf (input_refnum, mac_input_buffer, 0);
      CloseDriver (input_refnum);
      input_refnum = 0;
    }
  if (output_refnum)
    {
      if (0 /* custom buffer */)
	SetSetBuf (input_refnum, mac_output_buffer, 0);
      CloseDriver (output_refnum);
      output_refnum = 0;
    }
}

static struct serial_ops mac_ops =
{
  "hardwire",
  0,
  mac_open,
  mac_close,
  mac_readchar,
  mac_write,
  mac_noop,			/* flush output */
  mac_noop,			/* flush input */
  mac_noop,			/* send break -- currently only for nindy */
  mac_raw,
  mac_get_tty_state,
  mac_set_tty_state,
  mac_print_tty_state,
  mac_noflush_set_tty_state,
  mac_set_baud_rate,
  mac_set_stop_bits,
};

void
_initialize_ser_mac ()
{
  serial_add_interface (&mac_ops);
}
