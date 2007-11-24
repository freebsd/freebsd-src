/*
 * Copyright (c) 1997, 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: inquire_context.c,v 1.5 2003/03/16 17:43:30 lha Exp $");

OM_uint32 gss_inquire_context (
            OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_name_t * src_name,
            gss_name_t * targ_name,
            OM_uint32 * lifetime_rec,
            gss_OID * mech_type,
            OM_uint32 * ctx_flags,
            int * locally_initiated,
            int * open_context
           )
{
  OM_uint32 ret;

  if (src_name) {
    ret = gss_duplicate_name (minor_status,
			      context_handle->source,
			      src_name);
    if (ret)
      return ret;
  }

  if (targ_name) {
    ret = gss_duplicate_name (minor_status,
			      context_handle->target,
			      targ_name);
    if (ret)
      return ret;
  }

  if (lifetime_rec)
    *lifetime_rec = context_handle->lifetime;

  if (mech_type)
    *mech_type = GSS_KRB5_MECHANISM;

  if (ctx_flags)
    *ctx_flags = context_handle->flags;

  if (locally_initiated)
    *locally_initiated = context_handle->more_flags & LOCAL;

  if (open_context)
    *open_context = context_handle->more_flags & OPEN;

  *minor_status = 0;
  return GSS_S_COMPLETE;
}
