/* trans.h
   Header file for file and command transfer routines.

   Copyright (C) 1992 Ian Lance Taylor

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
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

/* The maximum possible number of channels.  */
#define IMAX_CHAN (16)

/* The ifeatures field of the sdaemon structure is an or of the
   following values.  These values are sent during the uucico
   handshake, and MUST NOT CHANGE.  */

/* File size negotiation.  */
#define FEATURE_SIZES (01)

/* File transfer restart.  */
#define FEATURE_RESTART (02)

/* The E (execute) command.  */
#define FEATURE_EXEC (04)

/* Version 1.03: requires decimal size in S and R command.  Needless
   to say, this should not be used by any new programs.  */
#define FEATURE_V103 (010)

/* SVR4 UUCP: expects dummy string between notify field and size field
   in send command.  There is probably some meaning to this string,
   but I don't know what it is.  If I ever find out, this flag will
   still be used to indicate it.  */
#define FEATURE_SVR4 (020)

/* This structure is used to hold information concerning the
   communication link established with the remote system.  */

struct sdaemon
{
  /* Global uuconf pointer.  */
  pointer puuconf;
  /* Remote system information.  */
  const struct uuconf_system *qsys;
  /* Local name being used.  */
  const char *zlocalname;
  /* Connection structure.  */
  struct sconnection *qconn;
  /* Protocol being used.  */
  const struct sprotocol *qproto;
  /* The largest file size permitted for a local request.  */
  long clocal_size;
  /* The largest file size permitted for a remote request.  */
  long cremote_size;
  /* The largest file size that may ever be transferred.  */
  long cmax_ever;
  /* The remote system ulimit.  */
  long cmax_receive;
  /* Features supported by the remote side.  */
  int ifeatures;
  /* TRUE if we should request the remote side to hang up.  */
  boolean frequest_hangup;
  /* TRUE if the remote side requested a hangup.  */
  boolean fhangup_requested;
  /* TRUE if we are hanging up.  */
  boolean fhangup;
  /* TRUE if the local system is currently the master.  */
  boolean fmaster;
  /* TRUE if the local system placed the call.  */
  boolean fcaller;
  /* UUCONF_RELIABLE_* flags for the connection.  */
  int ireliable;
  /* If fcaller is FALSE, the lowest grade which may be transferred
     during this call.  */
  char bgrade;
};

/* This structure is used to hold a file or command transfer which is
   in progress.  */

struct stransfer
{
  /* Next file transfer in queue.  */
  struct stransfer *qnext;
  /* Previous file transfer in queue.  */
  struct stransfer *qprev;
  /* Points to the queue this structure is on.  */
  struct stransfer **pqqueue;
  /* The function to call to send some data.  */
  boolean (*psendfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon));
  /* The function to call when data is received.  */
  boolean (*precfn) P((struct stransfer *qtrans, struct sdaemon *qdaemon,
		       const char *zdata, size_t cdata));
  /* Type specific information.   */
  pointer pinfo;
  /* TRUE if we are sending the file e (this is used to avoid a call
     to psendfn).  */
  boolean fsendfile;
  /* TRUE if we are receiving the file e (this is used to avoid a call
     to precfn).  */
  boolean frecfile;
  /* The file to read or write.  */
  openfile_t e;
  /* The position we are at in the file.  */
  long ipos;
  /* TRUE if we are waiting for a command string.  */
  boolean fcmd;
  /* The command string we have so far.  */
  char *zcmd;
  /* The length of the command string we have so far.  */
  size_t ccmd;
  /* Local destination number.  */
  int ilocal;
  /* Remote destination number.  */
  int iremote;
  /* The command.  */
  struct scmd s;
  /* A message to log when work starts.  */
  char *zlog;
  /* The process time; imicros can be negative.  */
  long isecs;
  long imicros;
  /* Number of bytes sent or received.  */
  long cbytes;
};

/* Reasons that a file transfer might fail.  */

enum tfailure
{
  /* No failure.  */
  FAILURE_NONE,
  /* No permission for operation.  */
  FAILURE_PERM,
  /* Can't open necessary file.  */
  FAILURE_OPEN,
  /* Not enough space to receive file.  */
  FAILURE_SIZE,
  /* File was received in a previous conversation.  */
  FAILURE_RECEIVED
};

/* The main loop which talks to the remote system, passing transfer
   requests and file back and forth.  */
extern boolean floop P((struct sdaemon *qdaemon));

/* Allocate a new transfer structure.  */
extern struct stransfer *qtransalc P((struct scmd *qcmd));

