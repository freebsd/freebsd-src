#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>

#include "k5-platform.h"	/* pull in asprintf decl/defn */
#include "test_ccapi_util.h"


// ---------------------------------------------------------------------------

cc_int32 destroy_all_ccaches(cc_context_t context) {
	cc_int32 err = ccNoError;
	cc_ccache_t ccache = NULL;

	while (!err) {
		err = cc_context_open_default_ccache(context, &ccache);
		if (!err) {
			err = cc_ccache_destroy(ccache);
		}
	}
	if (err == ccErrCCacheNotFound) {
		err = ccNoError;
	}
	else {
		log_error("cc_context_open_default_ccache or cc_ccache_destroy failed with %s (%d)", translate_ccapi_error(err), err);
	}

	return err;
}


// ---------------------------------------------------------------------------

cc_int32 new_v5_creds_union (cc_credentials_union *out_union, const char *realm)
{
    cc_int32 err = ccNoError;
    cc_credentials_union *cred_union = NULL;
	cc_credentials_v5_t *v5creds = NULL;
    static int num_runs = 1;
	char *client = NULL;
	char *server = NULL;

    if (!out_union) { err = ccErrBadParam; }

	if (!err) {
		v5creds = malloc (sizeof (*v5creds));
		if (!v5creds) {
			err = ccErrNoMem;
		}
	}

	if (!err) {
		asprintf(&client, "client@%s", realm);
		asprintf(&server, "host/%d%s@%s", num_runs++, realm, realm);
		if (!client || !server) {
			err = ccErrNoMem;
		}
	}

	if (!err) {
		v5creds->client = client;
		v5creds->server = server;
		v5creds->keyblock.type = 1;
		v5creds->keyblock.length = 0;
		v5creds->keyblock.data = NULL;
		v5creds->authtime = time (NULL);
		v5creds->starttime = time (NULL);
		v5creds->endtime = time(NULL) + 1000;
		v5creds->renew_till = time(NULL) + 10000;
		v5creds->is_skey = 0;
		v5creds->ticket_flags = TKT_FLG_FORWARDABLE | TKT_FLG_PROXIABLE | TKT_FLG_RENEWABLE | TKT_FLG_INITIAL;
		v5creds->addresses = NULL;
		v5creds->ticket.type = 0;
		v5creds->ticket.length = 0;
		v5creds->ticket.data = NULL;
		v5creds->second_ticket.type = 0;
		v5creds->second_ticket.length = 0;
		v5creds->second_ticket.data = NULL;
		v5creds->authdata = NULL;
	}


    if (!err) {
        cred_union = malloc (sizeof (*cred_union));
        if (cred_union) {
			cred_union->version = cc_credentials_v5;
			cred_union->credentials.credentials_v5 = v5creds;
        } else {
            err = ccErrNoMem;
        }
    }
	if (!err) {
		*out_union = *cred_union;
		cred_union = NULL;
	}

    return err;
}


// ---------------------------------------------------------------------------

void release_v5_creds_union(cc_credentials_union *creds_union) {
	cc_credentials_v5_t *v5creds = NULL;

	if (creds_union) {
		if (creds_union->credentials.credentials_v5) {
			v5creds = creds_union->credentials.credentials_v5;
			if (v5creds->client) { free(v5creds->client); }
			if (v5creds->server) { free(v5creds->server); }
			if (v5creds->keyblock.data) { free(v5creds->keyblock.data); }
			if (v5creds->ticket.data) { free(v5creds->ticket.data); }
			if (v5creds->second_ticket.data) { free(v5creds->second_ticket.data); }
			free(v5creds);
		}
		//free(creds_union);
	}
}


// ---------------------------------------------------------------------------

// return zero when both unions are considered equal, non-zero when not

int compare_v5_creds_unions(const cc_credentials_union *a, const cc_credentials_union *b) {
	int retval = -1;

	if (a &&
		b &&
		(a->version == cc_credentials_v5) &&
		(a->version == b->version) &&
		(strcmp(a->credentials.credentials_v5->client, b->credentials.credentials_v5->client) == 0) &&
		(strcmp(a->credentials.credentials_v5->server, b->credentials.credentials_v5->server) == 0))
	{
		retval = 0;
	}

	return retval;
}
