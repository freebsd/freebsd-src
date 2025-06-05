/*
    simple_lock_test.c

    Initializes two contexts in two different threads and tries to get read locks on both at the same time.
    Hangs at line 24.
*/
#include <stdio.h>
#include <stdarg.h>

#include "test_ccapi_log.h"

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#include <TargetConditionals.h>
#endif

#ifdef TARGET_OS_MAC
#include <stdlib.h>
#include <pthread.h>
#include <Kerberos/CredentialsCache.h>
#else
#include "CredentialsCache.h"
#endif


void *other_thread (void) {
    cc_int32 err;
    cc_context_t context = NULL;

    err = cc_initialize(&context, ccapi_version_7, NULL, NULL);

    log_error("thread: attempting lock. may hang. err == %d", err);

    if (!err) {
        // hangs with cc_lock_read which should succeed immediately, but does not hang with write, upgrade, and downgrade, which fail immediately
        err = cc_context_lock(context, cc_lock_read, cc_lock_noblock);
    }

    if (context) {
        cc_context_unlock(context);
        cc_context_release(context);
        context = NULL;
    }
    log_error("thread: return. err == %d", err);
}


int main (int argc, char *argv[]) {
    cc_int32        err;
    int             status;
    cc_context_t    context = NULL;

#ifdef TARGET_OS_MAC
    pthread_t       thread_id;
#endif

    err = cc_initialize(&context, ccapi_version_7, NULL, NULL);
    if (!err) {
        err = cc_context_lock(context, cc_lock_read, cc_lock_noblock);
    }

    log_error("main: initialized and read locked context. err == %d", err);

#ifdef TARGET_OS_MAC
   status = pthread_create (&thread_id, NULL, (void *) other_thread, NULL);
    if (status != 0) {
        log_error("pthread_create() returned %d", status);
        exit(-1);
    }

    pthread_join(thread_id, NULL);
#else

#endif

    log_error("main: unlocking and releasing context. err == %d", err);

    if (context) {
        log_error("main: calling cc_context_unlock");
        cc_context_unlock(context);
        log_error("main: calling cc_context_release");
        cc_context_release(context);
        context = NULL;
    }

    log_error("main: return. err == %d", err);

#if defined(_WIN32)
    UNREFERENCED_PARAMETER(status);       // no whining!
#endif
    return 0;
}
