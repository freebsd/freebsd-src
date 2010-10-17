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
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
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

/***********************************************************
 *
 * HEADER: dat_error.h
 *
 * PURPOSE: DAT return codes
 *
 * Description: Header file for "DAPL: Direct Access Programming
 *		Library, Version: 2.0"
 *
 * Mapping rules:
 *	Error types are compound types, as mapped out below.
 *
 *********************************************************/

#ifndef _DAT_ERROR_H_
#define _DAT_ERROR_H_

/*
 *
 * All return codes are actually a 3-way tuple:
 *
 * type: DAT_RETURN_CLASS DAT_RETURN_TYPE DAT_RETURN_SUBTYPE
 * bits: 31-30            29-16           15-0
 *
 *    3                   2                   1
 *  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | C | DAT_RETURN_TYPE           | DAT_RETURN_SUBTYPE            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

/*
 * Class Bits
 */
#define DAT_CLASS_ERROR           0x80000000
#define DAT_CLASS_WARNING         0x40000000
#define DAT_CLASS_SUCCESS         0x00000000
/*
 * DAT Error bits
 */
#define DAT_TYPE_MASK             0x3fff0000 /* mask for DAT_TYPE_STATUS bits */
#define DAT_SUBTYPE_MASK          0x0000FFFF /* mask for DAT_SUBTYPE_STATUS bits */

/*
 * Determining the success of an operation is best done with a macro;
 * each of these returns a boolean value.
 */
#define DAT_IS_WARNING(status)  ((DAT_UINT32)(status) & DAT_CLASS_WARNING)

#define DAT_GET_TYPE(status)    ((DAT_UINT32)(status) & DAT_TYPE_MASK)
#define DAT_GET_SUBTYPE(status) ((DAT_UINT32)(status) & DAT_SUBTYPE_MASK)

/*
 * DAT return types. The ERROR bit is enabled for these definitions
 */
typedef enum dat_return_type
{
        /* The operation was successful.                              */
    DAT_SUCCESS		                        = 0x00000000,

        /* The operation was aborted because IA was closed or EVD was *
         * destroyed.                                                 */
    DAT_ABORT                                   = 0x00010000,

        /* The specified Connection Qualifier was in use.             */
    DAT_CONN_QUAL_IN_USE                        = 0x00020000,

        /* The operation failed due to resource limitations.          */
    DAT_INSUFFICIENT_RESOURCES                  = 0x00030000,

        /* Provider internal error. This error can be returned by any *
         * operation when the Provider has detected an internal error.*
         * This error does not mask any error caused by the Consumer. */
    DAT_INTERNAL_ERROR                          = 0x00040000,

        /* One of the DAT handles was invalid.                        */
    DAT_INVALID_HANDLE                          = 0x00050000,

        /* One of the parameters was invalid.                         */
    DAT_INVALID_PARAMETER                       = 0x00060000,

        /* One of the parameters was invalid for this operation. There*
         * are Event Streams associated with the Event Dispatcher     *
	 * feeding it.                                                */
    DAT_INVALID_STATE                           = 0x00070000,

        /* The size of the receiving buffer is too small for sending  *
         * buffer data.  The size of the local buffer is too small for*
         * the data of the remote buffer.                             */
    DAT_LENGTH_ERROR                            = 0x00080000,

        /* The requested Model was not supported by the Provider.     */
    DAT_MODEL_NOT_SUPPORTED                     = 0x00090000,

        /* The specified IA name was not found in the list of         *
	 * registered Providers.                                      */
    DAT_PROVIDER_NOT_FOUND                      = 0x000A0000,

        /* Protection violation for local or remote memory access     *
         * Protection Zone mismatch between an LMR of one of the      *
         * local_iov segments and the local Endpoint.                 */
    DAT_PRIVILEGES_VIOLATION                    = 0x000B0000,

        /* Privileges violation for local or remote memory access. One*
         * of the LMRs used in local_iov was either invalid or did not*
         * have local read privileges.                                */
    DAT_PROTECTION_VIOLATION                    = 0x000C0000,

        /* The operation timed out without a notification.            */
    DAT_QUEUE_EMPTY                             = 0x000D0000,

        /* The Event Dispatcher queue is full.                        */
    DAT_QUEUE_FULL                              = 0x000E0000,

        /* The operation timed out. uDAPL ONLY                        */
    DAT_TIMEOUT_EXPIRED                         = 0x000F0000,

        /* The provider name was already registered                   */
    DAT_PROVIDER_ALREADY_REGISTERED             = 0x00100000,

        /* The provider is "in-use" and cannot be closed at this time */
    DAT_PROVIDER_IN_USE                         = 0x00110000,

        /* The requested remote address is not valid or not reachable */
    DAT_INVALID_ADDRESS	                        = 0x00120000,

        /* [Unix only] dat_evd_wait or dat_cno_wait has been          *
	 * interrupted.                                               */
    DAT_INTERRUPTED_CALL                        = 0x00130000,

        /* No Connection Qualifiers are available                     */
    DAT_CONN_QUAL_UNAVAILABLE                   = 0x00140000,

    	/* kDAPL reserved LMR 						*/
    DAT_RESERVED_LMR				= 0x00150000, /* kdapl only */

    	/* The specified IP Port was in use. */
    DAT_PORT_IN_USE                             = 0x00160000,
	/* The specified COMM not supported. */
    DAT_COMM_NOT_SUPPORTED                      = 0x00170000,

#ifdef DAT_EXTENSIONS
	/* The DAT extensions support. */
    DAT_EXTENSION_BASE                         = 0x10000000,
	    /* range 0x10000000 - 0x3FFF0000 is reserved for extensions */
#endif /* DAT_EXTENSIONS */

        /* Provider does not support the operation yet.               */
    DAT_NOT_IMPLEMENTED                         = 0x3FFF0000
} DAT_RETURN_TYPE;

