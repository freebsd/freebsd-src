/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains various auxiliary functions related to multiple
 * precision integers.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: mpaux.h,v 1.9 2000/12/19 23:17:57 markus Exp $"); */

#ifndef MPAUX_H
#define MPAUX_H

/*
 * Computes a 16-byte session id in the global variable session_id. The
 * session id is computed by concatenating the linearized, msb first
 * representations of host_key_n, session_key_n, and the cookie.
 */
void
compute_session_id(u_char session_id[16],
    u_char cookie[8],
    BIGNUM * host_key_n,
    BIGNUM * session_key_n);

#endif				/* MPAUX_H */
