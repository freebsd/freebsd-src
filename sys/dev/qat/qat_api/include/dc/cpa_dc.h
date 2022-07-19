/****************************************************************************
 *
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
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
 * @file cpa_dc.h
 *
 * @defgroup cpaDc Data Compression API
 *
 * @ingroup cpa
 *
 * @description
 *      These functions specify the API for Data Compression operations.
 *
 * @remarks
 *
 *
 *****************************************************************************/

#ifndef CPA_DC_H
#define CPA_DC_H

#ifdef __cplusplus
extern"C" {
#endif


#ifndef CPA_H
#include "cpa.h"
#endif

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      CPA Dc Major Version Number
 * @description
 *      The CPA_DC API major version number. This number will be incremented
 *      when significant churn to the API has occurred. The combination of the
 *      major and minor number definitions represent the complete version number
 *      for this interface.
 *
 *****************************************************************************/
#define CPA_DC_API_VERSION_NUM_MAJOR (2)

/**
 *****************************************************************************
 * @ingroup cpaDc
 *       CPA DC Minor Version Number
 * @description
 *      The CPA_DC API minor version number. This number will be incremented
 *      when minor changes to the API has occurred. The combination of the major
 *      and minor number definitions represent the complete version number for
 *      this interface.
 *
 *****************************************************************************/
#define CPA_DC_API_VERSION_NUM_MINOR (2)

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression API session handle type
 *
 * @description
 *      Handle used to uniquely identify a Compression API session handle. This
 *      handle is established upon registration with the API using
 *      cpaDcInitSession().
 *
 *
 *
 *****************************************************************************/
typedef void * CpaDcSessionHandle;


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported file types
 *
 * @description
 *      This enumerated lists identified file types.  Used to select Huffman
 *      trees.
 *      File types are associated with Precompiled Huffman Trees.
 *
 * @deprecated
 *      As of v1.6 of the Compression API, this enum has been deprecated.
 *
 *****************************************************************************/
typedef enum _CpaDcFileType
{
    CPA_DC_FT_ASCII,
    /**< ASCII File Type */
    CPA_DC_FT_CSS,
    /**< Cascading Style Sheet File Type  */
    CPA_DC_FT_HTML,
    /**< HTML or XML (or similar) file type */
    CPA_DC_FT_JAVA,
    /**< File Java code or similar */
    CPA_DC_FT_OTHER
    /**< Other file types */
} CpaDcFileType;
/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported flush flags
 *
 * @description
 *      This enumerated list identifies the types of flush that can be
 *      specified for stateful and stateless cpaDcCompressData and
 *      cpaDcDecompressData functions.
 *
 *****************************************************************************/
typedef enum _CpaDcFlush
{
    CPA_DC_FLUSH_NONE = 0,
    /**< No flush request. */
    CPA_DC_FLUSH_FINAL,
    /**< Indicates that the input buffer contains all of the data for
    the compression session allowing any buffered data to be released.
    For Deflate, BFINAL is set in the compression header.*/
    CPA_DC_FLUSH_SYNC,
    /**< Used for stateful deflate compression to indicate that all pending
    output is flushed, byte aligned, to the output buffer. The session state
    is not reset.*/
    CPA_DC_FLUSH_FULL
    /**< Used for deflate compression to indicate that all pending output is
    flushed to the output buffer and the session state is reset.*/
} CpaDcFlush;
/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported Huffman Tree types
 *
 * @description
 *      This enumeration lists support for Huffman Tree types.
 *      Selecting Static Huffman trees generates compressed blocks with an RFC
 *      1951 header specifying "compressed with fixed Huffman trees".
 *
 *      Selecting Full Dynamic Huffman trees generates compressed blocks with
 *      an RFC 1951 header specifying "compressed with dynamic Huffman codes".
 *      The headers are calculated on the data being compressed, requiring two
 *      passes.
 *
 *      Selecting Precompiled Huffman Trees generates blocks with RFC 1951
 *      dynamic headers.  The headers are pre-calculated and are specified by
 *      the file type.
 *
 *****************************************************************************/
typedef enum _CpaDcHuffType
{
    CPA_DC_HT_STATIC,
    /**< Static Huffman Trees */
    CPA_DC_HT_PRECOMP,
    /**< Precompiled Huffman Trees  */
    CPA_DC_HT_FULL_DYNAMIC
    /**< Full Dynamic Huffman Trees */
} CpaDcHuffType;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported compression types
 *
 * @description
 *      This enumeration lists the supported data compression algorithms.
 *      In combination with CpaDcChecksum it is used to decide on the file
 *      header and footer format.
 *
 * @deprecated
 *      As of v1.6 of the Compression API, CPA_DC_LZS, CPA_DC_ELZS and
 *      CPA_DC_LZSS have been deprecated and should not be used.
 *
 *****************************************************************************/
typedef enum _CpaDcCompType
{
    CPA_DC_LZS,
    /**< LZS Compression */
    CPA_DC_ELZS,
    /**< Extended LZS Compression */
    CPA_DC_LZSS,
    /**< LZSS Compression */
    CPA_DC_DEFLATE
    /**< Deflate Compression */
} CpaDcCompType;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported checksum algorithms
 *
 * @description
 *      This enumeration lists the supported checksum algorithms
 *      Used to decide on file header and footer specifics.
 *
 *****************************************************************************/
typedef enum _CpaDcChecksum
{
    CPA_DC_NONE,
    /**< No checksums required */
    CPA_DC_CRC32,
    /**<  application requires a CRC32 checksum */
    CPA_DC_ADLER32
    /**< Application requires Adler-32 checksum */
} CpaDcChecksum;


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported session directions
 *
 * @description
 *      This enumerated list identifies the direction of a session.
 *      A session can be compress, decompress or both.
 *
 *****************************************************************************/
typedef enum _CpaDcSessionDir
{
    CPA_DC_DIR_COMPRESS,
    /**< Session will be used for compression */
    CPA_DC_DIR_DECOMPRESS,
    /**< Session will be used for decompression */
    CPA_DC_DIR_COMBINED
    /**< Session will be used for both compression and decompression */
} CpaDcSessionDir;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported session state settings
 *
 * @description
 *      This enumerated list identifies the stateful setting of a session.
 *      A session can be either stateful or stateless.
 *
 *      Stateful sessions are limited to have only one in-flight message per
 *      session. This means a compress or decompress request must be complete
 *      before a new request can be started. This applies equally to sessions
 *      that are uni-directional in nature and sessions that are combined
 *      compress and decompress. Completion occurs when the synchronous function
 *      returns, or when the asynchronous callback function has completed.
 *
 *****************************************************************************/
typedef enum _CpaDcSessionState
{
    CPA_DC_STATEFUL,
    /**< Session will be stateful, implying that state may need to be
        saved in some situations */
    CPA_DC_STATELESS
    /**< Session will be stateless, implying no state will be stored*/
} CpaDcSessionState;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported compression levels
 *
 * @description
 *      This enumerated lists the supported compressed levels.
 *      Lower values will result in less compressibility in less time.
 *
 *
 *****************************************************************************/
typedef enum _CpaDcCompLvl
{
    CPA_DC_L1 = 1,
    /**< Compression level 1 */
    CPA_DC_L2,
    /**< Compression level 2 */
    CPA_DC_L3,
    /**< Compression level 3 */
    CPA_DC_L4,
    /**< Compression level 4 */
    CPA_DC_L5,
    /**< Compression level 5 */
    CPA_DC_L6,
    /**< Compression level 6 */
    CPA_DC_L7,
    /**< Compression level 7 */
    CPA_DC_L8,
    /**< Compression level 8 */
    CPA_DC_L9
    /**< Compression level 9 */
} CpaDcCompLvl;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported additional details from accelerator
 *
 * @description
 *      This enumeration lists the supported additional details from the
 *      accelerator.  These may be useful in determining the best way to
 *      recover from a failure.
 *
 *
 *****************************************************************************/
typedef enum _CpaDcReqStatus
{
    CPA_DC_OK = 0,
    /**< No error detected by compression slice */
    CPA_DC_INVALID_BLOCK_TYPE = -1,
    /**< Invalid block type (type == 3) */
    CPA_DC_BAD_STORED_BLOCK_LEN = -2,
    /**< Stored block length did not match one's complement */
    CPA_DC_TOO_MANY_CODES  = -3,
    /**< Too many length or distance codes */
    CPA_DC_INCOMPLETE_CODE_LENS = -4,
    /**< Code length codes incomplete */
    CPA_DC_REPEATED_LENS = -5,
    /**< Repeated lengths with no first length */
    CPA_DC_MORE_REPEAT = -6,
    /**< Repeat more than specified lengths */
    CPA_DC_BAD_LITLEN_CODES = -7,
    /**< Invalid literal/length code lengths */
    CPA_DC_BAD_DIST_CODES = -8,
    /**< Invalid distance code lengths */
    CPA_DC_INVALID_CODE = -9,
    /**< Invalid literal/length or distance code in fixed or dynamic block */
    CPA_DC_INVALID_DIST = -10,
    /**< Distance is too far back in fixed or dynamic block */
    CPA_DC_OVERFLOW = -11,
    /**< Overflow detected.  This is an indication that output buffer has overflowed.
     * For stateful sessions, this is a warning (the input can be adjusted and
     * resubmitted).
     * For stateless sessions this is an error condition */
    CPA_DC_SOFTERR = -12,
    /**< Other non-fatal detected */
    CPA_DC_FATALERR = -13,
    /**< Fatal error detected */
    CPA_DC_MAX_RESUBITERR = -14,
    /**< On an error being detected, the firmware attempted to correct and resubmitted the
     * request, however, the maximum resubmit value was exceeded */
    CPA_DC_INCOMPLETE_FILE_ERR = -15,
    /**< The input file is incomplete.  Note this is an indication that the request was
     * submitted with a CPA_DC_FLUSH_FINAL, however, a BFINAL bit was not found in the
     * request */
    CPA_DC_WDOG_TIMER_ERR = -16,
   /**< The request was not completed as a watchdog timer hardware event occurred */
    CPA_DC_EP_HARDWARE_ERR = -17,
    /**< Request was not completed as an end point hardware error occurred (for
     * example, a parity error) */
    CPA_DC_VERIFY_ERROR = -18,
    /**< Error detected during "compress and verify" operation */
    CPA_DC_EMPTY_DYM_BLK = -19,
    /**< Decompression request contained an empty dynamic stored block
     * (not supported) */
    CPA_DC_CRC_INTEG_ERR = -20,
    /**< A data integrity CRC error was detected */
} CpaDcReqStatus;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported modes for automatically selecting the best compression type.
 *
 * @description
 *      This enumeration lists the supported modes for automatically selecting
 *      the best Huffman encoding which would lead to the best compression
 *      results.
 *
 *      The CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_NO_HDRS value is deprecated
 *      and should not be used.
 *
 *****************************************************************************/
typedef enum _CpaDcAutoSelectBest
{
    CPA_DC_ASB_DISABLED = 0,
    /**< Auto select best mode is disabled */
    CPA_DC_ASB_STATIC_DYNAMIC = 1,
    /**< Auto select between static and dynamic compression */
    CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_STORED_HDRS = 2,
    /**< Auto select between uncompressed, static and dynamic compression,
     * using stored block deflate headers if uncompressed is selected */
    CPA_DC_ASB_UNCOMP_STATIC_DYNAMIC_WITH_NO_HDRS = 3
    /**< Auto select between uncompressed, static and dynamic compression,
     * using no deflate headers if uncompressed is selected */
} CpaDcAutoSelectBest;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Supported modes for skipping regions of input or output buffers.
 *
 * @description
 *      This enumeration lists the supported modes for skipping regions of 
 *      input or output buffers. 
 *
 *****************************************************************************/
typedef enum _CpaDcSkipMode
{
    CPA_DC_SKIP_DISABLED = 0,
    /**< Skip mode is disabled */
    CPA_DC_SKIP_AT_START = 1,
    /**< Skip region is at the start of the buffer. */
    CPA_DC_SKIP_AT_END = 2,
    /**< Skip region is at the end of the buffer. */
    CPA_DC_SKIP_STRIDE = 3
    /**< Skip region occurs at regular intervals within the buffer. 
     CpaDcSkipData.strideLength specifies the number of bytes between each
     skip region. */
} CpaDcSkipMode;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Service specific return codes
 *
 * @description
 *      Compression specific return codes
 *
 *
 *****************************************************************************/

#define CPA_DC_BAD_DATA     (-100)
    /**<Input data in invalid */

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Definition of callback function invoked for asynchronous cpaDc
 *      requests.
 *
 * @description
 *      This is the prototype for the cpaDc compression callback functions.
 *      The callback function is registered by the application using the
 *      cpaDcInitSession() function call.
 *
 * @context
 *      This callback function can be executed in a context that DOES NOT
 *      permit sleeping to occur.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param callbackTag   User-supplied value to help identify request.
 * @param status        Status of the operation. Valid values are
 *                      CPA_STATUS_SUCCESS, CPA_STATUS_FAIL and
 *                      CPA_STATUS_UNSUPPORTED.
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
typedef void (*CpaDcCallbackFn)(
    void *callbackTag,
    CpaStatus status);


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Implementation Capabilities Structure
 * @description
 *      This structure contains data relating to the capabilities of an
 *      implementation. The capabilities include supported compression
 *      algorithms, RFC 1951 options and whether the implementation supports
 *      both stateful and stateless compress and decompress sessions.
 *
 ****************************************************************************/
typedef struct _CpaDcInstanceCapabilities  {
        CpaBoolean  statefulLZSCompression;
            /**<True if the Instance supports Stateful LZS compression */
        CpaBoolean  statefulLZSDecompression;
            /**<True if the Instance supports Stateful LZS decompression */
        CpaBoolean  statelessLZSCompression;
            /**<True if the Instance supports Stateless LZS compression */
        CpaBoolean  statelessLZSDecompression;
            /**<True if the Instance supports Stateless LZS decompression */
        CpaBoolean  statefulLZSSCompression;
            /**<True if the Instance supports Stateful LZSS compression */
        CpaBoolean  statefulLZSSDecompression;
            /**<True if the Instance supports Stateful LZSS decompression */
        CpaBoolean  statelessLZSSCompression;
            /**<True if the Instance supports Stateless LZSS compression */
        CpaBoolean  statelessLZSSDecompression;
            /**<True if the Instance supports Stateless LZSS decompression */
        CpaBoolean  statefulELZSCompression;
            /**<True if the Instance supports Stateful Extended LZS
            compression */
        CpaBoolean  statefulELZSDecompression;
            /**<True if the Instance supports Stateful Extended LZS
            decompression */
        CpaBoolean  statelessELZSCompression;
            /**<True if the Instance supports Stateless Extended LZS
            compression */
        CpaBoolean  statelessELZSDecompression;
            /**<True if the Instance supports Stateless Extended LZS
            decompression */
        CpaBoolean  statefulDeflateCompression;
            /**<True if the Instance supports Stateful Deflate compression */
        CpaBoolean  statefulDeflateDecompression;
            /**<True if the Instance supports Stateful Deflate
            decompression */
        CpaBoolean  statelessDeflateCompression;
            /**<True if the Instance supports Stateless Deflate compression */
        CpaBoolean  statelessDeflateDecompression;
            /**<True if the Instance supports Stateless Deflate
            decompression */
        CpaBoolean  checksumCRC32;
            /**<True if the Instance can calculate a CRC32 checksum over
                the uncompressed data*/
        CpaBoolean  checksumAdler32;
            /**<True if the Instance can calculate an Adler-32 checksum over
                the uncompressed data */
        CpaBoolean  dynamicHuffman;
            /**<True if the Instance supports dynamic Huffman trees in deflate
                blocks*/
        CpaBoolean  dynamicHuffmanBufferReq;
            /**<True if an Instance specific buffer is required to perform
                a dynamic Huffman tree deflate request */
        CpaBoolean  precompiledHuffman;
            /**<True if the Instance supports precompiled Huffman trees in
                deflate blocks*/
        CpaBoolean  autoSelectBestHuffmanTree;
            /**<True if the Instance has the ability to automatically select
                between different Huffman encoding schemes for better
                compression ratios */
        Cpa8U       validWindowSizeMaskCompression;
            /**<Bits set to '1' for each valid window size supported by
                the compression implementation */
        Cpa8U       validWindowSizeMaskDecompression;
            /**<Bits set to '1' for each valid window size supported by
                the decompression implementation */
        Cpa32U      internalHuffmanMem;
            /**<Number of bytes internally available to be used when
                    constructing dynamic Huffman trees. */
        CpaBoolean  endOfLastBlock;
            /**< True if the Instance supports stopping at the end of the last
             * block in a deflate stream during a decompression operation and
             * reporting that the end of the last block has been reached as
             * part of the CpaDcReqStatus data. */
        CpaBoolean  reportParityError;
            /**<True if the instance supports parity error reporting. */
        CpaBoolean  batchAndPack;
            /**< True if the instance supports 'batch and pack' compression */
        CpaBoolean  compressAndVerify;
            /**<True if the instance supports checking that compressed data,
             * generated as part of a compression operation, can be
             * successfully decompressed. */
        CpaBoolean  compressAndVerifyStrict;
            /**< True if compressAndVerify is 'strictly' enabled for the
             * instance. If strictly enabled, compressAndVerify will be enabled
             * by default for compression operations and cannot be disabled by
             * setting opData.compressAndVerify=0 with cpaDcCompressData2().
             * Compression operations with opData.compressAndVerify=0 will
             * return a CPA_STATUS_INVALID_PARAM error status when in
             * compressAndVerify strict mode.
             */
        CpaBoolean  compressAndVerifyAndRecover;
            /**<True if the instance supports recovering from errors detected
             * by compressAndVerify by generating a stored block in the
             * compressed output data buffer. This stored block replaces any
             * compressed content that resulted in a compressAndVerify error.
             */
        CpaBoolean integrityCrcs;
            /**<True if the instance supports integrity CRC checking in the
             * compression/decompression datapath. */
} CpaDcInstanceCapabilities;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Session Setup Data.
 * @description
 *      This structure contains data relating to setting up a session. The
 *      client needs to complete the information in this structure in order to
 *      setup a session.
 *
 * @deprecated
 *      As of v1.6 of the Compression API, the fileType and deflateWindowSize
 *      fields in this structure have been deprecated and should not be used.
 *
 ****************************************************************************/
typedef struct _CpaDcSessionSetupData  {
        CpaDcCompLvl compLevel;
          /**<Compression Level from CpaDcCompLvl */
        CpaDcCompType compType;
          /**<Compression type from CpaDcCompType */
        CpaDcHuffType huffType;
          /**<Huffman type from CpaDcHuffType */
        CpaDcAutoSelectBest autoSelectBestHuffmanTree;
          /**<Indicates if and how the implementation should select the best
           * Huffman encoding. */
        CpaDcFileType fileType;
          /**<File type for the purpose of determining Huffman Codes from
           * CpaDcFileType.
           * As of v1.6 of the Compression API, this field has been deprecated
           * and should not be used.  */
        CpaDcSessionDir sessDirection;
         /**<Session direction indicating whether session is used for
            compression, decompression or both */
        CpaDcSessionState sessState;
        /**<Session state indicating whether the session should be configured
            as stateless or stateful */
        Cpa32U deflateWindowSize;
        /**<Base 2 logarithm of maximum window size minus 8 (a value of 7 for
         * a 32K window size). Permitted values are 0 to 7. cpaDcDecompressData
         * may return an error if an attempt is made to decompress a stream that
         * has a larger window size.
         * As of v1.6 of the Compression API, this field has been deprecated and
         * should not be used. */
        CpaDcChecksum checksum;
        /**<Desired checksum required for the session */
} CpaDcSessionSetupData;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Statistics Data.
 * @description
 *      This structure contains data elements corresponding to statistics.
 *      Statistics are collected on a per instance basis and include:
 *      jobs submitted and completed for both compression and decompression.
 *
 ****************************************************************************/
typedef struct _CpaDcStats  {
        Cpa64U numCompRequests;
          /**< Number of successful compression requests */
        Cpa64U numCompRequestsErrors;
          /**< Number of compression requests that had errors and
             could not be processed */
        Cpa64U numCompCompleted;
          /**< Compression requests completed */
        Cpa64U numCompCompletedErrors;
          /**< Compression requests not completed due to errors */
        Cpa64U numCompCnvErrorsRecovered;
          /**< Compression CNV errors that have been recovered */

        Cpa64U numDecompRequests;
          /**< Number of successful decompression requests */
        Cpa64U numDecompRequestsErrors;
          /**< Number of decompression requests that had errors and
             could not be processed */
        Cpa64U numDecompCompleted;
          /**< Decompression requests completed */
        Cpa64U numDecompCompletedErrors;
          /**< Decompression requests not completed due to errors */

} CpaDcStats;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Request results data
 * @description
 *      This structure contains the request results.
 *
 *      For stateful sessions the status, produced, consumed and
 *      endOfLastBlock results are per request values while the checksum
 *      value is cumulative across all requests on the session so far.
 *      In this case the checksum value is not guaranteed to be correct
 *      until the final compressed data has been processed.
 *
 *      For stateless sessions, an initial checksum value is passed into
 *      the stateless operation. Once the stateless operation completes,
 *      the checksum value will contain checksum produced by the operation.
 *
 ****************************************************************************/
typedef struct _CpaDcRqResults  {
        CpaDcReqStatus status;
          /**< Additional status details from accelerator */
        Cpa32U produced;
          /**< Octets produced by the operation */
        Cpa32U consumed;
          /**< Octets consumed by the operation */
        Cpa32U checksum;
          /**< Initial checksum passed into stateless operations.
             Will also be updated to the checksum produced by the operation  */
       CpaBoolean endOfLastBlock;
          /**< Decompression operation has stopped at the end of the last
           * block in a deflate stream. */
} CpaDcRqResults;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Integrity CRC calculation details
 * @description
 *      This structure contains information about resulting integrity CRC
 *      calculations performed for a single request.
 *
 ****************************************************************************/
typedef struct _CpaIntegrityCrc {
        Cpa32U iCrc;   /**< CRC calculated on request's input  buffer */
        Cpa32U oCrc;   /**< CRC calculated on request's output buffer */
} CpaIntegrityCrc;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Collection of CRC related data
 * @description
 *      This structure contains data facilitating CRC calculations.
 *      After successful request, this structure will contain
 *      all resulting CRCs.
 *      Integrity specific CRCs (when enabled/supported) are located in
 *      'CpaIntegrityCrc integrityCrc' field.
 * @note
 *      this structure must be allocated in physical contiguous memory
 *
 ****************************************************************************/
typedef struct _CpaCrcData {
        Cpa32U crc32;
        /**< CRC32 calculated on the input buffer during compression
         * requests and on the output buffer during decompression requests. */
        Cpa32U adler32;
        /**< ADLER32 calculated on the input buffer during compression
         * requests and on the output buffer during decompression requests. */
        CpaIntegrityCrc integrityCrc;
          /**< Integrity CRCs */
} CpaCrcData;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Skip Region Data.
 * @description
 *      This structure contains data relating to configuring skip region
 *      behaviour. A skip region is a region of an input buffer that
 *      should be omitted from processing or a region that should be inserted
 *      into the output buffer.
 *
 ****************************************************************************/
typedef struct _CpaDcSkipData {
        CpaDcSkipMode skipMode;
        /**<Skip mode from CpaDcSkipMode for buffer processing */
        Cpa32U skipLength;
        /**<Number of bytes to skip when skip mode is enabled */
        Cpa32U strideLength;
        /**<Size of the stride between skip regions when skip mode is
         * set to CPA_DC_SKIP_STRIDE. */
        Cpa32U firstSkipOffset;
        /**< Number of bytes to skip in a buffer before reading/writing the
         * input/output data. */
} CpaDcSkipData;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      (De)Compression request input parameters.
 * @description
 *      This structure contains the request information for use with
 *      compression operations.
 *
 ****************************************************************************/
typedef struct _CpaDcOpData  {
        CpaDcFlush flushFlag;
        /**< Indicates the type of flush to be performed. */
        CpaBoolean compressAndVerify;
        /**< If set to true, for compression operations, the implementation
         * will verify that compressed data, generated by the compression
         * operation, can be successfully decompressed.
         * This behavior is only supported for stateless compression.
         * This behavior is only supported on instances that support the
         * compressAndVerify capability. */
        CpaBoolean compressAndVerifyAndRecover;
        /**< If set to true, for compression operations, the implementation
         * will automatically recover from a compressAndVerify error.
         * This behavior is only supported for stateless compression.
         * This behavior is only supported on instances that support the
         * compressAndVerifyAndRecover capability.
         * The compressAndVerify field in CpaDcOpData MUST be set to CPA_TRUE
         * if compressAndVerifyAndRecover is set to CPA_TRUE. */
        CpaBoolean integrityCrcCheck;
        /**< If set to true, the implementation will verify that data
         * integrity is preserved through the processing pipeline.
         * This behaviour supports stateless and stateful behavior for
         * both static and dynamic Huffman encoding.
         *
         * Integrity CRC checking is not supported for decompression operations
         * over data that contains multiple gzip headers. */
        CpaBoolean verifyHwIntegrityCrcs;
        /**< If set to true, software calculated CRCs will be compared
         * against hardware generated integrity CRCs to ensure that data
         * integrity is maintained when transferring data to and from the
         * hardware accelerator. */
        CpaDcSkipData inputSkipData;
        /**< Optional skip regions in the input buffers */
        CpaDcSkipData outputSkipData;
        /**< Optional skip regions in the output buffers */
        CpaCrcData *pCrcData;
        /**< Pointer to CRCs for this operation, when integrity checks
         * are enabled. */
} CpaDcOpData;

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Retrieve Instance Capabilities
 *
 * @description
 *      This function is used to retrieve the capabilities matrix of
 *      an instance.
 *
 * @context
 *      This function shall not be called in an interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance      Instance handle derived from discovery
 *                                  functions
 * @param[in,out]   pInstanceCapabilities   Pointer to a capabilities struct
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      Only a synchronous version of this function is provided.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcQueryCapabilities(  CpaInstanceHandle dcInstance,
        CpaDcInstanceCapabilities *pInstanceCapabilities );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Initialize compression decompression session
 *
 * @description
 *      This function is used to initialize a compression/decompression
 *      session.
 *      This function specifies a BufferList for context data.
 *      A single session can be used for both compression and decompression
 *      requests.  Clients MAY register a callback
 *      function for the compression service using this function.
 *      This function returns a unique session handle each time this function
 *      is invoked.
 *      If the session has been configured with a callback function, then
 *      the order of the callbacks are guaranteed to be in the same order the
 *      compression or decompression requests were submitted for each session,
 *      so long as a single thread of execution is used for job submission.
 *
 * @context
 *      This is a synchronous function and it cannot sleep. It can be executed in
 *      a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance      Instance handle derived from discovery
 *                                  functions.
 * @param[in,out]   pSessionHandle  Pointer to a session handle.
 * @param[in,out]   pSessionData    Pointer to a user instantiated structure
 *                                  containing session data.
 * @param[in]       pContextBuffer  pointer to context buffer.  This is not
 *                                  required for stateless operations.
 *                                  The total size of the buffer list must
 *                                  be equal to or larger than the specified
 *                                  contextSize retrieved from the
 *                                  cpaDcGetSessionSize() function.
 * @param[in]        callbackFn     For synchronous operation this callback
 *                                  shall be a null pointer.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      dcInstance has been started using cpaDcStartInstance.
 * @post
 *      None
 * @note
 *      Only a synchronous version of this function is provided.
 *
 *      This initializes opaque data structures in the session handle. Data
 *      compressed under this session will be compressed to the level
 *      specified in the pSessionData structure. Lower compression level
 *      numbers indicate a request for faster compression at the
 *      expense of compression ratio.  Higher compression level numbers
 *      indicate a request for higher compression ratios at the expense of
 *      execution time.
 *
 *      The session is opaque to the user application and the session handle
 *      contains job specific data.
 *
 *      The pointer to the ContextBuffer will be stored in session specific
 *      data if required by the implementation.
 *
 *      It is not permitted to have multiple
 *      outstanding asynchronous compression requests for stateful sessions.
 *      It is possible to add
 *      parallelization to compression by using multiple sessions.
 *
 *      The window size specified in the pSessionData must be match exactly
 *      one of the supported window sizes specified in the capabilities
 *      structure.  If a bi-directional session is being initialized, then
 *      the window size must be valid for both compress and decompress.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcInitSession( CpaInstanceHandle     dcInstance,
        CpaDcSessionHandle              pSessionHandle,
        CpaDcSessionSetupData           *pSessionData,
        CpaBufferList                   *pContextBuffer,
        CpaDcCallbackFn                 callbackFn );


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Session Reset Function.
 *
 * @description
 *      This function will reset a previously initialized session handle
 *      Reset will fail if outstanding calls still exist for the initialized
 *      session handle.
 *      The client needs to retry the reset function at a later time.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]      dcInstance      Instance handle.
 * @param[in,out]  pSessionHandle  Session handle.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance function.
 *      The session has been initialized via cpaDcInitSession function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcResetSession(const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle );


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Session Remove Function.
 *
 * @description
 *      This function will remove a previously initialized session handle
 *      and the installed callback handler function. Removal will fail if
 *      outstanding calls still exist for the initialized session handle.
 *      The client needs to retry the remove function at a later time.
 *      The memory for the session handle MUST not be freed until this call
 *      has completed successfully.
 *
 * @context
 *      This is a synchronous function that cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]      dcInstance      Instance handle.
 * @param[in,out]  pSessionHandle  Session handle.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_RETRY          Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE       Error related to system resources.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcRemoveSession(const CpaInstanceHandle dcInstance,
        CpaDcSessionHandle pSessionHandle );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Deflate Compression Bound API
 *
 * @description
 *      This function provides the maximum output buffer size for a Deflate
 *      compression operation in the "worst case" (non-compressible) scenario.
 *      It's primary purpose is for output buffer memory allocation.
 *
 * @context
 *      This is a synchronous function that will not sleep. It can be
 *      executed in a context that does not permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]      dcInstance      Instance handle.
 * @param[in]      huffType        CpaDcHuffType to be used with this operation.
 * @param[in]      inputSize       Input Buffer size.
 * @param[out]     outputSize      Maximum output buffer size.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance function.
 * @post
 *      None
 * @note
 *      This is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcDeflateCompressBound(const CpaInstanceHandle dcInstance,
        CpaDcHuffType huffType,
        Cpa32U inputSize,
        Cpa32U *outputSize );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Submit a request to compress a buffer of data.
 *
 * @description
 *      This API consumes data from the input buffer and generates compressed
 *      data in the output buffer.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance          Target service instance.
 * @param[in,out]   pSessionHandle      Session handle.
 * @param[in]       pSrcBuff            Pointer to data buffer for compression.
 * @param[in]       pDestBuff           Pointer to buffer space for data after
 *                                      compression.
 * @param[in,out]   pResults            Pointer to results structure
 * @param[in]       flushFlag           Indicates the type of flush to be
 *                                      performed.
 * @param[in]       callbackTag         User supplied value to help correlate
 *                                      the callback with its associated
 *                                      request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 * @post
 *     pSessionHandle has session related state information
 * @note
 *     This function passes control to the compression service for processing
 *
 *  In synchronous mode the function returns the error status returned from the
 *      service. In asynchronous mode the status is returned by the callback
 *      function.
 *
 *  This function may be called repetitively with input until all of the
 *  input has been consumed by the compression service and all the output
 *      has been produced.
 *
 *  When this function returns, it may be that all of the available data
 *  in the input buffer has not been compressed.  This situation will
 *  occur when there is insufficient space in the output buffer.  The
 *  calling application should note the amount of data processed, and clear
 *  the output buffer and then submit the request again, with the input
 *  buffer pointer to the data that was not previously compressed.
 *
 *  Relationship between input buffers and results buffers.
 *  -# Implementations of this API must not modify the individual
 *     flat buffers of the input buffer list.
 *  -# The implementation communicates the amount of data
 *     consumed from the source buffer list via pResults->consumed arg.
 *  -# The implementation communicates the amount of data in the
 *     destination buffer list via pResults->produced arg.
 *
 *  Source Buffer Setup Rules
 *  -# The buffer list must have the correct number of flat buffers. This
 *         is specified by the numBuffers element of the CpaBufferList.
 *  -# Each flat buffer must have a pointer to contiguous memory that has
 *     been allocated by the calling application.  The
 *         number of octets to be compressed or decompressed must be stored
 *     in the dataLenInBytes element of the flat buffer.
 *  -# It is permissible to have one or more flat buffers with a zero length
 *     data store.  This function will process all flat buffers until the
 *     destination buffer is full or all source data has been processed.
 *     If a buffer has zero length, then no data will be processed from
 *     that buffer.
 *
 *  Source Buffer Processing Rules.
 *  -# The buffer list is processed in index order - SrcBuff->pBuffers[0]
 *     will be completely processed before SrcBuff->pBuffers[1] begins to
 *     be processed.
 *  -# The application must drain the destination buffers.
 *     If the source data was not completely consumed, the application
 *     must resubmit the request.
 *  -# On return, the pResults->consumed will indicate the number of bytes
 *     consumed from the input buffers.
 *
 *  Destination Buffer Setup Rules
 *  -# The destination buffer list must have storage for processed data.
 *     This implies at least one flat buffer must exist in the buffer list.
 *  -# For each flat buffer in the buffer list, the dataLenInBytes element
 *     must be set to the size of the buffer space.
 *  -# It is permissible to have one or more flat buffers with a zero length
 *         data store.
 *     If a buffer has zero length, then no data will be added to
 *     that buffer.
 *
 *  Destination Buffer Processing Rules.
 *  -# The buffer list is processed in index order - DestBuff->pBuffers[0]
 *     will be completely processed before DestBuff->pBuffers[1] begins to
 *     be processed.
 *  -# On return, the pResults->produced will indicate the number of bytes
 *     written to the output buffers.
 *  -# If processing has not been completed, the application must drain the
 *         destination buffers and resubmit the request. The application must
 *         reset the dataLenInBytes for each flat buffer in the destination
 *         buffer list.
 *
 *  Checksum rules.
 *      If a checksum is specified in the session setup data, then:
 *  -# For the first request for a particular data segment the checksum
 *     is initialised internally by the implementation.
 *  -# The checksum is maintained by the implementation between calls
 *         until the flushFlag is set to CPA_DC_FLUSH_FINAL indicating the
 *         end of a particular data segment.
 *      -# Intermediate checksum values are returned to the application,
 *         via the CpaDcRqResults structure, in response to each request.
 *         However these checksum values are not guaranteed to the valid
 *         until the call with flushFlag set to CPA_DC_FLUSH_FINAL
 *         completes successfully.
 *
 *  The application should set flushFlag to
 *      CPA_DC_FLUSH_FINAL to indicate processing a particular data segment
 *      is complete. It should be noted that this function may have to be
 *      called more than once to process data after the flushFlag parameter has
 *      been set to CPA_DC_FLUSH_FINAL if the destination buffer fills.  Refer
 *      to buffer processing rules.
 *
 *  For stateful operations, when the function is invoked with flushFlag
 *  set to CPA_DC_FLUSH_NONE or CPA_DC_FLUSH_SYNC, indicating more data
 *  is yet to come, the function may or may not retain data.  When the
 *  function is invoked with flushFlag set to CPA_DC_FLUSH_FULL or
 *  CPA_DC_FLUSH_FINAL, the function will process all buffered data.
 *
 *  For stateless operations, CPA_DC_FLUSH_FINAL will cause the BFINAL
 *  bit to be set for deflate compression. The initial checksum for the
 *  stateless operation should be set to 0. CPA_DC_FLUSH_NONE and
 *  CPA_DC_FLUSH_SYNC should not be used for stateless operations.
 *
 *  It is possible to maintain checksum and length information across
 *  cpaDcCompressData() calls with a stateless session without maintaining
 *  the full history state that is maintained by a stateful session. In this
 *  mode of operation, an initial checksum value of 0 is passed into the
 *  first cpaDcCompressData() call with the flush flag set to
 *  CPA_DC_FLUSH_FULL. On subsequent calls to cpaDcCompressData() for this
 *  session, the checksum passed to cpaDcCompressData should be set to the
 *  checksum value produced by the previous call to cpaDcCompressData().
 *  When the last block of input data is passed to cpaDcCompressData(), the
 *  flush flag should be set to CP_DC_FLUSH_FINAL. This will cause the BFINAL
 *  bit to be set in a deflate stream. It is the responsibility of the calling
 *  application to maintain overall lengths across the stateless requests
 *  and to pass the checksum produced by one request into the next request.
 *
 *  When an instance supports compressAndVerifyAndRecover, it is enabled by
 *  default when using cpaDcCompressData(). If this feature needs to be
 *  disabled, cpaDcCompressData2() must be used.
 *
 *  Synchronous or Asynchronous operation of the API is determined by
 *  the value of the callbackFn parameter passed to cpaDcInitSession()
 *  when the sessionHandle was setup. If a non-NULL value was specified
 *  then the supplied callback function will be invoked asynchronously
 *  with the response of this request.
 *
 *  Response ordering:
 *  For each session, the implementation must maintain the order of
 *  responses.  That is, if in asynchronous mode, the order of the callback
 *  functions must match the order of jobs submitted by this function.
 *  In a simple synchronous mode implementation, the practice of submitting
 *  a request and blocking on its completion ensure ordering is preserved.
 *
 *  This limitation does not apply if the application employs multiple
 *  threads to service a single session.
 *
 *  If this API is invoked asynchronous, the return code represents
 *  the success or not of asynchronously scheduling the request.
 *  The results of the operation, along with the amount of data consumed
 *  and produced become available when the callback function is invoked.
 *  As such, pResults->consumed and pResults->produced are available
 *  only when the operation is complete.
 *
 *  The application must not use either the source or destination buffers
 *  until the callback has completed.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcCompressData( CpaInstanceHandle dcInstance,
        CpaDcSessionHandle  pSessionHandle,
        CpaBufferList       *pSrcBuff,
        CpaBufferList       *pDestBuff,
        CpaDcRqResults      *pResults,
        CpaDcFlush          flushFlag,
        void                 *callbackTag );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Submit a request to compress a buffer of data.
 *
 * @description
 *      This API consumes data from the input buffer and generates compressed
 *      data in the output buffer. This API is very similar to
 *      cpaDcCompressData() except it provides a CpaDcOpData structure for
 *      passing additional input parameters not covered in cpaDcCompressData().
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance          Target service instance.
 * @param[in,out]   pSessionHandle      Session handle.
 * @param[in]       pSrcBuff            Pointer to data buffer for compression.
 * @param[in]       pDestBuff           Pointer to buffer space for data after
 *                                      compression.
 * @param[in]       pOpData             Additional input parameters.
 * @param[in,out]   pResults            Pointer to results structure
 * @param[in]       callbackTag         User supplied value to help correlate
 *                                      the callback with its associated
 *                                      request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 * @post
 *     pSessionHandle has session related state information
 * @note
 *     This function passes control to the compression service for processing
 *
 * @see
 *      cpaDcCompressData()
 *
 *****************************************************************************/
