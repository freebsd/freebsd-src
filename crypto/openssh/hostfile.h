/*	$OpenBSD: hostfile.h,v 1.7 2001/02/08 19:30:51 itojun Exp $	*/

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
#ifndef HOSTFILE_H
#define HOSTFILE_H

int
auth_rsa_read_key(char **cpp, u_int *bitsp, BIGNUM * e, BIGNUM * n);

/*
 * Checks whether the given host is already in the list of our known hosts.
 * Returns HOST_OK if the host is known and has the specified key, HOST_NEW
 * if the host is not known, and HOST_CHANGED if the host is known but used
 * to have a different host key.  The host must be in all lowercase.
 */
typedef enum {
	HOST_OK, HOST_NEW, HOST_CHANGED
}       HostStatus;

HostStatus
check_host_in_hostfile(const char *filename, const char *host, Key *key,
    Key *found, int *line);

/*
 * Appends an entry to the host file.  Returns false if the entry could not
 * be appended.
 */
int	add_host_to_hostfile(const char *filename, const char *host, Key *key);

#endif
