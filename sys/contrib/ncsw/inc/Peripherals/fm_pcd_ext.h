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
 @File          fm_pcd_ext.h

 @Description   FM PCD ...
*//***************************************************************************/
#ifndef __FM_PCD_EXT
#define __FM_PCD_EXT

#include "std_ext.h"
#include "net_ext.h"
#include "list_ext.h"
#include "fm_ext.h"


/**************************************************************************//**

 @Group         FM_grp Frame Manager API

 @Description   FM API functions, definitions and enums

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Group         FM_PCD_grp FM PCD

 @Description   FM PCD API functions, definitions and enums

                The FM PCD module is responsible for the initialization of all
                global classifying FM modules. This includes the parser general and
                common registers, the key generator global and common registers,
                and the Policer global and common registers.
                In addition, the FM PCD SW module will initialize all required
                key generator schemes, coarse classification flows, and Policer
                profiles. When An FM module is configured to work with one of these
                entities, it will register to it using the FM PORT API. The PCD
                module will manage the PCD resources - i.e. resource management of
                Keygen schemes, etc.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Collection    General PCD defines
*//***************************************************************************/
typedef uint32_t fmPcdEngines_t; /**< options as defined below: */

#define FM_PCD_NONE                                 0                   /**< No PCD Engine indicated */
#define FM_PCD_PRS                                  0x80000000          /**< Parser indicated */
#define FM_PCD_KG                                   0x40000000          /**< Keygen indicated */
#define FM_PCD_CC                                   0x20000000          /**< Coarse classification indicated */
#define FM_PCD_PLCR                                 0x10000000          /**< Policer indicated */
#define FM_PCD_MANIP                                0x08000000          /**< Manipulation indicated */

#define FM_PCD_MAX_NUM_OF_PRIVATE_HDRS              2                   /**< Number of units/headers saved for user */

#define FM_PCD_PRS_NUM_OF_HDRS                      16                  /**< Number of headers supported by HW parser */
#define FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS         (32 - FM_PCD_MAX_NUM_OF_PRIVATE_HDRS)
                                                                        /**< number of distinction units is limited by
                                                                             register size (32), - reserved bits for
                                                                             private headers. */

#define FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS      4                   /**< Maximum number of interchangeable headers in a distinction unit */
#define FM_PCD_KG_NUM_OF_GENERIC_REGS               8                   /**< Total number of generic KG registers */
#define FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY       35                  /**< Max number allowed on any configuration.
                                                                             For reason of HW implementation, in most
                                                                             cases less than this will be allowed. The
                                                                             driver will return error in initialization
                                                                             time if resource is overused. */
#define FM_PCD_KG_NUM_OF_EXTRACT_MASKS              4                   /**< Total number of masks allowed on KG extractions. */
#define FM_PCD_KG_NUM_OF_DEFAULT_GROUPS             16                  /**< Number of default value logical groups */

#define FM_PCD_PRS_NUM_OF_LABELS                    32                  /**< Max number of SW parser label */
#define FM_PCD_SW_PRS_SIZE                          0x00000800          /**< Total size of sw parser area */
#define FM_PCD_PRS_SW_OFFSET                        0x00000040          /**< Size of illegal addresses at the beginning
                                                                             of the SW parser area */
#define FM_PCD_PRS_SW_PATCHES_SIZE                  0x00000200          /**< Number of bytes saved for patches */
#define FM_PCD_PRS_SW_TAIL_SIZE                     4                   /**< Number of bytes that must be cleared at
                                                                             the end of the SW parser area */
#define FM_SW_PRS_MAX_IMAGE_SIZE                    (FM_PCD_SW_PRS_SIZE-FM_PCD_PRS_SW_OFFSET-FM_PCD_PRS_SW_TAIL_SIZE-FM_PCD_PRS_SW_PATCHES_SIZE)
                                                                        /**< Max possible size of SW parser code */

#define FM_PCD_MAX_MANIP_INSRT_TEMPLATE_SIZE        128                 /**< Max possible size of insertion template for
                                                                             insert manipulation*/
/* @} */


/**************************************************************************//**
 @Group         FM_PCD_init_grp FM PCD Initialization Unit

 @Description   FM PCD Initialization Unit

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   PCD counters
*//***************************************************************************/
typedef enum e_FmPcdCounters {
    e_FM_PCD_KG_COUNTERS_TOTAL,                                 /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_YELLOW,                              /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_RED,                                 /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_RECOLORED_TO_RED,                    /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_RECOLORED_TO_YELLOW,                 /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_TOTAL,                               /**< Policer counter */
    e_FM_PCD_PLCR_COUNTERS_LENGTH_MISMATCH,                     /**< Policer counter */
    e_FM_PCD_PRS_COUNTERS_PARSE_DISPATCH,                       /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L2_PARSE_RESULT_RETURNED,             /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L3_PARSE_RESULT_RETURNED,             /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L4_PARSE_RESULT_RETURNED,             /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_SHIM_PARSE_RESULT_RETURNED,           /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L2_PARSE_RESULT_RETURNED_WITH_ERR,    /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L3_PARSE_RESULT_RETURNED_WITH_ERR,    /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_L4_PARSE_RESULT_RETURNED_WITH_ERR,    /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_SHIM_PARSE_RESULT_RETURNED_WITH_ERR,  /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_SOFT_PRS_CYCLES,                      /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_SOFT_PRS_STALL_CYCLES,                /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_HARD_PRS_CYCLE_INCL_STALL_CYCLES,     /**< Parser counter */
    e_FM_PCD_PRS_COUNTERS_MURAM_READ_CYCLES,                    /**< MURAM counter */
    e_FM_PCD_PRS_COUNTERS_MURAM_READ_STALL_CYCLES,              /**< MURAM counter */
    e_FM_PCD_PRS_COUNTERS_MURAM_WRITE_CYCLES,                   /**< MURAM counter */
    e_FM_PCD_PRS_COUNTERS_MURAM_WRITE_STALL_CYCLES,             /**< MURAM counter */
    e_FM_PCD_PRS_COUNTERS_FPM_COMMAND_STALL_CYCLES              /**< FPM counter */
} e_FmPcdCounters;

/**************************************************************************//**
 @Description   PCD interrupts
*//***************************************************************************/
typedef enum e_FmPcdExceptions {
    e_FM_PCD_KG_EXCEPTION_DOUBLE_ECC,                   /**< Keygen ECC error */
    e_FM_PCD_PLCR_EXCEPTION_DOUBLE_ECC,                 /**< Read Buffer ECC error */
    e_FM_PCD_KG_EXCEPTION_KEYSIZE_OVERFLOW,             /**< Write Buffer ECC error on system side */
    e_FM_PCD_PLCR_EXCEPTION_INIT_ENTRY_ERROR,           /**< Write Buffer ECC error on FM side */
    e_FM_PCD_PLCR_EXCEPTION_PRAM_SELF_INIT_COMPLETE,    /**< Self init complete */
    e_FM_PCD_PLCR_EXCEPTION_ATOMIC_ACTION_COMPLETE,     /**< Atomic action complete */
    e_FM_PCD_PRS_EXCEPTION_DOUBLE_ECC,                  /**< Parser ECC error */
    e_FM_PCD_PRS_EXCEPTION_SINGLE_ECC                   /**< Parser single ECC */
} e_FmPcdExceptions;


/**************************************************************************//**
 @Description   Exceptions user callback routine, will be called upon an
                exception passing the exception identification.

 @Param[in]     h_App      - User's application descriptor.
 @Param[in]     exception  - The exception.
  *//***************************************************************************/
typedef void (t_FmPcdExceptionCallback) (t_Handle h_App, e_FmPcdExceptions exception);

/**************************************************************************//**
 @Description   Exceptions user callback routine, will be called upon an exception
                passing the exception identification.

 @Param[in]     h_App           - User's application descriptor.
 @Param[in]     exception       - The exception.
 @Param[in]     index           - id of the relevant source (may be scheme or profile id).
 *//***************************************************************************/
typedef void (t_FmPcdIdExceptionCallback) ( t_Handle           h_App,
                                            e_FmPcdExceptions  exception,
                                            uint16_t           index);

/**************************************************************************//**
 @Description   A callback for enqueuing frame onto a QM queue.

 @Param[in]     h_App           - User's application descriptor.
 @Param[in]     p_Fd            - Frame descriptor for the frame.

 @Return        E_OK on success; Error code otherwise.
 *//***************************************************************************/
typedef t_Error (t_FmPcdQmEnqueueCallback) (t_Handle h_QmArg, void *p_Fd);

/**************************************************************************//**
 @Description   A structure for Host-Command
                When using Host command for PCD functionalities, a dedicated port
                must be used. If this routine is called for a PCD in a single partition
                environment, or it is the Master partition in a Multi partition
                environment, The port will be initialized by the PCD driver
                initialization routine.
 *//***************************************************************************/
typedef struct t_FmPcdHcParams {
    uintptr_t                   portBaseAddr;       /**< Host-Command Port Virtual Address of
                                                         memory mapped registers.*/
    uint8_t                     portId;             /**< Host-Command Port Id (0-6 relative
                                                         to Host-Command/Offline parsing ports) */
    uint16_t                    liodnBase;          /**< Irrelevant for P4080 rev 1. LIODN base for this port, to be
                                                         used together with LIODN offset. */
    uint32_t                    errFqid;            /**< Host-Command Port Error Queue Id. */
    uint32_t                    confFqid;           /**< Host-Command Port Confirmation queue Id. */
    uint32_t                    qmChannel;          /**< Host-Command port - QM-channel dedicated to
                                                         this port will be used by the FM for dequeue. */
    t_FmPcdQmEnqueueCallback    *f_QmEnqueue;       /**< Call back routine for enqueuing a frame to the QM */
    t_Handle                    h_QmArg;            /**< A handle of the QM module */
} t_FmPcdHcParams;

/**************************************************************************//**
 @Description   The main structure for PCD initialization
 *//***************************************************************************/
typedef struct t_FmPcdParams {
    bool                        prsSupport;             /**< TRUE if Parser will be used for any
                                                             of the FM ports */
    bool                        ccSupport;              /**< TRUE if Coarse Classification will be used for any
                                                             of the FM ports */
    bool                        kgSupport;              /**< TRUE if Keygen will be used for any
                                                             of the FM ports */
    bool                        plcrSupport;            /**< TRUE if Policer will be used for any
                                                             of the FM ports */
    t_Handle                    h_Fm;                   /**< A handle to the FM module */
    uint8_t                     numOfSchemes;           /**< Number of schemes dedicated to this partition. */
    bool                        useHostCommand;         /**< Optional for single partition, Mandatory for Multi partition */
    t_FmPcdHcParams             hc;                     /**< Relevant only if useHostCommand=TRUE.
                                                             Host Command parameters. */

    t_FmPcdExceptionCallback    *f_Exception;           /**< Relevant for master (or single) partition only: Callback routine
                                                             to be called of PCD exception */
    t_FmPcdIdExceptionCallback  *f_ExceptionId;         /**< Relevant for master (or single) partition only: Callback routine
                                                             to be used for a single scheme and
                                                             profile exceptions */
    t_Handle                    h_App;                  /**< Relevant for master (or single) partition only: A handle to an
                                                             application layer object; This handle will
                                                             be passed by the driver upon calling the above callbacks */
} t_FmPcdParams;


/**************************************************************************//**
 @Function      FM_PCD_Config

 @Description   Basic configuration of the PCD module.
                Creates descriptor for the FM PCD module.

 @Param[in]     p_FmPcdParams    A structure of parameters for the initialization of PCD.

 @Return        A handle to the initialized module.
*//***************************************************************************/
t_Handle FM_PCD_Config(t_FmPcdParams *p_FmPcdParams);

