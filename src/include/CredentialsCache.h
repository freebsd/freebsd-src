/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/CredentialsCache.h */
/*
 * Copyright 1998-2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __CREDENTIALSCACHE__
#define __CREDENTIALSCACHE__

#if defined(macintosh) || (defined(__MACH__) && defined(__APPLE__))
#include <TargetConditionals.h>

/* Notifications which are sent when the ccache collection or a ccache change.
 * Notifications are sent to the distributed notification center.
 * The object for kCCAPICacheCollectionChangedNotification is NULL.
 * The object for kCCAPICCacheChangedNotification is a CFString containing the
 * name of the ccache.
 *
 * Note: Notifications are not sent if the CCacheServer crashes.  */
#define kCCAPICacheCollectionChangedNotification CFSTR ("CCAPICacheCollectionChangedNotification")
#define kCCAPICCacheChangedNotification          CFSTR ("CCAPICCacheChangedNotification")
#endif

#if defined(_WIN32)
#include <winsock.h>
#include "win-mac.h"
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if TARGET_OS_MAC
#pragma pack(push,2)
#endif

#if defined(_WIN32)
#define CCACHE_API      __declspec(dllexport)

#if _INTEGRAL_MAX_BITS >= 64 && _MSC_VER >= 1500 && !defined(_WIN64) && !defined(_USE_32BIT_TIME_T)
#if defined(_TIME_T_DEFINED) || defined(_INC_IO) || defined(_INC_TIME) || defined(_INC_WCHAR)
#error time_t has been defined as a 64-bit integer which is incompatible with Kerberos on this platform.
#endif /* _TIME_T_DEFINED */
#define _USE_32BIT_TIME_T
#endif
#else
#define CCACHE_API
#endif

/*!
 * \mainpage Credentials Cache API (CCAPI) Documentation
 *
 * \section toc Table of Contents
 *
 * \li \ref introduction
 * \li \ref error_handling
 * \li \ref synchronization_atomicity
 * \li \ref memory_management
 * \li \ref opaque_types
 *
 * \li \ref ccapi_constants_reference
 * \li \ref ccapi_types_reference
 *
 * \li \ref cc_context_reference
 * \li \ref cc_context_f "cc_context_t Functions"
 *
 * \li \ref cc_ccache_reference
 * \li \ref cc_ccache_f "cc_ccache_t Functions"
 *
 * \li \ref cc_credentials_reference
 * \li \ref cc_credentials_f "cc_credentials_t Functions"
 *
 * \li \ref cc_ccache_iterator_reference
 * \li \ref cc_ccache_iterator_f "cc_ccache_iterator_t Functions"
 *
 * \li \ref cc_credentials_iterator_reference
 * \li \ref cc_credentials_iterator_f "cc_credentials_iterator_t Functions"
 *
 * \li \ref cc_string_reference
 * \li \ref cc_string_f "cc_string_t Functions"
 *
 * \section introduction Introduction
 *
 * This is the specification for an API which provides Credentials Cache
 * services for both Kerberos v5 and v4. The idea behind this API is that
 * multiple Kerberos implementations can share a single collection of
 * credentials caches, mediated by this API specification. On the Mac OS
 * and Microsoft Windows platforms this will allow single-login, even when
 * more than one Kerberos shared library is in use on a particular system.
 *
 * Abstractly, a credentials cache collection contains one or more credentials
 * caches, or ccaches. A ccache is uniquely identified by its name, which is
 * a string internal to the API and not intended to be presented to users.
 * The user presentable identifier of a ccache is its principal.
 *
 * Unlike the previous versions of the API, version 3 of the API stores both
 * Kerberos v4 and v5 credentials in the same ccache.
 *
 * At any given time, one ccache is the "default" ccache. The exact meaning
 * of a default ccache is OS-specific; refer to implementation requirements
 * for details.
 *
 * \section error_handling Error Handling
 *
 * All functions of the API return some of the error constants listed FIXME;
 * the exact list of error constants returned by any API function is provided
 * in the function descriptions below.
 *
 * When returning an error constant other than ccNoError or ccIteratorEnd, API
 * functions never modify any of the values passed in by reference.
 *
 * \section synchronization_atomicity Synchronization and Atomicity
 *
 * Every function in the API is atomic.  In order to make a series of calls
 * atomic, callers should lock the ccache or cache collection they are working
 * with to advise other callers not to modify that container.  Note that
 * advisory locks are per container so even if you have a read lock on the cache
 * collection other callers can obtain write locks on ccaches in that cache
 * collection.
 *
 * Note that iterators do not iterate over ccaches and credentials atomically
 * because locking ccaches and the cache collection over every iteration would
 * degrade performance considerably under high load.  However, iterators do
 * guarantee a consistent view of items they are iterating over.  Iterators
 * will never return duplicate entries or skip entries when items are removed
 * or added to the container they are iterating over.
 *
 * An application can always lock a ccache or the cache collection to guarantee
 * that other callers participating in the advisory locking system do not
 * modify the ccache or cache collection.
 *
 * Implementations should not use copy-on-write techniques to implement locks
 * because those techniques imply that same parts of the ccache collection
 * remain visible to some callers even though they are not present in the
 * collection, which is a potential security risk. For example, a copy-on-write
 * technique might make a copy of the entire collection when a read lock is
 * acquired, so as to allow the owner of the lock to access the collection in
 * an apparently unmodified state, while also allowing others to make
 * modifications to the collection. However, this would also enable the owner
 * of the lock to indefinitely (until the expiration time) use credentials that
 * have actually been deleted from the collection.
 *
 * \section memory_management Object Memory Management
 *
 * The lifetime of an object returned by the API is until release() is called
 * for it. Releasing one object has no effect on existence of any other object.
 * For example, a ccache obtained within a context continue to exist when the
 * context is released.
 *
 * Every object returned by the API (cc_context_t, cc_ccache_t, cc_ccache_iterator_t,
 * cc_credentials_t, cc_credentials_iterator_t, cc_string_t) is owned by the
 * caller of the API, and it is the responsibility of the caller to call release()
 * for every object to prevent memory leaks.
 *
 * \section opaque_types Opaque Types
 *
 * All of the opaque high-level types in CCache API are implemented as structures
 * of function pointers and private data. To perform some operation on a type, the
 * caller of the API has to first obtain an instance of that type, and then call the
 * appropriate function pointer from that instance. For example, to call
 * get_change_time() on a cc_context_t, one would call cc_initialize() which creates
 * a new cc_context_t and then call its get_change_time(), like this:
 *
 * \code
 * cc_context_t context;
 * cc_int32 err = cc_initialize (&context, ccapi_version_3, nil, nil);
 * if (err == ccNoError)
 * time = context->functions->get_change_time (context)
 * \endcode
 *
 * All API functions also have convenience preprocessor macros, which make the API
 * seem completely function-based. For example, cc_context_get_change_time
 * (context, time) is equivalent to context->functions->get_change_time
 * (context, time). The convenience macros follow the following naming convention:
 *
 * The API function some_function()
 * \code
 * cc_type_t an_object;
 * result = an_object->functions->some_function (opaque_pointer, args)
 * \endcode
 *
 * has an equivalent convenience macro of the form cc_type_some_function():
 * \code
 * cc_type_t an_object;
 * result = cc_type_some_function (an_object, args)
 * \endcode
 *
 * The specifications below include the names for both the functions and the
 * convenience macros, in that order. For clarity, it is recommended that clients
 * using the API use the convenience macros, but that is merely a stylistic choice.
 *
 * Implementing the API in this manner allows us to extend and change the interface
 * in the future, while preserving compatibility with older clients.
 *
 * For example, consider the case when the signature or the semantics of a cc_ccache_t
 * function is changed. The API version number is incremented. The library
 * implementation contains both a function with the old signature and semantics and
 * a function with the new signature and semantics. When a context is created, the API
 * version number used in that context is stored in the context, and therefore it can
 * be used whenever a ccache is created in that context. When a ccache is created in a
 * context with the old API version number, the function pointer structure for the
 * ccache is filled with pointers to functions implementing the old semantics; when a
 * ccache is created in a context with the new API version number, the function pointer
 * structure for the ccache is filled with poitners to functions implementing the new
 * semantics.
 *
 * Similarly, if a function is added to the API, the version number in the context can
 * be used to decide whether to include the implementation of the new function in the
 * appropriate function pointer structure or not.
 */

