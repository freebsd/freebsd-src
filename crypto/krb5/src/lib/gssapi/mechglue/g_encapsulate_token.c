/*
 * Copyright (c) 2011, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mglueP.h"

OM_uint32 KRB5_CALLCONV
gss_encapsulate_token(gss_const_buffer_t input_token,
                      gss_const_OID token_oid,
                      gss_buffer_t output_token)
{
    unsigned int tokenSize;
    struct k5buf buf;

    if (input_token == GSS_C_NO_BUFFER || token_oid == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (output_token == GSS_C_NO_BUFFER)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    tokenSize = g_token_size(token_oid, input_token->length);

    assert(tokenSize > 2);
    tokenSize -= 2; /* TOK_ID */

    output_token->value = gssalloc_malloc(tokenSize);
    if (output_token->value == NULL)
        return GSS_S_FAILURE;

    k5_buf_init_fixed(&buf, output_token->value, tokenSize);
    g_make_token_header(&buf, token_oid, input_token->length, -1);
    k5_buf_add_len(&buf, input_token->value, input_token->length);
    assert(buf.len == tokenSize);
    output_token->length = tokenSize;

    return GSS_S_COMPLETE;
}
