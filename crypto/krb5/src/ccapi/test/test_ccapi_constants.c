#include "test_ccapi_constants.h"
#include "test_ccapi_globals.h"
#include "test_ccapi_check.h"

int check_constants(void) {
	BEGIN_TEST("constants");
	/* API versions */

	check_int(ccapi_version_2, 2);
	check_int(ccapi_version_3, 3);
	check_int(ccapi_version_4, 4);
	check_int(ccapi_version_5, 5);
	check_int(ccapi_version_6, 6);

	/* Errors */

	check_int(ccNoError 					  , 0  );	 //   0
	check_int(ccIteratorEnd 				  , 201);    // 201
	check_int(ccErrBadParam 			      , 202);    // 202
	check_int(ccErrNoMem 					  , 203);    // 203
	check_int(ccErrInvalidContext             , 204);    // 204
	check_int(ccErrInvalidCCache              , 205);    // 205
	check_int(ccErrInvalidString              , 206);    // 206
	check_int(ccErrInvalidCredentials         , 207);    // 207
	check_int(ccErrInvalidCCacheIterator      , 208);    // 208
	check_int(ccErrInvalidCredentialsIterator , 209);    // 209
	check_int(ccErrInvalidLock                , 210);    // 210
	check_int(ccErrBadName                    , 211);    // 211
	check_int(ccErrBadCredentialsVersion      , 212);    // 212
	check_int(ccErrBadAPIVersion              , 213);    // 213
	check_int(ccErrContextLocked              , 214);    // 214
	check_int(ccErrContextUnlocked            , 215);    // 215
	check_int(ccErrCCacheLocked               , 216);    // 216
	check_int(ccErrCCacheUnlocked             , 217);    // 217
	check_int(ccErrBadLockType                , 218);    // 218
	check_int(ccErrNeverDefault               , 219);    // 219
	check_int(ccErrCredentialsNotFound        , 220);    // 220
	check_int(ccErrCCacheNotFound             , 221);    // 221
	check_int(ccErrContextNotFound            , 222);    // 222
	check_int(ccErrServerUnavailable          , 223);    // 223
	check_int(ccErrServerInsecure             , 224);    // 224
	check_int(ccErrServerCantBecomeUID        , 225);    // 225
	check_int(ccErrTimeOffsetNotSet           , 226);    // 226
	check_int(ccErrBadInternalMessage         , 227);    // 227
	check_int(ccErrNotImplemented             , 228);    // 228

	/* Credentials versions */

	check_int(cc_credentials_v5,    2);

	/* Lock types */

	check_int(cc_lock_read,      0);
	check_int(cc_lock_write,     1);
	check_int(cc_lock_upgrade,   2);
	check_int(cc_lock_downgrade, 3);

    /* Locking Modes */

	check_int(cc_lock_noblock, 0);
	check_int(cc_lock_block,   1);

	END_TEST_AND_RETURN
}
