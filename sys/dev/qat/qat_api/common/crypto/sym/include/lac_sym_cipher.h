/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 *****************************************************************************
 * @file lac_sym_cipher.h
 *
 * @defgroup  LacCipher  Cipher
 *
 * @ingroup LacSym
 *
 * API functions of the cipher component
 *
 * @lld_start
 * @lld_overview
 * There is a single \ref icp_LacSym "Symmetric LAC API" for hash, cipher,
 * auth encryption and algorithm chaining. This API is implemented by the
 * \ref LacSym "Symmetric" module. It demultiplexes calls to this API into
 * their basic operation and does some common parameter checking and deals
 * with accesses to the session table.
 *
 * The cipher component supports data encryption/decryption using the AES, DES,
 * and Triple-DES cipher algorithms, in ECB, CBC and CTR modes.  The ARC4 stream
 * cipher algorithm is also supported.  Data may be provided as a full packet,
 * or as a sequence of partial packets.  The result of the operation can be
 * written back to the source buffer (in-place) or to a seperate output buffer
 * (out-of-place).  Data must be encapsulated in ICP buffers.
 *
 * The cipher component is responsible for implementing the cipher-specific
 * functionality for registering and de-registering a session, for the perform
 * operation and for processing the QAT responses to cipher requests. Statistics
 * are maintained for cipher in the symmetric \ref CpaCySymStats64 "stats"
 * structure. This module has been seperated out into two. The cipher QAT module
 * deals entirely with QAT data structures. The cipher module itself has minimal
 * exposure to the QAT data structures.
 *
 * @lld_dependencies
 * - \ref LacCommon
 * - \ref LacSymQat "Symmetric QAT": Hash uses the lookup table provided by
 *   this module to validate user input. Hash also uses this module to build
 *   the hash QAT request message, request param structure, populate the
 *   content descriptor, allocate and populate the hash state prefix buffer.
 *   Hash also registers its function to process the QAT response with this
 *   module.
 * - OSAL : For memory functions, atomics and locking
 *
 * @lld_module_algorithms
 * In general, all the cipher algorithms supported by this component are
 * implemented entirely by the QAT.  However, in the case of the ARC4 algorithm,
 * it was deemed more efficient to carry out some processing on IA.  During
 * session registration, an initial state is derived from the base key provided
 * by the user, using a simple ARC4 Key Scheduling Algorithm (KSA). Then the
 * base key is discarded, but the state is maintained for the duration of the
 * session.
 *
 * The ARC4 key scheduling algorithm (KSA) is specified as follows
 * (taken from http://en.wikipedia.org/wiki/RC4_(cipher)):
 * \code
 * for i from 0 to 255
 *     S[i] := i
 * endfor
 * j := 0
 * for i from 0 to 255
 *     j := (j + S[i] + key[i mod keylength]) mod 256
 *     swap(S[i],S[j])
 * endfor
 * \endcode
 *
 * On registration of a new ARC4 session, the user provides a base key of any
 * length from 1 to 256 bytes.  This algorithm produces the initial ARC4 state
 * (key matrix + i & j index values) from that base key.  This ARC4 state is
 * used as input for each ARC4 cipher operation in that session, and is updated
 * by the QAT after each operation.  The ARC4 state is stored in a session
 * descriptor, and it's memory is freed when the session is deregistered.
 *
 * <b>Block Vs. Stream Ciphers</b>\n
 * Block ciphers are treated slightly differently than Stream ciphers by this
 * cipher component.  Supported stream ciphers consist of AES and
 * TripleDES algorithms in CTR mode, and ARC4. The 2 primary differences are:
 * - Data buffers for block ciphers are required to be a multiple of the
 *   block size defined for the algorithm (e.g. 8 bytes for DES).  For stream
 *   ciphers, there is no such restriction.
 * - For stream ciphers, decryption is performed by setting the QAT hardware
 *   to encryption mode.
 *
 * <b>Memory address alignment of data buffers </b>\n
 * The QAT requires that most data buffers are aligned on an 8-byte memory
 * address boundary (64-byte boundary for optimum performance).  For Cipher,
 * this applies to the cipher key buffer passed in the Content Descriptor,
 * and the IV/State buffer passed in the Request Parameters block in each
 * request.  Both of these buffers are provided by the user.  It does not
 * apply to the cipher source/destination data buffers.
 * Alignment of the key buffer is ensured because the key is always copied
 * from the user provided buffer into a new (aligned) buffer for the QAT
 * (the hardware setup block, which configures the QAT slice).  This is done
 * once only during session registration, and the user's key buffer can be
 * effectively discarded after that.
 * The IV/State buffer is provided per-request by the user, so it is recommended
 * to the user to provide aligned buffers for optimal performance.  In the case
 * where an unaligned buffer is provided, a new temporary buffer is allocated
 * and the user's IV/State data is copied into this buffer.  The aligned buffer
 * is then passed to the QAT in the request.  In the response callback, if the
 * IV was updated by the QAT, the contents are copied back to the user's buffer
 * and the temporary buffer is freed.
 *
 * @lld_process_context
 *
 * Session Register Sequence Diagram: For ARC4 cipher algorithm
 * \msc
 *  APP [label="Application"], SYM [label="Symmetric LAC"],
 *  Achain [label="Alg chain"], Cipher, SQAT [label="Symmetric QAT"];
 *
 *  APP=>SYM [ label = "cpaCySymInitSession(cbFunc)",
 *             URL="\ref cpaCySymInitSession()"] ;
 *  SYM=>SYM [ label = "LacSymSession_ParamCheck()",
 *             URL="\ref LacSymSession_ParamCheck()"];
 *  SYM=>Achain [ label = "LacAlgChain_SessionInit()",
 *                URL="\ref LacAlgChain_SessionInit()"];
 *  Achain=>Cipher [ label = "LacCipher_SessionSetupDataCheck()",
 *               URL="\ref LacCipher_SessionSetupDataCheck()"];
 *  Achain<<Cipher [ label="return"];
 *  Achain=>SQAT [ label = "LacSymQat_CipherContentDescPopulate()",
 *               URL="\ref LacSymQat_CipherContentDescPopulate()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>SQAT [ label = "LacSymQat_CipherArc4StateInit()",
 *               URL="\ref LacSymQat_CipherArc4StateInit()"];
 *  Achain<<SQAT [ label="return"];
 *  SYM<<Achain [ label = "status" ];
 *  SYM=>SYM [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  APP<<SYM [label = "status"];
 * \endmsc
 *
 * Perform Sequence Diagram: TripleDES CBC-mode encryption, in-place full
 *packet, asynchronous mode \msc APP [label="Application"], SYM
 *[label="Symmetric LAC"], SC [label="Symmetric Common"], Achain [label="Alg
 *chain"], Cipher, SQAT [label="Symmetric QAT"], BUF [label="LAC Buffer Desc"],
 *SYMQ [label="Symmetric Queue"], SYMCB [label="Symmetric Callback"], LMP
 *[label="LAC Mem Pool"], QATCOMMS [label="QAT Comms"];
 *
 *  APP=>SYM [ label = "cpaCySymPerformOp()",
 *             URL="\ref cpaCySymPerformOp()"] ;
 *  SYM=>SYM [ label = "LacSym_Perform()",
 *            URL="\ref LacSym_Perform()"];
 *  SYM=>SYM [ label = "LacSymPerform_BufferParamCheck()",
 *            URL="\ref LacSymPerform_BufferParamCheck()"];
 *  SYM<<SYM [ label = "status"];
 *  SYM=>Achain [ label = "LacAlgChain_Perform()",
 *                URL="\ref LacCipher()"];
 *  Achain=>Cipher [ label = "LacCipher_PerformParamCheck()",
 *                   URL="\ref LacCipher_PerformParamCheck()"];
 *  Achain<<Cipher [ label="status"];
 *  Achain=>LMP [label="Lac_MemPoolEntryAlloc()",
 *              URL="\ref Lac_MemPoolEntryAlloc()"];
 *  Achain<<LMP [label="return"];
 *  Achain=>Cipher [ label = "LacCipher_PerformIvCheckAndAlign()",
 *                   URL="\ref LacCipher_PerformIvCheckAndAlign()"];
 *  Achain<<Cipher [ label="status"];
 *  Achain=>SQAT [ label = "LacSymQat_CipherRequestParamsPopulate()",
 *               URL="\ref LacSymQat_CipherRequestParamsPopulate()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>BUF [ label = "LacBuffDesc_BufferListDescWrite()",
 *                URL = "\ref LacBuffDesc_BufferListDescWrite()"];
 *  Achain<<BUF [ label="return"];
 *  Achain=>SQAT [ label = "SalQatMsg_CmnMsgAndReqParamsPopulate()",
 *               URL="\ref SalQatMsg_CmnMsgAndReqParamsPopulate()"];
 *  Achain<<SQAT [ label="return"];
 *  Achain=>SYMQ [ label = "LacSymQueue_RequestSend()",
 *             URL="\ref LacSymQueue_RequestSend()"];
 *  SYMQ=>QATCOMMS [ label = "QatComms_MsgSend()",
 *                   URL="\ref QatComms_MsgSend()"];
 *  SYMQ<<QATCOMMS [ label="status"];
 *  Achain<<SYMQ [ label="status"];
 *  SYM<<Achain[ label="status"];
 *  SYM=>SYM [ label = "LacSym_PartialPacketStateUpdate()",
 *            URL="\ref LacSym_PartialPacketStateUpdate()"];
 *  SYM<<SYM [ label = "return"];
 *  SYM=>SC [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  SYM<<SC [ label="return"];
 *  SYM<<SYM [ label = "status"];
 *  APP<<SYM [label = "status"];
 *  ... [label = "QAT processing the request and generates response"];
 *  ...;
 *  QATCOMMS=>QATCOMMS [label ="QatComms_ResponseMsgHandler()",
 *                       URL="\ref QatComms_ResponseMsgHandler()"];
 *  QATCOMMS=>SQAT [label ="LacSymQat_SymRespHandler()",
 *                   URL="\ref LacSymQat_SymRespHandler()"];
 *  SQAT=>SYMCB [label="LacSymCb_ProcessCallback()",
 *              URL="\ref LacSymCb_ProcessCallback()"];
 *  SYMCB=>SYMCB [label="LacSymCb_ProcessCallbackInternal()",
 *              URL="\ref LacSymCb_ProcessCallbackInternal()"];
 *  SYMCB=>LMP [label="Lac_MemPoolEntryFree()",
 *              URL="\ref Lac_MemPoolEntryFree()"];
 *  SYMCB<<LMP [label="return"];
 *  SYMCB=>SC [label = "LAC_SYM_STAT_INC", URL="\ref LAC_SYM_STAT_INC"];
 *  SYMCB<<SC [label = "return"];
 *  SYMCB=>APP [label="cbFunc"];
 *  SYMCB<<APP [label="return"];
 *  SQAT<<SYMCB [label="return"];
 *  QATCOMMS<<SQAT [label="return"];
 * \endmsc
 *
 * #See the sequence diagram for cpaCySymInitSession()
 *
 * @lld_end
 *
 *****************************************************************************/

/***************************************************************************/

#ifndef LAC_SYM_CIPHER_H
#define LAC_SYM_CIPHER_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

#include "lac_session.h"
#include "lac_sym.h"

/*
 * WARNING: There are no checks done on the parameters of the functions in
 * this file. The expected values of the parameters are documented and it is
 * up to the caller to provide valid values.
 */

/***************************************************************************/

/**
 *****************************************************************************
 * @ingroup LacCipher
 *      Cipher session setup data check
 *
 * @description
 *      This function will check any algorithm-specific fields
 *      in the session cipher setup data structure
 *
 * @param[in] pCipherSetupData       Pointer to session cipher context
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter.
 *
 *****************************************************************************/
CpaStatus LacCipher_SessionSetupDataCheck(
    const CpaCySymCipherSetupData *pCipherSetupData);

/**
*******************************************************************************
* @ingroup LacCipher
*      Function that checks the perform common parameters for cipher
*
* @description
*      This function checks the perform parameters for cipher operations
*
* @param[in] cipherAlgorithm  read only pointer to cipher context structure
*
* @param[in] pOpData          read only pointer to user-supplied data for this
*                             cipher operation
* @param[in] packetLen        read only length of data in buffer
*
* @retval CPA_STATUS_SUCCESS        Success
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter
*
*****************************************************************************/
CpaStatus LacCipher_PerformParamCheck(CpaCySymCipherAlgorithm cipherAlgorithm,
				      const CpaCySymOpData *pOpData,
				      const Cpa64U packetLen);

/**
 *****************************************************************************
 * @ingroup LacCipher
 *      Cipher perform IV check
 *
 * @description
 *      This function will perform algorithm-specific checks on the
 *      cipher Initialisation Vector data provided by the user.
 *
 * @param[in] pCbCookie         Pointer to struct containing internal cookie
 *                              data for the operation
 * @param[in] qatPacketType     QAT partial packet type (start/mid/end/none)
 * @param[out] ppIvBuffer       Returns a pointer to an IV buffer.
 *
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter.
 *
 * @see LacCipher_Perform(), LacCipher_IvBufferRestore()
 *
 * @note LacCipher_IvBufferRestore() must be called when the request is
 *       completed to update the users IV buffer, only in the case of partial
 *       packet requests
 *
 *****************************************************************************/
CpaStatus LacCipher_PerformIvCheck(sal_service_t *pService,
				   lac_sym_bulk_cookie_t *pCbCookie,
				   Cpa32U qatPacketType,
				   Cpa8U **ppIvBuffer);

#endif /* LAC_SYM_CIPHER_H */
