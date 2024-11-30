/*
Copyright (c) 2018, MIPI Alliance, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived
  from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Contributors:
 * Norbert Schulz (Intel Corporation) - Initial API and implementation
 */

#ifndef MIPI_SYST_H_INCLUDED
#define MIPI_SYST_H_INCLUDED

/* SyS-T API version information
 */
#define MIPI_SYST_VERSION_MAJOR 1   /**< Major version, incremented if API changes */
#define MIPI_SYST_VERSION_MINOR 0   /**< Minor version, incremented on compatible extensions */
#define MIPI_SYST_VERSION_PATCH 0   /**< Patch for existing major, minor, usually 0 */

/** Define SyS-T API conformance level
 *
 * 10 = minimal (only short events)
 * 20 = low overhead  (exluding varag functions and CRC32)
 * 30 = full implementation
 */
#define MIPI_SYST_CONFORMANCE_LEVEL 30

/** Compute SYS-T version value
 *
 * Used to compare SYS-T Major.Minor.patch versions numerically at runtime.
 *
 * @param ma major version number
 * @param mi minor version number
 * @param p patch version number
 *
 * Example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
 *
 * #if  MIPI_SYST_VERSION_CODE >= MIPI_SYST_MAKE_VERSION_CODE(1,5,0)
 *     // do what only >= 1.5.x supports
 * #endif
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define MIPI_SYST_MAKE_VERSION_CODE(ma, mi, p) (((ma) << 16) | ((mi)<<8) | (p))

/** Numeric SYS-T version code */
#define MIPI_SYST_VERSION_CODE MIPI_SYST_MAKE_VERSION_CODE(\
  MIPI_SYST_VERSION_MAJOR,\
  MIPI_SYST_VERSION_MINOR,\
  MIPI_SYST_VERSION_PATCH)

/* Macros to trick numeric values like __LINE__ into a string
 */
#define _MIPI_SYST_STRINGIFY(x) #x
#define _MIPI_SYST_CPP_TOSTR(x) _MIPI_SYST_STRINGIFY(x)

#define _MIPI_SYST_VERSION_STRING(a, b, c)\
  _MIPI_SYST_CPP_TOSTR(a)"."_MIPI_SYST_CPP_TOSTR(b)"."_MIPI_SYST_CPP_TOSTR(c)

/** Textual version string */
#define MIPI_SYST_VERSION_STRING \
  _MIPI_SYST_VERSION_STRING(\
    MIPI_SYST_VERSION_MAJOR,\
    MIPI_SYST_VERSION_MINOR,\
    MIPI_SYST_VERSION_PATCH)

#ifndef MIPI_SYST_COMPILER_INCLUDED
#include "mipi_syst/compiler.h"
#endif

/* String hash macros for compile time computation of catalog ID's.
 * Notes:
 *    These macros will only be used with optimized builds, otherwise
 *    a lot of runtime code will be generated.
 *
 *    Only the last 64 bytes of the string are considered for hashing
 */
#define _MIPI_SYST_HASH1(s,i,x,l)  (x*65599u+(mipi_syst_u8)s[(i)<(l)?((l)-1-(i)):(l)])
#define _MIPI_SYST_HASH4(s,i,x,l)  _MIPI_SYST_HASH1(s,i,_MIPI_SYST_HASH1(s,i+1,_MIPI_SYST_HASH1(s,i+2,_MIPI_SYST_HASH1(s,i+3,x,l),l),l),l)
#define _MIPI_SYST_HASH16(s,i,x,l) _MIPI_SYST_HASH4(s,i,_MIPI_SYST_HASH4(s,i+4,_MIPI_SYST_HASH4(s,i+8,_MIPI_SYST_HASH4(s,i+12,x,l),l),l),l)
#define _MIPI_SYST_HASH64(s,i,x,l) _MIPI_SYST_HASH16(s,i,_MIPI_SYST_HASH16(s,i+16,_MIPI_SYST_HASH16(s,i+32,_MIPI_SYST_HASH16(s,i+48,x,l),l),l),l)

