/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: krb_err_txt.c,v 1.12 1997/04/02 05:37:10 joda Exp $");

/*
 * This file contains an array of error text strings.
 * The associated error codes (which are defined in "krb.h")
 * follow the string in the comments at the end of each line.
 */

const char *krb_err_txt[256] = {
  "OK",							/* 000 */
  "Principal expired (kerberos)",			/* 001 */
  "Service expired (kerberos)",				/* 002 */
  "Authentication expired (kerberos)",			/* 003 */
  "Unknown protocol version number (kerberos)", 	/* 004 */
  "Principal: Incorrect master key version (kerberos)", /* 005 */
  "Service: Incorrect master key version (kerberos)",   /* 006 */
  "Bad byte order (kerberos)",				/* 007 */
  "Principal unknown (kerberos)",			/* 008 */
  "Principal not unique (kerberos)",			/* 009 */
  "Principal has null key (kerberos)",			/* 010 */
  "Timeout in request (kerberos)",			/* 011 */
  "Reserved error message 12 (kerberos)",		/* 012 */
  "Reserved error message 13 (kerberos)",		/* 013 */
  "Reserved error message 14 (kerberos)",		/* 014 */
  "Reserved error message 15 (kerberos)",		/* 015 */
  "Reserved error message 16 (kerberos)",		/* 016 */
  "Reserved error message 17 (kerberos)",		/* 017 */
  "Reserved error message 18 (kerberos)",		/* 018 */
  "Reserved error message 19 (kerberos)",		/* 019 */
  "Permission Denied (kerberos)",			/* 020 */
  "Can't read ticket file (krb_get_cred)",		/* 021 */
  "Can't find ticket (krb_get_cred)",			/* 022 */
  "Reserved error message 23 (krb_get_cred)",		/* 023 */
  "Reserved error message 24 (krb_get_cred)",		/* 024 */
  "Reserved error message 25 (krb_get_cred)",		/* 025 */
  "Ticket granting ticket expired (krb_mk_req)",	/* 026 */
  "Reserved error message 27 (krb_mk_req)",		/* 027 */
  "Reserved error message 28 (krb_mk_req)",		/* 028 */
  "Reserved error message 29 (krb_mk_req)",		/* 029 */
  "Reserved error message 30 (krb_mk_req)",		/* 030 */
  "Can't decode authenticator (krb_rd_req)",		/* 031 */
  "Ticket expired (krb_rd_req)",			/* 032 */
  "Ticket issue date too far in the future (krb_rd_req)",/* 033 */
  "Repeat request (krb_rd_req)",			/* 034 */
  "Ticket for wrong server (krb_rd_req)",		/* 035 */
  "Request inconsistent (krb_rd_req)",			/* 036 */
  "Time is out of bounds (krb_rd_req)",			/* 037 */
  "Incorrect network address (krb_rd_req)",		/* 038 */
  "Protocol version mismatch (krb_rd_req)",		/* 039 */
  "Illegal message type (krb_rd_req)",			/* 040 */
  "Message integrity error (krb_rd_req)",		/* 041 */
  "Message duplicate or out of order (krb_rd_req)",	/* 042 */
  "Unauthorized request (krb_rd_req)",			/* 043 */
  "Reserved error message 44 (krb_rd_req)",		/* 044 */
  "Reserved error message 45 (krb_rd_req)",		/* 045 */
  "Reserved error message 46 (krb_rd_req)",		/* 046 */
  "Reserved error message 47 (krb_rd_req)",		/* 047 */
  "Reserved error message 48 (krb_rd_req)",		/* 048 */
  "Reserved error message 49 (krb_rd_req)",		/* 049 */
  "Reserved error message 50 (krb_rd_req)",		/* 050 */
  "Current password is NULL (get_pw_tkt)",		/* 051 */
  "Current password incorrect (get_pw_tkt)",		/* 052 */
  "Protocol error (gt_pw_tkt)",				/* 053 */
  "Error returned by KDC (gt_pw_tkt)",			/* 054 */
  "Null ticket returned by KDC (gt_pw_tkt)",		/* 055 */
  "Retry count exceeded (send_to_kdc)",			/* 056 */
  "Can't send request (send_to_kdc)",			/* 057 */
  "Reserved error message 58 (send_to_kdc)",		/* 058 */
  "Reserved error message 59 (send_to_kdc)",		/* 059 */
  "Reserved error message 60 (send_to_kdc)",		/* 060 */
  "Warning: Not ALL tickets returned",			/* 061 */
  "Password incorrect",					/* 062 */
  "Protocol error (get_in_tkt)",			/* 063 */
  "Reserved error message 64 (get_in_tkt)",		/* 064 */
  "Reserved error message 65 (get_in_tkt)",		/* 065 */
  "Reserved error message 66 (get_in_tkt)",		/* 066 */
  "Reserved error message 67 (get_in_tkt)",		/* 067 */
  "Reserved error message 68 (get_in_tkt)",		/* 068 */
  "Reserved error message 69 (get_in_tkt)",		/* 069 */
  "Generic error (get_in_tkt)(can't write ticket file)", /* 070 */
  "Don't have ticket granting ticket (get_ad_tkt)",	/* 071 */
  "Can't get inter-realm ticket granting ticket (get_ad_tkt)",	/* 072 */
  "Reserved error message 73 (get_ad_tkt)",		/* 073 */
  "Reserved error message 74 (get_ad_tkt)",		/* 074 */
  "Reserved error message 75 (get_ad_tkt)",		/* 075 */
  "No ticket file (tf_util)",				/* 076 */
  "Can't access ticket file (tf_util)",			/* 077 */
  "Can't lock ticket file; try later (tf_util)",	/* 078 */
  "Bad ticket file format (tf_util)",			/* 079 */
  "Read ticket file before tf_init (tf_util)",		/* 080 */
  "Bad Kerberos name format (kname_parse)",		/* 081 */
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "Generic kerberos error (kfailure)",			/* 255 */
};

static const char err_failure[] = "Illegal error code passed (krb_get_err_text)";

const char *
krb_get_err_text(int code)
{
  if(code < 0 || code >= MAX_KRB_ERRORS)
    return err_failure;
  return krb_err_txt[code];
}
