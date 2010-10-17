/*
 * Copyright (c) 2002-2006, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under all of the following licenses:
 * 
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see 
 *    http://www.opensource.org/licenses/cpl.php.
 * 
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 * 
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is in the file LICENSE3.txt in the root directory. The
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 * 
 * Licensee has the right to choose one of the above licenses.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain both the above copyright
 * notice and one of the license notices.
 * 
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Network Appliance, Inc. nor the names of other DAT
 * Collaborative contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 */

/****************************************************************
 *
 * HEADER: udat.h
 *
 * PURPOSE: defines the user DAT API
 *
 * Description: Header file for "uDAPL: User Direct Access Programming
 * 				Library, Version: 2.0"
 *
 * Mapping rules:
 *   All global symbols are prepended with DAT_ or dat_
 *   All DAT objects have an 'api' tag which, such as 'ep' or 'lmr'
 *   The method table is in the provider definition structure.
 *
 ***************************************************************/

#ifndef _UDAT_H_
#define _UDAT_H_

#include <dat2/udat_config.h>
#include <dat2/dat_platform_specific.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum dat_mem_type
{
        /* Shared between udat and kdat */
    DAT_MEM_TYPE_VIRTUAL              = 0x00,
    DAT_MEM_TYPE_LMR                  = 0x01,
        /* udat specific */
    DAT_MEM_TYPE_SHARED_VIRTUAL       = 0x02
} DAT_MEM_TYPE;

/* dat handle types */
typedef enum dat_handle_type
{
    DAT_HANDLE_TYPE_CR,
    DAT_HANDLE_TYPE_EP,
    DAT_HANDLE_TYPE_EVD,
    DAT_HANDLE_TYPE_IA,
    DAT_HANDLE_TYPE_LMR,
    DAT_HANDLE_TYPE_PSP,
    DAT_HANDLE_TYPE_PZ,
    DAT_HANDLE_TYPE_RMR,
    DAT_HANDLE_TYPE_RSP,
    DAT_HANDLE_TYPE_CNO,
    DAT_HANDLE_TYPE_SRQ,
    DAT_HANDLE_TYPE_CSP
#ifdef DAT_EXTENSIONS
    ,DAT_HANDLE_TYPE_EXTENSION_BASE
#endif
} DAT_HANDLE_TYPE;

/* EVD state consists of three orthogonal substates.  One for
 * enabled/disabled,one for waitable/unwaitable, and one for
 * configuration.  Within each substate the values are mutually
 * exclusive.
 */
typedef enum dat_evd_state
{
    DAT_EVD_STATE_ENABLED             = 0x01,
    DAT_EVD_STATE_DISABLED            = 0x02,
    DAT_EVD_STATE_WAITABLE            = 0x04,
    DAT_EVD_STATE_UNWAITABLE          = 0x08,
    DAT_EVD_STATE_CONFIG_NOTIFY       = 0x10,
    DAT_EVD_STATE_CONFIG_SOLICITED    = 0x20,
    DAT_EVD_STATE_CONFIG_THRESHOLD    = 0x30
} DAT_EVD_STATE;

typedef enum dat_evd_param_mask
{
    DAT_EVD_FIELD_IA_HANDLE           = 0x01,
    DAT_EVD_FIELD_EVD_QLEN            = 0x02,
    DAT_EVD_FIELD_EVD_STATE           = 0x04,
    DAT_EVD_FIELD_CNO                 = 0x08,
    DAT_EVD_FIELD_EVD_FLAGS           = 0x10,
    DAT_EVD_FIELD_ALL                 = 0x1F
} DAT_EVD_PARAM_MASK;

typedef DAT_UINT64 DAT_PROVIDER_ATTR_MASK;

