/* conn.h
   Header file for routines which manipulate connections.

   Copyright (C) 1991, 1992, 1993, 1994 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#ifndef CONN_H

#define CONN_H

#if ANSI_C
/* These structures are used in prototypes but are not defined in this
   header file.  */
struct uuconf_system;
struct uuconf_dialer;
struct uuconf_chat;
#endif

/* This structure represents a connection.  */

struct sconnection
{
  /* Pointer to command table for this type of connection.  */
  const struct sconncmds *qcmds;
  /* Pointer to system dependent information.  */
  pointer psysdep;
  /* Pointer to system independent information.  */
  struct uuconf_port *qport;
};

/* Whether fconn_dial got a dialer.  */

enum tdialerfound
{
  /* Did not find a dialer.  */
  DIALERFOUND_FALSE,
  /* Found a dialer which does not need to be freed.  */
  DIALERFOUND_TRUE,
  /* Found a dialer which does need to be freed.  */
  DIALERFOUND_FREE
};

/* Parity settings to pass to fconn_set.  */

enum tparitysetting
{
  /* Do not change output parity generation.  */
  PARITYSETTING_DEFAULT,
  /* No parity (all eight output bits used).  */
  PARITYSETTING_NONE,
  /* Even parity.  */
  PARITYSETTING_EVEN,
  /* Odd parity.  */
  PARITYSETTING_ODD,
  /* Mark parity.  */
  PARITYSETTING_MARK,
  /* Space parity.  */
  PARITYSETTING_SPACE
};

/* Type of strip control argument to fconn_set.  */

enum tstripsetting
{
  /* Do not change the stripping of input characters.  */
  STRIPSETTING_DEFAULT,
  /* Do not strip input characters to seven bits.  */
  STRIPSETTING_EIGHTBITS,
  /* Strip input characters to seven bits.  */
  STRIPSETTING_SEVENBITS
};

/* Type of XON/XOFF control argument to fconn_set.  */

enum txonxoffsetting
{
  /* Do not change XON/XOFF handshake setting.  */
  XONXOFF_DEFAULT,
  /* Do not do XON/XOFF handshaking.  */
  XONXOFF_OFF,
  /* Do XON/XOFF handshaking.  */
  XONXOFF_ON
};

/* A command table holds the functions which implement actions for
   each different kind of connection.  */

struct sconncmds
{
  /* Free up a connection.  */
  void (*pufree) P((struct sconnection *qconn));
  /* Lock the connection.  The fin argument is TRUE if the connection
     is to be used for an incoming call.  May be NULL.  */
  boolean (*pflock) P((struct sconnection *qconn, boolean fin));
  /* Unlock the connection.  May be NULL.  */
  boolean (*pfunlock) P((struct sconnection *qconn));
  /* Open the connection.  */
  boolean (*pfopen) P((struct sconnection *qconn, long ibaud,
		       boolean fwait));
  /* Close the connection.  */
  boolean (*pfclose) P((struct sconnection *qconn,
			pointer puuconf,
			struct uuconf_dialer *qdialer,
			boolean fsuccess));
  /* Dial a number on a connection.  This set *qdialer to the dialer
     used, if any, and sets *ptdialerfound appropriately.  The qsys
     and zphone arguments are for the chat script.  This field may be
     NULL.  */
  boolean (*pfdial) P((struct sconnection *qconn, pointer puuconf,
		       const struct uuconf_system *qsys,
		       const char *zphone,
		       struct uuconf_dialer *qdialer,
		       enum tdialerfound *ptdialerfound));
  /* Read data from a connection, with a timeout in seconds.  When
     called *pclen is the length of the buffer; on successful return
     *pclen is the number of bytes read into the buffer.  The cmin
     argument is the minimum number of bytes to read before returning
     ahead of a timeout.  */
  boolean (*pfread) P((struct sconnection *qconn, char *zbuf, size_t *pclen,
		       size_t cmin, int ctimeout, boolean freport));
  /* Write data to the connection.  */
  boolean (*pfwrite) P((struct sconnection *qconn, const char *zbuf,
			size_t clen));
  /* Read and write data to the connection.  This reads and writes
     data until either all passed in data has been written or the read
     buffer has been filled.  When called *pcread is the size of the
     read buffer and *pcwrite is the number of bytes to write; on
     successful return *pcread is the number of bytes read and
     *pcwrite is the number of bytes written.  */
  boolean (*pfio) P((struct sconnection *qconn, const char *zwrite,
		     size_t *pcwrite, char *zread, size_t *pcread));
  /* Send a break character.  This field may be NULL.  */
  boolean (*pfbreak) P((struct sconnection *qconn));
  /* Change the connection setting.  This field may be NULL.  */
  boolean (*pfset) P((struct sconnection *qconn,
		      enum tparitysetting tparity,
		      enum tstripsetting tstrip,
		      enum txonxoffsetting txonxoff));
  /* Require or ignore carrer.  This field may be NULL.  */
  boolean (*pfcarrier) P((struct sconnection *qconn,
			  boolean fcarrier));
  /* Run a chat program on a connection.  */
  boolean (*pfchat) P((struct sconnection *qconn, char **pzprog));
  /* Get the baud rate of a connection.  This field may be NULL.  */
  long (*pibaud) P((struct sconnection *qconn));
};

