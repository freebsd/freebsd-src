/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb_locl.h"

RCSID("$Id: str2key.c,v 1.17 1999/12/02 16:58:44 joda Exp $");

#define lowcase(c) (('A' <= (c) && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))

/*
 * The string to key function used by Transarc AFS.
 */
void
afs_string_to_key(const char *pass, const char *cell, des_cblock *key)
{
  if (strlen(pass) <= 8)	/* Short passwords. */
    {
      char buf[8 + 1], *s;
      int i;

      /*
       * XOR cell and password and pad (or fill) with 'X' to length 8,
       * then use crypt(3) to create DES key.
       */
      for (i = 0; i < 8; i++)
	{
	  buf[i] = *pass ^ lowcase(*cell);
	  if (buf[i] == 0)
	    buf[i] = 'X';
	  if (*pass != 0)
	    pass++;
	  if (*cell != 0)
	    cell++;
	}
      buf[8] = 0;

      s = crypt(buf, "p1");	/* Result from crypt is 7bit chars. */
      s = s + 2;		/* Skip 2 chars of salt. */
      for (i = 0; i < 8; i++)
	((char *) key)[i] = s[i] << 1; /* High bit is always zero */
      des_fixup_key_parity(key); /*       Low  bit is parity */
    }
  else				/* Long passwords */
    {
      int plen, clen;
      char *buf, *t;
      des_key_schedule sched;
      des_cblock ivec;

      /*
       * Concatenate password with cell name,
       * then checksum twice to create DES key.
       */
      plen = strlen(pass);
      clen = strlen(cell);
      buf = malloc(plen + clen + 1);
      memcpy(buf, pass, plen);
      for (t = buf + plen; *cell != 0; t++, cell++)
	*t = lowcase(*cell);

      memcpy(&ivec, "kerberos", 8);
      memcpy(key, "kdsbdsns", 8);
      des_key_sched(key, sched);
      /* Beware, ivec is passed twice */
      des_cbc_cksum((des_cblock *)buf, &ivec, plen + clen, sched, &ivec);

      memcpy(key, &ivec, 8);
      des_fixup_key_parity(key);
      des_key_sched(key, sched);
      /* Beware, ivec is passed twice */
      des_cbc_cksum((des_cblock *)buf, key, plen + clen, sched, &ivec);
      free(buf);
      des_fixup_key_parity(key);
    }
}
