/* uuconf.h
   Header file for UUCP configuration routines.

   Copyright (C) 1992, 1993, 1994, 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The use of an object file which uses material from this header
   file, and from no other portion of the uuconf library, is
   unrestricted, as described in paragraph 4 of section 5 of version 2
   of the GNU Library General Public License (this sentence is merely
   informative, and does not modify the License in any way).

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

/* $FreeBSD: src/gnu/libexec/uucp/common_sources/uuconf.h,v 1.6.8.1 2000/06/03 17:18:09 ume Exp $ */

#ifndef UUCONF_H

#define UUCONF_H

#include <stdio.h>

/* The macro UUCONF_ANSI_C may be used to override __STDC__.  */
#ifndef UUCONF_ANSI_C
#ifdef __STDC__
#define UUCONF_ANSI_C 1
#else /* ! defined (__STDC__) */
#define UUCONF_ANSI_C 0
#endif /* ! defined (__STDC__) */
#endif /* ! defined (UUCONF_ANSI_C) */

#if UUCONF_ANSI_C
#define UUCONF_CONST const
typedef void *UUCONF_POINTER;
#include <stddef.h>
typedef size_t UUCONF_SIZE_T;
#else
#define UUCONF_CONST
typedef char *UUCONF_POINTER;
typedef unsigned int UUCONF_SIZE_T;
#endif

/* The field names of each of the following structures begin with
   "uuconf_".  This is to avoid any conflicts with user defined
   macros.  The first character following the "uuconf_" string
   indicates the type of the field.

   z -- a string (char *)
   c -- a count (normally int)
   i -- an integer value (normally int)
   f -- a boolean value (normally int)
   b -- a single character value (char or int)
   t -- an enum (enum XX)
   s -- a structure (struct XX)
   u -- a union (union XX)
   q -- a pointer to a structure (struct XX *)
   p -- a pointer to something other than a string
   */

/* The information which is kept for a chat script.  */

struct uuconf_chat
{
  /* The script itself.  This is a NULL terminated list of expect/send
     pairs.  The first string is an expect string.  A string starting
     with a '-' indicates subsend string; the following strings which
     start with '-' are subexpect/subsend strings.  This field may be
     NULL, in which case there is no chat script (but pzprogram may
     hold a program to run).  */
  char **uuconf_pzchat;
  /* The chat program to run.  This is a NULL terminated list of
     arguments; element 0 if the program.  May be NULL, in which case
     there is no program.  */
  char **uuconf_pzprogram;
  /* The timeout in seconds to use for expect strings in the chat
     script.  */
  int uuconf_ctimeout;
  /* The NULL terminated list of failure strings.  If any of these
     strings appear, the chat script is aborted.  May be NULL, in
     which case there are no failure strings.  */
  char **uuconf_pzfail;
  /* Non-zero if incoming characters should be stripped to seven bits
     (by anding with 0x7f).  */
  int uuconf_fstrip;
};

/* The information which is kept for a time specification.  This is a
   linked list of structures.  Each element of the list represents a
   span of time, giving a starting time and an ending time.  The time
   only depends on the day of the week, not on the day of the month or
   of the year.  The time is only specified down to the minute, not
   down to the second or below.  The list is sorted by starting time.

   The starting and ending time are expressed in minutes since the
   beginning of the week, which is considered to be 12 midnight on
   Sunday.  Thus 60 is 1 am on Sunday, 1440 (== 60 * 24) is 12
   midnight on Monday, and the largest possible value is 10080 (== 60
   * 24 * 7) which is 12 midnight on the following Sunday.

   Each span of time has a value associated with it.  This is the
   lowest grade or the largest file size that may be transferred
   during that time, depending on the source of the time span.  When
   time specifications overlap, the value used for the overlap is the
   higher grade or the smaller file size.  Thus specifying
   ``call-timegrade z Any'' and ``call-timegrade Z Mo'' means that
   only grade Z or higher may be sent on Monday, since Z is the higer
   grade of the overlapping spans.  The final array wil have no
   overlaps.

   Each span also has a retry time associated with it.  This permits
   different retry times to be used at different times of day.  The
   retry time is only relevant if the span came from a ``time'' or
   ``timegrade'' command for a system.  */

struct uuconf_timespan
{
  /* Next element in list.  */
  struct uuconf_timespan *uuconf_qnext;
  /* Starting minute (-1 at the end of the array).  */
  int uuconf_istart;
  /* Ending minute.  */
  int uuconf_iend;
  /* Value for this span (lowest grade or largest file that may be
     transferred at this time).  */
  long uuconf_ival;
  /* Retry time.  */
  int uuconf_cretry;
};

/* The information which is kept for protocol parameters.  Protocol
   parameter information is stored as an array of the following
   structures.  */

struct uuconf_proto_param
{
  /* The name of the protocol to which this entry applies.  This is
     '\0' for the last element of the array.  */
  int uuconf_bproto;
  /* Specific entries for this protocol.  This points to an array
     ending in an element with a uuconf_cargs field of 0.  */
  struct uuconf_proto_param_entry *uuconf_qentries;
};

/* Each particular protocol parameter entry is one of the following
   structures.  */

struct uuconf_proto_param_entry
{
  /* The number of arguments to the ``protocol-parameter'' command
     (not counting ``protocol-parameter'' itself).  This is 0 for the
     last element of the array.  */
  int uuconf_cargs;
  /* The actual arguments to the ``protocol-parameter'' command; this
     is an array with cargs entries.  */
  char **uuconf_pzargs;
};

/* The information which is kept for a system.  The zname and zalias
   fields will be the same for all alternates.  Every other fields is
   specific to the particular alternate in which it appears (although
   most will be the same for all alternates).  */

