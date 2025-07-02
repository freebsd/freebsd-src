/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/foreachaddr.h */
/*
 * Copyright 1990,1991,2000,2001,2002,2004 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 *
 * Iterate over the protocol addresses supported by this host, invoking
 * a callback function or three supplied by the caller.
 *
 * XNS support is untested, but "should just work".  (Hah!)
 */

/* This function iterates over all the addresses it can find for the
   local system, in one or two passes.  In each pass, and between the
   two, it can invoke callback functions supplied by the caller.  The
   two passes should operate on the same information, though not
   necessarily in the same order each time.  Duplicate and local
   addresses should be eliminated.  Storage passed to callback
   functions should not be assumed to be valid after foreach_localaddr
   returns.

   The int return value is an errno value (XXX or krb5_error_code
   returned for a socket error) if something internal to
   foreach_localaddr fails.  If one of the callback functions wants to
   indicate an error, it should store something via the 'data' handle.
   If any callback function returns a non-zero value,
   foreach_localaddr will clean up and return immediately.

   Multiple definitions are provided below, dependent on various
   system facilities for extracting the necessary information.  */

extern int
krb5int_foreach_localaddr (/*@null@*/ void *data,
                           int (*pass1fn) (/*@null@*/ void *,
                                           struct sockaddr *) /*@*/,
                           /*@null@*/ int (*betweenfn) (/*@null@*/ void *) /*@*/,
                           /*@null@*/ int (*pass2fn) (/*@null@*/ void *,
                                                      struct sockaddr *) /*@*/)
#if defined(DEBUG) || defined(TEST)
/*@modifies fileSystem@*/
#endif
    ;

#define foreach_localaddr krb5int_foreach_localaddr