#define _MIPI_SYST_HASH_x65599(s,l) ((mipi_syst_u32)_MIPI_SYST_HASH64(s,0,0,l))

#define _MIPI_SYST_HASH_AT_CPP_TIME(str, offset) (_MIPI_SYST_HASH_x65599(str, sizeof(str)-1) + (offset))
#define _MIPI_SYST_HASH_AT_RUN_TIME(str, offset) (mipi_syst_hash_x65599(str, sizeof(str)-1) + (offset))

#if defined(_MIPI_SYST_OPTIMIZER_ON)
#define MIPI_SYST_HASH(a, b) _MIPI_SYST_HASH_AT_CPP_TIME((a), (b))
#else
#define MIPI_SYST_HASH(a, b) _MIPI_SYST_HASH_AT_RUN_TIME((a),(b))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/** Major Message Types
 */
enum mipi_syst_msgtype {
  MIPI_SYST_TYPE_BUILD = 0,          /**< client build id message   */
  MIPI_SYST_TYPE_SHORT32 = 1,        /**< value only message        */
  MIPI_SYST_TYPE_STRING = 2,         /**< text message output       */
  MIPI_SYST_TYPE_CATALOG = 3,        /**< catalog message output    */
  MIPI_SYST_TYPE_RAW = 6,            /**< raw binary data           */
  MIPI_SYST_TYPE_SHORT64 = 7,        /**<  value only message       */
  MIPI_SYST_TYPE_CLOCK = 8,          /**< clock sync message        */

  MIPI_SYST_TYPE_MAX
};

/** MIPI_SYST_TYPE_DEBUG_STRING Sub-Types
 */
enum mipi_syst_subtype_string {
  MIPI_SYST_STRING_GENERIC = 1,        /**< string generic debug         */
  MIPI_SYST_STRING_FUNCTIONENTER = 2,  /**< string is function name      */
  MIPI_SYST_STRING_FUNCTIONEXIT = 3,   /**< string is function name      */
  MIPI_SYST_STRING_INVALIDPARAM = 5,   /**< invalid SyS-T APIcall        */
  MIPI_SYST_STRING_ASSERT = 7,         /**< Software Assert: failure     */
  MIPI_SYST_STRING_PRINTF_32 = 11,     /**< printf with 32-bit packing   */
  MIPI_SYST_STRING_PRINTF_64 = 12,     /**< printf with 64-bit packing   */

  MIPI_SYST_STRING_MAX
};

/** MIPI_SYST_TYPE_CATALOG Sub-Types
 */
enum mipi_syst_subtype_catalog {
  MIPI_SYST_CATALOG_ID32_P32 = 1,   /**< 32-bit catalog ID, 32-bit packing */
  MIPI_SYST_CATALOG_ID64_P32 = 2,   /**< 64-bit catalog ID, 32-bit packing */
  MIPI_SYST_CATALOG_ID32_P64 = 5,   /**< 32-bit catalog ID, 64-bit packing */
  MIPI_SYST_CATALOG_ID64_P64 = 6,   /**< 64-bit catalog ID, 64-bit packing */

  MIPI_SYST_CATALOG_MAX
};

/** MIPI_SYST_TYPE_CLOCK Sub-Types
 */
enum mipi_syst_subtype_clock{
  MIPI_SYST_CLOCK_TRANSPORT_SYNC   =  1,  /**< SyS-T clock & frequency sync  */
  MIPI_SYST_CLOCK_MAX
};

enum mipi_syst_subtype_build {
  MIPI_SYST_BUILD_ID_COMPACT32  = 0, /**< compact32  build id       */
  MIPI_SYST_BUILD_ID_COMPACT64  = 1, /**< compact64  build id       */
  MIPI_SYST_BUILD_ID_LONG       = 2, /**< normal build  message     */
  MIPI_SYST_BUILD_MAX
};

struct mipi_syst_header;
struct mipi_syst_handle;
struct mipi_syst_scatter_prog;