struct uuconf_system
{
  /* The name of the system.  */
  char *uuconf_zname;
  /* A list of aliases for the system.  This is a NULL terminated list
     of strings.  May be NULL, in which case there are no aliases.  */
  char **uuconf_pzalias;
  /* A linked list of alternate call in or call out information.  Each
     alternative way to call this system occupies an element of this
     list.  May be NULL, in which case there are no alternates.  */
  struct uuconf_system *uuconf_qalternate;
  /* The name for this particular alternate.  May be NULL, in which
     case this alternate does not have a name.  */
  char *uuconf_zalternate;
  /* If non-zero, this alternate may be used for calling out.  */
  int uuconf_fcall;
  /* If non-zero, this alternate may be used for accepting a call.  */
  int uuconf_fcalled;
  /* The times at which this system may be called.  The ival field of
     each uuconf_timespan structure is the lowest grade which may be
     transferred at that time.  The cretry field is the number of
     minutes to wait before retrying the call, or 0 if it was not
     specified.  May be NULL, in which case the system may never be
     called.  */
  struct uuconf_timespan *uuconf_qtimegrade;
  /* The times at which to request a particular grade of the system
     when calling it, and the grades to request.  The ival field of
     each uuconf_timespan structure is the lowest grade which the
     other system should transfer at that time.  May be NULL, in which
     case there are no grade restrictions.  */
  struct uuconf_timespan *uuconf_qcalltimegrade;
  /* The times at which to allow a particular grade of work to be
     transferred to the system, when it calls in.  The ival field of
     each uuconf_timespan structure is the lowest grade which should
     be transferred at that time.  May be NULL, in which case there
     are no grade restrictions.  */
  struct uuconf_timespan *uuconf_qcalledtimegrade;
  /* The maximum number of times to retry calling this system.  If
     this is 0, there is no limit.  */
  int uuconf_cmax_retries;
  /* The number of minutes to wait between successful calls to a
     system.  */
  int uuconf_csuccess_wait;
  /* The size restrictions by time for local requests during a locally
     placed call.  The ival field of each uuconf_timespan structure is
     the size in bytes of the largest file which may be transferred at
     that time.  May be NULL, in which case there are no size
     restrictions.  */
  struct uuconf_timespan *uuconf_qcall_local_size;
  /* The size restrictions by time for remote requests during a
     locally placed call.  May be NULL.  */
  struct uuconf_timespan *uuconf_qcall_remote_size;
  /* The size restrictions by time for local requests during a
     remotely placed call.  May be NULL.  */
  struct uuconf_timespan *uuconf_qcalled_local_size;
  /* The size restrictions by time for remote requests during a
     remotely placed call.  May be NULL.  */
  struct uuconf_timespan *uuconf_qcalled_remote_size;
  /* Baud rate, or speed.  Zero means any baud rate.  If ihighbaud is
     non-zero, this is the low baud rate of a range.  */
  long uuconf_ibaud;
  /* If non-zero, ibaud is the low baud rate of a range and ihighbaud
     is the high baud rate.  */
  long uuconf_ihighbaud;
  /* Port name to use.  May be NULL.  If an HDB configuration file
     contains a modem class (alphabetic characters preceeding the baud
     rate), the class is appended to the port name.  */
  char *uuconf_zport;
  /* Specific port information, if the system entry includes port
     information.  May be NULL.  */
  struct uuconf_port *uuconf_qport;
  /* Phone number to call, or address to use for a TCP connection.
     May be NULL, in which case a dialer script may not use \D or \T
     for this system, and a TCP port will use the system name.  */
  char *uuconf_zphone;
  /* Chat script to use when logging in to the system.  */
  struct uuconf_chat uuconf_schat;
  /* Login name to use for \L in the login chat script.  This should
     normally be accessed via uuconf_callout.  If it is "*",
     uuconf_callout will look it up in the call out file.  This may be
     NULL, in which case the login script may not use \L.  */
  char *uuconf_zcall_login;
  /* Password to use for \P in the login chat script.  This should
     normally be accessed via uuconf_callout.  If it is "*",
     uuconf_callout will look it up in the call out file.  This may be
     NULL, in which case the login script may not use \P.  */
  char *uuconf_zcall_password;
  /* The login name this system must use when calling in.  This may be
     different for different alternates.  This should only be examined
     if uuconf_fcalled is TRUE.  If this is NULL or "ANY" then
     uuconf_validate must be called to make sure that whatever login
     name was used is permitted for this machine.  */
  char *uuconf_zcalled_login;
  /* If non-zero, then when this system calls in the call should not
     be allowed to proceed and the system should be called back.  */
  int uuconf_fcallback;
  /* If non-zero, then conversation sequence numbers should be used
     with this system.  */
  int uuconf_fsequence;
  /* A list of protocols to use with this system.  Each protocol has a
     single character name.  May be NULL, in which case any known
     protocol may be used.  */
  char *uuconf_zprotocols;
  /* Array of protocol parameters.  Ends in an entry with a
     uuconf_bproto field of '\0'.  May be NULL.  */
  struct uuconf_proto_param *uuconf_qproto_params;
  /* Chat script to run when called by this system.  */
  struct uuconf_chat uuconf_scalled_chat;
  /* Debugging level to set during a conversation.  May be NULL.  */
  char *uuconf_zdebug;
  /* Maximum remote debugging level this system may request.  May be
     NULL.  */
  char *uuconf_zmax_remote_debug;
  /* Non-zero if the remote system may request us to send files from
     the local system to the remote.  */
  int uuconf_fsend_request;
  /* Non-zero if the remote system may request us to receive files
     from the remote system to the local.  */
  int uuconf_frec_request;
  /* Non-zero if local requests are permitted when calling this
     system.  */
  int uuconf_fcall_transfer;
  /* Non-zero if local requests are permitted when this system calls
     in.  */
  int uuconf_fcalled_transfer;
  /* NULL terminated list of directories from which files may be sent
     by local request.  */
  char **uuconf_pzlocal_send;
  /* NULL terminated list of directories from which files may be sent
     by remote request.  */
  char **uuconf_pzremote_send;
  /* NULL terminated list of directories into which files may be
     received by local request.  */
  char **uuconf_pzlocal_receive;
  /* NULL terminated list of directories into which files may be
     received by remote request.  */
  char **uuconf_pzremote_receive;
  /* Path to use for command execution.  This is a NULL terminated
     list of directories.  */
  char **uuconf_pzpath;
  /* NULL terminated List of commands that may be executed.  */
  char **uuconf_pzcmds;
  /* Amount of free space to leave when accepting a file from this
     system, in bytes.  */
  long uuconf_cfree_space;
  /* NULL terminated list of systems that this system may forward
     from.  May be NULL if there are no systems from which files may
     be forwarded.  The list may include "ANY".  */
  char **uuconf_pzforward_from;
  /* NULL terminated list of systems that this system may forward to.
     May be NULL if there are no systems to which files may be
     forwarded.  The list may include "ANY".  */
  char **uuconf_pzforward_to;
  /* The public directory to use for this sytem.  */
  const char *uuconf_zpubdir;
  /* The local name to use for this remote system.  May be NULL if the
     usual local name should be used.  */
  char *uuconf_zlocalname;
  /* Memory allocation block for the system.  */
  UUCONF_POINTER uuconf_palloc;
};

/* Types of ports.  */

enum uuconf_porttype
{
  /* Unknown port type.  A port of this type should never be returned
     by the uuconf functions.  */
  UUCONF_PORTTYPE_UNKNOWN,
  /* Read from standard input and write to standard output.  Not
     normally used.  */
  UUCONF_PORTTYPE_STDIN,
  /* A modem port.  */
  UUCONF_PORTTYPE_MODEM,
  /* A direct connect port.  */
  UUCONF_PORTTYPE_DIRECT,
  /* A TCP port.  Not supported on all systems.  */
  UUCONF_PORTTYPE_TCP,
  /* A TLI port.  Not supported on all systems.  */
  UUCONF_PORTTYPE_TLI,
  /* A pipe port.  Not supported on all systems.  */
  UUCONF_PORTTYPE_PIPE
};

/* Additional information for a stdin port (there is none).  */

struct uuconf_stdin_port
{
  int uuconf_idummy;
};

/* Additional information for a modem port.  */

struct uuconf_modem_port
{
  /* The device name.  May be NULL, in which case the port name is
     used instead.  */
  char *uuconf_zdevice;
  /* The device name to send the dialer chat script to.  May be NULL,
     in which case the chat script is sent to the usual device.  */
  char *uuconf_zdial_device;
  /* The default baud rate (speed).  If zero, there is no default.  */
  long uuconf_ibaud;
  /* The low baud rate, if a range is used.  If zero, a range is not
     used and ihighbaud should be ignored.  */
  long uuconf_ilowbaud;
  /* The high baud rate, if ilowbaud is non-zero.  */
  long uuconf_ihighbaud;
  /* Non-zero if the port supports carrier detect.  */
  int uuconf_fcarrier;
  /* Non-zero if the port supports hardware flow control.  */
  int uuconf_fhardflow;
  /* A NULL terminated sequence of dialer/token pairs (element 0 is a
     dialer name, element 1 is a token, etc.)  May be NULL, in which
     case qdialer should not be NULL.  */
  char **uuconf_pzdialer;
  /* Specific dialer information.  Only used if pzdialer is NULL.  */
  struct uuconf_dialer *uuconf_qdialer;
};

/* Additional information for a direct connect port.  */

struct uuconf_direct_port
{
  /* The device name.  May be NULL, in which case the port name is
     used instead.  */
  char *uuconf_zdevice;
  /* The baud rate (speed).  */
  long uuconf_ibaud;
  /* Non-zero if the port uses carrier detect.  */
  int uuconf_fcarrier;
  /* Non-zero if the port supports hardware flow control.  */
  int uuconf_fhardflow;
};

/* Additional information for a TCP port.  */

struct uuconf_tcp_port
{
  /* The TCP port number to use.  May be a name or a number.  May be
     NULL, in which case "uucp" is looked up using getservbyname.  */
  char *uuconf_zport;
  /* Address family to use for a TCP connection.  */
  int uuconf_zfamily;
  /* A NULL terminated sequence of dialer/token pairs (element 0 is a
     dialer name, element 1 is a token, etc.)  May be NULL.  */
  char **uuconf_pzdialer;
};

/* Additional information for a TLI port.  */

