/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 * 
 * MODULE: dapl_vendor.h
 *
 * PURPOSE: 
 *	Vendor provides values for their implementation. Most of
 *	these values are returned in the DAT_IA_ATTR parameter of
 *	dat_ia_query()
 *
 * $Id:$
 **********************************************************************/

/**********************************************************************
 * DAT_IA_ATTR attributes
 *
 * These values are used in the provider support routine
 * dapls_ib_query_hca (). Many of the values there are HW
 * specific, the the vendor should look to make sure they are
 * appropriate for their implementation. Specifically, 
 * vendors are encouraged to update transport and vendor
 * attributes: the reference implementation sets these to NULL.
 */

/*
 * Product name of the adapter.
 * Returned in DAT_IA_ATTR.adapter_name
 */
#define VN_ADAPTER_NAME		"Generic OpenFabrics HCA"


/*
 * Vendor name
 * Returned in DAT_IA_ATTR.vendor_name
 */
#define VN_VENDOR_NAME		"DAPL OpenFabrics Implementation"


/**********************************************************************
 * PROVIDER Attributes
 *
 * These values are used in ./common/dapl_ia_query.c, in dapl_ia_query ().
 * The values below are the most common for vendors to change, but
 * there are several other values that may be updated once the
 * implementation becomes mature.
 *
 */

/*
 * Provider Versions
 * Returned in DAT_PROVIDER_ATTR.provider_version_major and
 * DAT_PROVIDER_ATTR.provider_version_minor
 */

#define VN_PROVIDER_MAJOR	2
#define VN_PROVIDER_MINOR	0

/*
 * Provider support for memory types. The reference implementation
 * always supports DAT_MEM_TYPE_VIRTUAL and DAT_MEM_TYPE_LMR, so
 * the vendor must indicate if they support DAT_MEM_TYPE_SHARED_VIRTUAL.
 * Set this value to '1' if DAT_MEM_TYPE_SHARED_VIRTUAL is supported.
 *
 * Returned in DAT_PROVIDER_ATTR.lmr_mem_types_supported
 */

#define VN_MEM_SHARED_VIRTUAL_SUPPORT 1


/**********************************************************************
 *
 * This value will be assigned to dev_name_prefix in ./udapl/dapl_init.c.
 *
 * DAT is designed to support multiple DAPL instances simultaneously,
 * with different dapl libraries originating from different providers.
 * There is always the possibility of name conflicts, so a dat name
 * prefix is provided to make a vendor's adapter name unique. This is
 * especially true of the IBM Access API, which returns adapter
 * names that are simply ordinal numbers (e.g. 0, 1, 2). If
 * a vendor doesn't need or want a prefix, it should be left
 * as a NULL (use "").
 *
 * Values that might be used:
 *  #define VN_PREFIX		"ia"    (generic prefix)
 *  #define VN_PREFIX		"jni"	(JNI: OS Acces API)
 *  #define VN_PREFIX		"ibm"	(IBM: OS Acces API)
 *  #define VN_PREFIX		""      (Mellanox: VAPI)
 *  #define VN_PREFIX		""      (Intel: IB Common API)
 */
#define VN_PREFIX		"ia"
