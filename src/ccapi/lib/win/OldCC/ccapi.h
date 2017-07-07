/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0366 */
/* at Fri Nov 30 10:06:16 2007
 */
/* Compiler settings for ccapi.idl:
    Oic, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, oldnames
    error checks: allocation ref bounds_check enum stub_data
    VC __declspec() decoration level:
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __ccapi_h__
#define __ccapi_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */

#ifdef __cplusplus
extern "C"{
#endif

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * );

#ifndef __ccapi_INTERFACE_DEFINED__
#define __ccapi_INTERFACE_DEFINED__

/* interface ccapi */
/* [implicit_handle][unique][version][uuid] */

typedef /* [context_handle] */ struct opaque_handle_CTX *HCTX;

typedef /* [context_handle] */ struct opaque_handle_CACHE *HCACHE;

typedef /* [context_handle] */ struct opaque_handle_CACHE_ITER *HCACHE_ITER;

typedef /* [context_handle] */ struct opaque_handle_CRED_ITER *HCRED_ITER;

typedef unsigned char CC_CHAR;

typedef unsigned char CC_UCHAR;

typedef int CC_INT32;

typedef unsigned int CC_UINT32;

typedef CC_INT32 CC_TIME_T;


enum __MIDL_ccapi_0001
    {	STK_AFS	= 0,
	STK_DES	= 1
    } ;

enum __MIDL_ccapi_0002
    {	CC_API_VER_1	= 1,
	CC_API_VER_2	= 2
    } ;

enum __MIDL_ccapi_0003
    {	KRB_NAME_SZ	= 40,
	KRB_INSTANCE_SZ	= 40,
	KRB_REALM_SZ	= 40,
	MAX_V4_CRED_LEN	= 1250
    } ;
typedef struct _NC_INFO
    {
    /* [string] */ CC_CHAR *name;
    /* [string] */ CC_CHAR *principal;
    CC_INT32 vers;
    } 	NC_INFO;

typedef struct _NC_INFO_LIST
    {
    CC_UINT32 length;
    /* [size_is] */ NC_INFO *info;
    } 	NC_INFO_LIST;

typedef struct _V4_CRED
    {
    CC_UCHAR kversion;
    CC_CHAR principal[ 41 ];
    CC_CHAR principal_instance[ 41 ];
    CC_CHAR service[ 41 ];
    CC_CHAR service_instance[ 41 ];
    CC_CHAR realm[ 41 ];
    CC_UCHAR session_key[ 8 ];
    CC_INT32 kvno;
    CC_INT32 str_to_key;
    CC_INT32 issue_date;
    CC_INT32 lifetime;
    CC_UINT32 address;
    CC_INT32 ticket_sz;
    CC_UCHAR ticket[ 1250 ];
    } 	V4_CRED;

typedef struct _CC_DATA
    {
    CC_UINT32 type;
    CC_UINT32 length;
    /* [size_is] */ CC_UCHAR *data;
    } 	CC_DATA;

typedef struct _CC_DATA_LIST
    {
    CC_UINT32 count;
    /* [size_is] */ CC_DATA *data;
    } 	CC_DATA_LIST;

typedef struct _V5_CRED
    {
    /* [string] */ CC_CHAR *client;
    /* [string] */ CC_CHAR *server;
    CC_DATA keyblock;
    CC_TIME_T authtime;
    CC_TIME_T starttime;
    CC_TIME_T endtime;
    CC_TIME_T renew_till;
    CC_UINT32 is_skey;
    CC_UINT32 ticket_flags;
    CC_DATA_LIST addresses;
    CC_DATA ticket;
    CC_DATA second_ticket;
    CC_DATA_LIST authdata;
    } 	V5_CRED;

typedef /* [switch_type] */ union _CRED_PTR_UNION
    {
    /* [case()] */ V4_CRED *pV4Cred;
    /* [case()] */ V5_CRED *pV5Cred;
    } 	CRED_PTR_UNION;

typedef struct _CRED_UNION
    {
    CC_INT32 cred_type;
    /* [switch_is] */ CRED_PTR_UNION cred;
    } 	CRED_UNION;

CC_INT32 rcc_initialize(
    /* [out] */ HCTX *pctx);

