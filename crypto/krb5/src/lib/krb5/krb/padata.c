/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/padata.c - utility functions for krb5_pa_data lists */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"

krb5_pa_data *
krb5int_find_pa_data(krb5_context context, krb5_pa_data *const *pa_list,
                     krb5_preauthtype pa_type)
{
    krb5_pa_data *const *pa;

    for (pa = pa_list; pa != NULL && *pa != NULL; pa++) {
        if ((*pa)->pa_type == pa_type)
            return *pa;
    }
    return NULL;
}

krb5_error_code
k5_alloc_pa_data(krb5_preauthtype pa_type, size_t len, krb5_pa_data **out)
{
    krb5_pa_data *pa;
    uint8_t *buf = NULL;

    *out = NULL;
    if (len > 0) {
        buf = malloc(len);
        if (buf == NULL)
            return ENOMEM;
    }
    pa = malloc(sizeof(*pa));
    if (pa == NULL) {
        free(buf);
        return ENOMEM;
    }
    pa->magic = KV5M_PA_DATA;
    pa->pa_type = pa_type;
    pa->length = len;
    pa->contents = buf;
    *out = pa;
    return 0;
}

void
k5_free_pa_data_element(krb5_pa_data *pa)
{
    if (pa != NULL) {
        free(pa->contents);
        free(pa);
    }
}

krb5_error_code
k5_add_pa_data_element(krb5_pa_data ***list, krb5_pa_data **pa)
{
    size_t count;
    krb5_pa_data **newlist;

    for (count = 0; *list != NULL && (*list)[count] != NULL; count++);

    newlist = realloc(*list, (count + 2) * sizeof(*newlist));
    if (newlist == NULL)
        return ENOMEM;
    newlist[count] = *pa;
    newlist[count + 1] = NULL;
    *pa = NULL;
    *list = newlist;
    return 0;
}

krb5_error_code
k5_add_pa_data_from_data(krb5_pa_data ***list, krb5_preauthtype pa_type,
                         krb5_data *data)
{
    krb5_error_code ret;
    krb5_pa_data *pa;

    ret = k5_alloc_pa_data(pa_type, 0, &pa);
    if (ret)
        return ret;
    pa->contents = (uint8_t *)data->data;
    pa->length = data->length;
    ret = k5_add_pa_data_element(list, &pa);
    if (ret) {
        free(pa);
        return ret;
    }
    *data = empty_data();
    return 0;
}

krb5_error_code
k5_add_empty_pa_data(krb5_pa_data ***list, krb5_preauthtype pa_type)
{
    krb5_data empty = empty_data();

    return k5_add_pa_data_from_data(list, pa_type, &empty);
}