/*!
 * \defgroup ccapi_constants_reference Constants
 * @{
 */

/*!
 * API version numbers
 *
 * These constants are passed into cc_initialize() to indicate the version
 * of the API the caller wants to use.
 *
 * CCAPI v1 and v2 are deprecated and should not be used.
 */
enum {
    ccapi_version_2 = 2,
    ccapi_version_3 = 3,
    ccapi_version_4 = 4,
    ccapi_version_5 = 5,
    ccapi_version_6 = 6,
    ccapi_version_7 = 7,
    ccapi_version_max = ccapi_version_7
};

/*!
 * Error codes
 */
enum {

    ccNoError = 0,  /*!< Success. */

    ccIteratorEnd = 201,  /*!< Iterator is done iterating. */
    ccErrBadParam,  /*!< Bad parameter (NULL or invalid pointer where valid pointer expected). */
    ccErrNoMem,  /*!< Not enough memory to complete the operation. */
    ccErrInvalidContext,  /*!< Context is invalid (e.g., it was released). */
    ccErrInvalidCCache,  /*!< CCache is invalid (e.g., it was released or destroyed). */

    /* 206 */
    ccErrInvalidString,  /*!< String is invalid (e.g., it was released). */
    ccErrInvalidCredentials,  /*!< Credentials are invalid (e.g., they were released), or they have a bad version. */
    ccErrInvalidCCacheIterator,  /*!< CCache iterator is invalid (e.g., it was released). */
    ccErrInvalidCredentialsIterator,  /*!< Credentials iterator is invalid (e.g., it was released). */
    ccErrInvalidLock,  /*!< Lock is invalid (e.g., it was released). */

    /* 211 */
    ccErrBadName,  /*!< Bad credential cache name format. */
    ccErrBadCredentialsVersion,  /*!< Credentials version is invalid. */
    ccErrBadAPIVersion,  /*!< Unsupported API version. */
    ccErrContextLocked,  /*!< Context is already locked. */
    ccErrContextUnlocked,  /*!< Context is not locked by the caller. */

    /* 216 */
    ccErrCCacheLocked,   /*!< CCache is already locked. */
    ccErrCCacheUnlocked,  /*!< CCache is not locked by the caller. */
    ccErrBadLockType,  /*!< Bad lock type. */
    ccErrNeverDefault,  /*!< CCache was never default. */
    ccErrCredentialsNotFound,  /*!< Matching credentials not found in the ccache. */

    /* 221 */
    ccErrCCacheNotFound,  /*!< Matching ccache not found in the collection. */
    ccErrContextNotFound,  /*!< Matching cache collection not found. */
    ccErrServerUnavailable,  /*!< CCacheServer is unavailable. */
    ccErrServerInsecure,  /*!< CCacheServer has detected that it is running as the wrong user. */
    ccErrServerCantBecomeUID,  /*!< CCacheServer failed to start running as the user. */

    /* 226 */
    ccErrTimeOffsetNotSet,  /*!< KDC time offset not set for this ccache. */
    ccErrBadInternalMessage,  /*!< The client and CCacheServer can't communicate (e.g., a version mismatch). */
    ccErrNotImplemented,  /*!< API function not supported by this implementation. */
    ccErrClientNotFound  /*!< CCacheServer has no record of the caller's process (e.g., the server crashed). */
};

/*!
 * Credentials versions
 *
 * These constants are used in several places in the API to discern
 * between Kerberos v4 and Kerberos v5. Not all values are valid
 * inputs and outputs for all functions; function specifications
 * below detail the allowed values.
 *
 * Kerberos version constants will always be a bit-field, and can be
 * tested as such; for example the following test will tell you if
 * a ccacheVersion includes v5 credentials:
 *
 * if ((ccacheVersion & cc_credentials_v5) != 0)
 */
enum cc_credential_versions {
    cc_credentials_v4 = 1,
    cc_credentials_v5 = 2,
    cc_credentials_v4_v5 = 3
};

/*!
 * Lock types
 *
 * These constants are used in the locking functions to describe the
 * type of lock requested.  Note that all CCAPI locks are advisory
 * so only callers using the lock calls will be blocked by each other.
 * This is because locking functions were introduced after the CCAPI
 * came into common use and we did not want to break existing callers.
 */
enum cc_lock_types {
    cc_lock_read = 0,
    cc_lock_write = 1,
    cc_lock_upgrade = 2,
    cc_lock_downgrade = 3
};

/*!
 * Locking Modes
 *
 * These constants are used in the advisory locking functions to
 * describe whether or not the lock function should block waiting for
 * a lock or return an error immediately.   For example, attempting to
 * acquire a lock with a non-blocking call will result in an error if the
 * lock cannot be acquired; otherwise, the call will block until the lock
 * can be acquired.
 */
enum cc_lock_modes {
    cc_lock_noblock = 0,
    cc_lock_block = 1
};

/*!
 * Sizes of fields in cc_credentials_v4_t.
 */
enum {
    /* Make sure all of these are multiples of four (for alignment sanity) */
    cc_v4_name_size     = 40,
    cc_v4_instance_size = 40,
    cc_v4_realm_size    = 40,
    cc_v4_ticket_size   = 1254,
    cc_v4_key_size      = 8
};

/*!
 * String to key type (Kerberos v4 only)
 */
enum cc_string_to_key_type {
    cc_v4_stk_afs = 0,
    cc_v4_stk_des = 1,
    cc_v4_stk_columbia_special = 2,
    cc_v4_stk_krb5 = 3,
    cc_v4_stk_unknown = 4
};

/*!@}*/

/*!
 * \defgroup ccapi_types_reference Basic Types
 * @{
 */

/*! Unsigned 32-bit integer type */
typedef uint32_t            cc_uint32;
/*! Signed 32-bit integer type */
typedef int32_t             cc_int32;
#if defined (WIN32)
typedef __int64             cc_int64;
typedef unsigned __int64    cc_uint64;
#else
/*! Unsigned 64-bit integer type */
typedef int64_t             cc_int64;
/*! Signed 64-bit integer type */
typedef uint64_t            cc_uint64;
#endif
/*!
 * The cc_time_t type is used to represent a time in seconds. The time must
 * be stored as the number of seconds since midnight GMT on January 1, 1970.
 */
typedef cc_uint32           cc_time_t;

/*!@}*/

/*!
 * \defgroup cc_context_reference cc_context_t Overview
 * @{
 *
 * The cc_context_t type gives the caller access to a ccache collection.
 * Before being able to call any functions in the CCache API, the caller
 * needs to acquire an instance of cc_context_t by calling cc_initialize().
 *
 * For API function documentation see \ref cc_context_f.
 */
struct cc_context_f;
typedef struct cc_context_f cc_context_f;

struct cc_context_d {
    const cc_context_f *functions;
#if TARGET_OS_MAC
    const cc_context_f *vector_functions;
#endif
};
typedef struct cc_context_d cc_context_d;
typedef cc_context_d *cc_context_t;

/*!@}*/

/*!
 * \defgroup cc_ccache_reference cc_ccache_t Overview
 * @{
 *
 * The cc_ccache_t type represents a reference to a ccache.
 * Callers can access a ccache and the credentials stored in it
 * via a cc_ccache_t. A cc_ccache_t can be acquired via
 * cc_context_open_ccache(), cc_context_open_default_ccache(), or
 * cc_ccache_iterator_next().
 *
 * For API function documentation see \ref cc_ccache_f.
 */
struct cc_ccache_f;
typedef struct cc_ccache_f cc_ccache_f;

struct cc_ccache_d {
    const cc_ccache_f *functions;
#if TARGET_OS_MAC
    const cc_ccache_f *vector_functions;
#endif
};
typedef struct cc_ccache_d cc_ccache_d;
typedef cc_ccache_d *cc_ccache_t;

/*!@}*/

