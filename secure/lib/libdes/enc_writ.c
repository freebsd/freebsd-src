/* lib/des/enc_writ.c */
/* Copyright (C) 1995 Eric Young (eay@mincom.oz.au)
 * All rights reserved.
 * 
 * This file is part of an SSL implementation written
 * by Eric Young (eay@mincom.oz.au).
 * The implementation was written so as to conform with Netscapes SSL
 * specification.  This library and applications are
 * FREE FOR COMMERCIAL AND NON-COMMERCIAL USE
 * as long as the following conditions are aheared to.
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.  If this code is used in a product,
 * Eric Young should be given attribution as the author of the parts used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Eric Young (eay@mincom.oz.au)
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <errno.h>
#include <time.h>
#include "des_locl.h"

int des_enc_write(fd, buf, len, sched, iv)
int fd;
char *buf;
int len;
des_key_schedule sched;
des_cblock (*iv);
	{
#ifdef _LIBC
	extern int srandom();
	extern unsigned long time();
	extern int random();
	extern int write();
#endif

	long rnum;
	int i,j,k,outnum;
	char outbuf[BSIZE+HDRSIZE];
	char shortbuf[8];
	char *p;
	static int start=1;

	/* If we are sending less than 8 bytes, the same char will look
	 * the same if we don't pad it out with random bytes */
	if (start)
		{
		start=0;
		srandom((unsigned int)time(NULL));
		}

	/* lets recurse if we want to send the data in small chunks */
	if (len > MAXWRITE)
		{
		j=0;
		for (i=0; i<len; i+=k)
			{
			k=des_enc_write(fd,&(buf[i]),
				((len-i) > MAXWRITE)?MAXWRITE:(len-i),sched,iv);
			if (k < 0)
				return(k);
			else
				j+=k;
			}
		return(j);
		}

	/* write length first */
	p=outbuf;
	l2n(len,p);

	/* pad short strings */
	if (len < 8)
		{
		p=shortbuf;
		memcpy(shortbuf,buf,(unsigned int)len);
		for (i=len; i<8; i++)
			shortbuf[i]=random();
		rnum=8;
		}
	else
		{
		p=buf;
		rnum=((len+7)/8*8); /* round up to nearest eight */
		}

	if (des_rw_mode & DES_PCBC_MODE)
		pcbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
			(long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 
	else
		cbc_encrypt((des_cblock *)p,(des_cblock *)&(outbuf[HDRSIZE]),
			(long)((len<8)?8:len),sched,iv,DES_ENCRYPT); 

	/* output */
	outnum=rnum+HDRSIZE;

	for (j=0; j<outnum; j+=i)
		{
		/* eay 26/08/92 I was not doing writing from where we
		 * got upto. */
		i=write(fd,&(outbuf[j]),(unsigned int)(outnum-j));
		if (i == -1)
			{
			if (errno == EINTR)
				i=0;
			else 	/* This is really a bad error - very bad
				 * It will stuff-up both ends. */
				return(-1);
			}
		}

	return(len);
	}
