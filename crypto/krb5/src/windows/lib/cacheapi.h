/* windows/lib/cacheapi.h */
/*
 * Copyright 1997 by the Regents of the University of Michigan
 *
 * This software is being provided to you, the LICENSEE, by the
 * Regents of the University of Michigan (UM) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee or royalty is hereby
 * granted, provided that you agree to comply with the following copyright
 * notice and statements, including the disclaimer, and that the same
 * appear on ALL copies of the software and documentation, including
 * modifications that you make for internal use or for distribution:
 *
 * Copyright 1997 by the Regents of the University of Michigan.
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND UM MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, UM MAKES NO REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY
 * OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED
 * SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
 * COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the University of Michigan or UM may NOT be used in
 * advertising or publicity pertaining to distribution of the software.
 * Title to copyright in this software and any associated documentation
 * shall at all times remain with UM, and USER agrees to preserve same.
 *
 * The University of Michigan
 * c/o Steve Rothwell <sgr@umich.edu>
 * 535 W. William Street
 * Ann Arbor, Michigan 48013-4943
 * U.S.A.
 */

/*
**  CacheAPI.h
**
**      The externally visible functions and data structures
**      for the Kerberos Common Cache DLL
**      This should be the ONLY externally visible file.
**      This is ALL anyone should need to call the API.
**
**
*/

#ifndef Krb_CCacheAPI_h_
#define Krb_CCacheAPI_h_

#include <windows.h>

//typedef int cc_int32;
#define cc_int32  long
#define cc_uint32 unsigned long

typedef cc_int32  cc_time_t;

#define CC_API_VER_1	1
#define CC_API_VER_2	2

//enum {
//	CC_API_VER_1 = 1,
//	CC_API_VER_2 = 2
//};

#define CCACHE_API __declspec(dllexport) cc_int32

/*
** The Official Error Codes
*/
#define CC_NOERROR           0
#define CC_BADNAME           1
#define CC_NOTFOUND          2
#define CC_END               3
#define CC_IO                4
#define CC_WRITE             5
#define CC_NOMEM             6
#define CC_FORMAT            7
#define CC_LOCKED            8
#define CC_BAD_API_VERSION   9
#define CC_NO_EXIST          10
#define CC_NOT_SUPP          11
#define CC_BAD_PARM          12
#define CC_ERR_CACHE_ATTACH  13
#define CC_ERR_CACHE_RELEASE 14
#define CC_ERR_CACHE_FULL    15
#define CC_ERR_CRED_VERSION  16

/*
** types, structs, & constants
*/
// Flag bits promised by Ted "RSN"
#define CC_FLAGS_RESERVED 0xFFFFFFFF

typedef cc_uint32 cc_nc_flags;       // set via constants above

typedef struct opaque_dll_control_block_type* apiCB;
typedef struct opaque_ccache_pointer_type* ccache_p;
typedef struct opaque_credential_iterator_type* ccache_cit;

typedef struct _cc_data
{
    cc_uint32       type;		// should be one of _cc_data_type
    cc_uint32       length;
    unsigned char*  data;		// the proverbial bag-o-bits
} cc_data;

// V5 Credentials
typedef struct _cc_creds {
    char*           client;
    char*           server;
    cc_data         keyblock;
    cc_time_t       authtime;
    cc_time_t       starttime;
    cc_time_t       endtime;
    cc_time_t       renew_till;
    cc_uint32       is_skey;
    cc_uint32       ticket_flags;
    cc_data **  addresses;
    cc_data         ticket;
    cc_data         second_ticket;
    cc_data **  authdata;
} cc_creds;


typedef union cred_ptr_union_type {
    cc_creds*    pV5Cred;
} cred_ptr_union;

typedef struct cred_union_type {
    cc_int32        cred_type;
    cred_ptr_union  cred;
} cred_union;

typedef struct _infoNC {
    char*     name;
    char*     principal;
    cc_int32  vers;
} infoNC;

