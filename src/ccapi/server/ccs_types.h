/* ccapi/server/ccs_types.h */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
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

#ifndef CCS_TYPES_H
#define CCS_TYPES_H

#ifdef WIN32
#pragma warning ( disable : 4068)
#endif

#include "cci_types.h"

struct cci_array_d;

typedef struct cci_array_d *ccs_client_array_t;

typedef struct cci_array_d *ccs_callback_array_t;

typedef struct cci_array_d *ccs_callbackref_array_t;

typedef struct cci_array_d *ccs_iteratorref_array_t;

typedef struct cci_array_d *ccs_lock_array_t;

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ccs_os_pipe_t is IPC-specific so it's special cased here */

#if TARGET_OS_MAC
#include <mach/mach_types.h>
typedef mach_port_t ccs_pipe_t;  /* Mach IPC port */
#define CCS_PIPE_NULL MACH_PORT_NULL

#else

#ifdef WIN32
/* On Windows, a pipe is s struct: */
#include "ccs_win_pipe.h"
typedef struct ccs_win_pipe_t* ccs_pipe_t;
#define CCS_PIPE_NULL (ccs_pipe_t)NULL

#else
typedef int ccs_pipe_t; /* Unix domain socket */
#define CCS_PIPE_NULL -1

#endif
#endif

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

struct ccs_callback_d;
typedef struct ccs_callback_d *ccs_callback_t;

struct ccs_list_d;
struct ccs_list_iterator_d;

/* Used for iterator array invalidate function */
typedef struct ccs_list_iterator_d *ccs_generic_list_iterator_t;

typedef struct ccs_list_d *ccs_cache_collection_list_t;

typedef struct ccs_list_d *ccs_ccache_list_t;
typedef struct ccs_list_iterator_d *ccs_ccache_list_iterator_t;

typedef struct ccs_list_d *ccs_credentials_list_t;
typedef struct ccs_list_iterator_d *ccs_credentials_list_iterator_t;

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

struct ccs_client_d;
typedef struct ccs_client_d *ccs_client_t;

struct ccs_lock_d;
typedef struct ccs_lock_d *ccs_lock_t;

struct ccs_lock_state_d;
typedef struct ccs_lock_state_d *ccs_lock_state_t;

struct ccs_credentials_d;
typedef struct ccs_credentials_d *ccs_credentials_t;

typedef ccs_credentials_list_iterator_t ccs_credentials_iterator_t;

struct ccs_ccache_d;
typedef struct ccs_ccache_d *ccs_ccache_t;

typedef ccs_ccache_list_iterator_t ccs_ccache_iterator_t;

struct ccs_cache_collection_d;
typedef struct ccs_cache_collection_d *ccs_cache_collection_t;

#endif /* CCS_TYPES_H */