enum  dat_lmr_param_mask
{
	DAT_LMR_FIELD_IA_HANDLE          = 0x001,
	DAT_LMR_FIELD_MEM_TYPE           = 0x002,
	DAT_LMR_FIELD_REGION_DESC        = 0x004,
	DAT_LMR_FIELD_LENGTH             = 0x008,
	DAT_LMR_FIELD_PZ_HANDLE          = 0x010,
	DAT_LMR_FIELD_MEM_PRIV           = 0x020,
	DAT_LMR_FIELD_VA_TYPE            = 0x040,
	DAT_LMR_FIELD_LMR_CONTEXT        = 0x080,
	DAT_LMR_FIELD_RMR_CONTEXT        = 0x100,
	DAT_LMR_FIELD_REGISTERED_SIZE    = 0x200,
	DAT_LMR_FIELD_REGISTERED_ADDRESS = 0x400,

	DAT_LMR_FIELD_ALL                = 0x7FF
};

#include <dat2/dat.h>

typedef DAT_HANDLE      DAT_CNO_HANDLE;

struct dat_evd_param
{
    DAT_IA_HANDLE               ia_handle;
    DAT_COUNT                   evd_qlen;
    DAT_EVD_STATE               evd_state;
    DAT_CNO_HANDLE              cno_handle;
    DAT_EVD_FLAGS               evd_flags;
};

#define DAT_LMR_COOKIE_SIZE 40 /* size of DAT_LMR_COOKIE in bytes */
typedef char (* DAT_LMR_COOKIE)[DAT_LMR_COOKIE_SIZE];

/* Format for OS wait proxy agent function */

typedef void (DAT_API *DAT_AGENT_FUNC)
(
    DAT_PVOID,                 /* instance data */
    DAT_EVD_HANDLE             /* Event Dispatcher*/
);

/* Definition */

typedef struct dat_os_wait_proxy_agent
{
    DAT_PVOID                   instance_data;
    DAT_AGENT_FUNC              proxy_agent_func;
} DAT_OS_WAIT_PROXY_AGENT;

/* Define NULL Proxy agent */

#define DAT_OS_WAIT_PROXY_AGENT_NULL \
    (DAT_OS_WAIT_PROXY_AGENT) {      \
    (DAT_PVOID) NULL,                \
    (DAT_AGENT_FUNC) NULL}

/* Flags */

/* The value specified by the uDAPL Consumer for dat_ia_open to indicate
 * that no async EVD should be created for the opening instance of an IA.
 * The same IA has been open before that has the only async EVD to
 * handle async errors for all open instances of the IA.
 */

#define DAT_EVD_ASYNC_EXISTS (DAT_EVD_HANDLE) 0x1

/*
 * The value returned by the dat_ia_query for the case when there is no
 * async EVD for the IA instance. The Consumer specified the value of
 * DAT_EVD_ASYNC_EXISTS for the async_evd_handle for dat_ia_open.
 */

#define DAT_EVD_OUT_OF_SCOPE (DAT_EVD_HANDLE) 0x2

/*
 * Memory types
 *
 * Specifying memory type for LMR create. A Consumer must use a single
 * value when registering memory. The union of any of these
 * flags is used in the Provider parameters to indicate what memory
 * type Provider supports for LMR memory creation.
 */

/* For udapl only */

typedef struct dat_shared_memory
{
    DAT_PVOID                   virtual_address;
    DAT_LMR_COOKIE              shared_memory_id;
} DAT_SHARED_MEMORY;

typedef union dat_region_description
{
    DAT_PVOID                   for_va;
    DAT_LMR_HANDLE              for_lmr_handle;
    DAT_SHARED_MEMORY           for_shared_memory;	/* For udapl only */
} DAT_REGION_DESCRIPTION;

/* LMR Arguments */

struct dat_lmr_param
{
    DAT_IA_HANDLE               ia_handle;
    DAT_MEM_TYPE                mem_type;
    DAT_REGION_DESCRIPTION      region_desc;
    DAT_VLEN                    length;
    DAT_PZ_HANDLE               pz_handle;
    DAT_MEM_PRIV_FLAGS          mem_priv;
    DAT_VA_TYPE			va_type;
    DAT_LMR_CONTEXT             lmr_context;
    DAT_RMR_CONTEXT             rmr_context;
    DAT_VLEN                    registered_size;
    DAT_VADDR                   registered_address;
};

