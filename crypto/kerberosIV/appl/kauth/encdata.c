/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

#include "kauth.h"

RCSID("$Id: encdata.c,v 1.10 1999/12/02 16:58:31 joda Exp $");

int
write_encrypted (int fd, void *buf, size_t len, des_key_schedule schedule,
		 des_cblock *session, struct sockaddr_in *me,
		 struct sockaddr_in *him)
{
     void *outbuf;
     int32_t outlen, l;
     int i;
     unsigned char tmp[4];

     outbuf = malloc(len + 30);
     if (outbuf == NULL)
	  return -1;
     outlen = krb_mk_priv (buf, outbuf, len, schedule, session, me, him);
     if (outlen < 0) {
	  free(outbuf);
	  return -1;
     }
     l = outlen;
     for(i = 3; i >= 0; i--, l = l >> 8)
	 tmp[i] = l & 0xff;
     if (krb_net_write (fd, tmp, 4) != 4 ||
	 krb_net_write (fd, outbuf, outlen) != outlen) {
	  free(outbuf);
	  return -1;
     }
     
     free(outbuf);
     return 0;
}


int
read_encrypted (int fd, void *buf, size_t len, void **ret,
		des_key_schedule schedule, des_cblock *session,
		struct sockaddr_in *him, struct sockaddr_in *me)
{
     int status;
     int32_t l;
     MSG_DAT msg;
     unsigned char tmp[4];

     l = krb_net_read (fd, tmp, 4);
     if (l != 4)
	 return l;
     l = (tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3];
     if (l > len)
	  return -1;
     if (krb_net_read (fd, buf, l) != l)
	  return -1;
     status = krb_rd_priv (buf, l, schedule, session, him, me, &msg);
     if (status != RD_AP_OK) {
	  fprintf (stderr, "read_encrypted: %s\n",
		   krb_get_err_text(status));
	  return -1;
     }
     *ret  = msg.app_data;
     return  msg.app_length;
}