struct uuconf_tli_port
{
  /* Device name to open.  May be NULL, in which case the port name is
     used.  */
  char *uuconf_zdevice;
  /* Whether this port should be turned into a stream, permitting the
     read and write calls instead of the t_rcv and t_send calls.  */
  int uuconf_fstream;
  /* A NULL terminated list of modules to push after making the
     connection.  May be NULL, in which case if fstream is non-zero,
     then "tirdwr" is pushed onto the stream, and otherwise nothing is
     pushed.  */
  char **uuconf_pzpush;
  /* A NULL terminated sequence of dialer/token pairs (element 0 is a
     dialer name, element 1 is a token, etc.)  May be NULL.  If
     element 0 is TLI or TLIS, element 1 is used as the address to
     connect to; otherwise uuconf_zphone from the system information
     is used.  */
  char **uuconf_pzdialer;
  /* Address to use when operating as a server.  This may contain
     escape sequences.  */
  char *uuconf_zservaddr;
};

/* Additional information for a pipe port.  */

struct uuconf_pipe_port
{
  /* The command and its arguments.  */
  char **uuconf_pzcmd;
};

/* Information kept for a port.  */

struct uuconf_port
{
  /* The name of the port.  */
  char *uuconf_zname;
  /* The type of the port.  */
  enum uuconf_porttype uuconf_ttype;
  /* The list of protocols supported by the port.  The name of each
     protocol is a single character.  May be NULL, in which case any
     protocol is permitted.  */
  char *uuconf_zprotocols;
  /* Array of protocol parameters.  Ends in an entry with a
     uuconf_bproto field of '\0'.  May be NULL.  */
  struct uuconf_proto_param *uuconf_qproto_params;
  /* The set of reliability bits.  */
  int uuconf_ireliable;
  /* The lock file name to use.  */
  char *uuconf_zlockname;
  /* Memory allocation block for the port.  */
  UUCONF_POINTER uuconf_palloc;
  /* The type specific information.  */
  union
    {
      struct uuconf_stdin_port uuconf_sstdin;
      struct uuconf_modem_port uuconf_smodem;
      struct uuconf_direct_port uuconf_sdirect;
      struct uuconf_tcp_port uuconf_stcp;
      struct uuconf_tli_port uuconf_stli;
      struct uuconf_pipe_port uuconf_spipe;
    } uuconf_u;
};

/* Information kept about a dialer.  */

struct uuconf_dialer
{
  /* The name of the dialer.  */
  char *uuconf_zname;
  /* The chat script to use when dialing out.  */
  struct uuconf_chat uuconf_schat;
  /* The string to send when a `=' appears in the phone number.  */
  char *uuconf_zdialtone;
  /* The string to send when a `-' appears in the phone number.  */
  char *uuconf_zpause;
  /* Non-zero if the dialer supports carrier detect.  */
  int uuconf_fcarrier;
  /* The number of seconds to wait for carrier after the chat script
     is complete.  Only used if fcarrier is non-zero.  Only supported
     on some systems.  */
  int uuconf_ccarrier_wait;
  /* If non-zero, DTR should be toggled before dialing.  Only
     supported on some systems.  */
  int uuconf_fdtr_toggle;
  /* If non-zero, sleep for 1 second after toggling DTR.  Ignored if
     fdtr_toggle is zero.  */
  int uuconf_fdtr_toggle_wait;
  /* The chat script to use when a call is complete.  */
  struct uuconf_chat uuconf_scomplete;
  /* The chat script to use when a call is aborted.  */
  struct uuconf_chat uuconf_sabort;
  /* Array of protocol parameters.  Ends in an entry with a
     uuconf_bproto field of '\0'.  May be NULL.  */
  struct uuconf_proto_param *uuconf_qproto_params;
  /* The set of reliability bits.  */
  int uuconf_ireliable;
  /* Memory allocation block for the dialer.  */
  UUCONF_POINTER uuconf_palloc;
};

/* Reliability bits for the ireliable field of ports and dialers.
   These bits are used to decide which protocol to run.  A given
   protocol will have a set of these bits, and each of them must be
   turned on for the port before we will permit that protocol to be
   used.  This will be overridden by the zprotocols field.  */

/* Whether a set of reliability bits is given.  If this bit is not
   set, then there is no reliability information.  */
#define UUCONF_RELIABLE_SPECIFIED (01)

/* Set if the connection is eight bit transparent.  */
#define UUCONF_RELIABLE_EIGHT (02)

/* Set if the connection is error-free.  */
#define UUCONF_RELIABLE_RELIABLE (04)

/* Set if the connection is end-to-end reliable (e.g. TCP).  */
#define UUCONF_RELIABLE_ENDTOEND (010)

/* Set if the connection is full-duplex; that is, no time consuming
   line turnaround is required before sending data in the reverse
   direction.  If the connection is truly half-duplex, in the sense
   that communication can only flow in one direction, UUCP can not be
   used.  */
#define UUCONF_RELIABLE_FULLDUPLEX (020)

/* UUCP grades range from 0 to 9, A to Z, a to z in order from highest
   to lowest (work of higher grades is done before work of lower
   grades).  */

/* The highest grade.  */
#define UUCONF_GRADE_HIGH ('0')

/* The lowest grade.  */
#define UUCONF_GRADE_LOW ('z')

/* Whether a character is a legal grade (requires <ctype.h>).  */
#define UUCONF_GRADE_LEGAL(b) (isalnum (BUCHAR (b)))

/* Return < 0 if the first grade should be done before the second
   grade, == 0 if they are the same, or > 0 if the first grade should
   be done after the second grade.  On an ASCII system, this can just
   be b1 - b2.  */
#define UUCONF_GRADE_CMP(b1, b2) (uuconf_grade_cmp ((b1), (b2)))

/* Definitions for bits returned by uuconf_strip.  */
#define UUCONF_STRIP_LOGIN (01)
#define UUCONF_STRIP_PROTO (02)

/* uuconf_runuuxqt returns either a positive number (the number of
   execution files to receive between uuxqt invocations) or one of
   these constant values.  */
#define UUCONF_RUNUUXQT_NEVER (0)
#define UUCONF_RUNUUXQT_ONCE (-1)
#define UUCONF_RUNUUXQT_PERCALL (-2)

/* Most of the uuconf functions returns an error code.  A value of
   zero (UUCONF_SUCCESS) indicates success.  */

/* If this bit is set in the returned error code, then the
   uuconf_errno function may be used to obtain the errno value as set
   by the function which caused the failure.  */
#define UUCONF_ERROR_ERRNO (0x100)

/* If this bit is set in the returned error code, then the
   uuconf_filename function may be used to get the name of a file
   associated with the error.  */
#define UUCONF_ERROR_FILENAME (0x200)

/* If this bit is set in the returned error code, then the
   uuconf_lineno function may be used to get a line number associated
   with the error; normally if this is set UUCONF_ERROR_FILENAME will
   also be set.  */
#define UUCONF_ERROR_LINENO (0x400)

/* There are two UUCONF_CMDTABRET bits that may be set in the return
   value of uuconf_cmd_line or uuconf_cmd_args, described below.  They
   do not indicate an error, but instead give instructions to the
   calling function, often uuconf_cmd_file.  They may also be set in
   the return value of a user function listed in a uuconf_cmdtab
   table, in which case they will be honored by uuconf_cmd_file.  */

/* This bit means that the memory occupied by the arguments passed to
   the function should be preserved, and not overwritten or freed.  It
   refers only to the contents of the arguments; the contents of the
   argv array itself may always be destroyed.  If this bit is set in
   the return value of uuconf_cmd_line or uuconf_cmd_args, it must be
   honored.  It will be honored by uuconf_cmd_file.  This may be
   combined with an error code or with UUCONF_CMDTABRET_EXIT, although
   neither uuconf_cmd_file or uuconf_cmd_line will do so.  */
#define UUCONF_CMDTABRET_KEEP (0x800)