/**************************************************************************//**
 @Function      FM_PCD_Init

 @Description   Initialization of the PCD module.

 @Param[in]     h_FmPcd - FM PCD module descriptor.

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PCD_Init(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_Free

 @Description   Frees all resources that were assigned to FM module.

                Calling this routine invalidates the descriptor.

 @Param[in]     h_FmPcd - FM PCD module descriptor.

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PCD_Free(t_Handle h_FmPcd);

/**************************************************************************//**
 @Group         FM_PCD_advanced_init_grp    FM PCD Advanced Configuration Unit

 @Description   Configuration functions used to change default values.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Function      FM_PCD_ConfigPlcrNumOfSharedProfiles

 @Description   Calling this routine changes the internal driver data base
                from its default selection of exceptions enablement.
                [4].

 @Param[in]     h_FmPcd                     FM PCD module descriptor.
 @Param[in]     numOfSharedPlcrProfiles     Number of profiles to
                                            be shared between ports on this partition

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PCD_ConfigPlcrNumOfSharedProfiles(t_Handle h_FmPcd, uint16_t numOfSharedPlcrProfiles);

/**************************************************************************//**
 @Function      FM_PCD_ConfigException

 @Description   Calling this routine changes the internal driver data base
                from its default selection of exceptions enablement.
                By default all exceptions are enabled.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     exception       The exception to be selected.
 @Param[in]     enable          TRUE to enable interrupt, FALSE to mask it.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_ConfigException(t_Handle h_FmPcd, e_FmPcdExceptions exception, bool enable);

/**************************************************************************//**
 @Function      FM_PCD_ConfigPlcrAutoRefreshMode

 @Description   Calling this routine changes the internal driver data base
                from its default selection of exceptions enablement.
                By default autorefresh is enabled.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     enable          TRUE to enable, FALSE to disable

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_ConfigPlcrAutoRefreshMode(t_Handle h_FmPcd, bool enable);

/**************************************************************************//**
 @Function      FM_PCD_ConfigPrsMaxCycleLimit

 @Description   Calling this routine changes the internal data structure for
                the maximum parsing time from its default value
                [0].

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     value           0 to disable the mechanism, or new
                                maximum parsing time.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_ConfigPrsMaxCycleLimit(t_Handle h_FmPcd,uint16_t value);

/** @} */ /* end of FM_PCD_advanced_init_grp group */
/** @} */ /* end of FM_PCD_init_grp group */


/**************************************************************************//**
 @Group         FM_PCD_Runtime_grp FM PCD Runtime Unit

 @Description   FM PCD Runtime Unit

                The runtime control allows creation of PCD infrastructure modules
                such as Network Environment Characteristics, Classification Plan
                Groups and Coarse Classification Trees.
                It also allows on-the-fly initialization, modification and removal
                of PCD modules such as Keygen schemes, coarse classification nodes
                and Policer profiles.


                In order to explain the programming model of the PCD driver interface
                a few terms should be explained, and will be used below.
                  * Distinction Header - One of the 16 protocols supported by the FM parser,
                    or one of the shim headers (1 or 2). May be a header with a special
                    option (see below).
                  * Interchangeable Headers Group- This is a group of Headers recognized
                    by either one of them. For example, if in a specific context the user
                    chooses to treat IPv4 and IPV6 in the same way, they may create an
                    interchangeable Headers Unit consisting of these 2 headers.
                  * A Distinction Unit - a Distinction Header or an Interchangeable Headers
                    Group.
                  * Header with special option - applies to ethernet, mpls, vlan, ipv4 and
                    ipv6, includes multicast, broadcast and other protocol specific options.
                    In terms of hardware it relates to the options available in the classification
                    plan.
                  * Network Environment Characteristics - a set of Distinction Units that define
                    the total recognizable header selection for a certain environment. This is
                    NOT the list of all headers that will ever appear in a flow, but rather
                    everything that needs distinction in a flow, where distinction is made by keygen
                    schemes and coarse classification action descriptors.

                The PCD runtime modules initialization is done in stages. The first stage after
                initializing the PCD module itself is to establish a Network Flows Environment
                Definition. The application may choose to establish one or more such environments.
                Later, when needed, the application will have to state, for some of its modules,
                to which single environment it belongs.

 @{
*//***************************************************************************/

/**************************************************************************//**
 @Description   A structure for sw parser labels
 *//***************************************************************************/
typedef struct t_FmPcdPrsLabelParams {
    uint32_t                instructionOffset;              /**< SW parser label instruction offset (2 bytes
                                                                 resolution), relative to Parser RAM. */
    e_NetHeaderType         hdr;                            /**< The existance of this header will envoke
                                                                 the sw parser code. */
    uint8_t                 indexPerHdr;                    /**< Normally 0, if more than one sw parser
                                                                 attachments for the same header, use this
                                                                 index to distinguish between them. */
} t_FmPcdPrsLabelParams;

/**************************************************************************//**
 @Description   A structure for sw parser
 *//***************************************************************************/
typedef struct t_FmPcdPrsSwParams {
    bool                    override;                   /**< FALSE to invoke a check that nothing else
                                                             was loaded to this address, including
                                                             internal patches.
                                                             TRUE to override any existing code.*/
    uint32_t                size;                       /**< SW parser code size */
    uint16_t                base;                       /**< SW parser base (in instruction counts!
                                                             must be larger than 0x20)*/
    uint8_t                 *p_Code;                    /**< SW parser code */
    uint32_t                swPrsDataParams[FM_PCD_PRS_NUM_OF_HDRS];
                                                        /**< SW parser data (parameters) */
    uint8_t                 numOfLabels;                /**< Number of labels for SW parser. */
    t_FmPcdPrsLabelParams   labelsTable[FM_PCD_PRS_NUM_OF_LABELS];
                                                        /**< SW parser labels table, containing
                                                             numOfLabels entries */
} t_FmPcdPrsSwParams;