/** 128-bit GUID style message origin ID */
struct mipi_syst_guid {
  union {
    mipi_syst_u8  b[16];
    mipi_syst_u64 ll[2];
  } u;
};

/** GUID initializer code
 *
 * This macro simplifies converting a GUID from its string representation
 * into the mipi_syst_guid data structure. The following example shows
 * how the values from a GUID string are used with the macro. Each numeric
 * component from the GUID string gets converted into a hex value parameter
 * when invoking the macro.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
 *
 *  // Guid: f614b99d-99a1-4c04-8c30-90999ab5fe05
 *
 *   struct mipi_syst_guid guid =
 *      MIPI_SYST_GEN_GUID(0xf614b99d, 0x99a1, 0x4c04, 0x8c30, 0x90999ab5fe05);
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define MIPI_SYST_GEN_GUID(l1, w1, w2, w3, l2) \
  {{\
    (mipi_syst_u8)((mipi_syst_u32)(l1) >> 24), \
    (mipi_syst_u8)((mipi_syst_u32)(l1) >> 16), \
    (mipi_syst_u8)((mipi_syst_u32)(l1) >>  8), \
    (mipi_syst_u8)((mipi_syst_u32)(l1) >>  0), \
    (mipi_syst_u8)((mipi_syst_u16)(w1) >>  8), \
    (mipi_syst_u8)((mipi_syst_u16)(w1) >>  0), \
    (mipi_syst_u8)((mipi_syst_u16)(w2) >>  8), \
    (mipi_syst_u8)((mipi_syst_u16)(w2) >>  0), \
    (mipi_syst_u8)((mipi_syst_u16)(w3) >>  8), \
    (mipi_syst_u8)((mipi_syst_u16)(w3) >>  0), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >> 40), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >> 32), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >> 24), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >> 16), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >>  8), \
    (mipi_syst_u8)((mipi_syst_u64)(l2) >>  0)  \
  }}

 /** SyS-T client origin data
  *
  * This structure holds the GUID or header origin and unit data
  * used by SyS-T handles. The structure gets passed into the handle
  * creation functions to initialize the values that identify clients.
  * @see MIPI_SYST_SET_HANDLE_GUID_UNIT
  * @see MIPI_SYST_SET_HANDLE_MODULE_UNIT
  * @see MIPI_SYST_SET_HANDLE_ORIGIN
  */
struct mipi_syst_origin {
  struct mipi_syst_guid  guid;    /**< origin GUID or module value */
  mipi_syst_u16   unit;           /**< unit value                  */
};

/** Origin structure initializer code using GUID
*
* This macro simplifies initializing a mipi_syst_origin structure. The
* first 5 parameters are GUID values as used by the MIPI_SYST_GEN_GUID
* macro. The last parameter is the unit value (11-Bits).
* @see MIPI_SYST_GEN_GUID
*
*
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
*
*  // Guid: {494E5443-B659-45AF-B786-9DB0786248AE}

*
*   struct mipi_syst_origin = origin
*      MIPI_SYST_GEN_ORIGIN_GUID(
*        0x494E5443, 0xB659, 0x45AF, 0xB786, 0x9DB0786248AE,
*        0x1);
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
#define MIPI_SYST_GEN_ORIGIN_GUID(l1, w1, w2, w3, l2 , u) \
  {\
    MIPI_SYST_GEN_GUID(l1, w1, w2, w3, l2) ,\
    u\
  }

/** Origin structure initializer code using header module value
*
* This macro simplifies initializing a mipi_syst_origin structure. The
* first parameter is the header origin value (7-Bits). The second parameter
* is the unit value (4-bits)
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.c}
*
*  // Guid: {494E5443-B659-45AF-B786-9DB0786248AE}

*   #define MODULE_X 0x10
*   struct mipi_syst_origin =
*      MIPI_SYST_GEN_ORIGIN_MODULE(MODULE_X, 0x1);
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
#define MIPI_SYST_GEN_ORIGIN_MODULE(m , u) \
  {\
    MIPI_SYST_GEN_GUID(0,0,0, ((mipi_syst_u16)(m & 0x7F)) << 8, 0 ),\
    u\
  }
/**
 * Global state initialization hook definition
 *
 * This function gets called in the context of the mipi_syst_init() API
 * function after the generic state members of the global state
 * structure syst_hdr have been setup. It's purpose is to initialize the
 * platform dependent portion of the state and other necessary
 * platform specific initialization steps.
 *
 * @param systh Pointer to global state structure
 * @param p user defined value or pointer to data
 * @see  mipi_syst_header
 */