/* This bit means that uuconf_cmd_file should exit, rather than go on
   to read and process the next line.  If uuconf_cmd_line or
   uuconf_cmd_args encounter an error, the return value will have this
   bit set along with the error code.  A user function may set this
   bit with or without an error; the return value of the user function
   will be returned by uuconf_cmd_file, except that the
   UUCONF_CMDTABRET_KEEP and UUCONF_CMDTABRET_EXIT bits will be
   cleared.  */
#define UUCONF_CMDTABRET_EXIT (0x1000)

/* This macro may be used to extract the specific error value.  */
#define UUCONF_ERROR_VALUE(i) ((i) & 0xff)

/* UUCONF_ERROR_VALUE will return one of the following values.  */

/* Function succeeded.  */
#define UUCONF_SUCCESS (0)
/* Named item not found.  */
#define UUCONF_NOT_FOUND (1)
/* A call to fopen failed.  */
#define UUCONF_FOPEN_FAILED (2)
/* A call to fseek failed.  */
#define UUCONF_FSEEK_FAILED (3)
/* A call to malloc or realloc failed.  */
#define UUCONF_MALLOC_FAILED (4)
/* Syntax error in file.  */
#define UUCONF_SYNTAX_ERROR (5)
/* Unknown command.  */
#define UUCONF_UNKNOWN_COMMAND (6)

#if UUCONF_ANSI_C

/* For each type of configuration file (Taylor, V2, HDB), there are
   separate routines to read various sorts of information.  There are
   also generic routines, which call on the appropriate type specific
   routines.  The library can be compiled to read any desired
   combination of the configuration file types.  This affects only the
   generic routines, as it determines which type specific routines
   they call.  Thus, on a system which, for example, does not have any
   V2 configuration files, there is no need to include the overhead of
   the code to parse the files and the time to look for them.
   However, a program which specifically wants to be able to parse
   them can call the V2 specific routines.

   The uuconf functions all take as an argument a pointer to uuconf
   global information.  This must be initialized by any the
   initialization routines (the generic one and the three file type
   specific ones) before any of the other uuconf functions may be
   called.  */

/* Initialize the configuration file reading routines.  The ppglobal
   argument should point to a generic pointer (a void *, or, on older
   compilers, a char *) which will be initialized and may then be
   passed to the other uuconf routines.  The zprogram argument is the
   name of the program for which files should be read.  A NULL is
   taken as "uucp", and reads the standard UUCP configuration files.
   The only other common argument is "cu", but any string is
   permitted.  The zname argument is the name of the Taylor UUCP
   config file; if it is NULL, the default config file will be read.
   If not reading Taylor UUCP configuration information, the argument
   is ignored.  This function must be called before any of the other
   uuconf functions.

   Note that if the zname argument is obtained from the user running
   the program, the program should be careful to revoke any special
   privileges it may have (e.g. on Unix call setuid (getuid ()) and
   setgid (getgid ())).  Otherwise various sorts of spoofing become
   possible.  */
extern int uuconf_init (void **uuconf_ppglobal,
			const char *uuconf_zprogram,
			const char *uuconf_zname);

/* Adjust the configuration file global pointer for a new thread.  The
   library is fully reentrant (with the exception of the function
   uuconf_error_string, which calls strerror, which on some systems is
   not reentrant), provided that each new thread that wishes to call
   the library calls this function and uses the new global pointer
   value.  The ppglobal argument should be set to the address of the
   global pointer set by any of the init functions; it will be
   modified to become a new global pointer.  */
extern int uuconf_init_thread (void **uuconf_ppglobal);

/* Get the names of all known systems.  This sets sets *ppzsystems to
   point to an array of system names.  The list of names is NULL
   terminated.  The array is allocated using malloc, as is each
   element of the array, and they may all be passed to free when they
   are no longer needed.  If the falias argument is 0, the list will
   not include any aliases; otherwise, it will.  */
extern int uuconf_system_names (void *uuconf_pglobal,
				char ***uuconf_ppzsystems,
				int uuconf_falias);

/* Get the information for the system zsystem.  This sets the fields
   in *qsys.  This will work whether zsystem is the official name of
   the system or merely an alias.  */
extern int uuconf_system_info (void *uuconf_pglobal,
			       const char *uuconf_zsystem,
			       struct uuconf_system *uuconf_qsys);

/* Get information for an unknown (anonymous) system.  The
   uuconf_zname field of the returned system information will be NULL.
   If no information is available for unknown systems, this will
   return UUCONF_NOT_FOUND.  This does not run the HDB remote.unknown
   shell script.  */
extern int uuconf_system_unknown (void *uuconf_pglobal,
				  struct uuconf_system *uuconf_qsys);

/* Get information for the local system.  Normally the local system
   name should first be looked up using uuconf_system_info.  If that
   returns UUCONF_NOT_FOUND, this function may be used to get an
   appropriate set of defaults.  The uuconf_zname field of the
   returned system information may be NULL.  */
extern int uuconf_system_local (void *uuconf_pglobal,
				struct uuconf_system *uuconf_qsys);

/* Free the memory occupied by system information returned by
   uuconf_system_info, uuconf_system_unknown, uuconf_system_local, or
   any of the configuration file type specific routines described
   below.  After this is called, the contents of the structure shall
   not be referred to.  */
extern int uuconf_system_free (void *uuconf_pglobal,
			       struct uuconf_system *uuconf_qsys);

#ifdef __OPTIMIZE__
#define uuconf_system_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#endif

/* Find a matching port.  This will consider each port in turn.

   If the zname argument is not NULL, the port's uuconf_zname field
   must match it.

   If the ibaud argument is not zero and the ihighbaud argument is
   zero, the port's baud rate, if defined, must be the same (if the
   port has a range of baud rates, ibaud must be within the range).
   If ibaud and ihighbaud are both not zero, the port's baud rate, if
   defined, must be between ibaud and ihighbaud inclusive (if the port
   has a range of baud rates, the ranges must intersect).  If the port
   has no baud rate, either because it is a type of port for which
   baud rate is not defined (e.g. a TCP port) or because the
   uuconf_ibaud field is 0, the ibaud and ihighbaud arguments are
   ignored.

   If the pifn argument is not NULL, the port is passed to pifn, along
   with the pinfo argument (which is otherwise ignored).  If pifn
   returns UUCONF_SUCCESS, the port matches.  If pifn returns
   UUCONF_NOT_FOUND, a new port is sought.  Otherwise the return value
   of pifn is returned from uuconf_find_port.  The pifn function may
   be used to further restrict the port, such as by modem class or
   device name.  It may also be used to lock the port, if appropriate;
   in this case, if the lock fails, pifn may return UUCONF_NOT_FOUND
   to force uuconf_find_port to continue searching for a port.

   If the port matches, the information is set into uuconf_qport, and
   uuconf_find_port returns UUCONF_SUCCESS.  */
extern int uuconf_find_port (void *uuconf_pglobal,
			     const char *uuconf_zname,
			     long uuconf_ibaud,
			     long uuconf_ihighbaud,
			     int (*uuconf_pifn) (struct uuconf_port *,
						 void *uuconf_pinfo),
			     void *uuconf_pinfo,
			     struct uuconf_port *uuconf_qport);

/* Free the memory occupied by system information returned by
   uuconf_find_port (or any of the configuration file specific
   routines described below).  After this is called, the contents of
   the structure shall not be referred to.  */
extern int uuconf_port_free (void *uuconf_pglobal,
			     struct uuconf_port *uuconf_qport);

#ifdef __OPTIMIZE__
#define uuconf_port_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#endif

/* Get the names of all known dialers.  This sets sets *ppzdialers to
   point to an array of dialer names.  The list of names is NULL
   terminated.  The array is allocated using malloc, as is each
   element of the array, and they may all be passed to free when they
   are no longer needed.  */
extern int uuconf_dialer_names (void *uuconf_pglobal,
				char ***uuconf_ppzdialers);

/* Get the information for the dialer zdialer.  This sets the fields
   in *qdialer.  */
extern int uuconf_dialer_info (void *uuconf_pglobal,
			       const char *uuconf_zdialer,
			       struct uuconf_dialer *uuconf_qdialer);

