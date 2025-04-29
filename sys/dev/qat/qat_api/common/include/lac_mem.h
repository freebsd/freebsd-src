/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 ***************************************************************************
 * @file lac_mem.h
 *
 * @defgroup LacMem     Memory
 *
 * @ingroup LacCommon
 *
 * Memory re-sizing functions and memory accessor macros.
 *
 ***************************************************************************/

#ifndef LAC_MEM_H
#define LAC_MEM_H

/***************************************************************************
 * Include header files
 ***************************************************************************/
#include "cpa.h"
#include "qat_utils.h"
#include "lac_common.h"

/**
 *******************************************************************************
 * @ingroup LacMem
 *      These macros are used to Endian swap variables from IA to QAT.
 *
 * @param[out] x    The variable to be swapped.
 *
 * @retval none
 ******************************************************************************/
#if (BYTE_ORDER == LITTLE_ENDIAN)
#define LAC_MEM_WR_64(x) QAT_UTILS_HOST_TO_NW_64(x)
#define LAC_MEM_WR_32(x) QAT_UTILS_HOST_TO_NW_32(x)
#define LAC_MEM_WR_16(x) QAT_UTILS_HOST_TO_NW_16(x)
#define LAC_MEM_RD_64(x) QAT_UTILS_NW_TO_HOST_64(x)
#define LAC_MEM_RD_32(x) QAT_UTILS_NW_TO_HOST_32(x)
#define LAC_MEM_RD_16(x) QAT_UTILS_NW_TO_HOST_16(x)
#else
#define LAC_MEM_WR_64(x) (x)
#define LAC_MEM_WR_32(x) (x)
#define LAC_MEM_WR_16(x) (x)
#define LAC_MEM_RD_64(x) (x)
#define LAC_MEM_RD_32(x) (x)
#define LAC_MEM_RD_16(x) (x)
#endif

/*
*******************************************************************************
* Shared Memory Macros (memory accessible by Acceleration Engines, e.g. QAT)
*******************************************************************************
*/

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to write to a variable that will be read by the
 * QAT. The macro will automatically detect the size of the target variable and
 * will select the correct method for performing the write. The data is cast to
 * the type of the field that it will be written to.
 * This macro swaps data if required.
 *
 * @param[out] var    The variable to be written. Can be a field of a struct.
 *
 * @param[in] data    The value to be written.  Will be cast to the size of the
 *                    target.
 *
 * @retval none
 ******************************************************************************/
#define LAC_MEM_SHARED_WRITE_SWAP(var, data)                                   \
	do {                                                                   \
		switch (sizeof(var)) {                                         \
		case 1:                                                        \
			(var) = (Cpa8U)(data);                                 \
			break;                                                 \
		case 2:                                                        \
			(var) = (Cpa16U)(data);                                \
			(var) = LAC_MEM_WR_16(((Cpa16U)var));                  \
			break;                                                 \
		case 4:                                                        \
			(var) = (Cpa32U)(data);                                \
			(var) = LAC_MEM_WR_32(((Cpa32U)var));                  \
			break;                                                 \
		case 8:                                                        \
			(var) = (Cpa64U)(data);                                \
			(var) = LAC_MEM_WR_64(((Cpa64U)var));                  \
			break;                                                 \
		default:                                                       \
			break;                                                 \
		}                                                              \
	} while (0)

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to read a variable that was written by the QAT.
 * The macro will automatically detect the size of the data to be read and will
 * select the correct method for performing the read. The value read from the
 * variable is cast to the size of the data type it will be stored in.
 * This macro swaps data if required.
 *
 * @param[in] var     The variable to be read. Can be a field of a struct.
 *
 * @param[out] data   The variable to hold the result of the read. Data read
 *                    will be cast to the size of this variable
 *
 * @retval none
 ******************************************************************************/
#define LAC_MEM_SHARED_READ_SWAP(var, data)                                    \
	do {                                                                   \
		switch (sizeof(var)) {                                         \
		case 1:                                                        \
			(data) = (var);                                        \
			break;                                                 \
		case 2:                                                        \
			(data) = LAC_MEM_RD_16(((Cpa16U)var));                 \
			break;                                                 \
		case 4:                                                        \
			(data) = LAC_MEM_RD_32(((Cpa32U)var));                 \
			break;                                                 \
		case 8:                                                        \
			(data) = LAC_MEM_RD_64(((Cpa64U)var));                 \
			break;                                                 \
		default:                                                       \
			break;                                                 \
		}                                                              \
	} while (0)

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to write a pointer to a QAT request. The fields
 *      for pointers in the QAT request and response messages are always 64 bits
 *
 * @param[out] var    The variable to be written to. Can be a field of a struct.
 *
 * @param[in] data    The value to be written.  Will be cast to size of target
 *                    variable
 *
 * @retval none
 ******************************************************************************/