/*!
 * \defgroup cc_ccache_iterator_reference cc_ccache_iterator_t Overview
 * @{
 *
 * The cc_ccache_iterator_t type represents an iterator that
 * iterates over a set of ccaches and returns them in all in some
 * order. A new instance of this type can be obtained by calling
 * cc_context_new_ccache_iterator().
 *
 * For API function documentation see \ref cc_ccache_iterator_f.
 */
struct cc_ccache_iterator_f;
typedef struct cc_ccache_iterator_f cc_ccache_iterator_f;

struct cc_ccache_iterator_d {
    const cc_ccache_iterator_f *functions;
#if TARGET_OS_MAC
    const cc_ccache_iterator_f *vector_functions;
#endif
};
typedef struct cc_ccache_iterator_d cc_ccache_iterator_d;
typedef cc_ccache_iterator_d *cc_ccache_iterator_t;
/*!@}*/

/*!
 * \defgroup cc_credentials_reference cc_credentials_t Overview
 * @{
 *
 * The cc_credentials_t type is used to store a single set of
 * credentials for either Kerberos v4 or Kerberos v5. In addition
 * to its only function, release(), it contains a pointer to a
 * cc_credentials_union structure. A cc_credentials_union
 * structure contains an integer of the enumerator type
 * cc_credentials_version, which is either #cc_credentials_v4 or
 * #cc_credentials_v5, and a pointer union, which contains either a
 * cc_credentials_v4_t pointer or a cc_credentials_v5_t pointer,
 * depending on the value in version.
 *
 * Variables of the type cc_credentials_t are allocated by the CCAPI
 * implementation, and should be released with their release()
 * function. API functions which receive credentials structures
 * from the caller always accept cc_credentials_union, which is
 * allocated by the caller, and accordingly disposed by the caller.
 *
 * For API functions see \ref cc_credentials_f.
 */

/*!
 * If a cc_credentials_t variable is used to store Kerberos v4
 * credentials, then credentials.credentials_v4 points to a v4
 * credentials structure.  This structure is similar to a
 * krb4 API CREDENTIALS structure.
 */
struct cc_credentials_v4_t {
    cc_uint32       version;
    /*! A properly quoted string representation of the first component of the client principal */
    char            principal [cc_v4_name_size];
    /*! A properly quoted string representation of the second component of the client principal */
    char            principal_instance [cc_v4_instance_size];
    /*! A properly quoted string representation of the first component of the service principal */
    char            service [cc_v4_name_size];
    /*! A properly quoted string representation of the second component of the service principal */
    char            service_instance [cc_v4_instance_size];
    /*! A properly quoted string representation of the realm */
    char            realm [cc_v4_realm_size];
    /*! Ticket session key */
    unsigned char   session_key [cc_v4_key_size];
    /*! Key version number */
    cc_int32        kvno;
    /*! String to key type used.  See cc_string_to_key_type for valid values */
    cc_int32        string_to_key_type;
    /*! Time when the ticket was issued */
    cc_time_t       issue_date;
    /*! Ticket lifetime in 5 minute units */
    cc_int32        lifetime;
    /*! IPv4 address of the client the ticket was issued for */
    cc_uint32       address;
    /*! Ticket size (no greater than cc_v4_ticket_size) */
    cc_int32        ticket_size;
    /*! Ticket data */
    unsigned char   ticket [cc_v4_ticket_size];
};
typedef struct cc_credentials_v4_t cc_credentials_v4_t;

/*!
 * The CCAPI data structure.  This structure is similar to a krb5_data structure.
 * In a v5 credentials structure, cc_data structures are used
 * to store tagged variable-length binary data. Specifically,
 * for cc_credentials_v5.ticket and
 * cc_credentials_v5.second_ticket, the cc_data.type field must
 * be zero. For the cc_credentials_v5.addresses,
 * cc_credentials_v5.authdata, and cc_credentials_v5.keyblock,
 * the cc_data.type field should be the address type,
 * authorization data type, and encryption type, as defined by
 * the Kerberos v5 protocol definition.
 */
struct cc_data {
    /*! The type of the data as defined by the krb5_data structure. */
    cc_uint32                   type;
    /*! The length of \a data. */
    cc_uint32                   length;
    /*! The data buffer. */
    void*                       data;
};
typedef struct cc_data cc_data;

/*!
 * If a cc_credentials_t variable is used to store Kerberos v5 c
 * redentials, and then credentials.credentials_v5 points to a
 * v5 credentials structure.  This structure is similar to a
 * krb5_creds structure.
 */
struct cc_credentials_v5_t {
    /*! A properly quoted string representation of the client principal. */
    char*      client;
    /*! A properly quoted string representation of the service principal. */
    char*      server;
    /*! Session encryption key info. */
    cc_data    keyblock;
    /*! The time when the ticket was issued. */
    cc_time_t  authtime;
    /*! The time when the ticket becomes valid. */
    cc_time_t  starttime;
    /*! The time when the ticket expires. */
    cc_time_t  endtime;
    /*! The time when the ticket becomes no longer renewable (if renewable). */
    cc_time_t  renew_till;
    /*! 1 if the ticket is encrypted in another ticket's key, or 0 otherwise. */
    cc_uint32  is_skey;
    /*! Ticket flags, as defined by the Kerberos 5 API. */
    cc_uint32  ticket_flags;
    /*! The the list of network addresses of hosts that are allowed to authenticate
     * using this ticket. */
    cc_data**  addresses;
    /*! Ticket data. */
    cc_data    ticket;
    /*! Second ticket data. */
    cc_data    second_ticket;
    /*! Authorization data. */
    cc_data**  authdata;
};
typedef struct cc_credentials_v5_t cc_credentials_v5_t;

struct cc_credentials_union {
    /*! The credentials version of this credentials object. */
    cc_uint32                   version;
    /*! The credentials. */
    union {
        /*! If \a version is #cc_credentials_v4, a pointer to a cc_credentials_v4_t. */
        cc_credentials_v4_t*    credentials_v4;
        /*! If \a version is #cc_credentials_v5, a pointer to a cc_credentials_v5_t. */
        cc_credentials_v5_t*    credentials_v5;
    }                           credentials;
};
typedef struct cc_credentials_union cc_credentials_union;

struct cc_credentials_f;
typedef struct cc_credentials_f cc_credentials_f;

struct cc_credentials_d {
    const cc_credentials_union *data;
    const cc_credentials_f *functions;
#if TARGET_OS_MAC
    const cc_credentials_f *otherFunctions;
#endif
};
typedef struct cc_credentials_d cc_credentials_d;
typedef cc_credentials_d *cc_credentials_t;
/*!@}*/

/*!
 * \defgroup cc_credentials_iterator_reference cc_credentials_iterator_t
 * @{
 * The cc_credentials_iterator_t type represents an iterator that
 * iterates over a set of credentials. A new instance of this type
 * can be obtained by calling cc_ccache_new_credentials_iterator().
 *
 * For API function documentation see \ref cc_credentials_iterator_f.
 */
struct cc_credentials_iterator_f;
typedef struct cc_credentials_iterator_f cc_credentials_iterator_f;

struct cc_credentials_iterator_d {
    const cc_credentials_iterator_f *functions;
#if TARGET_OS_MAC
    const cc_credentials_iterator_f *vector_functions;
#endif
};
typedef struct cc_credentials_iterator_d cc_credentials_iterator_d;
typedef cc_credentials_iterator_d *cc_credentials_iterator_t;
/*!@}*/

/*!
 * \defgroup cc_string_reference cc_string_t Overview
 * @{
 * The cc_string_t represents a C string returned by the API.
 * It has a pointer to the string data and a release() function.
 * This type is used for both principal names and ccache names
 * returned by the API. Principal names may contain UTF-8 encoded
 * strings for internationalization purposes.
 *
 * For API function documentation see \ref cc_string_f.
 */
struct cc_string_f;
typedef struct cc_string_f cc_string_f;

struct cc_string_d {
    const char *data;
    const cc_string_f *functions;
#if TARGET_OS_MAC
    const cc_string_f *vector_functions;
#endif
};
typedef struct cc_string_d cc_string_d;
typedef cc_string_d *cc_string_t;
/*!@}*/