CpaStatus
cpaDcCompressData2( CpaInstanceHandle dcInstance,
        CpaDcSessionHandle  pSessionHandle,
        CpaBufferList       *pSrcBuff,
        CpaBufferList       *pDestBuff,
        CpaDcOpData         *pOpData,
        CpaDcRqResults      *pResults,
        void                 *callbackTag );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Submit a request to decompress a buffer of data.
 *
 * @description
 *      This API consumes compressed data from the input buffer and generates
 *      uncompressed data in the output buffer.
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance          Target service instance.
 * @param[in,out]   pSessionHandle      Session handle.
 * @param[in]       pSrcBuff            Pointer to data buffer for compression.
 * @param[in]       pDestBuff           Pointer to buffer space for data
 *                                      after decompression.
 * @param[in,out]   pResults            Pointer to results structure
 * @param[in]       flushFlag           When set to CPA_DC_FLUSH_FINAL, indicates
 *                                      that the input buffer contains all of
 *                                      the data for the compression session,
 *                                      allowing the function to release
 *                                      history data.
 * @param[in]        callbackTag        User supplied value to help correlate
 *                                      the callback with its associated
 *                                      request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 * @post
 *     pSessionHandle has session related state information
 * @note
 *      This function passes control to the compression service for
 *      decompression.  The function returns the status from the service.
 *
 *      This function may be called repetitively with input until all of the
 *      input has been provided and all the output has been consumed.
 *
 *      This function has identical buffer processing rules as
 *      cpaDcCompressData().
 *
 *      This function has identical checksum processing rules as
 *      cpaDcCompressData().
 *
 *      The application should set flushFlag to
 *      CPA_DC_FLUSH_FINAL to indicate processing a particular compressed
 *      data segment is complete. It should be noted that this function may
 *      have to be called more than once to process data after flushFlag
 *      has been set if the destination buffer fills.  Refer to
 *      buffer processing rules in cpaDcCompressData().
 *
 *      Synchronous or Asynchronous operation of the API is determined by
 *      the value of the callbackFn parameter passed to cpaDcInitSession()
 *      when the sessionHandle was setup. If a non-NULL value was specified
 *      then the supplied callback function will be invoked asynchronously
 *      with the response of this request, along with the callbackTag
 *      specified in the function.
 *
 *      The same response ordering constraints identified in the
 *      cpaDcCompressData API apply to this function.
 *
 * @see
 *      cpaDcCompressData()
 *
 *****************************************************************************/
