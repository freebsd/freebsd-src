#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "test_ccapi_check.h"
#include "test_ccapi_util.h"
#include "test_ccapi_context.h"
#include "test_ccapi_ccache.h"

// ---------------------------------------------------------------------------


int check_cc_ccache_release(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_release");

	#ifndef cc_ccache_release
	log_error("cc_ccache_release is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}



	if (!err) {
		check_once_cc_ccache_release(context, ccache, ccNoError, NULL);
		ccache = NULL;
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_ccache_release */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_release(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[2] = {
		ccNoError,
		ccErrInvalidCCache,
	};

	cc_string_t name = NULL;

	err = cc_ccache_get_name(ccache, &name);
	err = cc_ccache_release(ccache);
	ccache = NULL;

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_release

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err && name) { // try opening released ccache to make sure it still exists
		err = cc_context_open_ccache(context, name->data, &ccache);
	}
	check_if(err == ccErrCCacheNotFound, "released ccache was actually destroyed instead");

	if (ccache) { cc_ccache_destroy(ccache); }
	if (name) { cc_string_release(name); }

	#endif /* cc_ccache_release */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_destroy(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_destroy");

	#ifndef cc_ccache_destroy
	log_error("cc_ccache_destroy is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}



	if (!err) {
		check_once_cc_ccache_destroy(context, ccache, ccNoError, NULL);
		ccache = NULL;
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_ccache_destroy */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_destroy(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[2] = {
		ccNoError,
		ccErrInvalidCCache,
	};

	cc_string_t name = NULL;

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_destroy

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_get_name(ccache, &name);
	err = cc_ccache_destroy(ccache);
	ccache = NULL;

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err && name) { // try opening released ccache to make sure it still exists
		err = cc_context_open_ccache(context, name->data, &ccache);
	}
	check_if(err != ccErrCCacheNotFound, "destroyed ccache was actually released instead");

	if (ccache) { cc_ccache_destroy(ccache); }
	if (name) { cc_string_release(name); }

	#endif /* cc_ccache_destroy */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_set_default(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_set_default");

	#ifndef cc_ccache_set_default
	log_error("cc_ccache_set_default is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	// try when it's the only ccache (already default)
	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_set_default(context, ccache, ccNoError, "when it's the only ccache (already default)");
	}
	if (ccache) {
		err = cc_ccache_release(ccache);
		ccache = NULL;
	}

	// try when it's not the only ccache (and not default)
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "baz@BAR.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_set_default(context, ccache, ccNoError, "when it's not the only ccache (and not default)");
	}
	if (ccache) {
		err = cc_ccache_release(ccache);
		ccache = NULL;
	}

	// try when it's not the only ccache (and already default)
	if (!err) {
		err = cc_context_open_default_ccache(context, &ccache);
	}
	if (!err) {
		check_once_cc_ccache_set_default(context, ccache, ccNoError, "when it's not the only ccache (and already default)");
	}
	if (ccache) {
		err = cc_ccache_release(ccache);
		ccache = NULL;
	}

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_ccache_set_default */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_set_default(cc_context_t context, cc_ccache_t ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[3] = {
		ccNoError,
		ccErrInvalidCCache,
		ccErrCCacheNotFound,
	};

	cc_ccache_t default_ccache = NULL;
	cc_string_t name = NULL;
	cc_string_t default_name = NULL;

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_set_default

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_set_default(ccache);
	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		err = cc_ccache_get_name(ccache, &name);
	}
	if (!err) {
		err = cc_context_open_default_ccache(context, &default_ccache);
	}
	if (!err) {
		err = cc_ccache_get_name(default_ccache, &default_name);
	}
	if (name && default_name) {
		check_if(strcmp(name->data, default_name->data), NULL);
	}
	else {
		check_if(1, "cc_ccache_get_name failed");
	}

	if (default_ccache) { cc_ccache_release(default_ccache); }
	//if (ccache) { cc_ccache_destroy(ccache); } // ccache is released by the caller
	if (default_name) { cc_string_release(default_name); }
	if (name) { cc_string_release(name); }

	#endif /* cc_ccache_set_default */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_get_credentials_version(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_get_credentials_version");

	#ifndef cc_ccache_get_credentials_version
	log_error("cc_ccache_get_credentials_version is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	// try one created with v5 creds
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_credentials_version(ccache, cc_credentials_v5, ccNoError, "v5 creds");
	}
	else {
		log_error("cc_context_create_new_ccache failed, can't complete test");
		failure_count++;
	}

	if (ccache) {
		cc_ccache_destroy(ccache);
		ccache = NULL;
	}

	err = ccNoError;

	if (context) { cc_context_release(context); }

	#endif /* cc_ccache_get_credentials_version */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_get_credentials_version(cc_ccache_t ccache, cc_uint32 expected_cred_vers, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrCCacheNotFound,
	};

	cc_uint32 stored_cred_vers = 0;

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_credentials_version

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_get_credentials_version(ccache, &stored_cred_vers);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(stored_cred_vers != expected_cred_vers, NULL);
	}

	#endif /* cc_ccache_get_credentials_version */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_get_name(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_get_name");

	#ifndef cc_ccache_get_name
	log_error("cc_ccache_get_name is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// try with unique ccache (which happens to be default)
	if (!err) {
		err = cc_context_create_ccache(context, "0", cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_name(ccache, "0", ccNoError, "unique ccache (which happens to be default)");
	}
	else {
		log_error("cc_context_create_ccache failed, can't complete test");
		failure_count++;
	}
	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}

	// try with unique ccache (which is not default)
	if (!err) {
		err = cc_context_create_ccache(context, "1", cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_name(ccache, "1", ccNoError, "unique ccache (which is not default)");
	}
	else {
		log_error("cc_context_create_ccache failed, can't complete test");
		failure_count++;
	}

	// try with bad param
	if (!err) {
		check_once_cc_ccache_get_name(ccache, NULL, ccErrBadParam, "NULL param");
	}
	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_get_name */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_get_name(cc_ccache_t ccache, const char *expected_name, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrCCacheNotFound,
	};

	cc_string_t stored_name = NULL;

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_name

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (expected_name == NULL) { // we want to try with a NULL param
		err = cc_ccache_get_name(ccache, NULL);
	}
	else {
		err = cc_ccache_get_name(ccache, &stored_name);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(strcmp(stored_name->data, expected_name), NULL);
	}

	if (stored_name) { cc_string_release(stored_name); }

	#endif /* cc_ccache_get_name */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------

cc_int32 check_once_cc_ccache_get_principal(cc_ccache_t ccache,
                                            cc_uint32 cred_vers,
                                            const char *expected_principal,
                                            cc_int32 expected_err,
                                            const char *description) {
	cc_int32 err = ccNoError;
	cc_string_t stored_principal = NULL;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrNoMem,
		ccErrBadCredentialsVersion,
		ccErrBadParam,
		ccErrInvalidCCache,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_principal

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (expected_principal == NULL) { // we want to try with a NULL param
		err = cc_ccache_get_principal(ccache, cred_vers, NULL);
	}
	else {
		err = cc_ccache_get_principal(ccache, cred_vers, &stored_principal);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(strcmp(stored_principal->data, expected_principal), "expected princ == \"%s\" stored princ == \"%s\"", expected_principal, stored_principal->data);
	}

	if (stored_principal) { cc_string_release(stored_principal); }

	#endif /* cc_ccache_get_principal */

	END_CHECK_ONCE;

	return err;
}

// ---------------------------------------------------------------------------

int check_cc_ccache_get_principal(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_get_principal");

	#ifndef cc_ccache_get_principal
	log_error("cc_ccache_get_principal is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// try with krb5 principal
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo/BAR@BAZ.ORG", &ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_principal(ccache, cc_credentials_v5, "foo/BAR@BAZ.ORG", ccNoError, "trying to get krb5 princ for krb5 ccache");
	}
	else {
		log_error("cc_context_create_new_ccache failed, can't complete test");
		failure_count++;
	}

        // try with bad param
        if (!err) {
            check_once_cc_ccache_get_principal(ccache, cc_credentials_v5,
                                               NULL, ccErrBadParam,
                                               "passed null out param");
        }

	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_get_principal */

	END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

int check_cc_ccache_set_principal(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_set_principal");

	#ifndef cc_ccache_set_principal
	log_error("cc_ccache_set_principal is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

        // replace v5 only ccache's principal
        if (!err) {
            err = cc_context_create_new_ccache(context, cc_credentials_v5,
                                               "foo@BAZ.ORG", &ccache);
        }
        if (!err) {
            check_once_cc_ccache_set_principal(
                ccache, cc_credentials_v5, "foo/BAZ@BAR.ORG", ccNoError,
                "replace v5 only ccache's principal (empty ccache)");
        }
        else {
            log_error(
                "cc_context_create_new_ccache failed, can't complete test");
            failure_count++;
        }

        // bad params
        if (!err) {
            check_once_cc_ccache_set_principal(ccache, cc_credentials_v5,
                                               NULL, ccErrBadParam,
                                               "NULL principal");
        }

        if (ccache) {
            cc_ccache_destroy(ccache);
            ccache = NULL;
        }

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_set_principal */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_set_principal(cc_ccache_t ccache, cc_uint32 cred_vers, const char *in_principal, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_string_t stored_principal = NULL;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrNoMem,
		ccErrInvalidCCache,
		ccErrBadCredentialsVersion,
		ccErrBadParam,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_set_principal

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_set_principal(ccache, cred_vers, in_principal);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		err = cc_ccache_get_principal(ccache, cred_vers, &stored_principal);
	}

	// compare stored with input
	if (!err) {
		check_if(strcmp(stored_principal->data, in_principal), "expected princ == \"%s\" stored princ == \"%s\"", in_principal, stored_principal->data);
	}

	if (stored_principal) { cc_string_release(stored_principal); }

	#endif /* cc_ccache_set_principal */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_store_credentials(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_ccache_t dup_ccache = NULL;
	cc_credentials_union creds_union;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_ccache_store_credentials");

	#ifndef cc_ccache_store_credentials
	log_error("cc_ccache_store_credentials is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	// cred with matching version and realm
	if (!err) {
		err = new_v5_creds_union(&creds_union, "BAR.ORG");
	}

	if (!err) {
		check_once_cc_ccache_store_credentials(ccache, &creds_union, ccNoError, "ok creds");
	}

	if (&creds_union) { release_v5_creds_union(&creds_union); }

	// try with bad params
	check_once_cc_ccache_store_credentials(ccache, NULL, ccErrBadParam, "NULL creds param");

	// invalid creds
	if (!err) {
		err = new_v5_creds_union(&creds_union, "BAR.ORG");
	}

	if (!err) {
		if (creds_union.credentials.credentials_v5->client) {
			free(creds_union.credentials.credentials_v5->client);
			creds_union.credentials.credentials_v5->client = NULL;
		}
		check_once_cc_ccache_store_credentials(ccache, &creds_union, ccErrBadParam, "invalid creds (NULL client string)");
	}

	if (&creds_union) { release_v5_creds_union(&creds_union); }

	// non-existent ccache
	if (ccache) {
		err = cc_ccache_get_name(ccache, &name);
		if (!err) {
			err = cc_context_open_ccache(context, name->data, &dup_ccache);
		}
		if (name) { cc_string_release(name); }
		if (dup_ccache) { cc_ccache_destroy(dup_ccache); }
	}

	if (!err) {
		err = new_v5_creds_union(&creds_union, "BAR.ORG");
	}

	if (!err) {
		check_once_cc_ccache_store_credentials(ccache, &creds_union, ccErrInvalidCCache, "invalid ccache");
	}

	if (&creds_union) { release_v5_creds_union(&creds_union); }
	if (ccache) { cc_ccache_release(ccache); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_store_credentials */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_store_credentials(cc_ccache_t ccache, const cc_credentials_union *credentials, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_credentials_t creds = NULL;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadParam,
		ccErrInvalidCCache,
		ccErrInvalidCredentials,
		ccErrBadCredentialsVersion,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_store_credentials

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_store_credentials(ccache, credentials);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	// make sure credentials were truly stored
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &creds_iterator);
	}
	while (!err) {
		err = cc_credentials_iterator_next(creds_iterator, &creds);
		if (creds) {
			if (compare_v5_creds_unions(credentials, creds->data) == 0) {
				break;
			}
			cc_credentials_release(creds);
			creds = NULL;
		}
	}

	if (err == ccIteratorEnd) {
		check_if((creds != NULL), "stored credentials not found in ccache");
		err = ccNoError;
	}
	if (creds) { cc_credentials_release(creds); }

	#endif /* cc_ccache_store_credentials */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_remove_credentials(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_ccache_t dup_ccache = NULL;
	cc_credentials_t creds_array[10];
	cc_credentials_t creds = NULL;
	cc_credentials_union creds_union;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_string_t name = NULL;
	unsigned int i;

	BEGIN_TEST("cc_ccache_remove_credentials");

	#ifndef cc_ccache_remove_credentials
	log_error("cc_ccache_remove_credentials is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	// store 10 creds and retrieve their cc_credentials_t representations
	for(i = 0; !err && (i < 10); i++) {
		new_v5_creds_union(&creds_union, "BAR.ORG");
		err = cc_ccache_store_credentials(ccache, &creds_union);
		if (&creds_union) { release_v5_creds_union(&creds_union); }
	}
	if (err) {
		log_error("failure to store creds_union in remove_creds test");
	}
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &creds_iterator);
	}
	i = 0;
	while (!err && i < 10) {
		err = cc_credentials_iterator_next(creds_iterator, &creds);
		if (creds) {
			creds_array[i++] = creds;
			creds = NULL;
		}
	}
	if (err == ccIteratorEnd) { err = ccNoError; }

	// remove 10 valid creds
	for (i = 0; !err && (i < 8); i++) {
		check_once_cc_ccache_remove_credentials(ccache, creds_array[i], ccNoError, "10 ok creds");
	}

	// NULL param
	check_once_cc_ccache_remove_credentials(ccache, NULL, ccErrBadParam, "NULL creds in param");

	// non-existent creds (remove same one twice)
	check_once_cc_ccache_remove_credentials(ccache, creds_array[0], ccErrInvalidCredentials, "removed same creds twice");

	// non-existent ccache
	if (ccache) {
		err = cc_ccache_get_name(ccache, &name);
		if (!err) {
			err = cc_context_open_ccache(context, name->data, &dup_ccache);
		}
		if (name) { cc_string_release(name); }
		if (dup_ccache) { cc_ccache_destroy(dup_ccache); }
	}

	if (!err) {
		err = new_v5_creds_union(&creds_union, "BAR.ORG");
	}

	if (!err) {
		check_once_cc_ccache_remove_credentials(ccache, creds_array[8], ccErrInvalidCCache, "invalid ccache");
	}

	for(i = 0; i < 10; i++) {
		if (creds_array[i]) { cc_credentials_release(creds_array[i]); }
	}

	if (ccache) { cc_ccache_release(ccache); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_remove_credentials */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_remove_credentials(cc_ccache_t ccache, cc_credentials_t in_creds, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_credentials_t creds = NULL;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadParam,
		ccErrInvalidCCache,
		ccErrInvalidCredentials,
		ccErrCredentialsNotFound,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_remove_credentials

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_remove_credentials(ccache, in_creds);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	// make sure credentials were truly removed
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &creds_iterator);
	}
	while (!err) {
		err = cc_credentials_iterator_next(creds_iterator, &creds);
		if (creds) {
			if (compare_v5_creds_unions(in_creds->data, creds->data) == 0) {
				break;
			}
			cc_credentials_release(creds);
			creds = NULL;
		}
	}

	if (err == ccIteratorEnd) {
		err = ccNoError;
	}
	else {
		check_if((creds != NULL), "credentials not removed from ccache");
	}
	if (creds) { cc_credentials_release(creds); }

	#endif /* cc_ccache_remove_credentials */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------


int check_cc_ccache_new_credentials_iterator(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_ccache_t dup_ccache = NULL;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_ccache_new_credentials_iterator");

	#ifndef cc_ccache_new_credentials_iterator
	log_error("cc_ccache_new_credentials_iterator is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	// valid params
	if (!err) {
		check_once_cc_ccache_new_credentials_iterator(ccache, &creds_iterator, ccNoError, "valid params");
	}
	if (creds_iterator) {
		cc_credentials_iterator_release(creds_iterator);
		creds_iterator = NULL;
	}

	// NULL out param
	if (!err) {
		check_once_cc_ccache_new_credentials_iterator(ccache, NULL, ccErrBadParam, "NULL out iterator param");
	}
	if (creds_iterator) {
		cc_credentials_iterator_release(creds_iterator);
		creds_iterator = NULL;
	}

	// non-existent ccache
	if (ccache) {
		err = cc_ccache_get_name(ccache, &name);
		if (!err) {
			err = cc_context_open_ccache(context, name->data, &dup_ccache);
		}
		if (name) { cc_string_release(name); }
		if (dup_ccache) { cc_ccache_destroy(dup_ccache); }
	}

	if (!err) {
		check_once_cc_ccache_new_credentials_iterator(ccache, &creds_iterator, ccErrInvalidCCache, "invalid ccache");
	}

	if (creds_iterator) {
		cc_credentials_iterator_release(creds_iterator);
		creds_iterator = NULL;
	}
	if (ccache) { cc_ccache_release(ccache); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_new_credentials_iterator */

	END_TEST_AND_RETURN
}


cc_int32 check_once_cc_ccache_new_credentials_iterator(cc_ccache_t ccache, cc_credentials_iterator_t *iterator, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[5] = {
		ccNoError,
		ccErrBadParam,
		ccErrNoMem,
		ccErrCCacheNotFound,
		ccErrInvalidCCache,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_new_credentials_iterator

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_new_credentials_iterator(ccache, iterator);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	#endif /* cc_ccache_new_credentials_iterator */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------

cc_int32 check_once_cc_ccache_get_change_time(cc_ccache_t ccache, cc_time_t *last_time, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_time_t this_time = 0;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_change_time

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (last_time == NULL) {
		err = cc_ccache_get_change_time(ccache, NULL); // passed NULL to compare against because intention is actually to pass bad param instead
	} else {
		err = cc_ccache_get_change_time(ccache, &this_time);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if ((!err) && last_time) {
		check_if(this_time <= *last_time, "change time didn't increase when expected");
		*last_time = this_time;
	}

	#endif /* cc_ccache_get_change_time */

	END_CHECK_ONCE;

	return err;
}

// ---------------------------------------------------------------------------

int check_cc_ccache_get_change_time(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t dummy_ccache = NULL;
	cc_ccache_t ccache = NULL;
	cc_credentials_union creds_union;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_credentials_t credentials = NULL;
	cc_time_t last_time = 0;

    BEGIN_TEST("cc_ccache_get_change_time");

	#ifndef cc_ccache_get_change_time
	log_error("cc_ccache_get_change_time is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// create some ccaches (so that the one we keep around as 'ccache' is not default)
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (ccache) {
		cc_ccache_release(ccache);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAZ.ORG", &ccache);
	}

	// change it in all the ways it can change, checking after each

	// the ccache is created
	if (!err) {
		check_once_cc_ccache_get_change_time(ccache, &last_time, ccNoError, "new ccache (change time should be > 0)");
	}

	// the ccache is made default
	if (!err) {
		err = cc_ccache_set_default(ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_change_time(ccache, &last_time, ccNoError, "non-default ccache became default");
	}

	// the ccache is made not-default
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "something@ELSE.COM", &dummy_ccache);
	}
	if (!err) {
		err = cc_ccache_set_default(dummy_ccache);
	}
	if (dummy_ccache) {
		cc_ccache_release(dummy_ccache);
	}
	if (!err) {
		check_once_cc_ccache_get_change_time(ccache, &last_time, ccNoError, "default ccache became non-default");
	}

	// try with bad params

	// NULL out param
	if (!err) {
		check_once_cc_ccache_get_change_time(ccache, NULL, ccErrBadParam, "NULL out param for time");
	}

	// store a credential
	if (!err) {
		new_v5_creds_union(&creds_union, "BAR.ORG");
		err = cc_ccache_store_credentials(ccache, &creds_union);
		release_v5_creds_union(&creds_union);
	}
	check_once_cc_ccache_get_change_time(ccache, &last_time, ccNoError, "stored new credential");

	if (!err) {
		// change principal (fails with ccErrBadInternalMessage)
		err = cc_ccache_set_principal(ccache, cc_credentials_v5, "foo@BAR.ORG");
		if (err) {
			log_error("failed to change ccache's principal - %s (%d)", translate_ccapi_error(err), err);
			failure_count++;
			err = ccNoError;
		}
	}
	check_once_cc_context_get_change_time(context, &last_time, ccNoError, "after changing a principle");

	// remove a credential
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &creds_iterator);
	}
	if (!err) {
		err = cc_credentials_iterator_next(creds_iterator, &credentials);
	}
	if (err == ccIteratorEnd) {
		err = ccNoError;
	}
	if (!err) {
		err = cc_ccache_remove_credentials(ccache, credentials);
	}
	check_once_cc_context_get_change_time(context, &last_time, ccNoError, "after removing a credential");


	// invalid ccache
	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		check_once_cc_ccache_get_change_time(ccache, &last_time, ccErrInvalidCCache, "getting change time on destroyed ccache");
	}

	if (ccache) { cc_ccache_release(ccache); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_get_change_time */

	END_TEST_AND_RETURN
}


// ---------------------------------------------------------------------------

cc_int32 check_once_cc_ccache_get_last_default_time(cc_ccache_t ccache, cc_time_t *last_time, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_time_t this_time = 0;

	cc_int32 possible_return_values[5] = {
		ccNoError,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrNeverDefault,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_last_default_time

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (last_time == NULL) {
		err = cc_ccache_get_last_default_time(ccache, NULL); // passed NULL to compare against because intention is actually to pass bad param instead
	} else {
		err = cc_ccache_get_last_default_time(ccache, &this_time);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err && last_time) {
		check_if(this_time > *last_time, "last default time isn't as expected");
		*last_time = this_time;
	}

	#endif /* cc_ccache_get_last_default_time */

	END_CHECK_ONCE;

	return err;
}

// ---------------------------------------------------------------------------

int check_cc_ccache_get_last_default_time(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache_1 = NULL;
	cc_ccache_t ccache_2 = NULL;
	cc_time_t last_time_1 = 0;
	cc_time_t last_time_2 = 0;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_ccache_get_last_default_time");

	#ifndef cc_ccache_get_last_default_time
	log_error("cc_ccache_get_last_default_time is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// create 2 ccaches
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@ONE.ORG", &ccache_1);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@TWO.ORG", &ccache_2);
	}

	if (!err) {
		err = cc_ccache_get_change_time(ccache_1, &last_time_1);
	}

	// since we destroyed all ccaches before creating these two,
	// ccache_1 should be default and ccache_2 should never have been default
	if (!err) {
		check_once_cc_ccache_get_last_default_time(ccache_1, &last_time_1, ccNoError, "ccache_1 default at creation");
		check_once_cc_ccache_get_last_default_time(ccache_2, &last_time_2, ccErrNeverDefault, "ccache_2 never default");
	}

	// make ccache_2 default and check each of their times again
	if (!err) {
		err = cc_ccache_set_default(ccache_2);
	}
	if (!err) {
		err = cc_ccache_get_change_time(ccache_2, &last_time_2);
	}
	if (!err) {
		check_once_cc_ccache_get_last_default_time(ccache_1, &last_time_1, ccNoError, "ccache_1 no longer default");
		check_once_cc_ccache_get_last_default_time(ccache_2, &last_time_2, ccNoError, "ccache_2 newly default");
	}

	// NULL param
	if (!err) {
		check_once_cc_ccache_get_last_default_time(ccache_1, NULL, ccErrBadParam, "NULL out param");
	}

	// non-existent ccache
	if (ccache_2) {
		cc_ccache_release(ccache_2);
		ccache_2 = NULL;
	}
	if (!err) {
		err = cc_ccache_get_name(ccache_1, &name);
	}
	if (!err) {
		err = cc_context_open_ccache(context, name->data, &ccache_2);
	}
	if (!err) {
		cc_ccache_destroy(ccache_2);
		ccache_2 = NULL;
	}

	if (!err) {
		check_once_cc_ccache_get_last_default_time(ccache_1, &last_time_1, ccErrInvalidCCache, "destroyed ccache");
	}

	if (ccache_1) { cc_ccache_release(ccache_1); }

	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_get_last_default_time */

	END_TEST_AND_RETURN
}

// ---------------------------------------------------------------------------

int check_cc_ccache_move(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t source = NULL;
	cc_ccache_t destination = NULL;

	cc_credentials_union creds_union;
	unsigned int i = 0;

	BEGIN_TEST("cc_ccache_move");

	#ifndef cc_ccache_move
	log_error("cc_ccache_move is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// create 2 ccaches
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@ONE.ORG", &source);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@TWO.ORG", &destination);
	}

	// store credentials in each
	for (i = 0; !err && (i < 10); i++) {
		new_v5_creds_union(&creds_union, "ONE.ORG");
		err = cc_ccache_store_credentials(source, &creds_union);
	}
	for (i = 0; !err && (i < 10); i++) {
		new_v5_creds_union(&creds_union, "TWO.ORG");
		err = cc_ccache_store_credentials(destination, &creds_union);
	}

	// move source into destination
	if (!err) {
		check_once_cc_ccache_move(source, destination, ccNoError, "valid params");
	}

	// NULL param
	if (!err) {
		check_once_cc_ccache_move(destination, NULL, ccErrBadParam, "NULL destination param");
	}

	// non-existent ccache
	if (!err) {
		check_once_cc_ccache_move(destination, source, ccErrInvalidCCache, "recently moved source as destination param");
	}

	if (source) { cc_ccache_release(source); }
	if (destination) { cc_ccache_release(destination); }

	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_move */

	END_TEST_AND_RETURN


}

cc_int32 check_once_cc_ccache_move(cc_ccache_t source, cc_ccache_t destination, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_credentials_t dst_creds[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, };
	cc_credentials_t creds = NULL;
	cc_credentials_iterator_t cred_iterator = NULL;
	unsigned int i = 0;

	cc_string_t src_principal = NULL;
	cc_string_t dst_principal = NULL;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrBadParam,
		ccErrInvalidCCache,
		ccErrCCacheNotFound,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_move

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (destination) {
		// verify all of destination's credentials are no longer there (save a list and call remove_cred for each, expecting an err in response)
		if (!err) {
			err = cc_ccache_new_credentials_iterator(destination, &cred_iterator);
		}
		while (!err && (i < 10)) {
			err = cc_credentials_iterator_next(cred_iterator, &creds);
			if (creds) {
				dst_creds[i++] = creds;
			}
		}
		if (err == ccIteratorEnd) {
			err = ccNoError;
		}
		if (cred_iterator) {
			cc_credentials_iterator_release(cred_iterator);
			cred_iterator = NULL;
		}

		// verify that destination's principal has changed to source's (strcmp)
		if (!err) {
			err = cc_ccache_get_principal(source, cc_credentials_v5, &src_principal);
		}
	}


	if (!err) {
		err = cc_ccache_move(source, destination);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);


	if (!err) {
		// verify all of destination's credentials are no longer there (save a list and call remove_cred for each, expecting an err in response)
		i = 0;
		while (dst_creds[i] && (i < 10)) {
			err = cc_ccache_remove_credentials(destination, dst_creds[i]);
			check_if(!(!err || err == ccErrCredentialsNotFound || ccErrInvalidCredentials), "credentials in destination not removed as promised");
			cc_credentials_release(dst_creds[i]);
			i++;
		}
		err = ccNoError;
	}

		// verify that destination's principal has changed to source's (strcmp)
		if (!err) {
			err = cc_ccache_get_principal(destination, cc_credentials_v5, &dst_principal);
		}
		if (!err) {
			check_if(strcmp(src_principal->data, dst_principal->data), "destination principal not overwritten by source");
		}

		// verify that handles for source are no longer valid (get_change_time)
		if (src_principal) {
			cc_string_release(src_principal);
			src_principal = NULL;
		}
		if (!err) {
			err = cc_ccache_get_principal(source, cc_credentials_v5, &src_principal);
			check_if(err != ccErrInvalidCCache, "source ccache was not invalidated after move");
		}


	if (cred_iterator) { cc_credentials_iterator_release(cred_iterator); }
	if (src_principal) { cc_string_release(src_principal); }
	if (dst_principal) { cc_string_release(dst_principal); }

	#endif /* cc_ccache_move */

	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_ccache_compare(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache_a = NULL;
	cc_ccache_t ccache_b = NULL;
	cc_uint32 equal = 0;

	BEGIN_TEST("cc_ccache_compare");

	#ifndef cc_ccache_compare
	log_error("cc_ccache_compare is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache_a);
	}
	if (!err) {
		err = cc_context_open_default_ccache(context, &ccache_b);
	}

	equal = 1;
	check_once_cc_ccache_compare(ccache_a, ccache_a, &equal, ccNoError, "compare ccache with same pointer");
	equal = 1;
	check_once_cc_ccache_compare(ccache_a, ccache_b, &equal, ccNoError, "compare different handles to same ccache");

	if (ccache_b) {
		cc_ccache_release(ccache_b);
		ccache_b = NULL;
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "baz@BAR.ORG", &ccache_b);
	}
	equal = 0;
	check_once_cc_ccache_compare(ccache_a, ccache_b, &equal, ccNoError, "compare different ccaches");
	check_once_cc_ccache_compare(ccache_a, NULL, &equal, ccErrBadParam, "NULL compare_to ccache");
	check_once_cc_ccache_compare(ccache_a, ccache_b, NULL, ccErrBadParam, "NULL out param");

	if (ccache_a) { cc_ccache_release(ccache_a); }
	if (ccache_b) { cc_ccache_release(ccache_b); }

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_compare */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_compare(cc_ccache_t ccache, cc_ccache_t compare_to, cc_uint32 *equal, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_uint32 actually_equal = 0;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidContext,
		ccErrBadParam,
		ccErrServerUnavailable,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_compare

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (equal) {
		actually_equal = *equal;
	}

	err = cc_ccache_compare(ccache, compare_to, equal);

	if (!err && equal) {
		if (actually_equal) {
			check_if(actually_equal != *equal, "equal ccaches not considered equal");
		}
		else {
			check_if(actually_equal != *equal, "non-equal ccaches considered equal");
		}
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	#endif /* cc_ccache_compare */

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_ccache_get_kdc_time_offset(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_time_t time_offset = 0;

	BEGIN_TEST("cc_ccache_get_kdc_time_offset");

	#ifndef cc_ccache_get_kdc_time_offset
	log_error("cc_ccache_get_kdc_time_offset is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	time_offset = 0;
	check_once_cc_ccache_get_kdc_time_offset(ccache, cc_credentials_v5, &time_offset, ccErrTimeOffsetNotSet, "brand new ccache (offset not yet set)");

	time_offset = 10;
	if (!err) {
		err = cc_ccache_set_kdc_time_offset(ccache, cc_credentials_v5, time_offset);
	}
	if (!err) {
		check_once_cc_ccache_get_kdc_time_offset(ccache, cc_credentials_v5, &time_offset, ccNoError, "offset set for v5");
	}

	check_once_cc_ccache_get_kdc_time_offset(ccache, cc_credentials_v5, NULL, ccErrBadParam, "NULL time_offset out param");

	if (ccache) { cc_ccache_release(ccache); }

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_get_kdc_time_offset */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_get_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_time_t *time_offset, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_time_t expected_offset;

	cc_int32 possible_return_values[7] = {
		ccNoError,
		ccErrTimeOffsetNotSet,
		ccErrCCacheNotFound,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrServerUnavailable,
		ccErrBadCredentialsVersion,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_get_kdc_time_offset

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (time_offset) {
		expected_offset = *time_offset;
	}

	err = cc_ccache_get_kdc_time_offset(ccache, credentials_version, time_offset);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err && time_offset) {
		check_if(*time_offset != expected_offset, "kdc time offset doesn't match expected value");
	}

	#endif /* cc_ccache_get_kdc_time_offset */

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_ccache_set_kdc_time_offset(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_set_kdc_time_offset");

	#ifndef cc_ccache_set_kdc_time_offset
	log_error("cc_ccache_set_kdc_time_offset is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	check_once_cc_ccache_set_kdc_time_offset(ccache, cc_credentials_v5, 0, ccNoError, "first time setting offset (v5)");

	if (ccache) { cc_ccache_release(ccache); }

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_set_kdc_time_offset */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_set_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_time_t time_offset, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_time_t stored_offset = 0;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrCCacheNotFound,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrServerUnavailable,
		ccErrBadCredentialsVersion,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_set_kdc_time_offset

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_set_kdc_time_offset(ccache, credentials_version, time_offset);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		err = cc_ccache_get_kdc_time_offset(ccache, credentials_version, &stored_offset);
	}

	if (!err) {
		check_if(time_offset != stored_offset, "kdc time offset doesn't match expected value");
	}

	#endif /* cc_ccache_set_kdc_time_offset */

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_ccache_clear_kdc_time_offset(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_ccache_clear_kdc_time_offset");

	#ifndef cc_ccache_clear_kdc_time_offset
	log_error("cc_ccache_clear_kdc_time_offset is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}

	check_once_cc_ccache_clear_kdc_time_offset(ccache, cc_credentials_v5, ccNoError, "clearing an offset that was never set (v5)");

	err = cc_ccache_set_kdc_time_offset(ccache, cc_credentials_v5, 0);

	check_once_cc_ccache_clear_kdc_time_offset(ccache, cc_credentials_v5, ccNoError, "clearing v5");

	if (ccache) { cc_ccache_release(ccache); }

	if (context) {
		err = destroy_all_ccaches(context);
		cc_context_release(context);
	}

	#endif /* cc_ccache_clear_kdc_time_offset */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_clear_kdc_time_offset(cc_ccache_t ccache, cc_int32 credentials_version, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;
	cc_time_t stored_offset = 0;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrCCacheNotFound,
		ccErrInvalidCCache,
		ccErrBadParam,
		ccErrServerUnavailable,
		ccErrBadCredentialsVersion,
	};
	BEGIN_CHECK_ONCE(description);

	#ifdef cc_ccache_clear_kdc_time_offset

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_ccache_clear_kdc_time_offset(ccache, credentials_version);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		err = cc_ccache_get_kdc_time_offset(ccache, credentials_version, &stored_offset);
		check_if(err != ccErrTimeOffsetNotSet, "time offset not cleared");
	}

	#endif /* cc_ccache_clear_kdc_time_offset */

	return err;
}