/*!
 * Function pointer table for cc_context_t.  For more information see
 * \ref cc_context_reference.
 */
struct cc_context_f {
    /*!
     * \param io_context the context object to free.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_release(): Release memory associated with a cc_context_t.
     */
    cc_int32 (*release) (cc_context_t io_context);

    /*!
     * \param in_context the context object for the cache collection to examine.
     * \param out_time on exit, the time of the most recent change for the entire ccache collection.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_get_change_time(): Get the last time the cache collection changed.
     *
     * This function returns the time of the most recent change for the entire ccache collection.
     * By maintaining a local copy the caller can deduce whether or not the ccache collection has
     * been modified since the previous call to cc_context_get_change_time().
     *
     * The time returned by cc_context_get_changed_time() increases whenever:
     *
     * \li a ccache is created
     * \li a ccache is destroyed
     * \li a credential is stored
     * \li a credential is removed
     * \li a ccache principal is changed
     * \li the default ccache is changed
     *
     * \note In order to be able to compare two values returned by cc_context_get_change_time(),
     * the caller must use the same context to acquire them. Callers should maintain a single
     * context in memory for cc_context_get_change_time() calls rather than creating a new
     * context for every call.
     *
     * \sa wait_for_change
     */
    cc_int32 (*get_change_time) (cc_context_t  in_context,
                                 cc_time_t    *out_time);

    /*!
     * \param in_context the context object for the cache collection.
     * \param out_name on exit, the name of the default ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_get_default_ccache_name(): Get the name of the default ccache.
     *
     * This function returns the name of the default ccache. When the default ccache
     * exists, its name is returned. If there are no ccaches in the collection, and
     * thus there is no default ccache, the name that the default ccache should have
     * is returned. The ccache with that name will be used as the default ccache by
     * all processes which initialized Kerberos libraries before the ccache was created.
     *
     * If there is no default ccache, and the client is creating a new ccache, it
     * should be created with the default name. If there already is a default ccache,
     * and the client wants to create a new ccache (as opposed to reusing an existing
     * ccache), it should be created with any unique name; #create_new_ccache()
     * can be used to accomplish that more easily.
     *
     * If the first ccache is created with a name other than the default name, then
     * the processes already running will not notice the credentials stored in the
     * new ccache, which is normally undesirable.
     */
    cc_int32 (*get_default_ccache_name) (cc_context_t  in_context,
                                         cc_string_t  *out_name);

    /*!
     * \param in_context the context object for the cache collection.
     * \param in_name the name of the ccache to open.
     * \param out_ccache on exit, a ccache object for the ccache
     * \return On success, #ccNoError.  If no ccache named \a in_name exists,
     * #ccErrCCacheNotFound. On failure, an error code representing the failure.
     * \brief \b cc_context_open_ccache(): Open a ccache.
     *
     * Opens an already existing ccache identified by its name. It returns a reference
     * to the ccache in \a out_ccache.
     *
     * The list of all ccache names, principals, and credentials versions may be retrieved
     * by calling cc_context_new_cache_iterator(), cc_ccache_get_name(),
     * cc_ccache_get_principal(), and cc_ccache_get_cred_version().
     */
    cc_int32 (*open_ccache) (cc_context_t  in_context,
                             const char   *in_name,
                             cc_ccache_t  *out_ccache);

    /*!
     * \param in_context the context object for the cache collection.
     * \param out_ccache on exit, a ccache object for the default ccache
     * \return On success, #ccNoError.  If no default ccache exists,
     * #ccErrCCacheNotFound. On failure, an error code representing the failure.
     * \brief \b cc_context_open_default_ccache(): Open the default ccache.
     *
     * Opens the default ccache. It returns a reference to the ccache in *ccache.
     *
     * This function performs the same function as calling
     * cc_context_get_default_ccache_name followed by cc_context_open_ccache,
     * but it performs it atomically.
     */
    cc_int32 (*open_default_ccache) (cc_context_t  in_context,
                                     cc_ccache_t  *out_ccache);

    /*!
     * \param in_context the context object for the cache collection.
     * \param in_name the name of the new ccache to create
     * \param in_cred_vers the version of the credentials the new ccache will hold
     * \param in_principal the client principal of the credentials the new ccache will hold
     * \param out_ccache on exit, a ccache object for the newly created ccache
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_create_ccache(): Create a new ccache.
     *
     * Create a new credentials cache. The ccache is uniquely identified by its name.
     * The principal given is also associated with the ccache and the credentials
     * version specified. A NULL name is not allowed (and ccErrBadName is returned
     * if one is passed in). Only cc_credentials_v4 and cc_credentials_v5 are valid
     * input values for cred_vers. If you want to create a new ccache that will hold
     * both versions of credentials, call cc_context_create_ccache() with one version,
     * and then cc_ccache_set_principal() with the other version.
     *
     * If you want to create a new ccache (with a unique name), you should use
     * cc_context_create_new_ccache() instead. If you want to create or reinitialize
     * the default cache, you should use cc_context_create_default_ccache().
     *
     * If name is non-NULL and there is already a ccache named name:
     *
     * \li the credentials in the ccache whose version is cred_vers are removed
     * \li the principal (of the existing ccache) associated with cred_vers is set to principal
     * \li a handle for the existing ccache is returned and all existing handles for the ccache remain valid
     *
     * If no ccache named name already exists:
     *
     * \li a new empty ccache is created
     * \li the principal of the new ccache associated with cred_vers is set to principal
     * \li a handle for the new ccache is returned
     *
     * For a new ccache, the name should be any unique string. The name is not
     * intended to be presented to users.
     *
     * If the created ccache is the first ccache in the collection, it is made
     * the default ccache. Note that normally it is undesirable to create the first
     * ccache with a name different from the default ccache name (as returned by
     * cc_context_get_default_ccache_name()); see the description of
     * cc_context_get_default_ccache_name() for details.
     *
     * The principal should be a C string containing an unparsed Kerberos principal
     * in the format of the appropriate Kerberos version, i.e. \verbatim foo.bar/@BAZ
     * \endverbatim for Kerberos v4 and \verbatim foo/bar/@BAZ \endverbatim
     * for Kerberos v5.
     */
    cc_int32 (*create_ccache) (cc_context_t  in_context,
                               const char   *in_name,
                               cc_uint32     in_cred_vers,
                               const char   *in_principal,
                               cc_ccache_t  *out_ccache);

    /*!
     * \param in_context the context object for the cache collection.
     * \param in_cred_vers the version of the credentials the new default ccache will hold
     * \param in_principal the client principal of the credentials the new default ccache will hold
     * \param out_ccache on exit, a ccache object for the newly created default ccache
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_create_default_ccache(): Create a new default ccache.
     *
     * Create the default credentials cache. The behavior of this function is
     * similar to that of cc_create_ccache(). If there is a default ccache
     * (which is always the case except when there are no ccaches at all in
     * the collection), it is initialized with the specified credentials version
     * and principal, as per cc_create_ccache(); otherwise, a new ccache is
     * created, and its name is the name returned by
     * cc_context_get_default_ccache_name().
     */
    cc_int32 (*create_default_ccache) (cc_context_t  in_context,
                                       cc_uint32     in_cred_vers,
                                       const char   *in_principal,
                                       cc_ccache_t  *out_ccache);

    /*!
     * \param in_context the context object for the cache collection.
     * \param in_cred_vers the version of the credentials the new ccache will hold
     * \param in_principal the client principal of the credentials the new ccache will hold
     * \param out_ccache on exit, a ccache object for the newly created ccache
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_create_new_ccache(): Create a new uniquely named ccache.
     *
     * Create a new unique credentials cache. The behavior of this function
     * is similar to that of cc_create_ccache(). If there are no ccaches, and
     * therefore no default ccache, the new ccache is created with the default
     * ccache name as would be returned by get_default_ccache_name(). If there
     * are some ccaches, and therefore there is a default ccache, the new ccache
     * is created with a new unique name. Clearly, this function never reinitializes
     * a ccache, since it always uses a unique name.
     */
    cc_int32 (*create_new_ccache) (cc_context_t in_context,
                                   cc_uint32    in_cred_vers,
                                   const char  *in_principal,
                                   cc_ccache_t *out_ccache);