/*
** The official (externally visible) API
*/

#ifdef __cplusplus
extern "C" /* this entire list of functions */
{
#endif /* __cplusplus */

/*
** Main cache routines : initialize, shutdown, get_cache_names, & get_change_time
*/
CCACHE_API
cc_initialize(
    apiCB** cc_ctx,           // <  DLL's primary control structure.
                              //    returned here, passed everywhere else
    cc_int32 api_version,     // >  ver supported by caller (use CC_API_VER_1)
    cc_int32*  api_supported, // <  if ~NULL, max ver supported by DLL
    const char** vendor       // <  if ~NULL, vendor name in read only C string
    );

CCACHE_API
cc_shutdown(
    apiCB** cc_ctx            // <> DLL's primary control structure. NULL after call.
    );

CCACHE_API
cc_get_change_time(
    apiCB* cc_ctx,       // >  DLL's primary control structure
    cc_time_t* time      // <  time of last change to main cache
    );

/*
** Named Cache (NC) routines
**   create, open, close, destroy, get_principal, get_cred_version, &
**   lock_request
**
** Multiple NCs are allowed within the main cache.  Each has a Name and
** kerberos version # (V5).  Caller gets "ccache_ptr"s for NCs.
*/
CCACHE_API
cc_create(
    apiCB* cc_ctx,          // >  DLL's primary control structure
    const char* name,       // >  name of cache to be [destroyed if exists, then] created
    const char* principal,
    cc_int32 vers,          // >  ticket version (CC_CRED_V5)
    cc_uint32 cc_flags,     // >  options
    ccache_p** ccache_ptr   // <  NC control structure
    );

CCACHE_API
cc_open(
    apiCB* cc_ctx,          // >  DLL's primary control structure
    const char* name,       // >  name of pre-created cache
    cc_int32 vers,          // >  ticket version (CC_CRED_V5)
    cc_uint32 cc_flags,     // >  options
    ccache_p** ccache_ptr   // <  NC control structure
    );

CCACHE_API
cc_close(
    apiCB* cc_ctx,         // >  DLL's primary control structure
    ccache_p** ccache_ptr  // <> NC control structure. NULL after call.
    );

CCACHE_API
cc_destroy(
    apiCB* cc_ctx,         // >  DLL's primary control structure
    ccache_p** ccache_ptr  // <> NC control structure. NULL after call.
    );

/*
** Ways to get information about the NCs
*/

CCACHE_API
cc_seq_fetch_NCs_begin(
    apiCB* cc_ctx,
    ccache_cit** itNCs
    );

CCACHE_API
cc_seq_fetch_NCs_end(
    apiCB* cc_ctx,
    ccache_cit** itNCs
    );

CCACHE_API
cc_seq_fetch_NCs_next(
    apiCB* cc_ctx,
    ccache_p** ccache_ptr,
    ccache_cit* itNCs
    );

CCACHE_API
cc_seq_fetch_NCs(
    apiCB* cc_ctx,         // >  DLL's primary control structure
    ccache_p** ccache_ptr, // <  NC control structure (free via cc_close())
    ccache_cit** itNCs     // <> iterator used by DLL,
                           //    set to NULL before first call
                           //    returned NULL at CC_END
    );

CCACHE_API
cc_get_NC_info(
    apiCB* cc_ctx,          // >  DLL's primary control structure
    struct _infoNC*** ppNCi // <  (NULL before call) null terminated,
                            //    list of a structs (free via cc_free_infoNC())
    );

CCACHE_API
cc_free_NC_info(
    apiCB* cc_ctx,
    struct _infoNC*** ppNCi // <  free list of structs returned by
                            //    cc_get_cache_names().  set to NULL on return
    );

/*
** Functions that provide distinguishing characteristics of NCs.
*/

CCACHE_API
cc_get_name(
    apiCB* cc_ctx,              // > DLL's primary control structure
    const ccache_p* ccache_ptr, // > NC control structure
    char** name                 // < name of NC associated with ccache_ptr
                                //   (free via cc_free_name())
    );

CCACHE_API
cc_set_principal(
    apiCB* cc_ctx,                  // > DLL's primary control structure
    const ccache_p* ccache_pointer, // > NC control structure
    const cc_int32 vers,
    const char* principal           // > name of principal associated with NC
                                    //   Free via cc_free_principal()
    );

CCACHE_API
cc_get_principal(
    apiCB* cc_ctx,                  // > DLL's primary control structure
    const ccache_p* ccache_pointer, // > NC control structure
    char** principal                // < name of principal associated with NC
                                    //   Free via cc_free_principal()
    );

CCACHE_API
cc_get_cred_version(
    apiCB* cc_ctx,              // > DLL's primary control structure
    const ccache_p* ccache_ptr, // > NC control structure
    cc_int32* vers              // < ticket version associated with NC
    );

#define CC_LOCK_UNLOCK   1
#define CC_LOCK_READER   2
#define CC_LOCK_WRITER   3
#define CC_LOCK_NOBLOCK 16

CCACHE_API
cc_lock_request(
    apiCB* cc_ctx,     	        // > DLL's primary control structure
    const ccache_p* ccache_ptr, // > NC control structure
    const cc_int32 lock_type    // > one (or combination) of above defined
                                //   lock types
    );

/*
** Credentials routines (work within an NC)
** store, remove_cred, seq_fetch_creds
*/
CCACHE_API
cc_store(
    apiCB* cc_ctx,               // > DLL's primary control structure
    ccache_p* ccache_ptr,        // > NC control structure
    const cred_union creds       // > credentials to be copied into NC
    );

CCACHE_API
cc_remove_cred(
    apiCB* cc_ctx,            // > DLL's primary control structure
    ccache_p* ccache_ptr,     // > NC control structure
    const cred_union cred     // > credentials to remove from NC
    );

CCACHE_API
cc_seq_fetch_creds(
    apiCB* cc_ctx,              // > DLL's primary control structure
    const ccache_p* ccache_ptr, // > NC control structure
    cred_union** creds,         // < filled in by DLL, free via cc_free_creds()
    ccache_cit** itCreds        // <> iterator used by DLL, set to NULL
                                //    before first call -- Also NULL for final
                                //    call if loop ends before CC_END
    );

CCACHE_API
cc_seq_fetch_creds_begin(
    apiCB* cc_ctx,
    const ccache_p* ccache_ptr,
    ccache_cit** itCreds
    );

CCACHE_API
cc_seq_fetch_creds_end(
    apiCB* cc_ctx,
    ccache_cit** itCreds
    );

CCACHE_API
cc_seq_fetch_creds_next(
    apiCB* cc_ctx,
    cred_union** cred,
    ccache_cit* itCreds
    );

/*
** methods of liberation,
** or freeing space via the free that goes with the malloc used to get it
** It's important to use the free carried in the DLL, not the one supplied
** by your compiler vendor.
**
** freeing a NULL pointer is not treated as an error
*/
CCACHE_API
cc_free_principal(
    apiCB* cc_ctx,   // >  DLL's primary control structure
    char** principal // <> ptr to principal to be freed, returned as NULL
                     //    (from cc_get_principal())
    );

CCACHE_API
cc_free_name(
    apiCB* cc_ctx,   // >  DLL's primary control structure
    char** name      // <> ptr to name to be freed, returned as NULL
                     //    (from cc_get_name())
    );

CCACHE_API
cc_free_creds(
    apiCB* cc_ctx,     // > DLL's primary control structure
    cred_union** pCred // <> cred (from cc_seq_fetch_creds()) to be freed
                       //    Returned as NULL.
    );

#ifdef __cplusplus
} /* end extern "C" */
#endif /* __cplusplus */

#endif /* Krb_CCacheAPI_h_ */
