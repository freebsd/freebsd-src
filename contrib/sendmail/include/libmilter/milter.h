/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: milter.h,v 8.24.16.9 2001/03/02 21:22:48 geir Exp $
 */

/*
**  MILTER.H -- Global definitions for mail filter and MTA.
*/

#ifndef _LIBMILTER_MILTER_H
# define _LIBMILTER_MILTER_H	1

#include "libmilter/mfapi.h"
#include "sendmail.h"

/* Shared protocol constants */
# define MILTER_LEN_BYTES	4	/* length of 32 bit integer in bytes */
# define MILTER_OPTLEN	(MILTER_LEN_BYTES * 3) /* length of options */
# define MILTER_CHUNK_SIZE	65535	/* body chunk size */

/* address families */
# define SMFIA_UNKNOWN		'U'	/* unknown */
# define SMFIA_UNIX		'L'	/* unix/local */
# define SMFIA_INET		'4'	/* inet */
# define SMFIA_INET6		'6'	/* inet6 */

/* commands: don't use anything smaller than ' ' */
# define SMFIC_ABORT		'A'	/* Abort */
# define SMFIC_BODY		'B'	/* Body chunk */
# define SMFIC_CONNECT		'C'	/* Connection information */
# define SMFIC_MACRO		'D'	/* Define macro */
# define SMFIC_BODYEOB		'E'	/* final body chunk (End) */
# define SMFIC_HELO		'H'	/* HELO/EHLO */
# define SMFIC_HEADER		'L'	/* Header */
# define SMFIC_MAIL		'M'	/* MAIL from */
# define SMFIC_EOH		'N'	/* EOH */
# define SMFIC_OPTNEG		'O'	/* Option negotiation */
# define SMFIC_QUIT		'Q'	/* QUIT */
# define SMFIC_RCPT		'R'	/* RCPT to */

/* actions (replies) */
# define SMFIR_ADDRCPT		'+'	/* add recipient */
# define SMFIR_DELRCPT		'-'	/* remove recipient */
# define SMFIR_ACCEPT		'a'	/* accept */
# define SMFIR_REPLBODY		'b'	/* replace body (chunk) */
# define SMFIR_CONTINUE		'c'	/* continue */
# define SMFIR_DISCARD		'd'	/* discard */
# define SMFIR_CHGHEADER	'm'	/* change header */
# define SMFIR_PROGRESS		'p'	/* progress */
# define SMFIR_REJECT		'r'	/* reject */
# define SMFIR_TEMPFAIL		't'	/* tempfail */
# define SMFIR_ADDHEADER	'h'	/* add header */
# define SMFIR_REPLYCODE	'y'	/* reply code etc */

/* What the MTA can send/filter wants in protocol */
# define SMFIP_NOCONNECT 0x00000001L	/* MTA should not send connect info */
# define SMFIP_NOHELO	0x00000002L	/* MTA should not send HELO info */
# define SMFIP_NOMAIL	0x00000004L	/* MTA should not send MAIL info */
# define SMFIP_NORCPT	0x00000008L	/* MTA should not send RCPT info */
# define SMFIP_NOBODY	0x00000010L	/* MTA should not send body */
# define SMFIP_NOHDRS	0x00000020L	/* MTA should not send headers */
# define SMFIP_NOEOH	0x00000040L	/* MTA should not send EOH */

# define SMFI_V1_PROT	0x0000003FL	/* The protocol of V1 filter */
# define SMFI_V2_PROT	0x0000007FL	/* The protocol of V2 filter */
# define SMFI_CURR_PROT	SMFI_V2_PROT	/* The current version */

/* socket and thread portability */
# include <pthread.h>
typedef pthread_t	sthread_t;
typedef int		socket_t;

# define MAX_MACROS_ENTRIES	4	/* max size of macro pointer array */

/*
**  context for milter
**  implementation hint:
**  macros are stored in mac_buf[] as sequence of:
**  macro_name \0 macro_value
**  (just as read from the MTA)
**  mac_ptr is a list of pointers into mac_buf to the beginning of each
**  entry, i.e., macro_name, macro_value, ...
*/

struct smfi_str
{
	sthread_t	ctx_id;		/* thread id */
	socket_t	ctx_sd;		/* socket descriptor */
	int		ctx_dbg;	/* debug level */
	time_t		ctx_timeout;	/* timeout */
	int		ctx_state;	/* state */
	smfiDesc_ptr	ctx_smfi;	/* filter description */
	u_long		ctx_pflags;	/* protocol flags */
	char		**ctx_mac_ptr[MAX_MACROS_ENTRIES];
	char		*ctx_mac_buf[MAX_MACROS_ENTRIES];
	char		*ctx_reply;	/* reply code */
	void		*ctx_privdata;	/* private data */
};

#endif /* !_LIBMILTER_MILTER_H */
