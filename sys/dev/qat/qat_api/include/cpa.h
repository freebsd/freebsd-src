/***************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2023 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *
 ***************************************************************************/

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa.h
 *
 * @defgroup cpa CPA API
 *
 * @description
 *      This is the top level API definition for Intel(R) QuickAssist Technology.
 *		It contains structures, data types and definitions that are common
 *		across the interface.
 *
 *****************************************************************************/

/**
 *****************************************************************************
 * @defgroup cpa_BaseDataTypes Base Data Types
 * @file cpa.h
 *
 * @ingroup cpa
 *
 * @description
 *      The base data types for the Intel CPA API.
 *
 *****************************************************************************/

#ifndef CPA_H
#define CPA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_types.h"

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance handle type.
 *
 * @description
 *      Handle used to uniquely identify an instance.
 *
 * @note
 *      Where only a single instantiation exists this field may be set to
 *      @ref CPA_INSTANCE_HANDLE_SINGLE.
 *
 *****************************************************************************/
typedef void * CpaInstanceHandle;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Default instantiation handle value where there is only a single instance
 *
 * @description
 *      Used as an instance handle value where only one instance exists.
 *
 *****************************************************************************/
#define CPA_INSTANCE_HANDLE_SINGLE ((CpaInstanceHandle)0)

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Physical memory address.
 * @description
 *      Type for physical memory addresses.
 *****************************************************************************/
typedef Cpa64U CpaPhysicalAddr;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Virtual to physical address conversion routine.
 *
 * @description
 *      This function is used to convert virtual addresses to physical
 *      addresses.
 *
 * @context
 *      The function shall not be called in an interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] pVirtualAddr           Virtual address to be converted.
 *
 * @return
 * 		Returns the corresponding physical address.
 *      On error, the value NULL is returned.
 *
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
typedef CpaPhysicalAddr (*CpaVirtualToPhysical)(void * pVirtualAddr);


/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Flat buffer structure containing a pointer and length member.
 *
 * @description
 *      A flat buffer structure. The data pointer, pData, is a virtual address.
 *      An API instance may require the actual data to be in contiguous
 *      physical memory as determined by @ref CpaInstanceInfo2.
 *
 *****************************************************************************/
typedef struct _CpaFlatBuffer {
    Cpa32U dataLenInBytes;
    /**< Data length specified in bytes.
     * When used as an input parameter to a function, the length specifies
     * the current length of the buffer.
     * When used as an output parameter to a function, the length passed in
     * specifies the maximum length of the buffer on return (i.e. the allocated
     * length).  The implementation will not write past this length.  On return,
     * the length is always unchanged. */
  Cpa8U *pData;
    /**< The data pointer is a virtual address, however the actual data pointed
     * to is required to be in contiguous physical memory unless the field
     requiresPhysicallyContiguousMemory in CpaInstanceInfo2 is false. */
} CpaFlatBuffer;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Scatter/Gather buffer list containing an array of flat buffers.
 *
 * @description
 *      A scatter/gather buffer list structure.  This buffer structure is
 *      typically used to represent a region of memory which is not
 *      physically contiguous, by describing it as a collection of
 *      buffers, each of which is physically contiguous.
 *
 * @note
 *      The memory for the pPrivateMetaData member must be allocated
 *      by the client as physically contiguous memory.  When allocating
 *      memory for pPrivateMetaData, a call to the corresponding
 *      BufferListGetMetaSize function (e.g. cpaCyBufferListGetMetaSize)
 *      MUST be made to determine the size of the Meta Data Buffer.  The
 *      returned size (in bytes) may then be passed in a memory allocation
 *      routine to allocate the pPrivateMetaData memory.
 *****************************************************************************/
