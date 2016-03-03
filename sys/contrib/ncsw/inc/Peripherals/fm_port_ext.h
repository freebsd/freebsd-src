/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**************************************************************************//**
 @File          fm_port_ext.h

 @Description   FM-Port Application Programming Interface.
*//***************************************************************************/
#ifndef __FM_PORT_EXT
#define __FM_PORT_EXT

#include "error_ext.h"
#include "std_ext.h"
#include "fm_pcd_ext.h"
#include "fm_ext.h"
#include "net_ext.h"


/**************************************************************************//**

 @Group         FM_grp Frame Manager API

 @Description   FM API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Group         FM_PORT_grp FM Port

 @Description   FM Port API

                The FM uses a general module called "port" to represent a Tx port
                (MAC), an Rx port (MAC), offline parsing flow or host command
                flow. There may be up to 17 (may change) ports in an FM - 5 Tx
                ports (4 for the 1G MACs, 1 for the 10G MAC), 5 Rx Ports, and 7
                Host command/Offline parsing ports. The SW driver manages these
                ports as sub-modules of the FM, i.e. after an FM is initialized,
                its ports may be initialized and operated upon.

                The port is initialized aware of its type, but other functions on
                a port may be indifferent to its type. When necessary, the driver
                verifies coherency and returns error if applicable.

                On initialization, user specifies the port type and it's index
                (relative to the port's type). Host command and Offline parsing
                ports share the same id range, I.e user may not initialized host
                command port 0 and offline parsing port 0.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   An enum for defining port PCD modes.
                This enum defines the superset of PCD engines support - i.e. not
                all engines have to be used, but all have to be enabled. The real
                flow of a specific frame depends on the PCD configuration and the
                frame headers and payload.
*//***************************************************************************/
typedef enum e_FmPortPcdSupport {
    e_FM_PORT_PCD_SUPPORT_NONE = 0,             /**< BMI to BMI, PCD is not used */
    e_FM_PORT_PCD_SUPPORT_PRS_ONLY,             /**< Use only Parser */
    e_FM_PORT_PCD_SUPPORT_PLCR_ONLY,            /**< Use only Policer */
    e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR,         /**< Use Parser and Policer */
    e_FM_PORT_PCD_SUPPORT_PRS_AND_KG,           /**< Use Parser and Keygen */
    e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC,    /**< Use Parser, Keygen and Coarse Classification */
    e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_CC_AND_PLCR,
                                                /**< Use all PCD engines */
    e_FM_PORT_PCD_SUPPORT_PRS_AND_KG_AND_PLCR   /**< Use Parser, Keygen and Policer */
#ifdef FM_CAPWAP_SUPPORT
    ,
    e_FM_PORT_PCD_SUPPORT_CC_ONLY,              /**< Use only Coarse Classification */
    e_FM_PORT_PCD_SUPPORT_CC_AND_KG,            /**< Use Coarse Classification,and Keygen */
    e_FM_PORT_PCD_SUPPORT_CC_AND_KG_AND_PLCR    /**< Use Coarse Classification, Keygen and Policer */
#endif /* FM_CAPWAP_SUPPORT */
} e_FmPortPcdSupport;

/**************************************************************************//**
 @Description   Port interrupts
*//***************************************************************************/
typedef enum e_FmPortExceptions {
    e_FM_PORT_EXCEPTION_IM_BUSY                 /**< Independent-Mode Rx-BUSY */
} e_FmPortExceptions;


/**************************************************************************//**
 @Collection    General FM Port defines
*//***************************************************************************/
#define FM_PORT_PRS_RESULT_NUM_OF_WORDS     8   /**< Number of 4 bytes words in parser result */
/* @} */

/**************************************************************************//**
 @Collection   FM Frame error
*//***************************************************************************/
typedef uint32_t    fmPortFrameErrSelect_t;                         /**< typedef for defining Frame Descriptor errors */

#define FM_PORT_FRM_ERR_UNSUPPORTED_FORMAT              0x04000000  /**< Offline parsing only! Unsupported Format */
#define FM_PORT_FRM_ERR_LENGTH                          0x02000000  /**< Offline parsing only! Length Error */
#define FM_PORT_FRM_ERR_DMA                             0x01000000  /**< DMA Data error */
#ifdef FM_CAPWAP_SUPPORT
#define FM_PORT_FRM_ERR_NON_FM                          0x00400000  /**< non Frame-Manager error; probably come from SEC that
                                                                         was chained to FM */
#endif /* FM_CAPWAP_SUPPORT */
#define FM_PORT_FRM_ERR_PHYSICAL                        0x00080000  /**< Rx FIFO overflow, FCS error, code error, running disparity
                                                                         error (SGMII and TBI modes), FIFO parity error. PHY
                                                                         Sequence error, PHY error control character detected. */
#define FM_PORT_FRM_ERR_SIZE                            0x00040000  /**< Frame too long OR Frame size exceeds max_length_frame  */
#define FM_PORT_FRM_ERR_CLS_DISCARD                     0x00020000  /**< classification discard */
#define FM_PORT_FRM_ERR_EXTRACTION                      0x00008000  /**< Extract Out of Frame */
#define FM_PORT_FRM_ERR_NO_SCHEME                       0x00004000  /**< No Scheme Selected */
#define FM_PORT_FRM_ERR_KEYSIZE_OVERFLOW                0x00002000  /**< Keysize Overflow */
#define FM_PORT_FRM_ERR_COLOR_YELLOW                    0x00000400  /**< Frame color is yellow */
#define FM_PORT_FRM_ERR_COLOR_RED                       0x00000800  /**< Frame color is red */
#define FM_PORT_FRM_ERR_ILL_PLCR                        0x00000200  /**< Illegal Policer Profile selected */
#define FM_PORT_FRM_ERR_PLCR_FRAME_LEN                  0x00000100  /**< Policer frame length error */
#define FM_PORT_FRM_ERR_PRS_TIMEOUT                     0x00000080  /**< Parser Time out Exceed */
#define FM_PORT_FRM_ERR_PRS_ILL_INSTRUCT                0x00000040  /**< Invalid Soft Parser instruction */
#define FM_PORT_FRM_ERR_PRS_HDR_ERR                     0x00000020  /**< Header error was identified during parsing */
#define FM_PORT_FRM_ERR_BLOCK_LIMIT_EXCEEDED            0x00000008  /**< Frame parsed beyind 256 first bytes */
#define FM_PORT_FRM_ERR_PROCESS_TIMEOUT                 0x00000001  /**< FPM Frame Processing Timeout Exceeded */
/* @} */



/**************************************************************************//**
 @Group         FM_PORT_init_grp FM Port Initialization Unit

 @Description   FM Port Initialization Unit

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   Exceptions user callback routine, will be called upon an
                exception passing the exception identification.

 @Param[in]     h_App      - User's application descriptor.
 @Param[in]     exception  - The exception.
  *//***************************************************************************/
typedef void (t_FmPortExceptionCallback) (t_Handle h_App, e_FmPortExceptions exception);

/**************************************************************************//**
 @Description   User callback function called by driver with received data.

                User provides this function. Driver invokes it.

 @Param[in]     h_App           Application's handle originally specified to
                                the API Config function
 @Param[in]     p_Data          A pointer to data received
 @Param[in]     length          length of received data
 @Param[in]     status          receive status and errors
 @Param[in]     position        position of buffer in frame
 @Param[in]     h_BufContext    A handle of the user acossiated with this buffer

 @Retval        e_RX_STORE_RESPONSE_CONTINUE - order the driver to continue Rx
                                               operation for all ready data.
 @Retval        e_RX_STORE_RESPONSE_PAUSE    - order the driver to stop Rx operation.
*//***************************************************************************/
typedef e_RxStoreResponse (t_FmPortImRxStoreCallback) (t_Handle h_App,
                                                       uint8_t  *p_Data,
                                                       uint16_t length,
                                                       uint16_t status,
                                                       uint8_t  position,
                                                       t_Handle h_BufContext);

/**************************************************************************//**
 @Description   User callback function called by driver when transmit completed.

                User provides this function. Driver invokes it.

 @Param[in]     h_App           Application's handle originally specified to
                                the API Config function
 @Param[in]     p_Data          A pointer to data received
 @Param[in]     status          transmit status and errors
 @Param[in]     lastBuffer      is last buffer in frame
 @Param[in]     h_BufContext    A handle of the user acossiated with this buffer
 *//***************************************************************************/
typedef void (t_FmPortImTxConfCallback) (t_Handle   h_App,
                                         uint8_t    *p_Data,
                                         uint16_t   status,
                                         t_Handle   h_BufContext);

/**************************************************************************//**
 @Description   A structure of information about each of the external
                buffer pools used by the port,
*//***************************************************************************/
typedef struct t_FmPortExtPoolParams {
    uint8_t                 id;                 /**< External buffer pool id */
    uint16_t                size;               /**< External buffer pool buffer size */
} t_FmPortExtPoolParams;