    /*!
     * \param in_context the context object for the cache collection.
     * \param out_iterator on exit, a ccache iterator object for the ccache collection.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_new_ccache_iterator(): Get an iterator for the cache collection.
     *
     * Used to allocate memory and initialize iterator. Successive calls to iterator's
     * next() function will return ccaches in the collection.
     *
     * If changes are made to the collection while an iterator is being used
     * on it, the iterator must return at least the intersection, and at most
     * the union, of the set of ccaches that were present when the iteration
     * began and the set of ccaches that are present when it ends.
     */
    cc_int32 (*new_ccache_iterator) (cc_context_t          in_context,
                                     cc_ccache_iterator_t *out_iterator);

    /*!
     * \param in_context the context object for the cache collection.
     * \param in_lock_type the type of lock to obtain.
     * \param in_block whether or not the function should block if the lock cannot be obtained immediately.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_lock(): Lock the cache collection.
     *
     * Attempts to acquire an advisory lock for the ccache collection. Allowed values
     * for lock_type are:
     *
     * \li cc_lock_read: a read lock.
     * \li cc_lock_write: a write lock
     * \li cc_lock_upgrade: upgrade an already-obtained read lock to a write lock
     * \li cc_lock_downgrade: downgrade an already-obtained write lock to a read lock
     *
     * If block is cc_lock_block, lock() will not return until the lock is acquired.
     * If block is cc_lock_noblock, lock() will return immediately, either acquiring
     * the lock and returning ccNoError, or failing to acquire the lock and returning
     * an error explaining why.
     *
     * Locks apply only to the list of ccaches, not the contents of those ccaches.  To
     * prevent callers participating in the advisory locking from changing the credentials
     * in a cache you must also lock that ccache with cc_ccache_lock().  This is so
     * that you can get the list of ccaches without preventing applications from
     * simultaneously obtaining service tickets.
     *
     * To avoid having to deal with differences between thread semantics on different
     * platforms, locks are granted per context, rather than per thread or per process.
     * That means that different threads of execution have to acquire separate contexts
     * in order to be able to synchronize with each other.
     *
     * The lock should be unlocked by using cc_context_unlock().
     *
     * \note All locks are advisory.  For example, callers which do not call
     * cc_context_lock() and cc_context_unlock() will not be prevented from writing
     * to the cache collection when you have a read lock.  This is because the CCAPI
     * locking was added after the first release and thus adding mandatory locks would
     * have changed the user experience and performance of existing applications.
     */
    cc_int32 (*lock) (cc_context_t in_context,
                      cc_uint32    in_lock_type,
                      cc_uint32    in_block);

    /*!
     * \param in_context the context object for the cache collection.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_unlock(): Unlock the cache collection.
     */
    cc_int32 (*unlock) (cc_context_t in_cc_context);

    /*!
     * \param in_context a context object.
     * \param in_compare_to_context a context object to compare with \a in_context.
     * \param out_equal on exit, whether or not the two contexts refer to the same cache collection.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_compare(): Compare two context objects.
     */
    cc_int32 (*compare) (cc_context_t  in_cc_context,
                         cc_context_t  in_compare_to_context,
                         cc_uint32    *out_equal);

    /*!
     * \param in_context a context object.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_context_wait_for_change(): Wait for the next change in the cache collection.
     *
     * This function blocks until the next change is made to the cache collection
     * ccache collection. By repeatedly calling cc_context_wait_for_change() from
     * a worker thread the caller can effectively receive callbacks whenever the
     * cache collection changes.  This is considerably more efficient than polling
     * with cc_context_get_change_time().
     *
     * cc_context_wait_for_change() will return whenever:
     *
     * \li a ccache is created
     * \li a ccache is destroyed
     * \li a credential is stored
     * \li a credential is removed
     * \li a ccache principal is changed
     * \li the default ccache is changed
     *
     * \note In order to make sure that the caller doesn't miss any changes,
     * cc_context_wait_for_change() always returns immediately after the first time it
     * is called on a new context object. Callers must use the same context object
     * for successive calls to cc_context_wait_for_change() rather than creating a new
     * context for every call.
     *
     * \sa get_change_time
     */
    cc_int32 (*wait_for_change) (cc_context_t in_cc_context);
};

/*!
 * Function pointer table for cc_ccache_t.  For more information see
 * \ref cc_ccache_reference.
 */
struct cc_ccache_f {
    /*!
     * \param io_ccache the ccache object to release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_release(): Release memory associated with a cc_ccache_t object.
     * \note Does not modify the ccache.  If you wish to remove the ccache see cc_ccache_destroy().
     */
    cc_int32 (*release) (cc_ccache_t io_ccache);

    /*!
     * \param io_ccache the ccache object to destroy and release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_destroy(): Destroy a ccache.
     *
     * Destroy the ccache referred to by \a io_ccache and releases memory associated with
     * the \a io_ccache object.  After this call \a io_ccache becomes invalid.  If
     * \a io_ccache was the default ccache, the next ccache in the cache collection (if any)
     * becomes the new default.
     */
    cc_int32 (*destroy) (cc_ccache_t io_ccache);

    /*!
     * \param io_ccache a ccache object to make the new default ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_set_default(): Make a ccache the default ccache.
     */
    cc_int32 (*set_default) (cc_ccache_t io_ccache);

    /*!
     * \param in_ccache a ccache object.
     * \param out_credentials_version on exit, the credentials version of \a in_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_get_credentials_version(): Get the credentials version of a ccache.
     *
     * cc_ccache_get_credentials_version() returns one value of the enumerated type
     * cc_credentials_vers. The possible return values are #cc_credentials_v4
     * (if ccache's v4 principal has been set), #cc_credentials_v5
     * (if ccache's v5 principal has been set), or #cc_credentials_v4_v5
     * (if both ccache's v4 and v5 principals have been set). A ccache's
     * principal is set with one of cc_context_create_ccache(),
     * cc_context_create_new_ccache(), cc_context_create_default_ccache(), or
     * cc_ccache_set_principal().
     */
    cc_int32 (*get_credentials_version) (cc_ccache_t  in_ccache,
                                         cc_uint32   *out_credentials_version);

    /*!
     * \param in_ccache a ccache object.
     * \param out_name on exit, a cc_string_t representing the name of \a in_ccache.
     * \a out_name must be released with cc_string_release().
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_get_name(): Get the name of a ccache.
     */
    cc_int32 (*get_name) (cc_ccache_t  in_ccache,
                          cc_string_t *out_name);

    /*!
     * \param in_ccache a ccache object.
     * \param in_credentials_version the credentials version to get the principal for.
     * \param out_principal on exit, a cc_string_t representing the principal of \a in_ccache.
     * \a out_principal must be released with cc_string_release().
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_get_principal(): Get the principal of a ccache.
     *
     * Return the principal for the ccache that was set via cc_context_create_ccache(),
     * cc_context_create_default_ccache(), cc_context_create_new_ccache(), or
     * cc_ccache_set_principal(). Principals for v4 and v5 are separate, but
     * should be kept synchronized for each ccache; they can be retrieved by
     * passing cc_credentials_v4 or cc_credentials_v5 in cred_vers. Passing
     * cc_credentials_v4_v5 will result in the error ccErrBadCredentialsVersion.
     */
    cc_int32 (*get_principal) (cc_ccache_t  in_ccache,
                               cc_uint32    in_credentials_version,
                               cc_string_t *out_principal);


    /*!
     * \param in_ccache a ccache object.
     * \param in_credentials_version the credentials version to set the principal for.
     * \param in_principal a C string representing the new principal of \a in_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_set_principal(): Set the principal of a ccache.
     *
     * Set the a principal for ccache. The v4 and v5 principals can be set
     * independently, but they should always be kept equal, up to differences in
     * string representation between v4 and v5. Passing cc_credentials_v4_v5 in
     * cred_vers will result in the error ccErrBadCredentialsVersion.
     */
    cc_int32 (*set_principal) (cc_ccache_t  io_ccache,
                               cc_uint32    in_credentials_version,
                               const char  *in_principal);