CpaStatus
cpaDcDecompressData( CpaInstanceHandle dcInstance,
        CpaDcSessionHandle  pSessionHandle,
        CpaBufferList       *pSrcBuff,
        CpaBufferList       *pDestBuff,
        CpaDcRqResults      *pResults,
        CpaDcFlush          flushFlag,
        void                *callbackTag );


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Submit a request to decompress a buffer of data.
 *
 * @description
 *      This API consumes compressed data from the input buffer and generates
 *      uncompressed data in the output buffer. This API is very similar to
 *      cpaDcDecompressData() except it provides a CpaDcOpData structure for
 *      passing additional input parameters not covered in cpaDcDecompressData().
 *
 * @context
 *      When called as an asynchronous function it cannot sleep. It can be
 *      executed in a context that does not permit sleeping.
 *      When called as a synchronous function it may sleep. It MUST NOT be
 *      executed in a context that DOES NOT permit sleeping.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes when configured to operate in synchronous mode.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]       dcInstance          Target service instance.
 * @param[in,out]   pSessionHandle      Session handle.
 * @param[in]       pSrcBuff            Pointer to data buffer for compression.
 * @param[in]       pDestBuff           Pointer to buffer space for data
 *                                      after decompression.
 * @param[in]       pOpData             Additional input parameters.
 * @param[in,out]   pResults            Pointer to results structure
 * @param[in]        callbackTag        User supplied value to help correlate
 *                                      the callback with its associated
 *                                      request.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed.
 * @retval CPA_STATUS_RETRY         Resubmit the request.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_RESOURCE      Error related to system resources.
 * @retval CPA_DC_BAD_DATA          The input data was not properly formed.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 * @retval CPA_STATUS_RESTARTING    API implementation is restarting. Resubmit
 *                                  the request.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 * @post
 *     pSessionHandle has session related state information
 * @note
 *      This function passes control to the compression service for
 *      decompression.  The function returns the status from the service.
 *
 * @see
 *      cpaDcDecompressData()
 *      cpaDcCompressData2()
 *      cpaDcCompressData()
 *
 *****************************************************************************/