/**************************************************************************//**
 @Description   A structure for informing the driver about the external
                buffer pools allocated in the BM and used by this port.
*//***************************************************************************/
typedef struct t_FmPortExtPools {
    uint8_t                 numOfPoolsUsed;     /**< Number of pools use by this port */
    t_FmPortExtPoolParams   extBufPool[FM_PORT_MAX_NUM_OF_EXT_POOLS];
                                                /**< Parameters for each port */
} t_FmPortExtPools;

/**************************************************************************//**
 @Description   structure for additional Rx port parameters
*//***************************************************************************/
typedef struct t_FmPortRxParams {
    uint32_t                errFqid;            /**< Error Queue Id. */
    uint32_t                dfltFqid;           /**< Default Queue Id.  */
    uint16_t                liodnOffset;        /**< Port's LIODN offset. */
    t_FmPortExtPools        extBufPools;        /**< Which external buffer pools are used
                                                     (up to FM_PORT_MAX_NUM_OF_EXT_POOLS), and their sizes. */
} t_FmPortRxParams;

/**************************************************************************//**
 @Description   structure for additional non-Rx port parameters
*//***************************************************************************/
typedef struct t_FmPortNonRxParams {
    uint32_t                errFqid;            /**< Error Queue Id. */
    uint32_t                dfltFqid;           /**< For Tx and HC - Default Confirmation queue,
                                                     0 means no Tx confirmation for processed
                                                     frames. For OP - default Rx queue. */
    uint32_t                qmChannel;          /**< QM-channel dedicated to this port; will be used
                                                     by the FM for dequeue. */
#ifdef FM_OP_PARTITION_ERRATA_FMANx8
    uint16_t                opLiodnOffset;      /**< For Offline Parsing ports only. Port's LIODN offset. */
#endif  /* FM_OP_PARTITION_ERRATA_FMANx8 */
} t_FmPortNonRxParams;

/**************************************************************************//**
 @Description   structure for additional Rx port parameters
*//***************************************************************************/
typedef struct t_FmPortImRxTxParams {
    t_Handle                    h_FmMuram;          /**< A handle of the FM-MURAM partition */
    uint16_t                    liodnOffset;        /**< For Rx ports only. Port's LIODN Offset. */
    uint8_t                     dataMemId;          /**< Memory partition ID for data buffers */
    uint32_t                    dataMemAttributes;  /**< Memory attributes for data buffers */
    t_BufferPoolInfo            rxPoolParams;       /**< For Rx ports only. */
    t_FmPortImRxStoreCallback   *f_RxStore;         /**< For Rx ports only. */
    t_FmPortImTxConfCallback    *f_TxConf;          /**< For Tx ports only. */
} t_FmPortImRxTxParams;

/**************************************************************************//**
 @Description   Union for additional parameters depending on port type
*//***************************************************************************/
typedef union u_FmPortSpecificParams {
    t_FmPortImRxTxParams        imRxTxParams;       /**< Rx/Tx Independent-Mode port parameter structure */
    t_FmPortRxParams            rxParams;           /**< Rx port parameters structure */
    t_FmPortNonRxParams         nonRxParams;        /**< Non-Rx port parameters structure */
} u_FmPortSpecificParams;

/**************************************************************************//**
 @Description   structure representing FM initialization parameters
*//***************************************************************************/
typedef struct t_FmPortParams {
    uintptr_t                   baseAddr;           /**< Virtual Address of memory mapped FM Port registers.*/
    t_Handle                    h_Fm;               /**< A handle to the FM object this port related to */
    e_FmPortType                portType;           /**< Port type */
    uint8_t                     portId;             /**< Port Id - relative to type */
    bool                        independentModeEnable;
                                                    /**< This port is Independent-Mode - Used for Rx/Tx ports only! */
    uint16_t                    liodnBase;          /**< Irrelevant for P4080 rev 1. LIODN base for this port, to be
                                                         used together with LIODN offset. */
    u_FmPortSpecificParams      specificParams;     /**< Additional parameters depending on port
                                                         type. */

    t_FmPortExceptionCallback   *f_Exception;       /**< Callback routine to be called of PCD exception */
    t_Handle                    h_App;              /**< A handle to an application layer object; This handle will
                                                         be passed by the driver upon calling the above callbacks */
} t_FmPortParams;


/**************************************************************************//**
 @Function      FM_PORT_Config

 @Description   Creates descriptor for the FM PORT module.

                The routine returns a handle (descriptor) to the FM PORT object.
                This descriptor must be passed as first parameter to all other
                FM PORT function calls.

                No actual initialization or configuration of FM hardware is
                done by this routine.

 @Param[in]     p_FmPortParams   - Pointer to data structure of parameters

 @Retval        Handle to FM object, or NULL for Failure.
*//***************************************************************************/
t_Handle FM_PORT_Config(t_FmPortParams *p_FmPortParams);

/**************************************************************************//**
 @Function      FM_PORT_Init

 @Description   Initializes the FM PORT module

 @Param[in]     h_FmPort - FM PORT module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PORT_Init(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_Free

 @Description   Frees all resources that were assigned to FM PORT module.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_FmPort - FM PORT module descriptor

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PORT_Free(t_Handle h_FmPort);


/**************************************************************************//**
 @Group         FM_PORT_advanced_init_grp    FM Port Advanced Configuration Unit

 @Description   Configuration functions used to change default values.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   enum for defining QM frame dequeue
*//***************************************************************************/
typedef enum e_FmPortDeqType {
   e_FM_PORT_DEQ_TYPE1,             /**< Dequeue from the SP channel - with priority precedence,
                                         and Intra-Class Scheduling respected. */
   e_FM_PORT_DEQ_TYPE2,             /**< Dequeue from the SP channel - with active FQ precedence,
                                         and Intra-Class Scheduling respected. */
   e_FM_PORT_DEQ_TYPE3              /**< Dequeue from the SP channel - with active FQ precedence,
                                         and override Intra-Class Scheduling */
} e_FmPortDeqType;

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
/**************************************************************************//**
 @Description   enum for defining QM frame dequeue
*//***************************************************************************/
typedef enum e_FmPortDeqPrefetchOption {
   e_FM_PORT_DEQ_NO_PREFETCH,       /**< QMI preforms a dequeue action for a single frame
                                         only when a dedicated portID Tnum is waiting. */
   e_FM_PORT_DEQ_PARTIAL_PREFETCH,  /**< QMI preforms a dequeue action for 3 frames when
                                         one dedicated portId tnum is waiting. */
   e_FM_PORT_DEQ_FULL_PREFETCH      /**< QMI preforms a dequeue action for 3 frames when
                                         no dedicated portId tnums are waiting. */

} e_FmPortDeqPrefetchOption;
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

/**************************************************************************//**
 @Description   enum for defining port DMA swap mode
*//***************************************************************************/
typedef enum e_FmPortDmaSwap {
    e_FM_PORT_DMA_NO_SWP,           /**< No swap, transfer data as is.*/
    e_FM_PORT_DMA_SWP_PPC_LE,       /**< The transferred data should be swapped
                                         in PowerPc Little Endian mode. */
    e_FM_PORT_DMA_SWP_BE            /**< The transferred data should be swapped
                                         in Big Endian mode */
} e_FmPortDmaSwap;

/**************************************************************************//**
 @Description   enum for defining port DMA cache attributes
*//***************************************************************************/
typedef enum e_FmPortDmaCache {
    e_FM_PORT_DMA_NO_STASH = 0,     /**< Cacheable, no Allocate (No Stashing) */
    e_FM_PORT_DMA_STASH = 1         /**< Cacheable and Allocate (Stashing on) */
} e_FmPortDmaCache;

/**************************************************************************//**
 @Description   enum for defining port default color
*//***************************************************************************/
typedef enum e_FmPortColor {
    e_FM_PORT_COLOR_GREEN,          /**< Default port color is green */
    e_FM_PORT_COLOR_YELLOW,         /**< Default port color is yellow */
    e_FM_PORT_COLOR_RED,            /**< Default port color is red */
    e_FM_PORT_COLOR_OVERRIDE        /**< Ignore color */
} e_FmPortColor;

/**************************************************************************//**
 @Description   struct for defining Dual Tx rate limiting scale
*//***************************************************************************/
typedef enum e_FmPortDualRateLimiterScaleDown {
    e_FM_PORT_DUAL_RATE_LIMITER_NONE = 0,           /**< Use only single rate limiter  */
    e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_2,    /**< Divide high rate limiter by 2 */
    e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_4,    /**< Divide high rate limiter by 4 */
    e_FM_PORT_DUAL_RATE_LIMITER_SCALE_DOWN_BY_8     /**< Divide high rate limiter by 8 */
} e_FmPortDualRateLimiterScaleDown;


