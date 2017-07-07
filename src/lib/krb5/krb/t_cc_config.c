/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* t_cc_config.c: read and write configuration items in ccaches */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

/*
 * A test helper which either reads or writes a configuration setting from or
 * to a credential cache, optionally for a specified server principal name.
 */

#include <k5-int.h>
#include "int-proto.h"

static void
bail_on_err(krb5_context context, const char *msg, krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(context, code);
        printf("%s: %s\n", msg, errmsg);
        krb5_free_error_message(context, errmsg);
        exit(1);
    }
}

/*
 * The default unset code path depends on the underlying ccache implementation
 * knowing how to remove a credential, which most types don't actually support,
 * so we have to jump through some hoops to ensure that when we set a value for
 * a key, it'll be the only value for that key that'll be found later.  The
 * ccache portions of libkrb5 will currently duplicate some of the actual
 * tickets.
 */
static void
unset_config(krb5_context context, krb5_ccache ccache,
             krb5_principal server, const char *key)
{
    krb5_ccache tmp1, tmp2;
    krb5_cc_cursor cursor;
    krb5_creds mcreds, creds;

    memset(&mcreds, 0, sizeof(mcreds));
    memset(&creds, 0, sizeof(creds));
    bail_on_err(context, "Error while deriving configuration principal names",
                k5_build_conf_principals(context, ccache, server, key,
                                         &mcreds));
    bail_on_err(context, "Error resolving first in-memory ccache",
                krb5_cc_resolve(context, "MEMORY:tmp1", &tmp1));
    bail_on_err(context, "Error initializing first in-memory ccache",
                krb5_cc_initialize(context, tmp1, mcreds.client));
    bail_on_err(context, "Error resolving second in-memory ccache",
                krb5_cc_resolve(context, "MEMORY:tmp2", &tmp2));
    bail_on_err(context, "Error initializing second in-memory ccache",
                krb5_cc_initialize(context, tmp2, mcreds.client));
    bail_on_err(context, "Error copying credentials to first in-memory ccache",
                krb5_cc_copy_creds(context, ccache, tmp1));
    bail_on_err(context, "Error starting traversal of first in-memory ccache",
                krb5_cc_start_seq_get(context, tmp1, &cursor));
    while (krb5_cc_next_cred(context, tmp1, &cursor, &creds) == 0) {
        if (!krb5_is_config_principal(context, creds.server) ||
            !krb5_principal_compare(context, mcreds.server, creds.server) ||
            !krb5_principal_compare(context, mcreds.client, creds.client)) {
            bail_on_err(context,
                        "Error storing non-config item to in-memory ccache",
                        krb5_cc_store_cred(context, tmp2, &creds));
        }
        krb5_free_cred_contents(context, &creds);
    }
    bail_on_err(context, "Error ending traversal of first in-memory ccache",
                krb5_cc_end_seq_get(context, tmp1, &cursor));
    bail_on_err(context, "Error clearing ccache",
                krb5_cc_initialize(context, ccache, mcreds.client));
    bail_on_err(context, "Error storing creds to the ccache",
                krb5_cc_copy_creds(context, tmp2, ccache));
    bail_on_err(context, "Error cleaning up first in-memory ccache",
                krb5_cc_destroy(context, tmp1));
    bail_on_err(context, "Error cleaning up second in-memory ccache",
                krb5_cc_destroy(context, tmp2));
    krb5_free_principal(context, mcreds.client);
    krb5_free_principal(context, mcreds.server);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_principal server;
    krb5_ccache ccache;
    krb5_data data;
    krb5_error_code ret;
    char *perr;
    int c;
    unsigned int i;

    bail_on_err(NULL, "Error initializing Kerberos library",
                krb5_init_context(&context));
    bail_on_err(context, "Error getting location of default ccache",
                krb5_cc_default(context, &ccache));
    server = NULL;
    while ((c = getopt(argc, argv, "p:")) != -1) {
        switch (c) {
        case 'p':
            if (asprintf(&perr, "Error parsing principal name \"%s\"",
                         optarg) < 0)
                abort();
            bail_on_err(context, perr,
                        krb5_parse_name(context, optarg, &server));
            free(perr);
            break;
        }
    }
    if (argc - optind < 1 || argc - optind > 2) {
        fprintf(stderr, "Usage: %s [-p principal] key [value]\n", argv[0]);
        return 1;
    }
    memset(&data, 0, sizeof(data));
    if (argc - optind == 2) {
        unset_config(context, ccache, server, argv[optind]);
        data = string2data(argv[optind + 1]);
        bail_on_err(context, "Error adding configuration data to ccache",
                    krb5_cc_set_config(context, ccache, server, argv[optind],
                                       &data));
    } else {
        ret = krb5_cc_get_config(context, ccache, server, argv[optind], &data);
        if (ret == 0) {
            for (i = 0; i < data.length; i++)
                putc((unsigned int)data.data[i], stdout);
            krb5_free_data_contents(context, &data);
        }
    }
    krb5_free_principal(context, server);
    krb5_cc_close(context, ccache);
    krb5_free_context(context);
    return 0;
}