typedef enum dat_proxy_type
{
	DAT_PROXY_TYPE_NONE	= 0x0,
	DAT_PROXY_TYPE_AGENT	= 0x1,
	DAT_PROXY_TYPE_FD	= 0x2
} DAT_PROXY_TYPE;

typedef struct dat_cno_param
{
    DAT_IA_HANDLE               ia_handle;
    DAT_PROXY_TYPE		proxy_type;
    union {
    	DAT_OS_WAIT_PROXY_AGENT		agent;
    	DAT_FD				fd;
	DAT_PVOID			none;
    } proxy;
} DAT_CNO_PARAM;

typedef enum dat_cno_param_mask
{
    DAT_CNO_FIELD_IA_HANDLE		= 0x1,
    DAT_CNO_FIELD_PROXY_TYPE		= 0x2,
    DAT_CNO_FIELD_PROXY			= 0x3,
    DAT_CNO_FIELD_ALL                	= 0x4
} DAT_CNO_PARAM_MASK;

struct dat_ia_attr
{
	char                        adapter_name[DAT_NAME_MAX_LENGTH];
	char                        vendor_name[DAT_NAME_MAX_LENGTH];
	DAT_UINT32                  hardware_version_major;
	DAT_UINT32                  hardware_version_minor;
	DAT_UINT32                  firmware_version_major;
	DAT_UINT32                  firmware_version_minor;
	DAT_IA_ADDRESS_PTR          ia_address_ptr;
	DAT_COUNT                   max_eps;
	DAT_COUNT                   max_dto_per_ep;
	DAT_COUNT                   max_rdma_read_per_ep_in;
	DAT_COUNT                   max_rdma_read_per_ep_out;
	DAT_COUNT                   max_evds;
	DAT_COUNT                   max_evd_qlen;
	DAT_COUNT                   max_iov_segments_per_dto;
	DAT_COUNT                   max_lmrs;
	DAT_SEG_LENGTH              max_lmr_block_size;
	DAT_VADDR                   max_lmr_virtual_address;
	DAT_COUNT                   max_pzs;
	DAT_SEG_LENGTH              max_message_size;
	DAT_SEG_LENGTH              max_rdma_size;
	DAT_COUNT                   max_rmrs;
	DAT_VADDR                   max_rmr_target_address;
	DAT_COUNT                   max_srqs;
	DAT_COUNT                   max_ep_per_srq;
	DAT_COUNT                   max_recv_per_srq;
	DAT_COUNT                   max_iov_segments_per_rdma_read;
	DAT_COUNT                   max_iov_segments_per_rdma_write;
	DAT_COUNT                   max_rdma_read_in;
	DAT_COUNT                   max_rdma_read_out;
	DAT_BOOLEAN                 max_rdma_read_per_ep_in_guaranteed;
	DAT_BOOLEAN                 max_rdma_read_per_ep_out_guaranteed;
	DAT_BOOLEAN                 zb_supported;
	DAT_EXTENSION               extension_supported;
	DAT_COUNT                   extension_version;
	DAT_COUNT                   num_transport_attr;
	DAT_NAMED_ATTR              *transport_attr;
	DAT_COUNT                   num_vendor_attr;
	DAT_NAMED_ATTR              *vendor_attr;
};


#define DAT_IA_FIELD_IA_EXTENSION                       UINT64_C(0x100000000)
#define DAT_IA_FIELD_IA_EXTENSION_VERSION               UINT64_C(0x200000000)

#define DAT_IA_FIELD_IA_NUM_TRANSPORT_ATTR               UINT64_C(0x400000000)
#define DAT_IA_FIELD_IA_TRANSPORT_ATTR                   UINT64_C(0x800000000)
#define DAT_IA_FIELD_IA_NUM_VENDOR_ATTR                  UINT64_C(0x1000000000)
#define DAT_IA_FIELD_IA_VENDOR_ATTR                      UINT64_C(0x2000000000)
#define DAT_IA_FIELD_ALL                                 UINT64_C(0x3FFFFFFFFF)

