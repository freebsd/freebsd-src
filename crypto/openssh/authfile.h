/*	$OpenBSD: authfile.h,v 1.9 2002/03/04 17:27:39 stevesk Exp $	*/

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

#ifndef AUTHFILE_H
#define AUTHFILE_H

int	 key_save_private(Key *, const char *, const char *, const char *);
Key	*key_load_public(const char *, char **);
Key	*key_load_public_type(int, const char *, char **);
Key	*key_load_private(const char *, const char *, char **);
Key	*key_load_private_type(int, const char *, const char *, char **);

#endif
