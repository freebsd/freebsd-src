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

#include "gssapi_locl.h"

RCSID("$Id: add_oid_set_member.c,v 1.7 2001/02/18 03:39:08 assar Exp $");

OM_uint32 gss_add_oid_set_member (
            OM_uint32 * minor_status,
            const gss_OID member_oid,
            gss_OID_set * oid_set
           )
{
  gss_OID tmp;
  size_t n;
  OM_uint32 res;
  int present;

  res = gss_test_oid_set_member(minor_status, member_oid, *oid_set, &present);
  if (res != GSS_S_COMPLETE)
    return res;

  if (present)
    return GSS_S_COMPLETE;

  n = (*oid_set)->count + 1;
  tmp = realloc ((*oid_set)->elements, n * sizeof(gss_OID_desc));
  if (tmp == NULL) {
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
  }
  (*oid_set)->elements = tmp;
  (*oid_set)->count = n;
  (*oid_set)->elements[n-1] = *member_oid;
  return GSS_S_COMPLETE;
}