/* Free the memory occupied by system information returned by
   uuconf_dialer_info (or any of the configuration file specific
   routines described below).  After this is called, the contents of
   the structure shall not be referred to.  */
extern int uuconf_dialer_free (void *uuconf_pglobal,
			       struct uuconf_dialer *uuconf_qsys);

#ifdef __OPTIMIZE__
#define uuconf_dialer_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#endif

/* Get the local node name.  If the node name is not specified
   (because no ``nodename'' command appeared in the config file) this
   will return UUCONF_NOT_FOUND, and some system dependent function
   must be used to determine the node name.  Otherwise it will return
   a pointer to a constant string, which should not be freed.  */
extern int uuconf_localname (void *uuconf_pglobal,
			     const char **pzname);

/* Get the local node name that should be used, given a login name.
   This function will check for any special local name that may be
   associated with the login name zlogin (as set by the ``myname''
   command in a Taylor configuration file, or the MYNAME field in a
   Permissions entry).  This will set *pzname to the node name.  If no
   node name can be determined, *pzname will be set to NULL and the
   function will return UUCONF_NOT_FOUND; in this case some system
   dependent function must be used to determine the node name.  If the
   function returns UUCONF_SUCCESS, *pzname will be point to an
   malloced buffer.  */
extern int uuconf_login_localname (void *uuconf_pglobal,
				   const char *uuconf_zlogin,
				   char **pzname);

/* Get the name of the UUCP spool directory.  This will set *pzspool
   to a constant string, which should not be freed.  */
extern int uuconf_spooldir (void *uuconf_pglobal,
			    const char **uuconf_pzspool);

/* Get the name of the default UUCP public directory.  This will set
   *pzpub to a constant string, which should not be freed.  Note that
   particular systems may use a different public directory.  */
extern int uuconf_pubdir (void *uuconf_pglobal,
			  const char **uuconf_pzpub);

/* Get the name of the UUCP lock directory.  This will set *pzlock to
   a constant string, which should not be freed.  */
extern int uuconf_lockdir (void *uuconf_pglobal,
			   const char **uuconf_pzlock);

/* Get the name of the UUCP log file.  This will set *pzlog to a
   constant string, which should not be freed.  */
extern int uuconf_logfile (void *uuconf_pglobal,
			   const char **uuconf_pzlog);

/* Get the name of the UUCP statistics file.  This will set *pzstats
   to a constant string, which should not be freed.  */
extern int uuconf_statsfile (void *uuconf_pglobal,
			     const char **uuconf_pzstats);

/* Get the name of the UUCP debugging file.  This will set *pzdebug to
   a constant string, which should not be freed.  */
extern int uuconf_debugfile (void *uuconf_pglobal,
			     const char **uuconf_pzdebug);

/* Get the default debugging level to use.  This basically gets the
   argument of the ``debug'' command from the Taylor UUCP config file.
   It will set *pzdebug to a constant string, which should not be
   freed.  */
extern int uuconf_debuglevel (void *uuconf_pglobal,
			      const char **uuconf_pzdebug);

/* Get a combination of UUCONF_STRIP bits indicating what types of
   global information should be stripped on input.  */
extern int uuconf_strip (void *uuconf_pglobal,
			 int *uuconf_pistrip);

/* Get the maximum number of simultaneous uuxqt executions.  This will
   set *pcmaxuuxqt to the number.  Zero indicates no maximum.  */
extern int uuconf_maxuuxqts (void *uuconf_pglobal,
			     int *uuconf_pcmaxuuxqt);

/* Get the frequency with which to spawn a uuxqt process.  This
   returns an integer.  A positive number is the number of execution
   files that should be received between spawns.  Other values are one
   of the UUCONF_RUNUUXQT constants listed above.  */
extern int uuconf_runuuxqt (void *uuconf_pglobal,
			    int *uuconf_pirunuuxqt);

/* Check a login name and password.  This checks the Taylor UUCP
   password file (not /etc/passwd).  It will work even if
   uuconf_taylor_init was not called.  All comparisons are done via a
   callback function.  The first argument to the function will be zero
   when comparing login names, non-zero when comparing passwords.  The
   second argument to the function will be the pinfo argument passed
   to uuconf_callin.  The third argument will be the login name or
   password from the UUCP password file.  The comparison function
   should return non-zero for a match, or zero for a non-match.  If
   the login name is found and the password compares correctly,
   uuconf_callin will return UUCONF_SUCCESS.  If the login is not
   found, or the password does not compare correctly, uuconf_callin
   will return UUCONF_NOT_FOUND.  Other errors are also possible.  */
extern int uuconf_callin (void *uuconf_pglobal,
			  int (*uuconf_cmp) (int, void *, const char *),
			  void *uuconf_pinfo);

/* Get the callout login name and password for a system.  This will
   set both *pzlog and *pzpass to a string allocated by malloc, or to
   NULL if the value is not found.  If neither value is found, the
   function will return UUCONF_NOT_FOUND.  */
extern int uuconf_callout (void *uuconf_pglobal,
			   const struct uuconf_system *uuconf_qsys,
			   char **uuconf_pzlog,
			   char **uuconf_pzpass);

/* See if a login name is permitted for a system.  This will return
   UUCONF_SUCCESS if it is permitted or UUCONF_NOT_FOUND if it is
   invalid.  This simply calls uuconf_taylor_validate or returns
   UUCONF_SUCCESS, depending on the value of HAVE_TAYLOR_CONFIG.  */
extern int uuconf_validate (void *uuconf_pglobal,
			    const struct uuconf_system *uuconf_qsys,
			    const char *uuconf_zlogin);

/* Get the name of the HDB remote.unknown shell script, if using
   HAVE_HDB_CONFIG.  This does not actually run the shell script.  If
   the function returns UUCONF_SUCCESS, the name will be in *pzname,
   which will point to an malloced buffer.  If it returns
   UUCONF_NOT_FOUND, then there is no script to run.  */
extern int uuconf_remote_unknown (void *uuconf_pglobal,
				  char **pzname);

/* Translate a dial code.  This sets *pznum to an malloced string.
   This will look up the entire zdial string in the dialcode file, so
   for normal use the alphabetic prefix should be separated.  */
extern int uuconf_dialcode (void *uuconf_pglobal,
			    const char *uuconf_zdial,
			    char **uuconf_pznum);

/* Compare two grades, returning < 0 if b1 should be executed before
   b2, == 0 if they are the same, or > 0 if b1 should be executed
   after b2.  This can not fail, and does not return a standard uuconf
   error code; it is normally called via the macro UUCONF_GRADE_CMP,
   defined above.  */
extern int uuconf_grade_cmp (int uuconf_b1, int uuconf_b2);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_init ();
extern int uuconf_init_thread ();
extern int uuconf_system_names ();
extern int uuconf_system_info ();
extern int uuconf_system_unknown ();
extern int uuconf_system_local ();
extern int uuconf_system_free ();
extern int uuconf_find_port ();
extern int uuconf_port_free ();
extern int uuconf_dialer_names ();
extern int uuconf_dialer_info ();
extern int uuconf_dialer_free ();
extern int uuconf_localname ();
extern int uuconf_login_localname ();
extern int uuconf_spooldir ();
extern int uuconf_lockdir ();
extern int uuconf_pubdir ();
extern int uuconf_logfile ();
extern int uuconf_statsfile ();
extern int uuconf_debugfile ();
extern int uuconf_debuglevel ();
extern int uuconf_maxuuxqts ();
extern int uuconf_runuuxqt ();
extern int uuconf_callin ();
extern int uuconf_callout ();
extern int uuconf_remote_unknown ();
extern int uuconf_validate ();
extern int uuconf_grade_cmp ();

#ifdef __OPTIMIZE__
#define uuconf_system_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#define uuconf_port_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#define uuconf_dialer_free(qglob, q) \
  (uuconf_free_block ((q)->uuconf_palloc), UUCONF_SUCCESS)
#endif

#endif /* ! UUCONF_ANSI_C */

#if UUCONF_ANSI_C

