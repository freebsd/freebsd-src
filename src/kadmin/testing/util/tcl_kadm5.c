/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "autoconf.h"
#include <stdio.h>
#include <string.h>
#if HAVE_TCL_H
#include <tcl.h>
#elif HAVE_TCL_TCL_H
#include <tcl/tcl.h>
#endif
#define USE_KADM5_API_VERSION 2
#include <kadm5/admin.h>
#include <com_err.h>
#include <errno.h>
#include <stdlib.h>
#include <adb_err.h>
#include "tcl_kadm5.h"

struct flagval {
    char *name;
    krb5_flags val;
};

/* XXX This should probably be in the hash table like server_handle */
static krb5_context context;

static struct flagval krb5_flags_array[] = {
    {"KRB5_KDB_DISALLOW_POSTDATED", KRB5_KDB_DISALLOW_POSTDATED},
    {"KRB5_KDB_DISALLOW_FORWARDABLE", KRB5_KDB_DISALLOW_FORWARDABLE},
    {"KRB5_KDB_DISALLOW_TGT_BASED", KRB5_KDB_DISALLOW_TGT_BASED},
    {"KRB5_KDB_DISALLOW_RENEWABLE", KRB5_KDB_DISALLOW_RENEWABLE},
    {"KRB5_KDB_DISALLOW_PROXIABLE", KRB5_KDB_DISALLOW_PROXIABLE},
    {"KRB5_KDB_DISALLOW_DUP_SKEY", KRB5_KDB_DISALLOW_DUP_SKEY},
    {"KRB5_KDB_DISALLOW_ALL_TIX", KRB5_KDB_DISALLOW_ALL_TIX},
    {"KRB5_KDB_REQUIRES_PRE_AUTH", KRB5_KDB_REQUIRES_PRE_AUTH},
    {"KRB5_KDB_REQUIRES_HW_AUTH", KRB5_KDB_REQUIRES_HW_AUTH},
    {"KRB5_KDB_REQUIRES_PWCHANGE", KRB5_KDB_REQUIRES_PWCHANGE},
    {"KRB5_KDB_DISALLOW_SVR", KRB5_KDB_DISALLOW_SVR},
    {"KRB5_KDB_PWCHANGE_SERVICE", KRB5_KDB_PWCHANGE_SERVICE}
};

static struct flagval aux_attributes[] = {
    {"KADM5_POLICY",   KADM5_POLICY}
};

static struct flagval principal_mask_flags[] = {
    {"KADM5_PRINCIPAL", KADM5_PRINCIPAL},
    {"KADM5_PRINC_EXPIRE_TIME", KADM5_PRINC_EXPIRE_TIME},
    {"KADM5_PW_EXPIRATION", KADM5_PW_EXPIRATION},
    {"KADM5_LAST_PWD_CHANGE", KADM5_LAST_PWD_CHANGE},
    {"KADM5_ATTRIBUTES", KADM5_ATTRIBUTES},
    {"KADM5_MAX_LIFE", KADM5_MAX_LIFE},
    {"KADM5_MOD_TIME", KADM5_MOD_TIME},
    {"KADM5_MOD_NAME", KADM5_MOD_NAME},
    {"KADM5_KVNO", KADM5_KVNO},
    {"KADM5_MKVNO", KADM5_MKVNO},
    {"KADM5_AUX_ATTRIBUTES", KADM5_AUX_ATTRIBUTES},
    {"KADM5_POLICY", KADM5_POLICY},
    {"KADM5_POLICY_CLR", KADM5_POLICY_CLR},
    {"KADM5_MAX_RLIFE", KADM5_MAX_RLIFE},
    {"KADM5_LAST_SUCCESS", KADM5_LAST_SUCCESS},
    {"KADM5_LAST_FAILED", KADM5_LAST_FAILED},
    {"KADM5_FAIL_AUTH_COUNT", KADM5_FAIL_AUTH_COUNT},
    {"KADM5_KEY_DATA", KADM5_KEY_DATA},
    {"KADM5_TL_DATA", KADM5_TL_DATA},
    {"KADM5_PRINCIPAL_NORMAL_MASK", KADM5_PRINCIPAL_NORMAL_MASK}
};

static struct flagval policy_mask_flags[] = {
    {"KADM5_POLICY", KADM5_POLICY},
    {"KADM5_PW_MAX_LIFE", KADM5_PW_MAX_LIFE},
    {"KADM5_PW_MIN_LIFE", KADM5_PW_MIN_LIFE},
    {"KADM5_PW_MIN_LENGTH", KADM5_PW_MIN_LENGTH},
    {"KADM5_PW_MIN_CLASSES", KADM5_PW_MIN_CLASSES},
    {"KADM5_PW_HISTORY_NUM", KADM5_PW_HISTORY_NUM},
    {"KADM5_REF_COUNT", KADM5_REF_COUNT},
    {"KADM5_PW_MAX_FAILURE", KADM5_PW_MAX_FAILURE},
    {"KADM5_PW_FAILURE_COUNT_INTERVAL", KADM5_PW_FAILURE_COUNT_INTERVAL},
    {"KADM5_PW_LOCKOUT_DURATION", KADM5_PW_LOCKOUT_DURATION},
};

static struct flagval config_mask_flags[] = {
    {"KADM5_CONFIG_REALM", KADM5_CONFIG_REALM},
    {"KADM5_CONFIG_DBNAME", KADM5_CONFIG_DBNAME},
    {"KADM5_CONFIG_MKEY_NAME", KADM5_CONFIG_MKEY_NAME},
    {"KADM5_CONFIG_MAX_LIFE", KADM5_CONFIG_MAX_LIFE},
    {"KADM5_CONFIG_MAX_RLIFE", KADM5_CONFIG_MAX_RLIFE},
    {"KADM5_CONFIG_EXPIRATION", KADM5_CONFIG_EXPIRATION},
    {"KADM5_CONFIG_FLAGS", KADM5_CONFIG_FLAGS},
    {"KADM5_CONFIG_STASH_FILE", KADM5_CONFIG_STASH_FILE},
    {"KADM5_CONFIG_ENCTYPE", KADM5_CONFIG_ENCTYPE},
    {"KADM5_CONFIG_ADBNAME", KADM5_CONFIG_ADBNAME},
    {"KADM5_CONFIG_ADB_LOCKFILE", KADM5_CONFIG_ADB_LOCKFILE},
    {"KADM5_CONFIG_ACL_FILE", KADM5_CONFIG_ACL_FILE},
    {"KADM5_CONFIG_KADMIND_PORT", KADM5_CONFIG_KADMIND_PORT},
    {"KADM5_CONFIG_ENCTYPES", KADM5_CONFIG_ENCTYPES},
    {"KADM5_CONFIG_ADMIN_SERVER", KADM5_CONFIG_ADMIN_SERVER},
    {"KADM5_CONFIG_DICT_FILE", KADM5_CONFIG_DICT_FILE},
    {"KADM5_CONFIG_MKEY_FROM_KBD", KADM5_CONFIG_MKEY_FROM_KBD},
};

static struct flagval priv_flags[] = {
    {"KADM5_PRIV_GET", KADM5_PRIV_GET},
    {"KADM5_PRIV_ADD", KADM5_PRIV_ADD},
    {"KADM5_PRIV_MODIFY", KADM5_PRIV_MODIFY},
    {"KADM5_PRIV_DELETE", KADM5_PRIV_DELETE}
};


static char *arg_error = "wrong # args";

static Tcl_HashTable *struct_table = 0;

static int put_server_handle(Tcl_Interp *interp, void *handle, char **name)
{
    int i = 1, newPtr = 0;
    static char buf[20];
    Tcl_HashEntry *entry;

    if (! struct_table) {
        if (! (struct_table =
               malloc(sizeof(*struct_table)))) {
            fprintf(stderr, "Out of memory!\n");
            exit(1); /* XXX */
        }
        Tcl_InitHashTable(struct_table, TCL_STRING_KEYS);
    }

    do {
        sprintf(buf, "kadm5_handle%d", i);
        entry = Tcl_CreateHashEntry(struct_table, buf, &newPtr);
        i++;
    } while (! newPtr);

    Tcl_SetHashValue(entry, handle);

    *name = buf;

    return TCL_OK;
}

static int get_server_handle(Tcl_Interp *interp, const char *name,
                             void **handle)
{
    Tcl_HashEntry *entry;

    if(!strcasecmp(name, "null"))
        *handle = 0;
    else {
        if (! (struct_table &&
               (entry = Tcl_FindHashEntry(struct_table, name)))) {
            Tcl_AppendResult(interp, "unknown server handle ", name, 0);
            return TCL_ERROR;
        }
        *handle = (void *) Tcl_GetHashValue(entry);
    }
    return TCL_OK;
}

static int remove_server_handle(Tcl_Interp *interp, const char *name)
{
    Tcl_HashEntry *entry;

    if (! (struct_table &&
           (entry = Tcl_FindHashEntry(struct_table, name)))) {
        Tcl_AppendResult(interp, "unknown server handle ", name, 0);
        return TCL_ERROR;
    }

    Tcl_SetHashValue(entry, NULL);
    return TCL_OK;
}

#define GET_HANDLE(num_args, ignored)                                   \
    void *server_handle;                                                \
    const char *whoami = argv[0];                                       \
    argv++, argc--;                                                     \
    if (argc != num_args + 1) {                                         \
        Tcl_AppendResult(interp, whoami, ": ", arg_error, 0);           \
        return TCL_ERROR;                                               \
    }                                                                   \
    {                                                                   \
        int ltcl_ret;                                                   \
        if ((ltcl_ret = get_server_handle(interp, argv[0], &server_handle)) \
            != TCL_OK) {                                                \
            return ltcl_ret;                                            \
        }                                                               \
    }                                                                   \
    argv++, argc--;