typedef void (MIPI_SYST_CALLCONV *mipi_syst_inithook_t)(struct mipi_syst_header *systh,
    const void *p);

/**
 * Global state destroy hook definition
 *
 * This function gets called in the context of the mipi_syst_destroy() API
 * function before the generic state members of the global state
 * structure syst_hdr have been destroyed. Its purpose is to free resources
 * used by the platform dependent portion of the global state.
 *
 * @param systh Pointer to global state structure
 */
typedef void (MIPI_SYST_CALLCONV *mipi_syst_destroyhook_t)(struct mipi_syst_header *systh);

/**
 * SyS-T handle state initialization hook definition
 *
 * This function gets called in the context of IO handle generation.
 * Its purpose is to initialize the platform dependent portion of
*  the handle and other necessary platform specific initialization steps.
 *
 * @param systh Pointer to new SyS-T handle
 * @see syst_handle_t
 */
typedef void (*mipi_syst_inithandle_hook_t)(struct mipi_syst_handle *systh);

/**
 * SyS-T handle state release hook definition
 *
 * This function gets called when a handle is about to be destroyed..
 * Its purpose is to free any resources allocated during the handle
 * generation.
 *
 * @param systh Pointer to handle that is destroyed
 * @see syst_handle_t
 */
typedef void (*mipi_syst_releasehandle_hook_t)(struct mipi_syst_handle *systh);

/**
 * Low level message write routine definition
 *
 * This function is called at the end of an instrumentation API to output
 * the raw message data.
 *
 * @param systh pointer to a SyS-T handle structure used in the API call,
 * @param scatterprog pointer to a list of scatter write instructions that
 *                    encodes how to convert the descriptor pointer by
 *                    pdesc into raw binary data. This list doesn't include
 *                    the mandatory first 32 tag byte value pointed by pdesc.
 * @param pdesc pointer to a message descriptor, which containing at least
 *              the 32-bit message tag data
 */
typedef void (*mipi_syst_msg_write_t)(
    struct mipi_syst_handle *systh,
    struct mipi_syst_scatter_prog *scatterprog,
    const void *pdesc);

#ifdef __cplusplus
} /* extern C */
#endif
#ifndef MIPI_SYST_PLATFORM_INCLUDED

/**
 * @defgroup PCFG_Config  Platform Feature Configuration Defines
 *
 * Defines to customize the SyS-T feature set to match the platform needs.
 *
 * Each optional library feature can be disabled by not defining the related
 * MIPI_SYST_PCFG_ENABLE define. Removing unused features in this way reduces
 * both memory footprint and runtime overhead of SyS-T.
 */

/**
 * @defgroup PCFG_Global Platform Wide Configuration
 * @ingroup  PCFG_Config
 *
 * These defines enable global features in the SyS-T library.
 * @{
 */


 /**
 * Extend Platform global SyS-T data state
 *
 * This define extends the global SyS-T state data structure
 * mipi_syst_header with platform private content. A platform typically
 * stores data for SyS-T handle creation processing in this structure.
 *
 * Note: This data is not touched by the library code itself, but typically
 * is used by platform  hook functions for handle creation and destruction.
 * **These hook function calls are not lock protected and may happen
 * concurrently!**  The hook functions need to implement locking if they
 *  modify the platform state data.
 *
 * The platform example uses #mipi_syst_platform_state as data state extension.
 */