/**************************************************************************//**
 @Description   struct for defining FM port resources
*//***************************************************************************/
typedef struct t_FmPortRsrc {
    uint32_t    num;                /**< Committed required resource */
    uint32_t    extra;              /**< Extra (not committed) required resource */
} t_FmPortRsrc;

/**************************************************************************//**
 @Description   struct for defining pool depletion criteria
*//***************************************************************************/
typedef struct t_FmPortBufPoolDepletion {
    bool        numberOfPoolsModeEnable;            /**< select mode in which pause frames will be sent after
                                                         a number of pools are depleted */
    uint8_t     numOfPools;                         /**< the minimum number of depleted pools that will
                                                         invoke pause frames transmission. */
    bool        poolsToConsider[BM_MAX_NUM_OF_POOLS];
                                                    /**< For each pool, TRUE if it should be considered for
                                                         depletion (Note - this pool must be used by this port!) */
    bool        singlePoolModeEnable;               /**< select mode in which pause frames will be sent after
                                                         a single of pools are depleted */
    bool        poolsToConsiderForSingleMode[BM_MAX_NUM_OF_POOLS];
                                                    /**< For each pool, TRUE if it should be considered for
                                                         depletion (Note - this pool must be used by this port!) */
} t_FmPortBufPoolDepletion;

/**************************************************************************//**
 @Description   struct for defining observed pool depletion
*//***************************************************************************/
typedef struct t_FmPortObservedBufPoolDepletion {
    t_FmPortBufPoolDepletion    poolDepletionParams;/**< parameters to define pool depletion */
    t_FmPortExtPools            poolsParams;        /**< Which external buffer pools are observed
                                                         (up to FM_PORT_MAX_NUM_OF_OBSERVED_EXT_POOLS),
                                                         and their sizes. */
} t_FmPortObservedBufPoolDepletion;

/**************************************************************************//**
 @Description   struct for defining Tx rate limiting
*//***************************************************************************/
typedef struct t_FmPortRateLimit {
    uint16_t                            maxBurstSize;           /**< in kBytes for Tx ports, in frames
                                                                     for offline parsing ports. (note that
                                                                     for early chips burst size is
                                                                     rounded up to a multiply of 1000 frames).*/
    uint32_t                            rateLimit;              /**< in Kb/sec for Tx ports, in frame/sec for
                                                                     offline parsing ports. Rate limit refers to
                                                                     data rate (rather than line rate). */
    e_FmPortDualRateLimiterScaleDown    rateLimitDivider;       /**< For offline parsing ports only. Not-valid
                                                                     for some earlier chip revisions */
} t_FmPortRateLimit;

/**************************************************************************//**
 @Description   struct for defining define the parameters of
                the Rx port performance counters
*//***************************************************************************/
typedef struct t_FmPortPerformanceCnt {
    uint8_t     taskCompVal;            /**< Task compare value */
    uint8_t     queueCompVal;           /**< Rx queue/Tx confirm queue compare
                                             value (unused for H/O) */
    uint8_t     dmaCompVal;             /**< Dma compare value */
    uint32_t    fifoCompVal;            /**< Fifo compare value (in bytes) */
} t_FmPortPerformanceCnt;

/**************************************************************************//**
 @Description   struct for defining buffer content.
*//***************************************************************************/
typedef struct t_FmPortBufferPrefixContent {
    uint16_t    privDataSize;       /**< Number of bytes to be left at the beginning
                                         of the external buffer */
    bool        passPrsResult;      /**< TRUE to pass the parse result to/from the FM */
    bool        passTimeStamp;      /**< TRUE to pass the timeStamp to/from the FM */
    bool        passHashResult;     /**< TRUE to pass the KG hash result to/from the FM */
    bool        passAllOtherPCDInfo;/**< Add all other Internal-Context information:
                                         AD, hash-result, key, etc. */
    uint16_t    dataAlign;          /**< 0 to use driver's default alignment, other value
                                         for selecting a data alignment (must be a
                                         power of 2) */
#ifdef DEBUG
    bool        passDebugInfo;      /**< Debug-information */
#endif /* DEBUG */
#ifdef FM_CAPWAP_SUPPORT
    uint8_t     manipExtraSpace;    /**< Maximum extra size needed (insertion-size minus removal-size) */
#endif /* FM_CAPWAP_SUPPORT */
} t_FmPortBufferPrefixContent;

/**************************************************************************//**
 @Description   struct for defining backup Bm Pools.
*//***************************************************************************/
typedef struct t_FmPortBackupBmPools {
    uint8_t     numOfBackupPools;        /**< Number of BM backup pools -
                                             must be smaller than the total number of
                                             pools defined for the specified port.*/
    uint8_t     poolIds[FM_PORT_MAX_NUM_OF_EXT_POOLS];
                                        /**< numOfBackupPools pool id's, specifying which
                                             pools should be used only as backup. Pool
                                             id's specified here must be a subset of the
                                             pools used by the specified port.*/
} t_FmPortBackupBmPools;


/**************************************************************************//**
 @Function      FM_PORT_ConfigDeqHighPriority

 @Description   Calling this routine changes the dequeue priority in the
                internal driver data base from its default configuration
                [TRUE]

                May be used for Non-Rx ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     highPri     TRUE to select high priority, FALSE for normal operation.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDeqHighPriority(t_Handle h_FmPort, bool highPri);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDeqType

 @Description   Calling this routine changes the dequeue type parameter in the
                internal driver data base from its default configuration
                [e_FM_PORT_DEQ_TYPE1].

                May be used for Non-Rx ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     deqType     According to QM definition.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDeqType(t_Handle h_FmPort, e_FmPortDeqType deqType);

#ifdef FM_QMI_DEQ_OPTIONS_SUPPORT
/**************************************************************************//**
 @Function      FM_PORT_ConfigDeqPrefetchOption

 @Description   Calling this routine changes the dequeue prefetch option parameter in the
                internal driver data base from its default configuration
                [e_FM_PORT_DEQ_FULL_PREFETCH]
                Note: Available for some chips only

                May be used for Non-Rx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     deqPrefetchOption   New option

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDeqPrefetchOption(t_Handle h_FmPort, e_FmPortDeqPrefetchOption deqPrefetchOption);
#endif /* FM_QMI_DEQ_OPTIONS_SUPPORT */