/* cast pointer to scalar of same size of the native pointer */
#define LAC_MEM_SHARED_WRITE_FROM_PTR(var, data)                               \
	((var) = (Cpa64U)(LAC_ARCH_UINT)(data))

/* Note: any changes to this macro implementation should also be made to the
 * similar LAC_MEM_CAST_PTR_TO_UINT64 macro
 */

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to read a pointer from a QAT response. The fields
 *      for pointers in the QAT request and response messages are always 64 bits
 *
 * @param[in] var     The variable to be read. Can be a field of a struct.
 *
 * @param[out] data   The variable to hold the result of the read. Data read
 *                    will be cast to the size of this variable
 *
 * @retval none
 ******************************************************************************/
/* Cast back to native pointer */
#define LAC_MEM_SHARED_READ_TO_PTR(var, data)                                  \
	((data) = (void *)(LAC_ARCH_UINT)(var))

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro safely casts a pointer to a Cpa64U type.
 *
 * @param[in] pPtr   The pointer to be cast.
 *
 * @retval pointer cast to Cpa64U
 ******************************************************************************/
#define LAC_MEM_CAST_PTR_TO_UINT64(pPtr) ((Cpa64U)(pPtr))

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro uses an QAT Utils macro to convert from a virtual address to
 *a
 *      physical address for internally allocated memory.
 *
 * @param[in] pVirtAddr   The address to be converted.
 *
 * @retval The converted physical address
 ******************************************************************************/
#define LAC_OS_VIRT_TO_PHYS_INTERNAL(pVirtAddr)                                \
	(QAT_UTILS_MMU_VIRT_TO_PHYS(pVirtAddr))

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro should be called on all externally allocated memory it calls
 *      SalMem_virt2PhysExternal function which allows a user
 *      to set the virt2phys function used by an instance.
 *      Defaults to virt to phys for kernel.
 *
 * @param[in] genService  Generic sal_service_t structure.
 * @param[in] pVirtAddr   The address to be converted.
 *
 * @retval The converted physical address
 ******************************************************************************/
#define LAC_OS_VIRT_TO_PHYS_EXTERNAL(genService, pVirtAddr)                    \
	((SalMem_virt2PhysExternal(pVirtAddr, &(genService))))

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to write an address variable that will be read by
 * the QAT.  The macro will perform the necessary virt2phys address translation
 * This macro is only to be called on memory allocated internally by the driver.
 *
 * @param[out] var  The address variable to write. Can be a field of a struct.
 *
 * @param[in] pPtr  The pointer variable to containing the address to be
 *                  written
 *
 * @retval none
 ******************************************************************************/
#define LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_INTERNAL(var, pPtr)              \
	do {                                                                   \
		Cpa64U physAddr = 0;                                           \
		physAddr = LAC_MEM_CAST_PTR_TO_UINT64(                         \
		    LAC_OS_VIRT_TO_PHYS_INTERNAL(pPtr));                       \
		var = physAddr;                                                \
	} while (0)

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This macro can be used to write an address variable that will be read by
 * the QAT.  The macro will perform the necessary virt2phys address translation
 * This macro is to be used on memory allocated externally by the user. It calls
 * the user supplied virt2phys address translation.
 *
 * @param[in] pService The pointer to the service
 * @param[out] var     The address variable to write. Can be a field of a struct
 * @param[in] pPtr     The pointer variable to containing the address to be
 *                     written
 *
 * @retval none
 ******************************************************************************/
#define LAC_MEM_SHARED_WRITE_VIRT_TO_PHYS_PTR_EXTERNAL(pService, var, pPtr)    \
	do {                                                                   \
		Cpa64U physAddr = 0;                                           \
		physAddr = LAC_MEM_CAST_PTR_TO_UINT64(                         \
		    LAC_OS_VIRT_TO_PHYS_EXTERNAL(pService, pPtr));             \
		var = physAddr;                                                \
	} while (0)