/* General Provider attributes. udat specific. */

typedef enum dat_pz_support
{
    DAT_PZ_UNIQUE,
    DAT_PZ_SHAREABLE
} DAT_PZ_SUPPORT;

#include <dat2/udat_vendor_specific.h>

/* Provider should support merging of all event stream types. Provider
 * attribute specify support for merging different event stream types.
 * It is a 2D binary matrix where each row and column represents an event
 * stream type. Each binary entry is 1 if the event streams of its raw
 * and column can fed the same EVD, and 0 otherwise. The order of event
 * streams in row and column is the same as in the definition of
 * DAT_EVD_FLAGS: index 0 - Software Event, 1- Connection Request,
 * 2 - DTO Completion, 3 - Connection event, 4 - RMR Bind Completion,
 * 5 - Asynchronous event. By definition each diagonal entry is 1.
 * Consumer allocates an array for it and passes it IN as a pointer
 * for the array that Provider fills. Provider must fill the array
 * that Consumer passes.
 */

struct dat_provider_attr
{
    char                        provider_name[DAT_NAME_MAX_LENGTH];
    DAT_UINT32                  provider_version_major;
    DAT_UINT32                  provider_version_minor;
    DAT_UINT32                  dapl_version_major;
    DAT_UINT32                  dapl_version_minor;
    DAT_MEM_TYPE                lmr_mem_types_supported;
    DAT_IOV_OWNERSHIP           iov_ownership_on_return;
    DAT_QOS                     dat_qos_supported;
    DAT_COMPLETION_FLAGS        completion_flags_supported;
    DAT_BOOLEAN                 is_thread_safe;
    DAT_COUNT                   max_private_data_size;
    DAT_BOOLEAN                 supports_multipath;
    DAT_EP_CREATOR_FOR_PSP      ep_creator;
    DAT_PZ_SUPPORT              pz_support;
    DAT_UINT32                  optimal_buffer_alignment;
    const DAT_BOOLEAN           evd_stream_merging_supported[6][6];
    DAT_BOOLEAN                 srq_supported;
    DAT_COUNT                   srq_watermarks_supported;
    DAT_BOOLEAN                 srq_ep_pz_difference_supported;
    DAT_COUNT                   srq_info_supported;
    DAT_COUNT                   ep_recv_info_supported;
    DAT_BOOLEAN                 lmr_sync_req;
    DAT_BOOLEAN                 dto_async_return_guaranteed;
    DAT_BOOLEAN                 rdma_write_for_rdma_read_req;
    DAT_BOOLEAN			rdma_read_lmr_rmr_context_exposure;
    DAT_RMR_SCOPE		rmr_scope_supported;
    DAT_BOOLEAN                 is_signal_safe;
    DAT_BOOLEAN                 ha_supported;
    DAT_HA_LB			ha_loadbalancing;
    DAT_COUNT                   num_provider_specific_attr;
    DAT_NAMED_ATTR *            provider_specific_attr;
};