/**************************************************************************//**
 @Function      FM_PORT_ConfigDeqByteCnt

 @Description   Calling this routine changes the dequeue byte count parameter in
                the internal driver data base from its default configuration [2000].

                May be used for Non-Rx ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     deqByteCnt      New byte count

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDeqByteCnt(t_Handle h_FmPort, uint16_t deqByteCnt);

/**************************************************************************//**
 @Function      FM_PORT_ConfigTxFifoMinFillLevel

 @Description   Calling this routine changes the fifo minimum
                fill level parameter in the internal driver data base
                from its default configuration  [0]

                May be used for Tx ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     minFillLevel    New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigTxFifoMinFillLevel(t_Handle h_FmPort, uint32_t minFillLevel);

/**************************************************************************//**
 @Function      FM_PORT_ConfigTxFifoDeqPipelineDepth

 @Description   Calling this routine changes the fifo dequeue
                pipeline depth parameter in the internal driver data base

                from its default configuration: 1G ports: [2],
                10G port: [8]

                May be used for Tx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     deqPipelineDepth    New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigTxFifoDeqPipelineDepth(t_Handle h_FmPort, uint8_t deqPipelineDepth);

/**************************************************************************//**
 @Function      FM_PORT_ConfigTxFifoLowComfLevel

 @Description   Calling this routine changes the fifo low comfort level
                parameter in internal driver data base
                from its default configuration  [5]

                May be used for Tx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     fifoLowComfLevel    New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigTxFifoLowComfLevel(t_Handle h_FmPort, uint32_t fifoLowComfLevel);

/**************************************************************************//**
 @Function      FM_PORT_ConfigRxFifoThreshold

 @Description   Calling this routine changes the threshold of the FIFO
                fill level parameter in the internal driver data base
                from its default configuration [BMI_MAX_FIFO_SIZE]

                If the total number of buffers which are
                currently in use and associated with the
                specific RX port exceed this threshold, the
                BMI will signal the MAC to send a pause frame
                over the link.

                May be used for Rx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     fifoThreshold       New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigRxFifoThreshold(t_Handle h_FmPort, uint32_t fifoThreshold);

/**************************************************************************//**
 @Function      FM_PORT_ConfigRxFifoPriElevationLevel

 @Description   Calling this routine changes the priority elevation level
                parameter in the internal driver data base from its default
                configuration  [BMI_MAX_FIFO_SIZE]

                If the total number of buffers which are currently in use and
                associated with the specific RX port exceed the amount specified
                in priElevationLevel, BMI will signal the main FM's DMA to
                elevate the FM priority on the system bus.

                May be used for Rx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     priElevationLevel   New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigRxFifoPriElevationLevel(t_Handle h_FmPort, uint32_t priElevationLevel);

/**************************************************************************//**
 @Function      FM_PORT_ConfigBufferPrefixContent

 @Description   Defines the structure, size and content of the application buffer.
                The prefix will
                In Tx ports, if 'passPrsResult', the application
                should set a value to their offsets in the prefix of
                the FM will save the first 'privDataSize', than,
                depending on 'passPrsResult' and 'passTimeStamp', copy parse result
                and timeStamp, and the packet itself (in this order), to the
                application buffer, and to offset.
                Calling this routine changes the buffer margins definitions
                in the internal driver data base from its default
                configuration: Data size:  [0]
                               Pass Parser result: [FALSE].
                               Pass timestamp: [FALSE].

                May be used for all ports

 @Param[in]     h_FmPort                        A handle to a FM Port module.
 @Param[in,out] p_FmPortBufferPrefixContent     A structure of parameters describing the
                                                structure of the buffer.
                                                Out parameter: Start margin - offset
                                                of data from start of external buffer.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigBufferPrefixContent(t_Handle h_FmPort, t_FmPortBufferPrefixContent *p_FmPortBufferPrefixContent);


/**************************************************************************//**
 @Function      FM_PORT_ConfigCheksumLastBytesIgnore

 @Description   Calling this routine changes the number of checksum bytes to ignore
                parameter in the internal driver data base from its default configuration
                [0]

                May be used by Tx & Rx ports only

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     cheksumLastBytesIgnore    New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigCheksumLastBytesIgnore(t_Handle h_FmPort, uint8_t cheksumLastBytesIgnore);

/**************************************************************************//**
 @Function      FM_PORT_ConfigCutBytesFromEnd

 @Description   Calling this routine changes the number of bytes to cut from a
                frame's end parameter in the internal driver data base
                from its default configuration  [4]
                Note that if the result of (frame length before chop - cutBytesFromEnd) is
                less than 14 bytes, the chop operation is not executed.

                May be used for Rx ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     cutBytesFromEnd     New value

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigCutBytesFromEnd(t_Handle h_FmPort, uint8_t cutBytesFromEnd);

/**************************************************************************//**
 @Function      FM_PORT_ConfigPoolDepletion

 @Description   Calling this routine enables pause frame generation depending on the
                depletion status of BM pools. It also defines the conditions to activate
                this functionality. By default, this functionality is disabled.

                May be used for Rx ports only

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     p_BufPoolDepletion      A structure of pool depletion parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigPoolDepletion(t_Handle h_FmPort, t_FmPortBufPoolDepletion *p_BufPoolDepletion);

/**************************************************************************//**
 @Function      FM_PORT_ConfigObservedPoolDepletion

 @Description   Calling this routine enables a mechanism to stop port enqueue
                depending on the depletion status of selected BM pools.
                It also defines the conditions to activate
                this functionality. By default, this functionality is disabled.

                Note: Available for some chips only

                May be used for Offline Parsing ports only

 @Param[in]     h_FmPort                            A handle to a FM Port module.
 @Param[in]     p_FmPortObservedBufPoolDepletion    A structure of parameters for pool depletion.


 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigObservedPoolDepletion(t_Handle h_FmPort, t_FmPortObservedBufPoolDepletion *p_FmPortObservedBufPoolDepletion);

/**************************************************************************//**
 @Function      FM_PORT_ConfigExtBufPools

 @Description   This routine should be called for offline parsing ports
                that internally use BM buffer pools. In such cases, e.g. for fragmentation and
                re-assembly, the FM needs new BM buffers. By calling this routine the user
                specifies the BM buffer pools that should be used.

                Note: Available for some chips only

                May be used for Offline Parsing ports only

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_FmPortExtPools    A structure of parameters for the external pools.


 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigExtBufPools(t_Handle h_FmPort, t_FmPortExtPools *p_FmPortExtPools);

/**************************************************************************//**
 @Function      FM_PORT_ConfigBackupPools

 @Description   Calling this routine allows the configuration of some of the BM pools
                defined for this port as backup pools.
                A pool configured to be a backup pool will be used only if all other
                enabled non-backup pools are depleted.

                May be used for Rx ports only

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     p_FmPortBackupBmPools   An array of pool id's. All pools specified here will
                                        be defined as backup pools.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigBackupPools(t_Handle h_FmPort, t_FmPortBackupBmPools *p_FmPortBackupBmPools);

/**************************************************************************//**
 @Function      FM_PORT_ConfigFrmDiscardOverride

 @Description   Calling this routine changes the error frames destination parameter
                in the internal driver data base from its default configuration:
                override = [FALSE]

                May be used for Rx and offline parsing ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     override    TRUE to override dicarding of error frames and
                            enqueueing them to error queue.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigFrmDiscardOverride(t_Handle h_FmPort, bool override);

/**************************************************************************//**
 @Function      FM_PORT_ConfigErrorsToDiscard

 @Description   Calling this routine changes the behaviour on error parameter
                in the internal driver data base from its default configuration:
                [FM_PORT_FRM_ERR_CLS_DISCARD].
                If a requested error was previously defined as "ErrorsToEnqueue" it's
                definition will change and the frame will be discarded.
                Errors that were not defined either as "ErrorsToEnqueue" nor as
                "ErrorsToDiscard", will be forwarded to CPU.


                May be used for Rx and offline parsing ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     errs        A list of errors to discard

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigErrorsToDiscard(t_Handle h_FmPort, fmPortFrameErrSelect_t errs);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDmaSwapData

 @Description   Calling this routine changes the DMA swap data aparameter
                in the internal driver data base from its default
                configuration  [e_FM_PORT_DMA_NO_SWP]

                May be used for all port types

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     swapData    New selection

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDmaSwapData(t_Handle h_FmPort, e_FmPortDmaSwap swapData);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDmaIcCacheAttr

 @Description   Calling this routine changes the internal context cache
                attribute parameter in the internal driver data base
                from its default configuration  [e_FM_PORT_DMA_NO_STASH]

                May be used for all port types

 @Param[in]     h_FmPort               A handle to a FM Port module.
 @Param[in]     intContextCacheAttr    New selection

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDmaIcCacheAttr(t_Handle h_FmPort, e_FmPortDmaCache intContextCacheAttr);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDmaHdrAttr

 @Description   Calling this routine changes the header cache
                attribute parameter in the internal driver data base
                from its default configuration  [e_FM_PORT_DMA_NO_STASH]

                May be used for all port types

 @Param[in]     h_FmPort                    A handle to a FM Port module.
 @Param[in]     headerCacheAttr             New selection

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDmaHdrAttr(t_Handle h_FmPort, e_FmPortDmaCache headerCacheAttr);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDmaScatterGatherAttr

 @Description   Calling this routine changes the scatter gather cache
                attribute parameter in the internal driver data base
                from its default configuration  [e_FM_PORT_DMA_NO_STASH]

                May be used for all port types

 @Param[in]     h_FmPort                    A handle to a FM Port module.
 @Param[in]     scatterGatherCacheAttr      New selection

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDmaScatterGatherAttr(t_Handle h_FmPort, e_FmPortDmaCache scatterGatherCacheAttr);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDmaWriteOptimize

 @Description   Calling this routine changes the write optimization
                parameter in the internal driver data base
                from its default configuration:  optimize = [TRUE]

                May be used for non-Tx port types

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     optimize    TRUE to enable optimization, FALSE for normal operation

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDmaWriteOptimize(t_Handle h_FmPort, bool optimize);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDfltColor

 @Description   Calling this routine changes the internal default color parameter
                in the internal driver data base
                from its default configuration  [e_FM_PORT_COLOR_GREEN]

                May be used for all port types

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     color           New selection

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDfltColor(t_Handle h_FmPort, e_FmPortColor color);

/**************************************************************************//**
 @Function      FM_PORT_ConfigSyncReq

 @Description   Calling this routine changes the synchronization attribute parameter
                in the internal driver data base from its default configuration:
                syncReq = [TRUE]

                May be used for all port types

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     syncReq         TRUE to request synchronization, FALSE otherwize.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigSyncReq(t_Handle h_FmPort, bool syncReq);

/**************************************************************************//**
 @Function      FM_PORT_ConfigForwardReuseIntContext

 @Description   This routine is relevant for Rx ports that are routed to offline
                parsing. It changes the internal context reuse option
                in the internal driver data base from its default configuration:
                reuse = [FALSE]

                May be used for Rx ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     reuse           TRUE to reuse internal context on frames
                                forwarded to offline parsing.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigForwardReuseIntContext(t_Handle h_FmPort, bool reuse);

/**************************************************************************//**
 @Function      FM_PORT_ConfigDontReleaseTxBufToBM

 @Description   This routine should be called if no Tx confirmation
                is done, and yet buffers should not be released to the BM.
                Normally, buffers are returned using the Tx confirmation
                process. When Tx confirmation is not used (defFqid=0),
                buffers are typically released to the BM. This routine
                may be called to avoid this behavior and not release the
                buffers.

                May be used for Tx ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ConfigDontReleaseTxBufToBM(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_ConfigIMMaxRxBufLength

 @Description   Changes the maximum receive buffer length from its default
                configuration: Closest rounded down power of 2 value of the
                data buffer size.

                The maximum receive buffer length directly affects the structure
                of received frames (single- or multi-buffered) and the performance
                of both the FM and the driver.

                The selection between single- or multi-buffered frames should be
                done according to the characteristics of the specific application.
                The recommended mode is to use a single data buffer per packet,
                as this mode provides the best performance. However, the user can
                select to use multiple data buffers per packet.

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     newVal          Maximum receive buffer length (in bytes).

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
                This routine is to be used only if Independent-Mode is enabled.
*//***************************************************************************/
t_Error FM_PORT_ConfigIMMaxRxBufLength(t_Handle h_FmPort, uint16_t newVal);