/* Initialize the Taylor UUCP configuration file reading routines.
   This must be called before calling any of the Taylor UUCP
   configuration file specific routines.  The ppglobal argument should
   point to a generic pointer.  Moreover, before calling this function
   the pointer either must be set to NULL, or must have been passed to
   one of the other uuconf init routines.  The zprogram argument is
   the name of the program for which files should be read.  If NULL,
   it is taken as "uucp", which means to read the standard UUCP files.
   The zname argument is the name of the config file.  If it is NULL,
   the default config file will be used.

   Note that if the zname argument is obtained from the user running
   the program, the program should be careful to revoke any special
   privileges it may have (e.g. on Unix call setuid (getuid ()) and
   setgid (getgid ())).  Otherwise various sorts of spoofing become
   possible.  */
extern int uuconf_taylor_init (void **uuconf_pglobal,
			       const char *uuconf_zprogram,
			       const char *uuconf_zname);

/* Get the names of all systems listed in the Taylor UUCP
   configuration files.  This sets *ppzsystems to point to an array of
   system names.  The list of names is NULL terminated.  The array is
   allocated using malloc, as is each element of the array.  If the
   falias argument is 0, the list will not include any aliases;
   otherwise, it will.  */
extern int uuconf_taylor_system_names (void *uuconf_pglobal,
				       char ***uuconf_ppzsystems,
				       int uuconf_falias);

/* Get the information for system zsystem from the Taylor UUCP
   configuration files.  This will set *qsys.   */
extern int uuconf_taylor_system_info (void *uuconf_pglobal,
				      const char *uuconf_zsystem,
				      struct uuconf_system *uuconf_qsys);

/* Get information for an unknown (anonymous) system.  This returns
   the values set by the ``unknown'' command in the main configuration
   file.  If the ``unknown'' command was not used, this will return
   UUCONF_NOT_FOUND.  */
extern int uuconf_taylor_system_unknown (void *uuconf_pglobal,
					 struct uuconf_system *uuconf_qsys);

/* Find a port from the Taylor UUCP configuration files.  The
   arguments and return values are identical to those of
   uuconf_find_port.  */
extern int uuconf_taylor_find_port (void *uuconf_pglobal,
				    const char *uuconf_zname,
				    long uuconf_ibaud,
				    long uuconf_ihighbaud,
				    int (*uuconf_pifn) (struct uuconf_port *,
							void *uuconf_pinfo),
				    void *uuconf_pinfo,
				    struct uuconf_port *uuconf_qport);

/* Get the names of all dialers listed in the Taylor UUCP
   configuration files.  This sets *ppzdialers to point to an array of
   dialer names.  The list of names is NULL terminated.  The array is
   allocated using malloc, as is each element of the array.  */
extern int uuconf_taylor_dialer_names (void *uuconf_pglobal,
				       char ***uuconf_ppzdialers);

/* Get the information for the dialer zdialer from the Taylor UUCP
   configuration files.  This sets the fields in *qdialer.  */
extern int uuconf_taylor_dialer_info (void *uuconf_pglobal,
				      const char *uuconf_zdialer,
				      struct uuconf_dialer *uuconf_qdialer);

/* Get the local node name that should be used, given a login name,
   considering only the ``myname'' command in the Taylor UUCP
   configuration files.  If the function returns UUCONF_SUCCESS,
   *pzname will point to an malloced buffer.  */
extern int uuconf_taylor_login_localname (void *uuconf_pglobal,
					  const char *uuconf_zlogin,
					  char **pzname);

/* Get the callout login name and password for a system from the
   Taylor UUCP configuration files.  This will set both *pzlog and
   *pzpass to a string allocated by malloc, or to NULL if the value is
   not found.  If neither value is found, the function will return
   UUCONF_NOT_FOUND.  */
extern int uuconf_taylor_callout (void *uuconf_pglobal,
				  const struct uuconf_system *uuconf_qsys,
				  char **uuconf_pzlog,
				  char **uuconf_pzpass);

/* See if a login name is permitted for a system.  This will return
   UUCONF_SUCCESS if it is permitted or UUCONF_NOT_FOUND if it is
   invalid.  This checks whether the login name appears in a
   called-login command with a list of system which does not include
   the system qsys.  */
extern int uuconf_taylor_validate (void *uuconf_pglobal,
				   const struct uuconf_system *uuconf_qsys,
				   const char *uuconf_zlogin);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_taylor_init ();
extern int uuconf_taylor_system_names ();
extern int uuconf_taylor_system_info ();
extern int uuconf_taylor_system_unknown ();
extern int uuconf_taylor_find_port ();
extern int uuconf_taylor_dialer_names ();
extern int uuconf_taylor_dialer_info ();
extern int uuconf_taylor_login_localname ();
extern int uuconf_taylor_callout ();
extern int uuconf_taylor_validate ();

#endif /* ! UUCONF_ANSI_C */

#if UUCONF_ANSI_C

/* Initialize the V2 configuration file reading routines.  This must
   be called before any of the other V2 routines are called.  The
   ppglobal argument should point to a generic pointer.  Moreover,
   before calling this function the pointer either must be set to
   NULL, or must have been passed to one of the other uuconf init
   routines.  */
extern int uuconf_v2_init (void **uuconf_ppglobal);

/* Get the names of all systems listed in the V2 configuration files.
   This sets *ppzsystems to point to an array of system names.  The
   list of names is NULL terminated.  The array is allocated using
   malloc, as is each element of the array.  If the falias argument is
   0, the list will not include any aliases; otherwise, it will.  */
extern int uuconf_v2_system_names (void *uuconf_pglobal,
				   char ***uuconf_ppzsystems,
				   int uuconf_falias);

/* Get the information for system zsystem from the V2 configuration
   files.  This will set *qsys.  */
extern int uuconf_v2_system_info (void *uuconf_pglobal,
				  const char *uuconf_zsystem,
				  struct uuconf_system *uuconf_qsys);

/* Find a port from the V2 configuration files.  The arguments and
   return values are identical to those of uuconf_find_port.  */
extern int uuconf_v2_find_port (void *uuconf_pglobal,
				const char *uuconf_zname,
				long uuconf_ibaud,
				long uuconf_ihighbaud,
				int (*uuconf_pifn) (struct uuconf_port *,
						    void *uuconf_pinfo),
				void *uuconf_pinfo,
				struct uuconf_port *uuconf_qport);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_v2_init ();
extern int uuconf_v2_system_names ();
extern int uuconf_v2_system_info ();
extern int uuconf_v2_find_port ();

#endif /* ! UUCONF_ANSI_C */

#if UUCONF_ANSI_C

/* Initialize the HDB configuration file reading routines.  This
   should be called before any of the other HDB routines are called.
   The ppglobal argument should point to a generic pointer.  Moreover,
   before calling this function the pointer either must be set to
   NULL, or must have been passed to one of the other uuconf init
   routines.  The zprogram argument is used to match against a
   "services" string in Sysfiles.  A NULL or "uucp" argument is taken
   as "uucico".  */
extern int uuconf_hdb_init (void **uuconf_ppglobal,
			    const char *uuconf_zprogram);

/* Get the names of all systems listed in the HDB configuration files.
   This sets *ppzsystems to point to an array of system names.  The
   list of names is NULL terminated.  The array is allocated using
   malloc, as is each element of the array.  If the falias argument is
   0, the list will not include any aliases; otherwise, it will (an
   alias is created by using the ALIAS= keyword in the Permissions
   file).  */
extern int uuconf_hdb_system_names (void *uuconf_pglobal,
				    char ***uuconf_ppzsystems,
				    int uuconf_falias);

/* Get the information for system zsystem from the HDB configuration
   files.  This will set *qsys.  */
extern int uuconf_hdb_system_info (void *uuconf_pglobal,
				   const char *uuconf_zsystem,
				   struct uuconf_system *uuconf_qsys);


/* Get information for an unknown (anonymous) system.  If no
   information is available for unknown systems, this will return
   UUCONF_NOT_FOUND.  This does not run the remote.unknown shell
   script.  */
extern int uuconf_hdb_system_unknown (void *uuconf_pglobal,
				      struct uuconf_system *uuconf_qsys);

