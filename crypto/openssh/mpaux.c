/*
 * 
 * mpaux.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Sun Jul 16 04:29:30 1995 ylo
 * 
 * This file contains various auxiliary functions related to multiple
 * precision integers.
 * 
*/

#include "includes.h"
RCSID("$Id: mpaux.c,v 1.9 1999/12/08 22:37:42 markus Exp $");

#include <ssl/bn.h>
#include "getput.h"
#include "xmalloc.h"

#include <ssl/md5.h>

void
compute_session_id(unsigned char session_id[16],
		   unsigned char cookie[8],
		   BIGNUM* host_key_n,
		   BIGNUM* session_key_n)
{
	unsigned int host_key_bytes = BN_num_bytes(host_key_n);
	unsigned int session_key_bytes = BN_num_bytes(session_key_n);
	unsigned int bytes = host_key_bytes + session_key_bytes;
	unsigned char *buf = xmalloc(bytes);
	MD5_CTX md;

	BN_bn2bin(host_key_n, buf);
	BN_bn2bin(session_key_n, buf + host_key_bytes);
	MD5_Init(&md);
	MD5_Update(&md, buf, bytes);
	MD5_Update(&md, cookie, 8);
	MD5_Final(session_id, &md);
	memset(buf, 0, bytes);
	xfree(buf);
}