#define MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
#undef MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA
/**
 * Extend SyS-T handle data state
 *
 * This define extends the SyS-T handle state data structure
 * mipi_syst_handle with platform private content. A platform typically
 * stores data for fast trace hardware access in the handle data, for
 * example a volatile pointer to an MMIO space.
 *
 * The platform example uses #mipi_syst_platform_handle as handle state
 * extension.
 */
#define MIPI_SYST_PCFG_ENABLE_PLATFORM_HANDLE_DATA

/**
 * Enable HEAP usage for handle generation
 *
 * This macro tells the SyS-T library to enable the heap allocation handle
 * creation API #MIPI_SYST_ALLOC_HANDLE.
 * The platform must provide the macros #MIPI_SYST_HEAP_MALLOC and
 * #MIPI_SYST_HEAP_FREE to point SyS-T to the platform malloc and free
 * functions.
 *
 * Note: In OS kernel space environments, you must use unpaged memory
 * allocation functions.
 */
#define MIPI_SYST_PCFG_ENABLE_HEAP_MEMORY
#undef MIPI_SYST_PCFG_ENABLE_HEAP_MEMORY
/* MSVC and GNU compiler 64-bit mode */

/**
 * Enable 64-bit instruction addresses
 *
 * Set this define if running in 64-bit code address space.
 */
#if defined(_WIN64) || defined(__x86_64__) || defined (__LP64__)
#define MIPI_SYST_PCFG_ENABLE_64BIT_ADDR
#endif
/**
 * Enable atomic 64-bit write operations
 *
 * Set this define if your platform supports an atomic 64-bit data write
 * operation. This results in fewer MMIO accesses.The SyS-T library
 * defaults to 2 consecutive 32-Bit writes otherwise.
 */
#if defined(_WIN64) || defined(__x86_64__) || defined (__LP64__)
#define MIPI_SYST_PCFG_ENABLE_64BIT_IO
#endif

/**
 * Enable helper function code inlining
 *
 * Set this define if speed is more important than code size on your platform.
 * It causes several helper function to get inlined, producing faster, but
 * also larger, code.
 */
#define MIPI_SYST_PCFG_ENABLE_INLINE

/** @} */

/**
 * @defgroup PCFG_ApiSet Supported API sets
 * @ingroup  PCFG_Config
 *
 * These defines enable API sets in the SyS-T library. They are set by default
 * depending on the SyS-T API conformance level. The level is specified using
 * the define #MIPI_SYST_CONFORMANCE_LEVEL.
 * @{
 */

#if MIPI_SYST_CONFORMANCE_LEVEL > 10
 /**
 * Use SyS-T scatter write output function
 *
 * The library comes with an output routine that is intended to write data out
 * to an MMIO space. It simplifies a SyS-T platform integration as
 * only low-level access macros must be provided for outputting data. These
 * macros follow MIPI System Trace Protocol (STP) naming convention, also
 * non STP generators can use this interface.
 *
 * These low level output macros are:
 *
 * #MIPI_SYST_OUTPUT_D32MTS, #MIPI_SYST_OUTPUT_D64MTS,
 * #MIPI_SYST_OUTPUT_D32TS, #MIPI_SYST_OUTPUT_D64,
 * #MIPI_SYST_OUTPUT_D32, #MIPI_SYST_OUTPUT_D16, #MIPI_SYST_OUTPUT_D8 and
 * #MIPI_SYST_OUTPUT_FLAG
 *
 * Note: This version of the write function always starts messages
 * using a 32-bit timestamped record also other sized timestamped
 * packets are allowed by the SyS-T specification.
 */
#define MIPI_SYST_PCFG_ENABLE_DEFAULT_SCATTER_WRITE

/**
 * Enable the Catalog API for 32-Bit Catalog IDs.
 */
#define MIPI_SYST_PCFG_ENABLE_CATID32_API

/**
 * Enable the Catalog API for 64-Bit Catalog IDs.
 */
#define MIPI_SYST_PCFG_ENABLE_CATID64_API

/**
 * Enable plain UTF-8 string output APIs.
 */
