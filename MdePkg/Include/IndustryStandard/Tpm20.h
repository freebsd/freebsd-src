/** @file
  TPM2.0 Specification data structures
  (Trusted Platform Module Library Specification, Family "2.0", Level 00, Revision 00.96,
  @http://www.trustedcomputinggroup.org/resources/tpm_library_specification)

  Check http://trustedcomputinggroup.org for latest specification updates.

Copyright (c) 2013 - 2015, Intel Corporation. All rights reserved. <BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _TPM20_H_
#define _TPM20_H_

#include <IndustryStandard/Tpm12.h>

#pragma pack (1)

// Annex A Algorithm Constants

// Table 205 - Defines for SHA1 Hash Values
#define SHA1_DIGEST_SIZE 20
#define SHA1_BLOCK_SIZE  64

// Table 206 - Defines for SHA256 Hash Values
#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

// Table 207 - Defines for SHA384 Hash Values
#define SHA384_DIGEST_SIZE 48
#define SHA384_BLOCK_SIZE  128

// Table 208 - Defines for SHA512 Hash Values
#define SHA512_DIGEST_SIZE 64
#define SHA512_BLOCK_SIZE  128

// Table 209 - Defines for SM3_256 Hash Values
#define SM3_256_DIGEST_SIZE 32
#define SM3_256_BLOCK_SIZE  64

// Table 210 - Defines for Architectural Limits Values
#define MAX_SESSION_NUMBER 3

// Annex B Implementation Definitions

// Table 211 - Defines for Logic Values
#define YES   1
#define NO    0
#define SET   1
#define CLEAR 0

// Table 215 - Defines for RSA Algorithm Constants
#define MAX_RSA_KEY_BITS  2048
#define MAX_RSA_KEY_BYTES ((MAX_RSA_KEY_BITS + 7) / 8)

// Table 216 - Defines for ECC Algorithm Constants
#define MAX_ECC_KEY_BITS  256
#define MAX_ECC_KEY_BYTES ((MAX_ECC_KEY_BITS + 7) / 8)

// Table 217 - Defines for AES Algorithm Constants
#define MAX_AES_KEY_BITS         128
#define MAX_AES_BLOCK_SIZE_BYTES 16
#define MAX_AES_KEY_BYTES        ((MAX_AES_KEY_BITS + 7) / 8)

// Table 218 - Defines for SM4 Algorithm Constants
#define MAX_SM4_KEY_BITS         128
#define MAX_SM4_BLOCK_SIZE_BYTES 16
#define MAX_SM4_KEY_BYTES        ((MAX_SM4_KEY_BITS + 7) / 8)

// Table 219 - Defines for Symmetric Algorithm Constants
#define MAX_SYM_KEY_BITS   MAX_AES_KEY_BITS
#define MAX_SYM_KEY_BYTES  MAX_AES_KEY_BYTES
#define MAX_SYM_BLOCK_SIZE MAX_AES_BLOCK_SIZE_BYTES

// Table 220 - Defines for Implementation Values
typedef UINT16                        BSIZE;
#define BUFFER_ALIGNMENT              4
#define IMPLEMENTATION_PCR            24
#define PLATFORM_PCR                  24
#define DRTM_PCR                      17
#define NUM_LOCALITIES                5
#define MAX_HANDLE_NUM                3
#define MAX_ACTIVE_SESSIONS           64
typedef UINT16                        CONTEXT_SLOT;
typedef UINT64                        CONTEXT_COUNTER;
#define MAX_LOADED_SESSIONS           3
#define MAX_SESSION_NUM               3
#define MAX_LOADED_OBJECTS            3
#define MIN_EVICT_OBJECTS             2
#define PCR_SELECT_MIN                ((PLATFORM_PCR + 7) / 8)
#define PCR_SELECT_MAX                ((IMPLEMENTATION_PCR + 7) / 8)
#define NUM_POLICY_PCR_GROUP          1
#define NUM_AUTHVALUE_PCR_GROUP       1
#define MAX_CONTEXT_SIZE              4000
#define MAX_DIGEST_BUFFER             1024
#define MAX_NV_INDEX_SIZE             1024
#define MAX_CAP_BUFFER                1024
#define NV_MEMORY_SIZE                16384
#define NUM_STATIC_PCR                16
#define MAX_ALG_LIST_SIZE             64
#define TIMER_PRESCALE                100000
#define PRIMARY_SEED_SIZE             32
#define CONTEXT_ENCRYPT_ALG           TPM_ALG_AES
#define CONTEXT_ENCRYPT_KEY_BITS      MAX_SYM_KEY_BITS
#define CONTEXT_ENCRYPT_KEY_BYTES     ((CONTEXT_ENCRYPT_KEY_BITS + 7) / 8)
#define CONTEXT_INTEGRITY_HASH_ALG    TPM_ALG_SHA256
#define CONTEXT_INTEGRITY_HASH_SIZE   SHA256_DIGEST_SIZE
#define PROOF_SIZE                    CONTEXT_INTEGRITY_HASH_SIZE
#define NV_CLOCK_UPDATE_INTERVAL      12
#define NUM_POLICY_PCR                1
#define MAX_COMMAND_SIZE              4096
#define MAX_RESPONSE_SIZE             4096
#define ORDERLY_BITS                  8
#define MAX_ORDERLY_COUNT             ((1 << ORDERLY_BITS) - 1)
#define ALG_ID_FIRST                  TPM_ALG_FIRST
#define ALG_ID_LAST                   TPM_ALG_LAST
#define MAX_SYM_DATA                  128
#define MAX_RNG_ENTROPY_SIZE          64
#define RAM_INDEX_SPACE               512
#define RSA_DEFAULT_PUBLIC_EXPONENT   0x00010001
#define CRT_FORMAT_RSA                YES
#define PRIVATE_VENDOR_SPECIFIC_BYTES ((MAX_RSA_KEY_BYTES / 2) * ( 3 + CRT_FORMAT_RSA * 2))

// Capability related MAX_ value
#define MAX_CAP_DATA       (MAX_CAP_BUFFER - sizeof(TPM_CAP) - sizeof(UINT32))
#define MAX_CAP_ALGS       (MAX_CAP_DATA / sizeof(TPMS_ALG_PROPERTY))
#define MAX_CAP_HANDLES    (MAX_CAP_DATA / sizeof(TPM_HANDLE))
#define MAX_CAP_CC         (MAX_CAP_DATA / sizeof(TPM_CC))
#define MAX_TPM_PROPERTIES (MAX_CAP_DATA / sizeof(TPMS_TAGGED_PROPERTY))
#define MAX_PCR_PROPERTIES (MAX_CAP_DATA / sizeof(TPMS_TAGGED_PCR_SELECT))
#define MAX_ECC_CURVES     (MAX_CAP_DATA / sizeof(TPM_ECC_CURVE))

//
// Always set 5 here, because we want to support all hash algo in BIOS.
//
#define HASH_COUNT 5

// 5 Base Types

// Table 3 - Definition of Base Types
typedef UINT8 BYTE;

// Table 4 - Definition of Types for Documentation Clarity
//
// NOTE: Comment because it has same name as TPM1.2 (value is same, so not runtime issue)
//
//typedef UINT32 TPM_ALGORITHM_ID;
//typedef UINT32 TPM_MODIFIER_INDICATOR;
typedef UINT32 TPM_AUTHORIZATION_SIZE;
typedef UINT32 TPM_PARAMETER_SIZE;
typedef UINT16 TPM_KEY_SIZE;
typedef UINT16 TPM_KEY_BITS;

// 6 Constants

// Table 6 - TPM_GENERATED Constants
typedef UINT32 TPM_GENERATED;
#define TPM_GENERATED_VALUE (TPM_GENERATED)(0xff544347)

// Table 7 - TPM_ALG_ID Constants
typedef UINT16 TPM_ALG_ID;
//
// NOTE: Comment some algo which has same name as TPM1.2 (value is same, so not runtime issue)
//
#define TPM_ALG_ERROR          (TPM_ALG_ID)(0x0000)
#define TPM_ALG_FIRST          (TPM_ALG_ID)(0x0001)
//#define TPM_ALG_RSA            (TPM_ALG_ID)(0x0001)
//#define TPM_ALG_SHA            (TPM_ALG_ID)(0x0004)
#define TPM_ALG_SHA1           (TPM_ALG_ID)(0x0004)
//#define TPM_ALG_HMAC           (TPM_ALG_ID)(0x0005)
#define TPM_ALG_AES            (TPM_ALG_ID)(0x0006)
//#define TPM_ALG_MGF1           (TPM_ALG_ID)(0x0007)
#define TPM_ALG_KEYEDHASH      (TPM_ALG_ID)(0x0008)
//#define TPM_ALG_XOR            (TPM_ALG_ID)(0x000A)
#define TPM_ALG_SHA256         (TPM_ALG_ID)(0x000B)
#define TPM_ALG_SHA384         (TPM_ALG_ID)(0x000C)
#define TPM_ALG_SHA512         (TPM_ALG_ID)(0x000D)
#define TPM_ALG_NULL           (TPM_ALG_ID)(0x0010)
#define TPM_ALG_SM3_256        (TPM_ALG_ID)(0x0012)
#define TPM_ALG_SM4            (TPM_ALG_ID)(0x0013)
#define TPM_ALG_RSASSA         (TPM_ALG_ID)(0x0014)
#define TPM_ALG_RSAES          (TPM_ALG_ID)(0x0015)
#define TPM_ALG_RSAPSS         (TPM_ALG_ID)(0x0016)
#define TPM_ALG_OAEP           (TPM_ALG_ID)(0x0017)
#define TPM_ALG_ECDSA          (TPM_ALG_ID)(0x0018)
#define TPM_ALG_ECDH           (TPM_ALG_ID)(0x0019)
#define TPM_ALG_ECDAA          (TPM_ALG_ID)(0x001A)
#define TPM_ALG_SM2            (TPM_ALG_ID)(0x001B)
#define TPM_ALG_ECSCHNORR      (TPM_ALG_ID)(0x001C)
#define TPM_ALG_ECMQV          (TPM_ALG_ID)(0x001D)
#define TPM_ALG_KDF1_SP800_56a (TPM_ALG_ID)(0x0020)
#define TPM_ALG_KDF2           (TPM_ALG_ID)(0x0021)
#define TPM_ALG_KDF1_SP800_108 (TPM_ALG_ID)(0x0022)
#define TPM_ALG_ECC            (TPM_ALG_ID)(0x0023)
#define TPM_ALG_SYMCIPHER      (TPM_ALG_ID)(0x0025)
#define TPM_ALG_CTR            (TPM_ALG_ID)(0x0040)
#define TPM_ALG_OFB            (TPM_ALG_ID)(0x0041)
#define TPM_ALG_CBC            (TPM_ALG_ID)(0x0042)
#define TPM_ALG_CFB            (TPM_ALG_ID)(0x0043)
#define TPM_ALG_ECB            (TPM_ALG_ID)(0x0044)
#define TPM_ALG_LAST           (TPM_ALG_ID)(0x0044)

// Table 8 - TPM_ECC_CURVE Constants
typedef UINT16 TPM_ECC_CURVE;
#define TPM_ECC_NONE      (TPM_ECC_CURVE)(0x0000)
#define TPM_ECC_NIST_P192 (TPM_ECC_CURVE)(0x0001)
#define TPM_ECC_NIST_P224 (TPM_ECC_CURVE)(0x0002)
#define TPM_ECC_NIST_P256 (TPM_ECC_CURVE)(0x0003)
#define TPM_ECC_NIST_P384 (TPM_ECC_CURVE)(0x0004)
#define TPM_ECC_NIST_P521 (TPM_ECC_CURVE)(0x0005)
#define TPM_ECC_BN_P256   (TPM_ECC_CURVE)(0x0010)
#define TPM_ECC_BN_P638   (TPM_ECC_CURVE)(0x0011)
#define TPM_ECC_SM2_P256  (TPM_ECC_CURVE)(0x0020)

// Table 11 - TPM_CC Constants (Numeric Order)
typedef UINT32 TPM_CC;
#define TPM_CC_FIRST                      (TPM_CC)(0x0000011F)
#define TPM_CC_PP_FIRST                   (TPM_CC)(0x0000011F)
#define TPM_CC_NV_UndefineSpaceSpecial    (TPM_CC)(0x0000011F)
#define TPM_CC_EvictControl               (TPM_CC)(0x00000120)
#define TPM_CC_HierarchyControl           (TPM_CC)(0x00000121)
#define TPM_CC_NV_UndefineSpace           (TPM_CC)(0x00000122)
#define TPM_CC_ChangeEPS                  (TPM_CC)(0x00000124)
#define TPM_CC_ChangePPS                  (TPM_CC)(0x00000125)
#define TPM_CC_Clear                      (TPM_CC)(0x00000126)
#define TPM_CC_ClearControl               (TPM_CC)(0x00000127)
#define TPM_CC_ClockSet                   (TPM_CC)(0x00000128)
#define TPM_CC_HierarchyChangeAuth        (TPM_CC)(0x00000129)
#define TPM_CC_NV_DefineSpace             (TPM_CC)(0x0000012A)
#define TPM_CC_PCR_Allocate               (TPM_CC)(0x0000012B)
#define TPM_CC_PCR_SetAuthPolicy          (TPM_CC)(0x0000012C)
#define TPM_CC_PP_Commands                (TPM_CC)(0x0000012D)
#define TPM_CC_SetPrimaryPolicy           (TPM_CC)(0x0000012E)
#define TPM_CC_FieldUpgradeStart          (TPM_CC)(0x0000012F)
#define TPM_CC_ClockRateAdjust            (TPM_CC)(0x00000130)
#define TPM_CC_CreatePrimary              (TPM_CC)(0x00000131)
#define TPM_CC_NV_GlobalWriteLock         (TPM_CC)(0x00000132)
#define TPM_CC_PP_LAST                    (TPM_CC)(0x00000132)
#define TPM_CC_GetCommandAuditDigest      (TPM_CC)(0x00000133)
#define TPM_CC_NV_Increment               (TPM_CC)(0x00000134)
#define TPM_CC_NV_SetBits                 (TPM_CC)(0x00000135)
#define TPM_CC_NV_Extend                  (TPM_CC)(0x00000136)
#define TPM_CC_NV_Write                   (TPM_CC)(0x00000137)
#define TPM_CC_NV_WriteLock               (TPM_CC)(0x00000138)
#define TPM_CC_DictionaryAttackLockReset  (TPM_CC)(0x00000139)
#define TPM_CC_DictionaryAttackParameters (TPM_CC)(0x0000013A)
#define TPM_CC_NV_ChangeAuth              (TPM_CC)(0x0000013B)
#define TPM_CC_PCR_Event                  (TPM_CC)(0x0000013C)
#define TPM_CC_PCR_Reset                  (TPM_CC)(0x0000013D)
#define TPM_CC_SequenceComplete           (TPM_CC)(0x0000013E)
#define TPM_CC_SetAlgorithmSet            (TPM_CC)(0x0000013F)
#define TPM_CC_SetCommandCodeAuditStatus  (TPM_CC)(0x00000140)
#define TPM_CC_FieldUpgradeData           (TPM_CC)(0x00000141)
#define TPM_CC_IncrementalSelfTest        (TPM_CC)(0x00000142)
#define TPM_CC_SelfTest                   (TPM_CC)(0x00000143)
#define TPM_CC_Startup                    (TPM_CC)(0x00000144)
#define TPM_CC_Shutdown                   (TPM_CC)(0x00000145)
#define TPM_CC_StirRandom                 (TPM_CC)(0x00000146)
#define TPM_CC_ActivateCredential         (TPM_CC)(0x00000147)
#define TPM_CC_Certify                    (TPM_CC)(0x00000148)
#define TPM_CC_PolicyNV                   (TPM_CC)(0x00000149)
#define TPM_CC_CertifyCreation            (TPM_CC)(0x0000014A)
#define TPM_CC_Duplicate                  (TPM_CC)(0x0000014B)
#define TPM_CC_GetTime                    (TPM_CC)(0x0000014C)
#define TPM_CC_GetSessionAuditDigest      (TPM_CC)(0x0000014D)
#define TPM_CC_NV_Read                    (TPM_CC)(0x0000014E)
#define TPM_CC_NV_ReadLock                (TPM_CC)(0x0000014F)
#define TPM_CC_ObjectChangeAuth           (TPM_CC)(0x00000150)
#define TPM_CC_PolicySecret               (TPM_CC)(0x00000151)
#define TPM_CC_Rewrap                     (TPM_CC)(0x00000152)
#define TPM_CC_Create                     (TPM_CC)(0x00000153)
#define TPM_CC_ECDH_ZGen                  (TPM_CC)(0x00000154)
#define TPM_CC_HMAC                       (TPM_CC)(0x00000155)
#define TPM_CC_Import                     (TPM_CC)(0x00000156)
#define TPM_CC_Load                       (TPM_CC)(0x00000157)
#define TPM_CC_Quote                      (TPM_CC)(0x00000158)
#define TPM_CC_RSA_Decrypt                (TPM_CC)(0x00000159)
#define TPM_CC_HMAC_Start                 (TPM_CC)(0x0000015B)
#define TPM_CC_SequenceUpdate             (TPM_CC)(0x0000015C)
#define TPM_CC_Sign                       (TPM_CC)(0x0000015D)
#define TPM_CC_Unseal                     (TPM_CC)(0x0000015E)
#define TPM_CC_PolicySigned               (TPM_CC)(0x00000160)
#define TPM_CC_ContextLoad                (TPM_CC)(0x00000161)
#define TPM_CC_ContextSave                (TPM_CC)(0x00000162)
#define TPM_CC_ECDH_KeyGen                (TPM_CC)(0x00000163)
#define TPM_CC_EncryptDecrypt             (TPM_CC)(0x00000164)
#define TPM_CC_FlushContext               (TPM_CC)(0x00000165)
#define TPM_CC_LoadExternal               (TPM_CC)(0x00000167)
#define TPM_CC_MakeCredential             (TPM_CC)(0x00000168)
#define TPM_CC_NV_ReadPublic              (TPM_CC)(0x00000169)
#define TPM_CC_PolicyAuthorize            (TPM_CC)(0x0000016A)
#define TPM_CC_PolicyAuthValue            (TPM_CC)(0x0000016B)
#define TPM_CC_PolicyCommandCode          (TPM_CC)(0x0000016C)
#define TPM_CC_PolicyCounterTimer         (TPM_CC)(0x0000016D)
#define TPM_CC_PolicyCpHash               (TPM_CC)(0x0000016E)
#define TPM_CC_PolicyLocality             (TPM_CC)(0x0000016F)
#define TPM_CC_PolicyNameHash             (TPM_CC)(0x00000170)
#define TPM_CC_PolicyOR                   (TPM_CC)(0x00000171)
#define TPM_CC_PolicyTicket               (TPM_CC)(0x00000172)
#define TPM_CC_ReadPublic                 (TPM_CC)(0x00000173)
#define TPM_CC_RSA_Encrypt                (TPM_CC)(0x00000174)
#define TPM_CC_StartAuthSession           (TPM_CC)(0x00000176)
#define TPM_CC_VerifySignature            (TPM_CC)(0x00000177)
#define TPM_CC_ECC_Parameters             (TPM_CC)(0x00000178)
#define TPM_CC_FirmwareRead               (TPM_CC)(0x00000179)
#define TPM_CC_GetCapability              (TPM_CC)(0x0000017A)
#define TPM_CC_GetRandom                  (TPM_CC)(0x0000017B)
#define TPM_CC_GetTestResult              (TPM_CC)(0x0000017C)
#define TPM_CC_Hash                       (TPM_CC)(0x0000017D)
#define TPM_CC_PCR_Read                   (TPM_CC)(0x0000017E)
#define TPM_CC_PolicyPCR                  (TPM_CC)(0x0000017F)
#define TPM_CC_PolicyRestart              (TPM_CC)(0x00000180)
#define TPM_CC_ReadClock                  (TPM_CC)(0x00000181)
#define TPM_CC_PCR_Extend                 (TPM_CC)(0x00000182)
#define TPM_CC_PCR_SetAuthValue           (TPM_CC)(0x00000183)
#define TPM_CC_NV_Certify                 (TPM_CC)(0x00000184)
#define TPM_CC_EventSequenceComplete      (TPM_CC)(0x00000185)
#define TPM_CC_HashSequenceStart          (TPM_CC)(0x00000186)
#define TPM_CC_PolicyPhysicalPresence     (TPM_CC)(0x00000187)
#define TPM_CC_PolicyDuplicationSelect    (TPM_CC)(0x00000188)
#define TPM_CC_PolicyGetDigest            (TPM_CC)(0x00000189)
#define TPM_CC_TestParms                  (TPM_CC)(0x0000018A)
#define TPM_CC_Commit                     (TPM_CC)(0x0000018B)
#define TPM_CC_PolicyPassword             (TPM_CC)(0x0000018C)
#define TPM_CC_ZGen_2Phase                (TPM_CC)(0x0000018D)
#define TPM_CC_EC_Ephemeral               (TPM_CC)(0x0000018E)
#define TPM_CC_LAST                       (TPM_CC)(0x0000018E)

// Table 15 - TPM_RC Constants (Actions)
typedef UINT32 TPM_RC;
#define TPM_RC_SUCCESS           (TPM_RC)(0x000)
#define TPM_RC_BAD_TAG           (TPM_RC)(0x030)
#define RC_VER1                  (TPM_RC)(0x100)
#define TPM_RC_INITIALIZE        (TPM_RC)(RC_VER1 + 0x000)
#define TPM_RC_FAILURE           (TPM_RC)(RC_VER1 + 0x001)
#define TPM_RC_SEQUENCE          (TPM_RC)(RC_VER1 + 0x003)
#define TPM_RC_PRIVATE           (TPM_RC)(RC_VER1 + 0x00B)
#define TPM_RC_HMAC              (TPM_RC)(RC_VER1 + 0x019)
#define TPM_RC_DISABLED          (TPM_RC)(RC_VER1 + 0x020)
#define TPM_RC_EXCLUSIVE         (TPM_RC)(RC_VER1 + 0x021)
#define TPM_RC_AUTH_TYPE         (TPM_RC)(RC_VER1 + 0x024)
#define TPM_RC_AUTH_MISSING      (TPM_RC)(RC_VER1 + 0x025)
#define TPM_RC_POLICY            (TPM_RC)(RC_VER1 + 0x026)
#define TPM_RC_PCR               (TPM_RC)(RC_VER1 + 0x027)
#define TPM_RC_PCR_CHANGED       (TPM_RC)(RC_VER1 + 0x028)
#define TPM_RC_UPGRADE           (TPM_RC)(RC_VER1 + 0x02D)
#define TPM_RC_TOO_MANY_CONTEXTS (TPM_RC)(RC_VER1 + 0x02E)
#define TPM_RC_AUTH_UNAVAILABLE  (TPM_RC)(RC_VER1 + 0x02F)
#define TPM_RC_REBOOT            (TPM_RC)(RC_VER1 + 0x030)
#define TPM_RC_UNBALANCED        (TPM_RC)(RC_VER1 + 0x031)
#define TPM_RC_COMMAND_SIZE      (TPM_RC)(RC_VER1 + 0x042)
#define TPM_RC_COMMAND_CODE      (TPM_RC)(RC_VER1 + 0x043)
#define TPM_RC_AUTHSIZE          (TPM_RC)(RC_VER1 + 0x044)
#define TPM_RC_AUTH_CONTEXT      (TPM_RC)(RC_VER1 + 0x045)
#define TPM_RC_NV_RANGE          (TPM_RC)(RC_VER1 + 0x046)
#define TPM_RC_NV_SIZE           (TPM_RC)(RC_VER1 + 0x047)
#define TPM_RC_NV_LOCKED         (TPM_RC)(RC_VER1 + 0x048)
#define TPM_RC_NV_AUTHORIZATION  (TPM_RC)(RC_VER1 + 0x049)
#define TPM_RC_NV_UNINITIALIZED  (TPM_RC)(RC_VER1 + 0x04A)
#define TPM_RC_NV_SPACE          (TPM_RC)(RC_VER1 + 0x04B)
#define TPM_RC_NV_DEFINED        (TPM_RC)(RC_VER1 + 0x04C)
#define TPM_RC_BAD_CONTEXT       (TPM_RC)(RC_VER1 + 0x050)
#define TPM_RC_CPHASH            (TPM_RC)(RC_VER1 + 0x051)
#define TPM_RC_PARENT            (TPM_RC)(RC_VER1 + 0x052)
#define TPM_RC_NEEDS_TEST        (TPM_RC)(RC_VER1 + 0x053)
#define TPM_RC_NO_RESULT         (TPM_RC)(RC_VER1 + 0x054)
#define TPM_RC_SENSITIVE         (TPM_RC)(RC_VER1 + 0x055)
#define RC_MAX_FM0               (TPM_RC)(RC_VER1 + 0x07F)
#define RC_FMT1                  (TPM_RC)(0x080)
#define TPM_RC_ASYMMETRIC        (TPM_RC)(RC_FMT1 + 0x001)
#define TPM_RC_ATTRIBUTES        (TPM_RC)(RC_FMT1 + 0x002)
#define TPM_RC_HASH              (TPM_RC)(RC_FMT1 + 0x003)
#define TPM_RC_VALUE             (TPM_RC)(RC_FMT1 + 0x004)
#define TPM_RC_HIERARCHY         (TPM_RC)(RC_FMT1 + 0x005)
#define TPM_RC_KEY_SIZE          (TPM_RC)(RC_FMT1 + 0x007)
#define TPM_RC_MGF               (TPM_RC)(RC_FMT1 + 0x008)
#define TPM_RC_MODE              (TPM_RC)(RC_FMT1 + 0x009)
#define TPM_RC_TYPE              (TPM_RC)(RC_FMT1 + 0x00A)
#define TPM_RC_HANDLE            (TPM_RC)(RC_FMT1 + 0x00B)
#define TPM_RC_KDF               (TPM_RC)(RC_FMT1 + 0x00C)
#define TPM_RC_RANGE             (TPM_RC)(RC_FMT1 + 0x00D)
#define TPM_RC_AUTH_FAIL         (TPM_RC)(RC_FMT1 + 0x00E)
#define TPM_RC_NONCE             (TPM_RC)(RC_FMT1 + 0x00F)
#define TPM_RC_PP                (TPM_RC)(RC_FMT1 + 0x010)
#define TPM_RC_SCHEME            (TPM_RC)(RC_FMT1 + 0x012)
#define TPM_RC_SIZE              (TPM_RC)(RC_FMT1 + 0x015)
#define TPM_RC_SYMMETRIC         (TPM_RC)(RC_FMT1 + 0x016)
#define TPM_RC_TAG               (TPM_RC)(RC_FMT1 + 0x017)
#define TPM_RC_SELECTOR          (TPM_RC)(RC_FMT1 + 0x018)
#define TPM_RC_INSUFFICIENT      (TPM_RC)(RC_FMT1 + 0x01A)
#define TPM_RC_SIGNATURE         (TPM_RC)(RC_FMT1 + 0x01B)
#define TPM_RC_KEY               (TPM_RC)(RC_FMT1 + 0x01C)
#define TPM_RC_POLICY_FAIL       (TPM_RC)(RC_FMT1 + 0x01D)
#define TPM_RC_INTEGRITY         (TPM_RC)(RC_FMT1 + 0x01F)
#define TPM_RC_TICKET            (TPM_RC)(RC_FMT1 + 0x020)
#define TPM_RC_RESERVED_BITS     (TPM_RC)(RC_FMT1 + 0x021)
#define TPM_RC_BAD_AUTH          (TPM_RC)(RC_FMT1 + 0x022)
#define TPM_RC_EXPIRED           (TPM_RC)(RC_FMT1 + 0x023)
#define TPM_RC_POLICY_CC         (TPM_RC)(RC_FMT1 + 0x024 )
#define TPM_RC_BINDING           (TPM_RC)(RC_FMT1 + 0x025)
#define TPM_RC_CURVE             (TPM_RC)(RC_FMT1 + 0x026)
#define TPM_RC_ECC_POINT         (TPM_RC)(RC_FMT1 + 0x027)
#define RC_WARN                  (TPM_RC)(0x900)
#define TPM_RC_CONTEXT_GAP       (TPM_RC)(RC_WARN + 0x001)
#define TPM_RC_OBJECT_MEMORY     (TPM_RC)(RC_WARN + 0x002)
#define TPM_RC_SESSION_MEMORY    (TPM_RC)(RC_WARN + 0x003)
#define TPM_RC_MEMORY            (TPM_RC)(RC_WARN + 0x004)
#define TPM_RC_SESSION_HANDLES   (TPM_RC)(RC_WARN + 0x005)
#define TPM_RC_OBJECT_HANDLES    (TPM_RC)(RC_WARN + 0x006)
#define TPM_RC_LOCALITY          (TPM_RC)(RC_WARN + 0x007)
#define TPM_RC_YIELDED           (TPM_RC)(RC_WARN + 0x008)
#define TPM_RC_CANCELED          (TPM_RC)(RC_WARN + 0x009)
#define TPM_RC_TESTING           (TPM_RC)(RC_WARN + 0x00A)
#define TPM_RC_REFERENCE_H0      (TPM_RC)(RC_WARN + 0x010)
#define TPM_RC_REFERENCE_H1      (TPM_RC)(RC_WARN + 0x011)
#define TPM_RC_REFERENCE_H2      (TPM_RC)(RC_WARN + 0x012)
#define TPM_RC_REFERENCE_H3      (TPM_RC)(RC_WARN + 0x013)
#define TPM_RC_REFERENCE_H4      (TPM_RC)(RC_WARN + 0x014)
#define TPM_RC_REFERENCE_H5      (TPM_RC)(RC_WARN + 0x015)
#define TPM_RC_REFERENCE_H6      (TPM_RC)(RC_WARN + 0x016)
#define TPM_RC_REFERENCE_S0      (TPM_RC)(RC_WARN + 0x018)
#define TPM_RC_REFERENCE_S1      (TPM_RC)(RC_WARN + 0x019)
#define TPM_RC_REFERENCE_S2      (TPM_RC)(RC_WARN + 0x01A)
#define TPM_RC_REFERENCE_S3      (TPM_RC)(RC_WARN + 0x01B)
#define TPM_RC_REFERENCE_S4      (TPM_RC)(RC_WARN + 0x01C)
#define TPM_RC_REFERENCE_S5      (TPM_RC)(RC_WARN + 0x01D)
#define TPM_RC_REFERENCE_S6      (TPM_RC)(RC_WARN + 0x01E)
#define TPM_RC_NV_RATE           (TPM_RC)(RC_WARN + 0x020)
#define TPM_RC_LOCKOUT           (TPM_RC)(RC_WARN + 0x021)
#define TPM_RC_RETRY             (TPM_RC)(RC_WARN + 0x022)
#define TPM_RC_NV_UNAVAILABLE    (TPM_RC)(RC_WARN + 0x023)
#define TPM_RC_NOT_USED          (TPM_RC)(RC_WARN + 0x7F)
#define TPM_RC_H                 (TPM_RC)(0x000)
#define TPM_RC_P                 (TPM_RC)(0x040)
#define TPM_RC_S                 (TPM_RC)(0x800)
#define TPM_RC_1                 (TPM_RC)(0x100)
#define TPM_RC_2                 (TPM_RC)(0x200)
#define TPM_RC_3                 (TPM_RC)(0x300)
#define TPM_RC_4                 (TPM_RC)(0x400)
#define TPM_RC_5                 (TPM_RC)(0x500)
#define TPM_RC_6                 (TPM_RC)(0x600)
#define TPM_RC_7                 (TPM_RC)(0x700)
#define TPM_RC_8                 (TPM_RC)(0x800)
#define TPM_RC_9                 (TPM_RC)(0x900)
#define TPM_RC_A                 (TPM_RC)(0xA00)
#define TPM_RC_B                 (TPM_RC)(0xB00)
#define TPM_RC_C                 (TPM_RC)(0xC00)
#define TPM_RC_D                 (TPM_RC)(0xD00)
#define TPM_RC_E                 (TPM_RC)(0xE00)
#define TPM_RC_F                 (TPM_RC)(0xF00)
#define TPM_RC_N_MASK            (TPM_RC)(0xF00)

// Table 16 - TPM_CLOCK_ADJUST Constants
typedef INT8 TPM_CLOCK_ADJUST;
#define TPM_CLOCK_COARSE_SLOWER (TPM_CLOCK_ADJUST)(-3)
#define TPM_CLOCK_MEDIUM_SLOWER (TPM_CLOCK_ADJUST)(-2)
#define TPM_CLOCK_FINE_SLOWER   (TPM_CLOCK_ADJUST)(-1)
#define TPM_CLOCK_NO_CHANGE     (TPM_CLOCK_ADJUST)(0)
#define TPM_CLOCK_FINE_FASTER   (TPM_CLOCK_ADJUST)(1)
#define TPM_CLOCK_MEDIUM_FASTER (TPM_CLOCK_ADJUST)(2)
#define TPM_CLOCK_COARSE_FASTER (TPM_CLOCK_ADJUST)(3)

// Table 17 - TPM_EO Constants
typedef UINT16 TPM_EO;
#define TPM_EO_EQ          (TPM_EO)(0x0000)
#define TPM_EO_NEQ         (TPM_EO)(0x0001)
#define TPM_EO_SIGNED_GT   (TPM_EO)(0x0002)
#define TPM_EO_UNSIGNED_GT (TPM_EO)(0x0003)
#define TPM_EO_SIGNED_LT   (TPM_EO)(0x0004)
#define TPM_EO_UNSIGNED_LT (TPM_EO)(0x0005)
#define TPM_EO_SIGNED_GE   (TPM_EO)(0x0006)
#define TPM_EO_UNSIGNED_GE (TPM_EO)(0x0007)
#define TPM_EO_SIGNED_LE   (TPM_EO)(0x0008)
#define TPM_EO_UNSIGNED_LE (TPM_EO)(0x0009)
#define TPM_EO_BITSET      (TPM_EO)(0x000A)
#define TPM_EO_BITCLEAR    (TPM_EO)(0x000B)

// Table 18 - TPM_ST Constants
typedef UINT16 TPM_ST;
#define TPM_ST_RSP_COMMAND          (TPM_ST)(0x00C4)
#define TPM_ST_NULL                 (TPM_ST)(0X8000)
#define TPM_ST_NO_SESSIONS          (TPM_ST)(0x8001)
#define TPM_ST_SESSIONS             (TPM_ST)(0x8002)
#define TPM_ST_ATTEST_NV            (TPM_ST)(0x8014)
#define TPM_ST_ATTEST_COMMAND_AUDIT (TPM_ST)(0x8015)
#define TPM_ST_ATTEST_SESSION_AUDIT (TPM_ST)(0x8016)
#define TPM_ST_ATTEST_CERTIFY       (TPM_ST)(0x8017)
#define TPM_ST_ATTEST_QUOTE         (TPM_ST)(0x8018)
#define TPM_ST_ATTEST_TIME          (TPM_ST)(0x8019)
#define TPM_ST_ATTEST_CREATION      (TPM_ST)(0x801A)
#define TPM_ST_CREATION             (TPM_ST)(0x8021)
#define TPM_ST_VERIFIED             (TPM_ST)(0x8022)
#define TPM_ST_AUTH_SECRET          (TPM_ST)(0x8023)
#define TPM_ST_HASHCHECK            (TPM_ST)(0x8024)
#define TPM_ST_AUTH_SIGNED          (TPM_ST)(0x8025)
#define TPM_ST_FU_MANIFEST          (TPM_ST)(0x8029)

// Table 19 - TPM_SU Constants
typedef UINT16 TPM_SU;
#define TPM_SU_CLEAR (TPM_SU)(0x0000)
#define TPM_SU_STATE (TPM_SU)(0x0001)

// Table 20 - TPM_SE Constants
typedef UINT8 TPM_SE;
#define TPM_SE_HMAC   (TPM_SE)(0x00)
#define TPM_SE_POLICY (TPM_SE)(0x01)
#define TPM_SE_TRIAL  (TPM_SE)(0x03)

// Table 21 - TPM_CAP Constants
typedef UINT32 TPM_CAP;
#define TPM_CAP_FIRST           (TPM_CAP)(0x00000000)
#define TPM_CAP_ALGS            (TPM_CAP)(0x00000000)
#define TPM_CAP_HANDLES         (TPM_CAP)(0x00000001)
#define TPM_CAP_COMMANDS        (TPM_CAP)(0x00000002)
#define TPM_CAP_PP_COMMANDS     (TPM_CAP)(0x00000003)
#define TPM_CAP_AUDIT_COMMANDS  (TPM_CAP)(0x00000004)
#define TPM_CAP_PCRS            (TPM_CAP)(0x00000005)
#define TPM_CAP_TPM_PROPERTIES  (TPM_CAP)(0x00000006)
#define TPM_CAP_PCR_PROPERTIES  (TPM_CAP)(0x00000007)
#define TPM_CAP_ECC_CURVES      (TPM_CAP)(0x00000008)
#define TPM_CAP_LAST            (TPM_CAP)(0x00000008)
#define TPM_CAP_VENDOR_PROPERTY (TPM_CAP)(0x00000100)

// Table 22 - TPM_PT Constants
typedef UINT32 TPM_PT;
#define TPM_PT_NONE                (TPM_PT)(0x00000000)
#define PT_GROUP                   (TPM_PT)(0x00000100)
#define PT_FIXED                   (TPM_PT)(PT_GROUP * 1)
#define TPM_PT_FAMILY_INDICATOR    (TPM_PT)(PT_FIXED + 0)
#define TPM_PT_LEVEL               (TPM_PT)(PT_FIXED + 1)
#define TPM_PT_REVISION            (TPM_PT)(PT_FIXED + 2)
#define TPM_PT_DAY_OF_YEAR         (TPM_PT)(PT_FIXED + 3)
#define TPM_PT_YEAR                (TPM_PT)(PT_FIXED + 4)
#define TPM_PT_MANUFACTURER        (TPM_PT)(PT_FIXED + 5)
#define TPM_PT_VENDOR_STRING_1     (TPM_PT)(PT_FIXED + 6)
#define TPM_PT_VENDOR_STRING_2     (TPM_PT)(PT_FIXED + 7)
#define TPM_PT_VENDOR_STRING_3     (TPM_PT)(PT_FIXED + 8)
#define TPM_PT_VENDOR_STRING_4     (TPM_PT)(PT_FIXED + 9)
#define TPM_PT_VENDOR_TPM_TYPE     (TPM_PT)(PT_FIXED + 10)
#define TPM_PT_FIRMWARE_VERSION_1  (TPM_PT)(PT_FIXED + 11)
#define TPM_PT_FIRMWARE_VERSION_2  (TPM_PT)(PT_FIXED + 12)
#define TPM_PT_INPUT_BUFFER        (TPM_PT)(PT_FIXED + 13)
#define TPM_PT_HR_TRANSIENT_MIN    (TPM_PT)(PT_FIXED + 14)
#define TPM_PT_HR_PERSISTENT_MIN   (TPM_PT)(PT_FIXED + 15)
#define TPM_PT_HR_LOADED_MIN       (TPM_PT)(PT_FIXED + 16)
#define TPM_PT_ACTIVE_SESSIONS_MAX (TPM_PT)(PT_FIXED + 17)
#define TPM_PT_PCR_COUNT           (TPM_PT)(PT_FIXED + 18)
#define TPM_PT_PCR_SELECT_MIN      (TPM_PT)(PT_FIXED + 19)
#define TPM_PT_CONTEXT_GAP_MAX     (TPM_PT)(PT_FIXED + 20)
#define TPM_PT_NV_COUNTERS_MAX     (TPM_PT)(PT_FIXED + 22)
#define TPM_PT_NV_INDEX_MAX        (TPM_PT)(PT_FIXED + 23)
#define TPM_PT_MEMORY              (TPM_PT)(PT_FIXED + 24)
#define TPM_PT_CLOCK_UPDATE        (TPM_PT)(PT_FIXED + 25)
#define TPM_PT_CONTEXT_HASH        (TPM_PT)(PT_FIXED + 26)
#define TPM_PT_CONTEXT_SYM         (TPM_PT)(PT_FIXED + 27)
#define TPM_PT_CONTEXT_SYM_SIZE    (TPM_PT)(PT_FIXED + 28)
#define TPM_PT_ORDERLY_COUNT       (TPM_PT)(PT_FIXED + 29)
#define TPM_PT_MAX_COMMAND_SIZE    (TPM_PT)(PT_FIXED + 30)
#define TPM_PT_MAX_RESPONSE_SIZE   (TPM_PT)(PT_FIXED + 31)
#define TPM_PT_MAX_DIGEST          (TPM_PT)(PT_FIXED + 32)
#define TPM_PT_MAX_OBJECT_CONTEXT  (TPM_PT)(PT_FIXED + 33)
#define TPM_PT_MAX_SESSION_CONTEXT (TPM_PT)(PT_FIXED + 34)
#define TPM_PT_PS_FAMILY_INDICATOR (TPM_PT)(PT_FIXED + 35)
#define TPM_PT_PS_LEVEL            (TPM_PT)(PT_FIXED + 36)
#define TPM_PT_PS_REVISION         (TPM_PT)(PT_FIXED + 37)
#define TPM_PT_PS_DAY_OF_YEAR      (TPM_PT)(PT_FIXED + 38)
#define TPM_PT_PS_YEAR             (TPM_PT)(PT_FIXED + 39)
#define TPM_PT_SPLIT_MAX           (TPM_PT)(PT_FIXED + 40)
#define TPM_PT_TOTAL_COMMANDS      (TPM_PT)(PT_FIXED + 41)
#define TPM_PT_LIBRARY_COMMANDS    (TPM_PT)(PT_FIXED + 42)
#define TPM_PT_VENDOR_COMMANDS     (TPM_PT)(PT_FIXED + 43)
#define PT_VAR                     (TPM_PT)(PT_GROUP * 2)
#define TPM_PT_PERMANENT           (TPM_PT)(PT_VAR + 0)
#define TPM_PT_STARTUP_CLEAR       (TPM_PT)(PT_VAR + 1)
#define TPM_PT_HR_NV_INDEX         (TPM_PT)(PT_VAR + 2)
#define TPM_PT_HR_LOADED           (TPM_PT)(PT_VAR + 3)
#define TPM_PT_HR_LOADED_AVAIL     (TPM_PT)(PT_VAR + 4)
#define TPM_PT_HR_ACTIVE           (TPM_PT)(PT_VAR + 5)
#define TPM_PT_HR_ACTIVE_AVAIL     (TPM_PT)(PT_VAR + 6)
#define TPM_PT_HR_TRANSIENT_AVAIL  (TPM_PT)(PT_VAR + 7)
#define TPM_PT_HR_PERSISTENT       (TPM_PT)(PT_VAR + 8)
#define TPM_PT_HR_PERSISTENT_AVAIL (TPM_PT)(PT_VAR + 9)
#define TPM_PT_NV_COUNTERS         (TPM_PT)(PT_VAR + 10)
#define TPM_PT_NV_COUNTERS_AVAIL   (TPM_PT)(PT_VAR + 11)
#define TPM_PT_ALGORITHM_SET       (TPM_PT)(PT_VAR + 12)
#define TPM_PT_LOADED_CURVES       (TPM_PT)(PT_VAR + 13)
#define TPM_PT_LOCKOUT_COUNTER     (TPM_PT)(PT_VAR + 14)
#define TPM_PT_MAX_AUTH_FAIL       (TPM_PT)(PT_VAR + 15)
#define TPM_PT_LOCKOUT_INTERVAL    (TPM_PT)(PT_VAR + 16)
#define TPM_PT_LOCKOUT_RECOVERY    (TPM_PT)(PT_VAR + 17)
#define TPM_PT_NV_WRITE_RECOVERY   (TPM_PT)(PT_VAR + 18)
#define TPM_PT_AUDIT_COUNTER_0     (TPM_PT)(PT_VAR + 19)
#define TPM_PT_AUDIT_COUNTER_1     (TPM_PT)(PT_VAR + 20)

// Table 23 - TPM_PT_PCR Constants
typedef UINT32 TPM_PT_PCR;
#define TPM_PT_PCR_FIRST        (TPM_PT_PCR)(0x00000000)
#define TPM_PT_PCR_SAVE         (TPM_PT_PCR)(0x00000000)
#define TPM_PT_PCR_EXTEND_L0    (TPM_PT_PCR)(0x00000001)
#define TPM_PT_PCR_RESET_L0     (TPM_PT_PCR)(0x00000002)
#define TPM_PT_PCR_EXTEND_L1    (TPM_PT_PCR)(0x00000003)
#define TPM_PT_PCR_RESET_L1     (TPM_PT_PCR)(0x00000004)
#define TPM_PT_PCR_EXTEND_L2    (TPM_PT_PCR)(0x00000005)
#define TPM_PT_PCR_RESET_L2     (TPM_PT_PCR)(0x00000006)
#define TPM_PT_PCR_EXTEND_L3    (TPM_PT_PCR)(0x00000007)
#define TPM_PT_PCR_RESET_L3     (TPM_PT_PCR)(0x00000008)
#define TPM_PT_PCR_EXTEND_L4    (TPM_PT_PCR)(0x00000009)
#define TPM_PT_PCR_RESET_L4     (TPM_PT_PCR)(0x0000000A)
#define TPM_PT_PCR_NO_INCREMENT (TPM_PT_PCR)(0x00000011)
#define TPM_PT_PCR_DRTM_RESET   (TPM_PT_PCR)(0x00000012)
#define TPM_PT_PCR_POLICY       (TPM_PT_PCR)(0x00000013)
#define TPM_PT_PCR_AUTH         (TPM_PT_PCR)(0x00000014)
#define TPM_PT_PCR_LAST         (TPM_PT_PCR)(0x00000014)

// Table 24 - TPM_PS Constants
typedef UINT32 TPM_PS;
#define TPM_PS_MAIN           (TPM_PS)(0x00000000)
#define TPM_PS_PC             (TPM_PS)(0x00000001)
#define TPM_PS_PDA            (TPM_PS)(0x00000002)
#define TPM_PS_CELL_PHONE     (TPM_PS)(0x00000003)
#define TPM_PS_SERVER         (TPM_PS)(0x00000004)
#define TPM_PS_PERIPHERAL     (TPM_PS)(0x00000005)
#define TPM_PS_TSS            (TPM_PS)(0x00000006)
#define TPM_PS_STORAGE        (TPM_PS)(0x00000007)
#define TPM_PS_AUTHENTICATION (TPM_PS)(0x00000008)
#define TPM_PS_EMBEDDED       (TPM_PS)(0x00000009)
#define TPM_PS_HARDCOPY       (TPM_PS)(0x0000000A)
#define TPM_PS_INFRASTRUCTURE (TPM_PS)(0x0000000B)
#define TPM_PS_VIRTUALIZATION (TPM_PS)(0x0000000C)
#define TPM_PS_TNC            (TPM_PS)(0x0000000D)
#define TPM_PS_MULTI_TENANT   (TPM_PS)(0x0000000E)
#define TPM_PS_TC             (TPM_PS)(0x0000000F)

// 7 Handles

// Table 25 - Handles Types
//
// NOTE: Comment because it has same name as TPM1.2 (value is same, so not runtime issue)
//
//typedef UINT32    TPM_HANDLE;

// Table 26 - TPM_HT Constants
typedef UINT8 TPM_HT;
#define TPM_HT_PCR            (TPM_HT)(0x00)
#define TPM_HT_NV_INDEX       (TPM_HT)(0x01)
#define TPM_HT_HMAC_SESSION   (TPM_HT)(0x02)
#define TPM_HT_LOADED_SESSION (TPM_HT)(0x02)
#define TPM_HT_POLICY_SESSION (TPM_HT)(0x03)
#define TPM_HT_ACTIVE_SESSION (TPM_HT)(0x03)
#define TPM_HT_PERMANENT      (TPM_HT)(0x40)
#define TPM_HT_TRANSIENT      (TPM_HT)(0x80)
#define TPM_HT_PERSISTENT     (TPM_HT)(0x81)

// Table 27 - TPM_RH Constants
typedef UINT32 TPM_RH;
#define TPM_RH_FIRST       (TPM_RH)(0x40000000)
#define TPM_RH_SRK         (TPM_RH)(0x40000000)
#define TPM_RH_OWNER       (TPM_RH)(0x40000001)
#define TPM_RH_REVOKE      (TPM_RH)(0x40000002)
#define TPM_RH_TRANSPORT   (TPM_RH)(0x40000003)
#define TPM_RH_OPERATOR    (TPM_RH)(0x40000004)
#define TPM_RH_ADMIN       (TPM_RH)(0x40000005)
#define TPM_RH_EK          (TPM_RH)(0x40000006)
#define TPM_RH_NULL        (TPM_RH)(0x40000007)
#define TPM_RH_UNASSIGNED  (TPM_RH)(0x40000008)
#define TPM_RS_PW          (TPM_RH)(0x40000009)
#define TPM_RH_LOCKOUT     (TPM_RH)(0x4000000A)
#define TPM_RH_ENDORSEMENT (TPM_RH)(0x4000000B)
#define TPM_RH_PLATFORM    (TPM_RH)(0x4000000C)
#define TPM_RH_PLATFORM_NV (TPM_RH)(0x4000000D)
#define TPM_RH_AUTH_00     (TPM_RH)(0x40000010)
#define TPM_RH_AUTH_FF     (TPM_RH)(0x4000010F)
#define TPM_RH_LAST        (TPM_RH)(0x4000010F)

// Table 28 - TPM_HC Constants
typedef TPM_HANDLE TPM_HC;
#define HR_HANDLE_MASK       (TPM_HC)(0x00FFFFFF)
#define HR_RANGE_MASK        (TPM_HC)(0xFF000000)
#define HR_SHIFT             (TPM_HC)(24)
#define HR_PCR               (TPM_HC)((TPM_HC)TPM_HT_PCR << HR_SHIFT)
#define HR_HMAC_SESSION      (TPM_HC)((TPM_HC)TPM_HT_HMAC_SESSION << HR_SHIFT)
#define HR_POLICY_SESSION    (TPM_HC)((TPM_HC)TPM_HT_POLICY_SESSION << HR_SHIFT)
#define HR_TRANSIENT         (TPM_HC)((TPM_HC)TPM_HT_TRANSIENT << HR_SHIFT)
#define HR_PERSISTENT        (TPM_HC)((TPM_HC)TPM_HT_PERSISTENT << HR_SHIFT)
#define HR_NV_INDEX          (TPM_HC)((TPM_HC)TPM_HT_NV_INDEX << HR_SHIFT)
#define HR_PERMANENT         (TPM_HC)((TPM_HC)TPM_HT_PERMANENT << HR_SHIFT)
#define PCR_FIRST            (TPM_HC)(HR_PCR + 0)
#define PCR_LAST             (TPM_HC)(PCR_FIRST + IMPLEMENTATION_PCR - 1)
#define HMAC_SESSION_FIRST   (TPM_HC)(HR_HMAC_SESSION + 0)
#define HMAC_SESSION_LAST    (TPM_HC)(HMAC_SESSION_FIRST + MAX_ACTIVE_SESSIONS - 1)
#define LOADED_SESSION_FIRST (TPM_HC)(HMAC_SESSION_FIRST)
#define LOADED_SESSION_LAST  (TPM_HC)(HMAC_SESSION_LAST)
#define POLICY_SESSION_FIRST (TPM_HC)(HR_POLICY_SESSION + 0)
#define POLICY_SESSION_LAST  (TPM_HC)(POLICY_SESSION_FIRST + MAX_ACTIVE_SESSIONS - 1)
#define TRANSIENT_FIRST      (TPM_HC)(HR_TRANSIENT + 0)
#define ACTIVE_SESSION_FIRST (TPM_HC)(POLICY_SESSION_FIRST)
#define ACTIVE_SESSION_LAST  (TPM_HC)(POLICY_SESSION_LAST)
#define TRANSIENT_LAST       (TPM_HC)(TRANSIENT_FIRST+MAX_LOADED_OBJECTS - 1)
#define PERSISTENT_FIRST     (TPM_HC)(HR_PERSISTENT + 0)
#define PERSISTENT_LAST      (TPM_HC)(PERSISTENT_FIRST + 0x00FFFFFF)
#define PLATFORM_PERSISTENT  (TPM_HC)(PERSISTENT_FIRST + 0x00800000)
#define NV_INDEX_FIRST       (TPM_HC)(HR_NV_INDEX + 0)
#define NV_INDEX_LAST        (TPM_HC)(NV_INDEX_FIRST + 0x00FFFFFF)
#define PERMANENT_FIRST      (TPM_HC)(TPM_RH_FIRST)
#define PERMANENT_LAST       (TPM_HC)(TPM_RH_LAST)

// 8 Attribute Structures

// Table 29 - TPMA_ALGORITHM Bits
typedef struct {
  UINT32 asymmetric    : 1;
  UINT32 symmetric     : 1;
  UINT32 hash          : 1;
  UINT32 object        : 1;
  UINT32 reserved4_7   : 4;
  UINT32 signing       : 1;
  UINT32 encrypting    : 1;
  UINT32 method        : 1;
  UINT32 reserved11_31 : 21;
} TPMA_ALGORITHM;

// Table 30 - TPMA_OBJECT Bits
typedef struct {
  UINT32 reserved1            : 1;
  UINT32 fixedTPM             : 1;
  UINT32 stClear              : 1;
  UINT32 reserved4            : 1;
  UINT32 fixedParent          : 1;
  UINT32 sensitiveDataOrigin  : 1;
  UINT32 userWithAuth         : 1;
  UINT32 adminWithPolicy      : 1;
  UINT32 reserved8_9          : 2;
  UINT32 noDA                 : 1;
  UINT32 encryptedDuplication : 1;
  UINT32 reserved12_15        : 4;
  UINT32 restricted           : 1;
  UINT32 decrypt              : 1;
  UINT32 sign                 : 1;
  UINT32 reserved19_31        : 13;
} TPMA_OBJECT;

// Table 31 - TPMA_SESSION Bits
typedef struct {
  UINT8 continueSession : 1;
  UINT8 auditExclusive  : 1;
  UINT8 auditReset      : 1;
  UINT8 reserved3_4     : 2;
  UINT8 decrypt         : 1;
  UINT8 encrypt         : 1;
  UINT8 audit           : 1;
} TPMA_SESSION;

// Table 32 - TPMA_LOCALITY Bits
//
// NOTE: Use low case here to resolve conflict
//
typedef struct {
  UINT8 locZero  : 1;
  UINT8 locOne   : 1;
  UINT8 locTwo   : 1;
  UINT8 locThree : 1;
  UINT8 locFour  : 1;
  UINT8 Extended : 3;
} TPMA_LOCALITY;

// Table 33 - TPMA_PERMANENT Bits
typedef struct {
  UINT32 ownerAuthSet       : 1;
  UINT32 endorsementAuthSet : 1;
  UINT32 lockoutAuthSet     : 1;
  UINT32 reserved3_7        : 5;
  UINT32 disableClear       : 1;
  UINT32 inLockout          : 1;
  UINT32 tpmGeneratedEPS    : 1;
  UINT32 reserved11_31      : 21;
} TPMA_PERMANENT;

// Table 34 - TPMA_STARTUP_CLEAR Bits
typedef struct {
  UINT32 phEnable     : 1;
  UINT32 shEnable     : 1;
  UINT32 ehEnable     : 1;
  UINT32 reserved3_30 : 28;
  UINT32 orderly      : 1;
} TPMA_STARTUP_CLEAR;

// Table 35 - TPMA_MEMORY Bits
typedef struct {
  UINT32 sharedRAM         : 1;
  UINT32 sharedNV          : 1;
  UINT32 objectCopiedToRam : 1;
  UINT32 reserved3_31      : 29;
} TPMA_MEMORY;

// Table 36 - TPMA_CC Bits
typedef struct {
  UINT32 commandIndex  : 16;
  UINT32 reserved16_21 : 6;
  UINT32 nv            : 1;
  UINT32 extensive     : 1;
  UINT32 flushed       : 1;
  UINT32 cHandles      : 3;
  UINT32 rHandle       : 1;
  UINT32 V             : 1;
  UINT32 Res           : 2;
} TPMA_CC;

// 9 Interface Types

// Table 37 - TPMI_YES_NO Type
typedef BYTE TPMI_YES_NO;

// Table 38 - TPMI_DH_OBJECT Type
typedef TPM_HANDLE TPMI_DH_OBJECT;

// Table 39 - TPMI_DH_PERSISTENT Type
typedef TPM_HANDLE TPMI_DH_PERSISTENT;

// Table 40 - TPMI_DH_ENTITY Type
typedef TPM_HANDLE TPMI_DH_ENTITY;

// Table 41 - TPMI_DH_PCR Type
typedef TPM_HANDLE TPMI_DH_PCR;

// Table 42 - TPMI_SH_AUTH_SESSION Type
typedef TPM_HANDLE TPMI_SH_AUTH_SESSION;

// Table 43 - TPMI_SH_HMAC Type
typedef TPM_HANDLE TPMI_SH_HMAC;

// Table 44 - TPMI_SH_POLICY Type
typedef TPM_HANDLE TPMI_SH_POLICY;

// Table 45 - TPMI_DH_CONTEXT Type
typedef TPM_HANDLE TPMI_DH_CONTEXT;

// Table 46 - TPMI_RH_HIERARCHY Type
typedef TPM_HANDLE TPMI_RH_HIERARCHY;

// Table 47 - TPMI_RH_HIERARCHY_AUTH Type
typedef TPM_HANDLE TPMI_RH_HIERARCHY_AUTH;

// Table 48 - TPMI_RH_PLATFORM Type
typedef TPM_HANDLE TPMI_RH_PLATFORM;

// Table 49 - TPMI_RH_OWNER Type
typedef TPM_HANDLE TPMI_RH_OWNER;

// Table 50 - TPMI_RH_ENDORSEMENT Type
typedef TPM_HANDLE TPMI_RH_ENDORSEMENT;

// Table 51 - TPMI_RH_PROVISION Type
typedef TPM_HANDLE TPMI_RH_PROVISION;

// Table 52 - TPMI_RH_CLEAR Type
typedef TPM_HANDLE TPMI_RH_CLEAR;

// Table 53 - TPMI_RH_NV_AUTH Type
typedef TPM_HANDLE TPMI_RH_NV_AUTH;

// Table 54 - TPMI_RH_LOCKOUT Type
typedef TPM_HANDLE TPMI_RH_LOCKOUT;

// Table 55 - TPMI_RH_NV_INDEX Type
typedef TPM_HANDLE TPMI_RH_NV_INDEX;

// Table 56 - TPMI_ALG_HASH Type
typedef TPM_ALG_ID TPMI_ALG_HASH;

// Table 57 - TPMI_ALG_ASYM Type
typedef TPM_ALG_ID TPMI_ALG_ASYM;

// Table 58 - TPMI_ALG_SYM Type
typedef TPM_ALG_ID TPMI_ALG_SYM;

// Table 59 - TPMI_ALG_SYM_OBJECT Type
typedef TPM_ALG_ID TPMI_ALG_SYM_OBJECT;

// Table 60 - TPMI_ALG_SYM_MODE Type
typedef TPM_ALG_ID TPMI_ALG_SYM_MODE;

// Table 61 - TPMI_ALG_KDF Type
typedef TPM_ALG_ID TPMI_ALG_KDF;

// Table 62 - TPMI_ALG_SIG_SCHEME Type
typedef TPM_ALG_ID TPMI_ALG_SIG_SCHEME;

// Table 63 - TPMI_ECC_KEY_EXCHANGE Type
typedef TPM_ALG_ID TPMI_ECC_KEY_EXCHANGE;

// Table 64 - TPMI_ST_COMMAND_TAG Type
typedef TPM_ST TPMI_ST_COMMAND_TAG;

// 10 Structure Definitions

// Table 65 - TPMS_ALGORITHM_DESCRIPTION Structure
typedef struct {
  TPM_ALG_ID     alg;
  TPMA_ALGORITHM attributes;
} TPMS_ALGORITHM_DESCRIPTION;

// Table 66 - TPMU_HA Union
typedef union {
  BYTE sha1[SHA1_DIGEST_SIZE];
  BYTE sha256[SHA256_DIGEST_SIZE];
  BYTE sm3_256[SM3_256_DIGEST_SIZE];
  BYTE sha384[SHA384_DIGEST_SIZE];
  BYTE sha512[SHA512_DIGEST_SIZE];
} TPMU_HA;

// Table 67 - TPMT_HA Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
  TPMU_HA       digest;
} TPMT_HA;

// Table 68 - TPM2B_DIGEST Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(TPMU_HA)];
} TPM2B_DIGEST;

// Table 69 - TPM2B_DATA Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(TPMT_HA)];
} TPM2B_DATA;

// Table 70 - TPM2B_NONCE Types
typedef TPM2B_DIGEST TPM2B_NONCE;

// Table 71 - TPM2B_AUTH Types
typedef TPM2B_DIGEST TPM2B_AUTH;

// Table 72 - TPM2B_OPERAND Types
typedef TPM2B_DIGEST TPM2B_OPERAND;

// Table 73 - TPM2B_EVENT Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[1024];
} TPM2B_EVENT;

// Table 74 - TPM2B_MAX_BUFFER Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_DIGEST_BUFFER];
} TPM2B_MAX_BUFFER;

// Table 75 - TPM2B_MAX_NV_BUFFER Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_NV_INDEX_SIZE];
} TPM2B_MAX_NV_BUFFER;

// Table 76 - TPM2B_TIMEOUT Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(UINT64)];
} TPM2B_TIMEOUT;

// Table 77 -- TPM2B_IV Structure <I/O>
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_SYM_BLOCK_SIZE];
} TPM2B_IV;

// Table 78 - TPMU_NAME Union
typedef union {
  TPMT_HA    digest;
  TPM_HANDLE handle;
} TPMU_NAME;

// Table 79 - TPM2B_NAME Structure
typedef struct {
  UINT16 size;
  BYTE   name[sizeof(TPMU_NAME)];
} TPM2B_NAME;

// Table 80 - TPMS_PCR_SELECT Structure
typedef struct {
  UINT8 sizeofSelect;
  BYTE  pcrSelect[PCR_SELECT_MAX];
} TPMS_PCR_SELECT;

// Table 81 - TPMS_PCR_SELECTION Structure
typedef struct {
  TPMI_ALG_HASH hash;
  UINT8         sizeofSelect;
  BYTE          pcrSelect[PCR_SELECT_MAX];
} TPMS_PCR_SELECTION;

// Table 84 - TPMT_TK_CREATION Structure
typedef struct {
  TPM_ST            tag;
  TPMI_RH_HIERARCHY hierarchy;
  TPM2B_DIGEST      digest;
} TPMT_TK_CREATION;

// Table 85 - TPMT_TK_VERIFIED Structure
typedef struct {
  TPM_ST            tag;
  TPMI_RH_HIERARCHY hierarchy;
  TPM2B_DIGEST      digest;
} TPMT_TK_VERIFIED;

// Table 86 - TPMT_TK_AUTH Structure
typedef struct {
  TPM_ST            tag;
  TPMI_RH_HIERARCHY hierarchy;
  TPM2B_DIGEST      digest;
} TPMT_TK_AUTH;

// Table 87 - TPMT_TK_HASHCHECK Structure
typedef struct {
  TPM_ST            tag;
  TPMI_RH_HIERARCHY hierarchy;
  TPM2B_DIGEST      digest;
} TPMT_TK_HASHCHECK;

// Table 88 - TPMS_ALG_PROPERTY Structure
typedef struct {
  TPM_ALG_ID     alg;
  TPMA_ALGORITHM algProperties;
} TPMS_ALG_PROPERTY;

// Table 89 - TPMS_TAGGED_PROPERTY Structure
typedef struct {
  TPM_PT property;
  UINT32 value;
} TPMS_TAGGED_PROPERTY;

// Table 90 - TPMS_TAGGED_PCR_SELECT Structure
typedef struct {
  TPM_PT tag;
  UINT8  sizeofSelect;
  BYTE   pcrSelect[PCR_SELECT_MAX];
} TPMS_TAGGED_PCR_SELECT;

// Table 91 - TPML_CC Structure
typedef struct {
  UINT32 count;
  TPM_CC commandCodes[MAX_CAP_CC];
} TPML_CC;

// Table 92 - TPML_CCA Structure
typedef struct {
  UINT32  count;
  TPMA_CC commandAttributes[MAX_CAP_CC];
} TPML_CCA;

// Table 93 - TPML_ALG Structure
typedef struct {
  UINT32     count;
  TPM_ALG_ID algorithms[MAX_ALG_LIST_SIZE];
} TPML_ALG;

// Table 94 - TPML_HANDLE Structure
typedef struct {
  UINT32     count;
  TPM_HANDLE handle[MAX_CAP_HANDLES];
} TPML_HANDLE;

// Table 95 - TPML_DIGEST Structure
typedef struct {
  UINT32       count;
  TPM2B_DIGEST digests[8];
} TPML_DIGEST;

// Table 96 -- TPML_DIGEST_VALUES Structure <I/O>
typedef struct {
  UINT32  count;
  TPMT_HA digests[HASH_COUNT];
} TPML_DIGEST_VALUES;

// Table 97 - TPM2B_DIGEST_VALUES Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(TPML_DIGEST_VALUES)];
} TPM2B_DIGEST_VALUES;

// Table 98 - TPML_PCR_SELECTION Structure
typedef struct {
  UINT32             count;
  TPMS_PCR_SELECTION pcrSelections[HASH_COUNT];
} TPML_PCR_SELECTION;

// Table 99 - TPML_ALG_PROPERTY Structure
typedef struct {
  UINT32            count;
  TPMS_ALG_PROPERTY algProperties[MAX_CAP_ALGS];
} TPML_ALG_PROPERTY;

// Table 100 - TPML_TAGGED_TPM_PROPERTY Structure
typedef struct {
  UINT32               count;
  TPMS_TAGGED_PROPERTY tpmProperty[MAX_TPM_PROPERTIES];
} TPML_TAGGED_TPM_PROPERTY;

// Table 101 - TPML_TAGGED_PCR_PROPERTY Structure
typedef struct {
  UINT32                 count;
  TPMS_TAGGED_PCR_SELECT pcrProperty[MAX_PCR_PROPERTIES];
} TPML_TAGGED_PCR_PROPERTY;

// Table 102 - TPML_ECC_CURVE Structure
typedef struct {
  UINT32        count;
  TPM_ECC_CURVE eccCurves[MAX_ECC_CURVES];
} TPML_ECC_CURVE;

// Table 103 - TPMU_CAPABILITIES Union
typedef union {
  TPML_ALG_PROPERTY        algorithms;
  TPML_HANDLE              handles;
  TPML_CCA                 command;
  TPML_CC                  ppCommands;
  TPML_CC                  auditCommands;
  TPML_PCR_SELECTION       assignedPCR;
  TPML_TAGGED_TPM_PROPERTY tpmProperties;
  TPML_TAGGED_PCR_PROPERTY pcrProperties;
  TPML_ECC_CURVE           eccCurves;
} TPMU_CAPABILITIES;

// Table 104 - TPMS_CAPABILITY_DATA Structure
typedef struct {
  TPM_CAP           capability;
  TPMU_CAPABILITIES data;
} TPMS_CAPABILITY_DATA;

// Table 105 - TPMS_CLOCK_INFO Structure
typedef struct {
  UINT64      clock;
  UINT32      resetCount;
  UINT32      restartCount;
  TPMI_YES_NO safe;
} TPMS_CLOCK_INFO;

// Table 106 - TPMS_TIME_INFO Structure
typedef struct {
  UINT64          time;
  TPMS_CLOCK_INFO clockInfo;
} TPMS_TIME_INFO;

// Table 107 - TPMS_TIME_ATTEST_INFO Structure
typedef struct {
  TPMS_TIME_INFO time;
  UINT64         firmwareVersion;
} TPMS_TIME_ATTEST_INFO;

// Table 108 - TPMS_CERTIFY_INFO Structure
typedef struct {
  TPM2B_NAME name;
  TPM2B_NAME qualifiedName;
} TPMS_CERTIFY_INFO;

// Table 109 - TPMS_QUOTE_INFO Structure
typedef struct {
  TPML_PCR_SELECTION pcrSelect;
  TPM2B_DIGEST       pcrDigest;
} TPMS_QUOTE_INFO;

// Table 110 - TPMS_COMMAND_AUDIT_INFO Structure
typedef struct {
  UINT64       auditCounter;
  TPM_ALG_ID   digestAlg;
  TPM2B_DIGEST auditDigest;
  TPM2B_DIGEST commandDigest;
} TPMS_COMMAND_AUDIT_INFO;

// Table 111 - TPMS_SESSION_AUDIT_INFO Structure
typedef struct {
  TPMI_YES_NO  exclusiveSession;
  TPM2B_DIGEST sessionDigest;
} TPMS_SESSION_AUDIT_INFO;

// Table 112 - TPMS_CREATION_INFO Structure
typedef struct {
  TPM2B_NAME   objectName;
  TPM2B_DIGEST creationHash;
} TPMS_CREATION_INFO;

// Table 113 - TPMS_NV_CERTIFY_INFO Structure
typedef struct {
  TPM2B_NAME          indexName;
  UINT16              offset;
  TPM2B_MAX_NV_BUFFER nvContents;
} TPMS_NV_CERTIFY_INFO;

// Table 114 - TPMI_ST_ATTEST Type
typedef TPM_ST TPMI_ST_ATTEST;

// Table 115 - TPMU_ATTEST Union
typedef union {
  TPMS_CERTIFY_INFO       certify;
  TPMS_CREATION_INFO      creation;
  TPMS_QUOTE_INFO         quote;
  TPMS_COMMAND_AUDIT_INFO commandAudit;
  TPMS_SESSION_AUDIT_INFO sessionAudit;
  TPMS_TIME_ATTEST_INFO   time;
  TPMS_NV_CERTIFY_INFO    nv;
} TPMU_ATTEST;

// Table 116 - TPMS_ATTEST Structure
typedef struct {
  TPM_GENERATED   magic;
  TPMI_ST_ATTEST  type;
  TPM2B_NAME      qualifiedSigner;
  TPM2B_DATA      extraData;
  TPMS_CLOCK_INFO clockInfo;
  UINT64          firmwareVersion;
  TPMU_ATTEST     attested;
} TPMS_ATTEST;

// Table 117 - TPM2B_ATTEST Structure
typedef struct {
  UINT16 size;
  BYTE   attestationData[sizeof(TPMS_ATTEST)];
} TPM2B_ATTEST;

// Table 118 - TPMS_AUTH_COMMAND Structure
typedef struct {
  TPMI_SH_AUTH_SESSION sessionHandle;
  TPM2B_NONCE          nonce;
  TPMA_SESSION         sessionAttributes;
  TPM2B_AUTH           hmac;
} TPMS_AUTH_COMMAND;

// Table 119 - TPMS_AUTH_RESPONSE Structure
typedef struct {
  TPM2B_NONCE  nonce;
  TPMA_SESSION sessionAttributes;
  TPM2B_AUTH   hmac;
} TPMS_AUTH_RESPONSE;

// 11 Algorithm Parameters and Structures

// Table 120 - TPMI_AES_KEY_BITS Type
typedef TPM_KEY_BITS TPMI_AES_KEY_BITS;

// Table 121 - TPMI_SM4_KEY_BITS Type
typedef TPM_KEY_BITS TPMI_SM4_KEY_BITS;

// Table 122 - TPMU_SYM_KEY_BITS Union
typedef union {
  TPMI_AES_KEY_BITS aes;
  TPMI_SM4_KEY_BITS SM4;
  TPM_KEY_BITS      sym;
  TPMI_ALG_HASH     xor;
} TPMU_SYM_KEY_BITS;

// Table 123 - TPMU_SYM_MODE Union
typedef union {
  TPMI_ALG_SYM_MODE aes;
  TPMI_ALG_SYM_MODE SM4;
  TPMI_ALG_SYM_MODE sym;
} TPMU_SYM_MODE;

// Table 125 - TPMT_SYM_DEF Structure
typedef struct {
  TPMI_ALG_SYM      algorithm;
  TPMU_SYM_KEY_BITS keyBits;
  TPMU_SYM_MODE     mode;
} TPMT_SYM_DEF;

// Table 126 - TPMT_SYM_DEF_OBJECT Structure
typedef struct {
  TPMI_ALG_SYM_OBJECT algorithm;
  TPMU_SYM_KEY_BITS   keyBits;
  TPMU_SYM_MODE       mode;
} TPMT_SYM_DEF_OBJECT;

// Table 127 - TPM2B_SYM_KEY Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_SYM_KEY_BYTES];
} TPM2B_SYM_KEY;

// Table 128 - TPMS_SYMCIPHER_PARMS Structure
typedef struct {
  TPMT_SYM_DEF_OBJECT sym;
} TPMS_SYMCIPHER_PARMS;

// Table 129 - TPM2B_SENSITIVE_DATA Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_SYM_DATA];
} TPM2B_SENSITIVE_DATA;

// Table 130 - TPMS_SENSITIVE_CREATE Structure
typedef struct {
  TPM2B_AUTH           userAuth;
  TPM2B_SENSITIVE_DATA data;
} TPMS_SENSITIVE_CREATE;

// Table 131 - TPM2B_SENSITIVE_CREATE Structure
typedef struct {
  UINT16                size;
  TPMS_SENSITIVE_CREATE sensitive;
} TPM2B_SENSITIVE_CREATE;

// Table 132 - TPMS_SCHEME_SIGHASH Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_SIGHASH;

// Table 133 - TPMI_ALG_KEYEDHASH_SCHEME Type
typedef TPM_ALG_ID TPMI_ALG_KEYEDHASH_SCHEME;

// Table 134 - HMAC_SIG_SCHEME Types
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_HMAC;

// Table 135 - TPMS_SCHEME_XOR Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
  TPMI_ALG_KDF  kdf;
} TPMS_SCHEME_XOR;

// Table 136 - TPMU_SCHEME_KEYEDHASH Union
typedef union {
  TPMS_SCHEME_HMAC hmac;
  TPMS_SCHEME_XOR  xor;
} TPMU_SCHEME_KEYEDHASH;

// Table 137 - TPMT_KEYEDHASH_SCHEME Structure
typedef struct {
  TPMI_ALG_KEYEDHASH_SCHEME scheme;
  TPMU_SCHEME_KEYEDHASH     details;
} TPMT_KEYEDHASH_SCHEME;

// Table 138 - RSA_SIG_SCHEMES Types
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_RSASSA;
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_RSAPSS;

// Table 139 - ECC_SIG_SCHEMES Types
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_ECDSA;
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_SM2;
typedef TPMS_SCHEME_SIGHASH TPMS_SCHEME_ECSCHNORR;

// Table 140 - TPMS_SCHEME_ECDAA Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
  UINT16        count;
} TPMS_SCHEME_ECDAA;

// Table 141 - TPMU_SIG_SCHEME Union
typedef union {
  TPMS_SCHEME_RSASSA    rsassa;
  TPMS_SCHEME_RSAPSS    rsapss;
  TPMS_SCHEME_ECDSA     ecdsa;
  TPMS_SCHEME_ECDAA     ecdaa;
  TPMS_SCHEME_ECSCHNORR ecSchnorr;
  TPMS_SCHEME_HMAC      hmac;
  TPMS_SCHEME_SIGHASH   any;
} TPMU_SIG_SCHEME;

// Table 142 - TPMT_SIG_SCHEME Structure
typedef struct {
  TPMI_ALG_SIG_SCHEME scheme;
  TPMU_SIG_SCHEME     details;
} TPMT_SIG_SCHEME;

// Table 143 - TPMS_SCHEME_OAEP Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_OAEP;

// Table 144 - TPMS_SCHEME_ECDH Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_ECDH;

// Table 145 - TPMS_SCHEME_MGF1 Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_MGF1;

// Table 146 - TPMS_SCHEME_KDF1_SP800_56a Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_KDF1_SP800_56a;

// Table 147 - TPMS_SCHEME_KDF2 Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_KDF2;

// Table 148 - TPMS_SCHEME_KDF1_SP800_108 Structure
typedef struct {
  TPMI_ALG_HASH hashAlg;
} TPMS_SCHEME_KDF1_SP800_108;

// Table 149 - TPMU_KDF_SCHEME Union
typedef union {
  TPMS_SCHEME_MGF1           mgf1;
  TPMS_SCHEME_KDF1_SP800_56a kdf1_SP800_56a;
  TPMS_SCHEME_KDF2           kdf2;
  TPMS_SCHEME_KDF1_SP800_108 kdf1_sp800_108;
} TPMU_KDF_SCHEME;

// Table 150 - TPMT_KDF_SCHEME Structure
typedef struct {
  TPMI_ALG_KDF    scheme;
  TPMU_KDF_SCHEME details;
} TPMT_KDF_SCHEME;

// Table 151 - TPMI_ALG_ASYM_SCHEME Type
typedef TPM_ALG_ID TPMI_ALG_ASYM_SCHEME;

// Table 152 - TPMU_ASYM_SCHEME Union
typedef union {
  TPMS_SCHEME_RSASSA    rsassa;
  TPMS_SCHEME_RSAPSS    rsapss;
  TPMS_SCHEME_OAEP      oaep;
  TPMS_SCHEME_ECDSA     ecdsa;
  TPMS_SCHEME_ECDAA     ecdaa;
  TPMS_SCHEME_ECSCHNORR ecSchnorr;
  TPMS_SCHEME_SIGHASH   anySig;
} TPMU_ASYM_SCHEME;

// Table 153 - TPMT_ASYM_SCHEME Structure
typedef struct {
  TPMI_ALG_ASYM_SCHEME scheme;
  TPMU_ASYM_SCHEME     details;
} TPMT_ASYM_SCHEME;

// Table 154 - TPMI_ALG_RSA_SCHEME Type
typedef TPM_ALG_ID TPMI_ALG_RSA_SCHEME;

// Table 155 - TPMT_RSA_SCHEME Structure
typedef struct {
  TPMI_ALG_RSA_SCHEME scheme;
  TPMU_ASYM_SCHEME    details;
} TPMT_RSA_SCHEME;

// Table 156 - TPMI_ALG_RSA_DECRYPT Type
typedef TPM_ALG_ID TPMI_ALG_RSA_DECRYPT;

// Table 157 - TPMT_RSA_DECRYPT Structure
typedef struct {
  TPMI_ALG_RSA_DECRYPT scheme;
  TPMU_ASYM_SCHEME     details;
} TPMT_RSA_DECRYPT;

// Table 158 - TPM2B_PUBLIC_KEY_RSA Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_RSA_KEY_BYTES];
} TPM2B_PUBLIC_KEY_RSA;

// Table 159 - TPMI_RSA_KEY_BITS Type
typedef TPM_KEY_BITS TPMI_RSA_KEY_BITS;

// Table 160 - TPM2B_PRIVATE_KEY_RSA Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_RSA_KEY_BYTES/2];
} TPM2B_PRIVATE_KEY_RSA;

// Table 161 - TPM2B_ECC_PARAMETER Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_ECC_KEY_BYTES];
} TPM2B_ECC_PARAMETER;

// Table 162 - TPMS_ECC_POINT Structure
typedef struct {
  TPM2B_ECC_PARAMETER x;
  TPM2B_ECC_PARAMETER y;
} TPMS_ECC_POINT;

// Table 163 -- TPM2B_ECC_POINT Structure <I/O>
typedef struct {
  UINT16         size;
  TPMS_ECC_POINT point;
} TPM2B_ECC_POINT;

// Table 164 - TPMI_ALG_ECC_SCHEME Type
typedef TPM_ALG_ID TPMI_ALG_ECC_SCHEME;

// Table 165 - TPMI_ECC_CURVE Type
typedef TPM_ECC_CURVE TPMI_ECC_CURVE;

// Table 166 - TPMT_ECC_SCHEME Structure
typedef struct {
  TPMI_ALG_ECC_SCHEME scheme;
  TPMU_SIG_SCHEME     details;
} TPMT_ECC_SCHEME;

// Table 167 - TPMS_ALGORITHM_DETAIL_ECC Structure
typedef struct {
  TPM_ECC_CURVE       curveID;
  UINT16              keySize;
  TPMT_KDF_SCHEME     kdf;
  TPMT_ECC_SCHEME     sign;
  TPM2B_ECC_PARAMETER p;
  TPM2B_ECC_PARAMETER a;
  TPM2B_ECC_PARAMETER b;
  TPM2B_ECC_PARAMETER gX;
  TPM2B_ECC_PARAMETER gY;
  TPM2B_ECC_PARAMETER n;
  TPM2B_ECC_PARAMETER h;
} TPMS_ALGORITHM_DETAIL_ECC;

// Table 168 - TPMS_SIGNATURE_RSASSA Structure
typedef struct {
  TPMI_ALG_HASH        hash;
  TPM2B_PUBLIC_KEY_RSA sig;
} TPMS_SIGNATURE_RSASSA;

// Table 169 - TPMS_SIGNATURE_RSAPSS Structure
typedef struct {
  TPMI_ALG_HASH        hash;
  TPM2B_PUBLIC_KEY_RSA sig;
} TPMS_SIGNATURE_RSAPSS;

// Table 170 - TPMS_SIGNATURE_ECDSA Structure
typedef struct {
  TPMI_ALG_HASH       hash;
  TPM2B_ECC_PARAMETER signatureR;
  TPM2B_ECC_PARAMETER signatureS;
} TPMS_SIGNATURE_ECDSA;

// Table 171 - TPMU_SIGNATURE Union
typedef union {
  TPMS_SIGNATURE_RSASSA rsassa;
  TPMS_SIGNATURE_RSAPSS rsapss;
  TPMS_SIGNATURE_ECDSA  ecdsa;
  TPMS_SIGNATURE_ECDSA  sm2;
  TPMS_SIGNATURE_ECDSA  ecdaa;
  TPMS_SIGNATURE_ECDSA  ecschnorr;
  TPMT_HA               hmac;
  TPMS_SCHEME_SIGHASH   any;
} TPMU_SIGNATURE;

// Table 172 - TPMT_SIGNATURE Structure
typedef struct {
  TPMI_ALG_SIG_SCHEME sigAlg;
  TPMU_SIGNATURE      signature;
} TPMT_SIGNATURE;

// Table 173 - TPMU_ENCRYPTED_SECRET Union
typedef union {
  BYTE ecc[sizeof(TPMS_ECC_POINT)];
  BYTE rsa[MAX_RSA_KEY_BYTES];
  BYTE symmetric[sizeof(TPM2B_DIGEST)];
  BYTE keyedHash[sizeof(TPM2B_DIGEST)];
} TPMU_ENCRYPTED_SECRET;

// Table 174 - TPM2B_ENCRYPTED_SECRET Structure
typedef struct {
  UINT16 size;
  BYTE   secret[sizeof(TPMU_ENCRYPTED_SECRET)];
} TPM2B_ENCRYPTED_SECRET;

// 12 Key/Object Complex

// Table 175 - TPMI_ALG_PUBLIC Type
typedef TPM_ALG_ID TPMI_ALG_PUBLIC;

// Table 176 - TPMU_PUBLIC_ID Union
typedef union {
  TPM2B_DIGEST         keyedHash;
  TPM2B_DIGEST         sym;
  TPM2B_PUBLIC_KEY_RSA rsa;
  TPMS_ECC_POINT       ecc;
} TPMU_PUBLIC_ID;

// Table 177 - TPMS_KEYEDHASH_PARMS Structure
typedef struct {
  TPMT_KEYEDHASH_SCHEME scheme;
} TPMS_KEYEDHASH_PARMS;

// Table 178 - TPMS_ASYM_PARMS Structure
typedef struct {
  TPMT_SYM_DEF_OBJECT symmetric;
  TPMT_ASYM_SCHEME    scheme;
} TPMS_ASYM_PARMS;

// Table 179 - TPMS_RSA_PARMS Structure
typedef struct {
  TPMT_SYM_DEF_OBJECT symmetric;
  TPMT_RSA_SCHEME     scheme;
  TPMI_RSA_KEY_BITS   keyBits;
  UINT32              exponent;
} TPMS_RSA_PARMS;

// Table 180 - TPMS_ECC_PARMS Structure
typedef struct {
  TPMT_SYM_DEF_OBJECT symmetric;
  TPMT_ECC_SCHEME     scheme;
  TPMI_ECC_CURVE      curveID;
  TPMT_KDF_SCHEME     kdf;
} TPMS_ECC_PARMS;

// Table 181 - TPMU_PUBLIC_PARMS Union
typedef union {
  TPMS_KEYEDHASH_PARMS keyedHashDetail;
  TPMT_SYM_DEF_OBJECT  symDetail;
  TPMS_RSA_PARMS       rsaDetail;
  TPMS_ECC_PARMS       eccDetail;
  TPMS_ASYM_PARMS      asymDetail;
} TPMU_PUBLIC_PARMS;

// Table 182 - TPMT_PUBLIC_PARMS Structure
typedef struct {
  TPMI_ALG_PUBLIC   type;
  TPMU_PUBLIC_PARMS parameters;
} TPMT_PUBLIC_PARMS;

// Table 183 - TPMT_PUBLIC Structure
typedef struct {
  TPMI_ALG_PUBLIC   type;
  TPMI_ALG_HASH     nameAlg;
  TPMA_OBJECT       objectAttributes;
  TPM2B_DIGEST      authPolicy;
  TPMU_PUBLIC_PARMS parameters;
  TPMU_PUBLIC_ID    unique;
} TPMT_PUBLIC;

// Table 184 - TPM2B_PUBLIC Structure
typedef struct {
  UINT16      size;
  TPMT_PUBLIC publicArea;
} TPM2B_PUBLIC;

// Table 185 - TPM2B_PRIVATE_VENDOR_SPECIFIC Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[PRIVATE_VENDOR_SPECIFIC_BYTES];
} TPM2B_PRIVATE_VENDOR_SPECIFIC;

// Table 186 - TPMU_SENSITIVE_COMPOSITE Union
typedef union {
  TPM2B_PRIVATE_KEY_RSA         rsa;
  TPM2B_ECC_PARAMETER           ecc;
  TPM2B_SENSITIVE_DATA          bits;
  TPM2B_SYM_KEY                 sym;
  TPM2B_PRIVATE_VENDOR_SPECIFIC any;
} TPMU_SENSITIVE_COMPOSITE;

// Table 187 - TPMT_SENSITIVE Structure
typedef struct {
  TPMI_ALG_PUBLIC          sensitiveType;
  TPM2B_AUTH               authValue;
  TPM2B_DIGEST             seedValue;
  TPMU_SENSITIVE_COMPOSITE sensitive;
} TPMT_SENSITIVE;

// Table 188 - TPM2B_SENSITIVE Structure
typedef struct {
  UINT16         size;
  TPMT_SENSITIVE sensitiveArea;
} TPM2B_SENSITIVE;

// Table 189 - _PRIVATE Structure
typedef struct {
  TPM2B_DIGEST   integrityOuter;
  TPM2B_DIGEST   integrityInner;
  TPMT_SENSITIVE sensitive;
} _PRIVATE;

// Table 190 - TPM2B_PRIVATE Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(_PRIVATE)];
} TPM2B_PRIVATE;

// Table 191 - _ID_OBJECT Structure
typedef struct {
  TPM2B_DIGEST integrityHMAC;
  TPM2B_DIGEST encIdentity;
} _ID_OBJECT;

// Table 192 - TPM2B_ID_OBJECT Structure
typedef struct {
  UINT16 size;
  BYTE   credential[sizeof(_ID_OBJECT)];
} TPM2B_ID_OBJECT;

// 13 NV Storage Structures

// Table 193 - TPM_NV_INDEX Bits
//
// NOTE: Comment here to resolve conflict
//
//typedef struct {
//  UINT32 index : 22;
//  UINT32 space : 2;
//  UINT32 RH_NV : 8;
//} TPM_NV_INDEX;

// Table 195 - TPMA_NV Bits
typedef struct {
  UINT32 TPMA_NV_PPWRITE        : 1;
  UINT32 TPMA_NV_OWNERWRITE     : 1;
  UINT32 TPMA_NV_AUTHWRITE      : 1;
  UINT32 TPMA_NV_POLICYWRITE    : 1;
  UINT32 TPMA_NV_COUNTER        : 1;
  UINT32 TPMA_NV_BITS           : 1;
  UINT32 TPMA_NV_EXTEND         : 1;
  UINT32 reserved7_9            : 3;
  UINT32 TPMA_NV_POLICY_DELETE  : 1;
  UINT32 TPMA_NV_WRITELOCKED    : 1;
  UINT32 TPMA_NV_WRITEALL       : 1;
  UINT32 TPMA_NV_WRITEDEFINE    : 1;
  UINT32 TPMA_NV_WRITE_STCLEAR  : 1;
  UINT32 TPMA_NV_GLOBALLOCK     : 1;
  UINT32 TPMA_NV_PPREAD         : 1;
  UINT32 TPMA_NV_OWNERREAD      : 1;
  UINT32 TPMA_NV_AUTHREAD       : 1;
  UINT32 TPMA_NV_POLICYREAD     : 1;
  UINT32 reserved20_24          : 5;
  UINT32 TPMA_NV_NO_DA          : 1;
  UINT32 TPMA_NV_ORDERLY        : 1;
  UINT32 TPMA_NV_CLEAR_STCLEAR  : 1;
  UINT32 TPMA_NV_READLOCKED     : 1;
  UINT32 TPMA_NV_WRITTEN        : 1;
  UINT32 TPMA_NV_PLATFORMCREATE : 1;
  UINT32 TPMA_NV_READ_STCLEAR   : 1;
} TPMA_NV;

// Table 196 - TPMS_NV_PUBLIC Structure
typedef struct {
  TPMI_RH_NV_INDEX nvIndex;
  TPMI_ALG_HASH    nameAlg;
  TPMA_NV          attributes;
  TPM2B_DIGEST     authPolicy;
  UINT16           dataSize;
} TPMS_NV_PUBLIC;

// Table 197 - TPM2B_NV_PUBLIC Structure
typedef struct {
  UINT16         size;
  TPMS_NV_PUBLIC nvPublic;
} TPM2B_NV_PUBLIC;

// 14 Context Data

// Table 198 - TPM2B_CONTEXT_SENSITIVE Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[MAX_CONTEXT_SIZE];
} TPM2B_CONTEXT_SENSITIVE;

// Table 199 - TPMS_CONTEXT_DATA Structure
typedef struct {
  TPM2B_DIGEST            integrity;
  TPM2B_CONTEXT_SENSITIVE encrypted;
} TPMS_CONTEXT_DATA;

// Table 200 - TPM2B_CONTEXT_DATA Structure
typedef struct {
  UINT16 size;
  BYTE   buffer[sizeof(TPMS_CONTEXT_DATA)];
} TPM2B_CONTEXT_DATA;

// Table 201 - TPMS_CONTEXT Structure
typedef struct {
  UINT64             sequence;
  TPMI_DH_CONTEXT    savedHandle;
  TPMI_RH_HIERARCHY  hierarchy;
  TPM2B_CONTEXT_DATA contextBlob;
} TPMS_CONTEXT;

// 15 Creation Data

// Table 203 - TPMS_CREATION_DATA Structure
typedef struct {
  TPML_PCR_SELECTION pcrSelect;
  TPM2B_DIGEST       pcrDigest;
  TPMA_LOCALITY      locality;
  TPM_ALG_ID         parentNameAlg;
  TPM2B_NAME         parentName;
  TPM2B_NAME         parentQualifiedName;
  TPM2B_DATA         outsideInfo;
} TPMS_CREATION_DATA;

// Table 204 - TPM2B_CREATION_DATA Structure
typedef struct {
  UINT16             size;
  TPMS_CREATION_DATA creationData;
} TPM2B_CREATION_DATA;


//
// Command Header
//
typedef struct {
  TPM_ST tag;
  UINT32 paramSize;
  TPM_CC commandCode;
} TPM2_COMMAND_HEADER;

typedef struct {
  TPM_ST tag;
  UINT32 paramSize;
  TPM_RC responseCode;
} TPM2_RESPONSE_HEADER;

#pragma pack ()

//
// TCG Algorithm Registry
//
#define HASH_ALG_SHA1    0x00000001
#define HASH_ALG_SHA256  0x00000002
#define HASH_ALG_SHA384  0x00000004
#define HASH_ALG_SHA512  0x00000008
#define HASH_ALG_SM3_256 0x00000010

#endif