#define DAT_PROVIDER_FIELD_PROVIDER_NAME                 UINT64_C(0x00000001)
#define DAT_PROVIDER_FIELD_PROVIDER_VERSION_MAJOR        UINT64_C(0x00000002)
#define DAT_PROVIDER_FIELD_PROVIDER_VERSION_MINOR        UINT64_C(0x00000004)
#define DAT_PROVIDER_FIELD_DAPL_VERSION_MAJOR            UINT64_C(0x00000008)
#define DAT_PROVIDER_FIELD_DAPL_VERSION_MINOR            UINT64_C(0x00000010)
#define DAT_PROVIDER_FIELD_LMR_MEM_TYPE_SUPPORTED        UINT64_C(0x00000020)
#define DAT_PROVIDER_FIELD_IOV_OWNERSHIP                 UINT64_C(0x00000040)
#define DAT_PROVIDER_FIELD_DAT_QOS_SUPPORTED             UINT64_C(0x00000080)
#define DAT_PROVIDER_FIELD_COMPLETION_FLAGS_SUPPORTED    UINT64_C(0x00000100)
#define DAT_PROVIDER_FIELD_IS_THREAD_SAFE                UINT64_C(0x00000200)
#define DAT_PROVIDER_FIELD_MAX_PRIVATE_DATA_SIZE         UINT64_C(0x00000400)
#define DAT_PROVIDER_FIELD_SUPPORTS_MULTIPATH            UINT64_C(0x00000800)
#define DAT_PROVIDER_FIELD_EP_CREATOR                    UINT64_C(0x00001000)
#define DAT_PROVIDER_FIELD_PZ_SUPPORT                    UINT64_C(0x00002000)
#define DAT_PROVIDER_FIELD_OPTIMAL_BUFFER_ALIGNMENT      UINT64_C(0x00004000)
#define DAT_PROVIDER_FIELD_EVD_STREAM_MERGING_SUPPORTED  UINT64_C(0x00008000)
#define DAT_PROVIDER_FIELD_SRQ_SUPPORTED                 UINT64_C(0x00010000)
#define DAT_PROVIDER_FIELD_SRQ_WATERMARKS_SUPPORTED      UINT64_C(0x00020000)
#define DAT_PROVIDER_FIELD_SRQ_EP_PZ_DIFFERENCE_SUPPORTED UINT64_C(0x00040000)
#define DAT_PROVIDER_FIELD_SRQ_INFO_SUPPORTED            UINT64_C(0x00080000)
#define DAT_PROVIDER_FIELD_EP_RECV_INFO_SUPPORTED        UINT64_C(0x00100000)
#define DAT_PROVIDER_FIELD_LMR_SYNC_REQ                  UINT64_C(0x00200000)
#define DAT_PROVIDER_FIELD_DTO_ASYNC_RETURN_GUARANTEED   UINT64_C(0x00400000)
#define DAT_PROVIDER_FIELD_RDMA_WRITE_FOR_RDMA_READ_REQ  UINT64_C(0x00800000)
#define DAT_PROVIDER_FIELD_RDMA_READ_LMR_RMR_CONTEXT_EXPOSURE	UINT64_C(0x01000000) 
#define DAT_PROVIDER_FIELD_RMR_SCOPE_SUPPORTED		UINT64_C(0x02000000)
#define DAT_PROVIDER_FIELD_IS_SIGNAL_SAFE		UINT64_C(0x04000000)
#define DAT_PROVIDER_FIELD_HA_SUPPORTED			UINT64_C(0x08000000)
#define DAT_PROVIDER_FIELD_HA_LB			UINT64_C(0x10000000)
#define DAT_PROVIDER_FIELD_NUM_PROVIDER_SPECIFIC_ATTR    UINT64_C(0x20000000)
#define DAT_PROVIDER_FIELD_PROVIDER_SPECIFIC_ATTR        UINT64_C(0x40000000)

#define DAT_PROVIDER_FIELD_ALL                           UINT64_C(0x7FFFFFFF)
#define DAT_PROVIDER_FIELD_NONE				 UINT64_C(0x0)

/**************************************************************/

/*
 * User DAT function call definitions,
 */

extern DAT_RETURN DAT_API dat_lmr_create (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_MEM_TYPE,		/* mem_type             */
	IN      DAT_REGION_DESCRIPTION, /* region_description   */
	IN      DAT_VLEN,		/* length               */
	IN      DAT_PZ_HANDLE,		/* pz_handle            */
	IN      DAT_MEM_PRIV_FLAGS,	/* privileges           */
	IN	DAT_VA_TYPE,		/* va_type		*/
	OUT     DAT_LMR_HANDLE *,	/* lmr_handle           */
	OUT     DAT_LMR_CONTEXT *,	/* lmr_context          */
	OUT     DAT_RMR_CONTEXT *,	/* rmr_context          */
	OUT     DAT_VLEN *,		/* registered_length    */
	OUT     DAT_VADDR * );		/* registered_address   */