/* Connection functions.  */

/* Initialize a connection.  This must be called before any of the
   other connection functions are called.  It initializes the fields
   of qconn.  If qport is NULL, this opens standard input as a port
   using type ttype.  This function returns FALSE on error.  */
extern boolean fconn_init P((struct uuconf_port *qport,
			     struct sconnection *qconn,
			     enum uuconf_porttype ttype));

/* Free up connection data.  */
extern void uconn_free P((struct sconnection *qconn));

/* Lock a connection.  The fin argument is TRUE if the port is to be
   used for an incoming call; certains type of Unix locking need this
   information because they need to open the port.  */
extern boolean fconn_lock P((struct sconnection *qconn, boolean fin));

/* Unlock a connection.  */
extern boolean fconn_unlock P((struct sconnection *qconn));

/* Open a connection.  If ibaud is 0, the natural baud rate of the
   port is used.  If ihighbaud is not 0, fconn_open chooses the
   highest supported baud rate between ibaud and ihighbaud.  If fwait
   is TRUE, this should wait for an incoming call.  */
extern boolean fconn_open P((struct sconnection *qconn, long ibaud,
			     long ihighbaud, boolean fwait));

/* Close a connection.  The fsuccess argument is TRUE if the
   conversation completed normally, FALSE if it is being aborted.  */
extern boolean fconn_close P((struct sconnection *qconn,
			      pointer puuconf,
			      struct uuconf_dialer *qdialer,
			      boolean fsuccess));

/* Dial out on a connection.  The qsys and zphone arguments are for
   the chat scripts; zphone is the phone number to dial.  If qdialer
   is not NULL, *qdialer will be set to the dialer information used if
   any; *ptdialerfound will be set appropriately.  */
extern boolean fconn_dial P((struct sconnection *q, pointer puuconf,
			     const struct uuconf_system *qsys,
			     const char *zphone,
			     struct uuconf_dialer *qdialer,
			     enum tdialerfound *ptdialerfound));

/* Read from a connection.
   zbuf -- buffer to read bytes into
   *pclen on call -- length of zbuf
   *pclen on successful return -- number of bytes read
   cmin -- minimum number of bytes to read before returning ahead of timeout
   ctimeout -- timeout in seconds, 0 if none
   freport -- whether to report errors.  */
extern boolean fconn_read P((struct sconnection *qconn, char *zbuf,
			     size_t *pclen, size_t cmin,
			     int ctimeout, boolean freport));

/* Write to a connection.  */
extern boolean fconn_write P((struct sconnection *qconn, const char *zbuf,
			      size_t cbytes));

