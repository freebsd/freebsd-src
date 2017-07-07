#include <string.h>
#include "k5-platform.h"	/* pull in asprintf decl/defn */
#include "test_ccapi_v2.h"
#include <limits.h>
#include <time.h>
#include "test_ccapi_check.h"
#include "test_ccapi_util.h"

// ---------------------------------------------------------------------------

static cc_result destroy_all_ccaches_v2(apiCB *context) {
    cc_result err = CC_NOERROR;
    infoNC **info = NULL;
    int i = 0;

    err = cc_get_NC_info(context, &info);

    for (i = 0; !err && info[i]; i++) {
        ccache_p *ccache = NULL;

        err = cc_open(context, info[i]->name, info[i]->vers, 0, &ccache);

        if (!err) { cc_destroy(context, &ccache); }
    }

    if (info) { cc_free_NC_info(context, &info); }

    if (err) {
        log_error("cc_get_NC_info or cc_open failed with %s (%d)", translate_ccapi_error(err), err);
    }

    return err;
}

// ---------------------------------------------------------------------------
// return zero when both unions are considered equal, non-zero when not

static int compare_v5_creds_unions_compat(const cred_union *a, const cred_union *b) {
    int retval = -1;

    if (a && b && a->cred_type == b->cred_type) {
        if (a->cred_type == CC_CRED_V5) {
            if (!strcmp(a->cred.pV5Cred->client, b->cred.pV5Cred->client) &&
                !strcmp(a->cred.pV5Cred->server, b->cred.pV5Cred->server) &&
                a->cred.pV5Cred->starttime == b->cred.pV5Cred->starttime) {
                retval = 0;
            }
        } else if (a->cred_type == CC_CRED_V4) {
            if (!strcmp (a->cred.pV4Cred->principal,
                         b->cred.pV4Cred->principal) &&
                !strcmp (a->cred.pV4Cred->principal_instance,
                         b->cred.pV4Cred->principal_instance) &&
                !strcmp (a->cred.pV4Cred->service,
                         b->cred.pV4Cred->service) &&
                !strcmp (a->cred.pV4Cred->service_instance,
                         b->cred.pV4Cred->service_instance) &&
                !strcmp (a->cred.pV4Cred->realm,
                         b->cred.pV4Cred->realm) &&
                a->cred.pV4Cred->issue_date == b->cred.pV4Cred->issue_date) {
                retval = 0;
            }
        }
    }

    return retval;
}

// ---------------------------------------------------------------------------

