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

/* $OpenBSD: authfile.h,v 1.6 2001/03/26 08:07:08 markus Exp $ */

#ifndef AUTHFILE_H
#define AUTHFILE_H

int
key_save_private(Key *key, const char *filename, const char *passphrase,
    const char *comment);

Key *
key_load_public(const char *filename, char **commentp);

Key *
key_load_public_type(int type, const char *filename, char **commentp);

Key *
key_load_private(const char *filename, const char *passphrase,
    char **commentp);

Key *
key_load_private_type(int type, const char *filename, const char *passphrase,
    char **commentp);

#endif
