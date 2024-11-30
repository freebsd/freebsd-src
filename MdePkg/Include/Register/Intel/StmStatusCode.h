/** @file
  STM Status Codes

  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  SMI Transfer Monitor (STM) User Guide Revision 1.00

**/

#ifndef _INTEL_STM_STATUS_CODE_H_
#define _INTEL_STM_STATUS_CODE_H_

/**
  STM Status Codes
**/
typedef UINT32 STM_STATUS;

/**
  Success code have BIT31 clear.
  All error codes have BIT31 set.
  STM errors have BIT16 set.
  SMM errors have BIT17 set
  Errors that apply to both STM and SMM have bits BIT15, BT16, and BIT17 set.
  STM TXT.ERRORCODE codes have BIT30 set.
  @{
**/
#define STM_SUCCESS                             0x00000000
#define SMM_SUCCESS                             0x00000000
#define ERROR_STM_SECURITY_VIOLATION            (BIT31 | BIT16 | 0x0001)
#define ERROR_STM_CACHE_TYPE_NOT_SUPPORTED      (BIT31 | BIT16 | 0x0002)
#define ERROR_STM_PAGE_NOT_FOUND                (BIT31 | BIT16 | 0x0003)
#define ERROR_STM_BAD_CR3                       (BIT31 | BIT16 | 0x0004)
#define ERROR_STM_PHYSICAL_OVER_4G              (BIT31 | BIT16 | 0x0005)
#define ERROR_STM_VIRTUAL_SPACE_TOO_SMALL       (BIT31 | BIT16 | 0x0006)
#define ERROR_STM_UNPROTECTABLE_RESOURCE        (BIT31 | BIT16 | 0x0007)
#define ERROR_STM_ALREADY_STARTED               (BIT31 | BIT16 | 0x0008)
#define ERROR_STM_WITHOUT_SMX_UNSUPPORTED       (BIT31 | BIT16 | 0x0009)
#define ERROR_STM_STOPPED                       (BIT31 | BIT16 | 0x000A)
#define ERROR_STM_BUFFER_TOO_SMALL              (BIT31 | BIT16 | 0x000B)
#define ERROR_STM_INVALID_VMCS_DATABASE         (BIT31 | BIT16 | 0x000C)
#define ERROR_STM_MALFORMED_RESOURCE_LIST       (BIT31 | BIT16 | 0x000D)
#define ERROR_STM_INVALID_PAGECOUNT             (BIT31 | BIT16 | 0x000E)
#define ERROR_STM_LOG_ALLOCATED                 (BIT31 | BIT16 | 0x000F)
#define ERROR_STM_LOG_NOT_ALLOCATED             (BIT31 | BIT16 | 0x0010)
#define ERROR_STM_LOG_NOT_STOPPED               (BIT31 | BIT16 | 0x0011)
#define ERROR_STM_LOG_NOT_STARTED               (BIT31 | BIT16 | 0x0012)
#define ERROR_STM_RESERVED_BIT_SET              (BIT31 | BIT16 | 0x0013)
#define ERROR_STM_NO_EVENTS_ENABLED             (BIT31 | BIT16 | 0x0014)
#define ERROR_STM_OUT_OF_RESOURCES              (BIT31 | BIT16 | 0x0015)
#define ERROR_STM_FUNCTION_NOT_SUPPORTED        (BIT31 | BIT16 | 0x0016)
#define ERROR_STM_UNPROTECTABLE                 (BIT31 | BIT16 | 0x0017)
#define ERROR_STM_UNSUPPORTED_MSR_BIT           (BIT31 | BIT16 | 0x0018)
#define ERROR_STM_UNSPECIFIED                   (BIT31 | BIT16 | 0xFFFF)
#define ERROR_SMM_BAD_BUFFER                    (BIT31 | BIT17 | 0x0001)
#define ERROR_SMM_INVALID_RSC                   (BIT31 | BIT17 | 0x0004)
#define ERROR_SMM_INVALID_BUFFER_SIZE           (BIT31 | BIT17 | 0x0005)
#define ERROR_SMM_BUFFER_TOO_SHORT              (BIT31 | BIT17 | 0x0006)
#define ERROR_SMM_INVALID_LIST                  (BIT31 | BIT17 | 0x0007)
#define ERROR_SMM_OUT_OF_MEMORY                 (BIT31 | BIT17 | 0x0008)
#define ERROR_SMM_AFTER_INIT                    (BIT31 | BIT17 | 0x0009)
#define ERROR_SMM_UNSPECIFIED                   (BIT31 | BIT17 | 0xFFFF)
#define ERROR_INVALID_API                       (BIT31 | BIT17 | BIT16 | BIT15 | 0x0001)
#define ERROR_INVALID_PARAMETER                 (BIT31 | BIT17 | BIT16 | BIT15 | 0x0002)
#define STM_CRASH_PROTECTION_EXCEPTION          (BIT31 | BIT30 | 0xF001)
#define STM_CRASH_PROTECTION_EXCEPTION_FAILURE  (BIT31 | BIT30 | 0xF002)
#define STM_CRASH_DOMAIN_DEGRADATION_FAILURE    (BIT31 | BIT30 | 0xF003)
#define STM_CRASH_BIOS_PANIC                    (BIT31 | BIT30 | 0xE000)
/// @}

#endif