CpaStatus
cpaDcDecompressData2( CpaInstanceHandle dcInstance,
        CpaDcSessionHandle  pSessionHandle,
        CpaBufferList       *pSrcBuff,
        CpaBufferList       *pDestBuff,
        CpaDcOpData         *pOpData,
        CpaDcRqResults      *pResults,
        void                *callbackTag );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Generate compression header.
 *
 * @description
 *      This API generates the gzip or the zlib header and stores it in the
 *      output buffer.
 *
 * @context
 *      This function may be call from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in,out] pSessionHandle    Session handle.
 * @param[in] pDestBuff             Pointer to data buffer where the
 *                                  compression header will go.
 * @param[out] count                Pointer to counter filled in with
 *                                  header size.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 *
 * @note
 *      This function can output a 10 byte gzip header or 2 byte zlib header to
 *      the destination buffer. The session properties are used to determine
 *      the header type. To output a header the session must have been
 *      initialized with CpaDcCompType CPA_DC_DEFLATE for any other value no
 *      header is produced. To output a gzip header the session must have been
 *      initialized with CpaDcChecksum CPA_DC_CRC32. To output a zlib header
 *      the session must have been initialized with CpaDcChecksum CPA_DC_ADLER32.
 *      For CpaDcChecksum CPA_DC_NONE no header is output.
 *
 *      If the compression requires a gzip header, then this header requires
 *      at a minimum the following fields, defined in RFC1952:
 *          ID1: 0x1f
 *          ID2: 0x8b
 *          CM: Compression method = 8 for deflate
 *
 *      The zlib header is defined in RFC1950 and this function must implement
 *      as a minimum:
 *          CM: four bit compression method - 8 is deflate with window size to
 *              32k
 *          CINFO: four bit window size (see RFC1950 for details), 7 is 32k
 *              window
 *          FLG: defined as:
 *              -   Bits 0 - 4: check bits for CM, CINFO and FLG (see RFC1950)
 *              -   Bit 5:  FDICT 0 = default, 1 is preset dictionary
 *              -   Bits 6 - 7: FLEVEL, compression level (see RFC 1950)
 *
 *      The counter parameter will be set
 *      to the number of bytes added to the buffer. The pData will be
 *      not be changed.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcGenerateHeader( CpaDcSessionHandle pSessionHandle,
    CpaFlatBuffer *pDestBuff, Cpa32U *count );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Generate compression footer.
 *
 * @description
 *      This API generates the footer for gzip or zlib and stores it in the
 *      output buffer.
 * @context
 *      This function may be call from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      All session variables are reset
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in,out] pSessionHandle    Session handle.
 * @param[in] pDestBuff             Pointer to data buffer where the
 *                                  compression footer will go.
 * @param[in,out] pResults          Pointer to results structure filled by
 *                                  CpaDcCompressData.  Updated with the
 *                                  results of this API call
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      pSessionHandle has been setup using cpaDcInitSession()
 *      pResults structure has been filled by CpaDcCompressData().
 *
 * @note
 *      Depending on the session variables, this function can add the
 *      alder32 footer to the zlib compressed data as defined in RFC1950.  If
 *      required, it can also add the gzip footer, which is the crc32 of the
 *      uncompressed data and the length of the uncompressed data.  This
 *      section is defined in RFC1952. The session variables used to determine
 *      the header type are CpaDcCompType and CpaDcChecksum, see cpaDcGenerateHeader
 *      for more details.
 *
 *      An artifact of invoking this function for writing the footer data is
 *      that all opaque session specific data is re-initialized.  If the
 *      compression level and file types are consistent, the upper level
 *      application can continue processing compression requests using the
 *      same session handle.
 *
 *      The produced element of the pResults structure will be incremented by the
 *      numbers bytes added to the buffer.  The pointer to the buffer
 *      will not be modified.
 *
 *      This function is not supported for stateless sessions.
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcGenerateFooter( CpaDcSessionHandle pSessionHandle,
    CpaFlatBuffer *pDestBuff, CpaDcRqResults *pResults );


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Retrieve statistics
 *
 * @description
 *      This API retrieves the current statistics for a compression instance.
 *
 * @context
 *      This function may be call from any context.
 * @assumptions
 *      None
 * @sideEffects
 *        None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  dcInstance       Instance handle.
 * @param[out] pStatistics      Pointer to statistics structure.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_RESTARTING     API implementation is restarting. Resubmit
 *                                   the request.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *     None
 *
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcGetStats( CpaInstanceHandle dcInstance,
      CpaDcStats *pStatistics );

