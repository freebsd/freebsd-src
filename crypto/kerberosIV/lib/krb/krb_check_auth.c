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

#include "krb_locl.h"

RCSID("$Id: krb_check_auth.c,v 1.5 1999/12/02 16:58:42 joda Exp $");

/*
 *
 * Receive an mutual-authenticator for a server in `packet', with
 * `checksum', `session', and `schedule' having the appropriate values
 * and return the data in `msg_data'.
 *
 * Return KSUCCESS if the received checksum is correct.
 *
 */

int
krb_check_auth(KTEXT packet,
	       u_int32_t checksum,
	       MSG_DAT *msg_data,
	       des_cblock *session,
	       struct des_ks_struct *schedule,
	       struct sockaddr_in *laddr,
	       struct sockaddr_in *faddr)
{
  int ret;
  u_int32_t checksum2;

  ret = krb_rd_priv (packet->dat, packet->length, schedule, session, faddr,
		     laddr, msg_data);
  if (ret != RD_AP_OK)
    return ret;
  if (msg_data->app_length != 4)
    return KFAILURE;
  krb_get_int (msg_data->app_data, &checksum2, 4, 0);
  if (checksum2 == checksum + 1)
    return KSUCCESS;
  else
    return KFAILURE;
}