/* Read and write to a connection.  This reads and writes data until
   either all passed-in data has been written or the read buffer is
   full.
   zwrite -- buffer to write bytes from
   *pcwrite on call -- number of bytes to write
   *pcwrite on successful return -- number of bytes written
   zread -- buffer to read bytes into
   *pcread on call -- size of read buffer
   *pcread on successful return -- number of bytes read.  */
extern boolean fconn_io P((struct sconnection *qconn, const char *zwrite,
			   size_t *pcwrite, char *zread, size_t *pcread));

/* Send a break character to a connection.  */
extern boolean fconn_break P((struct sconnection *qconn));

/* Change the settings of a connection.  This allows independent
   control over the parity of output characters, whether to strip
   input characters, and whether to do XON/XOFF handshaking.  There is
   no explicit control over parity checking of input characters.  This
   function returns FALSE on error.  Attempts to set values not
   supported by the hardware are silently ignored.  */
extern boolean fconn_set P((struct sconnection *qconn,
			    enum tparitysetting tparity,
			    enum tstripsetting tstrip,
			    enum txonxoffsetting txonxoff));

/* Get the baud rate of a connection.  */
extern long iconn_baud P((struct sconnection *qconn));

/* Do a chat script with a system.  */
extern boolean fchat P((struct sconnection *qconn, pointer puuconf,
			const struct uuconf_chat *qchat,
			const struct uuconf_system *qsys,
			const struct uuconf_dialer *qdialer,
			const char *zphone, boolean ftranslate,
			const char *zport, long ibaud));

/* Tell the connection to either require or ignore carrier as fcarrier
   is TRUE or FALSE respectively.  This is called with fcarrier TRUE
   when \m is encountered in a chat script, and with fcarrier FALSE
   when \M is encountered.  */
extern boolean fconn_carrier P((struct sconnection *qconn,
				boolean fcarrier));

/* Run a chat program on a connection.  */
extern boolean fconn_run_chat P((struct sconnection *qconn,
				 char **pzprog));

/* Run through a dialer sequence.  This is a support routine for the
   port type specific dialing routines.  */
extern boolean fconn_dial_sequence P((struct sconnection *qconn,
				      pointer puuconf, char **pzdialer,
				      const struct uuconf_system *qsys,
				      const char *zphone,
				      struct uuconf_dialer *qdialer,
				      enum tdialerfound *ptdialerfound));

/* Dialing out on a modem is partially system independent.  This is
   the modem dialing routine.  */
extern boolean fmodem_dial P((struct sconnection *qconn, pointer puuconf,
			      const struct uuconf_system *qsys,
			      const char *zphone,
			      struct uuconf_dialer *qdialer,
			      enum tdialerfound *ptdialerfound));

/* Begin dialing out.  This should open the dialer device if there is
   one, toggle DTR if requested and possible, and tell the port to
   ignore carrier.  It should return FALSE on error.  */
extern boolean fsysdep_modem_begin_dial P((struct sconnection *qconn,
					   struct uuconf_dialer *qdial));

/* Finish dialing out on a modem.  This should close the dialer device
   if there is one.  If the dialer and the port both support carrier,
   the connection should be told to pay attention to carrier.  If it
   is possible to wait for carrier to come on, and the dialer and the
   port both the port support carrier, it should wait until carrier
   comes on.  */
extern boolean fsysdep_modem_end_dial P((struct sconnection *qconn,
					 struct uuconf_dialer *qdial));

/* System dependent initialization routines.  */
extern boolean fsysdep_stdin_init P((struct sconnection *qconn));
extern boolean fsysdep_modem_init P((struct sconnection *qconn));
extern boolean fsysdep_direct_init P((struct sconnection *qconn));
#if HAVE_TCP
extern boolean fsysdep_tcp_init P((struct sconnection *qconn));
#endif
#if HAVE_TLI
extern boolean fsysdep_tli_init P((struct sconnection *qconn));
#endif
extern boolean fsysdep_pipe_init P((struct sconnection *qconn));

#endif /* ! defined (CONN_H) */
