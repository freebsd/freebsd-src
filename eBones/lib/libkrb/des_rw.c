/*
 * Copyright (c) 1994 Geoffrey M. Rehmet, Rhodes University
 * All rights reserved.
 *
 * This code is derived from a specification based on software
 * which forms part of the 4.4BSD-Lite distribution, which was developed
 * by the University of California and its contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the entire comment,
 *    including the above copyright notice, this list of conditions
 *    and the following disclaimer, verbatim, at the beginning of
 *    the source file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Geoffrey M. Rehmet
 * 4. Neither the name of Geoffrey M. Rehmet nor that of Rhodes University
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL GEOFFREY M. REHMET OR RHODES UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: des_rw.c,v 1.9 1997/06/14 02:29:19 ache Exp $
 */

/*
 *
 *	NB: THESE ROUTINES WILL FAIL IF NON-BLOCKING I/O IS USED.
 *
 */

/*
 * Routines for reading and writing DES encrypted messages onto sockets.
 * (These routines will fail if non-blocking I/O is used.)
 *
 * When a message is written, its length is first transmitted as an int,
 * in network byte order.  The encrypted message is then transmitted,
 * to a multiple of 8 bytes.  Messages shorter than 8 bytes are right
 * justified into a buffer of length 8 bytes, and the remainder of the
 * buffer is filled with random garbage (before encryption):
 *
 *     DDD -------->--+--------+
 *                    |        |
 *     +--+--+--+--+--+--+--+--+
 *     |x |x |x |x |x |D |D |D |
 *     +--+--+--+--+--+--+--+--+
 *     |  garbage     | data   |
 *     |                       |
 *     +-----------------------+----> des_pcbc_encrypt() -->
 *
 * (Note that the length field sent before the actual message specifies
 * the number of data bytes, not the length of the entire padded message.
 *
 * When data is read, if the message received is longer than the number
 * of bytes requested, then the remaining bytes are stored until the
 * following call to des_read().  If the number of bytes received is
 * less then the number of bytes received, then only the number of bytes
 * actually received is returned.
 *
 * This interface corresponds with the original des_rw.c, except for the
 * bugs in des_read() in the original 4.4BSD version.  (One bug is
 * normally not visible, due to undocumented behaviour of
 * des_pcbc_encrypt() in the original MIT libdes.)
 *
 * XXX Todo:
 *	1) Give better error returns on writes
 *	2) Improve error checking on reads
 *	3) Get rid of need for extern decl. of krb_net_read()
 *	4) Tidy garbage generation a bit
 *	5) Make the above comment more readable
 */

#ifdef CRYPT
#ifdef KERBEROS

#ifndef BUFFER_LEN
#define	BUFFER_LEN	10240
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/types.h>

#include <des.h>
#include <krb.h>

static des_cblock	des_key;
static des_key_schedule	key_schedule;

/*
 * Buffer for storing extra data when more data is received, then was
 * actually requested in des_read().
 */
static u_char		des_buff[BUFFER_LEN];
static u_char		buffer[BUFFER_LEN];
static unsigned		stored = 0;
static u_char		*buff_ptr = buffer;

/*
 * Set the encryption key for des_read() and des_write().
 * inkey is the initial vector for the DES encryption, while insched is
 * the DES key, in unwrapped form.
 */

int
des_set_key_krb(inkey, insched)
	des_cblock *inkey;
	des_key_schedule insched;
{
	bcopy(inkey, des_key, sizeof(des_cblock));
	bcopy(insched, &key_schedule, sizeof(des_key_schedule));
	return 0;
}

/*
 * Clear the key schedule, and initial vector, which were previously
 * stored in static vars by des_set_key().
 */
void
des_clear_key_krb()
{
	bzero(&des_key, sizeof(des_cblock));
	bzero(&key_schedule, sizeof(des_key_schedule));
}

int
des_read(fd, buf, len)
	int fd;
	register char * buf;
	int len;
{
	int	msg_length;	/* length of actual message data */
	int	pad_length;	/* length of padded message */
	int	nread;		/* number of bytes actually read */
	int	nreturned = 0;

	if(stored >= len) {
		bcopy(buff_ptr, buf, len);
		stored -= len;
		buff_ptr += len;
		return(len);
	} else {
		if (stored) {
			bcopy(buff_ptr, buf, stored);
			nreturned = stored;
			len -= stored;
			stored = 0;
			buff_ptr = buffer;
		} else {
			nreturned = 0;
			buff_ptr = buffer;
		}
	}

	nread = krb_net_read(fd, (char *)&msg_length, sizeof(msg_length));
	if(nread != (int)(sizeof(msg_length)))
		return(0);

	msg_length = ntohl(msg_length);
	pad_length = roundup(msg_length, 8);

	nread = krb_net_read(fd, des_buff, pad_length);
	if(nread != pad_length)
		return(0);

	des_pcbc_encrypt((des_cblock*) des_buff, (des_cblock*) buff_ptr,
		(msg_length < 8 ? 8 : msg_length),
		key_schedule, (des_cblock*) &des_key, DES_DECRYPT);


	if(msg_length < 8)
		buff_ptr += (8 - msg_length);
	stored = msg_length;

	if(stored >= len) {
		bcopy(buff_ptr, buf, len);
		stored -= len;
		buff_ptr += len;
		nreturned += len;
	} else {
		bcopy(buff_ptr, buf, stored);
		nreturned += stored;
		stored = 0;
	}

	return(nreturned);
}


/*
 * Write a message onto a file descriptor (generally a socket), using
 * DES to encrypt the message.
 */
int
des_write(fd, buf, len)
	int fd;
	char * buf;
	int len;
{
	char	garbage[8];
	long	rnd;
	int	pad_len;
	int	write_len;
	int	i;
	char	*data;

	if(len < 8) {
		/*
		 * Right justify the message in 8 bytes of random garbage.
		 */

		for(i = 0 ; i < 8 ; i+= sizeof(long)) {
			rnd = arc4random();
			bcopy(&rnd, garbage+i,
				(i <= (8 - sizeof(long)))?sizeof(long):(8-i));
		}
		bcopy(buf, garbage + 8 - len, len);
		data = garbage;
		pad_len = 8;
	} else {
		data = buf;
		pad_len = roundup(len, 8);
	}

	des_pcbc_encrypt((des_cblock*) data, (des_cblock*) des_buff,
		(len < 8)?8:len, key_schedule, (des_cblock*) &des_key, DES_ENCRYPT);


	write_len = htonl(len);
	if(write(fd, &write_len, sizeof(write_len)) != sizeof(write_len))
		return(-1);
	if(write(fd, des_buff, pad_len) != pad_len)
		return(-1);

	return(len);
}

#endif /* KERBEROS */
#endif /* CRYPT */