typedef struct _CpaBufferList {
    Cpa32U numBuffers;
    /**< Number of buffers in the list */
    CpaFlatBuffer *pBuffers;
    /**< Pointer to an unbounded array containing the number of CpaFlatBuffers
     * defined by numBuffers
     */
    void *pUserData;
    /**< This is an opaque field that is not read or modified internally. */
    void *pPrivateMetaData;
    /**< Private representation of this buffer list.  The memory for this
     * buffer needs to be allocated by the client as contiguous data.
     * The amount of memory required is returned with a call to
     * the corresponding BufferListGetMetaSize function. If that function
     * returns a size of zero then no memory needs to be allocated, and this
     * parameter can be NULL.
     */
} CpaBufferList;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Flat buffer structure with physical address.
 *
 * @description
 *      Functions taking this structure do not need to do any virtual to
 *      physical address translation before writing the buffer to hardware.
 *****************************************************************************/
typedef struct _CpaPhysFlatBuffer {
    Cpa32U dataLenInBytes;
    /**< Data length specified in bytes.
     * When used as an input parameter to a function, the length specifies
     * the current length of the buffer.
     * When used as an output parameter to a function, the length passed in
     * specifies the maximum length of the buffer on return (i.e. the allocated
     * length).  The implementation will not write past this length.  On return,
     * the length is always unchanged.
     */
    Cpa32U reserved;
    /**< Reserved for alignment */
    CpaPhysicalAddr bufferPhysAddr;
    /**< The physical address at which the data resides.  The data pointed
     * to is required to be in contiguous physical memory.
     */
} CpaPhysFlatBuffer;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Scatter/gather list containing an array of flat buffers with
 *      physical addresses.
 *
 * @description
 *      Similar to @ref CpaBufferList, this buffer structure is typically
 *      used to represent a region of memory which is not physically
 *      contiguous, by describing it as a collection of buffers, each of
 *      which is physically contiguous.  The difference is that, in this
 *      case, the individual "flat" buffers are represented using
 *      physical, rather than virtual, addresses.
 *****************************************************************************/
typedef struct _CpaPhysBufferList {
    Cpa64U reserved0;
    /**< Reserved for internal usage */
    Cpa32U numBuffers;
    /**< Number of buffers in the list */
    Cpa32U reserved1;
    /**< Reserved for alignment */
    CpaPhysFlatBuffer flatBuffers[];
    /**< Array of flat buffer structures, of size numBuffers */
} CpaPhysBufferList;


/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Special value which can be taken by length fields on some of the
 *      "data plane" APIs to indicate that the buffer in question is of
 *      type CpaPhysBufferList, rather than simply an array of bytes.
 ****************************************************************************/
#define CPA_DP_BUFLIST ((Cpa32U)0xFFFFFFFF)


/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      API status value type definition
 *
 * @description
 *      This type definition is used for the return values used in all the
 *      API functions.  Common values are defined, for example see
 *      @ref CPA_STATUS_SUCCESS, @ref CPA_STATUS_FAIL, etc.
 *****************************************************************************/
typedef Cpa32S CpaStatus;

#define CPA_STATUS_SUCCESS (0)
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Success status value. */
#define CPA_STATUS_FAIL (-1)
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Fail status value. */
#define CPA_STATUS_RETRY (-2)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  Retry status value. */
#define CPA_STATUS_RESOURCE (-3)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  The resource that has been requested is unavailable. Refer
 *  to relevant sections of the API for specifics on what the suggested
 *  course of action is. */
#define CPA_STATUS_INVALID_PARAM (-4)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  Invalid parameter has been passed in. */
#define CPA_STATUS_FATAL (-5)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  A serious error has occurred. Recommended course of action
 *  is to shutdown and restart the component. */
#define CPA_STATUS_UNSUPPORTED (-6)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  The function is not supported, at least not with the specific
 *  parameters supplied.  This may be because a particular
 *  capability is not supported by the current implementation. */