CC_INT32 rcc_shutdown(
    /* [out][in] */ HCTX *pctx);

CC_INT32 rcc_get_change_time(
    /* [in] */ HCTX ctx,
    /* [out] */ CC_TIME_T *time);

CC_INT32 rcc_create(
    /* [in] */ HCTX ctx,
    /* [string][in] */ const CC_CHAR *name,
    /* [string][in] */ const CC_CHAR *principal,
    /* [in] */ CC_INT32 vers,
    /* [in] */ CC_UINT32 flags,
    /* [out] */ HCACHE *pcache);

CC_INT32 rcc_open(
    /* [in] */ HCTX ctx,
    /* [string][in] */ const CC_CHAR *name,
    /* [in] */ CC_INT32 vers,
    /* [in] */ CC_UINT32 flags,
    /* [out] */ HCACHE *pcache);

CC_INT32 rcc_close(
    /* [out][in] */ HCACHE *pcache);

CC_INT32 rcc_destroy(
    /* [out][in] */ HCACHE *pcache);

CC_INT32 rcc_seq_fetch_NCs_begin(
    /* [in] */ HCTX ctx,
    /* [out] */ HCACHE_ITER *piter);

CC_INT32 rcc_seq_fetch_NCs_end(
    /* [out][in] */ HCACHE_ITER *piter);

CC_INT32 rcc_seq_fetch_NCs_next(
    /* [in] */ HCACHE_ITER iter,
    /* [out] */ HCACHE *pcache);

CC_INT32 rcc_seq_fetch_NCs(
    /* [in] */ HCTX ctx,
    /* [out][in] */ HCACHE_ITER *piter,
    /* [out] */ HCACHE *pcache);

CC_INT32 rcc_get_NC_info(
    /* [in] */ HCTX ctx,
    /* [out] */ NC_INFO_LIST **info_list);

CC_INT32 rcc_get_name(
    /* [in] */ HCACHE cache,
    /* [string][out] */ CC_CHAR **name);

CC_INT32 rcc_set_principal(
    /* [in] */ HCACHE cache,
    /* [in] */ CC_INT32 vers,
    /* [string][in] */ const CC_CHAR *principal);

CC_INT32 rcc_get_principal(
    /* [in] */ HCACHE cache,
    /* [string][out] */ CC_CHAR **principal);

CC_INT32 rcc_get_cred_version(
    /* [in] */ HCACHE cache,
    /* [out] */ CC_INT32 *vers);

CC_INT32 rcc_lock_request(
    /* [in] */ HCACHE cache,
    /* [in] */ CC_INT32 lock_type);

CC_INT32 rcc_store(
    /* [in] */ HCACHE cache,
    /* [in] */ CRED_UNION cred);

CC_INT32 rcc_remove_cred(
    /* [in] */ HCACHE cache,
    /* [in] */ CRED_UNION cred);

CC_INT32 rcc_seq_fetch_creds(
    /* [in] */ HCACHE cache,
    /* [out][in] */ HCRED_ITER *piter,
    /* [out] */ CRED_UNION **cred);

CC_INT32 rcc_seq_fetch_creds_begin(
    /* [in] */ HCACHE cache,
    /* [out] */ HCRED_ITER *piter);

CC_INT32 rcc_seq_fetch_creds_end(
    /* [out][in] */ HCRED_ITER *piter);

CC_INT32 rcc_seq_fetch_creds_next(
    /* [in] */ HCRED_ITER iter,
    /* [out] */ CRED_UNION **cred);

CC_UINT32 Connect(
    /* [string][in] */ CC_CHAR *name);

void Shutdown( void);


extern handle_t ccapi_IfHandle;


extern RPC_IF_HANDLE ccapi_ClientIfHandle;
extern RPC_IF_HANDLE ccapi_ServerIfHandle;
#endif /* __ccapi_INTERFACE_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

void __RPC_USER HCTX_rundown( HCTX );
void __RPC_USER HCACHE_rundown( HCACHE );
void __RPC_USER HCACHE_ITER_rundown( HCACHE_ITER );
void __RPC_USER HCRED_ITER_rundown( HCRED_ITER );

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif
