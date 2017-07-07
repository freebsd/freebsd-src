/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <ldap.h>
#include <errno.h>
#include <krb5.h>
#include "ldap_err.h"
#ifndef LDAP_X_ERROR
#define LDAP_X_ERROR(x) (0)
#endif

#ifndef LDAP_NAME_ERROR
#ifdef NAME_ERROR
#define LDAP_NAME_ERROR NAME_ERROR
#else
#define LDAP_NAME_ERROR(x) (0)
#endif
#endif

#ifndef LDAP_SECURITY_ERROR
#define LDAP_SECURITY_ERROR(x) (0)
#endif

#ifndef LDAP_SERVICE_ERROR
#define LDAP_SERVICE_ERROR(x) (0)
#endif

#ifndef LDAP_API_ERROR
#define LDAP_API_ERROR(x) (0)
#endif

#ifndef LDAP_UPDATE_ERROR
#define LDAP_UPDATE_ERROR(x) (0)
#endif

/*
 * The possible KDB errors are
 * 1. KRB5_KDB_UK_RERROR
 * 2. KRB5_KDB_UK_SERROR
 * 3. KRB5_KDB_NOENTRY
 * 4. KRB5_KDB_TRUNCATED_RECORD
 * 5. KRB5_KDB_UNAUTH
 * 6. KRB5_KDB_DB_CORRUPT
 * 7. KRB5_KDB_ACCESS_ERROR             (NEW)
 * 8. KRB5_KDB_INTERNAL_ERROR           (NEW)
 * 9. KRB5_KDB_SERVER_INTERNAL_ERR      (NEW)
 * 10. KRB5_KDB_CONSTRAINT_VIOLATION    (NEW)
 *
 */

/*
 * op :
 *  0          => not specified
 *  OP_INIT    => ldap_init
 *  OP_BIND    => ldap_bind
 *  OP_UNBIND  => ldap_unbind
 *  OP_ADD     => ldap_add
 *  OP_MOD     => ldap_modify
 *  OP_DEL     => ldap_delete
 *  OP_SEARCH  => ldap_search
 *  OP_CMP     => ldap_compare
 *  OP_ABANDON => ldap_abandon
 */