#define CPA_STATUS_RESTARTING (-7)
/**<
 *  @ingroup cpa_BaseDataTypes
 *  The API implementation is restarting. This may be reported if, for example,
 *  a hardware implementation is undergoing a reset. Recommended course of
 *  action is to retry the request. */

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      API status string type definition
 * @description
 *      This type definition is used for the generic status text strings
 *      provided by cpaXxGetStatusText API functions.  Common values are
 *      defined, for example see @ref CPA_STATUS_STR_SUCCESS,
 *      @ref CPA_STATUS_FAIL, etc., as well as the maximum size
 *      @ref CPA_STATUS_MAX_STR_LENGTH_IN_BYTES.
 *****************************************************************************/
#define CPA_STATUS_MAX_STR_LENGTH_IN_BYTES (255)
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Maximum length of the Overall Status String (including generic and specific
 *   strings returned by calls to cpaXxGetStatusText) */

#define CPA_STATUS_STR_SUCCESS       ("Operation was successful:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_SUCCESS. */
#define CPA_STATUS_STR_FAIL          ("General or unspecified error occurred:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_FAIL. */
#define CPA_STATUS_STR_RETRY         ("Recoverable error occurred:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_RETRY. */
#define CPA_STATUS_STR_RESOURCE      ("Required resource unavailable:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_RESOURCE. */
#define CPA_STATUS_STR_INVALID_PARAM ("Invalid parameter supplied:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_INVALID_PARAM. */
#define CPA_STATUS_STR_FATAL         ("Fatal error has occurred:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_FATAL. */
#define CPA_STATUS_STR_UNSUPPORTED   ("Operation not supported:")
/**<
 *  @ingroup cpa_BaseDataTypes
 *   Status string for @ref CPA_STATUS_UNSUPPORTED. */

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance Types
 *
 * @deprecated
 * 		As of v1.3 of the Crypto API, this enum has been deprecated,
 * 		replaced by @ref CpaAccelerationServiceType.
 *
 * @description
 *      Enumeration of the different instance types.
 *
 *****************************************************************************/
typedef enum _CpaInstanceType
{
    CPA_INSTANCE_TYPE_CRYPTO = 0,
    /**< Cryptographic instance type */
    CPA_INSTANCE_TYPE_DATA_COMPRESSION,
    /**< Data compression instance type */
    CPA_INSTANCE_TYPE_RAID,
    /**< RAID instance type */
    CPA_INSTANCE_TYPE_XML,
    /**< XML instance type */
    CPA_INSTANCE_TYPE_REGEX
    /**< Regular Expression instance type */
} CpaInstanceType CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Service Type
 * @description
 *      Enumeration of the different service types.
 *
 *****************************************************************************/
typedef enum _CpaAccelerationServiceType
{
    CPA_ACC_SVC_TYPE_CRYPTO = CPA_INSTANCE_TYPE_CRYPTO,
    /**< Cryptography */
    CPA_ACC_SVC_TYPE_DATA_COMPRESSION = CPA_INSTANCE_TYPE_DATA_COMPRESSION,
    /**< Data Compression */
    CPA_ACC_SVC_TYPE_PATTERN_MATCH = CPA_INSTANCE_TYPE_REGEX,
    /**< Pattern Match */
    CPA_ACC_SVC_TYPE_RAID = CPA_INSTANCE_TYPE_RAID,
    /**< RAID */
    CPA_ACC_SVC_TYPE_XML = CPA_INSTANCE_TYPE_XML,
    /**< XML */
    CPA_ACC_SVC_TYPE_VIDEO_ANALYTICS,
    /**< Video Analytics */
    CPA_ACC_SVC_TYPE_CRYPTO_ASYM,
    /**< Cryptography - Asymmetric service */
    CPA_ACC_SVC_TYPE_CRYPTO_SYM
    /**< Cryptography - Symmetric service */
} CpaAccelerationServiceType;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance State
 *
 * @deprecated
 *      As of v1.3 of the Crypto API, this enum has been deprecated,
 *      replaced by @ref CpaOperationalState.
 *
 * @description
 *      Enumeration of the different instance states that are possible.
 *
 *****************************************************************************/