static Tcl_HashTable *create_flag_table(struct flagval *flags, int size)
{
    Tcl_HashTable *table;
    Tcl_HashEntry *entry;
    int i;

    if (! (table = (Tcl_HashTable *) malloc(sizeof(Tcl_HashTable)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_InitHashTable(table, TCL_STRING_KEYS);

    for (i = 0; i < size; i++) {
        int newPtr;

        if (! (entry = Tcl_CreateHashEntry(table, flags[i].name, &newPtr))) {
            fprintf(stderr, "Out of memory!\n");
            exit(1); /* XXX */
        }

        Tcl_SetHashValue(entry, &flags[i].val);
    }

    return table;
}


static Tcl_DString *unparse_str(char *in_str)
{
    Tcl_DString *str;

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    if (! in_str) {
        Tcl_DStringAppend(str, "null", -1);
    }
    else {
        Tcl_DStringAppend(str, in_str, -1);
    }

    return str;
}



static int parse_str(Tcl_Interp *interp, const char *in_str, char **out_str)
{
    if (! in_str) {
        *out_str = 0;
    }
    else if (! strcasecmp(in_str, "null")) {
        *out_str = 0;
    }
    else {
        *out_str = (char *) in_str;
    }
    return TCL_OK;
}


static void set_ok(Tcl_Interp *interp, char *string)
{
    Tcl_SetResult(interp, "OK", TCL_STATIC);
    Tcl_AppendElement(interp, "KADM5_OK");
    Tcl_AppendElement(interp, string);
}



static Tcl_DString *unparse_err(kadm5_ret_t code)
{
    char *code_string;
    const char *error_string;
    Tcl_DString *dstring;

    switch (code) {
    case KADM5_FAILURE: code_string = "KADM5_FAILURE"; break;
    case KADM5_AUTH_GET: code_string = "KADM5_AUTH_GET"; break;
    case KADM5_AUTH_ADD: code_string = "KADM5_AUTH_ADD"; break;
    case KADM5_AUTH_MODIFY:
        code_string = "KADM5_AUTH_MODIFY"; break;
    case KADM5_AUTH_DELETE:
        code_string = "KADM5_AUTH_DELETE"; break;
    case KADM5_AUTH_INSUFFICIENT:
        code_string = "KADM5_AUTH_INSUFFICIENT"; break;
    case KADM5_BAD_DB: code_string = "KADM5_BAD_DB"; break;
    case KADM5_DUP: code_string = "KADM5_DUP"; break;
    case KADM5_RPC_ERROR: code_string = "KADM5_RPC_ERROR"; break;
    case KADM5_NO_SRV: code_string = "KADM5_NO_SRV"; break;
    case KADM5_BAD_HIST_KEY:
        code_string = "KADM5_BAD_HIST_KEY"; break;
    case KADM5_NOT_INIT: code_string = "KADM5_NOT_INIT"; break;
    case KADM5_INIT: code_string = "KADM5_INIT"; break;
    case KADM5_BAD_PASSWORD:
        code_string = "KADM5_BAD_PASSWORD"; break;
    case KADM5_UNK_PRINC: code_string = "KADM5_UNK_PRINC"; break;
    case KADM5_UNK_POLICY: code_string = "KADM5_UNK_POLICY"; break;
    case KADM5_BAD_MASK: code_string = "KADM5_BAD_MASK"; break;
    case KADM5_BAD_CLASS: code_string = "KADM5_BAD_CLASS"; break;
    case KADM5_BAD_LENGTH: code_string = "KADM5_BAD_LENGTH"; break;
    case KADM5_BAD_POLICY: code_string = "KADM5_BAD_POLICY"; break;
    case KADM5_BAD_HISTORY: code_string = "KADM5_BAD_HISTORY"; break;
    case KADM5_BAD_PRINCIPAL:
        code_string = "KADM5_BAD_PRINCIPAL"; break;
    case KADM5_BAD_AUX_ATTR:
        code_string = "KADM5_BAD_AUX_ATTR"; break;
    case KADM5_PASS_Q_TOOSHORT:
        code_string = "KADM5_PASS_Q_TOOSHORT"; break;
    case KADM5_PASS_Q_CLASS:
        code_string = "KADM5_PASS_Q_CLASS"; break;
    case KADM5_PASS_Q_DICT:
        code_string = "KADM5_PASS_Q_DICT"; break;
    case KADM5_PASS_REUSE: code_string = "KADM5_PASS_REUSE"; break;
    case KADM5_PASS_TOOSOON:
        code_string = "KADM5_PASS_TOOSOON"; break;
    case KADM5_POLICY_REF:
        code_string = "KADM5_POLICY_REF"; break;
    case KADM5_PROTECT_PRINCIPAL:
        code_string = "KADM5_PROTECT_PRINCIPAL"; break;
    case KADM5_BAD_SERVER_HANDLE:
        code_string = "KADM5_BAD_SERVER_HANDLE"; break;
    case KADM5_BAD_STRUCT_VERSION:
        code_string = "KADM5_BAD_STRUCT_VERSION"; break;
    case KADM5_OLD_STRUCT_VERSION:
        code_string = "KADM5_OLD_STRUCT_VERSION"; break;
    case KADM5_NEW_STRUCT_VERSION:
        code_string = "KADM5_NEW_STRUCT_VERSION"; break;
    case KADM5_BAD_API_VERSION:
        code_string = "KADM5_BAD_API_VERSION"; break;
    case KADM5_OLD_LIB_API_VERSION:
        code_string = "KADM5_OLD_LIB_API_VERSION"; break;
    case KADM5_OLD_SERVER_API_VERSION:
        code_string = "KADM5_OLD_SERVER_API_VERSION"; break;
    case KADM5_NEW_LIB_API_VERSION:
        code_string = "KADM5_NEW_LIB_API_VERSION"; break;
    case KADM5_NEW_SERVER_API_VERSION:
        code_string = "KADM5_NEW_SERVER_API_VERSION"; break;
    case KADM5_SECURE_PRINC_MISSING:
        code_string = "KADM5_SECURE_PRINC_MISSING"; break;
    case KADM5_NO_RENAME_SALT:
        code_string = "KADM5_NO_RENAME_SALT"; break;
    case KADM5_BAD_CLIENT_PARAMS:
        code_string = "KADM5_BAD_CLIENT_PARAMS"; break;
    case KADM5_BAD_SERVER_PARAMS:
        code_string = "KADM5_BAD_SERVER_PARAMS"; break;
    case KADM5_AUTH_LIST:
        code_string = "KADM5_AUTH_LIST"; break;
    case KADM5_AUTH_CHANGEPW:
        code_string = "KADM5_AUTH_CHANGEPW"; break;
    case KADM5_GSS_ERROR: code_string = "KADM5_GSS_ERROR"; break;
    case KADM5_BAD_TL_TYPE: code_string = "KADM5_BAD_TL_TYPE"; break;
    case KADM5_MISSING_CONF_PARAMS:
        code_string = "KADM5_MISSING_CONF_PARAMS"; break;
    case KADM5_BAD_SERVER_NAME:
        code_string = "KADM5_BAD_SERVER_NAME"; break;
    case KADM5_MISSING_KRB5_CONF_PARAMS:
        code_string = "KADM5_MISSING_KRB5_CONF_PARAMS"; break;
    case KADM5_XDR_FAILURE: code_string = "KADM5_XDR_FAILURE"; break;
    case KADM5_CANT_RESOLVE: code_string = "KADM5_CANT_RESOLVE"; break;


    case OSA_ADB_DUP: code_string = "OSA_ADB_DUP"; break;
    case OSA_ADB_NOENT: code_string = "ENOENT"; break;
    case OSA_ADB_DBINIT: code_string = "OSA_ADB_DBINIT"; break;
    case OSA_ADB_BAD_POLICY: code_string = "Bad policy name"; break;
    case OSA_ADB_BAD_PRINC: code_string = "Bad principal name"; break;
    case OSA_ADB_BAD_DB: code_string = "Invalid database."; break;
    case OSA_ADB_XDR_FAILURE: code_string = "OSA_ADB_XDR_FAILURE"; break;
    case OSA_ADB_BADLOCKMODE: code_string = "OSA_ADB_BADLOCKMODE"; break;
    case OSA_ADB_CANTLOCK_DB: code_string = "OSA_ADB_CANTLOCK_DB"; break;
    case OSA_ADB_NOTLOCKED: code_string = "OSA_ADB_NOTLOCKED"; break;
    case OSA_ADB_NOLOCKFILE: code_string = "OSA_ADB_NOLOCKFILE"; break;
    case OSA_ADB_NOEXCL_PERM: code_string = "OSA_ADB_NOEXCL_PERM"; break;

    case KRB5_KDB_INUSE: code_string = "KRB5_KDB_INUSE"; break;
    case KRB5_KDB_UK_SERROR: code_string = "KRB5_KDB_UK_SERROR"; break;
    case KRB5_KDB_UK_RERROR: code_string = "KRB5_KDB_UK_RERROR"; break;
    case KRB5_KDB_UNAUTH: code_string = "KRB5_KDB_UNAUTH"; break;
    case KRB5_KDB_NOENTRY: code_string = "KRB5_KDB_NOENTRY"; break;
    case KRB5_KDB_ILL_WILDCARD: code_string = "KRB5_KDB_ILL_WILDCARD"; break;
    case KRB5_KDB_DB_INUSE: code_string = "KRB5_KDB_DB_INUSE"; break;
    case KRB5_KDB_DB_CHANGED: code_string = "KRB5_KDB_DB_CHANGED"; break;
    case KRB5_KDB_TRUNCATED_RECORD:
        code_string = "KRB5_KDB_TRUNCATED_RECORD"; break;
    case KRB5_KDB_RECURSIVELOCK:
        code_string = "KRB5_KDB_RECURSIVELOCK"; break;
    case KRB5_KDB_NOTLOCKED: code_string = "KRB5_KDB_NOTLOCKED"; break;
    case KRB5_KDB_BADLOCKMODE: code_string = "KRB5_KDB_BADLOCKMODE"; break;
    case KRB5_KDB_DBNOTINITED: code_string = "KRB5_KDB_DBNOTINITED"; break;
    case KRB5_KDB_DBINITED: code_string = "KRB5_KDB_DBINITED"; break;
    case KRB5_KDB_ILLDIRECTION: code_string = "KRB5_KDB_ILLDIRECTION"; break;
    case KRB5_KDB_NOMASTERKEY: code_string = "KRB5_KDB_NOMASTERKEY"; break;
    case KRB5_KDB_BADMASTERKEY: code_string = "KRB5_KDB_BADMASTERKEY"; break;
    case KRB5_KDB_INVALIDKEYSIZE:
        code_string = "KRB5_KDB_INVALIDKEYSIZE"; break;
    case KRB5_KDB_CANTREAD_STORED:
        code_string = "KRB5_KDB_CANTREAD_STORED"; break;
    case KRB5_KDB_BADSTORED_MKEY:
        code_string = "KRB5_KDB_BADSTORED_MKEY"; break;
    case KRB5_KDB_CANTLOCK_DB: code_string = "KRB5_KDB_CANTLOCK_DB"; break;
    case KRB5_KDB_DB_CORRUPT: code_string = "KRB5_KDB_DB_CORRUPT"; break;

    case KRB5_PARSE_ILLCHAR: code_string = "KRB5_PARSE_ILLCHAR"; break;
    case KRB5_PARSE_MALFORMED: code_string = "KRB5_PARSE_MALFORMED"; break;
    case KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN: code_string = "KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN"; break;
    case KRB5_REALM_UNKNOWN: code_string = "KRB5_REALM_UNKNOWN"; break;
    case KRB5_KDC_UNREACH: code_string = "KRB5_KDC_UNREACH"; break;
    case KRB5_KDCREP_MODIFIED: code_string = "KRB5_KDCREP_MODIFIED"; break;
    case KRB5KRB_AP_ERR_BAD_INTEGRITY: code_string  = "KRB5KRB_AP_ERR_BAD_INTEGRITY"; break;
    case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN: code_string = "KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN"; break;
    case KRB5_CONFIG_BADFORMAT: code_string = "KRB5_CONFIG_BADFORMAT"; break;

    case KRB5_CC_NOTFOUND: code_string = "KRB5_CC_NOTFOUND"; break;
    case KRB5_FCC_NOFILE: code_string = "KRB5_FCC_NOFILE"; break;

    case EINVAL: code_string = "EINVAL"; break;
    case ENOENT: code_string = "ENOENT"; break;

    default:
        fprintf(stderr, "**** CODE %ld (%s) ***\n", (long) code,
                error_message (code));
        code_string = "UNKNOWN";
        break;
    }

    error_string = error_message(code);

    if (! (dstring = (Tcl_DString *) malloc(sizeof(Tcl_DString)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX Do we really want to exit?  Ok if this is */
        /* just a test program, but what about if it gets */
        /* used for other things later? */
    }

    Tcl_DStringInit(dstring);

    if (! (Tcl_DStringAppendElement(dstring, "ERROR") &&
           Tcl_DStringAppendElement(dstring, code_string) &&
           Tcl_DStringAppendElement(dstring, error_string))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    return dstring;
}



static void stash_error(Tcl_Interp *interp, krb5_error_code code)
{
    Tcl_DString *dstring = unparse_err(code);
    Tcl_DStringResult(interp, dstring);
    Tcl_DStringFree(dstring);
    free(dstring);
}

static Tcl_DString *unparse_key_data(krb5_key_data *key_data, int n_key_data)
{
    Tcl_DString *str;
    char buf[2048];
    int i, j;

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);
    for (i = 0; i < n_key_data; i++) {
        krb5_key_data *key = &key_data[i];

        Tcl_DStringStartSublist(str);
        sprintf(buf, "%d", key->key_data_type[0]);
        Tcl_DStringAppendElement(str, buf);
        sprintf(buf, "%d", key->key_data_ver > 1 ?
                key->key_data_type[1] : -1);
        Tcl_DStringAppendElement(str, buf);
        if (key->key_data_contents[0]) {
            sprintf(buf, "0x");
            for (j = 0; j < key->key_data_length[0]; j++) {
                sprintf(buf + 2*(j+1), "%02x",
                        key->key_data_contents[0][j]);
            }
        } else *buf = '\0';
        Tcl_DStringAppendElement(str, buf);
        Tcl_DStringEndSublist(str);
    }

    return str;
}

static Tcl_DString *unparse_tl_data(krb5_tl_data *tl_data, int n_tl_data)
{
    Tcl_DString *str;
    char buf[2048];

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);
    Tcl_DStringStartSublist(str);
    for (; tl_data; tl_data = tl_data->tl_data_next) {
        Tcl_DStringStartSublist(str);
        sprintf(buf, "%d", tl_data->tl_data_type);
        Tcl_DStringAppendElement(str, buf);
        sprintf(buf, "%d", tl_data->tl_data_length);
        Tcl_DStringAppendElement(str, buf);
        Tcl_DStringAppend(str, " ", 1);
        Tcl_DStringAppend(str, (char *) tl_data->tl_data_contents,
                          tl_data->tl_data_length);
        Tcl_DStringEndSublist(str);
    }
    Tcl_DStringEndSublist(str);

    return str;
}

static Tcl_DString *unparse_flags(struct flagval *array, int size,
                                  krb5_int32 flags)
{
    int i;
    Tcl_DString *str;

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    for (i = 0; i < size; i++) {
        if (flags & array[i].val) {
            Tcl_DStringAppendElement(str, array[i].name);
        }
    }

    return str;
}


static int parse_flags(Tcl_Interp *interp, Tcl_HashTable *table,
                       struct flagval *array, int size, const char *str,
                       krb5_flags *flags)
{
    int tmp, argc, i, retcode = TCL_OK;
    const char **argv;
    Tcl_HashEntry *entry;

    if (Tcl_GetInt(interp, str, &tmp) == TCL_OK) {
        *flags = tmp;
        return TCL_OK;
    }
    Tcl_ResetResult(interp);

    if (Tcl_SplitList(interp, str, &argc, &argv) != TCL_OK) {
        return TCL_ERROR;
    }

    if (! table) {
        table = create_flag_table(array, size);
    }

    *flags = 0;

    for (i = 0; i < argc; i++) {
        if (! (entry = Tcl_FindHashEntry(table, argv[i]))) {
            Tcl_AppendResult(interp, "unknown krb5 flag ", argv[i], 0);
            retcode = TCL_ERROR;
            break;
        }
        *flags |= *(krb5_flags *) Tcl_GetHashValue(entry);
    }

    Tcl_Free((char *) argv);
    return(retcode);
}

static Tcl_DString *unparse_privs(krb5_flags flags)
{
    return unparse_flags(priv_flags, sizeof(priv_flags) /
                         sizeof(struct flagval), flags);
}


static Tcl_DString *unparse_krb5_flags(krb5_flags flags)
{
    return unparse_flags(krb5_flags_array, sizeof(krb5_flags_array) /
                         sizeof(struct flagval), flags);
}

static int parse_krb5_flags(Tcl_Interp *interp, const char *str,
                            krb5_flags *flags)
{
    krb5_flags tmp;
    static Tcl_HashTable *table = 0;
    int tcl_ret;

    if ((tcl_ret = parse_flags(interp, table, krb5_flags_array,
                               sizeof(krb5_flags_array) /
                               sizeof(struct flagval),
                               str, &tmp)) != TCL_OK) {
        return tcl_ret;
    }

    *flags = tmp;
    return TCL_OK;
}

static Tcl_DString *unparse_aux_attributes(krb5_int32 flags)
{
    return unparse_flags(aux_attributes, sizeof(aux_attributes) /
                         sizeof(struct flagval), flags);
}


static int parse_aux_attributes(Tcl_Interp *interp, const char *str,
                                long *flags)
{
    krb5_flags tmp;
    static Tcl_HashTable *table = 0;
    int tcl_ret;

    if ((tcl_ret = parse_flags(interp, table, aux_attributes,
                               sizeof(aux_attributes) /
                               sizeof(struct flagval),
                               str, &tmp)) != TCL_OK) {
        return tcl_ret;
    }

    *flags = tmp;
    return TCL_OK;
}

static int parse_principal_mask(Tcl_Interp *interp, const char *str,
                                krb5_int32 *flags)
{
    krb5_flags tmp;
    static Tcl_HashTable *table = 0;
    int tcl_ret;

    if ((tcl_ret = parse_flags(interp, table, principal_mask_flags,
                               sizeof(principal_mask_flags) /
                               sizeof(struct flagval),
                               str, &tmp)) != TCL_OK) {
        return tcl_ret;
    }

    *flags = tmp;
    return TCL_OK;
}

static int parse_policy_mask(Tcl_Interp *interp, const char *str,
                             krb5_int32 *flags)
{
    krb5_flags tmp;
    static Tcl_HashTable *table = 0;
    int tcl_ret;

    if ((tcl_ret = parse_flags(interp, table, policy_mask_flags,
                               sizeof(policy_mask_flags) /
                               sizeof(struct flagval),
                               str, &tmp)) != TCL_OK) {
        return tcl_ret;
    }

    *flags = tmp;
    return TCL_OK;
}


static Tcl_DString *unparse_principal_ent(kadm5_principal_ent_t princ,
                                          krb5_int32 mask)
{
    Tcl_DString *str, *tmp_dstring;
    char *tmp;
    char buf[20];
    krb5_error_code krb5_ret;

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    tmp = 0; /* It looks to me from looking at the library source */
    /* code for krb5_parse_name that the pointer passed into */
    /* it should be initialized to 0 if I want it do be */
    /* allocated automatically. */
    if (mask & KADM5_PRINCIPAL) {
        krb5_ret = krb5_unparse_name(context, princ->principal, &tmp);
        if (krb5_ret) {
            /* XXX Do we want to return an error?  Not sure. */
            Tcl_DStringAppendElement(str, "[unparseable principal]");
        }
        else {
            Tcl_DStringAppendElement(str, tmp);
            free(tmp);
        }
    } else
        Tcl_DStringAppendElement(str, "null");

    sprintf(buf, "%u", (unsigned int)princ->princ_expire_time);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%u", (unsigned int)princ->last_pwd_change);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%u", (unsigned int)princ->pw_expiration);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", princ->max_life);
    Tcl_DStringAppendElement(str, buf);

    tmp = 0;
    if (mask & KADM5_MOD_NAME) {
        if ((krb5_ret = krb5_unparse_name(context, princ->mod_name, &tmp))) {
            /* XXX */
            Tcl_DStringAppendElement(str, "[unparseable principal]");
        }
        else {
            Tcl_DStringAppendElement(str, tmp);
            free(tmp);
        }
    } else
        Tcl_DStringAppendElement(str, "null");

    sprintf(buf, "%u", (unsigned int)princ->mod_date);
    Tcl_DStringAppendElement(str, buf);

    if (mask & KADM5_ATTRIBUTES) {
        tmp_dstring = unparse_krb5_flags(princ->attributes);
        Tcl_DStringAppendElement(str, tmp_dstring->string);
        Tcl_DStringFree(tmp_dstring);
        free(tmp_dstring);
    } else
        Tcl_DStringAppendElement(str, "null");

    sprintf(buf, "%d", princ->kvno);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", princ->mkvno);
    Tcl_DStringAppendElement(str, buf);

    /* XXX This may be dangerous, because the contents of the policy */
    /* field are undefined if the POLICY bit isn't set.  However, I */
    /* think it's a bug for the field not to be null in that case */
    /* anyway, so we should assume that it will be null so that we'll */
    /* catch it if it isn't. */

    tmp_dstring = unparse_str(princ->policy);
    Tcl_DStringAppendElement(str, tmp_dstring->string);
    Tcl_DStringFree(tmp_dstring);
    free(tmp_dstring);

    tmp_dstring = unparse_aux_attributes(princ->aux_attributes);
    Tcl_DStringAppendElement(str, tmp_dstring->string);
    Tcl_DStringFree(tmp_dstring);
    free(tmp_dstring);

    sprintf(buf, "%d", princ->max_renewable_life);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%u", (unsigned int)princ->last_success);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%u", (unsigned int)princ->last_failed);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", princ->fail_auth_count);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", princ->n_key_data);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", princ->n_tl_data);
    Tcl_DStringAppendElement(str, buf);

    tmp_dstring = unparse_key_data(princ->key_data, princ->n_key_data);
    Tcl_DStringAppendElement(str, tmp_dstring->string);
    Tcl_DStringFree(tmp_dstring);
    free(tmp_dstring);

    tmp_dstring = unparse_tl_data(princ->tl_data, princ->n_tl_data);
    Tcl_DStringAppendElement(str, tmp_dstring->string);
    Tcl_DStringFree(tmp_dstring);
    free(tmp_dstring);

    return str;
}

static int parse_keysalts(Tcl_Interp *interp, const char *list,
                          krb5_key_salt_tuple **keysalts,
                          int num_keysalts)
{
    const char **argv, **argv1 = NULL;
    int i, tmp, argc, argc1, retcode;

    *keysalts = NULL;
    if (list == NULL)
        return TCL_OK;

    if ((retcode = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return retcode;
    }
    if (argc != num_keysalts) {
        Tcl_SetResult(interp, "wrong number of keysalts", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }
    *keysalts = (krb5_key_salt_tuple *)
        malloc(sizeof(krb5_key_salt_tuple)*num_keysalts);
    for (i = 0; i < num_keysalts; i++) {
        if ((retcode = Tcl_SplitList(interp, argv[i], &argc1, &argv1)) !=
            TCL_OK) {
            goto finished;
        }
        if (argc1 != 2) {
            Tcl_SetResult(interp, "wrong # of fields in keysalt", TCL_STATIC);
            retcode = TCL_ERROR;
            goto finished;
        }
        /* XXX this used to be argv1[1] too! */
        if ((retcode = Tcl_GetInt(interp, argv1[0], &tmp))
            != TCL_OK) {
            Tcl_AppendElement(interp, "while parsing ks_enctype");
            retcode = TCL_ERROR;
            goto finished;
        }
        (*keysalts)[i].ks_enctype = tmp;
        if ((retcode = Tcl_GetInt(interp, argv1[1], &tmp))
            != TCL_OK) {
            Tcl_AppendElement(interp, "while parsing ks_salttype");
            goto finished;
        }
        (*keysalts)[i].ks_salttype = tmp;

        Tcl_Free((char *) argv1);
        argv1 = NULL;
    }

finished:
    if (argv1) {
        Tcl_Free((char *) argv1);
    }
    Tcl_Free((char *) argv);
    return retcode;
}

static int parse_key_data(Tcl_Interp *interp, const char *list,
                          krb5_key_data **key_data,
                          int n_key_data)
{
    const char **argv = NULL;
    int argc, retcode;

    *key_data = NULL;
    if (list == NULL) {
        if (n_key_data != 0) {
            Tcl_SetResult(interp, "wrong number of key_datas", TCL_STATIC);
            retcode = TCL_ERROR;
            goto finished;
        } else
            return TCL_OK;
    }

    if ((retcode = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return retcode;
    }
    if (argc != n_key_data) {
        Tcl_SetResult(interp, "wrong number of key_datas", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

    if (argc != 0) {
        Tcl_SetResult(interp, "cannot parse key_data yet", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

finished:
    Tcl_Free((char *) argv);
    return retcode;
}

static int parse_tl_data(Tcl_Interp *interp, const char *list,
                         krb5_tl_data **tlp,
                         int n_tl_data)
{
    krb5_tl_data *tl, *tl2;
    const char **argv = NULL, **argv1 = NULL;
    int i, tmp, argc, argc1, retcode;

    *tlp = NULL;
    if (list == NULL) {
        if (n_tl_data != 0) {
            Tcl_SetResult(interp, "wrong number of tl_datas", TCL_STATIC);
            retcode = TCL_ERROR;
            goto finished;
        } else
            return TCL_OK;
    }

    if ((retcode = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return retcode;
    }
    if (argc != n_tl_data) {
        Tcl_SetResult(interp, "wrong number of tl_datas", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

    tl = tl2 = NULL;
    for (i = 0; i < n_tl_data; i++) {
        tl2 = (krb5_tl_data *) malloc(sizeof(krb5_tl_data));
        memset(tl2, 0, sizeof(krb5_tl_data));
        tl2->tl_data_next = tl;
        tl = tl2;
    }
    tl2 = tl;

    for (i = 0; i < n_tl_data; i++) {
        if ((retcode = Tcl_SplitList(interp, argv[i], &argc1, &argv1)) !=
            TCL_OK) {
            goto finished;
        }
        if (argc1 != 3) {
            Tcl_SetResult(interp, "wrong # of fields in tl_data", TCL_STATIC);
            retcode = TCL_ERROR;
            goto finished;
        }
        if ((retcode = Tcl_GetInt(interp, argv1[0], &tmp))
            != TCL_OK) {
            Tcl_AppendElement(interp, "while parsing tl_data_type");
            retcode = TCL_ERROR;
            goto finished;
        }
        tl->tl_data_type = tmp;
        if ((retcode = Tcl_GetInt(interp, argv1[1], &tmp))
            != TCL_OK) {
            Tcl_AppendElement(interp, "while parsing tl_data_length");
            retcode = TCL_ERROR;
            goto finished;
        }
        tl->tl_data_length = tmp;
        if (tl->tl_data_length != strlen(argv1[2])) {
            Tcl_SetResult(interp, "length != string length", TCL_STATIC);
            retcode = TCL_ERROR;
            goto finished;
        }
        tl->tl_data_contents = (krb5_octet *) strdup(argv1[2]);

        Tcl_Free((char *) argv1);
        argv1 = NULL;
        tl = tl->tl_data_next;
    }
    if (tl != NULL) {
        Tcl_SetResult(interp, "tl is not NULL!", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }
    *tlp = tl2;

finished:
    if (argv1) {
        Tcl_Free((char *) argv1);
    }
    Tcl_Free((char *) argv);
    return retcode;
}

static int parse_config_params(Tcl_Interp *interp, char *list,
                               kadm5_config_params *params)
{
    static Tcl_HashTable *table = 0;
    const char **argv = NULL;
    int tmp, argc, retcode;

    memset(params, 0, sizeof(kadm5_config_params));
    if (list == NULL)
        return TCL_OK;

    if ((retcode = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return retcode;
    }

    if (argc != 20) {
        Tcl_SetResult(interp, "wrong # args in config params structure",
                      TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

    if ((retcode = parse_flags(interp, table, config_mask_flags,
                               sizeof(config_mask_flags) /
                               sizeof(struct flagval),
                               argv[0], &tmp)) != TCL_OK) {
        goto finished;
    }
    params->mask = tmp;

    if ((retcode = parse_str(interp, argv[1], &params->realm)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing realm name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = Tcl_GetInt(interp, argv[2], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing kadmind_port");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->kadmind_port = tmp;
    if ((retcode = parse_str(interp, argv[3], &params->admin_server))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing profile name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = parse_str(interp, argv[4], &params->dbname)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing profile name");
        retcode = TCL_ERROR;
        goto finished;
    }
    /* Ignore argv[5], which used to set the admin_dbname field.  */
    /* Ignore argv[6], which used to set the admin_lockfile field.  */
    /* Ignore argv[7], which used to set the admin_keytab field.  */
    if ((retcode = parse_str(interp, argv[8], &params->acl_file)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing acl_file name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = parse_str(interp, argv[9], &params->dict_file)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing dict_file name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = Tcl_GetInt(interp, argv[10], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing mkey_from_kbd");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->mkey_from_kbd = tmp;
    if ((retcode = parse_str(interp, argv[11], &params->stash_file)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing stash_file name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = parse_str(interp, argv[12], &params->mkey_name)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing mkey_name name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((retcode = Tcl_GetInt(interp, argv[13], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing enctype");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->enctype = tmp;
    if ((retcode = Tcl_GetInt(interp, argv[14], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing max_life");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->max_life = tmp;
    if ((retcode = Tcl_GetInt(interp, argv[15], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing max_rlife");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->max_rlife = tmp;
    if ((retcode = Tcl_GetInt(interp, argv[16], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing expiration");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->expiration = tmp;
    if ((retcode = parse_krb5_flags(interp, argv[17], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing flags");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->flags = tmp;
    if ((retcode = Tcl_GetInt(interp, argv[18], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing num_keysalts");
        retcode = TCL_ERROR;
        goto finished;
    }
    params->num_keysalts = tmp;
    if ((retcode = parse_keysalts(interp, argv[19], &params->keysalts,
                                  params->num_keysalts)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing keysalts");
        retcode = TCL_ERROR;
        goto finished;
    }

finished:
    return retcode;
}

static int parse_principal_ent(Tcl_Interp *interp, char *list,
                               kadm5_principal_ent_t *out_princ)
{
    kadm5_principal_ent_t princ = 0;
    krb5_error_code krb5_ret;
    int tcl_ret;
    int argc;
    const char **argv;
    int tmp;
    int retcode = TCL_OK;

    if ((tcl_ret = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return tcl_ret;
    }

    if (argc != 12 && argc != 20) {
        Tcl_SetResult(interp, "wrong # args in principal structure",
                      TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

    if (! (princ = malloc(sizeof *princ))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }
    memset(princ, 0, sizeof(*princ));

    if ((krb5_ret = krb5_parse_name(context, argv[0], &princ->principal)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing principal");
        retcode = TCL_ERROR;
        goto finished;
    }

    /*
     * All of the numerical values parsed here are parsed into an
     * "int" and then assigned into the structure in case the actual
     * width of the field in the Kerberos structure is different from
     * the width of an integer.
     */

    if ((tcl_ret = Tcl_GetInt(interp, argv[1], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing princ_expire_time");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->princ_expire_time = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[2], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing last_pwd_change");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->last_pwd_change = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[3], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_expiration");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->pw_expiration = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[4], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing max_life");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->max_life = tmp;

    if ((krb5_ret = krb5_parse_name(context, argv[5], &princ->mod_name)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing mod_name");
        retcode = TCL_ERROR;
        goto finished;
    }

    if ((tcl_ret = Tcl_GetInt(interp, argv[6], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing mod_date");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->mod_date = tmp;

    if ((tcl_ret = parse_krb5_flags(interp, argv[7], &princ->attributes))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing attributes");
        retcode = TCL_ERROR;
        goto finished;
    }

    if ((tcl_ret = Tcl_GetInt(interp, argv[8], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing kvno");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->kvno = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[9], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing mkvno");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->mkvno = tmp;

    if ((tcl_ret = parse_str(interp, argv[10], &princ->policy)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy");
        retcode = TCL_ERROR;
        goto finished;
    }
    if(princ->policy != NULL) {
        if(!(princ->policy = strdup(princ->policy))) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
    }

    if ((tcl_ret = parse_aux_attributes(interp, argv[11],
                                        &princ->aux_attributes)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing aux_attributes");
        retcode = TCL_ERROR;
        goto finished;
    }

    if (argc == 12) goto finished;

    if ((tcl_ret = Tcl_GetInt(interp, argv[12], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing max_renewable_life");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->max_renewable_life = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[13], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing last_success");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->last_success = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[14], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing last_failed");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->last_failed = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[15], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing fail_auth_count");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->fail_auth_count = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[16], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing n_key_data");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->n_key_data = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[17], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing n_tl_data");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->n_tl_data = tmp;

    if ((tcl_ret = parse_key_data(interp, argv[18],
                                  &princ->key_data,
                                  princ->n_key_data)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing key_data");
        retcode = TCL_ERROR;
        goto finished;
    }

    if ((tcl_ret = parse_tl_data(interp, argv[19],
                                 &princ->tl_data,
                                 princ->n_tl_data)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing tl_data");
        retcode = TCL_ERROR;
        goto finished;
    }
    princ->n_tl_data = tmp;

finished:
    Tcl_Free((char *) argv);
    *out_princ = princ;
    return retcode;
}


static void free_principal_ent(kadm5_principal_ent_t *princ)
{
    krb5_free_principal(context, (*princ)->principal);
    krb5_free_principal(context, (*princ)->mod_name);
    free((*princ)->policy);
    free(*princ);
    *princ = 0;
}

static Tcl_DString *unparse_policy_ent(kadm5_policy_ent_t policy)
{
    Tcl_DString *str, *tmp_dstring;
    char buf[20];

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    tmp_dstring = unparse_str(policy->policy);
    Tcl_DStringAppendElement(str, tmp_dstring->string);
    Tcl_DStringFree(tmp_dstring);
    free(tmp_dstring);

    sprintf(buf, "%ld", policy->pw_min_life);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%ld", policy->pw_max_life);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%ld", policy->pw_min_length);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%ld", policy->pw_min_classes);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%ld", policy->pw_history_num);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%ld", policy->policy_refcnt);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", policy->pw_max_fail);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", policy->pw_failcnt_interval);
    Tcl_DStringAppendElement(str, buf);

    sprintf(buf, "%d", policy->pw_lockout_duration);
    Tcl_DStringAppendElement(str, buf);

    return str;
}



static int parse_policy_ent(Tcl_Interp *interp, char *list,
                            kadm5_policy_ent_t *out_policy)
{
    kadm5_policy_ent_t policy = 0;
    int tcl_ret;
    int argc;
    const char **argv;
    int tmp;
    int retcode = TCL_OK;

    if ((tcl_ret = Tcl_SplitList(interp, list, &argc, &argv)) != TCL_OK) {
        return tcl_ret;
    }

    if (argc != 7 && argc != 10) {
        Tcl_SetResult(interp, "wrong # args in policy structure", TCL_STATIC);
        retcode = TCL_ERROR;
        goto finished;
    }

    if (! (policy = malloc(sizeof *policy))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    if ((tcl_ret = parse_str(interp, argv[0], &policy->policy)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy name");
        retcode = TCL_ERROR;
        goto finished;
    }

    if(policy->policy != NULL) {
        if (! (policy->policy = strdup(policy->policy))) {
            fprintf(stderr, "Out of memory!\n");
            exit(1); /* XXX */
        }
    }

    /*
     * All of the numerical values parsed here are parsed into an
     * "int" and then assigned into the structure in case the actual
     * width of the field in the Kerberos structure is different from
     * the width of an integer.
     */

    if ((tcl_ret = Tcl_GetInt(interp, argv[1], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_min_life");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_min_life = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[2], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_max_life");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_max_life = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[3], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_min_length");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_min_length = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[4], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_min_classes");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_min_classes = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[5], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_history_num");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_history_num = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[6], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy_refcnt");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->policy_refcnt = tmp;

    if (argc == 7) goto finished;

    if ((tcl_ret = Tcl_GetInt(interp, argv[7], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_max_fail");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_max_fail = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[8], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_failcnt_interval");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_failcnt_interval = tmp;

    if ((tcl_ret = Tcl_GetInt(interp, argv[9], &tmp))
        != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_lockout_duration");
        retcode = TCL_ERROR;
        goto finished;
    }
    policy->pw_lockout_duration = tmp;

finished:
    Tcl_Free((char *) argv);
    *out_policy = policy;
    return retcode;
}


static void free_policy_ent(kadm5_policy_ent_t *policy)
{
    free((*policy)->policy);
    free(*policy);
    *policy = 0;
}

static Tcl_DString *unparse_keytype(krb5_enctype enctype)
{
    Tcl_DString *str;
    char buf[50];

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    switch (enctype) {
        /* XXX is this right? */
    case ENCTYPE_NULL: Tcl_DStringAppend(str, "ENCTYPE_NULL", -1); break;
    case ENCTYPE_DES_CBC_CRC:
        Tcl_DStringAppend(str, "ENCTYPE_DES_CBC_CRC", -1); break;
    default:
        sprintf(buf, "UNKNOWN KEYTYPE (0x%x)", enctype);
        Tcl_DStringAppend(str, buf, -1);
        break;
    }

    return str;
}


static Tcl_DString *unparse_keyblocks(krb5_keyblock *keyblocks, int num_keys)
{
    Tcl_DString *str;
    Tcl_DString *keytype;
    unsigned int i;
    int j;

    if (! (str = malloc(sizeof(*str)))) {
        fprintf(stderr, "Out of memory!\n");
        exit(1); /* XXX */
    }

    Tcl_DStringInit(str);

    for (j = 0; j < num_keys; j++) {
        krb5_keyblock *keyblock = &keyblocks[j];

        Tcl_DStringStartSublist(str);

        keytype = unparse_keytype(keyblock->enctype);
        Tcl_DStringAppendElement(str, keytype->string);
        Tcl_DStringFree(keytype);
        free(keytype);
        if (keyblock->length == 0) {
            Tcl_DStringAppendElement(str, "0x00");
        }
        else {
            Tcl_DStringAppendElement(str, "0x");
            for (i = 0; i < keyblock->length; i++) {
                char buf[3];
                sprintf(buf, "%02x", (int) keyblock->contents[i]);
                Tcl_DStringAppend(str, buf, -1);
            }
        }

        Tcl_DStringEndSublist(str);
    }


    return str;
}

enum init_type { INIT_NONE, INIT_PASS, INIT_CREDS };

static int _tcl_kadm5_init_any(enum init_type init_type, ClientData clientData,
                               Tcl_Interp *interp, int argc, const char *argv[])
{
    kadm5_ret_t ret;
    char *client_name, *pass, *service_name;
    int tcl_ret;
    krb5_ui_4 struct_version, api_version;
    const char *handle_var;
    void *server_handle;
    char *handle_name, *params_str;
    const char *whoami = argv[0];
    kadm5_config_params params;

    argv++, argc--;

    kadm5_init_krb5_context(&context);

    if (argc != 7) {
        Tcl_AppendResult(interp, whoami, ": ", arg_error, 0);
        return TCL_ERROR;
    }

    if (((tcl_ret = parse_str(interp, argv[0], &client_name)) != TCL_OK) ||
        ((tcl_ret = parse_str(interp, argv[1], &pass)) != TCL_OK) ||
        ((tcl_ret = parse_str(interp, argv[2], &service_name)) != TCL_OK) ||
        ((tcl_ret = parse_str(interp, argv[3], &params_str)) != TCL_OK) ||
        ((tcl_ret = parse_config_params(interp, params_str, &params))
         != TCL_OK) ||
        ((tcl_ret = Tcl_GetInt(interp, argv[4], (int *) &struct_version)) !=
         TCL_OK) ||
        ((tcl_ret = Tcl_GetInt(interp, argv[5], (int *) &api_version)) !=
         TCL_OK)) {
        return tcl_ret;
    }

    handle_var = argv[6];

    if (! (handle_var && *handle_var)) {
        Tcl_SetResult(interp, "must specify server handle variable name",
                      TCL_STATIC);
        return TCL_ERROR;
    }

    if (init_type == INIT_CREDS) {
        krb5_ccache cc;

        if (pass == NULL) {
            if ((ret = krb5_cc_default(context, &cc))) {
                stash_error(interp, ret);
                return TCL_ERROR;
            }
        } else {
            if ((ret = krb5_cc_resolve(context, pass, &cc))) {
                stash_error(interp, ret);
                return TCL_ERROR;
            }
        }

        ret = kadm5_init_with_creds(context, client_name, cc, service_name,
                                    &params, struct_version,
                                    api_version, NULL, &server_handle);

        (void) krb5_cc_close(context, cc);
    } else
        ret = kadm5_init(context, client_name, pass, service_name, &params,
                         struct_version, api_version, NULL, &server_handle);

    /* The string fields of params are aliases into argv[3], but
     * params.keysalts is allocated, so clean it up. */
    free(params.keysalts);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }

    if ((tcl_ret = put_server_handle(interp, server_handle, &handle_name))
        != TCL_OK) {
        return tcl_ret;
    }

    if (! Tcl_SetVar(interp, handle_var, handle_name, TCL_LEAVE_ERR_MSG)) {
        return TCL_ERROR;
    }

    set_ok(interp, "KADM5 API initialized.");
    return TCL_OK;
}

static int tcl_kadm5_init(ClientData clientData, Tcl_Interp *interp,
                          int argc, const char *argv[])
{
    return _tcl_kadm5_init_any(INIT_PASS, clientData, interp, argc, argv);
}

static int tcl_kadm5_init_with_creds(ClientData clientData, Tcl_Interp *interp,
                                     int argc, const char *argv[])
{
    return _tcl_kadm5_init_any(INIT_CREDS, clientData, interp, argc, argv);
}

static int tcl_kadm5_destroy(ClientData clientData, Tcl_Interp *interp,
                             int argc, const char *argv[])
{
    kadm5_ret_t ret;
    int tcl_ret;

    GET_HANDLE(0, 0);

    ret = kadm5_destroy(server_handle);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }

    if ((tcl_ret = remove_server_handle(interp, argv[-1])) != TCL_OK) {
        return tcl_ret;
    }

    set_ok(interp, "KADM5 API deinitialized.");
    return TCL_OK;
}

static int tcl_kadm5_create_principal(ClientData clientData,
                                      Tcl_Interp *interp,
                                      int argc, const char *argv[])
{
    int tcl_ret;
    kadm5_ret_t ret;
    int retcode = TCL_OK;
    char *princ_string;
    kadm5_principal_ent_t princ = 0;
    krb5_int32 mask;
    char *pw;
#ifdef OVERRIDE
    int override_qual;
#endif

    GET_HANDLE(3, 0);

    if ((tcl_ret = parse_str(interp, argv[0], &princ_string)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing principal");
        return tcl_ret;
    }

    if (princ_string &&
        ((tcl_ret = parse_principal_ent(interp, princ_string, &princ))
         != TCL_OK)) {
        return tcl_ret;
    }

    if ((tcl_ret = parse_principal_mask(interp, argv[1], &mask)) != TCL_OK) {
        retcode = tcl_ret;
        goto finished;
    }

    if ((tcl_ret = parse_str(interp, argv[2], &pw)) != TCL_OK) {
        retcode = tcl_ret;
        goto finished;
    }
#ifdef OVERRIDE
    if ((tcl_ret = Tcl_GetBoolean(interp, argv[3], &override_qual)) !=
        TCL_OK) {
        retcode = tcl_ret;
        goto finished;
    }
#endif

#ifdef OVERRIDE
    ret = kadm5_create_principal(server_handle, princ, mask, pw,
                                 override_qual);
#else
    ret = kadm5_create_principal(server_handle, princ, mask, pw);
#endif

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
        goto finished;
    }
    else {
        set_ok(interp, "Principal created.");
    }

finished:
    if (princ) {
        free_principal_ent(&princ);
    }
    return retcode;
}



static int tcl_kadm5_delete_principal(ClientData clientData,
                                      Tcl_Interp *interp,
                                      int argc, const char *argv[])
{
    krb5_principal princ;
    krb5_error_code krb5_ret;
    kadm5_ret_t ret;
    int tcl_ret;
    char *name;

    GET_HANDLE(1, 0);

    if((tcl_ret = parse_str(interp, argv[0], &name)) != TCL_OK)
        return tcl_ret;
    if(name != NULL) {
        if ((krb5_ret = krb5_parse_name(context, name, &princ))) {
            stash_error(interp, krb5_ret);
            Tcl_AppendElement(interp, "while parsing principal");
            return TCL_ERROR;
        }
    } else princ = NULL;
    ret = kadm5_delete_principal(server_handle, princ);

    if(princ != NULL)
        krb5_free_principal(context, princ);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }
    else {
        set_ok(interp, "Principal deleted.");
        return TCL_OK;
    }
}



static int tcl_kadm5_modify_principal(ClientData clientData,
                                      Tcl_Interp *interp,
                                      int argc, const char *argv[])
{
    char *princ_string;
    kadm5_principal_ent_t princ = 0;
    int tcl_ret;
    krb5_int32 mask;
    int retcode = TCL_OK;
    kadm5_ret_t ret;

    GET_HANDLE(2, 0);

    if ((tcl_ret = parse_str(interp, argv[0], &princ_string)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing principal");
        return tcl_ret;
    }

    if (princ_string &&
        ((tcl_ret = parse_principal_ent(interp, princ_string, &princ))
         != TCL_OK)) {
        return tcl_ret;
    }

    if ((tcl_ret = parse_principal_mask(interp, argv[1], &mask)) != TCL_OK) {
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_modify_principal(server_handle, princ, mask);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }
    else {
        set_ok(interp, "Principal modified.");
    }

finished:
    if (princ) {
        free_principal_ent(&princ);
    }
    return retcode;
}


static int tcl_kadm5_rename_principal(ClientData clientData,
                                      Tcl_Interp *interp,
                                      int argc, const char *argv[])
{
    krb5_principal source, target;
    krb5_error_code krb5_ret;
    kadm5_ret_t ret;
    int retcode = TCL_OK;

    GET_HANDLE(2, 0);

    if ((krb5_ret = krb5_parse_name(context, argv[0], &source)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing source");
        return TCL_ERROR;
    }

    if ((krb5_ret = krb5_parse_name(context, argv[1], &target)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing target");
        krb5_free_principal(context, source);
        return TCL_ERROR;
    }

    ret = kadm5_rename_principal(server_handle, source, target);

    if (ret == KADM5_OK) {
        set_ok(interp, "Principal renamed.");
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

    krb5_free_principal(context, source);
    krb5_free_principal(context, target);
    return retcode;
}



static int tcl_kadm5_chpass_principal(ClientData clientData,
                                      Tcl_Interp *interp,
                                      int argc, const char *argv[])
{
    krb5_principal princ;
    char *pw;
#ifdef OVERRIDE
    int override_qual;
#endif
    krb5_error_code krb5_ret;
    int retcode = TCL_OK;
    kadm5_ret_t ret;

    GET_HANDLE(2, 0);

    if ((krb5_ret = krb5_parse_name(context, argv[0], &princ)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing principal name");
        return TCL_ERROR;
    }

    if (parse_str(interp, argv[1], &pw) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing password");
        retcode = TCL_ERROR;
        goto finished;
    }

#ifdef OVERRIDE
    if (Tcl_GetBoolean(interp, argv[2], &override_qual) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing override_qual");
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_chpass_principal(server_handle,
                                 princ, pw, override_qual);
#else
    ret = kadm5_chpass_principal(server_handle, princ, pw);
#endif

    if (ret == KADM5_OK) {
        set_ok(interp, "Password changed.");
        goto finished;
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

finished:
    krb5_free_principal(context, princ);
    return retcode;
}



static int tcl_kadm5_chpass_principal_util(ClientData clientData,
                                           Tcl_Interp *interp,
                                           int argc, const char *argv[])
{
    krb5_principal princ;
    char *new_pw;
#ifdef OVERRIDE
    int override_qual;
#endif
    char *pw_ret, *pw_ret_var;
    char msg_ret[1024], *msg_ret_var;
    krb5_error_code krb5_ret;
    kadm5_ret_t ret;
    int retcode = TCL_OK;

    GET_HANDLE(4, 0);

    if ((krb5_ret = krb5_parse_name(context, argv[0], &princ)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing principal name");
        return TCL_ERROR;
    }

    if (parse_str(interp, argv[1], &new_pw) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing new password");
        retcode = TCL_ERROR;
        goto finished;
    }
#ifdef OVERRIDE
    if (Tcl_GetBoolean(interp, argv[2], &override_qual) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing override_qual");
        retcode = TCL_ERROR;
        goto finished;
    }
#endif
    if (parse_str(interp, argv[3], &pw_ret_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing pw_ret variable name");
        retcode = TCL_ERROR;
        goto finished;
    }

    if (parse_str(interp, argv[4], &msg_ret_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing msg_ret variable name");
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_chpass_principal_util(server_handle, princ, new_pw,
#ifdef OVERRIDE
                                      override_qual,
#endif
                                      pw_ret_var ? &pw_ret : 0,
                                      msg_ret_var ? msg_ret : 0,
                                      msg_ret_var ? sizeof(msg_ret) : 0);

    if (ret == KADM5_OK) {
        if (pw_ret_var &&
            (! Tcl_SetVar(interp, pw_ret_var, pw_ret,
                          TCL_LEAVE_ERR_MSG))) {
            Tcl_AppendElement(interp, "while setting pw_ret variable");
            retcode = TCL_ERROR;
            goto finished;
        }
        if (msg_ret_var &&
            (! Tcl_SetVar(interp, msg_ret_var, msg_ret,
                          TCL_LEAVE_ERR_MSG))) {
            Tcl_AppendElement(interp,
                              "while setting msg_ret variable");
            retcode = TCL_ERROR;
            goto finished;
        }
        set_ok(interp, "Password changed.");
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

finished:
    krb5_free_principal(context, princ);
    return retcode;
}



static int tcl_kadm5_randkey_principal(ClientData clientData,
                                       Tcl_Interp *interp,
                                       int argc, const char *argv[])
{
    krb5_principal princ;
    krb5_keyblock *keyblocks;
    int num_keys;
    char *keyblock_var, *num_var, buf[50];
    Tcl_DString *keyblock_dstring = 0;
    krb5_error_code krb5_ret;
    kadm5_ret_t ret;
    int retcode = TCL_OK;

    GET_HANDLE(3, 0);

    if ((krb5_ret = krb5_parse_name(context, argv[0], &princ)) != 0) {
        stash_error(interp, krb5_ret);
        Tcl_AppendElement(interp, "while parsing principal name");
        return TCL_ERROR;
    }

    if (parse_str(interp, argv[1], &keyblock_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing keyblock variable name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if (parse_str(interp, argv[2], &num_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing keyblock variable name");
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_randkey_principal(server_handle,
                                  princ, keyblock_var ? &keyblocks : 0,
                                  &num_keys);

    if (ret == KADM5_OK) {
        if (keyblock_var) {
            keyblock_dstring = unparse_keyblocks(keyblocks, num_keys);
            if (! Tcl_SetVar(interp, keyblock_var,
                             keyblock_dstring->string,
                             TCL_LEAVE_ERR_MSG)) {
                Tcl_AppendElement(interp,
                                  "while setting keyblock variable");
                retcode = TCL_ERROR;
                goto finished;
            }
        }
        if (num_var) {
            sprintf(buf, "%d", num_keys);
            if (! Tcl_SetVar(interp, num_var, buf,
                             TCL_LEAVE_ERR_MSG)) {
                Tcl_AppendElement(interp,
                                  "while setting num_keys variable");
            }
        }
        set_ok(interp, "Key randomized.");
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

finished:
    krb5_free_principal(context, princ);
    if (keyblock_dstring) {
        Tcl_DStringFree(keyblock_dstring);
        free(keyblock_dstring);
    }
    return retcode;
}



static int tcl_kadm5_get_principal(ClientData clientData, Tcl_Interp *interp,
                                   int argc, const char *argv[])
{
    krb5_principal princ;
    kadm5_principal_ent_rec ent;
    Tcl_DString *ent_dstring = 0;
    char *ent_var;
    char *name;
    krb5_error_code krb5_ret;
    int tcl_ret;
    kadm5_ret_t ret = -1;
    krb5_int32 mask;
    int retcode = TCL_OK;

    GET_HANDLE(3, 1);

    if((tcl_ret = parse_str(interp, argv[0], &name)) != TCL_OK)
        return tcl_ret;
    if(name != NULL) {
        if ((krb5_ret = krb5_parse_name(context, name, &princ)) != 0) {
            stash_error(interp, krb5_ret);
            Tcl_AppendElement(interp, "while parsing principal name");
            return TCL_ERROR;
        }
    } else princ = NULL;

    if ((tcl_ret = parse_str(interp, argv[1], &ent_var)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing entry variable name");
        retcode = TCL_ERROR;
        goto finished;
    }
    if ((tcl_ret = parse_principal_mask(interp, argv[2], &mask)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing principal mask");
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_get_principal(server_handle, princ, ent_var ? &ent : 0,
                              mask);

    if (ret == KADM5_OK) {
        if (ent_var) {
            ent_dstring = unparse_principal_ent(&ent, mask);
            if (! Tcl_SetVar(interp, ent_var, ent_dstring->string,
                             TCL_LEAVE_ERR_MSG)) {
                Tcl_AppendElement(interp,
                                  "while setting entry variable");
                retcode = TCL_ERROR;
                goto finished;
            }
            set_ok(interp, "Principal retrieved.");
        }
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

finished:
    if (ent_dstring) {
        Tcl_DStringFree(ent_dstring);
        free(ent_dstring);
    }
    if(princ != NULL)
        krb5_free_principal(context, princ);
    if (ret == KADM5_OK && ent_var &&
        (ret = kadm5_free_principal_ent(server_handle, &ent)) &&
        (retcode == TCL_OK)) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }
    return retcode;
}

static int tcl_kadm5_create_policy(ClientData clientData, Tcl_Interp *interp,
                                   int argc, const char *argv[])
{
    int tcl_ret;
    kadm5_ret_t ret;
    int retcode = TCL_OK;
    char *policy_string;
    kadm5_policy_ent_t policy = 0;
    krb5_int32 mask;

    GET_HANDLE(2, 0);

    if ((tcl_ret = parse_str(interp, argv[0], &policy_string)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy");
        return tcl_ret;
    }

    if (policy_string &&
        ((tcl_ret = parse_policy_ent(interp, policy_string, &policy))
         != TCL_OK)) {
        return tcl_ret;
    }

    if ((tcl_ret = parse_policy_mask(interp, argv[1], &mask)) != TCL_OK) {
        retcode = tcl_ret;
        goto finished;
    }

    ret = kadm5_create_policy(server_handle, policy, mask);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
        goto finished;
    }
    else {
        set_ok(interp, "Policy created.");
    }

finished:
    if (policy) {
        free_policy_ent(&policy);
    }
    return retcode;
}



static int tcl_kadm5_delete_policy(ClientData clientData, Tcl_Interp *interp,
                                   int argc, const char *argv[])
{
    kadm5_ret_t ret;
    char *policy;

    GET_HANDLE(1, 0);

    if (parse_str(interp, argv[0], &policy) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy name");
        return TCL_ERROR;
    }

    ret = kadm5_delete_policy(server_handle, policy);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }
    else {
        set_ok(interp, "Policy deleted.");
        return TCL_OK;
    }
}



static int tcl_kadm5_modify_policy(ClientData clientData, Tcl_Interp *interp,
                                   int argc, const char *argv[])
{
    char *policy_string;
    kadm5_policy_ent_t policy = 0;
    int tcl_ret;
    krb5_int32 mask;
    int retcode = TCL_OK;
    kadm5_ret_t ret;

    GET_HANDLE(2, 0);

    if ((tcl_ret = parse_str(interp, argv[0], &policy_string)) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy");
        return tcl_ret;
    }

    if (policy_string &&
        ((tcl_ret = parse_policy_ent(interp, policy_string, &policy))
         != TCL_OK)) {
        return tcl_ret;
    }

    if ((tcl_ret = parse_policy_mask(interp, argv[1], &mask)) != TCL_OK) {
        retcode = TCL_ERROR;
        goto finished;
    }

    ret = kadm5_modify_policy(server_handle, policy, mask);

    if (ret != KADM5_OK) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }
    else {
        set_ok(interp, "Policy modified.");
    }

finished:
    if (policy) {
        free_policy_ent(&policy);
    }
    return retcode;
}


static int tcl_kadm5_get_policy(ClientData clientData, Tcl_Interp *interp,
                                int argc, const char *argv[])
{
    kadm5_policy_ent_rec ent;
    Tcl_DString *ent_dstring = 0;
    char *policy;
    char *ent_var;
    kadm5_ret_t ret;
    int retcode = TCL_OK;

    GET_HANDLE(2, 1);

    if (parse_str(interp, argv[0], &policy) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing policy name");
        return TCL_ERROR;
    }

    if (parse_str(interp, argv[1], &ent_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing entry variable name");
        return TCL_ERROR;
    }

    ret = kadm5_get_policy(server_handle, policy, ent_var ? &ent : 0);

    if (ret == KADM5_OK) {
        if (ent_var) {
            ent_dstring = unparse_policy_ent(&ent);
            if (! Tcl_SetVar(interp, ent_var, ent_dstring->string,
                             TCL_LEAVE_ERR_MSG)) {
                Tcl_AppendElement(interp,
                                  "while setting entry variable");
                retcode = TCL_ERROR;
                goto finished;
            }
            set_ok(interp, "Policy retrieved.");
        }
    }
    else {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }

finished:
    if (ent_dstring) {
        Tcl_DStringFree(ent_dstring);
        free(ent_dstring);
    }
    if (ent_var && ret == KADM5_OK &&
        (ret = kadm5_free_policy_ent(server_handle, &ent)) &&
        (retcode == TCL_OK)) {
        stash_error(interp, ret);
        retcode = TCL_ERROR;
    }
    return retcode;
}



static int tcl_kadm5_free_principal_ent(ClientData clientData,
                                        Tcl_Interp *interp,
                                        int argc, const char *argv[])
{
    char *ent_name;
    kadm5_principal_ent_t ent;
    kadm5_ret_t ret;

    GET_HANDLE(1, 0);

    if (parse_str(interp, argv[0], &ent_name) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing entry name");
        return TCL_ERROR;
    }

    if ((! ent_name) &&
        (ret = kadm5_free_principal_ent(server_handle, 0))) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }
    else {
        Tcl_HashEntry *entry;

        if (strncmp(ent_name, "principal", sizeof("principal")-1)) {
            Tcl_AppendResult(interp, "invalid principal handle \"",
                             ent_name, "\"", 0);
            return TCL_ERROR;
        }
        if (! struct_table) {
            if (! (struct_table = malloc(sizeof(*struct_table)))) {
                fprintf(stderr, "Out of memory!\n");
                exit(1); /* XXX */
            }
            Tcl_InitHashTable(struct_table, TCL_STRING_KEYS);
        }

        if (! (entry = Tcl_FindHashEntry(struct_table, ent_name))) {
            Tcl_AppendResult(interp, "principal handle \"", ent_name,
                             "\" not found", 0);
            return TCL_ERROR;
        }

        ent = (kadm5_principal_ent_t) Tcl_GetHashValue(entry);

        ret = kadm5_free_principal_ent(server_handle, ent);
        if (ret != KADM5_OK) {
            stash_error(interp, ret);
            return TCL_ERROR;
        }
        Tcl_DeleteHashEntry(entry);
    }
    set_ok(interp, "Principal freed.");
    return TCL_OK;
}


static int tcl_kadm5_free_policy_ent(ClientData clientData,
                                     Tcl_Interp *interp,
                                     int argc, const char *argv[])
{
    char *ent_name;
    kadm5_policy_ent_t ent;
    kadm5_ret_t ret;

    GET_HANDLE(1, 0);

    if (parse_str(interp, argv[0], &ent_name) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing entry name");
        return TCL_ERROR;
    }

    if ((! ent_name) &&
        (ret = kadm5_free_policy_ent(server_handle, 0))) {
        stash_error(interp, ret);
        return TCL_ERROR;
    }
    else {
        Tcl_HashEntry *entry;

        if (strncmp(ent_name, "policy", sizeof("policy")-1)) {
            Tcl_AppendResult(interp, "invalid principal handle \"",
                             ent_name, "\"", 0);
            return TCL_ERROR;
        }
        if (! struct_table) {
            if (! (struct_table = malloc(sizeof(*struct_table)))) {
                fprintf(stderr, "Out of memory!\n");
                exit(1); /* XXX */
            }
            Tcl_InitHashTable(struct_table, TCL_STRING_KEYS);
        }

        if (! (entry = Tcl_FindHashEntry(struct_table, ent_name))) {
            Tcl_AppendResult(interp, "policy handle \"", ent_name,
                             "\" not found", 0);
            return TCL_ERROR;
        }

        ent = (kadm5_policy_ent_t) Tcl_GetHashValue(entry);

        if ((ret = kadm5_free_policy_ent(server_handle, ent)) != KADM5_OK) {
            stash_error(interp, ret);
            return TCL_ERROR;
        }
        Tcl_DeleteHashEntry(entry);
    }
    set_ok(interp, "Policy freed.");
    return TCL_OK;
}


static int tcl_kadm5_get_privs(ClientData clientData, Tcl_Interp *interp,
                               int argc, const char *argv[])
{
    const char *set_ret;
    kadm5_ret_t ret;
    char *priv_var;
    long privs;

    GET_HANDLE(1, 0);

    if (parse_str(interp, argv[0], &priv_var) != TCL_OK) {
        Tcl_AppendElement(interp, "while parsing privs variable name");
        return TCL_ERROR;
    }

    ret = kadm5_get_privs(server_handle, priv_var ? &privs : 0);

    if (ret == KADM5_OK) {
        if (priv_var) {
            Tcl_DString *str = unparse_privs(privs);
            set_ret = Tcl_SetVar(interp, priv_var, str->string,
                                 TCL_LEAVE_ERR_MSG);
            Tcl_DStringFree(str);
            free(str);
            if (! set_ret) {
                Tcl_AppendElement(interp, "while setting priv variable");
                return TCL_ERROR;
            }
        }
        set_ok(interp, "Privileges retrieved.");
        return TCL_OK;
    }
    else {
        stash_error(interp, ret);
        return TCL_ERROR;
    }
}


void Tcl_kadm5_init(Tcl_Interp *interp)
{
    char buf[20];

    Tcl_SetVar(interp, "KADM5_ADMIN_SERVICE",
               KADM5_ADMIN_SERVICE, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "KADM5_CHANGEPW_SERVICE",
               KADM5_CHANGEPW_SERVICE, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_STRUCT_VERSION);
    Tcl_SetVar(interp, "KADM5_STRUCT_VERSION", buf, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_API_VERSION_2);
    Tcl_SetVar(interp, "KADM5_API_VERSION_2", buf, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_API_VERSION_3);
    Tcl_SetVar(interp, "KADM5_API_VERSION_3", buf, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_API_VERSION_4);
    Tcl_SetVar(interp, "KADM5_API_VERSION_4", buf, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_API_VERSION_MASK);
    Tcl_SetVar(interp, "KADM5_API_VERSION_MASK", buf, TCL_GLOBAL_ONLY);
    (void) sprintf(buf, "%d", KADM5_STRUCT_VERSION_MASK);
    Tcl_SetVar(interp, "KADM5_STRUCT_VERSION_MASK", buf,
               TCL_GLOBAL_ONLY);

    Tcl_CreateCommand(interp, "kadm5_init", tcl_kadm5_init, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_init_with_creds",
                      tcl_kadm5_init_with_creds, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_destroy", tcl_kadm5_destroy, 0,
                      0);
    Tcl_CreateCommand(interp, "kadm5_create_principal",
                      tcl_kadm5_create_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_delete_principal",
                      tcl_kadm5_delete_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_modify_principal",
                      tcl_kadm5_modify_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_rename_principal",
                      tcl_kadm5_rename_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_chpass_principal",
                      tcl_kadm5_chpass_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_chpass_principal_util",
                      tcl_kadm5_chpass_principal_util, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_randkey_principal",
                      tcl_kadm5_randkey_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_get_principal",
                      tcl_kadm5_get_principal, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_create_policy",
                      tcl_kadm5_create_policy, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_delete_policy",
                      tcl_kadm5_delete_policy, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_modify_policy",
                      tcl_kadm5_modify_policy, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_get_policy",
                      tcl_kadm5_get_policy, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_free_principal_ent",
                      tcl_kadm5_free_principal_ent, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_free_policy_ent",
                      tcl_kadm5_free_policy_ent, 0, 0);
    Tcl_CreateCommand(interp, "kadm5_get_privs",
                      tcl_kadm5_get_privs, 0, 0);
}