/* Find a port from the HDB configuration files.  The arguments and
   return values are identical to those of uuconf_find_port.  */
extern int uuconf_hdb_find_port (void *uuconf_pglobal,
				 const char *uuconf_zname,
				 long uuconf_ibaud,
				 long uuconf_ihighbaud,
				 int (*uuconf_pifn) (struct uuconf_port *,
						     void *uuconf_pinfo),
				 void *uuconf_pinfo,
				 struct uuconf_port *uuconf_qport);

/* Get the names of all dialers listed in the HDB configuration files.
   This sets *ppzdialers to point to an array of dialer names.  The
   list of names is NULL terminated.  The array is allocated using
   malloc, as is each element of the array.  */
extern int uuconf_hdb_dialer_names (void *uuconf_pglobal,
				    char ***uuconf_ppzdialers);

/* Get the information for the dialer zdialer from the HDB
   configuration files.  This sets the fields in *qdialer.  */
extern int uuconf_hdb_dialer_info (void *uuconf_pglobal,
				   const char *uuconf_zdialer,
				   struct uuconf_dialer *uuconf_qdialer);

/* Get the local node name that should be used, given a login name,
   considering only the MYNAME field in the HDB Permissions file.  If
   the function returns UUCONF_SUCCESS, *pzname will point to an
   malloced buffer.  */
extern int uuconf_hdb_login_localname (void *uuconf_pglobal,
				       const char *uuconf_zlogin,
				       char **pzname);

/* Get the name of the HDB remote.unknown shell script.  This does not
   actually run the shell script.  If the function returns
   UUCONF_SUCCESS, the name will be in *pzname, which will point to an
   malloced buffer.  */
extern int uuconf_hdb_remote_unknown (void *uuconf_pglobal,
				      char **pzname);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_hdb_init ();
extern int uuconf_hdb_system_names ();
extern int uuconf_hdb_system_info ();
extern int uuconf_hdb_system_unknown ();
extern int uuconf_hdb_find_port ();
extern int uuconf_hdb_dialer_names ();
extern int uuconf_hdb_dialer_info ();
extern int uuconf_hdb_localname ();
extern int uuconf_hdb_remote_unknown ();

#endif /* ! UUCONF_ANSI_C */

#if UUCONF_ANSI_C

/* This function will set an appropriate error message into the buffer
   zbuf, given a uuconf error code.  The buffer will always be null
   terminated, and will never be accessed beyond the length cbuf.
   This function will return the number of characters needed for the
   complete message, including the null byte.  If this is less than
   the cbytes argument, the buffer holds a truncated string.  */
extern int uuconf_error_string (void *uuconf_pglobal, int ierror,
				char *zbuf, UUCONF_SIZE_T cbuf);

/* If UUCONF_ERROR_ERRNO is set in a return value, this function may
   be used to retrieve the errno value.  This will be the value of
   errno as set by the system function which failed.  However, some
   system functions, notably some stdio routines, may not set errno,
   in which case the value will be meaningless.  This function does
   not return a uuconf error code, and it cannot fail.  */
extern int uuconf_error_errno (void *uuconf_pglobal);

/* If UUCONF_ERROR_FILENAME is set in a return value, this function
   may be used to retrieve the file name.  This function does not
   return a uuconf error code, and it cannot fail.  The string that it
   returns a pointer to is not guaranteed to remain allocated across
   the next call to a uuconf function (other than one of the three
   error retrieving functions).  */
extern const char *uuconf_error_filename (void *uuconf_pglobal);

/* If UUCONF_ERROR_LINENO is set in a return value, this function may
   be used to retrieve the line number.  This function does not return
   a uuconf error code, and it cannot fail.  */
extern int uuconf_error_lineno (void *uuconf_pglobal);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_error_string ();
extern int uuconf_error_errno ();
extern UUCONF_CONST char *uuconf_error_filename ();
extern int uuconf_error_lineno ();

#endif /* ! UUCONF_ANSI_C */

/* The uuconf package also provides a few functions which can accept
   commands and parcel them out according to a table.  These are
   publically visible, partially in the hopes that they will be
   useful, but mostly because the rest of the Taylor UUCP package uses
   them.  */

/* The types of entries allowed in a command table (struct
   uuconf_cmdtab).  Each type defines how a particular command is
   interpreted.  Each type will either assign a value to a variable or
   call a function.  In all cases, a line of input is parsed into
   separate fields, separated by whitespace; comments beginning with
   '#' are discarded, except that a '#' preceeded by a backslash is
   retained.  The first field is taken as the command to execute, and
   the remaining fields are its arguments.  */

/* A boolean value.  Used for a command which accepts a single
   argument, which must begin with 'y', 'Y', 't', or 'T' for true (1)
   or 'n', 'N', 'f', or 'F' for false (0).  The corresponding variable
   must be an int.  */
#define UUCONF_CMDTABTYPE_BOOLEAN (0x12)

/* An integer value.  Used for a command which accepts a single
   argument, which must be an integer.  The corresponding variable
   must be an int.  */
#define UUCONF_CMDTABTYPE_INT (0x22)

/* A long value.  Used for a command which accepts a single value,
   which must be an integer.  The corresponding variable must be a
   long.  */
#define UUCONF_CMDTABTYPE_LONG (0x32)

/* A string value.  Used for a command which accepts a string
   argument.  If there is no argument, the variable will be set to
   point to a zero byte.  Otherwise the variable will be set to point
   to the string.  The corresponding variable must be a char *.  The
   memory pointed to by the variable after it is set must not be
   modified.  */
#define UUCONF_CMDTABTYPE_STRING (0x40)

/* A full string value.  Used for a command which accepts a series of
   string arguments separated by whitespace.  The corresponding
   variable must be a char **.  It will be set to an NULL terminated
   array of the arguments.  The memory occupied by the array itself,
   and by the strings within it, must not be modified.  */
#define UUCONF_CMDTABTYPE_FULLSTRING (0x50)

/* A function.  If this command is encountered, the command and its
   arguments are passed to the corresponding function.  They are
   passed as an array of strings, in which the first string is the
   command itself, along with a count of strings.  This value may be
   or'red with a specific number of required arguments;
   UUCONF_CMDTABTYPE_FN | 1 accepts no additional arguments besides
   the command itself, UUCONF_CMDTABTYPE_FN | 2 accepts 1 argument,
   etc.  UUCONF_CMDTABTYPE_FN | 0, accepts any number of additional
   arguments.  */
#define UUCONF_CMDTABTYPE_FN (0x60)

/* A prefix function.  The string in the table is a prefix; if a
   command is encountered with the same prefix, the corresponding
   function will be called as for UUCONF_CMDTABTYPE_FN.  The number of
   arguments may be or'red in as with UUCONF_CMDTABTYPE_FN.  */
#define UUCONF_CMDTABTYPE_PREFIX (0x70)

/* This macro will return the particular type of a CMDTABTYPE.  */
#define UUCONF_TTYPE_CMDTABTYPE(i) ((i) & 0x70)

/* This macro will return the required number of arguments of a
   CMDTABTYPE.  If it is zero, there is no restriction.  */
#define UUCONF_CARGS_CMDTABTYPE(i) ((i) & 0x0f)

/* When a function is called via UUCONF_CMDTABTYPE_FN or
   UUCONF_CMDTABTYPE_PREFIX, it may return any uuconf error code (see
   above).  However, it will normally return one of the following:

   UUCONF_CMDTABRET_CONTINUE: Take no special action.  In particular,
   the arguments passed to the function may be overwritten or freed.

   UUCONF_CMDTABRET_KEEP: The memory occupied by the arguments passed
   to the function must be preserved.  Continue processing commands.

   UUCONF_CMDTABRET_EXIT: If reading commands from a file, stop
   processing.  The arguments passed to the function may be
   overwritten or freed.

   UUCONF_CMDTABRET_KEEP_AND_EXIT: Stop processing any file.  The
   memory occupied by the arguments passed to the function must be
   preserved.

   These values are interpreted by uuconf_cmd_file.  The
   uuconf_cmd_line and uuconf_cmd_args functions may return
   UUCONF_CMDTABRET_KEEP.  It they get an error, they will return an
   error code with UUCONF_CMDTABRET_EXIT set.  Also, of course, they
   may return any value that is returned by one of the user functions
   in the uuconf_cmdtab table.  */

