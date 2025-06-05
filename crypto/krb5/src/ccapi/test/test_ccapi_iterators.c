#include "test_ccapi_globals.h"
#include "test_ccapi_iterators.h"
#include "test_ccapi_check.h"
#include "test_ccapi_util.h"

// ---------------------------------------------------------------------------

int check_cc_ccache_iterator_next(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_ccache_iterator_t iterator = NULL;
	unsigned int i;

    BEGIN_TEST("cc_ccache_iterator_next");

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// iterate with no ccaches
	if (!err) {
		err = cc_context_new_ccache_iterator(context, &iterator);
	}
	check_once_cc_ccache_iterator_next(iterator, 0, ccNoError, "iterating over an empty collection");
	if (iterator) {
		cc_ccache_iterator_release(iterator);
		iterator = NULL;
	}

	// iterate with one ccache
	if (!err) {
		destroy_all_ccaches(context);
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}
	if (!err) {
		err = cc_context_new_ccache_iterator(context, &iterator);
	}
	check_once_cc_ccache_iterator_next(iterator, 1, ccNoError, "iterating over a collection of 1 ccache");
	if (iterator) {
		cc_ccache_iterator_release(iterator);
		iterator = NULL;
	}

	// iterate with several ccaches
	if (!err) {
		destroy_all_ccaches(context);
	}
	for(i = 0; !err && (i < 1000); i++)
	{
        if (i%100 == 0) fprintf(stdout, ".");
        err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
		if (ccache) {
			cc_ccache_release(ccache);
			ccache = NULL;
		}
	}
	if (!err) {
		err = cc_context_new_ccache_iterator(context, &iterator);
	}
	check_once_cc_ccache_iterator_next(iterator, 1000, ccNoError, "iterating over a collection of 1000 ccache");
	if (iterator) {
		cc_ccache_iterator_release(iterator);
		iterator = NULL;
	}


	if (ccache) { cc_ccache_release(ccache); }
	if (iterator) { cc_ccache_iterator_release(iterator); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_ccache_iterator_next(cc_ccache_iterator_t iterator, cc_uint32 expected_count, cc_int32 expected_err, const char *description) {
	cc_int32 err = ccNoError;

//	BEGIN_CHECK_ONCE(description);

	cc_int32 possible_return_values[6] = {
		ccNoError,
		ccIteratorEnd,
		ccErrBadParam,
		ccErrNoMem,
		ccErrInvalidCCacheIterator,
		ccErrCCacheNotFound,
	};
	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	cc_ccache_t ccache = NULL;
	cc_uint32 actual_count = 0;

	while (!err) {
		err = cc_ccache_iterator_next(iterator, &ccache);
		if (ccache) {
			actual_count++;
			cc_ccache_release(ccache);
			ccache = NULL;
		}
	}
	if (err == ccIteratorEnd) {
		err = ccNoError;
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	check_if(actual_count != expected_count, "iterator didn't iterate over all ccaches");

//	END_CHECK_ONCE;

	return err;
}


// ---------------------------------------------------------------------------

int check_cc_credentials_iterator_next(void) {
	cc_int32 err = 0;
	cc_context_t context = NULL;
	cc_ccache_t ccache = NULL;
	cc_credentials_union creds_union;
	cc_credentials_iterator_t iterator = NULL;
	unsigned int i;

	BEGIN_TEST("cc_credentials_iterator_next");

	err = cc_initialize(&context, ccapi_version_3, NULL, NULL);

	if (!err) {
		err = destroy_all_ccaches(context);
	}

	// iterate with no creds
	if (!err) {
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &iterator);
	}
	check_once_cc_credentials_iterator_next(iterator, 0, ccNoError, "iterating over an empty ccache");
	if (iterator) {
		cc_ccache_iterator_release(iterator);
		iterator = NULL;
	}
	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}

	// iterate with one cred
	if (!err) {
		destroy_all_ccaches(context);
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	if (!err) {
		new_v5_creds_union(&creds_union, "BAR.ORG");
		err = cc_ccache_store_credentials(ccache, &creds_union);
		release_v5_creds_union(&creds_union);
	}
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &iterator);
	}
	check_once_cc_credentials_iterator_next(iterator, 1, ccNoError, "iterating over a ccache with 1 cred");
	if (iterator) {
		cc_credentials_iterator_release(iterator);
		iterator = NULL;
	}
	if (ccache) {
		cc_ccache_release(ccache);
		ccache = NULL;
	}

	// iterate with several creds
	if (!err) {
		destroy_all_ccaches(context);
		err = cc_context_create_new_ccache(context, cc_credentials_v5, "foo@BAR.ORG", &ccache);
	}
	for(i = 0; !err && (i < 1000); i++) {
        if (i%100 == 0) fprintf(stdout, ".");
		new_v5_creds_union(&creds_union, "BAR.ORG");
		err = cc_ccache_store_credentials(ccache, &creds_union);
		release_v5_creds_union(&creds_union);
	}
	if (!err) {
		err = cc_ccache_new_credentials_iterator(ccache, &iterator);
	}
	check_once_cc_credentials_iterator_next(iterator, 1000, ccNoError, "iterating over a ccache with 1000 creds");

	if (ccache) { cc_ccache_release(ccache); }
	if (iterator) { cc_credentials_iterator_release(iterator); }
	if (context) {
		destroy_all_ccaches(context);
		cc_context_release(context);
	}

	END_TEST_AND_RETURN
}

cc_int32 check_once_cc_credentials_iterator_next(cc_credentials_iterator_t iterator, cc_uint32 expected_count, cc_int32 expected_err, const char *description) {
	cc_int32            err             = ccNoError;
	cc_credentials_t    creds           = NULL;
	cc_uint32           actual_count    = 0;

	cc_int32 possible_return_values[5] = {
		ccNoError,
		ccIteratorEnd,
		ccErrBadParam,
		ccErrNoMem,
		ccErrInvalidCredentialsIterator,
	};

    BEGIN_CHECK_ONCE(description);

	#define possible_ret_val_count sizeof(possible_return_values)/sizeof(possible_return_values[0])

	while (!err) {
		err = cc_credentials_iterator_next(iterator, &creds);
		if (creds) {
			actual_count++;
			cc_credentials_release(creds);
			creds = NULL;
		}
	}
	if (err == ccIteratorEnd) {
		err = ccNoError;
	}

	// check returned error
	check_err(err, expected_err, possible_return_values);

	check_if(actual_count != expected_count, "iterator didn't iterate over all ccaches");

	END_CHECK_ONCE;

	return err;
}