int
translate_ldap_error(int err, int op) {

    switch (err) {
    case LDAP_SUCCESS:
        return 0;

    case LDAP_OPERATIONS_ERROR:
        /* LDAP_OPERATIONS_ERROR: Indicates an internal error. The server is
         * unable to respond with a more specific error and is also unable
         * to properly respond to a request */
    case LDAP_UNAVAILABLE_CRITICAL_EXTENSION:
        /* LDAP server was unable to satisfy a request because one or more
         * critical extensions were not available */
        /* This might mean that the schema was not extended ... */
    case LDAP_UNDEFINED_TYPE:
        /* The attribute specified in the modify or add operation does not
         * exist in the LDAP server's schema. */
        return KRB5_KDB_INTERNAL_ERROR;


    case LDAP_INAPPROPRIATE_MATCHING:
        /* The matching rule specified in the search filter does not match a
         * rule defined for the attribute's syntax */
        return KRB5_KDB_UK_RERROR;

    case LDAP_CONSTRAINT_VIOLATION:
        /* The attribute value specified in a modify, add, or modify DN
         * operation violates constraints placed on the attribute */
    case LDAP_TYPE_OR_VALUE_EXISTS:
        /* The attribute value specified in a modify or add operation
         * already exists as a value for that attribute */
        return KRB5_KDB_UK_SERROR;

    case LDAP_INVALID_SYNTAX:
        /* The attribute value specified in an add, compare, or modify
         * operation is an unrecognized or invalid syntax for the attribute */
        if (op == OP_ADD || op == OP_MOD)
            return KRB5_KDB_UK_SERROR;
        else /* OP_CMP */
            return KRB5_KDB_UK_RERROR;

        /* Ensure that the following don't occur in the DAL-LDAP code.
         * Don't rely on the LDAP server to catch it */
    case LDAP_SASL_BIND_IN_PROGRESS:
        /* This is not an error. So, this function should not be called */
    case LDAP_COMPARE_FALSE:
    case LDAP_COMPARE_TRUE:
        /* LDAP_COMPARE_FALSE and LDAP_COMPARE_TRUE are not errors. This
         * function should not be invoked for them */
    case LDAP_RESULTS_TOO_LARGE: /* CLDAP */
    case LDAP_TIMELIMIT_EXCEEDED:
    case LDAP_SIZELIMIT_EXCEEDED:
        return KRB5_KDB_SERVER_INTERNAL_ERR;

    case LDAP_INVALID_DN_SYNTAX:
        /* The syntax of the DN is incorrect */
        return EINVAL;

    case LDAP_PROTOCOL_ERROR:
        /* LDAP_PROTOCOL_ERROR: Indicates that the server has received an
         * invalid or malformed request from the client */
    case LDAP_CONFIDENTIALITY_REQUIRED:

        /* Bind problems ... */
    case LDAP_AUTH_METHOD_NOT_SUPPORTED:
/*      case LDAP_STRONG_AUTH_NOT_SUPPORTED: // Is this a bind error ? */
    case LDAP_INAPPROPRIATE_AUTH:
    case LDAP_INVALID_CREDENTIALS:
    case LDAP_UNAVAILABLE:
        return KRB5_KDB_ACCESS_ERROR;

    case LDAP_STRONG_AUTH_REQUIRED:
        if (op == OP_BIND) /* the LDAP server accepts only strong authentication. */
            return KRB5_KDB_ACCESS_ERROR;
        else /* Client requested an operation such that requires strong authentication */
            return KRB5_KDB_CONSTRAINT_VIOLATION;

    case LDAP_REFERRAL:
        return KRB5_KDB_NOENTRY;

    case LDAP_ADMINLIMIT_EXCEEDED:
        /* An LDAP server limit set by an administrative authority has been
         * exceeded */
        return KRB5_KDB_CONSTRAINT_VIOLATION;
    case LDAP_UNWILLING_TO_PERFORM:
        /* The LDAP server cannot process the request because of
         * server-defined restrictions */
        return KRB5_KDB_CONSTRAINT_VIOLATION;


    case LDAP_NO_SUCH_ATTRIBUTE:
        /* Indicates that the attribute specified in the modify or compare
         * operation does not exist in the entry */
        if (op == OP_MOD)
            return KRB5_KDB_UK_SERROR;
        else /* OP_CMP */
            return KRB5_KDB_TRUNCATED_RECORD;


    case LDAP_ALIAS_DEREF_PROBLEM:
        /* Either the client does not have access rights to read the aliased
         * object's name or dereferencing is not allowed */
#ifdef LDAP_PROXY_AUTHZ_FAILURE
    case LDAP_PROXY_AUTHZ_FAILURE: // Is this correct ?
#endif
    case LDAP_INSUFFICIENT_ACCESS:
        /* Caller does not have sufficient rights to perform the requested
         * operation */
        return KRB5_KDB_UNAUTH;

    case LDAP_LOOP_DETECT:
        /* Client discovered an alias or referral loop */
        return KRB5_KDB_DB_CORRUPT;

    default:

        if (LDAP_NAME_ERROR (err))
            return KRB5_KDB_NOENTRY;

        if (LDAP_SECURITY_ERROR (err))
            return KRB5_KDB_UNAUTH;

        if (LDAP_SERVICE_ERROR (err) || LDAP_API_ERROR (err) || LDAP_X_ERROR (err))
            return KRB5_KDB_ACCESS_ERROR;

        if (LDAP_UPDATE_ERROR(err))
            return KRB5_KDB_UK_SERROR;

        /* LDAP_OTHER */
        return KRB5_KDB_SERVER_INTERNAL_ERR;
    }
}