/* UUCONF_CMDTABRET_KEEP and UUCONF_CMDTABRET_EXIT are defined above,
   with the error codes.  */

#define UUCONF_CMDTABRET_CONTINUE UUCONF_SUCCESS
#define UUCONF_CMDTABRET_KEEP_AND_EXIT \
  (UUCONF_CMDTABRET_KEEP | UUCONF_CMDTABRET_EXIT)

/* When a function is called via CMDTABTYPE_FN or CMDTABTYPE_PREFIX,
   it is passed five arguments.  This is the type of a pointer to such
   a function.  The uuconf global information structure is passed in
   for convenience in calling another uuconf function.  The arguments
   to the command are passed in (the command itself is the first
   argument) along with a count and the value of the pvar field from
   the uuconf_cmdtab structure in which the function pointer was
   found.  The pinfo argument to the function is taken from the
   argument to uuconf_cmd_*.  */

#if UUCONF_ANSI_C
typedef int (*uuconf_cmdtabfn) (void *uuconf_pglobal,
				int uuconf_argc,
				char **uuconf_argv,
				void *uuconf_pvar,
				void *uuconf_pinfo);
#else
typedef int (*uuconf_cmdtabfn) ();
#endif

/* A table of commands is an array of the following structures.  The
   final element of the table should have uuconf_zcmd == NULL.  */

struct uuconf_cmdtab
{
  /* Command name.  */
  UUCONF_CONST char *uuconf_zcmd;
  /* Command type (one of CMDTABTYPE_*).  */
  int uuconf_itype;
  /* If not CMDTABTYPE_FN or CMDTABTYPE_PREFIX, the address of the
     associated variable.  Otherwise, a pointer value to pass to the
     function pifn.  */
  UUCONF_POINTER uuconf_pvar;
  /* The function to call if CMDTABTYPE_FN or CMDTABTYPE_PREFIX.  */
  uuconf_cmdtabfn uuconf_pifn;
};

/* Bit flags to pass to uuconf_processcmds.  */

/* If set, case is significant when checking commands.  Normally case
   is ignored.  */
#define UUCONF_CMDTABFLAG_CASE (0x1)

/* If set, a backslash at the end of a line may be used to include the
   next physical line in the logical line.  */
#define UUCONF_CMDTABFLAG_BACKSLASH (0x2)

/* If set, the comment character (#) is treated as a normal character,
   rather than as starting a comment.  */
#define UUCONF_CMDTABFLAG_NOCOMMENTS (0x4)

#if UUCONF_ANSI_C

/* Read commands from a file, look them up in a table, and take the
   appropriate action.  This continues reading lines from the file
   until EOF, or until a function returns with UUCONF_CMDTABRET_EXIT
   set, or until an error occurs.  The qtab argument must point to a
   table of struct uuconf_cmdtab; the last element in the table should
   have uuconf_zcmd == NULL.  When a UUCONF_CMDTABTYPE_FN or
   UUCONF_CMDTABTYPE_PREFIX command is found, the pinfo argument will
   be passed to the called function.  If an a command is found that is
   not in the table, then if pfiunknownfn is NULL the unknown command
   is ignored; otherwise it is passed to pfiunknownfn, which should
   return a uuconf return code which is handled as for any other
   function (the pvar argument to pfiunknownfn will always be NULL).
   The iflags argument is any combination of the above
   UUCONF_CMDTABFLAG bits.  The pblock argument may also be a memory
   block, as returned by uuconf_malloc_block (described below), in
   which case all memory preserved because of UUCONF_CMDTABRET_KEEP
   will be added to the block so that it may be freed later; it may
   also be NULL, in which case any such memory is permanently lost.

   This function initially sets the internal line number to 0, and
   then increments it as each line is read.  It is permitted for any
   called function to use the uuconf_lineno function to obtain it.  If
   this function is called when not at the start of a file, the value
   returned by uuconf_lineno (which is, in any case, only valid if an
   error code with UUCONF_ERROR_LINENO set is returned) must be
   adjusted by the caller.

   This returns a normal uuconf return value, as described above.  */
extern int uuconf_cmd_file (void *uuconf_pglobal,
			    FILE *uuconf_e,
			    const struct uuconf_cmdtab *uuconf_qtab,
			    void *uuconf_pinfo,
			    uuconf_cmdtabfn uuconf_pfiunknownfn,
			    int uuconf_iflags,
			    void *pblock);

/* This utility function is just like uuconf_cmd_file, except that it
   only operates on a single string.  If a function is called via
   qtab, its return value will be the return value of this function.
   UUCONF_CMDTABFLAG_BACKSLASH is ignored in iflags.  The string z is
   modified in place.  The return value may include the
   UUCONF_CMDTABRET_KEEP and, on error, the UUCONF_CMDTABRET_EXIT
   bits, which should be honored by the calling code.  */
extern int uuconf_cmd_line (void *uuconf_pglobal,
			    char *uuconf_z,
			    const struct uuconf_cmdtab *uuconf_qtab,
			    void *uuconf_pinfo,
			    uuconf_cmdtabfn uuconf_pfiunknownfn,
			    int uuconf_iflags,
			    void *pblock);

/* This utility function is just like uuconf_cmd_line, except it is
   given a list of already parsed arguments.  */
extern int uuconf_cmd_args (void *uuconf_pglobal,
			    int uuconf_cargs,
			    char **uuconf_pzargs,
			    const struct uuconf_cmdtab *uuconf_qtab,
			    void *uuconf_pinfo,
			    uuconf_cmdtabfn uuconf_pfiunknownfn,
			    int uuconf_iflags,
			    void *pblock);

#else /* ! UUCONF_ANSI_C */

extern int uuconf_cmd_file ();
extern int uuconf_cmd_line ();
extern int uuconf_cmd_args ();

#endif /* ! UUCONF_ANSI_C */

#if UUCONF_ANSI_C

/* The uuconf_cmd_file function may allocate memory permanently, as
   for setting a UUCONF_CMDTABTYPE_STRING value, in ways which are
   difficult to free up.  A memory block may be used to record all
   allocated memory, so that it can all be freed up at once at some
   later time.  These functions do not take a uuconf global pointer,
   and are independent of the rest of the uuconf library.  */

/* Allocate a block of memory.  If this returns NULL, then malloc
   returned NULL, and errno is whatever malloc set it to.  */
extern void *uuconf_malloc_block (void);

/* Allocate memory within a memory block.  If this returns NULL, then
   malloc returned NULL, and errno is whatever malloc set it to.  */
extern void *uuconf_malloc (void *uuconf_pblock,
			    UUCONF_SIZE_T uuconf_cbytes);

/* Add a block returned by the generic malloc routine to a memory
   block.  This returns zero on success, non-zero on failure.  If this
   fails (returns non-zero), then malloc returned NULL, and errno is
   whatever malloc set it to.  */
extern int uuconf_add_block (void *uuconf_pblock, void *uuconf_padd);

/* Free a value returned by uuconf_malloc from a memory block.  In the
   current implementation, this will normally not do anything, but it
   doesn't hurt.  No errors can occur.  */
extern void uuconf_free (void *uuconf_pblock, void *uuconf_pfree);

/* Free an entire memory block, including all values returned by
   uuconf_malloc from it and all values added to it with
   uuconf_add_block.  No errors can occur.  */
extern void uuconf_free_block (void *uuconf_pblock);

#else /* ! UUCONF_ANSI_C */

extern UUCONF_POINTER uuconf_malloc_block ();
extern UUCONF_POINTER uuconf_malloc ();
extern int uuconf_add_block ();
extern /* void */ uuconf_free ();
extern /* void */ uuconf_free_block ();

#endif /* ! UUCONF_ANSI_C */

#endif /* ! defined (UUCONF_H) */