/*
*******************************************************************************
* OS Memory Macros
*******************************************************************************
*/

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This function and associated macro allocates the memory for the given
 *      size and stores the address of the memory allocated in the pointer.
 *
 * @param[out] ppMemAddr    address of pointer where address will be stored
 * @param[in] sizeBytes     the size of the memory to be allocated.
 *
 * @retval CPA_STATUS_RESOURCE  Macro failed to allocate Memory
 * @retval CPA_STATUS_SUCCESS   Macro executed successfully
 *
 ******************************************************************************/
static __inline CpaStatus
LacMem_OsMemAlloc(void **ppMemAddr, Cpa32U sizeBytes)
{
	*ppMemAddr = malloc(sizeBytes, M_QAT, M_WAITOK);

	return CPA_STATUS_SUCCESS;
}

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This function and associated macro allocates the contiguous
 *       memory for the given
 *      size and stores the address of the memory allocated in the pointer.
 *
 * @param[out] ppMemAddr     address of pointer where address will be stored
 * @param[in] sizeBytes      the size of the memory to be allocated.
 * @param[in] alignmentBytes the alignment
 * @param[in] node           node to allocate from
 *
 * @retval CPA_STATUS_RESOURCE  Macro failed to allocate Memory
 * @retval CPA_STATUS_SUCCESS   Macro executed successfully
 *
 ******************************************************************************/