/* Free a transfer structure.  */
extern void utransfree P((struct stransfer *qtrans));

/* Queue up local requests.  If pfany is not NULL, this sets *pfany to
   TRUE if there are, in fact, any local requests which can be done at
   this point.  */
extern boolean fqueue P((struct sdaemon *qdaemon, boolean *pfany));

/* Clear away any queued requests.  This may be called more than once
   at the end of a call.  */
extern void uclear_queue P((struct sdaemon *qdaemon));

/* Queue a new transfer request made by the local system.  */
extern boolean fqueue_local P((struct sdaemon *qdaemon,
			       struct stransfer *qtrans));

/* Queue a new transfer request made by the remote system.  */
extern boolean fqueue_remote P((struct sdaemon *qdaemon,
				struct stransfer *qtrans));

/* Queue a transfer request which wants to send something.  */
extern boolean fqueue_send P((struct sdaemon *qdaemon,
			      struct stransfer *qtrans));

/* Queue a transfer request which wants to receiving something.  */
extern boolean fqueue_receive P((struct sdaemon *qdaemon,
				 struct stransfer *qtrans));

/* Prepare to send a file by local or remote request.  */
extern boolean flocal_send_file_init P((struct sdaemon *qdaemon,
					struct scmd *qcmd));
extern boolean fremote_send_file_init P((struct sdaemon *qdaemon,
					 struct scmd *qcmd,
					 int iremote));

/* Prepare to receive a file by local or remote request.  */
extern boolean flocal_rec_file_init P((struct sdaemon *qdaemon,
				       struct scmd *qcmd));
extern boolean fremote_rec_file_init P((struct sdaemon *qdaemon,
					struct scmd *qcmd,
					int iremote));

/* Prepare to request work by local or remote request.  */
extern boolean flocal_xcmd_init P((struct sdaemon *qdaemon,
				   struct scmd *qcmd));
extern boolean fremote_xcmd_init P((struct sdaemon *qdaemon,
				    struct scmd *qcmd,
				    int iremote));

/* We have lost the connection; record any in progress file transfers
   in the statistics file and discard any temporary files.  */
extern void ufailed P((struct sdaemon *qdaemon));

/* Check that there is enough disk space for a file receive.  Return
   FALSE if there is not.  */
extern boolean frec_check_free P((struct stransfer *qtrans,
				  long cfree_space));

/* Discard the temporary file being used to receive a file, if
   appropriate.  */
extern boolean frec_discard_temp P((struct sdaemon *qdaemon,
				    struct stransfer *qtrans));

/* Handle data received by a protocol.  This is called by the protocol
   specific routines as data comes in.  The data is passed as two
   buffers because that is convenient for packet based protocols, but
   normally csecond will be 0.  The ilocal argument is the local
   channel number, and the iremote argument is the remote channel
   number.  Either may be -1, if the protocol does not have channels.
   The ipos argument is the position in the file, if the protocol
   knows it; for most protocols, this will be -1.  The fallacked
   argument should be set to TRUE if the remote has acknowledged all
   outstanding data; see uwindow_acked, below, for details. This will
   set *pfexit to TRUE if there is something for the main loop to do.
   A file is complete is when a zero length buffer is passed (cfirst
   == 0).  A command is complete when data containing a null byte is
   passed.  This will return FALSE on error.  If the protocol pfwait
   entry point should exit and let the top level loop continue,
   *pfexit will be set to TRUE (if pfexit is not NULL).  This will not
   set *pfexit to FALSE, so the caller must do that.  */
extern boolean fgot_data P((struct sdaemon *qdaemon,
			    const char *zfirst, size_t cfirst,
			    const char *zsecond, size_t csecond,
			    int ilocal, int iremote,
			    long ipos, boolean fallacked,
			    boolean *pfexit));

/* This routine is called when an ack is sent for a file receive.  */
extern void usent_receive_ack P((struct sdaemon *qdaemon,
				 struct stransfer *qtrans));

/* A protocol may call this routine to indicate the packets have been
   acknowledged by the remote system.  If the fallacked argument is
   TRUE, then all outstanding packets have been acknowledged; for
   convenience, this may also be indicated by passing fallacked as
   TRUE to fgot_data, above.  Otherwise this routine should be called
   each time a complete window is acked by the remote system.  The
   transfer code uses this information to keep track of when an
   acknowledgement of a file receive has been seen by the other side,
   so that file receives may be handled cleanly if the connection is
   lost.  */
extern void uwindow_acked P((struct sdaemon *qdaemon,
			     boolean fallacked));