/**************************************************************************//**
 @Function      FM_PORT_ConfigIMRxBdRingLength

 @Description   Changes the receive BD ring length from its default
                configuration:[128]

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     newVal          The desired BD ring length.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
                This routine is to be used only if Independent-Mode is enabled.
*//***************************************************************************/
t_Error FM_PORT_ConfigIMRxBdRingLength(t_Handle h_FmPort, uint16_t newVal);

/**************************************************************************//**
 @Function      FM_PORT_ConfigIMTxBdRingLength

 @Description   Changes the transmit BD ring length from its default
                configuration:[16]

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     newVal          The desired BD ring length.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
                This routine is to be used only if Independent-Mode is enabled.
*//***************************************************************************/
t_Error FM_PORT_ConfigIMTxBdRingLength(t_Handle h_FmPort, uint16_t newVal);

/**************************************************************************//**
 @Function      FM_PORT_ConfigIMFmanCtrlExternalStructsMemory

 @Description   Configures memory partition and attributes for FMan-Controller
                data structures (e.g. BD rings).
                Calling this routine changes the internal driver data base
                from its default configuration
                [0 , MEMORY_ATTR_CACHEABLE].

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     memId           Memory partition ID.
 @Param[in]     memAttributes   Memory attributes mask (a combination of MEMORY_ATTR_x flags).

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error  FM_PORT_ConfigIMFmanCtrlExternalStructsMemory(t_Handle h_FmPort,
                                                       uint8_t  memId,
                                                       uint32_t memAttributes);

/**************************************************************************//**
 @Function      FM_PORT_ConfigIMPolling

 @Description   Changes the Rx flow from interrupt driven (default) to polling.

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
                This routine is to be used only if Independent-Mode is enabled.
*//***************************************************************************/
t_Error FM_PORT_ConfigIMPolling(t_Handle h_FmPort);

/** @} */ /* end of FM_PORT_advanced_init_grp group */
/** @} */ /* end of FM_PORT_init_grp group */


/**************************************************************************//**
 @Group         FM_PORT_runtime_control_grp FM Port Runtime Control Unit

 @Description   FM Port Runtime control unit API functions, definitions and enums.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   enum for defining FM Port counters
*//***************************************************************************/
typedef enum e_FmPortCounters {
    e_FM_PORT_COUNTERS_CYCLE,                       /**< BMI performance counter */
    e_FM_PORT_COUNTERS_TASK_UTIL,                   /**< BMI performance counter */
    e_FM_PORT_COUNTERS_QUEUE_UTIL,                  /**< BMI performance counter */
    e_FM_PORT_COUNTERS_DMA_UTIL,                    /**< BMI performance counter */
    e_FM_PORT_COUNTERS_FIFO_UTIL,                   /**< BMI performance counter */
    e_FM_PORT_COUNTERS_RX_PAUSE_ACTIVATION,         /**< BMI Rx only performance counter */
    e_FM_PORT_COUNTERS_FRAME,                       /**< BMI statistics counter */
    e_FM_PORT_COUNTERS_DISCARD_FRAME,               /**< BMI statistics counter */
    e_FM_PORT_COUNTERS_DEALLOC_BUF,                 /**< BMI deallocate buffer statistics counter */
    e_FM_PORT_COUNTERS_RX_BAD_FRAME,                /**< BMI Rx only statistics counter */
    e_FM_PORT_COUNTERS_RX_LARGE_FRAME,              /**< BMI Rx only statistics counter */
    e_FM_PORT_COUNTERS_RX_OUT_OF_BUFFERS_DISCARD,   /**< BMI Rx only statistics counter */
    e_FM_PORT_COUNTERS_RX_FILTER_FRAME,             /**< BMI Rx & OP only statistics counter */
    e_FM_PORT_COUNTERS_RX_LIST_DMA_ERR,             /**< BMI Rx, OP & HC only statistics counter */
    e_FM_PORT_COUNTERS_WRED_DISCARD,                /**< BMI OP & HC only statistics counter */
    e_FM_PORT_COUNTERS_LENGTH_ERR,                  /**< BMI non-Rx statistics counter */
    e_FM_PORT_COUNTERS_UNSUPPRTED_FORMAT,           /**< BMI non-Rx statistics counter */
    e_FM_PORT_COUNTERS_DEQ_TOTAL,                   /**< QMI counter */
    e_FM_PORT_COUNTERS_ENQ_TOTAL,                   /**< QMI counter */
    e_FM_PORT_COUNTERS_DEQ_FROM_DEFAULT,            /**< QMI counter */
    e_FM_PORT_COUNTERS_DEQ_CONFIRM                  /**< QMI counter */
} e_FmPortCounters;

/**************************************************************************//**
 @Description   Structure for Port id parameters.
                Fields commented 'IN' are passed by the port module to be used
                by the FM module.
                Fields commented 'OUT' will be filled by FM before returning to port.
*//***************************************************************************/
typedef struct t_FmPortCongestionGrps {
    uint16_t    numOfCongestionGrpsToConsider;          /**< The number of required congestion groups
                                                             to define the size of the following array */
    uint8_t     congestionGrpsToConsider[FM_PORT_NUM_OF_CONGESTION_GRPS];
                                                        /**< An array of 'numOfCongestionGrpsToConsider'
                                                             describing the groups */
} t_FmPortCongestionGrps;



#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
/**************************************************************************//**
 @Function      FM_PORT_DumpRegs

 @Description   Dump all regs.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_FmPort - FM PORT module descriptor

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_DumpRegs(t_Handle h_FmPort);
#endif /* (defined(DEBUG_ERRORS) && ... */

/**************************************************************************//**
 @Function      FM_PORT_GetBufferDataOffset

 @Description   Relevant for Rx ports.
                Returns the data offset from the beginning of the data buffer

 @Param[in]     h_FmPort - FM PORT module descriptor

 @Return        data offset.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
uint32_t FM_PORT_GetBufferDataOffset(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_GetBufferICInfo

 @Description   Returns the Internal Context offset from the beginning of the data buffer

 @Param[in]     h_FmPort - FM PORT module descriptor
 @Param[in]     p_Data      - A pointer to the data buffer.

 @Return        Internal context info pointer on success, NULL if 'allOtherInfo' was not
                configured for this port.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
uint8_t * FM_PORT_GetBufferICInfo(t_Handle h_FmPort, char *p_Data);

#ifdef DEBUG
/**************************************************************************//**
 @Function      FM_PORT_GetBufferDebugInfo

 @Description   Returns the debug info offset from the beginning of the data buffer

 @Param[in]     h_FmPort - FM PORT module descriptor
 @Param[in]     p_Data      - A pointer to the data buffer.

 @Return        Debug info pointer on success, NULL if 'passDebugInfo' was not
                configured for this port.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
uint8_t * FM_PORT_GetBufferDebugInfo(t_Handle h_FmPort, char *p_Data);
#endif /* DEBUG */