    /*!
     * \param io_ccache a ccache object.
     * \param in_credentials_union the credentials to store in \a io_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_store_credentials(): Store credentials in a ccache.
     *
     * Store a copy of credentials in the ccache.
     *
     * See the description of the credentials types for the meaning of
     * cc_credentials_union fields.
     *
     * Before credentials of a specific credential type can be stored in a ccache,
     * the corresponding principal version has to be set. For example, before you can
     * store Kerberos v4 credentials in a ccache, the Kerberos v4 principal has to be set
     * either by cc_context_create_ccache(), cc_context_create_default_ccache(),
     * cc_context_create_new_ccache(), or cc_ccache_set_principal(); likewise for
     * Kerberos v5. Otherwise, ccErrBadCredentialsVersion is returned.
     */
    cc_int32 (*store_credentials) (cc_ccache_t                 io_ccache,
                                   const cc_credentials_union *in_credentials_union);

    /*!
     * \param io_ccache a ccache object.
     * \param in_credentials the credentials to remove from \a io_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_remove_credentials(): Remove credentials from a ccache.
     *
     * Removes credentials from a ccache. Note that credentials must be previously
     * acquired from the CCache API; only exactly matching credentials will be
     * removed. (This places the burden of determining exactly which credentials
     * to remove on the caller, but ensures there is no ambigity about which
     * credentials will be removed.) cc_credentials_t objects can be obtained by
     * iterating over the ccache's credentials with cc_ccache_new_credentials_iterator().
     *
     * If found, the credentials are removed from the ccache. The credentials
     * parameter is not modified and should be freed by the caller. It is
     * legitimate to call this function while an iterator is traversing the
     * ccache, and the deletion of a credential already returned by
     * cc_credentials_iterator_next() will not disturb sequence of credentials
     * returned by cc_credentials_iterator_next().
     */
    cc_int32 (*remove_credentials) (cc_ccache_t      io_ccache,
                                    cc_credentials_t in_credentials);

    /*!
     * \param in_ccache a ccache object.
     * \param out_credentials_iterator a credentials iterator for \a io_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_new_credentials_iterator(): Iterate over credentials in a ccache.
     *
     * Allocates memory for iterator and initializes it. Successive calls to
     * cc_credentials_iterator_next() will return credentials from the ccache.
     *
     * If changes are made to the ccache while an iterator is being used on it,
     * the iterator must return at least the intersection, and at most the union,
     * of the set of credentials that were in the ccache when the iteration began
     * and the set of credentials that are in the ccache when it ends.
     */
    cc_int32 (*new_credentials_iterator) (cc_ccache_t                in_ccache,
                                          cc_credentials_iterator_t *out_credentials_iterator);

    /*!
     * \param io_source_ccache a ccache object to move.
     * \param io_destination_ccache a ccache object replace with the contents of \a io_source_ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_move(): Move the contents of one ccache into another, destroying the source.
     *
     * cc_ccache_move() atomically copies the credentials, credential versions and principals
     * from one ccache to another.  On successful completion \a io_source_ccache will be
     * released and the ccache it points to will be destroyed.  Any credentials previously
     * in \a io_destination_ccache will be replaced with credentials from \a io_source_ccache.
     * The only part of \a io_destination_ccache which remains constant is the name.  Any other
     * callers referring to \a io_destination_ccache will suddenly see new data in it.
     *
     * Typically cc_ccache_move() is used when the caller wishes to safely overwrite the
     * contents of a ccache with new data which requires several steps to generate.
     * cc_ccache_move() allows the caller to create a temporary ccache
     * (which can be destroyed if any intermediate step fails) and the atomically copy
     * the temporary cache into the destination.
     */
    cc_int32 (*move) (cc_ccache_t io_source_ccache,
                      cc_ccache_t io_destination_ccache);

    /*!
     * \param io_ccache the ccache object for the ccache you wish to lock.
     * \param in_lock_type the type of lock to obtain.
     * \param in_block whether or not the function should block if the lock cannot be obtained immediately.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_lock(): Lock a ccache.
     *
     * Attempts to acquire an advisory lock for a ccache. Allowed values for lock_type are:
     *
     * \li cc_lock_read: a read lock.
     * \li cc_lock_write: a write lock
     * \li cc_lock_upgrade: upgrade an already-obtained read lock to a write lock
     * \li cc_lock_downgrade: downgrade an already-obtained write lock to a read lock
     *
     * If block is cc_lock_block, lock() will not return until the lock is acquired.
     * If block is cc_lock_noblock, lock() will return immediately, either acquiring
     * the lock and returning ccNoError, or failing to acquire the lock and returning
     * an error explaining why.
     *
     * To avoid having to deal with differences between thread semantics on different
     * platforms, locks are granted per ccache, rather than per thread or per process.
     * That means that different threads of execution have to acquire separate contexts
     * in order to be able to synchronize with each other.
     *
     * The lock should be unlocked by using cc_ccache_unlock().
     *
     * \note All locks are advisory.  For example, callers which do not call
     * cc_ccache_lock() and cc_ccache_unlock() will not be prevented from writing
     * to the ccache when you have a read lock.  This is because the CCAPI
     * locking was added after the first release and thus adding mandatory locks would
     * have changed the user experience and performance of existing applications.
     */
    cc_int32 (*lock) (cc_ccache_t io_ccache,
                      cc_uint32   in_lock_type,
                      cc_uint32   in_block);

    /*!
     * \param io_ccache a ccache object.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_unlock(): Unlock a ccache.
     */
    cc_int32 (*unlock) (cc_ccache_t io_ccache);

    /*!
     * \param in_ccache a cache object.
     * \param out_last_default_time on exit, the last time the ccache was default.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_get_change_time(): Get the last time a ccache was the default ccache.
     *
     * This function returns the last time when the ccache was made the default ccache.
     * This allows clients to sort the ccaches by how recently they were default, which
     * is useful for user listing of ccaches. If the ccache was never default,
     * ccErrNeverDefault is returned.
     */
    cc_int32 (*get_last_default_time) (cc_ccache_t  in_ccache,
                                       cc_time_t   *out_last_default_time);

    /*!
     * \param in_ccache a cache object.
     * \param out_change_time on exit, the last time the ccache changed.
     * \return On success, #ccNoError.  If the ccache was never the default ccache,
     * #ccErrNeverDefault.  Otherwise, an error code representing the failure.
     * \brief \b cc_ccache_get_change_time(): Get the last time a ccache changed.
     *
     * This function returns the time of the most recent change made to a ccache.
     * By maintaining a local copy the caller can deduce whether or not the ccache has
     * been modified since the previous call to cc_ccache_get_change_time().
     *
     * The time returned by cc_ccache_get_change_time() increases whenever:
     *
     * \li a credential is stored
     * \li a credential is removed
     * \li a ccache principal is changed
     * \li the ccache becomes the default ccache
     * \li the ccache is no longer the default ccache
     *
     * \note In order to be able to compare two values returned by cc_ccache_get_change_time(),
     * the caller must use the same ccache object to acquire them. Callers should maintain a
     * single ccache object in memory for cc_ccache_get_change_time() calls rather than
     * creating a new ccache object for every call.
     *
     * \sa wait_for_change
     */
    cc_int32 (*get_change_time) (cc_ccache_t  in_ccache,
                                 cc_time_t   *out_change_time);

    /*!
     * \param in_ccache a ccache object.
     * \param in_compare_to_ccache a ccache object to compare with \a in_ccache.
     * \param out_equal on exit, whether or not the two ccaches refer to the same ccache.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_compare(): Compare two ccache objects.
     */
    cc_int32 (*compare) (cc_ccache_t  in_ccache,
                         cc_ccache_t  in_compare_to_ccache,
                         cc_uint32   *out_equal);

