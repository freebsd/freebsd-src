#include <string.h>
#include "test_ccapi_context.h"
#include <limits.h>
#include "test_ccapi_check.h"
#include "test_ccapi_util.h"

int check_cc_initialize(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;

	BEGIN_TEST("cc_initialize");

	// try every api_version
	err = check_once_cc_initialize(&context, ccapi_version_2, NULL, NULL, ccNoError, "cc_initialize with ccapi_version_2");   	   // err == CC_BAD_API_VERSION (9) would be imported by CredentialsCache2.h
	err = check_once_cc_initialize(&context, ccapi_version_3, NULL, NULL, ccNoError, "cc_initialize with ccapi_version_3");   	   // !err
	err = check_once_cc_initialize(&context, ccapi_version_4, NULL, NULL, ccNoError, "cc_initialize with ccapi_version_4");   	   //        "
	err = check_once_cc_initialize(&context, ccapi_version_5, NULL, NULL, ccNoError, "cc_initialize with ccapi_version_5");   	   //        "
	err = check_once_cc_initialize(&context, ccapi_version_6, NULL, NULL, ccNoError, "cc_initialize with ccapi_version_6");   	   //        "

	// try bad api_version
	err = check_once_cc_initialize(&context, INT_MAX,         NULL, NULL, ccErrBadAPIVersion, NULL); // err == ccErrBadAPIVersion

	// try bad param
	err = check_once_cc_initialize(NULL,     ccapi_version_3, NULL, NULL, ccErrBadParam, NULL);  	   // err == ccErrBadParam

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_initialize(cc_context_t *out_context, cc_int32 in_version, cc_int32 *out_supported_version, char const **out_vendor, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_context_t context;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrNoMem,
		ccErrBadAPIVersion,
		ccErrBadParam,
	};

    BEGIN_CHECK_ONCE(description);

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_initialize(out_context, in_version, out_supported_version, out_vendor);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (out_context) { context = *out_context; }
	else { context = NULL; }

	// check output parameters
	if (!err) {
		check_if(context == NULL, NULL);
		if (context) {
			cc_context_release(context);
			*out_context = NULL;
		}
	} else {
		check_if(context != NULL, NULL);
	}

	return err;
}

