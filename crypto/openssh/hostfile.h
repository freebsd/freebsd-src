/*	$OpenBSD: hostfile.h,v 1.15 2005/03/01 10:40:26 djm Exp $	*/

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

typedef enum {
	HOST_OK, HOST_NEW, HOST_CHANGED, HOST_FOUND
}       HostStatus;

int	 hostfile_read_key(char **, u_int *, Key *);
HostStatus check_host_in_hostfile(const char *, const char *,
	    const Key *, Key *, int *);
int	add_host_to_hostfile(const char *, const char *, const Key *, int);
int	lookup_key_in_hostfile_by_type(const char *, const char *,
	    int, Key *, int *);

#define HASH_MAGIC	"|1|"
#define HASH_DELIM	'|'

char	*host_hash(const char *, const char *, u_int);

#endif