static __inline CpaStatus
LacMem_OsContigAlignMemAlloc(void **ppMemAddr,
			     Cpa32U sizeBytes,
			     Cpa32U alignmentBytes,
			     Cpa32U node)
{
	if ((alignmentBytes & (alignmentBytes - 1)) !=
	    0) /* if is not power of 2 */
	{
		*ppMemAddr = NULL;
		QAT_UTILS_LOG("alignmentBytes MUST be the power of 2\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	*ppMemAddr =
	    qatUtilsMemAllocContiguousNUMA(sizeBytes, node, alignmentBytes);

	if (NULL == *ppMemAddr) {
		return CPA_STATUS_RESOURCE;
	}

	return CPA_STATUS_SUCCESS;
}

/**
 *******************************************************************************
 * @ingroup LacMem
 *      Macro from the malloc() function
 *
 ******************************************************************************/
#define LAC_OS_MALLOC(sizeBytes) malloc(sizeBytes, M_QAT, M_WAITOK)

/**
 *******************************************************************************
 * @ingroup LacMem
 *      Macro from the LacMem_OsContigAlignMemAlloc function
 *
 ******************************************************************************/
#define LAC_OS_CAMALLOC(ppMemAddr, sizeBytes, alignmentBytes, node)            \
	LacMem_OsContigAlignMemAlloc((void *)ppMemAddr,                        \
				     sizeBytes,                                \
				     alignmentBytes,                           \
				     node)

/**
 *******************************************************************************
 * @ingroup LacMem
 *      Macro for declaration static const unsigned int constant. One provides
 *   the compilation time computation with the highest bit set in the
 *   sizeof(TYPE) value. The constant is being put by the linker by default in
 *   .rodata section
 *
 *   E.g. Statement LAC_DECLARE_HIGHEST_BIT_OF(lac_mem_blk_t)
 *   results in following entry:
 *     static const unsigned int highest_bit_of_lac_mem_blk_t = 3
 *
 *   CAUTION!
 *      Macro is prepared only for type names NOT-containing ANY
 *  special characters. Types as amongst others:
 *  - void *
 *  - unsigned long
 *  - unsigned int
 *  are strictly forbidden and will result in compilation error.
 *  Use typedef to provide one-word type name for MACRO's usage.
 ******************************************************************************/
#define LAC_DECLARE_HIGHEST_BIT_OF(TYPE)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
	static const unsigned int highest_bit_of_##TYPE =                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	    (sizeof(TYPE) & 0x80000000 ? 31 : (sizeof(TYPE) & 0x40000000 ? 30 : (sizeof(TYPE) & 0x20000000 ? 29 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
														      sizeof(TYPE) & 0x10000000 ? 28 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
																			   sizeof(TYPE) & 0x08000000 ? 27 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
																								sizeof(TYPE) & 0x04000000 ? 26 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
																												     sizeof(TYPE) & 0x02000000 ? 25 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
																																	  sizeof(TYPE) & 0x01000000 ? 24 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
																																					       sizeof(TYPE) & 0x00800000 ?                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
																																						   23 :                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
																																						   (sizeof(TYPE) & 0x00400000 ? 22 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
																																											 sizeof(                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
																																											     TYPE) &                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
																																												 0x00200000 ?                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
																																											     21 :                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
																																											     (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
																																												 sizeof(TYPE) & 0x00100000 ? 20 : (sizeof(TYPE) & 0x00080000 ? 19 : (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
																																																					sizeof(                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
																																																					    TYPE) &                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
																																																						0x00040000 ?                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
																																																					    18 :                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
																																																					    (                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
																																																						sizeof(TYPE) & 0x00020000 ? 17 : (                                                                                                                                                                                                                                                                                                                                                                                                                                             \
																																																										     sizeof(TYPE) & 0x00010000 ? 16 : (sizeof(TYPE) &                                                                                                                                                                                                                                                                                                                                                                                          \
																																																															       0x00008000 ?                                                                                                                                                                                                                                                                                                                                                                                    \
																																																															   15 :                                                                                                                                                                                                                                                                                                                                                                                                \
																																																															   (sizeof(TYPE) & 0x00004000 ? 14 : (                                                                                                                                                                                                                                                                                                                                                                 \
																																																																				 sizeof(TYPE) & 0x00002000 ? 13 :                                                                                                                                                                                                                                                                                                                              \
																																																																							     (                                                                                                                                                                                                                                                                                                                                 \
																																																																								 sizeof(TYPE) & 0x00001000 ? 12 : (                                                                                                                                                                                                                                                                                            \
																																																																												      sizeof(TYPE) & 0x00000800 ? 11 : (                                                                                                                                                                                                                                                       \
																																																																																	   sizeof(TYPE) & 0x00000400 ? 10 :                                                                                                                                                                                                                    \
																																																																																				       (                                                                                                                                                                                                                       \
																																																																																					   sizeof(TYPE) &                                                                                                                                                                                                      \
																																																																																						   0x00000200 ?                                                                                                                                                                                                \
																																																																																					       9 :                                                                                                                                                                                                             \
																																																																																					       (sizeof(                                                                                                                                                                                                        \
																																																																																						    TYPE) &                                                                                                                                                                                                    \
																																																																																							0x00000100 ?                                                                                                                                                                                           \
																																																																																						    8 :                                                                                                                                                                                                        \
																																																																																						    (sizeof(TYPE) & 0x00000080 ? 7 :                                                                                                                                                                           \
																																																																																										 (                                                                                                                                                                             \
																																																																																										     sizeof(TYPE) & 0x00000040 ?                                                                                                                                               \
																																																																																											 6 :                                                                                                                                                                   \
																																																																																											 (                                                                                                                                                                     \
																																																																																											     sizeof(TYPE) & 0x00000020 ? 5 :                                                                                                                                   \
																																																																																															 (                                                                                                                                     \
																																																																																															     sizeof(TYPE) & 0x00000010 ? 4 :                                                                                                   \
																																																																																																			 (                                                                                                     \
																																																																																																			     sizeof(TYPE) & 0x00000008 ? 3 :                                                                   \
																																																																																																							 (                                                                     \
																																																																																																							     sizeof(TYPE) & 0x00000004 ? 2 :                                   \
																																																																																																											 (                                     \
																																																																																																											     sizeof(TYPE) & 0x00000002 ? 1 : ( \
																																																																																																																 sizeof(TYPE) & 0x00000001 ? 0 : 32))))))))))))))))) /*16*/))))))))))))))) /* 31 */

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This function and associated macro frees the memory at the given address
 *      and resets the pointer to NULL
 *
 * @param[out] ppMemAddr    address of pointer where mem address is stored.
 *                          If pointer is NULL, the function will exit silently
 *
 * @retval void
 *
 ******************************************************************************/
static __inline void
LacMem_OsMemFree(void **ppMemAddr)
{
	free(*ppMemAddr, M_QAT);
	*ppMemAddr = NULL;
}

/**
 *******************************************************************************
 * @ingroup LacMem
 *      This function and associated macro frees the contiguous memory at the
 *      given address and resets the pointer to NULL
 *
 * @param[out] ppMemAddr    address of pointer where mem address is stored.
 *                          If pointer is NULL, the function will exit silently
 *
 * @retval void
 *
 ******************************************************************************/
static __inline void
LacMem_OsContigAlignMemFree(void **ppMemAddr)
{
	if (NULL != *ppMemAddr) {
		qatUtilsMemFreeNUMA(*ppMemAddr);
		*ppMemAddr = NULL;
	}
}