static cc_result new_v5_creds_union_compat (cred_union *out_union, const char *realm)
{
    cc_result err = CC_NOERROR;
    cred_union *creds_union = NULL;
    cc_credentials_v5_compat *v5creds = NULL;
    static int num_runs = 1;
    char *client = NULL;
    char *server = NULL;

    if (!out_union) { err = CC_BAD_PARM; }

    if (!err) {
        v5creds = malloc (sizeof (*v5creds));
        if (!v5creds) {
            err = CC_NOMEM;
        }
    }

    if (!err) {
        asprintf(&client, "client@%s", realm);
        asprintf(&server, "host/%d%s@%s", num_runs++, realm, realm);
        if (!client || !server) {
            err = CC_NOMEM;
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
        creds_union = malloc (sizeof (*creds_union));
        if (creds_union) {
            creds_union->cred_type = CC_CRED_V5;
            creds_union->cred.pV5Cred = v5creds;
        } else {
            err = CC_NOMEM;
        }
    }
    if (!err) {
        *out_union = *creds_union;
        creds_union = NULL;
    }

    return err;
}

// ---------------------------------------------------------------------------

static void release_v5_creds_union_compat(cred_union *creds_union) {
    cc_credentials_v5_compat *v5creds = NULL;

    if (creds_union) {
        if (creds_union->cred.pV5Cred) {
            v5creds = creds_union->cred.pV5Cred;
            if (v5creds->client) { free(v5creds->client); }
            if (v5creds->server) { free(v5creds->server); }
            if (v5creds->keyblock.data) { free(v5creds->keyblock.data); }
            if (v5creds->ticket.data) { free(v5creds->ticket.data); }
            if (v5creds->second_ticket.data) { free(v5creds->second_ticket.data); }
            free(v5creds);
        }
    }
}

// ---------------------------------------------------------------------------

int check_cc_shutdown(void) {
    cc_result err = 0;
    apiCB *context = NULL;

    BEGIN_TEST("cc_shutdown");

    // try with valid context
    err = check_once_cc_shutdown(&context, CC_NOERROR, NULL);

    // try with NULL
    err = check_once_cc_shutdown(NULL, CC_BAD_PARM, NULL);

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

cc_result check_once_cc_shutdown(apiCB **out_context, cc_result expected_err, const char *description) {
    cc_result err = 0;
    apiCB *context = NULL;

    cc_result possible_return_values[2] = {
        CC_NOERROR,
        CC_BAD_PARM,
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    if (out_context) {
        err = cc_initialize(out_context, ccapi_version_2, NULL, NULL);
        if (!err) {
            context = *out_context;
        } else {
            log_error("failure in cc_initialize, unable to perform check");
            return err;
        }
    }

    if (!err) {
        err = cc_shutdown(&context);
        // check returned error
        check_err(err, expected_err, possible_return_values);

    }

    if (out_context) {
        *out_context = NULL;
    }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_get_change_time(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    cc_time_t last_change_time = 0;
    ccache_p *ccache = NULL;
    cred_union creds_union;

    BEGIN_TEST("cc_get_change_time");

    /*
     * Make a context
     * make sure the change time changes after:
     * 	a ccache is created
     * 	a ccache is destroyed
     * 	a credential is stored
     * 	a credential is removed
     * 	a ccache principal is changed
     * clean up memory
     */

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);
    if (!err) {

        // try bad parameters first
        err = check_once_cc_get_change_time(context, NULL, CC_BAD_PARM, "NULL param, should fail");

        // get_change_time should always give something > 0
        check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "first-run, should be > 0");

        // create a ccache
        err = cc_create(context, "TEST_CCACHE", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
        if (err) {
            log_error("failed to create a ccache - %s (%d)", translate_ccapi_error(err), err);
            failure_count++;
        }
        check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "after creating a new ccache");

        if (!err) {
            // change principal
            err = cc_set_principal(context, ccache, CC_CRED_V5, "foo@BAR.ORG");
            if (err) {
                log_error("failed to change ccache's principal - %s (%d)", translate_ccapi_error(err), err);
                failure_count++;
                err = CC_NOERROR;
            }
        }
        check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "after changing a principle");

        new_v5_creds_union_compat(&creds_union, "BAR.ORG");

        // store a credential
        if (!err) {
            err = cc_store(context, ccache, creds_union);
            if (err) {
                log_error("failed to store a credential - %s (%d)", translate_ccapi_error(err), err);
                failure_count++;
                err = CC_NOERROR;
            }
        }
        check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "after storing a credential");

        // remove a credential
        if (!err) {
            err = cc_remove_cred(context, ccache, creds_union);
            if (err) {
                log_error("failed to remove a credential - %s (%d)", translate_ccapi_error(err), err);
                failure_count++;
                err = CC_NOERROR;
            }
        }
        check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "after removing a credential");

        release_v5_creds_union_compat(&creds_union);

        if (ccache) {
            // destroy a ccache
            err = cc_destroy(context, &ccache);
            check_once_cc_get_change_time(context, &last_change_time, CC_NOERROR, "after destroying a ccache");
        }
    }

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_int32 check_once_cc_get_change_time(apiCB *context, cc_time_t *last_time, cc_result expected_err, const char *description) {
    cc_result err = 0;
    cc_time_t last_change_time;
    cc_time_t current_change_time = 0;

    cc_result possible_return_values[3] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NO_EXIST,
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    if (last_time != NULL) { // if we were passed NULL, then we're looking to pass a bad param
        err = cc_get_change_time(context, &current_change_time);
    } else {
        err = cc_get_change_time(context, NULL);
    }

    check_err(err, expected_err, possible_return_values);

    if (!err) {
        last_change_time = *last_time;
        check_if(current_change_time <= last_change_time, "context change time did not increase when it was supposed to (%d <= %d)", current_change_time, last_change_time);
        *last_time = current_change_time;
    }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_open(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name = "TEST_OPEN_CCACHE";

    BEGIN_TEST("cc_open");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);
    if (!err) {
        // create a ccache
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
        if (err) {
            log_error("failed to create a ccache - %s (%d)", translate_ccapi_error(err), err);
            failure_count++;
        }
        if (!err) {
            err = cc_close(context, &ccache);
            ccache = NULL;
        }

        // try default ccache
        if (!err) {
            err = check_once_cc_open(context, name, CC_CRED_V5, &ccache, CC_NOERROR, NULL);
        }

        // check version
        if (!err) {
            err = check_once_cc_open(context, name, CC_CRED_V4, &ccache, CC_ERR_CRED_VERSION, NULL);
        }
        // try bad parameters
        err = check_once_cc_open(context, NULL, CC_CRED_V5, &ccache, CC_BAD_PARM, NULL);
        err = check_once_cc_open(context, name, CC_CRED_V5, NULL, CC_BAD_PARM, NULL);
        err = check_once_cc_open(context, name, CC_CRED_UNKNOWN, &ccache, CC_ERR_CRED_VERSION, NULL);
     }

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_open(apiCB *context, const char *name, cc_int32 version, ccache_p **ccache, cc_result expected_err, const char *description) {
    cc_result err = 0;
    char *stored_name = NULL;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NO_EXIST,
        CC_NOMEM,
        CC_ERR_CRED_VERSION
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    if (ccache != NULL) { // if we were passed NULL, then we're looking to pass a bad param
        err = cc_open(context, name, version, 0, ccache);
    } else {
        err = cc_open(context, name, version, 0, NULL);
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        check_if(*ccache == NULL, NULL);

        if (!err) {
            err = cc_get_name(context, *ccache, &stored_name);
        }
        if (!err) {
            check_if(strcmp(stored_name, name), NULL);
        }
        if (stored_name) { cc_free_name(context, &stored_name); }


        if (ccache && *ccache) {
            cc_ccache_release(*ccache);
            *ccache = NULL;
        }
    }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_create(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name = "TEST_CC_CREATE";

    BEGIN_TEST("cc_create");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);
    if (!err) {
        if (!err) {
            err = cc_open(context, name, CC_CRED_V5, 0, &ccache);
            if (!err) {
                err = cc_destroy (context, &ccache);
            } else {
                err = CC_NOERROR;  /* ccache does not exist */
            }
        }
        // try making a ccache with a unique name (the now destroyed cache's name)
        if (!err) {
            err = check_once_cc_create(context, name, CC_CRED_V5, "foo@BAR.ORG", &ccache, CC_NOERROR, NULL);
        }

        // try making a ccache with a non-unique name (the existing cache's name)
        if (!err) {
            err = check_once_cc_create(context, name, CC_CRED_V5, "foo/baz@BAR.ORG", &ccache, CC_NOERROR, NULL);
        }

        // try bad parameters
        err = check_once_cc_create(context, NULL, CC_CRED_V5, "foo@BAR.ORG", &ccache, CC_BAD_PARM, "NULL name");                    // NULL name
        err = check_once_cc_create(context, "name", CC_CRED_MAX, "foo@BAR.ORG", &ccache, CC_ERR_CRED_VERSION, "invalid creds_vers"); // invalid creds_vers
        err = check_once_cc_create(context, "name", CC_CRED_V5, NULL, &ccache, CC_BAD_PARM, "NULL principal");                          // NULL principal
        err = check_once_cc_create(context, "name", CC_CRED_V5, "foo@BAR.ORG", NULL, CC_BAD_PARM, "NULL ccache");                    // NULL ccache
    }

    if (ccache) { cc_destroy(context, &ccache); }
    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_create(apiCB  *context, const char *name, cc_int32 cred_vers, const char *principal, ccache_p **ccache, cc_int32 expected_err, const char *description) {
    cc_result err = 0;
    char *stored_name = NULL;
    char *stored_principal = NULL;
    cc_int32 stored_creds_vers = 0;

    cc_result possible_return_values[6] = {
        CC_NOERROR,
        CC_BADNAME,
        CC_BAD_PARM,
        CC_NO_EXIST,
        CC_NOMEM,
        CC_ERR_CRED_VERSION,
    };
    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_create(context, name, principal, cred_vers, 0, ccache);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        check_if(*ccache == NULL, NULL);

        // make sure all of the ccache's info matches what we gave it
        // name
        err = cc_get_name(context, *ccache, &stored_name);
        if (!err) { check_if(strcmp(stored_name, name), NULL); }
        if (stored_name) { cc_free_name(context, &stored_name); }
        // cred_vers
        err = cc_get_cred_version(context, *ccache, &stored_creds_vers);
        if (!err) { check_if(stored_creds_vers != cred_vers, NULL); }
        // principal
        err = cc_get_principal(context, *ccache, &stored_principal);
        if (!err) { check_if(strcmp(stored_principal, principal), NULL); }
        if (stored_principal) { cc_free_principal(context, &stored_principal); }

        if (ccache && *ccache) {
            cc_destroy(context, ccache);
            *ccache = NULL;
        }
    }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_close(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name = "TEST_CC_CLOSE";

    BEGIN_TEST("cc_close");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }

    if (!err) {
        check_once_cc_close(context, ccache, CC_NOERROR, NULL);
        ccache = NULL;
    }

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_close(apiCB *context, ccache_p *ccache, cc_result expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[2] = {
        CC_NOERROR,
        CC_BAD_PARM
    };

    char *name = NULL;

    err = cc_get_name(context, ccache, &name);
    err = cc_close(context, &ccache);
    ccache = NULL;

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err && name) { // try opening released ccache to make sure it still exists
        err = cc_open(context, name, CC_CRED_V5, 0, &ccache);
    }
    check_if(err == CC_NO_EXIST, "released ccache was actually destroyed instead");
    check_if(err != CC_NOERROR, "released ccache cannot be opened");

    if (ccache) { cc_destroy(context, &ccache); }
    if (name) { cc_free_name(context, &name); }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_destroy(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name = "TEST_CC_DESTROY";

    BEGIN_TEST("cc_destroy");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }

    if (!err) {
        check_once_cc_destroy(context, ccache, CC_NOERROR, NULL);
        ccache = NULL;
    }

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_destroy(apiCB *context, ccache_p *ccache, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[2] = {
        CC_NOERROR,
        CC_BAD_PARM,
    };

    char *name = NULL;

    BEGIN_CHECK_ONCE(description);

#ifdef cc_ccache_destroy

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_get_name(context, ccache, &name);
    err = cc_destroy(context, &ccache);
    ccache = NULL;

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err && name) { // try opening released ccache to make sure it still exists
        err = cc_open(context, name, CC_CRED_V5, 0, &ccache);
    }
    check_if(err != CC_NO_EXIST, "destroyed ccache was actually released instead");

    if (ccache) { cc_destroy(context, &ccache); }
    if (name) { cc_free_name(context, &name); }

#endif /* cc_ccache_destroy */

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_get_cred_version(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name = "TEST_CC_GET_CRED_VERSION_V5";

    BEGIN_TEST("cc_get_cred_version");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    // try one created with v5 creds
    if (!err) {
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        check_once_cc_get_cred_version(context, ccache, CC_CRED_V5, CC_NOERROR, "v5 creds");
    }
    else {
        log_error("cc_context_create_new_ccache failed, can't complete test");
        failure_count++;
    }

    if (ccache) {
        cc_destroy(context, &ccache);
        ccache = NULL;
    }

    err = CC_NOERROR;

    // try one created with v4 creds
    if (!err) {
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V4, 0, &ccache);
    }
    if (!err) {
        check_once_cc_get_cred_version(context, ccache, CC_CRED_V4, CC_NOERROR, "v4 creds");
    }
    else {
        log_error("cc_context_create_new_ccache failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_destroy(context, &ccache);
        ccache = NULL;
    }

    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_get_cred_version(apiCB *context, ccache_p *ccache, cc_int32 expected_cred_vers, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[3] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NO_EXIST,
    };

    cc_int32 stored_cred_vers = 0;

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_get_cred_version(context, ccache, &stored_cred_vers);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        check_if(stored_cred_vers != expected_cred_vers, NULL);
    }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_get_name(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;

    BEGIN_TEST("cc_get_name");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // try with unique ccache (which happens to be default)
    if (!err) {
        err = cc_create(context, "0", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        check_once_cc_get_name(context, ccache, "0", CC_NOERROR, "unique ccache (which happens to be default)");
    }
    else {
        log_error("cc_context_create_ccache failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    // try with unique ccache (which is not default)
    if (!err) {
        err = cc_context_create_ccache(context, "1", CC_CRED_V5, "foo@BAR.ORG", &ccache);
    }
    if (!err) {
        check_once_cc_get_name(context, ccache, "1", CC_NOERROR, "unique ccache (which is not default)");
    }
    else {
        log_error("cc_context_create_ccache failed, can't complete test");
        failure_count++;
    }

    // try with bad param
    if (!err) {
        check_once_cc_get_name(context, ccache, NULL, CC_BAD_PARM, "NULL param");
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    if (context) {
        err = destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_int32 check_once_cc_get_name(apiCB *context, ccache_p *ccache, const char *expected_name, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[4] = {
        CC_NOERROR,
        CC_NOMEM,
        CC_BAD_PARM,
        CC_NO_EXIST,
    };

    char *stored_name = NULL;

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    if (expected_name == NULL) { // we want to try with a NULL param
        err = cc_get_name(context, ccache, NULL);
    }
    else {
        err = cc_get_name(context, ccache, &stored_name);
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        check_if(strcmp(stored_name, expected_name), NULL);
    }

    if (stored_name) { cc_free_name(context, &stored_name); }

    END_CHECK_ONCE;

    return err;
}


// ---------------------------------------------------------------------------

int check_cc_get_principal(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name_v5 = "TEST_CC_GET_PRINCIPAL_V5";
    char *name_v4 = "TEST_CC_GET_PRINCIPAL_V4";

    BEGIN_TEST("cc_get_principal");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // try with krb5 principal
    if (!err) {
        err = cc_create(context, name_v5, "foo/BAR@BAZ.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        check_once_cc_get_principal(context, ccache, "foo/BAR@BAZ.ORG", CC_NOERROR, "trying to get krb5 princ for krb5 ccache");
    }
    else {
        log_error("cc_create failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    // try with krb4 principal
    if (!err) {
        err = cc_create(context, name_v4, "foo.BAR@BAZ.ORG", CC_CRED_V4, 0, &ccache);
    }
    if (!err) {
        check_once_cc_get_principal(context, ccache, "foo.BAR@BAZ.ORG", CC_NOERROR, "trying to get krb4 princ for krb4 ccache");
    }
    else {
        log_error("cc_create failed, can't complete test");
        failure_count++;
    }

    // try with bad param
    if (!err) {
        check_once_cc_get_principal(context, ccache, NULL, CC_BAD_PARM, "passed null out param");
    }

    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    if (context) {
        err = destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_get_principal(apiCB *context,
                                      ccache_p *ccache,
                                      const char *expected_principal,
                                      cc_int32 expected_err,
                                      const char *description) {
    cc_result err = CC_NOERROR;
    char *stored_principal = NULL;

    cc_result possible_return_values[4] = {
        CC_NOERROR,
        CC_NOMEM,
        CC_NO_EXIST,
        CC_BAD_PARM
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    if (expected_principal == NULL) { // we want to try with a NULL param
        err = cc_get_principal(context, ccache, NULL);
    }
    else {
        err = cc_get_principal(context, ccache, &stored_principal);
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        check_if(strcmp(stored_principal, expected_principal), "expected princ == \"%s\" stored princ == \"%s\"", expected_principal, stored_principal);
    }

    if (stored_principal) { cc_free_principal(context, &stored_principal); }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_set_principal(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    char *name_v5 = "TEST_CC_GET_PRINCIPAL_V5";
    char *name_v4 = "TEST_CC_GET_PRINCIPAL_V4";

    BEGIN_TEST("cc_set_principal");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // bad params
    if (!err) {
        err = cc_create(context, name_v5, "foo@BAZ.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        check_once_cc_set_principal(context, ccache, CC_CRED_MAX, "foo/BAZ@BAR.ORG", CC_ERR_CRED_VERSION, "CC_CRED_MAX (not allowed)");
        check_once_cc_set_principal(context, ccache, CC_CRED_V5, NULL, CC_BAD_PARM, "NULL principal");
    }
    else {
        log_error("cc_create failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_destroy(context, &ccache);
        ccache = NULL;
    }

    // empty ccache

    // replace v5 ccache's principal
    if (!err) {
        err = cc_create(context, name_v5, "foo@BAZ.ORG", CC_CRED_V5, 0, &ccache);
     }
    if (!err) {
        check_once_cc_set_principal(context, ccache, CC_CRED_V5, "foo/BAZ@BAR.ORG", CC_NOERROR, "replace v5 only ccache's principal (empty ccache)");
        check_once_cc_set_principal(context, ccache, CC_CRED_V4, "foo.BAZ@BAR.ORG", CC_ERR_CRED_VERSION, "replace v5 principal with v4");
    }
    else {
        log_error("cc_create failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_destroy(context, &ccache);
        ccache = NULL;
    }

    // replace v4 ccache's principal
    if (!err) {
        err = cc_create(context, name_v4, "foo@BAZ.ORG", CC_CRED_V4, 0, &ccache);
    }
    if (!err) {
        check_once_cc_set_principal(context, ccache, CC_CRED_V4, "foo.BAZ@BAR.ORG", CC_NOERROR, "replace v4 only ccache's principal (empty ccache)");
        check_once_cc_set_principal(context, ccache, CC_CRED_V5, "foo/BAZ@BAR.ORG", CC_ERR_CRED_VERSION, "replace v4 principal with v5");
    }
    else {
        log_error("cc_create failed, can't complete test");
        failure_count++;
    }
    if (ccache) {
        cc_destroy(context, &ccache);
        ccache = NULL;
    }

    if (context) {
        err = destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_int32 check_once_cc_set_principal(apiCB *context, ccache_p *ccache, cc_int32 cred_vers, const char *in_principal, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;
    char *stored_principal = NULL;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_NOMEM,
        CC_NO_EXIST,
        CC_ERR_CRED_VERSION,
        CC_BAD_PARM
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_set_principal(context, ccache, cred_vers, (char *) in_principal);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    if (!err) {
        err = cc_get_principal(context, ccache, &stored_principal);
    }

    // compare stored with input
    if (!err) {
        check_if(strcmp(stored_principal, in_principal), "expected princ == \"%s\" stored princ == \"%s\"", in_principal, stored_principal);
    }

    if (stored_principal) { cc_free_principal(context, &stored_principal); }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_store(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    ccache_p *dup_ccache = NULL;
    cred_union creds_union;
    char *name = NULL;

    BEGIN_TEST("cc_store");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    if (!err) {
        err = cc_create(context, "TEST_CC_STORE", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }

    // cred with matching version and realm
    if (!err) {
        err = new_v5_creds_union_compat(&creds_union, "BAR.ORG");

        if (!err) {
            check_once_cc_store(context, ccache, creds_union, CC_NOERROR, "ok creds");
            release_v5_creds_union_compat(&creds_union);
        }
    }

    // invalid creds
    if (!err) {
        err = new_v5_creds_union_compat(&creds_union, "BAR.ORG");

        if (!err) {
            if (creds_union.cred.pV5Cred->client) {
                free(creds_union.cred.pV5Cred->client);
                creds_union.cred.pV5Cred->client = NULL;
            }
            check_once_cc_store(context, ccache, creds_union, CC_BAD_PARM, "invalid creds (NULL client string)");

            release_v5_creds_union_compat(&creds_union);
        }
    }

    // bad creds version
    if (!err) {
        err = new_v5_creds_union_compat(&creds_union, "BAR.ORG");

        if (!err) {
            creds_union.cred_type = CC_CRED_MAX;
            check_once_cc_store(context, ccache, creds_union, CC_ERR_CRED_VERSION, "CC_CRED_MAX (invalid) into a ccache with only v5 princ");
            creds_union.cred_type = CC_CRED_V4;
            check_once_cc_store(context, ccache, creds_union, CC_ERR_CRED_VERSION, "v4 creds into a v5 ccache");
            creds_union.cred_type = CC_CRED_V5;

            release_v5_creds_union_compat(&creds_union);
        }
    }

    // non-existent ccache
    if (ccache) {
        err = cc_get_name(context, ccache, &name);
        if (!err) {
            err = cc_open(context, name, CC_CRED_V5, 0, &dup_ccache);
        }
        if (name) { cc_free_name(context, &name); }
        if (dup_ccache) { cc_destroy(context, &dup_ccache); }
    }

    if (!err) {
        err = new_v5_creds_union_compat(&creds_union, "BAR.ORG");

        if (!err) {
            check_once_cc_store(context, ccache, creds_union, CC_NO_EXIST, "invalid ccache");

            release_v5_creds_union_compat(&creds_union);
        }
    }

    if (ccache) { cc_close(context, &ccache); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_store(apiCB *context, ccache_p *ccache, const cred_union in_creds, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;
    ccache_cit *iterator = NULL;
    int found = 0;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_ERR_CACHE_FULL,
        CC_ERR_CRED_VERSION,
        CC_NO_EXIST
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_store(context, ccache, in_creds);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    // make sure credentials were truly stored
    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }
    while (!err && !found) {
        cred_union *creds = NULL;

        err = cc_seq_fetch_creds_next(context, &creds, iterator);
        if (!err) {
            found = !compare_v5_creds_unions_compat(&in_creds, creds);
        }

        if (creds) { cc_free_creds(context, &creds); }
    }

    if (err == CC_END) {
        check_if(found, "stored credentials not found in ccache");
        err = CC_NOERROR;
    }

    if (iterator) { cc_seq_fetch_creds_end(context, &iterator); }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_remove_cred(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    cred_union *creds_array[10];
    ccache_cit *iterator = NULL;
    char *name = NULL;
    unsigned int i;

    BEGIN_TEST("cc_remove_cred");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    if (!err) {
        err = cc_create(context, "TEST_CC_REMOVE_CRED", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }

    // store 10 creds and retrieve their cc_credentials_t representations
    for(i = 0; !err && (i < 10); i++) {
        cred_union creds;

        new_v5_creds_union_compat(&creds, "BAR.ORG");
        err = cc_store(context, ccache, creds);
        if (err) {
            log_error("failure to store creds_union in remove_creds test");
        }
        release_v5_creds_union_compat(&creds);
    }

    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }

    for (i = 0; !err && i < 10; i++) {
        creds_array[i] = NULL;
        err = cc_seq_fetch_creds_next(context, &creds_array[i], iterator);
    }
    if (err == CC_END) { err = CC_NOERROR; }

    // remove 10 valid creds
    for (i = 0; !err && (i < 10); i++) {
        check_once_cc_remove_cred(context, ccache, *creds_array[i], CC_NOERROR, "10 ok creds");
    }

    // non-existent creds (remove same one twice)
    check_once_cc_remove_cred(context, ccache, *creds_array[0], CC_NOTFOUND, "removed same creds twice");

    // non-existent ccache
    if (ccache) {
        ccache_p *dup_ccache = NULL;

        err = cc_get_name(context, ccache, &name);

        if (!err) {
            err = cc_open(context, name, CC_CRED_V5, 0, &dup_ccache);
        }

        if (!err) {
            err = cc_destroy(context, &dup_ccache);
            check_once_cc_remove_cred(context, ccache, *creds_array[0], CC_NO_EXIST, "invalid ccache");
        }

        if (name) { cc_free_name(context, &name); }
    }

     for(i = 0; i < 10 && creds_array[i]; i++) {
        cc_free_creds(context, &creds_array[i]);
    }


    if (iterator) { cc_seq_fetch_creds_end(context, &iterator); iterator = NULL; }
    if (ccache) { cc_close(context, &ccache); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_remove_cred(apiCB *context, ccache_p *ccache, cred_union in_creds, cc_int32 expected_err, const char *description) {
    cc_result err = CC_NOERROR;
    ccache_cit *iterator = NULL;
    int found = 0;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_ERR_CRED_VERSION,
        CC_NOTFOUND,
        CC_NO_EXIST
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_remove_cred(context, ccache, in_creds);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    // make sure credentials were truly stored
    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }

    while (!err && !found) {
        cred_union *creds = NULL;

        err = cc_seq_fetch_creds_next(context, &creds, iterator);
        if (!err) {
            found = !compare_v5_creds_unions_compat(&in_creds, creds);
        }

        if (creds) { cc_free_creds(context, &creds); }
    }

    if (err == CC_END) {
        check_if(found, "credentials not removed from ccache");
        err = CC_NOERROR;
    }

    if (iterator) { cc_seq_fetch_creds_end(context, &iterator); }

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_seq_fetch_NCs_begin(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    ccache_cit *iterator = NULL;

    BEGIN_TEST("cc_seq_fetch_NCs_begin");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);
    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }
    if (!err) {
        // try making when there are no existing ccaches (shouldn't make a difference, but just in case)
        check_once_cc_seq_fetch_NCs_begin(context, &iterator, CC_NOERROR, "when there are no existing ccaches");

        err = cc_create(context, "TEST_CC_SEQ_FETCH_NCS_BEGIN", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        // try making when at least one ccache already exists (just to cover all our bases)
        check_once_cc_seq_fetch_NCs_begin(context, &iterator, CC_NOERROR, "when at least one ccache already exists");

        // try bad parameters
        check_once_cc_seq_fetch_NCs_begin(context, NULL, CC_BAD_PARM, "NULL param"); // NULL iterator
    }
    // we'll do a comprehensive test of cc_ccache_iterator related functions later in the test suite

    if (ccache ) { cc_close(context, &ccache); }
    if (context) { cc_shutdown(&context); }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_seq_fetch_NCs_begin(apiCB *context, ccache_cit **iterator, cc_result expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[4] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NOMEM,
        CC_NO_EXIST
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

     err = cc_seq_fetch_NCs_begin(context, iterator);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    // we'll do a comprehensive test of cc_ccache_iterator related functions later

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_seq_fetch_NCs_next(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    ccache_cit *iterator = NULL;
    unsigned int i;

    BEGIN_TEST("cc_seq_fetch_NCs_next");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // iterate with no ccaches
    if (!err) {
        err = cc_seq_fetch_NCs_begin(context, &iterator);
    }
    check_once_cc_seq_fetch_NCs_next(context, iterator, 0, CC_NOERROR, "iterating over an empty collection");
    if (iterator) {
        cc_seq_fetch_creds_end(context, &iterator);
        iterator = NULL;
    }

    // iterate with one ccache
    if (!err) {
        destroy_all_ccaches_v2(context);
        err = cc_create(context, "TEST_CC_SEQ_FETCH_NCS_NEXT", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }
    if (!err) {
        err = cc_seq_fetch_NCs_begin(context, &iterator);
    }
    check_once_cc_seq_fetch_NCs_next(context, iterator, 1, CC_NOERROR, "iterating over a collection of 1 ccache");
    if (iterator) {
        cc_seq_fetch_creds_end(context, &iterator);
        iterator = NULL;
    }

    // iterate with several ccaches
    if (!err) {
        destroy_all_ccaches_v2(context);
    }
    for(i = 0; !err && (i < 1000); i++)
    {
        char *name = NULL;

        if (i%100 == 0) fprintf(stdout, ".");
        asprintf (&name, "TEST_CC_SEQ_FETCH_NCS_NEXT_%d", i);
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
        if (ccache) {
            cc_close(context, &ccache);
            ccache = NULL;
        }
        free (name);
    }
    if (!err) {
        err = cc_seq_fetch_NCs_begin(context, &iterator);
    }
    check_once_cc_seq_fetch_NCs_next(context, iterator, 1000, CC_NOERROR, "iterating over a collection of 1000 ccache");
    if (iterator) {
        cc_seq_fetch_creds_end(context, &iterator);
        iterator = NULL;
    }


    if (ccache) { cc_close(context, &ccache); }
    if (iterator) { cc_seq_fetch_creds_end(context, &iterator); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_seq_fetch_NCs_next(apiCB *context, ccache_cit *iterator, cc_uint32 expected_count, cc_result expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_END,
        CC_BAD_PARM,
        CC_NOMEM,
        CC_NO_EXIST
    };
#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    ccache_p *ccache = NULL;
    cc_uint32 actual_count = 0;

    BEGIN_CHECK_ONCE(description);

    while (!err) {
        err = cc_seq_fetch_NCs_next(context, &ccache, iterator);
        if (ccache) {
            actual_count++;
            cc_close(context, &ccache);
            ccache = NULL;
        }
    }
    if (err == CC_END) {
        err = CC_NOERROR;
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    check_if(actual_count != expected_count, "iterator didn't iterate over all ccaches");

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_get_NC_info(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    unsigned int i;

    BEGIN_TEST("cc_get_NC_info");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // iterate with no ccaches
    check_once_cc_get_NC_info(context, "", "", CC_CRED_MAX, 0, CC_NOERROR, "iterating over an empty collection");

    // iterate with one ccache
    if (!err) {
        destroy_all_ccaches_v2(context);
        err = cc_create(context, "TEST_CC_GET_NC_INFO", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }
    check_once_cc_get_NC_info(context, "TEST_CC_GET_NC_INFO", "foo@BAR.ORG", CC_CRED_V5, 1, CC_NOERROR, "iterating over a collection of 1 ccache");

    // iterate with several ccaches
    if (!err) {
        destroy_all_ccaches_v2(context);
    }
    for(i = 0; !err && (i < 1000); i++)
    {
        char *name = NULL;

        if (i%100 == 0) fprintf(stdout, ".");
        asprintf (&name, "TEST_CC_GET_NC_INFO_%d", i);
        err = cc_create(context, name, "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
        if (ccache) {
            cc_close(context, &ccache);
            ccache = NULL;
        }
        free (name);
    }
    check_once_cc_get_NC_info(context, "TEST_CC_GET_NC_INFO", "foo@BAR.ORG", CC_CRED_V5, 1000, CC_NOERROR, "iterating over a collection of 1000 ccache");

    if (ccache) { cc_close(context, &ccache); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_get_NC_info(apiCB *context,
                                    const char *expected_name_prefix,
                                    const char *expected_principal,
                                    cc_int32 expected_version,
                                    cc_uint32 expected_count,
                                    cc_result expected_err,
                                    const char *description) {
    cc_result err = CC_NOERROR;
    infoNC **info = NULL;

    cc_result possible_return_values[4] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NOMEM,
        CC_NO_EXIST
    };
#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    cc_uint32 actual_count = 0;

    BEGIN_CHECK_ONCE(description);

    err = cc_get_NC_info(context, &info);

    for (actual_count = 0; !err && info[actual_count]; actual_count++) {
        check_if(strncmp(info[actual_count]->name, expected_name_prefix, strlen(expected_name_prefix)), "got incorrect ccache name");
        check_if(strcmp(info[actual_count]->principal, expected_principal), "got incorrect principal name");
        check_if(info[actual_count]->vers != expected_version, "got incorrect cred version");
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    check_if(actual_count != expected_count, "NC info didn't list all ccaches");

    if (info) { cc_free_NC_info (context, &info); }
    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_seq_fetch_creds_begin(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    ccache_p *dup_ccache = NULL;
    ccache_cit *creds_iterator = NULL;
    char *name = NULL;

    BEGIN_TEST("cc_seq_fetch_creds_begin");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    if (!err) {
        err = cc_create(context, "TEST_CC_SEQ_FETCH_CREDS_BEGIN", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }

    // valid params
    if (!err) {
        check_once_cc_seq_fetch_creds_begin(context, ccache, &creds_iterator, CC_NOERROR, "valid params");
    }
    if (creds_iterator) {
        cc_seq_fetch_creds_end(context, &creds_iterator);
        creds_iterator = NULL;
    }

    // NULL out param
    if (!err) {
        check_once_cc_seq_fetch_creds_begin(context, ccache, NULL, CC_BAD_PARM, "NULL out iterator param");
    }
    if (creds_iterator) {
        cc_seq_fetch_creds_end(context, &creds_iterator);
        creds_iterator = NULL;
    }

    // non-existent ccache
    if (ccache) {
        err = cc_get_name(context, ccache, &name);
        if (!err) {
            err = cc_open(context, name, CC_CRED_V5, 0, &dup_ccache);
        }
        if (name) { cc_free_name(context, &name); }
        if (dup_ccache) { cc_destroy(context, &dup_ccache); }
    }

    if (!err) {
        check_once_cc_seq_fetch_creds_begin(context, ccache, &creds_iterator, CC_NO_EXIST, "invalid ccache");
    }

    if (creds_iterator) {
        cc_seq_fetch_creds_end(context, &creds_iterator);
        creds_iterator = NULL;
    }
    if (ccache) { cc_close(context, &ccache); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_seq_fetch_creds_begin(apiCB *context, ccache_p *ccache, ccache_cit **iterator, cc_result expected_err, const char *description) {
    cc_result err = CC_NOERROR;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_BAD_PARM,
        CC_NOMEM,
        CC_NO_EXIST
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    err = cc_seq_fetch_creds_begin(context, ccache, iterator);

    // check returned error
    check_err(err, expected_err, possible_return_values);

    END_CHECK_ONCE;

    return err;
}

// ---------------------------------------------------------------------------

int check_cc_seq_fetch_creds_next(void) {
    cc_result err = 0;
    apiCB *context = NULL;
    ccache_p *ccache = NULL;
    cred_union creds_union;
    ccache_cit *iterator = NULL;
    unsigned int i;

    BEGIN_TEST("cc_seq_fetch_creds_next");

    err = cc_initialize(&context, ccapi_version_2, NULL, NULL);

    if (!err) {
        err = destroy_all_ccaches_v2(context);
    }

    // iterate with no creds
    if (!err) {
        err = cc_create(context, "TEST_CC_SEQ_FETCH_CREDS_NEXT", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }
    check_once_cc_seq_fetch_creds_next(context, iterator, 0, CC_NOERROR, "iterating over an empty ccache");
    if (iterator) {
        cc_seq_fetch_creds_end(context, &iterator);
        iterator = NULL;
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    // iterate with one cred
    if (!err) {
        destroy_all_ccaches_v2(context);
        err = cc_create(context, "TEST_CC_SEQ_FETCH_CREDS_NEXT", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    if (!err) {
        new_v5_creds_union_compat(&creds_union, "BAR.ORG");
        err = cc_store(context, ccache, creds_union);
        release_v5_creds_union_compat(&creds_union);
    }
    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }
    check_once_cc_seq_fetch_creds_next(context, iterator, 1, CC_NOERROR, "iterating over a ccache with 1 cred");
    if (iterator) {
        cc_seq_fetch_creds_end(context, &iterator);
        iterator = NULL;
    }
    if (ccache) {
        cc_close(context, &ccache);
        ccache = NULL;
    }

    // iterate with several creds
    if (!err) {
        destroy_all_ccaches_v2(context);
        err = cc_create(context, "TEST_CC_SEQ_FETCH_CREDS_NEXT", "foo@BAR.ORG", CC_CRED_V5, 0, &ccache);
    }
    for(i = 0; !err && (i < 1000); i++) {
        if (i%100 == 0) fprintf(stdout, ".");
        new_v5_creds_union_compat(&creds_union, "BAR.ORG");
        err = cc_store(context, ccache, creds_union);
        release_v5_creds_union_compat(&creds_union);
    }
    if (!err) {
        err = cc_seq_fetch_creds_begin(context, ccache, &iterator);
    }
    check_once_cc_seq_fetch_creds_next(context, iterator, 1000, CC_NOERROR, "iterating over a ccache with 1000 creds");

    if (ccache) { cc_close(context, &ccache); }
    if (iterator) { cc_seq_fetch_creds_end(context, &iterator); }
    if (context) {
        destroy_all_ccaches_v2(context);
        cc_shutdown(&context);
    }

    END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

cc_result check_once_cc_seq_fetch_creds_next(apiCB *context, ccache_cit *iterator, cc_uint32 expected_count, cc_result expected_err, const char *description) {
    cc_result   err             = CC_NOERROR;
    cred_union *creds           = NULL;
    cc_uint32   actual_count    = 0;

    cc_result possible_return_values[5] = {
        CC_NOERROR,
        CC_END,
        CC_BAD_PARM,
        CC_NOMEM,
        CC_NO_EXIST,
    };

    BEGIN_CHECK_ONCE(description);

#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

    while (!err) {
        err = cc_seq_fetch_creds_next(context, &creds, iterator);
        if (creds) {
            actual_count++;
            cc_free_creds(context, &creds);
            creds = NULL;
        }
    }
    if (err == CC_END) {
        err = CC_NOERROR;
    }

    // check returned error
    check_err(err, expected_err, possible_return_values);

    check_if(actual_count != expected_count, "iterator didn't iterate over all ccaches");

    END_CHECK_ONCE;

    return err;
}