/*****************************************************************************/
/* Instance Discovery Functions */

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Get the number of device instances that are supported by the API
 *      implementation.
 *
 * @description
 *
 *     This function will get the number of device instances that are supported
 *     by an implementation of the compression API. This number is then used to
 *     determine the size of the array that must be passed to
 *     cpaDcGetInstances().
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
 * @param[out] pNumInstances        Pointer to where the number of
 *                                   instances will be written.
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
 *      cpaDcGetInstances
 *
 *****************************************************************************/
CpaStatus
cpaDcGetNumInstances(Cpa16U* pNumInstances);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Get the handles to the device instances that are supported by the
 *      API implementation.
 *
 * @description
 *
 *      This function will return handles to the device instances that are
 *      supported by an implementation of the compression API. These instance
 *      handles can then be used as input parameters with other compression API
 *      functions.
 *
 *      This function will populate an array that has been allocated by the
 *      caller. The size of this API is determined by the
 *      cpaDcGetNumInstances() function.
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
 * @param[in]  numInstances     Size of the array.
 * @param[out] dcInstances          Pointer to where the instance
 *                                   handles will be written.
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
 *      cpaDcGetInstances
 *
 *****************************************************************************/
CpaStatus
cpaDcGetInstances(Cpa16U numInstances,
                        CpaInstanceHandle* dcInstances);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Component utility function to determine the number of
 *      intermediate buffers required by an implementation.
 *
 * @description
 *      This function will determine the number of intermediate buffer lists
 *      required by an implementation for a compression instance. These buffers
 *      should then be allocated and provided when calling @ref cpaDcStartInstance()
 *      to start a compression instance that will use dynamic compression.
 *
 * @context
 *      This function may sleep, and  MUST NOT be called in interrupt context.
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
 * @param[in,out] instanceHandle        Handle to an instance of this API to be
 *                                      initialized.
 * @param[out]  pNumBuffers             When the function returns, this will
 *                                      specify the number of buffer lists that
 *                                      should be used as intermediate buffers
 *                                      when calling cpaDcStartInstance().
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
 *                                  is to shutdown and restart.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcStartInstance()
 *
 *****************************************************************************/
