/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions to interface with the SSH_AUTHENTICATION_FD socket.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
#ifndef AUTHFILE_H
#define AUTHFILE_H

/*
 * Saves the authentication (private) key in a file, encrypting it with
 * passphrase.
 * For RSA keys: The identification of the file (lowest 64 bits of n)
 * will precede the key to provide identification of the key without
 * needing a passphrase.
 */
int
save_private_key(const char *filename, const char *passphrase,
    Key * private_key, const char *comment);

/*
 * Loads the public part of the key file (public key and comment). Returns 0
 * if an error occurred; zero if the public key was successfully read.  The
 * comment of the key is returned in comment_return if it is non-NULL; the
 * caller must free the value with xfree.
 */
int load_public_key(const char *filename, Key * pub, char **comment_return);
int try_load_public_key(const char *filename, Key * pub, char **comment_return);

/*
 * Loads the private key from the file.  Returns 0 if an error is encountered
 * (file does not exist or is not readable, or passphrase is bad). This
 * initializes the private key.  The comment of the key is returned in
 * comment_return if it is non-NULL; the caller must free the value with
 * xfree.
 */
int
load_private_key(const char *filename, const char *passphrase,
    Key * private_key, char **comment_return);

#endif
