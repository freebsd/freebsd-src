/*	$OpenBSD: readpass.h,v 1.2 2001/01/29 01:58:17 niklas Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/*
 * Reads a passphrase from /dev/tty with echo turned off.  Returns the
 * passphrase (allocated with xmalloc).  Exits if EOF is encountered. If
 * from_stdin is true, the passphrase will be read from stdin instead.
 */
char   *read_passphrase(char *prompt, int from_stdin);