/**************************************************************************//**
 @Function      FM_PORT_GetBufferPrsResult

 @Description   Returns the pointer to the parse result in the data buffer.
                In Rx ports this is relevant after reception, if parse
                result is configured to be part of the data passed to the
                application. For non Rx ports it may be used to get the pointer
                of the area in the buffer where parse result should be
                initialized - if so configured.
                See FM_PORT_ConfigBufferPrefixContent for data buffer prefix
                configuration.

 @Param[in]     h_FmPort    - FM PORT module descriptor
 @Param[in]     p_Data      - A pointer to the data buffer.

 @Return        Parse result pointer on success, NULL if parse result was not
                configured for this port.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_FmPrsResult * FM_PORT_GetBufferPrsResult(t_Handle h_FmPort, char *p_Data);

/**************************************************************************//**
 @Function      FM_PORT_GetBufferTimeStamp

 @Description   Returns the time stamp in the data buffer.
                Relevant for Rx ports for getting the buffer time stamp.
                See FM_PORT_ConfigBufferPrefixContent for data buffer prefix
                configuration.

 @Param[in]     h_FmPort    - FM PORT module descriptor
 @Param[in]     p_Data      - A pointer to the data buffer.

 @Return        A pointer to the hash result on success, NULL otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
uint64_t * FM_PORT_GetBufferTimeStamp(t_Handle h_FmPort, char *p_Data);

/**************************************************************************//**
 @Function      FM_PORT_GetBufferHashResult

 @Description   Given a data buffer, on the condition that hash result was defined
                as a part of the buffer content (see FM_PORT_ConfigBufferPrefixContent)
                this routine will return the pointer to the hash result location in the
                buffer prefix.

 @Param[in]     h_FmPort    - FM PORT module descriptor
 @Param[in]     p_Data      - A pointer to the data buffer.

 @Return        A pointer to the hash result on success, NULL otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
uint8_t * FM_PORT_GetBufferHashResult(t_Handle h_FmPort, char *p_Data);

/**************************************************************************//**
 @Function      FM_PORT_Disable

 @Description   Gracefully disable an FM port. The port will not start new tasks after all
                tasks associated with the port are terminated.

 @Param[in]     h_FmPort    A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
                This is a blocking routine, it returns after port is
                gracefully stopped, i.e. the port will not except new frames,
                but it will finish all frames or tasks which were already began
*//***************************************************************************/
t_Error FM_PORT_Disable(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_Enable

 @Description   A runtime routine provided to allow disable/enable of port.

 @Param[in]     h_FmPort    A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_Enable(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_SetRateLimit

 @Description   Calling this routine enables rate limit algorithm.
                By default, this functionality is disabled.
                Note that rate-limit mechanism uses the FM time stamp.
                The selected rate limit specified here would be
                rounded DOWN to the nearest 16M.

                May be used for Tx and offline parsing ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     p_RateLimit     A structure of rate limit parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetRateLimit(t_Handle h_FmPort, t_FmPortRateLimit *p_RateLimit);

/**************************************************************************//**
 @Function      FM_PORT_DeleteRateLimit

 @Description   Calling this routine disables and clears rate limit
                initialization.

                May be used for Tx and offline parsing ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_DeleteRateLimit(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_SetStatisticsCounters

 @Description   Calling this routine enables/disables port's statistics counters.
                By default, counters are enabled.

                May be used for all port types

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     enable      TRUE to enable, FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetStatisticsCounters(t_Handle h_FmPort, bool enable);

/**************************************************************************//**
 @Function      FM_PORT_SetFrameQueueCounters

 @Description   Calling this routine enables/disables port's enqueue/dequeue counters.
                By default, counters are enabled.

                May be used for all ports

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     enable      TRUE to enable, FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetFrameQueueCounters(t_Handle h_FmPort, bool enable);

/**************************************************************************//**
 @Function      FM_PORT_SetPerformanceCounters

 @Description   Calling this routine enables/disables port's performance counters.
                By default, counters are enabled.

                May be used for all port types

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     enable                  TRUE to enable, FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetPerformanceCounters(t_Handle h_FmPort, bool enable);

/**************************************************************************//**
 @Function      FM_PORT_SetPerformanceCounters

 @Description   Calling this routine defines port's performance
                counters parameters.

                May be used for all port types

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     p_FmPortPerformanceCnt  A pointer to a structure of performance
                                        counters parameters.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetPerformanceCountersParams(t_Handle h_FmPort, t_FmPortPerformanceCnt *p_FmPortPerformanceCnt);

/**************************************************************************//**
 @Function      FM_PORT_AnalyzePerformanceParams

 @Description   User may call this routine to so the driver will analyze if the
                basic performance parameters are correct and also the driver may
                suggest of improvments; The basic parameters are FIFO sizes, number
                of DMAs and number of TNUMs for the port.

                May be used for all port types

 @Param[in]     h_FmPort                A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_AnalyzePerformanceParams(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_SetNumOfOpenDmas

 @Description   Calling this routine updates the number of open DMA requested for
                this port.


                May be used for all port types.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_NumOfOpenDmas     A structure of resource requested parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetNumOfOpenDmas(t_Handle h_FmPort, t_FmPortRsrc *p_NumOfOpenDmas);

/**************************************************************************//**
 @Function      FM_PORT_SetNumOfTasks

 @Description   Calling this routine updates the number of tasks requested for
                this port.

                May be used for all port types.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_NumOfTasks        A structure of resource requested parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetNumOfTasks(t_Handle h_FmPort, t_FmPortRsrc *p_NumOfTasks);

/**************************************************************************//**
 @Function      FM_PORT_SetSizeOfFifo

 @Description   Calling this routine updates the Fifo size resource requested for
                this port.

                May be used for all port types - note that only Rx has 'extra'
                fifo size. For other ports 'extra' field must be disabled.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_SizeOfFifo        A structure of resource requested parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetSizeOfFifo(t_Handle h_FmPort, t_FmPortRsrc *p_SizeOfFifo);

/**************************************************************************//**
 @Function      FM_PORT_SetAllocBufCounter

 @Description   Calling this routine enables/disables BM pool allocate
                buffer counters.
                By default, counters are enabled.

                May be used for Rx ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     poolId      BM pool id.
 @Param[in]     enable      TRUE to enable, FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetAllocBufCounter(t_Handle h_FmPort, uint8_t poolId, bool enable);

/**************************************************************************//**
 @Function      FM_PORT_GetCounter

 @Description   Reads one of the FM PORT counters.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     fmPortCounter       The requested counter.

 @Return        Counter's current value.

 @Cautions      Allowed only following FM_PORT_Init().
                Note that it is user's responsibility to call this routine only
                for enabled counters, and there will be no indication if a
                disabled counter is accessed.
*//***************************************************************************/
uint32_t FM_PORT_GetCounter(t_Handle h_FmPort, e_FmPortCounters fmPortCounter);

/**************************************************************************//**
 @Function      FM_PORT_ModifyCounter

 @Description   Sets a value to an enabled counter. Use "0" to reset the counter.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     fmPortCounter       The requested counter.
 @Param[in]     value               The requested value to be written into the counter.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ModifyCounter(t_Handle h_FmPort, e_FmPortCounters fmPortCounter, uint32_t value);

/**************************************************************************//**
 @Function      FM_PORT_GetAllocBufCounter

 @Description   Reads one of the FM PORT buffer counters.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     poolId              The requested pool.

 @Return        Counter's current value.

 @Cautions      Allowed only following FM_PORT_Init().
                Note that it is user's responsibility to call this routine only
                for enabled counters, and there will be no indication if a
                disabled counter is accessed.
*//***************************************************************************/
uint32_t FM_PORT_GetAllocBufCounter(t_Handle h_FmPort, uint8_t poolId);

/**************************************************************************//**
 @Function      FM_PORT_ModifyAllocBufCounter

 @Description   Sets a value to an enabled counter. Use "0" to reset the counter.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     poolId              The requested pool.
 @Param[in]     value               The requested value to be written into the counter.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ModifyAllocBufCounter(t_Handle h_FmPort,  uint8_t poolId, uint32_t value);

/**************************************************************************//**
 @Function      FM_PORT_AddCongestionGrps

 @Description   This routine effects the corresponding Tx port.
                It should be called in order to enable pause
                frame transmission in case of congestion in one or more
                of the congestion groups relevant to this port.
                Each call to this routine may add one or more congestion
                groups to be considered relevant to this port.

                May be used for Rx, or  RX+OP ports only (depending on chip)

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_CongestionGrps    A pointer to an array of congestion groups
                                    id's to consider.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_AddCongestionGrps(t_Handle h_FmPort, t_FmPortCongestionGrps *p_CongestionGrps);

/**************************************************************************//**
 @Function      FM_PORT_RemoveCongestionGrps

 @Description   This routine effects the corresponding Tx port. It should be
                called when congestion groups were
                defined for this port and are no longer relevant, or pause
                frames transmitting is not required on their behalf.
                Each call to this routine may remove one or more congestion
                groups to be considered relevant to this port.

                May be used for Rx, or RX+OP ports only (depending on chip)

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_CongestionGrps    A pointer to an array of congestion groups
                                    id's to consider.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_RemoveCongestionGrps(t_Handle h_FmPort, t_FmPortCongestionGrps *p_CongestionGrps);

/**************************************************************************//**
 @Function      FM_PORT_IsStalled

 @Description   A routine for checking whether the specified port is stalled.

 @Param[in]     h_FmPort            A handle to a FM Port module.

 @Return        TRUE if port is stalled, FALSE otherwize

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
bool FM_PORT_IsStalled(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_ReleaseStalled

 @Description   This routine may be called in case the port was stalled and may
                now be released.

 @Param[in]     h_FmPort    A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_ReleaseStalled(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_SetRxL4ChecksumVerify

 @Description   This routine is relevant for Rx ports (1G and 10G). The routine
                set/clear the L3/L4 checksum verification (on RX side).
                Note that this takes affect only if hw-parser is enabled!

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     l4Checksum      boolean indicates whether to do L3/L4 checksum
                                on frames or not.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetRxL4ChecksumVerify(t_Handle h_FmPort, bool l4Checksum);

/**************************************************************************//**
 @Function      FM_PORT_SetErrorsRoute

 @Description   Errors selected for this routine will cause a frame with that error
                to be enqueued to error queue.
                Errors not selected for this routine will cause a frame with that error
                to be enqueued to the one of the other port queues.
                By default all errors are defined to be enqueued to error queue.
                Errors that were configured to be discarded (at initialization)
                may not be selected here.

                May be used for Rx and offline parsing ports only

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     errs        A list of errors to enqueue to error queue

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Config() and before FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetErrorsRoute(t_Handle h_FmPort, fmPortFrameErrSelect_t errs);

/**************************************************************************//**
 @Function      FM_PORT_SetIMExceptions

 @Description   Calling this routine enables/disables FM PORT interrupts.
                Note: Not available for guest partition.

 @Param[in]     h_FmPort        FM PORT module descriptor.
 @Param[in]     exception       The exception to be selected.
 @Param[in]     enable          TRUE to enable interrupt, FALSE to mask it.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetIMExceptions(t_Handle h_FmPort, e_FmPortExceptions exception, bool enable);



/**************************************************************************//**
 @Group         FM_PORT_pcd_runtime_control_grp FM Port PCD Runtime Control Unit

 @Description   FM Port PCD Runtime control unit API functions, definitions and enums.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   A structure defining the KG scheme after the parser.
                This is relevant only to change scheme selection mode - from
                direct to indirect and vice versa, or when the scheme is selected directly,
                to select the scheme id.

*//***************************************************************************/
typedef struct t_FmPcdKgSchemeSelect {
    bool        direct;                 /**< TRUE to use 'h_Scheme' directly, FALSE to use LCV.*/
    t_Handle    h_DirectScheme;         /**< Relevant for 'direct'=TRUE only.
                                             'h_DirectScheme' selects the scheme after parser. */
} t_FmPcdKgSchemeSelect;

/**************************************************************************//**
 @Description   A structure of scheme parameters
*//***************************************************************************/
typedef struct t_FmPcdPortSchemesParams {
    uint8_t     numOfSchemes;                           /**< Number of schemes for port to be bound to. */
    t_Handle    h_Schemes[FM_PCD_KG_NUM_OF_SCHEMES];    /**< Array of 'numOfSchemes' schemes for the
                                                             port to be bound to */
} t_FmPcdPortSchemesParams;

/**************************************************************************//**
 @Description   Union for defining port protocol parameters for parser
*//***************************************************************************/
typedef union u_FmPcdHdrPrsOpts {
    /* MPLS */
    struct {
        bool            labelInterpretationEnable;  /**< When this bit is set, the last MPLS label will be
                                                         interpreted as described in HW spec table. When the bit
                                                         is cleared, the parser will advance to MPLS next parse */
        e_NetHeaderType nextParse;                  /**< must be equal or higher than IPv4 */
    } mplsPrsOptions;
    /* VLAN */
    struct {
        uint16_t        tagProtocolId1;             /**< User defined Tag Protocol Identifier, to be recognized
                                                         on VLAN TAG on top of 0x8100 and 0x88A8 */
        uint16_t        tagProtocolId2;             /**< User defined Tag Protocol Identifier, to be recognized
                                                         on VLAN TAG on top of 0x8100 and 0x88A8 */
    } vlanPrsOptions;
    /* PPP */
    struct{
        bool            enableMTUCheck;             /**< Check validity of MTU according to RFC2516 */
    } pppoePrsOptions;

    /* IPV6 */
    struct{
        bool            routingHdrDisable;          /**< Disable routing header */
    } ipv6PrsOptions;

    /* UDP */
    struct{
        bool            padIgnoreChecksum;          /**< TRUE to ignore pad in checksum */
    } udpPrsOptions;

    /* TCP */
    struct {
        bool            padIgnoreChecksum;          /**< TRUE to ignore pad in checksum */
    } tcpPrsOptions;
} u_FmPcdHdrPrsOpts;

/**************************************************************************//**
 @Description   A structure for defining each header for the parser
*//***************************************************************************/
typedef struct t_FmPcdPrsAdditionalHdrParams {
    e_NetHeaderType         hdr;            /**< Selected header */
    bool                    errDisable;     /**< TRUE to disable error indication */
    bool                    swPrsEnable;    /**< Enable jump to SW parser when this
                                                 header is recognized by the HW parser. */
    uint8_t                 indexPerHdr;    /**< Normally 0, if more than one sw parser
                                                 attachments exists for the same header,
                                                 (in the main sw parser code) use this
                                                 index to distinguish between them. */
    bool                    usePrsOpts;     /**< TRUE to use parser options. */
    u_FmPcdHdrPrsOpts       prsOpts;        /**< A union according to header type,
                                                 defining the parser options selected.*/
} t_FmPcdPrsAdditionalHdrParams;

/**************************************************************************//**
 @Description   struct for defining port PCD parameters
*//***************************************************************************/
typedef struct t_FmPortPcdPrsParams {
    uint8_t                         prsResultPrivateInfo;           /**< The private info provides a method of inserting
                                                                         port information into the parser result. This information
                                                                         may be extracted by Keygen and be used for frames
                                                                         distribution when a per-port distinction is required,
                                                                         it may also be used as a port logical id for analyzing
                                                                         incoming frames. */
    uint8_t                         parsingOffset;                  /**< Number of bytes from beginning of packet to start parsing */
    e_NetHeaderType                 firstPrsHdr;                    /**< The type of the first header expected at 'parsingOffset' */
    bool                            includeInPrsStatistics;         /**< TRUE to include this port in the parser statistics;
                                                                         NOTE: this field is not valid when the FN is in "guest" mode. */
    uint8_t                         numOfHdrsWithAdditionalParams;  /**< Normally 0, some headers may get
                                                                         special parameters */
    t_FmPcdPrsAdditionalHdrParams   additionalParams[FM_PCD_PRS_NUM_OF_HDRS];
                                                                    /**< 'numOfHdrsWithAdditionalParams'  structures
                                                                         of additional parameters
                                                                         for each header that requires them */
    bool                            setVlanTpid1;                   /**< TRUE to configure user selection of Ethertype to
                                                                         indicate a VLAN tag (in addition to the TPID values
                                                                         0x8100 and 0x88A8). */
    uint16_t                        vlanTpid1;                      /**< extra tag to use if setVlanTpid1=TRUE. */
    bool                            setVlanTpid2;                   /**< TRUE to configure user selection of Ethertype to
                                                                         indicate a VLAN tag (in addition to the TPID values
                                                                         0x8100 and 0x88A8). */
    uint16_t                        vlanTpid2;                      /**< extra tag to use if setVlanTpid1=TRUE. */
} t_FmPortPcdPrsParams;

/**************************************************************************//**
 @Description   struct for defining coarse alassification parameters
*//***************************************************************************/
typedef struct t_FmPortPcdCcParams {
    t_Handle            h_CcTree;                       /**< A handle to a CC tree */
} t_FmPortPcdCcParams;

/**************************************************************************//**
 @Description   struct for defining keygen parameters
*//***************************************************************************/
typedef struct t_FmPortPcdKgParams {
    uint8_t             numOfSchemes;                   /**< Number of schemes for port to be bound to. */
    t_Handle            h_Schemes[FM_PCD_KG_NUM_OF_SCHEMES];
                                                        /**< Array of 'numOfSchemes' schemes handles for the
                                                             port to be bound to */
    bool                directScheme;                   /**< TRUE for going from parser to a specific scheme,
                                                             regardless of parser result */
    t_Handle            h_DirectScheme;                 /**< relevant only if direct == TRUE, Scheme handle,
                                                             as returned by FM_PCD_KgSetScheme */
} t_FmPortPcdKgParams;

/**************************************************************************//**
 @Description   struct for defining policer parameters
*//***************************************************************************/
typedef struct t_FmPortPcdPlcrParams {
    t_Handle                h_Profile;          /**< Selected profile handle; Relevant for one of
                                                     following cases:
                                                     e_FM_PORT_PCD_SUPPORT_PLCR_ONLY or
                                                     e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR were selected,
                                                     or if any flow uses a KG scheme were policer
                                                     profile is not generated
                                                     (bypassPlcrProfileGeneration selected) */
} t_FmPortPcdPlcrParams;

/**************************************************************************//**
 @Description   struct for defining port PCD parameters
*//***************************************************************************/
typedef struct t_FmPortPcdParams {
    e_FmPortPcdSupport      pcdSupport;         /**< Relevant for Rx and offline ports only.
                                                     Describes the active PCD engines for this port. */
    t_Handle                h_NetEnv;           /**< HL Unused in PLCR only mode */
    t_FmPortPcdPrsParams    *p_PrsParams;       /**< Parser parameters for this port */
    t_FmPortPcdCcParams     *p_CcParams;        /**< Coarse classification parameters for this port */
    t_FmPortPcdKgParams     *p_KgParams;        /**< Keygen parameters for this port */
    t_FmPortPcdPlcrParams   *p_PlcrParams;      /**< Policer parameters for this port */
} t_FmPortPcdParams;

/**************************************************************************//**
 @Description   A structure for defining the Parser starting point
*//***************************************************************************/
typedef struct t_FmPcdPrsStart {
    uint8_t             parsingOffset;  /**< Number of bytes from beginning of packet to
                                             start parsing */
    e_NetHeaderType     firstPrsHdr;    /**< The type of the first header axpected at
                                             'parsingOffset' */
} t_FmPcdPrsStart;


/**************************************************************************//**
 @Function      FM_PORT_SetPCD

 @Description   Calling this routine defines the port's PCD configuration.
                It changes it from its default configuration which is PCD
                disabled (BMI to BMI) and configures it according to the passed
                parameters.

                May be used for Rx and offline parsing ports only

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     p_FmPortPcd     A Structure of parameters defining the port's PCD
                                configuration.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_SetPCD(t_Handle h_FmPort, t_FmPortPcdParams *p_FmPortPcd);

/**************************************************************************//**
 @Function      FM_PORT_DeletePCD

 @Description   Calling this routine releases the port's PCD configuration.
                The port returns to its default configuration which is PCD
                disabled (BMI to BMI) and all PCD configuration is removed.

                May be used for Rx and offline parsing ports which are
                in PCD mode  only

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_DeletePCD(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_AttachPCD

 @Description   This routine may be called after FM_PORT_DetachPCD was called,
                to return to the originally configured PCD support flow.
                The couple of routines are used to allow PCD configuration changes
                that demand that PCD will not be used while changes take place.

                May be used for Rx and offline parsing ports which are
                in PCD mode only

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
*//***************************************************************************/
t_Error FM_PORT_AttachPCD(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_DetachPCD

 @Description   Calling this routine detaches the port from its PCD functionality.
                The port returns to its default flow which is BMI to BMI.

                May be used for Rx and offline parsing ports which are
                in PCD mode only

 @Param[in]     h_FmPort        A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_AttachPCD().
*//***************************************************************************/
t_Error FM_PORT_DetachPCD(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_PcdPlcrAllocProfiles

 @Description   This routine may be called only for ports that use the Policer in
                order to allocate private policer profiles.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     numOfProfiles       The number of required policer profiles

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PCD_Init(), and before FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdPlcrAllocProfiles(t_Handle h_FmPort, uint16_t numOfProfiles);

/**************************************************************************//**
 @Function      FM_PORT_PcdPlcrFreeProfiles

 @Description   This routine should be called for freeing private policer profiles.

 @Param[in]     h_FmPort            A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PCD_Init(), and before FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdPlcrFreeProfiles(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_PcdKgModifyInitialScheme

 @Description   This routine may be called only for ports that use the keygen in
                order to change the initial scheme frame should be routed to.
                The change may be of a scheme id (in case of direct mode),
                from direct to indirect, or from indirect to direct - specifying the scheme id.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     p_FmPcdKgScheme     A structure of parameters for defining whether
                                    a scheme is direct/indirect, and if direct - scheme id.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdKgModifyInitialScheme (t_Handle h_FmPort, t_FmPcdKgSchemeSelect *p_FmPcdKgScheme);

/**************************************************************************//**
 @Function      FM_PORT_PcdPlcrModifyInitialProfile

 @Description   This routine may be called for ports with flows
                e_FM_PORT_PCD_SUPPORT_PLCR_ONLY or e_FM_PORT_PCD_SUPPORT_PRS_AND_PLCR
                only, to change the initial Policer profile frame should be
                routed to. The change may be of a profile and/or absolute/direct
                mode selection.

 @Param[in]     h_FmPort                A handle to a FM Port module.
 @Param[in]     h_Profile               Policer profile handle

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdPlcrModifyInitialProfile (t_Handle h_FmPort, t_Handle h_Profile);

/**************************************************************************//**
 @Function      FM_PORT_PcdCcModifyTree

 @Description   This routine may be called for ports that use coarse classification tree
                if the user wishes to replace the tree. The routine may not be called while port
                receives packets using the PCD functionalities, therefor port must be first detached
                from the PCD, only than the routine may be called, and than port be attached to PCD again.

 @Param[in]     h_FmPort            A handle to a FM Port module.
 @Param[in]     h_CcTree            A CC tree that was already built. The tree id as returned from
                                    the BuildTree routine.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init(), FM_PORT_SetPCD() and FM_PORT_DetachPCD()
*//***************************************************************************/
t_Error FM_PORT_PcdCcModifyTree (t_Handle h_FmPort, t_Handle h_CcTree);

/**************************************************************************//**
 @Function      FM_PORT_PcdKgBindSchemes

 @Description   These routines may be called for adding more schemes for the
                port to be bound to. The selected schemes are not added,
                just this specific port starts using them.

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     p_PortScheme    A structure defining the list of schemes to be added.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdKgBindSchemes (t_Handle h_FmPort, t_FmPcdPortSchemesParams *p_PortScheme);

/**************************************************************************//**
 @Function      FM_PORT_PcdKgUnbindSchemes

 @Description   These routines may be called for adding more schemes for the
                port to be bound to. The selected schemes are not removed or invalidated,
                just this specific port stops using them.

 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     p_PortScheme    A structure defining the list of schemes to be added.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init() and FM_PORT_SetPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdKgUnbindSchemes (t_Handle h_FmPort, t_FmPcdPortSchemesParams *p_PortScheme);

/**************************************************************************//**
 @Function      FM_PORT_PcdPrsModifyStartOffset

 @Description   Runtime change of the parser start offset within the header.
                The routine may not be called while port
                receives packets using the PCD functionalities, therefore port must be first detached
                from the PCD, only than the routine may be called, and than port be attached to PCD again.
 @Param[in]     h_FmPort        A handle to a FM Port module.
 @Param[in]     p_FmPcdPrsStart A structure of parameters for defining the
                                start point for the parser.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init(), FM_PORT_SetPCD() and FM_PORT_DetatchPCD().
*//***************************************************************************/
t_Error FM_PORT_PcdPrsModifyStartOffset (t_Handle h_FmPort, t_FmPcdPrsStart *p_FmPcdPrsStart);

/** @} */ /* end of FM_PORT_pcd_runtime_control_grp group */
/** @} */ /* end of FM_PORT_runtime_control_grp group */


/**************************************************************************//**
 @Group         FM_PORT_runtime_data_grp FM Port Runtime Data-path Unit

 @Description   FM Port Runtime data unit API functions, definitions and enums.
                This API is valid only if working in Independent-Mode.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Function      FM_PORT_ImTx

 @Description   Tx function, called to transmit a data buffer on the port.

 @Param[in]     h_FmPort    A handle to a FM Port module.
 @Param[in]     p_Data      A pointer to an LCP data buffer.
 @Param[in]     length      Size of data for transmission.
 @Param[in]     lastBuffer  Buffer position - TRUE for the last buffer
                            of a frame, including a single buffer frame
 @Param[in]     h_BufContext  A handle of the user acossiated with this buffer

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
                NOTE - This routine can be used only when working in
                Independent-Mode mode.
*//***************************************************************************/
t_Error  FM_PORT_ImTx( t_Handle               h_FmPort,
                       uint8_t                *p_Data,
                       uint16_t               length,
                       bool                   lastBuffer,
                       t_Handle               h_BufContext);

/**************************************************************************//**
 @Function      FM_PORT_ImTxConf

 @Description   Tx port confirmation routine, optional, may be called to verify
                transmission of all frames. The procedure performed by this
                routine will be performed automatically on next buffer transmission,
                but if desired, calling this routine will invoke this action on
                demand.

 @Param[in]     h_FmPort            A handle to a FM Port module.

 @Cautions      Allowed only following FM_PORT_Init().
                NOTE - This routine can be used only when working in
                Independent-Mode mode.
*//***************************************************************************/
void FM_PORT_ImTxConf(t_Handle h_FmPort);

/**************************************************************************//**
 @Function      FM_PORT_ImRx

 @Description   Rx function, may be called to poll for received buffers.
                Normally, Rx process is invoked by the driver on Rx interrupt.
                Alternatively, this routine may be called on demand.

 @Param[in]     h_FmPort            A handle to a FM Port module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PORT_Init().
                NOTE - This routine can be used only when working in
                Independent-Mode mode.
*//***************************************************************************/
t_Error  FM_PORT_ImRx(t_Handle h_FmPort);

/** @} */ /* end of FM_PORT_runtime_data_grp group */
/** @} */ /* end of FM_PORT_grp group */
/** @} */ /* end of FM_grp group */




#endif /* __FM_PORT_EXT */