#define MIPI_SYST_PCFG_ENABLE_STRING_API

/**
 * Enable raw data output APIs
 */
#define MIPI_SYST_PCFG_ENABLE_WRITE_API

/**
 * Enable Build API
 */
#define MIPI_SYST_PCFG_ENABLE_BUILD_API
#endif /* MIPI_SYST_CONFORMANCE_LEVEL > 10 */

#if  MIPI_SYST_CONFORMANCE_LEVEL > 20
 /**
 * Enable printf API support
 *
 * Note:
 * Enabling printf requires compiler var_arg support as defined by the
 * header files stdarg.h stddef.h.
 */

#define MIPI_SYST_PCFG_ENABLE_PRINTF_API
#undef MIPI_SYST_PCFG_ENABLE_PRINTF_API
/**
 * Maximum size of printf payload in bytes.
 * Adjust this value if larger strings shall be supported by the library.
 * The buffer space is located in stack memory when calling one of the printf
 * style APIs.
 */
#define MIPI_SYST_PCFG_PRINTF_ARGBUF_SIZE 1024

#endif /* #if MIPI_SYST_CONFORMANCE_LEVEL > 20 */

/* @} */

/**
 * @defgroup PCFG_Message Optional Message Attributes
 * @ingroup  PCFG_Config
 *
 * These defines enable optional message components. They are set by default
 * depending on the SyS-T API conformance level. The level is specified using
 * the define #MIPI_SYST_CONFORMANCE_LEVEL.
 * @{
 */

#if MIPI_SYST_CONFORMANCE_LEVEL > 10
/**
 * Enable 128-bit origin GUID support.
 */
#define MIPI_SYST_PCFG_ENABLE_ORIGIN_GUID

/**
 * Enable the API variants that send file:line ID pair location records.
 */
#define MIPI_SYST_PCFG_ENABLE_LOCATION_RECORD
#undef MIPI_SYST_PCFG_ENABLE_LOCATION_RECORD
/**
 * Enable the API variants that send the address of the instrumentation location.
 *
 * This API requires #MIPI_SYST_PCFG_ENABLE_LOCATION_RECORD to be set as well.
 * It uses its own define as it additionally requires the function
 * @ref mipi_syst_return_addr() to be implemented for your platform.
 */
#define MIPI_SYST_PCFG_ENABLE_LOCATION_ADDRESS
#undef MIPI_SYST_PCFG_ENABLE_LOCATION_ADDRESS
/**
 * Enable protocol timestamp.
 *
 * This option adds a timestamp into the SyS-T protocol. This
 * option is used if the SyS-T protocol is not embedded into a hardware
 * timestamped trace protocol like MIPI STP or if the HW timestamp cannot
 * be used for other reasons. Setting this option creates the need to define
 * the macros #MIPI_SYST_PLATFORM_CLOCK and #MIPI_SYST_PLATFORM_FREQ to
 *  return a 64-bit clock tick value and its frequency.
 */
#define MIPI_SYST_PCFG_ENABLE_TIMESTAMP

#if defined(_DOXYGEN_)  /*  only for doxygen, remove the #if to enable */
 /**
 * Enable generation of length field
 *
 * Set this define if the message data shall include the optional length
 * field that indicates how many payload bytes follow.
 */
#define MIPI_SYST_PCFG_LENGTH_FIELD
#endif

#endif

#if MIPI_SYST_CONFORMANCE_LEVEL > 20
/**
 * Enable message data CRC32 generation.
 */
#define MIPI_SYST_PCFG_ENABLE_CHECKSUM

#endif /* #if MIPI_SYST_CONFORMANCE_LEVEL */

/** @} */

#include "Platform.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

#if defined(MIPI_SYST_PCFG_ENABLE_INLINE)
#define MIPI_SYST_INLINE static MIPI_SYST_CC_INLINE
#else
#define MIPI_SYST_INLINE MIPI_SYST_EXPORT
#endif

/** SyS-T global state structure.
 * This structure is holding the global SyS-T library state
 */