typedef enum _CpaInstanceState
{
    CPA_INSTANCE_STATE_INITIALISED = 0,
    /**< Instance is in the initialized state and ready for use. */
    CPA_INSTANCE_STATE_SHUTDOWN
    /**< Instance is in the shutdown state and not available for use. */
} CpaInstanceState CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance operational state
 * @description
 *      Enumeration of the different operational states that are possible.
 *
 *****************************************************************************/
typedef enum _CpaOperationalState
{
    CPA_OPER_STATE_DOWN= 0,
    /**< Instance is not available for use. May not yet be initialized,
     * or stopped. */
    CPA_OPER_STATE_UP
    /**< Instance is available for use. Has been initialized and started. */
} CpaOperationalState;

#define CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES 64
/**<
 *  @ingroup cpa_BaseDataTypes
 *  Maximum instance info name string length in bytes */
#define CPA_INSTANCE_MAX_ID_SIZE_IN_BYTES 128
/**<
 *  @ingroup cpa_BaseDataTypes
 *  Maximum instance info id string length in bytes */
#define CPA_INSTANCE_MAX_VERSION_SIZE_IN_BYTES 64
/**<
 *  @ingroup cpa_BaseDataTypes
 * Maximum instance info version string length in bytes */

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance Info Structure
 *
 * @deprecated
 * 		As of v1.3 of the Crypto API, this structure has been deprecated,
 * 		replaced by CpaInstanceInfo2.
 *
 * @description
 *      Structure that contains the information to describe the instance.
 *
 *****************************************************************************/
typedef struct _CpaInstanceInfo {
    enum _CpaInstanceType type;
    /**< Type definition for this instance. */
    enum _CpaInstanceState state;
    /**< Operational state of the instance. */
    Cpa8U name[CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES];
    /**< Simple text string identifier for the instance. */
    Cpa8U version[CPA_INSTANCE_MAX_VERSION_SIZE_IN_BYTES];
    /**< Version string. There may be multiple versions of the same type of
     * instance accessible through a particular library. */
} CpaInstanceInfo CPA_DEPRECATED;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Physical Instance ID
 * @description
 *      Identifies the physical instance of an accelerator execution
 *      engine.
 *
 *      Accelerators grouped into "packages".  Each accelerator can in
 *      turn contain one or more execution engines.  Implementations of
 *      this API will define the packageId, acceleratorId,
 *      executionEngineId and busAddress as appropriate for the
 *      implementation.  For example, for hardware-based accelerators,
 *      the packageId might identify the chip, which might contain
 *      multiple accelerators, each of which might contain multiple
 *      execution engines. The combination of packageId, acceleratorId
 *      and executionEngineId uniquely identifies the instance.
 *
 *      Hardware based accelerators implementing this API may also provide
 *      information on the location of the accelerator in the busAddress
 *      field. This field will be defined as appropriate for the
 *      implementation. For example, for PCIe attached accelerators,
 *      the busAddress may contain the PCIe bus, device and function
 *      number of the accelerators.
 *
 *****************************************************************************/
typedef struct _CpaPhysicalInstanceId {
    Cpa16U packageId;
    /**< Identifies the package within which the accelerator is
     * contained. */
    Cpa16U acceleratorId;
    /**< Identifies the specific accelerator within the package. */
    Cpa16U executionEngineId;
    /**< Identifies the specific execution engine within the
     * accelerator. */
    Cpa16U busAddress;
    /**< Identifies the bus address associated with the accelerator
     * execution engine. */
    Cpa32U kptAcHandle;
    /**< Identifies the achandle of the accelerator. */
} CpaPhysicalInstanceId;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance Info Structure, version 2
 * @description
 *      Structure that contains the information to describe the instance.
 *
 *****************************************************************************/
