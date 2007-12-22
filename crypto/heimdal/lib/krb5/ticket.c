/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

RCSID("$Id: ticket.c,v 1.5.8.1 2003/09/18 21:01:57 lha Exp $");

krb5_error_code
krb5_free_ticket(krb5_context context,
		 krb5_ticket *ticket)
{
    free_EncTicketPart(&ticket->ticket);
    krb5_free_principal(context, ticket->client);
    krb5_free_principal(context, ticket->server);
    return 0;
}

krb5_error_code
krb5_copy_ticket(krb5_context context,
		 const krb5_ticket *from,
		 krb5_ticket **to)
{
    krb5_error_code ret;
    krb5_ticket *tmp;

    *to = NULL;
    tmp = malloc(sizeof(*tmp));
    if(tmp == NULL) {
	krb5_set_error_string (context, "malloc: out of memory");
	return ENOMEM;
    }
    if((ret = copy_EncTicketPart(&from->ticket, &tmp->ticket))){
	free(tmp);
	return ret;
    }
    ret = krb5_copy_principal(context, from->client, &tmp->client);
    if(ret){
	free_EncTicketPart(&tmp->ticket);
	free(tmp);
	return ret;
    }
    ret = krb5_copy_principal(context, from->server, &tmp->server);
    if(ret){
	krb5_free_principal(context, tmp->client);
	free_EncTicketPart(&tmp->ticket);
	free(tmp);
	return ret;
    }
    *to = tmp;
    return 0;
}