CpaStatus
cpaDcGetNumIntermediateBuffers(CpaInstanceHandle instanceHandle,
        Cpa16U *pNumBuffers);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compression Component Initialization and Start function.
 *
 * @description
 *      This function will initialize and start the compression component.
 *      It MUST be called before any other compress function is called. This
 *      function SHOULD be called only once (either for the very first time,
 *      or after an cpaDcStopInstance call which succeeded) per instance.
 *      Subsequent calls will have no effect.
 *
 *      If required by an implementation, this function can be provided with
 *      instance specific intermediate buffers.  The intent is to provide an
 *      instance specific location to store intermediate results during dynamic
 *      instance Huffman tree compression requests. The memory should be
 *      accessible by the compression engine. The buffers are to support
 *      deflate compression with dynamic Huffman Trees.  Each buffer list
 *      should be similar in size to twice the destination buffer size passed
 *      to the compress API. The number of intermediate buffer lists may vary
 *      between implementations and so @ref cpaDcGetNumIntermediateBuffers()
 *      should be called first to determine the number of intermediate
 *      buffers required by the implementation.
 *
 *      If not required, this parameter can be passed in as NULL.
 *
 * @context
 *      This function may sleep, and  MUST NOT be called in interrupt context.
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
 * @param[in,out] instanceHandle        Handle to an instance of this API to be
 *                                      initialized.
 * @param[in]   numBuffers              Number of buffer lists represented by
 *                                      the pIntermediateBuffers parameter.
 *                                      Note: @ref cpaDcGetNumIntermediateBuffers()
 *                                      can be used to determine the number of
 *                                      intermediate buffers that an implementation
 *                                      requires.
 * @param[in]   pIntermediateBuffers    Optional pointer to Instance specific
 *                                      DRAM buffer.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
 *                                  is to shutdown and restart.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcStopInstance()
 *      cpaDcGetNumIntermediateBuffers()
 *
 *****************************************************************************/