struct mipi_syst_header {
  mipi_syst_u32 systh_version; /**< SyS-T version ID            */

#if defined(MIPI_SYST_PCFG_ENABLE_PLATFORM_HANDLE_DATA)
  mipi_syst_inithandle_hook_t systh_inith;       /**< handle init hook function*/
  mipi_syst_releasehandle_hook_t systh_releaseh; /**< handle release hook      */
#endif

#if MIPI_SYST_CONFORMANCE_LEVEL > 10
  mipi_syst_msg_write_t systh_writer;            /**< message output routine   */
#endif
#if defined(MIPI_SYST_PCFG_ENABLE_PLATFORM_STATE_DATA)
  struct mipi_syst_platform_state systh_platform;
  /**< platform specific state    */
#endif
};

/**
 * Message data header tag definition
 *
 * Each SyS-T message starts with a 32-bit message tag. The tag defines the
 * message originator and decoding information for the data following
 * the tag.
 */

struct mipi_syst_msg_tag {
#if defined(MIPI_SYST_BIG_ENDIAN)
  mipi_syst_u32 et_res31 : 1;    /**< reserved for future use        */
  mipi_syst_u32 et_res30 : 1;    /**< reserved for future use        */
  mipi_syst_u32 et_subtype : 6;  /**< type dependent sub category    */
  mipi_syst_u32 et_guid : 1;     /**< 128-bit GUID present           */
  mipi_syst_u32 et_modunit : 11; /**< unit for GUID or module:unit   */
  mipi_syst_u32 et_timestamp : 1;/**< indicate 64-bit timestamp      */
  mipi_syst_u32 et_chksum : 1;   /**< indicate 32-bit CRC            */
  mipi_syst_u32 et_length : 1;   /**< indicate length field          */
  mipi_syst_u32 et_location : 1; /**< indicate location information  */
  mipi_syst_u32 et_res7 : 1;     /**< reserved for future use        */
  mipi_syst_u32 et_severity : 3; /**< severity level of message      */
  mipi_syst_u32 et_type : 4;     /**< SyS-T message type ID          */
#else
  mipi_syst_u32 et_type : 4;     /**< SyS-T message type ID          */
  mipi_syst_u32 et_severity : 3; /**< severity level of message      */
  mipi_syst_u32 et_res7 : 1;     /**< reserved for future use        */
  mipi_syst_u32 et_location : 1; /**< indicate location information  */
  mipi_syst_u32 et_length : 1;   /**< indicate length field          */
  mipi_syst_u32 et_chksum : 1;   /**< indicate 32-bit CRC            */
  mipi_syst_u32 et_timestamp : 1;/**< indicate 64-bit timestamp      */
  mipi_syst_u32 et_modunit : 11; /**< unit for GUID or module:unit   */
  mipi_syst_u32 et_guid : 1;     /**< 128-bit GUID present           */
  mipi_syst_u32 et_subtype : 6;  /**< type dependent sub category    */
  mipi_syst_u32 et_res30 : 1;    /**< reserved for future use        */
  mipi_syst_u32 et_res31 : 1;    /**< reserved for future use        */
#endif
};
#define _MIPI_SYST_MK_MODUNIT_ORIGIN(m,u) (((u) & 0xF)|(m<<4))

/**
 * Message severity level enumeration
 */
enum mipi_syst_severity {
  MIPI_SYST_SEVERITY_MAX = 0,    /**< no assigned severity       */
  MIPI_SYST_SEVERITY_FATAL = 1,  /**< critical error level       */
  MIPI_SYST_SEVERITY_ERROR = 2,  /**< error message level        */
  MIPI_SYST_SEVERITY_WARNING = 3,/**< warning message level      */
  MIPI_SYST_SEVERITY_INFO = 4,   /**< information message level  */
  MIPI_SYST_SEVERITY_USER1 = 5,  /**< user defined level 5       */
  MIPI_SYST_SEVERITY_USER2 = 6,  /**< user defined level 6       */
  MIPI_SYST_SEVERITY_DEBUG = 7   /**< debug information level    */
};

