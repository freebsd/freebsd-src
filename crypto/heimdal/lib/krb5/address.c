/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: address.c,v 1.14 1999/12/02 17:05:07 joda Exp $");

#if 0
/* This is the supposedly MIT-api version */

krb5_boolean
krb5_address_search(krb5_context context,
		    const krb5_address *addr,
		    krb5_address *const *addrlist)
{
  krb5_address *a;

  while((a = *addrlist++))
    if (krb5_address_compare (context, addr, a))
      return TRUE;
  return FALSE;
}
#endif

krb5_boolean
krb5_address_search(krb5_context context,
		    const krb5_address *addr,
		    const krb5_addresses *addrlist)
{
    int i;

    for (i = 0; i < addrlist->len; ++i)
	if (krb5_address_compare (context, addr, &addrlist->val[i]))
	    return TRUE;
    return FALSE;
}

int
krb5_address_order(krb5_context context,
		   const krb5_address *addr1,
		   const krb5_address *addr2)
{
    return (addr1->addr_type - addr2->addr_type)
	|| memcmp (addr1->address.data,
		   addr2->address.data,
		   addr1->address.length);
}

krb5_boolean
krb5_address_compare(krb5_context context,
		     const krb5_address *addr1,
		     const krb5_address *addr2)
{
    return krb5_address_order (context, addr1, addr2) == 0;
}

krb5_error_code
krb5_copy_address(krb5_context context,
		  const krb5_address *inaddr,
		  krb5_address *outaddr)
{
    copy_HostAddress(inaddr, outaddr);
    return 0;
}

krb5_error_code
krb5_copy_addresses(krb5_context context,
		    const krb5_addresses *inaddr,
		    krb5_addresses *outaddr)
{
    copy_HostAddresses(inaddr, outaddr);
    return 0;
}

krb5_error_code
krb5_free_address(krb5_context context,
		  krb5_address *address)
{
    krb5_data_free (&address->address);
    return 0;
}

krb5_error_code
krb5_free_addresses(krb5_context context,
		    krb5_addresses *addresses)
{
    free_HostAddresses(addresses);
    return 0;
}

krb5_error_code
krb5_append_addresses(krb5_context context,
		      krb5_addresses *dest,
		      const krb5_addresses *source)
{
    krb5_address *tmp;
    krb5_error_code ret;
    int i;
    if(source->len > 0) {
	tmp = realloc(dest->val, (dest->len + source->len) * sizeof(*tmp));
	if(tmp == NULL)
	    return ENOMEM;
	dest->val = tmp;
	for(i = 0; i < source->len; i++) {
	    /* skip duplicates */
	    if(krb5_address_search(context, &source->val[i], dest))
		continue;
	    ret = krb5_copy_address(context, 
				    &source->val[i], 
				    &dest->val[dest->len]);
	    if(ret)
		return ret;
	    dest->len++;
	}
    }
    return 0;
}

/*
 * Create an address of type KRB5_ADDRESS_ADDRPORT from (addr, port)
 */

krb5_error_code
krb5_make_addrport (krb5_address **res, const krb5_address *addr, int16_t port)
{
    krb5_error_code ret;
    size_t len = addr->address.length + 2 + 4 * 4;
    u_char *p;

    *res = malloc (sizeof(**res));
    if (*res == NULL)
	return ENOMEM;
    (*res)->addr_type = KRB5_ADDRESS_ADDRPORT;
    ret = krb5_data_alloc (&(*res)->address, len);
    if (ret) {
	free (*res);
	return ret;
    }
    p = (*res)->address.data;
    *p++ = 0;
    *p++ = 0;
    *p++ = (addr->addr_type     ) & 0xFF;
    *p++ = (addr->addr_type >> 8) & 0xFF;

    *p++ = (addr->address.length      ) & 0xFF;
    *p++ = (addr->address.length >>  8) & 0xFF;
    *p++ = (addr->address.length >> 16) & 0xFF;
    *p++ = (addr->address.length >> 24) & 0xFF;

    memcpy (p, addr->address.data, addr->address.length);
    p += addr->address.length;

    *p++ = 0;
    *p++ = 0;
    *p++ = (KRB5_ADDRESS_IPPORT     ) & 0xFF;
    *p++ = (KRB5_ADDRESS_IPPORT >> 8) & 0xFF;

    *p++ = (2      ) & 0xFF;
    *p++ = (2 >>  8) & 0xFF;
    *p++ = (2 >> 16) & 0xFF;
    *p++ = (2 >> 24) & 0xFF;

    memcpy (p, &port, 2);
    p += 2;

    return 0;
}