typedef DAT_UINT32 DAT_RETURN;

/* Backward compatibility with DAT 1.0 */
#define DAT_NAME_NOT_FOUND DAT_PROVIDER_NOT_FOUND

/*
 * DAT_RETURN_SUBTYPE listing
 */

typedef enum dat_return_subtype
{
	/* First element is no subtype */
    DAT_NO_SUBTYPE,
	/* ABORT sub types                                            */
	/* call was interrupted by a signal, or otherwise             */
    DAT_SUB_INTERRUPTED,

	/* DAT_CONN_QUAL_IN_USE has no subtypes                       */

	/* INSUFFICIENT_RESOURCES subtypes                            */
    DAT_RESOURCE_MEMORY,
    DAT_RESOURCE_DEVICE,
    DAT_RESOURCE_TEP, /* transport endpoint, e.g. QP */
    DAT_RESOURCE_TEVD, /* transport EVD, e.g. CQ */
    DAT_RESOURCE_PROTECTION_DOMAIN,
    DAT_RESOURCE_MEMORY_REGION, /* HCA memory for LMR or RMR */
    DAT_RESOURCE_ERROR_HANDLER,
    DAT_RESOURCE_CREDITS, /* e.g outstanding RDMA Read credit as target */
    DAT_RESOURCE_SRQ,

        /* DAT_INTERNAL_ERROR has no subtypes */

        /* INVALID_HANDLE subtypes */

    DAT_INVALID_HANDLE_IA,
    DAT_INVALID_HANDLE_EP,
    DAT_INVALID_HANDLE_LMR,
    DAT_INVALID_HANDLE_RMR,
    DAT_INVALID_HANDLE_PZ,
    DAT_INVALID_HANDLE_PSP,
    DAT_INVALID_HANDLE_RSP,
    DAT_INVALID_HANDLE_CR,
    DAT_INVALID_HANDLE_CNO,
    DAT_INVALID_HANDLE_EVD_CR,
    DAT_INVALID_HANDLE_EVD_REQUEST,
    DAT_INVALID_HANDLE_EVD_RECV,
    DAT_INVALID_HANDLE_EVD_CONN,
    DAT_INVALID_HANDLE_EVD_ASYNC,
    DAT_INVALID_HANDLE_SRQ,
    DAT_INVALID_HANDLE_CSP,
    DAT_INVALID_HANDLE1,
    DAT_INVALID_HANDLE2,
    DAT_INVALID_HANDLE3,
    DAT_INVALID_HANDLE4,
    DAT_INVALID_HANDLE5,
    DAT_INVALID_HANDLE6,
    DAT_INVALID_HANDLE7,
    DAT_INVALID_HANDLE8,
    DAT_INVALID_HANDLE9,
    DAT_INVALID_HANDLE10,

        /* DAT_INVALID_PARAMETER subtypes */
    DAT_INVALID_ARG1,
    DAT_INVALID_ARG2,
    DAT_INVALID_ARG3,
    DAT_INVALID_ARG4,
    DAT_INVALID_ARG5,
    DAT_INVALID_ARG6,
    DAT_INVALID_ARG7,
    DAT_INVALID_ARG8,
    DAT_INVALID_ARG9,
    DAT_INVALID_ARG10,

	/* DAT_INVALID_EP_STATE subtypes */
    DAT_INVALID_STATE_EP_UNCONNECTED,
    DAT_INVALID_STATE_EP_ACTCONNPENDING,
    DAT_INVALID_STATE_EP_PASSCONNPENDING,
    DAT_INVALID_STATE_EP_TENTCONNPENDING,
    DAT_INVALID_STATE_EP_CONNECTED,
    DAT_INVALID_STATE_EP_DISCONNECTED,
    DAT_INVALID_STATE_EP_RESERVED,
    DAT_INVALID_STATE_EP_COMPLPENDING,
    DAT_INVALID_STATE_EP_DISCPENDING,
    DAT_INVALID_STATE_EP_PROVIDERCONTROL,
    DAT_INVALID_STATE_EP_NOTREADY,
    DAT_INVALID_STATE_EP_RECV_WATERMARK,
    DAT_INVALID_STATE_EP_PZ,
    DAT_INVALID_STATE_EP_EVD_REQUEST,
    DAT_INVALID_STATE_EP_EVD_RECV,
    DAT_INVALID_STATE_EP_EVD_CONNECT,
    DAT_INVALID_STATE_EP_UNCONFIGURED,
    DAT_INVALID_STATE_EP_UNCONFRESERVED,
    DAT_INVALID_STATE_EP_UNCONFPASSIVE,
    DAT_INVALID_STATE_EP_UNCONFTENTATIVE,
    DAT_INVALID_STATE_CNO_IN_USE,
    DAT_INVALID_STATE_CNO_DEAD,

	/* EVD states. Enabled/Disabled, Waitable/Unwaitable,         *
	 * and Notify/Solicited/Threshold are 3 orthogonal            *
	 * bands of EVD state.The Threshold one is uDAPL specific.    */
    DAT_INVALID_STATE_EVD_OPEN,
	/* EVD can be either in enabled or disabled but not both      *
	 * or neither at the same time                                */
    DAT_INVALID_STATE_EVD_ENABLED,
    DAT_INVALID_STATE_EVD_DISABLED,
	/* EVD can be either in waitable or unwaitable but not        *
	 * both or neither at the same time                           */
    DAT_INVALID_STATE_EVD_WAITABLE,
    DAT_INVALID_STATE_EVD_UNWAITABLE,
	/* Do not release an EVD if it is in use                      */
    DAT_INVALID_STATE_EVD_IN_USE,

	/* EVD can be either in notify or solicited or threshold      *
	 * but not any pair, or all, or none at the same time.        *
	 * The threshold one is for uDAPL only                        */
    DAT_INVALID_STATE_EVD_CONFIG_NOTIFY,
    DAT_INVALID_STATE_EVD_CONFIG_SOLICITED,
    DAT_INVALID_STATE_EVD_CONFIG_THRESHOLD,
    DAT_INVALID_STATE_EVD_WAITER,
    DAT_INVALID_STATE_EVD_ASYNC, /* Async EVD required */
    DAT_INVALID_STATE_IA_IN_USE,
    DAT_INVALID_STATE_LMR_IN_USE,
    DAT_INVALID_STATE_LMR_FREE,
    DAT_INVALID_STATE_PZ_IN_USE,
    DAT_INVALID_STATE_PZ_FREE,

        /* DAT_INVALID_STATE_SRQ subtypes */
    DAT_INVALID_STATE_SRQ_OPERATIONAL,
    DAT_INVALID_STATE_SRQ_ERROR,
    DAT_INVALID_STATE_SRQ_IN_USE,

        /* DAT_LENGTH_ERROR has no subtypes */

        /* DAT_MODEL_NOT_SUPPORTED has no subtypes */

        /* DAT_PRIVILEGES_VIOLATION subtypes */
    DAT_PRIVILEGES_READ,
    DAT_PRIVILEGES_WRITE,
    DAT_PRIVILEGES_RDMA_READ,
    DAT_PRIVILEGES_RDMA_WRITE,

        /* DAT_PROTECTION_VIOLATION subtypes */
    DAT_PROTECTION_READ,
    DAT_PROTECTION_WRITE,
    DAT_PROTECTION_RDMA_READ,
    DAT_PROTECTION_RDMA_WRITE,

        /* DAT_QUEUE_EMPTY has no subtypes */

        /* DAT_QUEUE_FULL has no subtypes */

        /* DAT_TIMEOUT_EXPIRED has no subtypes */

        /* DAT_PROVIDER_ALREADY_REGISTERED has no subtypes */

        /* DAT_PROVIDER_IN_USE has no subtypes */

        /* DAT_INVALID_ADDRESS subtypes */

        /* Unsupported addresses - those that are not Malformed, but   *
	 * are incorrect for use in DAT (regardless of local routing   *
         * capabilities): IPv6 Multicast Addresses (ff/8) IPv4         *
         * Broadcast/Multicast Addresses                               */

    DAT_INVALID_ADDRESS_UNSUPPORTED,
        /* Unreachable addresses - A Provider might know that certain  *
	 * addresses are unreachable immediately. One example would be *
	 * an IPv6 addresses on an IPv4-only system.  This can also be *
	 * returned if it is known that there is no route to the host. *
	 * A Provider is not obligated to check for this condition.    */
    DAT_INVALID_ADDRESS_UNREACHABLE,
	/* Malformed addresses -- these cannot be valid in any context.*
	 * Those listed in RFC1884 section 2.3 as "Reserved" or        *
	 * "Unassigned".                                               */
    DAT_INVALID_ADDRESS_MALFORMED,

        /* DAT_INTERRUPTED_CALL has no subtypes */

        /* DAT_CONN_QUAL_UNAVAILABLE has no subtypes */

        /* DAT_PROVIDER_NOT_FOUND subtypes. Erratta to the 1.1 spec */

    DAT_NAME_NOT_REGISTERED,
    DAT_MAJOR_NOT_FOUND,
    DAT_MINOR_NOT_FOUND,
    DAT_THREAD_SAFETY_NOT_FOUND
} DAT_RETURN_SUBTYPE;

#endif /* _DAT_ERROR_H_ */