typedef struct _CpaInstanceInfo2 {
    CpaAccelerationServiceType accelerationServiceType;
    /**< Type of service provided by this instance. */
#define CPA_INST_VENDOR_NAME_SIZE CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES
    /**< Maximum length of the vendor name. */
    Cpa8U vendorName[CPA_INST_VENDOR_NAME_SIZE];
    /**< String identifying the vendor of the accelerator. */

#define CPA_INST_PART_NAME_SIZE CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES
    /**< Maximum length of the part name. */
    Cpa8U partName[CPA_INST_PART_NAME_SIZE];
    /**< String identifying the part (name and/or number). */

#define CPA_INST_SW_VERSION_SIZE CPA_INSTANCE_MAX_VERSION_SIZE_IN_BYTES
    /**< Maximum length of the software version string. */
    Cpa8U swVersion[CPA_INST_SW_VERSION_SIZE];
    /**< String identifying the version of the software associated with
     * the instance.  For hardware-based implementations of the API,
     * this should be the driver version.  For software-based
     * implementations of the API, this should be the version of the
     * library.
     *
     * Note that this should NOT be used to store the version of the
     * API, nor should it be used to report the hardware revision
     * (which can be captured as part of the @ref partName, if required). */

#define CPA_INST_NAME_SIZE CPA_INSTANCE_MAX_NAME_SIZE_IN_BYTES
    /**< Maximum length of the instance name. */
    Cpa8U instName[CPA_INST_NAME_SIZE];
    /**< String identifying the name of the instance. */

#define CPA_INST_ID_SIZE CPA_INSTANCE_MAX_ID_SIZE_IN_BYTES
    Cpa8U instID[CPA_INST_ID_SIZE];
    /**< String containing a unique identifier for the instance */

    CpaPhysicalInstanceId physInstId;
    /**< Identifies the "physical instance" of the accelerator. */

#define CPA_MAX_CORES 4096
    /**< Maximum number of cores to support in the coreAffinity bitmap. */
    CPA_BITMAP(coreAffinity, CPA_MAX_CORES);
    /**< A bitmap identifying the core or cores to which the instance
     * is affinitized in an SMP operating system.
     *
     * The term core here is used to mean a "logical" core - for example,
     * in a dual-processor, quad-core system with hyperthreading (two
     * threads per core), there would be 16 such cores (2 processors x
     * 4 cores/processor x 2 threads/core).  The numbering of these cores
     * and the corresponding bit positions is OS-specific.  Note that Linux
     * refers to this as "processor affinity" or "CPU affinity", and refers
     * to the bitmap as a "cpumask".
     *
     * The term "affinity" is used to mean that this is the core on which
     * the callback function will be invoked when using the asynchronous
     * mode of the API.  In a hardware-based implementation of the API,
     * this might be the core to which the interrupt is affinitized.
     * In a software-based implementation, this might be the core to which
     * the process running the algorithm is affinitized.  Where there is
     * no affinity, the bitmap can be set to all zeroes.
     *
     * This bitmap should be manipulated using the macros @ref
     * CPA_BITMAP_BIT_SET, @ref CPA_BITMAP_BIT_CLEAR and @ref
     * CPA_BITMAP_BIT_TEST. */

    Cpa32U nodeAffinity;
    /**< Identifies the processor complex, or node, to which the accelerator
     * is physically connected, to help identify locality in NUMA systems.
     *
     * The values taken by this attribute will typically be in the range
     * 0..n-1, where n is the number of nodes (processor complexes) in the
     * system.  For example, in a dual-processor configuration, n=2.  The
     * precise values and their interpretation are OS-specific. */

    CpaOperationalState operState;
    /**< Operational state of the instance. */
    CpaBoolean requiresPhysicallyContiguousMemory;
    /**< Specifies whether the data pointed to by flat buffers
     * (CpaFlatBuffer::pData) supplied to this instance must be in
     * physically contiguous memory. */
    CpaBoolean isPolled;
    /**< Specifies whether the instance must be polled, or is event driven.
     * For hardware accelerators, the alternative to polling would be
     * interrupts. */
    CpaBoolean isOffloaded;
    /**< Identifies whether the instance uses hardware offload, or is a
     * software-only implementation. */
} CpaInstanceInfo2;