/**************************************************************************//**
 @Function      FM_PCD_Enable

 @Description   This routine should be called after PCD is initialized for enabling all
                PCD engines according to their existing configuration.

 @Param[in]     h_FmPcd         FM PCD module descriptor.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init() and when PCD is disabled.
*//***************************************************************************/
t_Error FM_PCD_Enable(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_Disable

 @Description   This routine may be called when PCD is enabled in order to
                disable all PCD engines. It may be called
                only when none of the ports in the system are using the PCD.

 @Param[in]     h_FmPcd         FM PCD module descriptor.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init() and when PCD is enabled.
*//***************************************************************************/
t_Error FM_PCD_Disable(t_Handle h_FmPcd);


/**************************************************************************//**
 @Function      FM_PCD_GetCounter

 @Description   Reads one of the FM PCD counters.

 @Param[in]     h_FmPcd     FM PCD module descriptor.
 @Param[in]     counter     The requested counter.

 @Return        Counter's current value.

 @Cautions      Allowed only following FM_PCD_Init().
                Note that it is user's responsibility to call this routine only
                for enabled counters, and there will be no indication if a
                disabled counter is accessed.
*//***************************************************************************/
uint32_t FM_PCD_GetCounter(t_Handle h_FmPcd, e_FmPcdCounters counter);

/**************************************************************************//**
@Function      FM_PCD_PrsLoadSw

@Description   This routine may be called in order to load software parsing code.


@Param[in]     h_FmPcd         FM PCD module descriptor.
@Param[in]     p_SwPrs         A pointer to a structure of software
                               parser parameters, including the software
                               parser image.

@Return        E_OK on success; Error code otherwise.

@Cautions      Allowed only following FM_PCD_Init() and when PCD is disabled.
               Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_PrsLoadSw(t_Handle h_FmPcd, t_FmPcdPrsSwParams *p_SwPrs);

/**************************************************************************//**
 @Function      FM_PCD_KgSetDfltValue

 @Description   Calling this routine sets a global default value to be used
                by the keygen when parser does not recognize a required
                field/header.
                By default default values are 0.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     valueId         0,1 - one of 2 global default values.
 @Param[in]     value           The requested default value.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init() and when PCD is disabled.
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_KgSetDfltValue(t_Handle h_FmPcd, uint8_t valueId, uint32_t value);

/**************************************************************************//**
 @Function      FM_PCD_KgSetAdditionalDataAfterParsing

 @Description   Calling this routine allows the keygen to access data past
                the parser finishing point.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     payloadOffset   the number of bytes beyond the parser location.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init() and when PCD is disabled.
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_KgSetAdditionalDataAfterParsing(t_Handle h_FmPcd, uint8_t payloadOffset);

/**************************************************************************//**
 @Function      FM_PCD_SetException

 @Description   Calling this routine enables/disables PCD interrupts.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     exception       The exception to be selected.
 @Param[in]     enable          TRUE to enable interrupt, FALSE to mask it.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_SetException(t_Handle h_FmPcd, e_FmPcdExceptions exception, bool enable);

/**************************************************************************//**
 @Function      FM_PCD_ModifyCounter

 @Description   Sets a value to an enabled counter. Use "0" to reset the counter.

 @Param[in]     h_FmPcd     FM PCD module descriptor.
 @Param[in]     counter     The requested counter.
 @Param[in]     value       The requested value to be written into the counter.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_ModifyCounter(t_Handle h_FmPcd, e_FmPcdCounters counter, uint32_t value);

/**************************************************************************//**
 @Function      FM_PCD_SetPlcrStatistics

 @Description   This routine may be used to enable/disable policer statistics
                counter. By default the statistics is enabled.

 @Param[in]     h_FmPcd         FM PCD module descriptor
 @Param[in]     enable          TRUE to enable, FALSE to disable.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_SetPlcrStatistics(t_Handle h_FmPcd, bool enable);

/**************************************************************************//**
 @Function      FM_PCD_SetPrsStatistics

 @Description   Defines whether to gather parser statistics including all ports.

 @Param[in]     h_FmPcd     FM PCD module descriptor.
 @Param[in]     enable      TRUE to enable, FALSE to disable.

 @Return        None

 @Cautions      Allowed only following FM_PCD_Init().
                Not available for guest partition.
*//***************************************************************************/
void FM_PCD_SetPrsStatistics(t_Handle h_FmPcd, bool enable);

/**************************************************************************//**
 @Function      FM_PCD_ForceIntr

 @Description   Causes an interrupt event on the requested source.

 @Param[in]     h_FmPcd     FM PCD module descriptor.
 @Param[in]     exception       An exception to be forced.

 @Return        E_OK on success; Error code if the exception is not enabled,
                or is not able to create interrupt.

 @Cautions      Allowed only following FM_PCD_Init().
                Not available for guest partition.
*//***************************************************************************/
t_Error FM_PCD_ForceIntr (t_Handle h_FmPcd, e_FmPcdExceptions exception);

/**************************************************************************//**
 @Function      FM_PCD_HcTxConf

 @Description   This routine should be called to confirm frames that were
                 received on the HC confirmation queue.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     p_Fd            Frame descriptor of the received frame.

 @Cautions      Allowed only following FM_PCD_Init(). Allowed only if 'useHostCommand'
                option was selected in the initialization.
*//***************************************************************************/
void FM_PCD_HcTxConf(t_Handle h_FmPcd, t_DpaaFD *p_Fd);

#if (defined(DEBUG_ERRORS) && (DEBUG_ERRORS > 0))
/**************************************************************************//**
 @Function      FM_PCD_DumpRegs

 @Description   Dumps all PCD registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_DumpRegs(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_KgDumpRegs

 @Description   Dumps all PCD KG registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_KgDumpRegs(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_PlcrDumpRegs

 @Description   Dumps all PCD Plcr registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_PlcrDumpRegs(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_PlcrProfileDumpRegs

 @Description   Dumps all PCD Plcr registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_Profile       A handle to a profile.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_PlcrProfileDumpRegs(t_Handle h_FmPcd, t_Handle h_Profile);

/**************************************************************************//**
 @Function      FM_PCD_PrsDumpRegs

 @Description   Dumps all PCD Prs registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_PrsDumpRegs(t_Handle h_FmPcd);

/**************************************************************************//**
 @Function      FM_PCD_HcDumpRegs

 @Description   Dumps HC Port registers

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error     FM_PCD_HcDumpRegs(t_Handle h_FmPcd);
#endif /* (defined(DEBUG_ERRORS) && ... */



/**************************************************************************//**
 @Group         FM_PCD_Runtime_tree_buildgrp FM PCD Tree building Unit

 @Description   FM PCD Runtime Unit

                This group contains routines for setting, deleting and modifying
                PCD resources, for defining the total PCD tree.
 @{
*//***************************************************************************/

/**************************************************************************//**
 @Collection    Definitions of coarse classification
                parameters as required by keygen (when coarse classification
                is the next engine after this scheme).
*//***************************************************************************/
#define FM_PCD_MAX_NUM_OF_CC_NODES          255
#define FM_PCD_MAX_NUM_OF_CC_TREES          8
#define FM_PCD_MAX_NUM_OF_CC_GROUPS         16
#define FM_PCD_MAX_NUM_OF_CC_UNITS          4
#define FM_PCD_MAX_NUM_OF_KEYS              256
#define FM_PCD_MAX_SIZE_OF_KEY              56
#define FM_PCD_MAX_NUM_OF_CC_ENTRIES_IN_GRP 16
/* @} */

/**************************************************************************//**
 @Collection    A set of definitions to allow protocol
                special option description.
*//***************************************************************************/
typedef uint32_t        protocolOpt_t;          /**< A general type to define a protocol option. */

typedef protocolOpt_t   ethProtocolOpt_t;       /**< Ethernet protocol options. */
#define ETH_BROADCAST               0x80000000  /**< Ethernet Broadcast. */
#define ETH_MULTICAST               0x40000000  /**< Ethernet Multicast. */

typedef protocolOpt_t   vlanProtocolOpt_t;      /**< Vlan protocol options. */
#define VLAN_STACKED                0x20000000  /**< Vlan Stacked. */

typedef protocolOpt_t   mplsProtocolOpt_t;      /**< MPLS protocol options. */
#define MPLS_STACKED                0x10000000  /**< MPLS Stacked. */

typedef protocolOpt_t   ipv4ProtocolOpt_t;      /**< IPv4 protocol options. */
#define IPV4_BROADCAST_1            0x08000000  /**< IPv4 Broadcast. */
#define IPV4_MULTICAST_1            0x04000000  /**< IPv4 Multicast. */
#define IPV4_UNICAST_2              0x02000000  /**< Tunneled IPv4 - Unicast. */
#define IPV4_MULTICAST_BROADCAST_2  0x01000000  /**< Tunneled IPv4 - Broadcast/Multicast. */

typedef protocolOpt_t   ipv6ProtocolOpt_t;      /**< IPv6 protocol options. */
#define IPV6_MULTICAST_1            0x00800000  /**< IPv6 Multicast. */
#define IPV6_UNICAST_2              0x00400000  /**< Tunneled IPv6 - Unicast. */
#define IPV6_MULTICAST_2            0x00200000  /**< Tunneled IPv6 - Multicast. */
/* @} */

/**************************************************************************//**
 @Description   A type used for returning the order of the key extraction.
                each value in this array represents the index of the extraction
                command as defined by the user in the initialization extraction array.
                The valid size of this array is the user define number of extractions
                required (also marked by the second '0' in this array).
*//***************************************************************************/
typedef    uint8_t    t_FmPcdKgKeyOrder [FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY];

/**************************************************************************//**
 @Description   All PCD engines
*//***************************************************************************/
typedef enum e_FmPcdEngine {
    e_FM_PCD_INVALID = 0,   /**< Invalid PCD engine indicated*/
    e_FM_PCD_DONE,          /**< No PCD Engine indicated */
    e_FM_PCD_KG,            /**< Keygen indicated */
    e_FM_PCD_CC,            /**< Coarse classification indicated */
    e_FM_PCD_PLCR,          /**< Policer indicated */
    e_FM_PCD_PRS            /**< Parser indicated */
} e_FmPcdEngine;

/**************************************************************************//**
 @Description   An enum for selecting extraction by header types
*//***************************************************************************/
typedef enum e_FmPcdExtractByHdrType {
    e_FM_PCD_EXTRACT_FROM_HDR,      /**< Extract bytes from header */
    e_FM_PCD_EXTRACT_FROM_FIELD,    /**< Extract bytes from header field */
    e_FM_PCD_EXTRACT_FULL_FIELD     /**< Extract a full field */
} e_FmPcdExtractByHdrType;

/**************************************************************************//**
 @Description   An enum for selecting extraction source
                (when it is not the header)
*//***************************************************************************/
typedef enum e_FmPcdExtractFrom {
    e_FM_PCD_EXTRACT_FROM_FRAME_START,          /**< KG & CC: Extract from beginning of frame */
    e_FM_PCD_EXTRACT_FROM_DFLT_VALUE,           /**< KG only: Extract from a default value */
    e_FM_PCD_EXTRACT_FROM_CURR_END_OF_PARSE,    /**< KG only: Extract from the point where parsing had finished */
    e_FM_PCD_EXTRACT_FROM_KEY,                  /**< CC only: Field where saved KEY */
    e_FM_PCD_EXTRACT_FROM_HASH,                 /**< CC only: Field where saved HASH */
    e_FM_PCD_EXTRACT_FROM_PARSE_RESULT,         /**< KG & CC: Extract from the parser result */
    e_FM_PCD_EXTRACT_FROM_ENQ_FQID,             /**< KG & CC: Extract from enqueue FQID */
    e_FM_PCD_EXTRACT_FROM_FLOW_ID               /**< CC only: Field where saved Dequeue FQID */
} e_FmPcdExtractFrom;

/**************************************************************************//**
 @Description   An enum for selecting extraction type
*//***************************************************************************/
typedef enum e_FmPcdExtractType {
    e_FM_PCD_EXTRACT_BY_HDR,                /**< Extract according to header */
    e_FM_PCD_EXTRACT_NON_HDR,               /**< Extract from data that is not the header */
    e_FM_PCD_KG_EXTRACT_PORT_PRIVATE_INFO   /**< Extract private info as specified by user */
} e_FmPcdExtractType;

/**************************************************************************//**
 @Description   An enum for selecting a default
*//***************************************************************************/
typedef enum e_FmPcdKgExtractDfltSelect {
    e_FM_PCD_KG_DFLT_GBL_0,          /**< Default selection is KG register 0 */
    e_FM_PCD_KG_DFLT_GBL_1,          /**< Default selection is KG register 1 */
    e_FM_PCD_KG_DFLT_PRIVATE_0,      /**< Default selection is a per scheme register 0 */
    e_FM_PCD_KG_DFLT_PRIVATE_1,      /**< Default selection is a per scheme register 1 */
    e_FM_PCD_KG_DFLT_ILLEGAL         /**< Illegal selection */
} e_FmPcdKgExtractDfltSelect;

/**************************************************************************//**
 @Description   An enum defining all default groups -
                each group shares a default value, one of 4 user
                initialized values.
*//***************************************************************************/
typedef enum e_FmPcdKgKnownFieldsDfltTypes {
    e_FM_PCD_KG_MAC_ADDR,               /**< MAC Address */
    e_FM_PCD_KG_TCI,                    /**< TCI field */
    e_FM_PCD_KG_ENET_TYPE,              /**< ENET Type */
    e_FM_PCD_KG_PPP_SESSION_ID,         /**< PPP Session id */
    e_FM_PCD_KG_PPP_PROTOCOL_ID,        /**< PPP Protocol id */
    e_FM_PCD_KG_MPLS_LABEL,             /**< MPLS label */
    e_FM_PCD_KG_IP_ADDR,                /**< IP addr */
    e_FM_PCD_KG_PROTOCOL_TYPE,          /**< Protocol type */
    e_FM_PCD_KG_IP_TOS_TC,              /**< TOS or TC */
    e_FM_PCD_KG_IPV6_FLOW_LABEL,        /**< IPV6 flow label */
    e_FM_PCD_KG_IPSEC_SPI,              /**< IPSEC SPI */
    e_FM_PCD_KG_L4_PORT,                /**< L4 Port */
    e_FM_PCD_KG_TCP_FLAG,               /**< TCP Flag */
    e_FM_PCD_KG_GENERIC_FROM_DATA,      /**< grouping implemented by sw,
                                             any data extraction that is not the full
                                             field described above  */
    e_FM_PCD_KG_GENERIC_FROM_DATA_NO_V, /**< grouping implemented by sw,
                                             any data extraction without validation */
    e_FM_PCD_KG_GENERIC_NOT_FROM_DATA   /**< grouping implemented by sw,
                                             extraction from parser result or
                                             direct use of default value  */
} e_FmPcdKgKnownFieldsDfltTypes;

/**************************************************************************//**
 @Description   enum for defining header index when headers may repeat
*//***************************************************************************/
typedef enum e_FmPcdHdrIndex {
    e_FM_PCD_HDR_INDEX_NONE = 0,        /**< used when multiple headers not used, also
                                             to specify regular IP (not tunneled). */
    e_FM_PCD_HDR_INDEX_1,               /**< may be used for VLAN, MPLS, tunneled IP */
    e_FM_PCD_HDR_INDEX_2,               /**< may be used for MPLS, tunneled IP */
    e_FM_PCD_HDR_INDEX_3,               /**< may be used for MPLS */
    e_FM_PCD_HDR_INDEX_LAST = 0xFF      /**< may be used for VLAN, MPLS */
} e_FmPcdHdrIndex;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile functional type
*//***************************************************************************/
typedef enum e_FmPcdProfileTypeSelection {
    e_FM_PCD_PLCR_PORT_PRIVATE,         /**< Port dedicated profile */
    e_FM_PCD_PLCR_SHARED                /**< Shared profile (shared within partition) */
} e_FmPcdProfileTypeSelection;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile algorithem
*//***************************************************************************/
typedef enum e_FmPcdPlcrAlgorithmSelection {
    e_FM_PCD_PLCR_PASS_THROUGH,         /**< Policer pass through */
    e_FM_PCD_PLCR_RFC_2698,             /**< Policer algorythm RFC 2698 */
    e_FM_PCD_PLCR_RFC_4115              /**< Policer algorythm RFC 4115 */
} e_FmPcdPlcrAlgorithmSelection;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile color mode
*//***************************************************************************/
typedef enum e_FmPcdPlcrColorMode {
    e_FM_PCD_PLCR_COLOR_BLIND,          /**< Color blind */
    e_FM_PCD_PLCR_COLOR_AWARE           /**< Color aware */
} e_FmPcdPlcrColorMode;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile color functional mode
*//***************************************************************************/
typedef enum e_FmPcdPlcrColor {
    e_FM_PCD_PLCR_GREEN,                /**< Green */
    e_FM_PCD_PLCR_YELLOW,               /**< Yellow */
    e_FM_PCD_PLCR_RED,                  /**< Red */
    e_FM_PCD_PLCR_OVERRIDE              /**< Color override */
} e_FmPcdPlcrColor;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile packet frame length selector
*//***************************************************************************/
typedef enum e_FmPcdPlcrFrameLengthSelect {
  e_FM_PCD_PLCR_L2_FRM_LEN,             /**< L2 frame length */
  e_FM_PCD_PLCR_L3_FRM_LEN,             /**< L3 frame length */
  e_FM_PCD_PLCR_L4_FRM_LEN,             /**< L4 frame length */
  e_FM_PCD_PLCR_FULL_FRM_LEN            /**< Full frame length */
} e_FmPcdPlcrFrameLengthSelect;

/**************************************************************************//**
 @Description   An enum for selecting rollback frame
*//***************************************************************************/
typedef enum e_FmPcdPlcrRollBackFrameSelect {
  e_FM_PCD_PLCR_ROLLBACK_L2_FRM_LEN,    /**< Rollback L2 frame length */
  e_FM_PCD_PLCR_ROLLBACK_FULL_FRM_LEN   /**< Rollback Full frame length */
} e_FmPcdPlcrRollBackFrameSelect;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile packet or byte mode
*//***************************************************************************/
typedef enum e_FmPcdPlcrRateMode {
    e_FM_PCD_PLCR_BYTE_MODE,            /**< Byte mode */
    e_FM_PCD_PLCR_PACKET_MODE           /**< Packet mode */
} e_FmPcdPlcrRateMode;

/**************************************************************************//**
 @Description   An enum for defining action of frame
*//***************************************************************************/
typedef enum e_FmPcdDoneAction {
    e_FM_PCD_ENQ_FRAME = 0,        /**< Enqueue frame */
    e_FM_PCD_DROP_FRAME            /**< Drop frame */
} e_FmPcdDoneAction;

/**************************************************************************//**
 @Description   A structure for selecting the policer counter
*//***************************************************************************/
typedef enum e_FmPcdPlcrProfileCounters {
    e_FM_PCD_PLCR_PROFILE_GREEN_PACKET_TOTAL_COUNTER,               /**< Green packets counter */
    e_FM_PCD_PLCR_PROFILE_YELLOW_PACKET_TOTAL_COUNTER,              /**< Yellow packets counter */
    e_FM_PCD_PLCR_PROFILE_RED_PACKET_TOTAL_COUNTER,                 /**< Red packets counter */
    e_FM_PCD_PLCR_PROFILE_RECOLOURED_YELLOW_PACKET_TOTAL_COUNTER,   /**< Recolored yellow packets counter */
    e_FM_PCD_PLCR_PROFILE_RECOLOURED_RED_PACKET_TOTAL_COUNTER       /**< Recolored red packets counter */
} e_FmPcdPlcrProfileCounters;

/**************************************************************************//**
 @Description   A structure for selecting action
*//***************************************************************************/
typedef enum e_FmPcdAction {
    e_FM_PCD_ACTION_NONE,                           /**< NONE  */
    e_FM_PCD_ACTION_EXACT_MATCH,                    /**< Exact match on the selected extraction*/
    e_FM_PCD_ACTION_INDEXED_LOOKUP                  /**< Indexed lookup on the selected extraction*/
} e_FmPcdAction;

#if defined(FM_CAPWAP_SUPPORT)
/**************************************************************************//**
 @Description   An enum for selecting type of insert manipulation
*//***************************************************************************/
typedef enum e_FmPcdManipInsrtType {
    e_FM_PCD_MANIP_INSRT_NONE = 0,                          /**< No insertion */
    e_FM_PCD_MANIP_INSRT_TO_START_OF_FRAME_INT_FRAME_HDR,   /**< Insert internal frame header to start of frame */
    e_FM_PCD_MANIP_INSRT_TO_START_OF_FRAME_TEMPLATE         /**< Insert template to start of frame*/
} e_FmPcdManipInsrtType;

/**************************************************************************//**
 @Description   An enum for selecting type of remove manipulation
*//***************************************************************************/
typedef enum e_FmPcdManipRmvParamsType {
    e_FM_PCD_MANIP_RMV_NONE = 0,                                        /**< No remove */
    e_FM_PCD_MANIP_RMV_FROM_START_OF_FRAME_TILL_SPECIFIC_LOCATION,      /**< Remove from start of frame till (excluding) specified indication */
    e_FM_PCD_MANIP_RMV_FROM_START_OF_FRAME_INCLUDE_SPECIFIC_LOCATION,   /**< Remove from start of frame till (including) specified indication */
    e_FM_PCD_MANIP_RMV_INT_FRAME_HDR                                    /**< Remove internal frame header to start of frame */
} e_FmPcdManipRmvParamsType;

/**************************************************************************//**
 @Description   An enum for selecting type of location
*//***************************************************************************/
typedef enum e_FmPcdManipLocateType {
    e_FM_PCD_MANIP_LOC_BY_HDR = 0,            /**< Locate according to header */
    e_FM_PCD_MANIP_LOC_NON_HDR                /**< Locate from data that is not the header */
} e_FmPcdManipLocateType;

/**************************************************************************//**
 @Description   An enum for selecting type of Timeout mode
*//***************************************************************************/
typedef enum e_FmPcdManipReassemTimeOutMode {
    e_FM_PCD_MANIP_TIME_OUT_BETWEEN_FRAMES, /**< limits the time of the reassm process from the first frag to the last */
    e_FM_PCD_MANIP_TIME_OUT_BETWEEN_FRAG    /**< limits the time of receiving the fragment */
} e_FmPcdManipReassemTimeOutMode;

/**************************************************************************//**
 @Description   An enum for selecting type of WaysNumber mode
*//***************************************************************************/
typedef enum e_FmPcdManipReassemWaysNumber {
    e_FM_PCD_MANIP_ONE_WAY_HASH = 1,    /**< -------------- */
    e_FM_PCD_MANIP_TWO_WAYS_HASH,       /**< -------------- */
    e_FM_PCD_MANIP_THREE_WAYS_HASH,     /**< -------------- */
    e_FM_PCD_MANIP_FOUR_WAYS_HASH,      /**< four ways hash */
    e_FM_PCD_MANIP_FIVE_WAYS_HASH,      /**< -------------- */
    e_FM_PCD_MANIP_SIX_WAYS_HASH,       /**< -------------- */
    e_FM_PCD_MANIP_SEVEN_WAYS_HASH,     /**< -------------- */
    e_FM_PCD_MANIP_EIGHT_WAYS_HASH      /**< eight ways hash*/
} e_FmPcdManipReassemWaysNumber;

/**************************************************************************//**
 @Description   An enum for selecting type of statistics mode
*//***************************************************************************/
typedef enum e_FmPcdStatsType {
    e_FM_PCD_STATS_PER_FLOWID = 0   /**< type where flowId used as index for getting statistics */
} e_FmPcdStatsType;

#endif /* FM_CAPWAP_SUPPORT */


/**************************************************************************//**
 @Description   A Union of protocol dependent special options
*//***************************************************************************/
typedef union u_FmPcdHdrProtocolOpt {
    ethProtocolOpt_t    ethOpt;     /**< Ethernet options */
    vlanProtocolOpt_t   vlanOpt;    /**< Vlan options */
    mplsProtocolOpt_t   mplsOpt;    /**< MPLS options */
    ipv4ProtocolOpt_t   ipv4Opt;    /**< IPv4 options */
    ipv6ProtocolOpt_t   ipv6Opt;    /**< IPv6 options */
} u_FmPcdHdrProtocolOpt;

/**************************************************************************//**
 @Description   A union holding all known protocol fields
*//***************************************************************************/
typedef union t_FmPcdFields {
    headerFieldEth_t            eth;            /**< eth      */
    headerFieldVlan_t           vlan;           /**< vlan     */
    headerFieldLlcSnap_t        llcSnap;        /**< llcSnap  */
    headerFieldPppoe_t          pppoe;          /**< pppoe    */
    headerFieldMpls_t           mpls;           /**< mpls     */
    headerFieldIpv4_t           ipv4;           /**< ipv4     */
    headerFieldIpv6_t           ipv6;           /**< ipv6     */
    headerFieldUdp_t            udp;            /**< udp      */
    headerFieldTcp_t            tcp;            /**< tcp      */
    headerFieldSctp_t           sctp;           /**< sctp     */
    headerFieldDccp_t           dccp;           /**< dccp     */
    headerFieldGre_t            gre;            /**< gre      */
    headerFieldMinencap_t       minencap;       /**< minencap */
    headerFieldIpsecAh_t        ipsecAh;        /**< ipsecAh  */
    headerFieldIpsecEsp_t       ipsecEsp;       /**< ipsecEsp */
    headerFieldUdpEncapEsp_t    udpEncapEsp;    /**< udpEncapEsp */
} t_FmPcdFields;

/**************************************************************************//**
 @Description   structure for defining header extraction for key generation
*//***************************************************************************/
typedef struct t_FmPcdFromHdr {
    uint8_t             size;           /**< Size in byte */
    uint8_t             offset;         /**< Byte offset */
} t_FmPcdFromHdr;

/**************************************************************************//**
 @Description   structure for defining field extraction for key generation
*//***************************************************************************/
typedef struct t_FmPcdFromField {
    t_FmPcdFields       field;          /**< Field selection */
    uint8_t             size;           /**< Size in byte */
    uint8_t             offset;         /**< Byte offset */
} t_FmPcdFromField;

/**************************************************************************//**
 @Description   A structure of parameters used to define a single network
                environment unit.
                A unit should be defined if it will later be used by one or
                more PCD engines to distinguich between flows.
*//***************************************************************************/
typedef struct t_FmPcdDistinctionUnit {
    struct {
        e_NetHeaderType         hdr;        /**< One of the headers supported by the FM */
        u_FmPcdHdrProtocolOpt   opt;        /**< only one option !! */
    } hdrs[FM_PCD_MAX_NUM_OF_INTERCHANGEABLE_HDRS];
} t_FmPcdDistinctionUnit;

/**************************************************************************//**
 @Description   A structure of parameters used to define the different
                units supported by a specific PCD Network Environment
                Characteristics module. Each unit represent
                a protocol or a group of protocols that may be used later
                by the different PCD engined to distinguich between flows.
*//***************************************************************************/
typedef struct t_FmPcdNetEnvParams {
    uint8_t                 numOfDistinctionUnits;                      /**< Number of different units to be identified */
    t_FmPcdDistinctionUnit  units[FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS]; /**< An array of numOfDistinctionUnits of the
                                                                             different units to be identified */
} t_FmPcdNetEnvParams;

/**************************************************************************//**
 @Description   structure for defining a single extraction action
                when creating a key
*//***************************************************************************/
typedef struct t_FmPcdExtractEntry {
    e_FmPcdExtractType                  type;           /**< Extraction type select */
    union {
        struct {
            e_NetHeaderType             hdr;            /**< Header selection */
            bool                        ignoreProtocolValidation;
                                                        /**< Ignore protocol validation */
            e_FmPcdHdrIndex             hdrIndex;       /**< Relevant only for MPLS, VLAN and tunneled
                                                             IP. Otherwise should be cleared.*/
            e_FmPcdExtractByHdrType     type;           /**< Header extraction type select */
            union {
                t_FmPcdFromHdr          fromHdr;        /**< Extract bytes from header parameters */
                t_FmPcdFromField        fromField;      /**< Extract bytes from field parameters*/
                t_FmPcdFields           fullField;      /**< Extract full filed parameters*/
            } extractByHdrType;
        } extractByHdr;                                 /**< used when type = e_FM_PCD_KG_EXTRACT_BY_HDR */
        struct {
            e_FmPcdExtractFrom          src;            /**< Non-header extraction source */
            e_FmPcdAction               action;         /**< Relevant for CC Only */
            uint16_t                    icIndxMask;     /**< Relevant only for CC where
                                                             action=e_FM_PCD_ACTION_INDEXED_LOOKUP */
            uint8_t                     offset;         /**< Byte offset */
            uint8_t                     size;           /**< Size in byte */
        } extractNonHdr;                                /**< used when type = e_FM_PCD_KG_EXTRACT_NON_HDR */
    };
} t_FmPcdExtractEntry;

/**************************************************************************//**
 @Description   A structure for defining masks for each extracted
                field in the key.
*//***************************************************************************/
typedef struct t_FmPcdKgExtractMask {
    uint8_t                         extractArrayIndex;   /**< Index in the extraction array, as initialized by user */
    uint8_t                         offset;              /**< Byte offset */
    uint8_t                         mask;                /**< A byte mask (selected bits will be used) */
} t_FmPcdKgExtractMask;

/**************************************************************************//**
 @Description   A structure for defining default selection per groups
                of fields
*//***************************************************************************/
typedef struct t_FmPcdKgExtractDflt {
    e_FmPcdKgKnownFieldsDfltTypes       type;                /**< Default type select*/
    e_FmPcdKgExtractDfltSelect          dfltSelect;          /**< Default register select */
} t_FmPcdKgExtractDflt;

/**************************************************************************//**
 @Description   A structure for defining all parameters needed for
                generation a key and using a hash function
*//***************************************************************************/
typedef struct t_FmPcdKgKeyExtractAndHashParams {
    uint32_t                    privateDflt0;                /**< Scheme default register 0 */
    uint32_t                    privateDflt1;                /**< Scheme default register 1 */
    uint8_t                     numOfUsedExtracts;           /**< defines the valid size of the following array */
    t_FmPcdExtractEntry         extractArray [FM_PCD_KG_MAX_NUM_OF_EXTRACTS_PER_KEY]; /**< An array of extractions definition. */
    uint8_t                     numOfUsedDflts;              /**< defines the valid size of the following array */
    t_FmPcdKgExtractDflt        dflts[FM_PCD_KG_NUM_OF_DEFAULT_GROUPS];
                                                             /**< For each extraction used in this scheme, specify the required
                                                                  default register to be used when header is not found.
                                                                  types not specified in this array will get undefined value. */
    uint8_t                     numOfUsedMasks;              /**< defines the valid size of the following array */
    t_FmPcdKgExtractMask        masks[FM_PCD_KG_NUM_OF_EXTRACT_MASKS];
    uint8_t                     hashShift;                   /**< hash result right shift. Select the 24 bits out of the 64 hash
                                                                  result. 0 means using the 24 LSB's, otherwise use the
                                                                  24 LSB's after shifting right.*/
    uint32_t                    hashDistributionNumOfFqids;  /**< must be > 1 and a power of 2. Represents the range
                                                                  of queues for the key and hash functionality */
    uint8_t                     hashDistributionFqidsShift;  /**< selects the FQID bits that will be effected by the hash */
    bool                        symmetricHash;               /**< TRUE to generate the same hash for frames with swapped source and
                                                                  destination fields on all layers; If TRUE, driver will check that for
                                                                  all layers, if SRC extraction is selected, DST extraction must also be
                                                                  selected, and vice versa. */
} t_FmPcdKgKeyExtractAndHashParams;

/**************************************************************************//**
 @Description   A structure of parameters for defining a single
                Fqid mask (extracted OR).
*//***************************************************************************/
typedef struct t_FmPcdKgExtractedOrParams {
    e_FmPcdExtractType              type;               /**< Extraction type select */
    union {
        struct {                                        /**< used when type = e_FM_PCD_KG_EXTRACT_BY_HDR */
            e_NetHeaderType         hdr;
            e_FmPcdHdrIndex         hdrIndex;           /**< Relevant only for MPLS, VLAN and tunneled
                                                             IP. Otherwise should be cleared.*/
            bool                    ignoreProtocolValidation;
                                                        /**< continue extraction even if protocol is not recognized */
        } extractByHdr;
        e_FmPcdExtractFrom          src;                /**< used when type = e_FM_PCD_KG_EXTRACT_NON_HDR */
    };
    uint8_t                         extractionOffset;   /**< Offset for extraction (in bytes).  */
    e_FmPcdKgExtractDfltSelect      dfltValue;          /**< Select register from which extraction is taken if
                                                             field not found */
    uint8_t                         mask;               /**< Extraction mask (specified bits are used) */
    uint8_t                         bitOffsetInFqid;    /**< 0-31, Selects which bits of the 24 FQID bits to effect using
                                                             the extracted byte; Assume byte is placed as the 8 MSB's in
                                                             a 32 bit word where the lower bits
                                                             are the FQID; i.e if bitOffsetInFqid=1 than its LSB
                                                             will effect the FQID MSB, if bitOffsetInFqid=24 than the
                                                             extracted byte will effect the 8 LSB's of the FQID,
                                                             if bitOffsetInFqid=31 than the byte's MSB will effect
                                                             the FQID's LSB; 0 means - no effect on FQID;
                                                             Note that one, and only one of
                                                             bitOffsetInFqid or bitOffsetInPlcrProfile must be set (i.e,
                                                             extracted byte must effect either FQID or Policer profile).*/
    uint8_t                         bitOffsetInPlcrProfile;
                                                        /**< 0-15, Selects which bits of the 8 policer profile id bits to
                                                             effect using the extracted byte; Assume byte is placed
                                                             as the 8 MSB's in a 16 bit word where the lower bits
                                                             are the policer profile id; i.e if bitOffsetInPlcrProfile=1
                                                             than its LSB will effect the profile MSB, if bitOffsetInFqid=8
                                                             than the extracted byte will effect the whole policer profile id,
                                                             if bitOffsetInFqid=15 than the byte's MSB will effect
                                                             the Policer Profile id's LSB;
                                                             0 means - no effect on policer profile; Note that one, and only one of
                                                             bitOffsetInFqid or bitOffsetInPlcrProfile must be set (i.e,
                                                             extracted byte must effect either FQID or Policer profile).*/
} t_FmPcdKgExtractedOrParams;

/**************************************************************************//**
 @Description   A structure for configuring scheme counter
*//***************************************************************************/
typedef struct t_FmPcdKgSchemeCounter {
    bool        update;     /**< FALSE to keep the current counter state
                                 and continue from that point, TRUE to update/reset
                                 the counter when the scheme is written. */
    uint32_t    value;      /**< If update=TRUE, this value will be written into the
                                 counter. clear this field to reset the counter. */
} t_FmPcdKgSchemeCounter;

/**************************************************************************//**
 @Description   A structure for defining policer profile
                parameters as required by keygen (when policer
                is the next engine after this scheme).
*//***************************************************************************/
typedef struct t_FmPcdKgPlcrProfile {
    bool                sharedProfile;              /**< TRUE if this profile is shared between ports
                                                         (i.e. managed by master partition) May not be TRUE
                                                         if profile is after Coarse Classification*/
    bool                direct;                     /**< if TRUE, directRelativeProfileId only selects the profile
                                                         id, if FALSE fqidOffsetRelativeProfileIdBase is used
                                                         together with fqidOffsetShift and numOfProfiles
                                                         parameters, to define a range of profiles from
                                                         which the keygen result will determine the
                                                         destination policer profile.  */
    union {
        uint16_t        directRelativeProfileId;    /**< Used if 'direct' is TRUE, to select policer profile.
                                                         This parameter should
                                                         indicate the policer profile offset within the port's
                                                         policer profiles or SHARED window. */
        struct {
            uint8_t     fqidOffsetShift;            /**< shift of KG results without the qid base */
            uint8_t     fqidOffsetRelativeProfileIdBase;
                                                    /**< OR of KG results without the qid base
                                                         This parameter should indicate the policer profile
                                                         offset within the port's policer profiles window or
                                                         SHARED window depends on sharedProfile */
            uint8_t     numOfProfiles;              /**< Range of profiles starting at base */
        } indirectProfile;
    } profileSelect;
} t_FmPcdKgPlcrProfile;

/**************************************************************************//**
 @Description   A structure for CC parameters if CC is the next engine after KG
*//***************************************************************************/
typedef struct t_FmPcdKgCc {
    t_Handle                h_CcTree;           /**< A handle to a CC Tree */
    uint8_t                 grpId;              /**< CC group id within the CC tree */
    bool                    plcrNext;           /**< TRUE if after CC, in case of data frame,
                                                     policing is required. */
    bool                    bypassPlcrProfileGeneration;
                                                /**< TRUE to bypass keygen policer profile
                                                     generation (profile selected is the one selected at
                                                     port initialization). */
    t_FmPcdKgPlcrProfile    plcrProfile;        /**< only if plcrNext=TRUE and bypassPlcrProfileGeneration=FALSE */
} t_FmPcdKgCc;

/**************************************************************************//**
 @Description   A structure for initializing a keygen single scheme
*//***************************************************************************/
typedef struct t_FmPcdKgSchemeParams {
    bool                                modify;                 /**< TRUE to change an existing scheme */
    union
    {
        uint8_t                         relativeSchemeId;       /**< if modify=FALSE:Partition relative scheme id */
        t_Handle                        h_Scheme;               /**< if modify=TRUE: a handle of the existing scheme */
    }id;
    bool                                alwaysDirect;           /**< This scheme is reached only directly, i.e.                                                              no need for match vector. Keygen will ignore
                                                                     it when matching   */
    struct {                                                    /**< HL Relevant only if alwaysDirect = FALSE */
        t_Handle                        h_NetEnv;               /**< A handle to the Network environment as returned
                                                                     by FM_PCD_SetNetEnvCharacteristics() */
        uint8_t                         numOfDistinctionUnits;  /**< Number of netenv units listed in unitIds array */
        uint8_t                         unitIds[FM_PCD_MAX_NUM_OF_DISTINCTION_UNITS];
                                                                /**< Indexes as passed to SetNetEnvCharacteristics array*/
    } netEnvParams;
    bool                                useHash;                /**< use the KG Hash functionality  */
    t_FmPcdKgKeyExtractAndHashParams    keyExtractAndHashParams;
                                                                /**< used only if useHash = TRUE */
    bool                                bypassFqidGeneration;   /**< Normally - FALSE, TRUE to avoid FQID update in the IC;
                                                                     In such a case FQID after KG will be the default FQID
                                                                     defined for the relevant port, or the FQID defined by CC
                                                                     in cases where CC was the previous engine. */
    uint32_t                            baseFqid;               /**< Base FQID; Relevant only if bypassFqidGeneration = FALSE;
                                                                     If hash is used and an even distribution is expected
                                                                     according to hashDistributionNumOfFqids, baseFqid must be aligned to
                                                                     hashDistributionNumOfFqids.  */
    uint8_t                             numOfUsedExtractedOrs;  /**< Number of Fqid masks listed in extractedOrs array*/
    t_FmPcdKgExtractedOrParams          extractedOrs[FM_PCD_KG_NUM_OF_GENERIC_REGS];
                                                                /**< IN: FM_PCD_KG_NUM_OF_GENERIC_REGS
                                                                     registers are shared between qidMasks
                                                                     functionality and some of the extraction
                                                                     actions; Normally only some will be used
                                                                     for qidMask. Driver will return error if
                                                                     resource is full at initialization time. */
    e_FmPcdEngine                       nextEngine;             /**< may be BMI, PLCR or CC */
    union {                                                     /**< depends on nextEngine */
        e_FmPcdDoneAction               doneAction;             /**< Used when next engine is BMI (done) */
        t_FmPcdKgPlcrProfile            plcrProfile;            /**< Used when next engine is PLCR */
        t_FmPcdKgCc                     cc;                     /**< Used when next engine is CC */
    } kgNextEngineParams;
    t_FmPcdKgSchemeCounter              schemeCounter;          /**< A structure of parameters for updating
                                                                     the scheme counter */
} t_FmPcdKgSchemeParams;

/**************************************************************************//**
 @Description   A structure for defining CC params when CC is the
                next engine after a CC node.
*//***************************************************************************/
typedef struct t_FmPcdCcNextCcParams {
    t_Handle    h_CcNode;               /**< A handle of the next CC node */
} t_FmPcdCcNextCcParams;

/**************************************************************************//**
 @Description   A structure for defining PLCR params when PLCR is the
                next engine after a CC node.
*//***************************************************************************/
typedef struct t_FmPcdCcNextPlcrParams {
    bool        overrideParams;         /**< TRUE if CC override previously decided parameters*/
    bool        sharedProfile;          /**< Relevant only if overrideParams=TRUE:
                                             TRUE if this profile is shared between ports */
    uint16_t    newRelativeProfileId;   /**< Relevant only if overrideParams=TRUE:
                                             (otherwise profile id is taken from keygen);
                                             This parameter should indicate the policer
                                             profile offset within the port's
                                             policer profiles or from SHARED window.*/
    uint32_t    newFqid;                /**< Relevant only if overrideParams=TRUE:
                                             FQID for enqueuing the frame;
                                             In earlier chips  if policer next engine is KEYGEN,
                                             this parameter can be 0, because the KEYGEN
                                             always decides the enqueue FQID.*/
    bool        statisticsEn;           /**< In the case of TRUE Statistic counter is
                                             incremented for each received frame passed through
                                             this Coarse Classification entry.*/
} t_FmPcdCcNextPlcrParams;

/**************************************************************************//**
 @Description   A structure for defining enqueue params when BMI is the
                next engine after a CC node.
*//***************************************************************************/
typedef struct t_FmPcdCcNextEnqueueParams {

    e_FmPcdDoneAction    action;        /**< Action - when next engine is BMI (done) */
    bool                 overrideFqid;  /**< TRUE if CC override previously decided Fqid(by Keygen),
                                             relevant if action = e_FM_PCD_ENQ_FRAME */
    uint32_t             newFqid;       /**< Valid if overrideFqid=TRUE, FQID for enqueuing the frame
                                             (otherwise FQID is taken from keygen),
                                             relevant if action = e_FM_PCD_ENQ_FRAME*/
    bool                 statisticsEn;  /**< In the case of TRUE Statistic counter is
                                             incremented for each received frame passed through
                                             this Coarse Classification entry.*/
} t_FmPcdCcNextEnqueueParams;

/**************************************************************************//**
 @Description   A structure for defining KG params when KG is the
                next engine after a CC node.
*//***************************************************************************/
typedef struct t_FmPcdCcNextKgParams {
    bool        overrideFqid;           /**< TRUE if CC override previously decided Fqid (by keygen),
                                             Note - this parameters irrelevant for earlier chips*/
    uint32_t    newFqid;                /**< Valid if overrideFqid=TRUE, FQID for enqueuing the frame
                                             (otherwise FQID is taken from keygen),
                                             Note - this parameters irrelevant for earlier chips*/
    t_Handle    h_DirectScheme;         /**< Direct scheme handle to go to. */
    bool        statisticsEn;           /**< In the case of TRUE Statistic counter is
                                             incremented for each received frame passed through
                                             this Coarse Classification entry.*/
} t_FmPcdCcNextKgParams;

/**************************************************************************//**
 @Description   A structure for defining next engine params after a CC node.
*//***************************************************************************/
typedef struct t_FmPcdCcNextEngineParams {
    e_FmPcdEngine                       nextEngine;    /**< User has to initialize parameters
                                                            according to nextEngine definition */
    union {
        t_FmPcdCcNextCcParams           ccParams;      /**< Parameters in case next engine is CC */
        t_FmPcdCcNextPlcrParams         plcrParams;    /**< Parameters in case next engine is PLCR */
        t_FmPcdCcNextEnqueueParams      enqueueParams; /**< Parameters in case next engine is BMI */
        t_FmPcdCcNextKgParams           kgParams;      /**< Parameters in case next engine is KG */
    } params;
#if defined(FM_CAPWAP_SUPPORT)
    t_Handle                            h_Manip;       /**< Handler to headerManip.
                                                            Relevant if next engine of the type result
                                                            (e_FM_PCD_PLCR, e_FM_PCD_KG, e_FM_PCD_DONE) */
#endif /* defined(FM_CAPWAP_SUPPORT) || ... */
} t_FmPcdCcNextEngineParams;

/**************************************************************************//**
 @Description   A structure for defining a single CC Key parameters
*//***************************************************************************/
typedef struct t_FmPcdCcKeyParams {
    uint8_t                     *p_Key;     /**< pointer to the key of the size defined in keySize*/
    uint8_t                     *p_Mask;    /**< pointer to the Mask per key  of the size defined
                                                 in keySize. p_Key and p_Mask (if defined) has to be
                                                 of the same size defined in the keySize */
    t_FmPcdCcNextEngineParams   ccNextEngineParams;
                                            /**< parameters for the next for the defined Key in
                                                 the p_Key */
} t_FmPcdCcKeyParams;

/**************************************************************************//**
 @Description   A structure for defining CC Keys parameters
*//***************************************************************************/
typedef struct t_KeysParams {
    uint8_t                     numOfKeys;      /**< Number Of relevant Keys  */
    uint8_t                     keySize;        /**< size of the key - in the case of the extraction of
                                                     the type FULL_FIELD keySize has to be as standard size of the relevant
                                                     key. In the another type of extraction keySize has to be as size of extraction.
                                                     In the case of action = e_FM_PCD_ACTION_INDEXED_LOOKUP the size of keySize has to be 2*/
    t_FmPcdCcKeyParams          keyParams[FM_PCD_MAX_NUM_OF_KEYS];
                                                /**< it's array with numOfKeys entries each entry in
                                                     the array of the type t_FmPcdCcKeyParams */
    t_FmPcdCcNextEngineParams   ccNextEngineParamsForMiss;
                                                /**< parameters for the next step of
                                                     unfound (or undefined) key . Not relevant in the case
                                                     of action = e_FM_PCD_ACTION_INDEXED_LOOKUP*/
} t_KeysParams;

/**************************************************************************//**
 @Description   A structure for defining the CC node params
*//***************************************************************************/
typedef struct t_FmPcdCcNodeParams {
    t_FmPcdExtractEntry         extractCcParams;    /**< params which defines extraction parameters */
    t_KeysParams                keysParams;         /**< params which defines Keys parameters of the
                                                         extraction defined in extractCcParams */
} t_FmPcdCcNodeParams;

/**************************************************************************//**
 @Description   A structure for defining each CC tree group in term of
                NetEnv units and the action to be taken in each case.
                the unitIds list must be in order from lower to higher indexes.

                t_FmPcdCcNextEngineParams is a list of 2^numOfDistinctionUnits
                structures where each defines the next action to be taken for
                each units combination. for example:
                numOfDistinctionUnits = 2
                unitIds = {1,3}
                p_NextEnginePerEntriesInGrp[0] = t_FmPcdCcNextEngineParams for the case that
                                                        unit 1 - not found; unit 3 - not found;
                p_NextEnginePerEntriesInGrp[1] = t_FmPcdCcNextEngineParams for the case that
                                                        unit 1 - not found; unit 3 - found;
                p_NextEnginePerEntriesInGrp[2] = t_FmPcdCcNextEngineParams for the case that
                                                        unit 1 - found; unit 3 - not found;
                p_NextEnginePerEntriesInGrp[3] = t_FmPcdCcNextEngineParams for the case that
                                                        unit 1 - found; unit 3 - found;
*//***************************************************************************/
typedef struct t_FmPcdCcGrpParams {
    uint8_t                     numOfDistinctionUnits;          /**< up to 4 */
    uint8_t                     unitIds[FM_PCD_MAX_NUM_OF_CC_UNITS];
                                                                /**< Indexes of the units as defined in
                                                                     FM_PCD_SetNetEnvCharacteristics() */
    t_FmPcdCcNextEngineParams   nextEnginePerEntriesInGrp[FM_PCD_MAX_NUM_OF_CC_ENTRIES_IN_GRP];
                                                                /**< Max size is 16 - if only one group used */
} t_FmPcdCcGrpParams;

/**************************************************************************//**
 @Description   A structure for defining the CC tree groups
*//***************************************************************************/
typedef struct t_FmPcdCcTreeParams {
    t_Handle                h_NetEnv;                                   /**< A handle to the Network environment as returned
                                                                             by FM_PCD_SetNetEnvCharacteristics() */
    uint8_t                 numOfGrps;                                  /**< Number of CC groups within the CC tree */
    t_FmPcdCcGrpParams      ccGrpParams[FM_PCD_MAX_NUM_OF_CC_GROUPS];   /**< Parameters for each group. */
} t_FmPcdCcTreeParams;

/**************************************************************************//**
 @Description   A structure for defining parameters for byte rate
*//***************************************************************************/
typedef struct t_FmPcdPlcrByteRateModeParams {
    e_FmPcdPlcrFrameLengthSelect    frameLengthSelection;   /**< Frame length selection */
    e_FmPcdPlcrRollBackFrameSelect  rollBackFrameSelection; /**< relevant option only e_FM_PCD_PLCR_L2_FRM_LEN,
                                                                 e_FM_PCD_PLCR_FULL_FRM_LEN */
} t_FmPcdPlcrByteRateModeParams;

/**************************************************************************//**
 @Description   A structure for selcting the policer profile RFC-2698 or
                RFC-4115 parameters
*//***************************************************************************/
typedef struct t_FmPcdPlcrNonPassthroughAlgParams {
    e_FmPcdPlcrRateMode              rateMode;                       /**< Byte / Packet */
    t_FmPcdPlcrByteRateModeParams    byteModeParams;                 /**< Valid for Byte NULL for Packet */
    uint32_t                         comittedInfoRate;               /**< KBits/Sec or Packets/Sec */
    uint32_t                         comittedBurstSize;              /**< Bytes/Packets */
    uint32_t                         peakOrAccessiveInfoRate;        /**< KBits/Sec or Packets/Sec */
    uint32_t                         peakOrAccessiveBurstSize;       /**< Bytes/Packets */
} t_FmPcdPlcrNonPassthroughAlgParams;

/**************************************************************************//**
 @Description   A union for defining Policer next engine parameters
*//***************************************************************************/
typedef union u_FmPcdPlcrNextEngineParams {
        e_FmPcdDoneAction               action;             /**< Action - when next engine is BMI (done) */
        t_Handle                        h_Profile;          /**< Policer profile handle -  used when next engine
                                                                 is PLCR, must be a SHARED profile */
        t_Handle                        h_DirectScheme;     /**< Direct scheme select - when next engine is Keygen */
} u_FmPcdPlcrNextEngineParams;

/**************************************************************************//**
 @Description   A structure for selecting the policer profile entry parameters
*//***************************************************************************/
typedef struct t_FmPcdPlcrProfileParams {
    bool                                modify;                     /**< TRUE to change an existing profile */
    union {
        struct {
            e_FmPcdProfileTypeSelection profileType;                /**< Type of policer profile */
            t_Handle                    h_FmPort;                   /**< Relevant for per-port profiles only */
            uint16_t                    relativeProfileId;          /**< Profile id - relative to shared group or to port */
        } newParams;                                                /**< use it when modify=FALSE */
        t_Handle                        h_Profile;                  /**< A handle to a profile - use it when modify=TRUE */
    } id;
    e_FmPcdPlcrAlgorithmSelection       algSelection;               /**< Profile Algorithm PASS_THROUGH, RFC_2698, RFC_4115 */
    e_FmPcdPlcrColorMode                colorMode;                  /**< COLOR_BLIND, COLOR_AWARE */

    union {
        e_FmPcdPlcrColor                dfltColor;                  /**< For Color-Blind Pass-Through mode. the policer will re-color
                                                                         any incoming packet with the default value. */
        e_FmPcdPlcrColor                override;                   /**< For Color-Aware modes. The profile response to a
                                                                         pre-color value of 2'b11. */
    } color;

    t_FmPcdPlcrNonPassthroughAlgParams  nonPassthroughAlgParams;    /**< RFC2698 or RFC4115 params */

    e_FmPcdEngine                       nextEngineOnGreen;          /**< Green next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnGreen;              /**< Green next engine params */

    e_FmPcdEngine                       nextEngineOnYellow;         /**< Yellow next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnYellow;             /**< Yellow next engine params */

    e_FmPcdEngine                       nextEngineOnRed;            /**< Red next engine type */
    u_FmPcdPlcrNextEngineParams         paramsOnRed;                /**< Red next engine params */

    bool                                trapProfileOnFlowA;         /**< Trap on flow A */
    bool                                trapProfileOnFlowB;         /**< Trap on flow B */
    bool                                trapProfileOnFlowC;         /**< Trap on flow C */
} t_FmPcdPlcrProfileParams;

#if defined(FM_CAPWAP_SUPPORT)
/**************************************************************************//**
 @Description   A structure for selecting the location of manipulation
*//***************************************************************************/
typedef struct t_FmPcdManipLocationParams {
    e_FmPcdManipLocateType              type;           /**< location of manipulation type select */
    struct {                                            /**< used when type = e_FM_PCD_MANIP_BY_HDR */
        e_NetHeaderType                 hdr;            /**< Header selection */
        e_FmPcdHdrIndex                 hdrIndex;       /**< Relevant only for MPLS, VLAN and tunneled
                                                             IP. Otherwise should be cleared. */
        bool                            byField;        /**< TRUE if the location of manipulation is according to some field in the specific header*/
        t_FmPcdFields                   fullField;      /**< Relevant only when byField = TRUE: Extract field */
    } manipByHdr;
} t_FmPcdManipLocationParams;

/**************************************************************************//**
 @Description   structure for defining insert manipulation
                of the type e_FM_PCD_MANIP_INSRT_TO_START_OF_FRAME_TEMPLATE
*//***************************************************************************/
typedef struct t_FmPcdManipInsrtByTemplateParams {
    uint8_t         size;                               /**< size of insert template to the start of the frame. */
    uint8_t         hdrTemplate[FM_PCD_MAX_MANIP_INSRT_TEMPLATE_SIZE];
                                                        /**< array of the insertion template. */

    bool            modifyOuterIp;                      /**< TRUE if user want to modify some fields in outer IP. */
    struct {
        uint16_t    ipOuterOffset;                      /**< offset of outer IP in the insert template, relevant if modifyOuterIp = TRUE.*/
        uint16_t    dscpEcn;                            /**< value of dscpEcn in IP outer, relevant if modifyOuterIp = TRUE.
                                                             in IPV4 dscpEcn only byte - it has to be adjusted to the right*/
        bool        udpPresent;                         /**< TRUE if UDP is present in the insert template, relevant if modifyOuterIp = TRUE.*/
        uint8_t     udpOffset;                          /**< offset in the insert template of UDP, relevant if modifyOuterIp = TRUE and udpPresent=TRUE.*/
        uint8_t     ipIdentGenId;                       /**< Used by FMan-CTRL to calculate IP-identification field,relevant if modifyOuterIp = TRUE.*/
        bool        recalculateLength;                  /**< TRUE if recalculate length has to be performed due to the engines in the path which can change the frame later, relevant if modifyOuterIp = TRUE.*/
        struct {
            uint8_t blockSize;                          /**< The CAAM block-size; Used by FMan-CTRL to calculate the IP-total-len field.*/
            uint8_t extraBytesAddedAlignedToBlockSize;  /**< Used by FMan-CTRL to calculate the IP-total-len field and UDP length*/
            uint8_t extraBytesAddedNotAlignedToBlockSize;/**< Used by FMan-CTRL to calculate the IP-total-len field and UDP length.*/
        } recalculateLengthParams;                      /**< recalculate length parameters - relevant if modifyOuterIp = TRUE and recalculateLength = TRUE */
    } modifyOuterIpParams;                              /**< Outer IP modification parameters - ignored if modifyOuterIp is FALSE */

    bool            modifyOuterVlan;                    /**< TRUE if user wants to modify vpri field in the outer VLAN header*/
    struct {
        uint8_t     vpri;                               /**< value of vpri, relevant if modifyOuterVlan = TRUE
                                                             vpri only 3 bits, it has to be adjusted to the right*/
    } modifyOuterVlanParams;
} t_FmPcdManipInsrtByTemplateParams;
#endif /* defined(FM_CAPWAP_SUPPORT) || ... */


#ifdef FM_CAPWAP_SUPPORT
/**************************************************************************//**
 @Description   structure for defining CAPWAP fragmentation
*//***************************************************************************/
typedef struct t_CapwapFragmentationParams {
    uint16_t         sizeForFragmentation;              /**< if length of the frame is greater than this value, CAPWAP fragmentation will be executed.*/
    bool             headerOptionsCompr;                /**< TRUE - first fragment include the CAPWAP header options field,
                                                             and all other fragments exclude the CAPWAP options field,
                                                             FALSE - all fragments include CAPWAP header options field. */
} t_CapwapFragmentationParams;

/**************************************************************************//**
 @Description   structure for defining CAPWAP Re-assembly
*//***************************************************************************/
typedef struct t_CapwapReassemblyParams {
    uint16_t                        maxNumFramesInProcess;  /**< Number of frames which can be processed by Reassembly in the same time.
                                                                 It has to be power of 2.
                                                                 In the case numOfFramesPerHashEntry == e_FM_PCD_MANIP_FOUR_WAYS_HASH,
                                                                 maxNumFramesInProcess has to be in the range of 4 - 512,
                                                                 In the case numOfFramesPerHashEntry == e_FM_PCD_MANIP_EIGHT_WAYS_HASH,
                                                                 maxNumFramesInProcess has to be in the range of 8 - 2048 */
    bool                            haltOnDuplicationFrag;  /**< In the case of TRUE, Reassembly process halted due to duplicated fragment,
                                                                 and all processed fragments passed for enqueue with error indication.
                                                                 In the case of FALSE, only duplicated fragment passed for enqueue with error indication */

    e_FmPcdManipReassemTimeOutMode  timeOutMode;            /**< Expiration delay initialized by Reassembly process */
    uint32_t                        fqidForTimeOutFrames;   /**< Fqid in which time out frames will enqueue during Time Out Process  */
    uint32_t                        timeoutRoutineRequestTime;
                                                            /**< Represents the time interval in microseconds between consecutive
                                                                 timeout routine requests It has to be power of 2. */
    uint32_t                        timeoutThresholdForReassmProcess;
                                                            /**< Represents the time interval in microseconds which defines
                                                                 if opened frame (at least one fragment was processed but not all the fragments)is found as too old*/

    e_FmPcdManipReassemWaysNumber   numOfFramesPerHashEntry;/**< Number of frames per hash entry needed for reassembly process */
} t_CapwapReassemblyParams;
#endif /* FM_CAPWAP_SUPPORT */


#if defined(FM_CAPWAP_SUPPORT)
/**************************************************************************//**
 @Description   structure for defining fragmentation/reassembly
*//***************************************************************************/
typedef struct t_FmPcdManipFragOrReasmParams {
    bool                                frag;               /**< TRUE if using the structure for fragmentation,
                                                                 otherwise this structure is used for reassembly */
    uint8_t                             extBufPoolIndx;     /**< Index of the buffer pool ID which was configured for port
                                                                 and can be used for manipulation;
                                                                 NOTE: This field is relevant only for CAPWAP fragmentation
                                                                 and reassembly */
    e_NetHeaderType                     hdr;                /**< Header selection */
    union {
#ifdef FM_CAPWAP_SUPPORT
        t_CapwapFragmentationParams     capwapFragParams;   /**< Structure for CAPWAP fragmentation, relevant if frag = TRUE, hdr = HEADER_TYPE_CAPWAP */
        t_CapwapReassemblyParams        capwapReasmParams;  /**< Structure for CAPWAP reassembly, relevant if frag = FALSE, hdr = HEADER_TYPE_CAPWAP */
#endif /* FM_CAPWAP_SUPPORT */
    };
} t_FmPcdManipFragOrReasmParams;

/**************************************************************************//**
 @Description   structure for defining insert manipulation
*//***************************************************************************/
typedef struct t_FmPcdManipInsrtParams {
    e_FmPcdManipInsrtType                       type;       /**< Type of insert manipulation */
    union {
        t_FmPcdManipInsrtByTemplateParams       insrtByTemplateParams;
                                                            /**< parameters for insert manipulation, relevant if
                                                                 type = e_FM_PCD_MANIP_INSRT_TO_START_OF_FRAME_TEMPLATE */
    };
} t_FmPcdManipInsrtParams;

/**************************************************************************//**
 @Description   structure for defining remove manipulation
*//***************************************************************************/
typedef struct t_FmPcdManipRmvParams {
    e_FmPcdManipRmvParamsType                   type;   /**< Type of remove manipulation */
    t_FmPcdManipLocationParams                  rmvSpecificLocationParams;
                                                        /**< Specified location of remove manipulation;
                                                              This params should be initialized in cases:
                                                              - e_FM_PCD_MANIP_RMV_FROM_START_OF_FRAME_TILL_SPECIFIC_LOCATION
                                                              - e_FM_PCD_MANIP_RMV_FROM_START_OF_FRAME_INCLUDE_SPECIFIC_LOCATION */
} t_FmPcdManipRmvParams;

/**************************************************************************//**
 @Description   structure for defining manipulation
*//***************************************************************************/
typedef struct t_FmPcdManipParams {
    bool                                        rmv;                /**< TRUE, if defined remove manipulation */
    t_FmPcdManipRmvParams                       rmvParams;          /**< Parameters for remove manipulation, relevant if rmv = TRUE */

    bool                                        insrt;              /**< TRUE, if defined insert manipulation */
    t_FmPcdManipInsrtParams                     insrtParams;        /**< Parameters for insert manipulation, relevant if insrt = TRUE */

    bool                                        fragOrReasm;        /**< TRUE, if defined fragmentation/reassembly manipulation */
    t_FmPcdManipFragOrReasmParams               fragOrReasmParams;  /**< Parameters for fragmentation/reassembly manipulation, relevant if fragOrReasm = TRUE */

    /**< General parameters */
    bool                                        treatFdStatusFieldsAsErrors;
                                                                    /**< Set to TRUE when the port that is using this manip is chained
                                                                         to SEC (i.e. the traffic was forwarded from SEC) */
} t_FmPcdManipParams;

/**************************************************************************//**
 @Description   structure for defining statistics node
*//***************************************************************************/
typedef struct t_FmPcdStatsParams {
    e_FmPcdStatsType        type; /**< type of statistics node */
} t_FmPcdStatsParams;
#endif /* defined(FM_CAPWAP_SUPPORT) || ... */


/**************************************************************************//**
 @Function      FM_PCD_SetNetEnvCharacteristics

 @Description   Define a set of Network Environment Characteristics.
                When setting an environment it is important to understand its
                application. It is not meant to describe the flows that will run
                on the ports using this environment, but what the user means TO DO
                with the PCD mechanisms in order to parse-classify-distribute those
                frames.
                By specifying a distinction unit, the user means it would use that option
                for distinction between frames at either a keygen scheme keygen or a coarse
                classification action descriptor. Using interchangeable headers to define a
                unit means that the user is indifferent to which of the interchangeable
                headers is present in the frame, and they want the distinction to be based
                on the presence of either one of them.
                Depending on context, there are limitations to the use of environments. A
                port using the PCD functionality is bound to an environment. Some or even
                all ports may share an environment but also an environment per port is
                possible. When initializing a scheme, a classification plan group (see below),
                or a coarse classification tree, one of the initialized environments must be
                stated and related to. When a port is bound to a scheme, a classification
                plan group, or a coarse classification tree, it MUST be bound to the same
                environment.
                The different PCD modules, may relate (for flows definition) ONLY on
                distinction units as defined by their environment. When initializing a
                scheme for example, it may not choose to select IPV4 as a match for
                recognizing flows unless it was defined in the relating environment. In
                fact, to guide the user through the configuration of the PCD, each module's
                characterization in terms of flows is not done using protocol names, but using
                environment indexes.
                In terms of HW implementation, the list of distinction units sets the LCV vectors
                and later used for match vector, classification plan vectors and coarse classification
                indexing.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     p_NetEnvParams  A structure of parameters for the initialization of
                                the network environment.

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_SetNetEnvCharacteristics(t_Handle h_FmPcd, t_FmPcdNetEnvParams *p_NetEnvParams);

/**************************************************************************//**
 @Function      FM_PCD_DeleteNetEnvCharacteristics

 @Description   Deletes a set of Network Environment Characteristics.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     h_NetEnv        A handle to the Network environment.

 @Return        E_OK on success; Error code otherwise.
*//***************************************************************************/
t_Error FM_PCD_DeleteNetEnvCharacteristics(t_Handle h_FmPcd, t_Handle h_NetEnv);

/**************************************************************************//**
 @Function      FM_PCD_KgSetScheme

 @Description   Initializing or modifying and enabling a scheme for the keygen.
                This routine should be called for adding or modifying a scheme.
                When a scheme needs modifying, the API requires that it will be
                rewritten. In such a case 'modify' should be TRUE. If the
                routine is called for a valid scheme and 'modify' is FALSE,
                it will return error.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in,out] p_Scheme        A structure of parameters for defining the scheme

 @Return        A handle to the initialized scheme on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_KgSetScheme (t_Handle                h_FmPcd,
                             t_FmPcdKgSchemeParams   *p_Scheme);

/**************************************************************************//**
 @Function      FM_PCD_KgDeleteScheme

 @Description   Deleting an initialized scheme.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_Scheme        scheme handle as returned by FM_PCD_KgSetScheme

 @Return        E_OK on success; Error code otherwise.
 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error     FM_PCD_KgDeleteScheme(t_Handle h_FmPcd, t_Handle h_Scheme);

/**************************************************************************//**
 @Function      FM_PCD_KgGetSchemeCounter

 @Description   Reads scheme packet counter.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     h_Scheme        scheme handle as returned by FM_PCD_KgSetScheme.

 @Return        Counter's current value.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
uint32_t  FM_PCD_KgGetSchemeCounter(t_Handle h_FmPcd, t_Handle h_Scheme);

/**************************************************************************//**
 @Function      FM_PCD_KgSetSchemeCounter

 @Description   Writes scheme packet counter.

 @Param[in]     h_FmPcd         FM PCD module descriptor.
 @Param[in]     h_Scheme        scheme handle as returned by FM_PCD_KgSetScheme.
 @Param[in]     value           New scheme counter value - typically '0' for
                                resetting the counter.
 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error  FM_PCD_KgSetSchemeCounter(t_Handle h_FmPcd, t_Handle h_Scheme, uint32_t value);

/**************************************************************************//**
 @Function      FM_PCD_CcBuildTree

 @Description   This routine must be called to define a complete coarse
                classification tree. This is the way to define coarse
                classification to a certain flow - the keygen schemes
                may point only to trees defined in this way.

 @Param[in]     h_FmPcd                 FM PCD module descriptor.
 @Param[in]     p_FmPcdCcTreeParams     A structure of parameters to define the tree.

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_CcBuildTree (t_Handle             h_FmPcd,
                             t_FmPcdCcTreeParams  *p_FmPcdCcTreeParams);

/**************************************************************************//**
 @Function      FM_PCD_CcDeleteTree

 @Description   Deleting an built tree.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_CcTree        A handle to a CC tree.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_CcDeleteTree(t_Handle h_FmPcd, t_Handle h_CcTree);

/**************************************************************************//**
 @Function      FM_PCD_CcSetNode

 @Description   This routine should be called for each CC (coarse classification)
                node. The whole CC tree should be built bottom up so that each
                node points to already defined nodes.

 @Param[in]     h_FmPcd             FM PCD module descriptor.
 @Param[in]     p_CcNodeParam       A structure of parameters defining the CC node

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle   FM_PCD_CcSetNode(t_Handle             h_FmPcd,
                            t_FmPcdCcNodeParams  *p_CcNodeParam);

/**************************************************************************//**
 @Function      FM_PCD_CcDeleteNode

 @Description   Deleting an built node.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_CcNode        A handle to a CC node.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_CcDeleteNode(t_Handle h_FmPcd, t_Handle h_CcNode);

/**************************************************************************//**
 @Function      FM_PCD_CcTreeModifyNextEngine

 @Description   Modify the Next Engine Parameters in the entry of the tree.

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcTree                    A handle to the tree
 @Param[in]     grpId                       A Group index in the tree
 @Param[in]     index                       Entry index in the group defined by grpId
 @Param[in]     p_FmPcdCcNextEngineParams   A structure for defining new next engine params

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcBuildTree().
*//***************************************************************************/
t_Error FM_PCD_CcTreeModifyNextEngine(t_Handle h_FmPcd, t_Handle h_CcTree, uint8_t grpId, uint8_t index, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeModifyNextEngine

 @Description   Modify the Next Engine Parameters in the relevant key entry of the node.

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for Next Engine Params modifications
 @Param[in]     p_FmPcdCcNextEngineParams   A structure for defining new next engine params

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode().
*//***************************************************************************/
t_Error FM_PCD_CcNodeModifyNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeModifyMissNextEngine

 @Description   Modify the Next Engine Parameters of the Miss key case of the node.

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     p_FmPcdCcNextEngineParams   A structure for defining new next engine params

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode().
*//***************************************************************************/
t_Error FM_PCD_CcNodeModifyMissNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, t_FmPcdCcNextEngineParams *p_FmPcdCcNextEngineParams);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeRemoveKey

 @Description   Remove the key (include Next Engine Parameters of this key) defined by the index of the relevant node .

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for removing

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode() not only of the relevant node but also
                the node that points to this node
*//***************************************************************************/
t_Error FM_PCD_CcNodeRemoveKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeAddKey

 @Description   Add the key(include Next Engine Parameters of this key)in the index defined by the keyIndex .

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for adding
 @Param[in]     keySize                     Key size of added key
 @Param[in]     p_KeyParams                 A pointer to the parameters includes new key with Next Engine Parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode() not only of the relevant node but also
                the node that points to this node
*//***************************************************************************/
t_Error FM_PCD_CcNodeAddKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeModifyKeyAndNextEngine

 @Description   Modify the key and Next Engine Parameters of this key in the index defined by the keyIndex .

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for adding
 @Param[in]     keySize                     Key size of added key
 @Param[in]     p_KeyParams                 A pointer to the parameters includes modified key and modified Next Engine Parameters

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode() not only of the relevant node but also
                the node that points to this node
*//***************************************************************************/
t_Error FM_PCD_CcNodeModifyKeyAndNextEngine(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, t_FmPcdCcKeyParams  *p_KeyParams);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeModifyKey

 @Description   Modify the key  in the index defined by the keyIndex .

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for adding
 @Param[in]     keySize                     Key size of added key
 @Param[in]     p_Key                       A pointer to the new key
 @Param[in]     p_Mask                      A pointer to the new mask if relevant, otherwise pointer to NULL

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_CcSetNode() not only of the relevant node but also
                the node that points to this node
*//***************************************************************************/
t_Error FM_PCD_CcNodeModifyKey(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex, uint8_t keySize, uint8_t  *p_Key, uint8_t *p_Mask);

/**************************************************************************//**
 @Function      FM_PCD_CcNodeGetKeyCounter

 @Description   This routine may be used to get a counter of specific key in a CC
                Node; This counter reflects how many frames passed that were matched
                this key.

 @Param[in]     h_FmPcd                     A handle to an FM PCD Module.
 @Param[in]     h_CcNode                    A handle to the node
 @Param[in]     keyIndex                    Key index for adding

 @Return        The specific key counter.

 @Cautions      Allowed only following FM_PCD_CcSetNode() not only of the relevant node but also
                the node that points to this node
*//***************************************************************************/
uint32_t FM_PCD_CcNodeGetKeyCounter(t_Handle h_FmPcd, t_Handle h_CcNode, uint8_t keyIndex);

/**************************************************************************//**
 @Function      FM_PCD_PlcrSetProfile

 @Description   Sets a profile entry in the policer profile table.
                The routine overrides any existing value.

 @Param[in]     h_FmPcd           A handle to an FM PCD Module.
 @Param[in]     p_Profile         A structure of parameters for defining a
                                  policer profile entry.

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_PlcrSetProfile(t_Handle                  h_FmPcd,
                               t_FmPcdPlcrProfileParams  *p_Profile);

/**************************************************************************//**
 @Function      FM_PCD_PlcrDeleteProfile

 @Description   Delete a profile entry in the policer profile table.
                The routine set entry to invalid.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_Profile       A handle to the profile.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_PlcrDeleteProfile(t_Handle h_FmPcd, t_Handle h_Profile);

/**************************************************************************//**
 @Function      FM_PCD_PlcrGetProfileCounter

 @Description   Sets an entry in the classification plan.
                The routine overrides any existing value.

 @Param[in]     h_FmPcd             A handle to an FM PCD Module.
 @Param[in]     h_Profile       A handle to the profile.
 @Param[in]     counter             Counter selector.

 @Return        specific counter value.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
uint32_t FM_PCD_PlcrGetProfileCounter(t_Handle h_FmPcd, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter);

/**************************************************************************//**
 @Function      FM_PCD_PlcrSetProfileCounter

 @Description   Sets an entry in the classification plan.
                The routine overrides any existing value.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_Profile       A handle to the profile.
 @Param[in]     counter         Counter selector.
 @Param[in]     value           value to set counter with.

 @Return        E_OK on success; Error code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error FM_PCD_PlcrSetProfileCounter(t_Handle h_FmPcd, t_Handle h_Profile, e_FmPcdPlcrProfileCounters counter, uint32_t value);

#if defined(FM_CAPWAP_SUPPORT)
/**************************************************************************//**
 @Function      FM_PCD_ManipSetNode

 @Description   This routine should be called for defining a manipulation
                node. A manipulation node must be defined before the CC node
                that precedes it.

 @Param[in]     h_FmPcd             FM PCD module descriptor.
 @Param[in]     p_FmPcdManipParams  A structure of parameters defining the manipulation

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_ManipSetNode(t_Handle h_FmPcd, t_FmPcdManipParams *p_FmPcdManipParams);

/**************************************************************************//**
 @Function      FM_PCD_ManipDeleteNode

 @Description   Delete an existing manip node.

 @Param[in]     h_FmPcd         A handle to an FM PCD Module.
 @Param[in]     h_HdrManipNode  A handle to a Manip node.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Error  FM_PCD_ManipDeleteNode(t_Handle h_FmPcd, t_Handle h_HdrManipNode);
#endif /* defined(FM_CAPWAP_SUPPORT) || ... */


#ifdef FM_CAPWAP_SUPPORT
/**************************************************************************//**
 @Function      FM_PCD_StatisticsSetNode

 @Description   This routine should be called for defining a statistics
                node.

 @Param[in]     h_FmPcd             FM PCD module descriptor.
 @Param[in]     p_FmPcdstatsParams  A structure of parameters defining the statistics

 @Return        A handle to the initialized object on success; NULL code otherwise.

 @Cautions      Allowed only following FM_PCD_Init().
*//***************************************************************************/
t_Handle FM_PCD_StatisticsSetNode(t_Handle h_FmPcd, t_FmPcdStatsParams *p_FmPcdstatsParams);
#endif /* FM_CAPWAP_SUPPORT */

/** @} */ /* end of FM_PCD_Runtime_tree_buildgrp group */
/** @} */ /* end of FM_PCD_Runtime_grp group */
/** @} */ /* end of FM_PCD_grp group */
/** @} */ /* end of FM_grp group */



#endif /* __FM_PCD_EXT */