    /*!
     * \param in_ccache a ccache object.
     * \param in_credentials_version the credentials version to get the time offset for.
     * \param out_time_offset on exit, the KDC time offset for \a in_ccache for credentials version
     * \a in_credentials_version.
     * \return On success, #ccNoError if a time offset was obtained or #ccErrTimeOffsetNotSet
     * if a time offset has not been set.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_get_kdc_time_offset(): Get the KDC time offset for credentials in a ccache.
     * \sa set_kdc_time_offset, clear_kdc_time_offset
     *
     * Sometimes the KDC and client's clocks get out of sync.  cc_ccache_get_kdc_time_offset()
     * returns the difference between the KDC and client's clocks at the time credentials were
     * acquired.  This offset allows callers to figure out how much time is left on a given
     * credential even though the end_time is based on the KDC's clock not the client's clock.
     */
    cc_int32 (*get_kdc_time_offset) (cc_ccache_t  in_ccache,
                                     cc_uint32    in_credentials_version,
                                     cc_time_t   *out_time_offset);

    /*!
     * \param in_ccache a ccache object.
     * \param in_credentials_version the credentials version to get the time offset for.
     * \param in_time_offset the new KDC time offset for \a in_ccache for credentials version
     * \a in_credentials_version.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_set_kdc_time_offset(): Set the KDC time offset for credentials in a ccache.
     * \sa get_kdc_time_offset, clear_kdc_time_offset
     *
     * Sometimes the KDC and client's clocks get out of sync.  cc_ccache_set_kdc_time_offset()
     * sets the difference between the KDC and client's clocks at the time credentials were
     * acquired.  This offset allows callers to figure out how much time is left on a given
     * credential even though the end_time is based on the KDC's clock not the client's clock.
     */
    cc_int32 (*set_kdc_time_offset) (cc_ccache_t io_ccache,
                                     cc_uint32   in_credentials_version,
                                     cc_time_t   in_time_offset);

    /*!
     * \param in_ccache a ccache object.
     * \param in_credentials_version the credentials version to get the time offset for.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_clear_kdc_time_offset(): Clear the KDC time offset for credentials in a ccache.
     * \sa get_kdc_time_offset, set_kdc_time_offset
     *
     * Sometimes the KDC and client's clocks get out of sync.  cc_ccache_clear_kdc_time_offset()
     * clears the difference between the KDC and client's clocks at the time credentials were
     * acquired.  This offset allows callers to figure out how much time is left on a given
     * credential even though the end_time is based on the KDC's clock not the client's clock.
     */
    cc_int32 (*clear_kdc_time_offset) (cc_ccache_t io_ccache,
                                       cc_uint32   in_credentials_version);

    /*!
     * \param in_ccache a ccache object.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_wait_for_change(): Wait for the next change to a ccache.
     *
     * This function blocks until the next change is made to the ccache referenced by
     * \a in_ccache. By repeatedly calling cc_ccache_wait_for_change() from
     * a worker thread the caller can effectively receive callbacks whenever the
     * ccache changes.  This is considerably more efficient than polling
     * with cc_ccache_get_change_time().
     *
     * cc_ccache_wait_for_change() will return whenever:
     *
     * \li a credential is stored
     * \li a credential is removed
     * \li the ccache principal is changed
     * \li the ccache becomes the default ccache
     * \li the ccache is no longer the default ccache
     *
     * \note In order to make sure that the caller doesn't miss any changes,
     * cc_ccache_wait_for_change() always returns immediately after the first time it
     * is called on a new ccache object. Callers must use the same ccache object
     * for successive calls to cc_ccache_wait_for_change() rather than creating a new
     * ccache object for every call.
     *
     * \sa get_change_time
     */
    cc_int32 (*wait_for_change) (cc_ccache_t in_ccache);
};

/*!
 * Function pointer table for cc_string_t.  For more information see
 * \ref cc_string_reference.
 */
struct cc_string_f {
    /*!
     * \param io_string the string object to release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_string_release(): Release memory associated with a cc_string_t object.
     */
    cc_int32 (*release) (cc_string_t io_string);
};

/*!
 * Function pointer table for cc_credentials_t.  For more information see
 * \ref cc_credentials_reference.
 */
struct cc_credentials_f {
    /*!
     * \param io_credentials the credentials object to release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_credentials_release(): Release memory associated with a cc_credentials_t object.
     */
    cc_int32 (*release) (cc_credentials_t  io_credentials);

    /*!
     * \param in_credentials a credentials object.
     * \param in_compare_to_credentials a credentials object to compare with \a in_credentials.
     * \param out_equal on exit, whether or not the two credentials objects refer to the
     * same credentials in the cache collection.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_credentials_compare(): Compare two credentials objects.
     */
    cc_int32 (*compare) (cc_credentials_t  in_credentials,
                         cc_credentials_t  in_compare_to_credentials,
                         cc_uint32        *out_equal);
};

/*!
 * Function pointer table for cc_ccache_iterator_t.  For more information see
 * \ref cc_ccache_iterator_reference.
 */
struct cc_ccache_iterator_f {
    /*!
     * \param io_ccache_iterator the ccache iterator object to release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_iterator_release(): Release memory associated with a cc_ccache_iterator_t object.
     */
    cc_int32 (*release) (cc_ccache_iterator_t io_ccache_iterator);

    /*!
     * \param in_ccache_iterator a ccache iterator object.
     * \param out_ccache on exit, the next ccache in the cache collection.
     * \return On success, #ccNoError if the next ccache in the cache collection was
     * obtained or #ccIteratorEnd if there are no more ccaches.
     * On failure, an error code representing the failure.
     * \brief \b cc_ccache_iterator_next(): Get the next ccache in the cache collection.
     */
    cc_int32 (*next) (cc_ccache_iterator_t  in_ccache_iterator,
                      cc_ccache_t          *out_ccache);

    /*!
     * \param in_ccache_iterator a ccache iterator object.
     * \param out_ccache_iterator on exit, a copy of \a in_ccache_iterator.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_ccache_iterator_clone(): Make a copy of a ccache iterator.
     */
    cc_int32 (*clone) (cc_ccache_iterator_t  in_ccache_iterator,
                       cc_ccache_iterator_t *out_ccache_iterator);
};

/*!
 * Function pointer table for cc_credentials_iterator_t.  For more information see
 * \ref cc_credentials_iterator_reference.
 */
struct cc_credentials_iterator_f {
    /*!
     * \param io_credentials_iterator the credentials iterator object to release.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_credentials_iterator_release(): Release memory associated with a cc_credentials_iterator_t object.
     */
    cc_int32 (*release) (cc_credentials_iterator_t io_credentials_iterator);

    /*!
     * \param in_credentials_iterator a credentials iterator object.
     * \param out_credentials on exit, the next credentials in the ccache.
     * \return On success, #ccNoError if the next credential in the ccache was obtained
     * or #ccIteratorEnd if there are no more credentials.
     * On failure, an error code representing the failure.
     * \brief \b cc_credentials_iterator_next(): Get the next credentials in the ccache.
     */
    cc_int32 (*next) (cc_credentials_iterator_t  in_credentials_iterator,
                      cc_credentials_t          *out_credentials);

    /*!
     * \ingroup cc_credentials_iterator_reference
     * \param in_credentials_iterator a credentials iterator object.
     * \param out_credentials_iterator on exit, a copy of \a in_credentials_iterator.
     * \return On success, #ccNoError.  On failure, an error code representing the failure.
     * \brief \b cc_credentials_iterator_clone(): Make a copy of a credentials iterator.
     */
    cc_int32 (*clone) (cc_credentials_iterator_t  in_credentials_iterator,
                       cc_credentials_iterator_t *out_credentials_iterator);
};

/*!
 * \ingroup cc_context_reference
 * \param out_context on exit, a new context object.  Must be free with cc_context_release().
 * \param in_version  the requested API version.  This should be the maximum version the
 * application supports.
 * \param out_supported_version if non-NULL, on exit contains the maximum API version
 * supported by the implementation.
 * \param out_vendor if non-NULL, on exit contains a pointer to a read-only C string which
 * contains a string describing the vendor which implemented the credentials cache API.
 * \return On success, #ccNoError.  On failure, an error code representing the failure.
 * May return CCAPI v2 error CC_BAD_API_VERSION if #ccapi_version_2 is passed in.
 * \brief Initialize a new cc_context.
 */