/**
 *****************************************************************************
 * @ingroup cpa_BaseDataTypes
 *      Instance Events
 * @description
 *      Enumeration of the different events that will cause the registered
 *  Instance notification callback function to be invoked.
 *
 *****************************************************************************/
typedef enum _CpaInstanceEvent
{
    CPA_INSTANCE_EVENT_RESTARTING = 0,
    /**< Event type that triggers the registered instance notification callback
     * function when and instance is restarting. The reason why an instance is
     * restarting is implementation specific. For example a hardware
     * implementation may send this event if the hardware device is about to
     * be reset.
     */
    CPA_INSTANCE_EVENT_RESTARTED,
    /**< Event type that triggers the registered instance notification callback
     * function when and instance has restarted. The reason why an instance has
     * restarted is implementation specific. For example a hardware
     * implementation may send this event after the hardware device has
     * been reset.
     */
    CPA_INSTANCE_EVENT_FATAL_ERROR
    /**< Event type that triggers the registered instance notification callback
     * function when an error has been detected that requires the device
     * to be reset. 
     * This event will be sent by all instances using the device, both on the 
     * host and guests. 
     */
} CpaInstanceEvent;

/*****************************************************************************/
/* CPA Instance Management Functions                                         */
/*****************************************************************************/
/**
 *****************************************************************************
 * @file cpa.h
 * @ingroup cpa
 *      Get the number of Acceleration Service instances that are supported by
 *      the API implementation.
 *
 * @description
 *     This function will get the number of instances that are supported
 *     for the required Acceleration Service by an implementation of the CPA
 *     API. This number is then used to determine the size of the array that
 *     must be passed to @ref cpaGetInstances().
 *
 * @context
 *      This function MUST NOT be called from an interrupt context as it MAY
 *      sleep.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  accelerationServiceType    Acceleration Service required
 * @param[out] pNumInstances              Pointer to where the number of
 *                                        instances will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated
 *
 * @see
 *      cpaGetInstances
 *
 *****************************************************************************/
CpaStatus
cpaGetNumInstances(
        const CpaAccelerationServiceType accelerationServiceType,
        Cpa16U *pNumInstances);

/**
 *****************************************************************************
 * @file cpa.h
 * @ingroup cpa
 *      Get the handles to the required Acceleration Service instances that are
 *      supported by the API implementation.
 *
 * @description
 *      This function will return handles to the required Acceleration Service
 *      instances that are supported by an implementation of the CPA API. These
 *      instance handles can then be used as input parameters with other
 *      API functions.
 *
 *      This function will populate an array that has been allocated by the
 *      caller. The size of this array will have been determined by the
 *      cpaGetNumInstances() function.
 *
 * @context
 *      This function MUST NOT be called from an interrupt context as it MAY
 *      sleep.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  accelerationServiceType   Acceleration Service requested
 * @param[in]  numInstances              Size of the array. If the value is
 *                                       greater than the number of instances
 *                                       supported, then an error (@ref
 *                                       CPA_STATUS_INVALID_PARAM) is returned.
 * @param[in,out] cpaInstances           Pointer to where the instance
 *                                       handles will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      This function operates in a synchronous manner and no asynchronous
 *      callback will be generated
 *
 * @see
 *      cpaGetNumInstances
 *
 *****************************************************************************/
CpaStatus
cpaGetInstances(
        const CpaAccelerationServiceType accelerationServiceType,
        Cpa16U numInstances,
        CpaInstanceHandle *cpaInstances);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_H */
