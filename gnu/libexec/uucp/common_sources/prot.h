/* prot.h
   Protocol header file.

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

/* We need the definition of uuconf_cmdtab to declare the protocol
   parameter arrays.  */
#ifndef UUCONF_H
#include "uuconf.h"
#endif

#if ANSI_C
/* These structures are used in prototypes but are not defined in this
   header file.  */
struct sdaemon;
struct sconnection;
struct stransfer;
#endif

/* The sprotocol structure holds information and functions for a specific
   protocol (e.g. the 'g' protocol).  */

struct sprotocol
{
  /* The name of the protocol (e.g. 'g').  */
  char bname;
  /* Reliability requirements, an or of UUCONF_RELIABLE_xxx defines
     from uuconf.h.  */
  int ireliable;
  /* The maximum number of channels this protocol can support.  */
  int cchans;
  /* Whether files may be reliably restarted using this protocol.  */
  boolean frestart;
  /* Protocol parameter commands.  */
  struct uuconf_cmdtab *qcmds;
  /* A routine to start the protocol.  If *pzlog is set to be
     non-NULL, it is an informative message to be logged; it should
     then be passed to ubuffree.  */
  boolean (*pfstart) P((struct sdaemon *qdaemon, char **pzlog));
  /* Shutdown the protocol.  */
  boolean (*pfshutdown) P((struct sdaemon *qdaemon));
  /* Send a command to the other side.  */
  boolean (*pfsendcmd) P((struct sdaemon *qdaemon, const char *z,
			  int ilocal, int iremote));
  /* Get buffer to space to fill with data.  This should set *pcdata
     to the amount of data desired.  */
  char *(*pzgetspace) P((struct sdaemon *qdaemon, size_t *pcdata));
  /* Send data to the other side.  The argument z must be a return
     value of pzgetspace.  The ipos argument is the file position, and
     is ignored by most protocols.  */
  boolean (*pfsenddata) P((struct sdaemon *qdaemon, char *z, size_t c,
			   int ilocal, int iremote, long ipos));
  /* Wait for data to come in and call fgot_data with it until
     fgot_data sets *pfexit.  */
  boolean (*pfwait) P((struct sdaemon *qdaemon));
  /* Handle any file level actions that need to be taken.  If a file
     transfer is starting rather than ending, fstart is TRUE.  If the
     file is being sent rather than received, fsend is TRUE.  If
     fstart and fsend are both TRUE, cbytes holds the size of the
     file.  If *pfhandled is set to TRUE, then the protocol routine
     has taken care of queueing up qtrans for the next action.  */
  boolean (*pffile) P((struct sdaemon *qdaemon, struct stransfer *qtrans,
		       boolean fstart, boolean fsend, long cbytes,
		       boolean *pfhandled));
};

/* Send data to the other system.  If the fread argument is TRUE, this
   will also receive data into the receive buffer abPrecbuf; fread is
   passed as TRUE if the protocol expects data to be coming back, to
   make sure the input buffer does not fill up.  Returns FALSE on
   error.  */
extern boolean fsend_data P((struct sconnection *qconn,
			     const char *zsend, size_t csend,
			     boolean fdoread));

/* Receive data from the other system when there is no data to send.
   The cneed argument is the amount of data desired and the ctimeout
   argument is the timeout in seconds.  This will set *pcrec to the
   amount of data received.  It will return FALSE on error.  If a
   timeout occurs, it will return TRUE with *pcrec set to zero.  */
extern boolean freceive_data P((struct sconnection *qconn, size_t cneed,
				size_t *pcrec, int ctimeout,
				boolean freport));

/* Get one character from the remote system, going through the
   procotol buffering.  The ctimeout argument is the timeout in
   seconds, and the freport argument is TRUE if errors should be
   reported (when closing a connection it is pointless to report
   errors).  This returns a character or -1 on a timeout or -2 on an
   error.  */
extern int breceive_char P((struct sconnection *qconn,
			    int ctimeout, boolean freport));

/* Compute a 32 bit CRC of a data buffer, given an initial CRC.  */
extern unsigned long icrc P((const char *z, size_t c, unsigned long ick));

/* The initial CRC value to use for a new buffer.  */
#if ANSI_C
#define ICRCINIT (0xffffffffUL)
#else
#define ICRCINIT ((unsigned long) 0xffffffffL)
#endif