int check_cc_context_release(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;

	BEGIN_TEST("cc_context_release");

	#ifndef cc_context_release
	log_error("cc_context_release is not implemented yet");
	failure_count++;
	#else

	// try with valid context
	err = check_once_cc_context_release(&context, ccNoError, NULL);

	// try with NULL
	//err = check_once_cc_context_release(NULL, ccErrInvalidContext);
	/* calling with NULL context crashes, because this macro expands to
	   ((NULL) -> functions -> release (NULL)) which is dereferencing NULL which is bad. */

	if (context) { cc_context_release(context); }

	#endif /* cc_context_release */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_release(cc_context_t *out_context, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_context_t context = NULL;

	cc_int32 possible_return_values[2] = {
		ccNoError,
		ccErrInvalidContext,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_release

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (out_context) {
		err = cc_initialize(out_context, ccapi_version_3, NULL, NULL);
		if (!err) {
			context = *out_context;
		}
	}

	if (err != ccNoError) {
		log_error("failure in cc_initialize, unable to perform check");
		return err;
	}
	else {
		err = cc_context_release(context);
		// check returned error
		check_err(err, expected_err, possible_return_values);
	}

	*out_context = NULL;

	#endif /* cc_context_release */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_get_change_time(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_time_t last_change_time = 0;
	cc_ccache_t ccache = NULL;
	cc_credentials_union creds_union;
	cc_credentials_iterator_t creds_iterator = NULL;
	cc_credentials_t credentials = NULL;

	BEGIN_TEST("cc_context_get_change_time");

	#ifndef cc_context_get_change_time
	log_error("cc_context_get_change_time is not implemented yet");
	failure_count++;
	#else

	/*
	 * Make a context
	 * make sure the change time changes after:
	 * 	a ccache is created
	 * 	a ccache is destroyed
	 * 	a credential is stored
	 * 	a credential is removed
	 * 	a ccache principal is changed
	 * 	the default ccache is changed
	 * clean up memory
	 */

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {

		// try bad parameters first
		err = check_once_cc_context_get_change_time(context, NULL, ccErrBadParam, "NULL param, should fail");

		// make sure we have a default ccache
		err = cc_context_open_default_ccache(context, &ccache);
		if (err == ccErrCCacheNotFound) {
			err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo/bar@BAZ.ORG", &ccache);
		}
		if (!err) {
			err = cc_ccache_release(ccache);
		}
		// either the default ccache already existed or we just created it
		// either way, the get_change_time should now give something > 0
		check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "first-run, should be > 0");

		// create a ccache
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
		check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after creating a new ccache");

		// store a credential
		if (!err) {
			new_v5_creds_union(&creds_union, "BAR.ORG");
			err = cc_ccache_store_credentials(ccache, &creds_union);
			release_v5_creds_union(&creds_union);
		}
		check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after storing a credential");

		if (!err) {
			// change principal (fails with ccErrBadInternalMessage)
			err = cc_ccache_set_principal(ccache, cc_credentials_v5, "foo@BAR.ORG");
			if (err) {
				log_error("failed to change ccache's principal - %s (%d)", translate_ccapi_error(err), err);
				failure_count++;
				err = ccNoError;
			}
		}
		check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after changing a principle");

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
		check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after removing a credential");

		if (!err) {
			// change default ccache
			err = cc_ccache_set_default(ccache);
			check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after changing default ccache");
		}

		if (ccache) {
			// destroy a ccache
			err = cc_ccache_destroy(ccache);
			check_once_cc_context_get_change_time(context, &last_change_time, ccNoError, "after destroying a ccache");
		}
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_get_change_time */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_get_change_time(cc_context_t context, cc_time_t *time, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_time_t last_change_time;
	cc_time_t current_change_time = 0;

	cc_int32 possible_return_values[3] = {
		ccNoError,
		ccErrInvalidContext,
		ccErrBadParam,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_get_change_time

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (time != NULL) { // if we were passed NULL, then we're looking to pass a bad param
		err = cc_context_get_change_time(context, &current_change_time);
	} else {
		err = cc_context_get_change_time(context, NULL);
	}

	check_err(err, expected_err, possible_return_values);

	if (!err) {
		last_change_time = *time;
		check_if(current_change_time <= last_change_time, "context change time did not increase when it was supposed to (%d <= %d)", current_change_time, last_change_time);
		*time = current_change_time;
	}

	#endif /* cc_context_get_change_time */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_get_default_ccache_name(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_context_get_default_ccache_name");

	#ifndef cc_context_get_default_ccache_name
	log_error("cc_context_get_default_ccache_name is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// try bad parameters first
		err = check_once_cc_context_get_default_ccache_name(context, NULL, ccErrBadParam, NULL);

		// try with no default
		err = destroy_all_ccaches(context);
		err = cc_context_open_default_ccache(context, &ccache);
		if (err != ccErrCCacheNotFound) {
			log_error("didn't remove all ccaches");
		}
		err = check_once_cc_context_get_default_ccache_name(context, &name, ccNoError, NULL);

		// try normally
		err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
		if (ccache) { cc_ccache_release(ccache); }
		err = check_once_cc_context_get_default_ccache_name(context, &name, ccNoError, NULL);

	}

	if (context) { cc_context_release(context); }

	#endif /* cc_context_get_default_ccache_name */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_get_default_ccache_name(cc_context_t context, cc_string_t *name, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidContext,
		ccErrBadParam,
		ccErrNoMem,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_get_default_ccache_name

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (name != NULL) { // if we were passed NULL, then we're looking to pass a bad param
		err = cc_context_get_default_ccache_name(context, name);
	} else {
		err = cc_context_get_default_ccache_name(context, NULL);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	// not really anything else to check

	if (name && *name) { cc_string_release(*name); }

	#endif /* cc_context_get_default_ccache_name */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_open_ccache(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_context_open_ccache");

	#ifndef cc_context_open_ccache
	log_error("cc_context_open_ccache is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// make sure we have a default ccache
		err = cc_context_open_default_ccache(context, &ccache);
		if (err == ccErrCCacheNotFound) {
			err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo/bar@BAZ.ORG", &ccache);
		}
		if (!err) {
			err = cc_ccache_release(ccache);
			ccache = NULL;
		}

		// try default ccache
		err = cc_context_get_default_ccache_name(context, &name);
		if (!err) {
			err = check_once_cc_context_open_ccache(context, name->data, &ccache, ccNoError, NULL);
		}

		// try bad parameters
		err = check_once_cc_context_open_ccache(context, NULL, &ccache, ccErrBadParam, NULL);
		err = check_once_cc_context_open_ccache(context, name->data, NULL, ccErrBadParam, NULL);

		// try a ccache that doesn't exist (create one and then destroy it)
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
		if (!err) {
			err = cc_ccache_get_name(ccache, &name);
		}
		if (!err) {
			err = cc_ccache_destroy(ccache);
			ccache = NULL;
		}

		err = check_once_cc_context_open_ccache(context, name->data, &ccache, ccErrCCacheNotFound, NULL);
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_context_open_ccache */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_open_ccache(cc_context_t context, const char *name, cc_ccache_t *ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_string_t stored_name = NULL;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadName,
		ccErrInvalidContext,
		ccErrNoMem,
		ccErrCCacheNotFound,
		ccErrBadParam,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_open_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (ccache != NULL) { // if we were passed NULL, then we're looking to pass a bad param
		err = cc_context_open_ccache(context, name, ccache);
	} else {
		err = cc_context_open_ccache(context, name, NULL);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(*ccache == NULL, NULL);

		if (!err) {
			err = cc_ccache_get_name(*ccache, &stored_name);
		}
		if (!err) {
			check_if(strcmp(stored_name->data, name), NULL);
		}
		if (stored_name) { cc_string_release(stored_name); }


		if (ccache && *ccache) {
			cc_ccache_release(*ccache);
			*ccache = NULL;
		}
	}

	#endif /* cc_context_open_ccache */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_open_default_ccache(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;

	BEGIN_TEST("cc_context_open_default_ccache");

	#ifndef cc_context_open_default_ccache
	log_error("cc_context_open_default_ccache is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// make sure we have a default ccache
		err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo/bar@BAZ.ORG", &ccache);
		if (ccache) { cc_ccache_release(ccache); }

		// try default ccache
		if (!err) {
			err = check_once_cc_context_open_default_ccache(context, &ccache, ccNoError, NULL);
		}

		// try bad parameters
		err = check_once_cc_context_open_default_ccache(context, NULL, ccErrBadParam, NULL);

		// try with no default ccache (destroy all ccaches first)
		err = destroy_all_ccaches(context);

		err = check_once_cc_context_open_default_ccache(context, &ccache, ccErrCCacheNotFound, NULL);
	}

	if (context) { cc_context_release(context); }

	#endif /* cc_context_open_default_ccache */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_open_default_ccache(cc_context_t context, cc_ccache_t *ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_string_t given_name = NULL;
	cc_string_t default_name = NULL;

	cc_int32 possible_return_values[5] = {
		ccNoError,
		ccErrInvalidContext,
		ccErrNoMem,
		ccErrCCacheNotFound,
		ccErrBadParam,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_open_default_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	if (ccache != NULL) { // if we were passed NULL, then we're looking to pass a bad param
		err = cc_context_open_default_ccache(context, ccache);
	} else {
		err = cc_context_open_default_ccache(context, NULL);
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(*ccache == NULL, NULL);

		// make sure this ccache is the one we were looking to get back (compare name with cc_context_get_default_ccache_name)
		err = cc_ccache_get_name(*ccache, &given_name);
		err = cc_context_get_default_ccache_name(context, &default_name);
		if (given_name && default_name) {
			check_if(strcmp(given_name->data, default_name->data), "name of ccache returned by cc_context_open_default_ccache doesn't match name returned by cc_context_get_default_ccache_name");
		}
		if (given_name) { cc_string_release(given_name); }
		if (default_name) { cc_string_release(default_name); }

		if (ccache && *ccache) {
			cc_ccache_release(*ccache);
			*ccache = NULL;
		}
	}

	#endif /* cc_context_open_default_ccache */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_create_ccache(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_context_create_ccache");

	#ifndef cc_context_create_ccache
	log_error("cc_context_create_ccache is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// try making a ccache with a non-unique name (the existing default's name)
		if (!err) {
			err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo/bar@BAZ.ORG", &ccache);
		}
		if (!err) {
			err = cc_ccache_get_name(ccache, &name);
		}
		if (ccache) { cc_ccache_release(ccache); }
		if (!err) {
			err = check_once_cc_context_create_ccache(context, name->data, cc_credentials_v5, "foo@BAR.ORG", &ccache, ccNoError, NULL);
		}

		// try making a ccache with a unique name (the now destroyed default's name)
		if (ccache) { cc_ccache_destroy(ccache); }
		if (!err) {
			err = check_once_cc_context_create_ccache(context, name->data, cc_credentials_v5, "foo/baz@BAR.ORG", &ccache, ccNoError, NULL);
		}

		// try bad parameters
		err = check_once_cc_context_create_ccache(context, NULL, cc_credentials_v5, "foo@BAR.ORG", &ccache, ccErrBadParam, "NULL name");                    // NULL name
		err = check_once_cc_context_create_ccache(context, "name", cc_credentials_v4_v5, "foo@BAR.ORG", &ccache, ccErrBadCredentialsVersion, "invalid creds_vers"); // invalid creds_vers
		err = check_once_cc_context_create_ccache(context, "name", cc_credentials_v5, NULL, &ccache, ccErrBadParam, "NULL principal");                          // NULL principal
		err = check_once_cc_context_create_ccache(context, "name", cc_credentials_v5, "foo@BAR.ORG", NULL, ccErrBadParam, "NULL ccache");                    // NULL ccache
	}

	if (name) { cc_string_release(name); }
	if (ccache) { cc_ccache_destroy(ccache); }
	if (context) { cc_context_release(context); }

	#endif /* cc_context_create_ccache */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_create_ccache(cc_context_t context, const char *name, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_string_t stored_name = NULL;
	cc_string_t stored_principal = NULL;
	cc_uint32 stored_creds_vers = 0;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadName,
		ccErrBadParam,
		ccErrInvalidContext,
		ccErrNoMem,
		ccErrBadCredentialsVersion,
	};
	BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_create_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_context_create_ccache(context, name, cred_vers, principal, ccache);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		check_if(*ccache == NULL, NULL);

		// make sure all of the ccache's info matches what we gave it
		// name
		err = cc_ccache_get_name(*ccache, &stored_name);
		if (!err) { check_if(strcmp(stored_name->data, name), NULL); }
		if (stored_name) { cc_string_release(stored_name); }
		// cred_vers
		// FIXME Documented function name of cc_ccache_get_credentials_version is a typo.
		// FIXME Documented type of creds param the wrong signedness (should be unsigned) for cc_ccache_get_credentials_version, cc_context_create_ccache, cc_context_create_default_ccache, cc_context_create_new_ccache
		err = cc_ccache_get_credentials_version(*ccache, &stored_creds_vers);
		if (!err) { check_if(stored_creds_vers != cred_vers, NULL); }
		// principal
		err = cc_ccache_get_principal(*ccache, cc_credentials_v5, &stored_principal);
		if (!err) { check_if(strcmp(stored_principal->data, principal), NULL); }
		if (stored_principal) { cc_string_release(stored_principal); }

		if (ccache && *ccache) {
			cc_ccache_destroy(*ccache);
			*ccache = NULL;
		}
	}

	#endif /* cc_context_create_ccache */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_create_default_ccache(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_context_create_default_ccache");

	#ifndef cc_context_create_default_ccache
	log_error("cc_context_create_default_ccache is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// try making the default when there are no existing ccaches
		err = destroy_all_ccaches(context);
		if (!err) {
			err = check_once_cc_context_create_default_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache, ccNoError, NULL);
		}
		if (ccache) { cc_ccache_release(ccache); }

		// try making a new default when one already exists
		if (!err) {
			err = check_once_cc_context_create_default_ccache(context, cc_credentials_v5, "foo/baz@BAR.ORG", &ccache, ccNoError, NULL);
		}

		// try bad parameters
		err = check_once_cc_context_create_default_ccache(context, cc_credentials_v4_v5, "foo@BAR.ORG", &ccache, ccErrBadCredentialsVersion, "invalid creds_vers"); // invalid creds_vers
		err = check_once_cc_context_create_default_ccache(context, cc_credentials_v5, NULL, &ccache, ccErrBadParam, "NULL principal");                          // NULL principal
		err = check_once_cc_context_create_default_ccache(context, cc_credentials_v5, "foo@BAR.ORG", NULL, ccErrBadParam, "NULL ccache");                    // NULL ccache
	}

	if (name) { cc_string_release(name); }
	if (ccache) { cc_ccache_destroy(ccache); }
	if (context) { cc_context_release(context); }

	#endif /* cc_context_create_default_ccache */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_create_default_ccache(cc_context_t context, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_string_t stored_principal = NULL;
	cc_uint32 stored_creds_vers = 0;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadName, // how can this be possible when the name isn't a parameter?
		ccErrBadParam,
		ccErrInvalidContext,
		ccErrNoMem,
		ccErrBadCredentialsVersion,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_create_default_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_context_create_default_ccache(context, cred_vers, principal, ccache);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		if (ccache) { check_if(*ccache == NULL, NULL); }
		// make sure all of the ccache's info matches what we gave it
		// cred_vers
		err = cc_ccache_get_credentials_version(*ccache, &stored_creds_vers);
		if (!err) { check_if(stored_creds_vers != cred_vers, NULL); }
		// principal
		err = cc_ccache_get_principal(*ccache, cc_credentials_v5, &stored_principal);
		if (!err) { check_if(strcmp(stored_principal->data, principal), NULL); }
		if (stored_principal) { cc_string_release(stored_principal); }

		if (ccache && *ccache) {
			cc_ccache_release(*ccache);
			*ccache = NULL;
		}
	}

	#endif /* cc_context_create_default_ccache */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_create_new_ccache(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;

	BEGIN_TEST("cc_context_create_new_ccache");

	#ifndef cc_context_create_new_ccache
	log_error("cc_context_create_new_ccache is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		// try making when there are no existing ccaches (should have name of default)
		err = destroy_all_ccaches(context);
		if (!err) {
			err = check_once_cc_context_create_new_ccache(context, 1, cc_credentials_v5, "foo@BAR.ORG", &ccache, ccNoError, NULL);
		}
		if (ccache) { cc_ccache_release(ccache); }

		// try making a new ccache when one already exists (should not have name of default)
		if (!err) {
			err = check_once_cc_context_create_new_ccache(context, 0, cc_credentials_v5, "foo/baz@BAR.ORG", &ccache, ccNoError, NULL);
		}
		if (ccache) { cc_ccache_release(ccache); }

		// try bad parameters
		err = check_once_cc_context_create_new_ccache(context, 1, cc_credentials_v4_v5, "foo@BAR.ORG", &ccache, ccErrBadCredentialsVersion, "invalid creds_vers"); // invalid creds_vers
		err = check_once_cc_context_create_new_ccache(context, 1, cc_credentials_v5, NULL, &ccache, ccErrBadParam, "NULL principal");                          // NULL principal
		err = check_once_cc_context_create_new_ccache(context, 1, cc_credentials_v5, "foo@BAR.ORG", NULL, ccErrBadParam, "NULL ccache");                    // NULL ccache
	}

	if (name) { cc_string_release(name); }
	if (ccache) { cc_ccache_destroy(ccache); }
	if (context) { cc_context_release(context); }

	#endif /* cc_context_create_new_ccache */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_create_new_ccache(cc_context_t context, cc_int32 should_be_default, cc_uint32 cred_vers, const char *principal, cc_ccache_t *ccache, cc_int32 expected_err, const char *description) {
	cc_int32 err = 0;
	cc_string_t name = NULL;
	cc_string_t stored_name = NULL;
	cc_string_t stored_principal = NULL;
	cc_uint32 stored_creds_vers = 0;

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccErrBadName, // how can this be possible when the name isn't a parameter?
		ccErrBadParam,
		ccErrInvalidContext,
		ccErrNoMem,
		ccErrBadCredentialsVersion,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_create_new_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_context_create_new_ccache(context, cred_vers, principal, ccache);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	if (!err) {
		if (ccache) { check_if(*ccache == NULL, NULL); }
		// make sure all of the ccache's info matches what we gave it
		if (!err) {
			err = cc_context_get_default_ccache_name(context, &name);
		}
		if (!err) {
			err = cc_ccache_get_name(*ccache, &stored_name);
		}
		if (!err) {
			if (should_be_default) {
				check_if(strcmp(stored_name->data, name->data), "new ccache does not have name of default");
			}
			else {
				check_if((strcmp(stored_name->data, name->data) == 0), "new cache has name of default");
			}
		}
		if (name) { cc_string_release(name); }
		if (stored_name) { cc_string_release(stored_name); }

		// cred_vers
		err = cc_ccache_get_credentials_version(*ccache, &stored_creds_vers);
		if (!err) { check_if(stored_creds_vers != cred_vers, NULL); }
		// principal
		err = cc_ccache_get_principal(*ccache, cc_credentials_v5, &stored_principal);
		if (!err) { check_if(strcmp(stored_principal->data, principal), NULL); }
		if (stored_principal) { cc_string_release(stored_principal); }

		if (ccache && *ccache) {
			cc_ccache_release(*ccache);
			*ccache = NULL;
		}
	}

	#endif /* cc_context_create_new_ccache */

	END_CHECK_ONCE;

	return err;
}

int check_cc_context_new_ccache_iterator(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_string_t name = NULL;
	cc_ccache_iterator_t iterator = NULL;

	BEGIN_TEST("cc_context_new_ccache_iterator");

	#ifndef cc_context_new_ccache_iterator
	log_error("cc_context_new_ccache_iterator is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);
	if (!err) {
		err = destroy_all_ccaches(context);
	}
	if (!err) {
		// try making when there are no existing ccaches (shouldn't make a difference, but just in case)
		check_once_cc_context_new_ccache_iterator(context, &iterator, ccNoError, "when there are no existing ccaches");

		err = cc_context_create_default_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		// try making when at least one ccache already exists (just to cover all our bases)
		check_once_cc_context_new_ccache_iterator(context, &iterator, ccNoError, "when at least one ccache already exists");

		// try bad parameters
		check_once_cc_context_new_ccache_iterator(context, NULL, ccErrBadParam, "NULL param"); // NULL iterator
	}
		// we'll do a comprehensive test of cc_ccache_iterator related functions later in the test suite

	if (name) { cc_string_release(name); }
	if (ccache) { cc_ccache_destroy(ccache); }
	if (context) { cc_context_release(context); }

	#endif /* cc_context_new_ccache_iterator */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_new_ccache_iterator(cc_context_t context, cc_ccache_iterator_t *iterator, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrBadParam,
		ccErrNoMem,
		ccErrInvalidContext,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_create_new_ccache

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_context_new_ccache_iterator(context, iterator);

	// check returned error
	check_err(err, expected_err, possible_return_values);

	// we'll do a comprehensive test of cc_ccache_iterator related functions later

	#endif /* cc_context_create_new_ccache */

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_context_compare(void) {
	cc_int32 err = 0;
	cc_context_t context_a = NULL;
	cc_context_t context_b = NULL;
	cc_uint32 equal = 0;

	BEGIN_TEST("cc_context_compare");

	#ifndef cc_context_compare
	log_error("cc_context_compare is not implemented yet");
	failure_count++;
	#else

	err = cc_initialize(&context_a, ccapi_version_3, NULL, NULL);
	if (!err) {
		err = cc_initialize(&context_b, ccapi_version_3, NULL, NULL);
	}

	check_once_cc_context_compare(context_a, context_a, &equal, ccNoError, "valid params, same contexts");
	check_once_cc_context_compare(context_a, context_b, &equal, ccNoError, "valid params, different contexts");
	check_once_cc_context_compare(context_a, NULL, &equal, ccErrBadParam, "NULL compare_to context");
	check_once_cc_context_compare(context_a, context_b, NULL, ccErrBadParam, "NULL out param");

	if (context_a) { cc_context_release(context_a); }
	if (context_b) { cc_context_release(context_b); }

	#endif /* cc_context_compare */

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_context_compare(cc_context_t context, cc_context_t compare_to, cc_uint32 *equal, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

	cc_int32 possible_return_values[4] = {
		ccNoError,
		ccErrInvalidContext,
		ccErrBadParam,
		ccErrServerUnavailable,
	};

    BEGIN_CHECK_ONCE(description);

	#ifdef cc_context_compare

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	err = cc_context_compare(context, compare_to, equal);

	if (!err) {
		*equal = 0;
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	#endif /* cc_context_compare */

	return err;
}
