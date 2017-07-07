/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2011 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "gssapiP_krb5.h"

OM_uint32
kg_value_from_cred_store(gss_const_key_value_set_t cred_store,
                         const char *type, const char **value)
{
    OM_uint32 i;

    if (value == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;
    *value = NULL;

    if (cred_store == GSS_C_NO_CRED_STORE)
        return GSS_S_COMPLETE;

    for (i = 0; i < cred_store->count; i++) {
        if (strcmp(cred_store->elements[i].key, type) == 0) {
            if (*value != NULL)
                return GSS_S_DUPLICATE_ELEMENT;
            *value = cred_store->elements[i].value;
        }
    }

    return GSS_S_COMPLETE;
}