/* The size of the receive buffer.  */
#define CRECBUFLEN (16384)

/* Buffer to hold received data.  */
extern char abPrecbuf[CRECBUFLEN];

/* Index of start of data in abPrecbuf.  */
extern int iPrecstart;

/* Index of end of data (first byte not included in data) in abPrecbuf.  */
extern int iPrecend;

/* There are a couple of variables and functions that are shared by
   the 'i' and 'j' protocols (the 'j' protocol is just a wrapper
   around the 'i' protocol).  These belong in a separate header file,
   protij.h, but I don't want to create one for just a couple of
   things.  */

/* An escape sequence of characters for the 'j' protocol to avoid
   (protocol parameter ``avoid'').  */
extern const char *zJavoid_parameter;

/* Timeout to use when sending the 'i' protocol SYNC packet (protocol
   parameter ``sync-timeout'').  */
extern int cIsync_timeout;

/* Shared startup routine for the 'i' and 'j' protocols.  */
extern boolean fijstart P((struct sdaemon *qdaemon, char **pzlog,
			   int imaxpacksize,
			   boolean (*pfsend) P((struct sconnection *qconn,
						const char *zsend,
						size_t csend,
						boolean fdoread)),
			   boolean (*pfreceive) P((struct sconnection *qconn,
						   size_t cneed,
						   size_t *pcrec,
						   int ctimeout,
						   boolean freport))));

/* Prototypes for 'g' protocol functions.  */

extern struct uuconf_cmdtab asGproto_params[];
extern boolean fgstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fbiggstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fvstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fgshutdown P((struct sdaemon *qdaemon));
extern boolean fgsendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *zggetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean fgsenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean fgwait P((struct sdaemon *qdaemon));

/* Prototypes for 'f' protocol functions.  */

extern struct uuconf_cmdtab asFproto_params[];
extern boolean ffstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean ffshutdown P((struct sdaemon *qdaemon));
extern boolean ffsendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *zfgetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean ffsenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean ffwait P((struct sdaemon *qdaemon));
extern boolean fffile P((struct sdaemon *qdaemon, struct stransfer *qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));

/* Prototypes for 't' protocol functions.  */

extern struct uuconf_cmdtab asTproto_params[];
extern boolean ftstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean ftshutdown P((struct sdaemon *qdaemon));
extern boolean ftsendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *ztgetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean ftsenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean ftwait P((struct sdaemon *qdaemon));
extern boolean ftfile P((struct sdaemon *qdaemon, struct stransfer *qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));

/* Prototypes for 'e' protocol functions.  */

extern struct uuconf_cmdtab asEproto_params[];
extern boolean festart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean feshutdown P((struct sdaemon *qdaemon));
extern boolean fesendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *zegetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean fesenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean fewait P((struct sdaemon *qdaemon));
extern boolean fefile P((struct sdaemon *qdaemon, struct stransfer *qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));

/* Prototypes for 'i' protocol functions.  */

extern struct uuconf_cmdtab asIproto_params[];
extern boolean fistart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fishutdown P((struct sdaemon *qdaemon));
extern boolean fisendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *zigetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean fisenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean fiwait P((struct sdaemon *qdaemon));

/* Prototypes for 'j' protocol functions.  The 'j' protocol mostly
   uses the 'i' protocol functions, but it has a couple of functions
   of its own.  */
extern boolean fjstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fjshutdown P((struct sdaemon *qdaemon));

/* Prototypes for 'a' protocol functions (these use 'z' as the second
   character because 'a' is a modified Zmodem protocol).  */
extern struct uuconf_cmdtab asZproto_params[];
extern boolean fzstart P((struct sdaemon *qdaemon, char **pzlog));
extern boolean fzshutdown P((struct sdaemon *qdaemon));
extern boolean fzsendcmd P((struct sdaemon *qdaemon, const char *z,
			    int ilocal, int iremote));
extern char *zzgetspace P((struct sdaemon *qdaemon, size_t *pcdata));
extern boolean fzsenddata P((struct sdaemon *qdaemon, char *z, size_t c,
			     int ilocal, int iremote, long ipos));
extern boolean fzwait P((struct sdaemon *qdaemon));
extern boolean fzfile P((struct sdaemon *qdaemon, struct stransfer *qtrans,
			 boolean fstart, boolean fsend, long cbytes,
			 boolean *pfhandled));
