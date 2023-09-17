/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_buffer_desc.h
 *
 * @defgroup icp_BufferDesc Buffer descriptor for LAC
 *
 * @ingroup LacCommon
 *
 * @description
 *      This file contains details of the hardware buffer descriptors used to
 *      communicate with the QAT.
 *
 *****************************************************************************/
#ifndef ICP_BUFFER_DESC_H
#define ICP_BUFFER_DESC_H

#include "cpa.h"

typedef Cpa64U icp_qat_addr_width_t; // hi32 first, lo32 second

// Alignement constraint of the buffer list.
#define ICP_DESCRIPTOR_ALIGNMENT_BYTES 8

/**
 *****************************************************************************
 * @ingroup icp_BufferDesc
 *      Buffer descriptors for FlatBuffers - used in communications with
 *      the QAT.
 *
 * @description
 *      A QAT friendly buffer descriptor.
 *      All buffer descriptor described in this structure are physcial
 *      and are 64 bit wide.
 *
 *      Updates in the CpaFlatBuffer should be also reflected in this
 *      structure
 *
 *****************************************************************************/
typedef struct icp_flat_buffer_desc_s {
	Cpa32U dataLenInBytes;
	Cpa32U reserved;
	icp_qat_addr_width_t phyBuffer;
	/**< The client will allocate memory for this using API function calls
	  *  and the access layer will fill it and the QAT will read it.
	  */
} icp_flat_buffer_desc_t;

/**
 *****************************************************************************
 * @ingroup icp_BufferDesc
 *      Buffer descriptors for BuffersLists - used in communications with
 *      the QAT.
 *
 * @description
 *      A QAT friendly buffer descriptor.
 *      All buffer descriptor described in this structure are physcial
 *      and are 64 bit wide.
 *
 *      Updates in the CpaBufferList should be also reflected in this structure
 *
 *****************************************************************************/
typedef struct icp_buffer_list_desc_s {
	Cpa64U resrvd;
	Cpa32U numBuffers;
	Cpa32U reserved;
	icp_flat_buffer_desc_t phyBuffers[];
	/**< Unbounded array of physical buffer pointers, these point to the
	  *  FlatBufferDescs. The client will allocate memory for this using
	  *  API function calls and the access layer will fill it and the QAT
	  *  will read it.
	  */
} icp_buffer_list_desc_t;

#endif /* ICP_BUFFER_DESC_H */