CCACHE_API cc_int32 cc_initialize (cc_context_t  *out_context,
                                   cc_int32       in_version,
                                   cc_int32      *out_supported_version,
                                   char const   **out_vendor);


/*! \defgroup helper_macros CCAPI Function Helper Macros
 * @{ */

/*! Helper macro for cc_context_f release() */
#define         cc_context_release(context)             \
    ((context) -> functions -> release (context))
/*! Helper macro for cc_context_f get_change_time() */
#define         cc_context_get_change_time(context, change_time)        \
    ((context) -> functions -> get_change_time (context, change_time))
/*! Helper macro for cc_context_f get_default_ccache_name() */
#define         cc_context_get_default_ccache_name(context, name)       \
    ((context) -> functions -> get_default_ccache_name (context, name))
/*! Helper macro for cc_context_f open_ccache() */
#define         cc_context_open_ccache(context, name, ccache)           \
    ((context) -> functions -> open_ccache (context, name, ccache))
/*! Helper macro for cc_context_f open_default_ccache() */
#define         cc_context_open_default_ccache(context, ccache)         \
    ((context) -> functions -> open_default_ccache (context, ccache))
/*! Helper macro for cc_context_f create_ccache() */
#define         cc_context_create_ccache(context, name, version, principal, ccache) \
    ((context) -> functions -> create_ccache (context, name, version, principal, ccache))
/*! Helper macro for cc_context_f create_default_ccache() */
#define         cc_context_create_default_ccache(context, version, principal, ccache) \
    ((context) -> functions -> create_default_ccache (context, version, principal, ccache))
/*! Helper macro for cc_context_f create_new_ccache() */
#define         cc_context_create_new_ccache(context, version, principal, ccache) \
    ((context) -> functions -> create_new_ccache (context, version, principal, ccache))
/*! Helper macro for cc_context_f new_ccache_iterator() */
#define         cc_context_new_ccache_iterator(context, iterator)       \
    ((context) -> functions -> new_ccache_iterator (context, iterator))
/*! Helper macro for cc_context_f lock() */
#define         cc_context_lock(context, type, block)           \
    ((context) -> functions -> lock (context, type, block))
/*! Helper macro for cc_context_f unlock() */
#define         cc_context_unlock(context)              \
    ((context) -> functions -> unlock (context))
/*! Helper macro for cc_context_f compare() */
#define         cc_context_compare(context, compare_to, equal)          \
    ((context) -> functions -> compare (context, compare_to, equal))
/*! Helper macro for cc_context_f wait_for_change() */
#define         cc_context_wait_for_change(context)             \
    ((context) -> functions -> wait_for_change (context))

/*! Helper macro for cc_ccache_f release() */
#define         cc_ccache_release(ccache)       \
    ((ccache) -> functions -> release (ccache))
/*! Helper macro for cc_ccache_f destroy() */
#define         cc_ccache_destroy(ccache)       \
    ((ccache) -> functions -> destroy (ccache))
/*! Helper macro for cc_ccache_f set_default() */
#define         cc_ccache_set_default(ccache)           \
    ((ccache) -> functions -> set_default (ccache))
/*! Helper macro for cc_ccache_f get_credentials_version() */
#define         cc_ccache_get_credentials_version(ccache, version)      \
    ((ccache) -> functions -> get_credentials_version (ccache, version))
/*! Helper macro for cc_ccache_f get_name() */
#define         cc_ccache_get_name(ccache, name)        \
    ((ccache) -> functions -> get_name (ccache, name))
/*! Helper macro for cc_ccache_f get_principal() */
#define         cc_ccache_get_principal(ccache, version, principal)     \
    ((ccache) -> functions -> get_principal (ccache, version, principal))
/*! Helper macro for cc_ccache_f set_principal() */
#define         cc_ccache_set_principal(ccache, version, principal)     \
    ((ccache) -> functions -> set_principal (ccache, version, principal))
/*! Helper macro for cc_ccache_f store_credentials() */
#define         cc_ccache_store_credentials(ccache, credentials)        \
    ((ccache) -> functions -> store_credentials (ccache, credentials))
/*! Helper macro for cc_ccache_f remove_credentials() */
#define         cc_ccache_remove_credentials(ccache, credentials)       \
    ((ccache) -> functions -> remove_credentials (ccache, credentials))
/*! Helper macro for cc_ccache_f new_credentials_iterator() */
#define         cc_ccache_new_credentials_iterator(ccache, iterator)    \
    ((ccache) -> functions -> new_credentials_iterator (ccache, iterator))
/*! Helper macro for cc_ccache_f lock() */
#define         cc_ccache_lock(ccache, type, block)             \
    ((ccache) -> functions -> lock (ccache, type, block))
/*! Helper macro for cc_ccache_f unlock() */
#define         cc_ccache_unlock(ccache)        \
    ((ccache) -> functions -> unlock (ccache))
/*! Helper macro for cc_ccache_f get_last_default_time() */
#define         cc_ccache_get_last_default_time(ccache, last_default_time) \
    ((ccache) -> functions -> get_last_default_time (ccache, last_default_time))
/*! Helper macro for cc_ccache_f get_change_time() */
#define         cc_ccache_get_change_time(ccache, change_time)          \
    ((ccache) -> functions -> get_change_time (ccache, change_time))
/*! Helper macro for cc_ccache_f move() */
#define         cc_ccache_move(source, destination)             \
    ((source) -> functions -> move (source, destination))
/*! Helper macro for cc_ccache_f compare() */
#define         cc_ccache_compare(ccache, compare_to, equal)            \
    ((ccache) -> functions -> compare (ccache, compare_to, equal))
/*! Helper macro for cc_ccache_f get_kdc_time_offset() */
#define         cc_ccache_get_kdc_time_offset(ccache, version, time_offset) \
    ((ccache) -> functions -> get_kdc_time_offset (ccache, version, time_offset))
/*! Helper macro for cc_ccache_f set_kdc_time_offset() */
#define         cc_ccache_set_kdc_time_offset(ccache, version, time_offset) \
    ((ccache) -> functions -> set_kdc_time_offset (ccache, version, time_offset))
/*! Helper macro for cc_ccache_f clear_kdc_time_offset() */
#define         cc_ccache_clear_kdc_time_offset(ccache, version)        \
    ((ccache) -> functions -> clear_kdc_time_offset (ccache, version))
/*! Helper macro for cc_ccache_f wait_for_change() */
#define         cc_ccache_wait_for_change(ccache)       \
    ((ccache) -> functions -> wait_for_change (ccache))

/*! Helper macro for cc_string_f release() */
#define         cc_string_release(string)       \
    ((string) -> functions -> release (string))

/*! Helper macro for cc_credentials_f release() */
#define         cc_credentials_release(credentials)             \
    ((credentials) -> functions -> release (credentials))
/*! Helper macro for cc_credentials_f compare() */
#define         cc_credentials_compare(credentials, compare_to, equal)  \
    ((credentials) -> functions -> compare (credentials, compare_to, equal))

/*! Helper macro for cc_ccache_iterator_f release() */
#define         cc_ccache_iterator_release(iterator)    \
    ((iterator) -> functions -> release (iterator))
/*! Helper macro for cc_ccache_iterator_f next() */
#define         cc_ccache_iterator_next(iterator, ccache)       \
    ((iterator) -> functions -> next (iterator, ccache))
/*! Helper macro for cc_ccache_iterator_f clone() */
#define         cc_ccache_iterator_clone(iterator, new_iterator)        \
    ((iterator) -> functions -> clone (iterator, new_iterator))

/*! Helper macro for cc_credentials_iterator_f release() */
#define         cc_credentials_iterator_release(iterator)       \
    ((iterator) -> functions -> release (iterator))
/*! Helper macro for cc_credentials_iterator_f next() */
#define         cc_credentials_iterator_next(iterator, credentials)     \
    ((iterator) -> functions -> next (iterator, credentials))
/*! Helper macro for cc_credentials_iterator_f clone() */
#define         cc_credentials_iterator_clone(iterator, new_iterator)   \
    ((iterator) -> functions -> clone (iterator, new_iterator))
/*!@}*/

#if TARGET_OS_MAC
#pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CREDENTIALSCACHE__ */
