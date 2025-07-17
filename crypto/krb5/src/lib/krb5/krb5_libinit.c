/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "k5-int.h"

#if defined(_WIN32) || defined(USE_CCAPI)
#include "stdcc.h"
#endif

#include "krb5_libinit.h"
#include "k5-platform.h"
#include "cc-int.h"
#include "kt-int.h"
#include "os-proto.h"

/*
 * Initialize the Kerberos v5 library.
 */

MAKE_INIT_FUNCTION(krb5int_lib_init);
MAKE_FINI_FUNCTION(krb5int_lib_fini);

/* Possibly load-time initialization -- mutexes, etc.  */
int krb5int_lib_init(void)
{
    int err;

    k5_set_error_info_callout_fn(error_message);

#ifdef SHOW_INITFINI_FUNCS
    printf("krb5int_lib_init\n");
#endif

    add_error_table(&et_krb5_error_table);
    add_error_table(&et_k5e1_error_table);
    add_error_table(&et_kv5m_error_table);
    add_error_table(&et_kdb5_error_table);
    add_error_table(&et_asn1_error_table);
    add_error_table(&et_k524_error_table);

    bindtextdomain(KRB5_TEXTDOMAIN, LOCALEDIR);

#ifndef LEAN_CLIENT
    err = krb5int_kt_initialize();
    if (err)
        return err;
#endif /* LEAN_CLIENT */
    err = krb5int_cc_initialize();
    if (err)
        return err;
    err = k5_mutex_finish_init(&krb5int_us_time_mutex);
    if (err)
        return err;

    return 0;
}

/* Always-delayed initialization -- error table linkage, etc.  */
krb5_error_code krb5int_initialize_library (void)
{
    return CALL_INIT_FUNCTION(krb5int_lib_init);
}

/*
 * Clean up the Kerberos v5 library state
 */

void krb5int_lib_fini(void)
{
    if (!INITIALIZER_RAN(krb5int_lib_init) || PROGRAM_EXITING()) {
#ifdef SHOW_INITFINI_FUNCS
        printf("krb5int_lib_fini: skipping\n");
#endif
        return;
    }

#ifdef SHOW_INITFINI_FUNCS
    printf("krb5int_lib_fini\n");
#endif

    k5_mutex_destroy(&krb5int_us_time_mutex);

    krb5int_cc_finalize();
#ifndef LEAN_CLIENT
    krb5int_kt_finalize();
#endif /* LEAN_CLIENT */

#if defined(_WIN32) || defined(USE_CCAPI)
    krb5_stdcc_shutdown();
#endif

    remove_error_table(&et_krb5_error_table);
    remove_error_table(&et_k5e1_error_table);
    remove_error_table(&et_kv5m_error_table);
    remove_error_table(&et_kdb5_error_table);
    remove_error_table(&et_asn1_error_table);
    remove_error_table(&et_k524_error_table);

    k5_set_error_info_callout_fn(NULL);
}

/* Still exists because it went into the export list on Windows.  But
   since the above function should be invoked at unload time, we don't
   actually want to do anything here.  */
void krb5int_cleanup_library (void)
{
}