extern DAT_RETURN DAT_API dat_lmr_query (
	IN      DAT_LMR_HANDLE,		/* lmr_handle           */
	IN      DAT_LMR_PARAM_MASK,	/* lmr_param_mask       */
	OUT     DAT_LMR_PARAM * );	/* lmr_param            */

/* Event Functions */

extern DAT_RETURN DAT_API dat_evd_create (
	IN      DAT_IA_HANDLE,		/* ia_handle            */
	IN      DAT_COUNT,		/* evd_min_qlen         */
	IN      DAT_CNO_HANDLE,		/* cno_handle           */
	IN      DAT_EVD_FLAGS,		/* evd_flags            */
	OUT     DAT_EVD_HANDLE * );	/* evd_handle           */

extern DAT_RETURN DAT_API dat_evd_modify_cno (
	IN      DAT_EVD_HANDLE,		/* evd_handle           */
	IN      DAT_CNO_HANDLE);	/* cno_handle           */

extern DAT_RETURN DAT_API dat_cno_create (
	IN 	DAT_IA_HANDLE,		/* ia_handle            */
	IN 	DAT_OS_WAIT_PROXY_AGENT,/* agent                */
	OUT 	DAT_CNO_HANDLE *);	/* cno_handle           */

extern DAT_RETURN DAT_API dat_cno_modify_agent (
	IN 	DAT_CNO_HANDLE,		 /* cno_handle           */
	IN 	DAT_OS_WAIT_PROXY_AGENT);/* agent                */

extern DAT_RETURN DAT_API dat_cno_query (
	IN      DAT_CNO_HANDLE,		/* cno_handle            */
	IN      DAT_CNO_PARAM_MASK,	/* cno_param_mask        */
	OUT     DAT_CNO_PARAM * );	/* cno_param             */

extern DAT_RETURN DAT_API dat_cno_free (
	IN DAT_CNO_HANDLE);		/* cno_handle            */

extern DAT_RETURN DAT_API dat_cno_wait (
	IN  	DAT_CNO_HANDLE,		/* cno_handle            */
	IN  	DAT_TIMEOUT,		/* timeout               */
	OUT 	DAT_EVD_HANDLE *);	/* evd_handle            */

extern DAT_RETURN DAT_API dat_evd_enable (
	IN      DAT_EVD_HANDLE);	/* evd_handle            */

extern DAT_RETURN DAT_API dat_evd_wait (
	IN  	DAT_EVD_HANDLE,		/* evd_handle            */
	IN  	DAT_TIMEOUT,		/* timeout               */
	IN  	DAT_COUNT,		/* threshold             */
	OUT 	DAT_EVENT *,		/* event                 */
	OUT 	DAT_COUNT * );		/* n_more_events         */

extern DAT_RETURN DAT_API dat_evd_disable (
	IN      DAT_EVD_HANDLE);	/* evd_handle            */

extern DAT_RETURN DAT_API dat_evd_set_unwaitable (
	IN DAT_EVD_HANDLE);		/* evd_handle            */

extern DAT_RETURN DAT_API dat_evd_clear_unwaitable (
	IN DAT_EVD_HANDLE);		/* evd_handle            */

extern DAT_RETURN DAT_API dat_cno_fd_create (
	IN	DAT_IA_HANDLE,		/* ia_handle		*/
	OUT	DAT_FD *,		/* file descriptor	*/
	OUT	DAT_CNO_HANDLE * );	/* cno_handle		*/

extern DAT_RETURN DAT_API dat_cno_trigger (
	IN	DAT_CNO_HANDLE,		/* cno_handle		*/
	OUT	DAT_EVD_HANDLE * );	/* evd_handle		*/

#ifdef __cplusplus
}
#endif

#endif /* _UDAT_H_ */