#define LAC_OS_FREE(pMemAddr) LacMem_OsMemFree((void *)&pMemAddr)

#define LAC_OS_CAFREE(pMemAddr) LacMem_OsContigAlignMemFree((void *)&pMemAddr)

/**
*******************************************************************************
 * @ingroup LacMem
 *     Copies user data to a working buffer of the correct size (required by
 *     PKE services)
 *
 * @description
 *      This function produces a correctly sized working buffer from the input
 *      user buffer. If the original buffer is too small a new buffer shall
 *      be allocated and memory is copied (left padded with zeros to the
*required
 *      length).
 *
 *      The returned working buffer is guaranteed to be of the desired size for
 *      QAT.
 *
 *      When this function is called pInternalMem describes the user_buffer and
 *      when the function returns pInternalMem describes the working buffer.
 *      This is because pInternalMem describes the memory that will be sent to
 *      QAT.
 *
 *      The caller must keep the original buffer pointer. The allocated buffer
*is
 *      freed (as necessary) using icp_LacBufferRestore().
 *
 * @param[in] instanceHandle Handle to crypto instance so pke_resize mem pool
*can
 *                           be located
 * @param[in] pUserBuffer Pointer on the user buffer
 * @param[in] userLen     length of the user buffer
 * @param[in] workingLen  length of the working (correctly sized) buffer
 * @param[in/out] pInternalMem    pointer to boolean if TRUE on input then
 *                                user_buffer is internally allocated memory
 *                                if false then it is externally allocated.
 *                                This value gets updated by the function
 *                                if the returned pointer references internally
 *                                allocated memory.
 *
 * @return a pointer to the working (correctly sized) buffer or NULL if the
 *      allocation failed
 *
 * @note the working length cannot be smaller than the user buffer length
 *
 * @warning the working buffer may be the same or different from the original
 * user buffer; the caller should make no assumptions in this regard
 *
 * @see icp_LacBufferRestore()
 *
 ******************************************************************************/
Cpa8U *icp_LacBufferResize(CpaInstanceHandle instanceHandle,
			   Cpa8U *pUserBuffer,
			   Cpa32U userLen,
			   Cpa32U workingLen,
			   CpaBoolean *pInternalMemory);

/**
*******************************************************************************
 * @ingroup LacMem
 *     Restores a user buffer
 *
 * @description
 *      This function restores a user buffer and releases its
 *      corresponding working buffer. The working buffer, assumed to be
 *      previously obtained using icp_LacBufferResize(), is freed as necessary.
 *
 *      The contents are copied in the process.
 *
 * @note the working length cannot be smaller than the user buffer length
 *
 * @param[out] pUserBuffer     Pointer on the user buffer
 * @param[in] userLen          length of the user buffer
 * @param[in] pWorkingBuffer   Pointer on the working buffer
 * @param[in] workingLen       working buffer length
 * @param[in] copyBuf          if set _TRUE the data in the workingBuffer
 *                             will be copied to the userBuffer before the
 *                             workingBuffer is freed.
 *
 * @return the status of the operation
 *
 * @see icp_LacBufferResize()
 *
 ******************************************************************************/
CpaStatus icp_LacBufferRestore(Cpa8U *pUserBuffer,
			       Cpa32U userLen,
			       Cpa8U *pWorkingBuffer,
			       Cpa32U workingLen,
			       CpaBoolean copyBuf);

/**
*******************************************************************************
 * @ingroup LacMem
 *    Uses an instance specific user supplied virt2phys function to convert a
 *    virtual address to a physical address.
 *
 * @description
 *    Uses an instance specific user supplied virt2phys function to convert a
 *    virtual address to a physical address. A client of QA API can set the
 *    virt2phys function for an instance by using the
 *    cpaXxSetAddressTranslation() function. If the client does not set the
 *    virt2phys function and the instance is in kernel space then OS specific
 *    virt2phys function will be used. In user space the virt2phys function
 *    MUST be set by the user.
 *
 * @param[in] pVirtAddr         the virtual addr to be converted
 * @param[in] pServiceGen       Pointer on the sal_service_t structure
 *                              so client supplied virt2phys function can be
 *                              called.
 *
 * @return the physical address
 *
 ******************************************************************************/
CpaPhysicalAddr SalMem_virt2PhysExternal(void *pVirtAddr, void *pServiceGen);

#endif /* LAC_MEM_H */