CpaStatus
cpaDcStartInstance(CpaInstanceHandle instanceHandle,
        Cpa16U numBuffers,
        CpaBufferList **pIntermediateBuffers);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Compress Component Stop function.
 *
 * @description
 *      This function will stop the Compression component and free
 *      all system resources associated with it. The client MUST ensure that
 *      all outstanding operations have completed before calling this function.
 *      The recommended approach to ensure this is to deregister all session or
 *      callback handles before calling this function. If outstanding
 *      operations still exist when this function is invoked, the callback
 *      function for each of those operations will NOT be invoked and the
 *      shutdown will continue.  If the component is to be restarted, then a
 *      call to cpaDcStartInstance is required.
 *
 * @context
 *      This function may sleep, and so MUST NOT be called in interrupt
 *      context.
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
 * @param[in] instanceHandle        Handle to an instance of this API to be
 *                                  shutdown.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
 *                                  is to ensure requests are not still being
 *                                  submitted and that all sessions are
 *                                  deregistered. If this does not help, then
 *                                  forcefully remove the component from the
 *                                  system.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaDcStartInstance
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaDcStartInstance()
 *
 *****************************************************************************/
CpaStatus
cpaDcStopInstance(CpaInstanceHandle instanceHandle);


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Function to get information on a particular instance.
 *
 * @description
 *      This function will provide instance specific information through a
 *      @ref CpaInstanceInfo2 structure.
 *
 * @context
 *      This function will be executed in a context that requires that sleeping
 *      MUST NOT be permitted.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      Yes
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle       Handle to an instance of this API to be
 *                                  initialized.
 * @param[out] pInstanceInfo2       Pointer to the memory location allocated by
 *                                  the client into which the CpaInstanceInfo2
 *                                  structure will be written.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The client has retrieved an instanceHandle from successive calls to
 *      @ref cpaDcGetNumInstances and @ref cpaDcGetInstances.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaDcGetNumInstances,
 *      cpaDcGetInstances,
 *      CpaInstanceInfo2
 *
 *****************************************************************************/