/**
 * Location information inside a message (64-bit format)
 * Location is either the source position of the instrumentation call, or
 * the call instruction pointer value.
 */
union mipi_syst_msglocation32 {
  struct {
#if defined(MIPI_SYST_BIG_ENDIAN)
    mipi_syst_u16 etls_lineNo; /**< line number in file       */
    mipi_syst_u16 etls_fileID; /**< ID of instrumented file   */
#else
    mipi_syst_u16 etls_fileID; /**< ID of instrumented file   */
    mipi_syst_u16 etls_lineNo; /**< line number in file       */
#endif
  } etls_source_location;

  mipi_syst_u32 etls_code_location:32; /**< instruction pointer value */
};

/**
 * Location information inside a message (32-bit format)
 * Location is either the source position of the instrumentation call, or
 * the call instruction pointer value.
 */
union mipi_syst_msglocation64 {
  struct {
#if defined(MIPI_SYST_BIG_ENDIAN)
    mipi_syst_u32 etls_lineNo; /**< line number in file       */
    mipi_syst_u32 etls_fileID; /**< ID of instrumented file   */
#else
    mipi_syst_u32 etls_fileID; /**< ID of instrumented file   */
    mipi_syst_u32 etls_lineNo; /**< line number in file       */
#endif
  } etls_source_location;
  mipi_syst_u64 etls_code_location; /**< instruction pointer value */
};

/**
 * Location information record descriptor
 */
struct mipi_syst_msglocation {
  /** Message format
   * 0 = 16-Bit file and 16-Bit line (total: 32-bit)
   * 1 = 32-Bit file and 32-Bit line (total: 64-bit)
   * 2 = 32-bit code address
   * 3 = 64-bit code address
   */
  mipi_syst_u8 el_format;
  union {
    union mipi_syst_msglocation32 loc32; /**< data for 32-bit variant  */
    union mipi_syst_msglocation64 loc64; /**< data for 64-bit variant  */
  } el_u;
};

/** internal handle state flags
 */
struct mipi_syst_handle_flags {
  mipi_syst_u32 shf_alloc:1; /**< set to 1 if heap allocated handle */
};

/** SyS-T connection handle state structure
 *
 * This structure connects the instrumentation API with the underlying SyS-T
 * infrastructure. It plays a similar role to a FILE * in traditional
 * C file IO.
 */
 struct mipi_syst_handle {
  struct mipi_syst_header* systh_header;     /**< global state            */
  struct mipi_syst_handle_flags systh_flags; /**< handle state            */
  struct mipi_syst_msg_tag systh_tag;        /**< tag flags               */

#if defined(MIPI_SYST_PCFG_ENABLE_ORIGIN_GUID)
  struct mipi_syst_guid systh_guid;          /**< module GUID             */
#endif

#if defined(MIPI_SYST_PCFG_ENABLE_LOCATION_RECORD)
  struct mipi_syst_msglocation systh_location;   /**< location record     */
#endif

  mipi_syst_u32 systh_param_count;          /**< number of parameters     */
  mipi_syst_u32 systh_param[6];             /**< catalog msg parameters   */

#if defined(MIPI_SYST_PCFG_ENABLE_PLATFORM_HANDLE_DATA)
  struct mipi_syst_platform_handle systh_platform;
            /**< platform specific state  */
#endif
};


#ifdef __cplusplus
} /* extern C */
#endif
#ifndef MIPI_SYST_API_INCLUDED
#include "mipi_syst/api.h"
#endif

typedef struct mipi_syst_header MIPI_SYST_HEADER;
typedef struct mipi_syst_handle MIPI_SYST_HANDLE;
typedef enum mipi_syst_severity MIPI_SYST_SEVERITY;
typedef struct mipi_syst_guid MIPI_SYST_GUID;
typedef struct mipi_syst_msg_tag MIPI_SYST_MSG_TAG;
typedef struct mipi_syst_handle_flags MIPI_SYST_HANDLE_FLAGS;
#endif
