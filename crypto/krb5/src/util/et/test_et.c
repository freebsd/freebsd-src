/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "autoconf.h"
#include <stdio.h>
#include <errno.h>
#include "com_err.h"
#include "test1.h"
#include "test2.h"

#ifdef _WIN32
# define EXPORT_LIST
#endif

/* XXX Not part of official public API.  */
extern const char *error_table_name (errcode_t);

#ifdef NEED_SYS_ERRLIST
extern int sys_nerr;
#endif

int main()
{
    printf("Before initiating error table:\n\n");
#ifndef EXPORT_LIST
    printf("Table name '%s'\n", error_table_name(KRB_MK_AP_TGTEXP));
    printf("UNIX  name '%s'\n", error_table_name(EPERM));
#endif
    printf("Msg TGT-expired is '%s'\n", error_message(KRB_MK_AP_TGTEXP));
    printf("Msg EPERM is '%s'\n", error_message(EPERM));
    printf("Msg FOO_ERR is '%s'\n", error_message(FOO_ERR));
    printf("Msg 1002 is '%s'\n", error_message (1002));
#ifdef HAVE_SYS_ERRLIST
    printf("Msg {sys_nerr-1} is '%s'\n", error_message(sys_nerr-1));
    printf("Msg {sys_nerr} is '%s'\n", error_message(sys_nerr));
#endif
    printf("Msg 0 is '%s'\n", error_message(0));

    printf("With 0: tgt-expired -> %s\n", error_message(KRB_MK_AP_TGTEXP));

    initialize_krb_error_table();
#ifndef EXPORT_LIST
    printf("KRB error table initialized:  base %ld (%s), name %s\n",
           ERROR_TABLE_BASE_krb, error_message(ERROR_TABLE_BASE_krb),
           error_table_name(ERROR_TABLE_BASE_krb));
#else
    printf("KRB error table initialized:  base %ld (%s)\n",
           ERROR_TABLE_BASE_krb, error_message(ERROR_TABLE_BASE_krb));
#endif
    add_error_table(&et_krb_error_table);
    printf("With krb: tgt-expired -> %s\n",
           error_message(KRB_MK_AP_TGTEXP));

    add_error_table(&et_quux_error_table);
#ifndef EXPORT_LIST
    printf("QUUX error table initialized: base %ld (%s), name %s\n",
           ERROR_TABLE_BASE_quux, error_message(ERROR_TABLE_BASE_quux),
           error_table_name(ERROR_TABLE_BASE_quux));
#else
    printf("QUUX error table initialized: base %ld (%s)\n",
           ERROR_TABLE_BASE_quux, error_message(ERROR_TABLE_BASE_quux));
#endif

    printf("Msg for TGT-expired is '%s'\n",
           error_message(KRB_MK_AP_TGTEXP));
#ifdef HAVE_SYS_ERRLIST
    printf("Msg {sys_nerr-1} is '%s'\n", error_message(sys_nerr-1));
#endif
    printf("Msg FOO_ERR is '%s'\n", error_message(FOO_ERR));
    printf("Msg KRB_SKDC_CANT is '%s'\n",
           error_message(KRB_SKDC_CANT));
    printf("Msg 1e6 (8B 64) is '%s'\n", error_message(1000000));
    printf("\n\nCOM_ERR tests:\n");
    com_err("whoami", FOO_ERR, (char *)NULL);
    com_err("whoami", FOO_ERR, " -- message goes %s", "here");
    com_err("whoami", 0, (char *)0);
    com_err("whoami", 0, "error number %d\n", 0);
    return 0;
}