CpaStatus
cpaDcInstanceGetInfo2(const CpaInstanceHandle instanceHandle,
        CpaInstanceInfo2 * pInstanceInfo2);

/*****************************************************************************/
/* Instance Notification Functions                                           */
/*****************************************************************************/
/**
 *****************************************************************************
  * @ingroup cpaDc
 *      Callback function for instance notification support.
 *
 * @description
 *      This is the prototype for the instance notification callback function.
 *      The callback function is passed in as a parameter to the
 *      @ref cpaDcInstanceSetNotificationCb function.
 *
 * @context
 *      This function will be executed in a context that requires that sleeping
 *      MUST NOT be permitted.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle   Instance handle.
 * @param[in] pCallbackTag     Opaque value provided by user while making
 *                             individual function calls.
 * @param[in] instanceEvent    The event that will trigger this function to
 *                             get invoked.
 *
 * @retval
 *      None
 * @pre
 *      Component has been initialized and the notification function has been
 *  set via the cpaDcInstanceSetNotificationCb function.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      cpaDcInstanceSetNotificationCb(),
 *
 *****************************************************************************/
typedef void (*CpaDcInstanceNotificationCbFunc)(
        const CpaInstanceHandle instanceHandle,
        void * pCallbackTag,
        const CpaInstanceEvent instanceEvent);

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Subscribe for instance notifications.
 *
 * @description
 *      Clients of the CpaDc interface can subscribe for instance notifications
 *      by registering a @ref CpaDcInstanceNotificationCbFunc function.
 *
 * @context
 *      This function may be called from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle           Instance handle.
 * @param[in] pInstanceNotificationCb  Instance notification callback
 *                                     function pointer.
 * @param[in] pCallbackTag             Opaque value provided by user while
 *                                     making individual function calls.
 *
 * @retval CPA_STATUS_SUCCESS          Function executed successfully.
 * @retval CPA_STATUS_FAIL             Function failed.
 * @retval CPA_STATUS_INVALID_PARAM    Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      Instance has been initialized.
 * @post
 *      None
 * @note
 *      None
 * @see
 *      CpaDcInstanceNotificationCbFunc
 *
 *****************************************************************************/
CpaStatus
cpaDcInstanceSetNotificationCb(
        const CpaInstanceHandle instanceHandle,
        const CpaDcInstanceNotificationCbFunc pInstanceNotificationCb,
        void *pCallbackTag);


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Get the size of the memory required to hold the session information.
 *
 * @description
 *
 *      The client of the Data Compression API is responsible for
 *      allocating sufficient memory to hold session information and the context
 *      data. This function provides a means for determining the size of the
 *      session information and the size of the context data.
 *
 * @context
 *      No restrictions
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] dcInstance            Instance handle.
 * @param[in] pSessionData          Pointer to a user instantiated structure
 *                                  containing session data.
 * @param[out] pSessionSize         On return, this parameter will be the size
 *                                  of the memory that will be
 *                                  required by cpaDcInitSession() for session
 *                                  data.
 * @param[out] pContextSize         On return, this parameter will be the size
 *                                  of the memory that will be required
 *                                  for context data.  Context data is
 *                                  save/restore data including history and
 *                                  any implementation specific data that is
 *                                  required for a save/restore operation.
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
 *      Only a synchronous version of this function is provided.
 *
 *      It is expected that context data is comprised of the history and
 *      any data stores that are specific to the history such as linked
 *      lists or hash tables.
 *      For stateless sessions the context size returned from this function
 *      will be zero. For stateful sessions the context size returned will
 *      depend on the session setup data.
 *
 *      Session data is expected to include interim checksum values, various
 *      counters and other session related data that needs to persist
 *      between invocations.
 *      For a given implementation of this API, it is safe to assume that
 *      cpaDcGetSessionSize() will always return the same session size and
 *      that the size will not be different for different setup data
 *      parameters. However, it should be noted that the size may change:
 *       (1) between different implementations of the API (e.g. between software
 *           and hardware implementations or between different hardware
 *           implementations)
 *       (2) between different releases of the same API implementation.
 *
 * @see
 *      cpaDcInitSession()
 *
 *****************************************************************************/
CpaStatus
cpaDcGetSessionSize(CpaInstanceHandle dcInstance,
        CpaDcSessionSetupData* pSessionData,
        Cpa32U* pSessionSize, Cpa32U* pContextSize );

/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Function to return the size of the memory which must be allocated for
 *      the pPrivateMetaData member of CpaBufferList.
 *
 * @description
 *      This function is used to obtain the size (in bytes) required to allocate
 *      a buffer descriptor for the pPrivateMetaData member in the
 *      CpaBufferList structure.
 *      Should the function return zero then no meta data is required for the
 *      buffer list.
 *
 * @context
 *      This function may be called from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle      Handle to an instance of this API.
 * @param[in]  numBuffers          The number of pointers in the CpaBufferList.
 *                                 This is the maximum number of CpaFlatBuffers
 *                                 which may be contained in this CpaBufferList.
 * @param[out] pSizeInBytes        Pointer to the size in bytes of memory to be
 *                                 allocated when the client wishes to allocate
 *                                 a cpaFlatBuffer.
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
 *      None
 * @see
 *      cpaDcGetInstances()
 *
 *****************************************************************************/
CpaStatus
cpaDcBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
        Cpa32U numBuffers,
        Cpa32U *pSizeInBytes);


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Function to return a string indicating the specific error that occurred
 *      within the system.
 *
 * @description
 *      When a function returns any error including CPA_STATUS_SUCCESS, the
 *      client can invoke this function to get a string which describes the
 *      general error condition, and if available additional information on
 *      the specific error.
 *      The Client MUST allocate CPA_STATUS_MAX_STR_LENGTH_IN_BYTES bytes for  the buffer
 *      string.
 *
 * @context
 *      This function may be called from any context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] dcInstance        Handle to an instance of this API.
 * @param[in] errStatus         The error condition that occurred.
 * @param[in,out] pStatusText   Pointer to the string buffer that will
 *                              be updated with the status text. The invoking
 *                              application MUST allocate this buffer to be
 *                              exactly CPA_STATUS_MAX_STR_LENGTH_IN_BYTES.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Note, in this scenario
 *                                  it is INVALID to call this function a
 *                                  second time.
 * @retval CPA_STATUS_INVALID_PARAM Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @note
 *      None
 * @see
 *      CpaStatus
 *
 *****************************************************************************/

CpaStatus
cpaDcGetStatusText(const CpaInstanceHandle dcInstance,
                   const CpaStatus errStatus,
                   Cpa8S * pStatusText);


/**
 *****************************************************************************
 * @ingroup cpaDc
 *      Set Address Translation function
 *
 * @description
 *      This function is used to set the virtual to physical address
 *      translation routine for the instance. The specified routine
 *      is used by the instance to perform any required translation of
 *      a virtual address to a physical address. If the application
 *      does not invoke this function, then the instance will use its
 *      default method, such as virt2phys, for address translation.
 *
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
 * @param[in] instanceHandle         Data Compression API instance handle.
 * @param[in] virtual2Physical       Routine that performs virtual to
 *                                    physical address translation.
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaDcSetAddressTranslation(const CpaInstanceHandle instanceHandle,
                           CpaVirtualToPhysical virtual2Physical);
#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_DC_H */
