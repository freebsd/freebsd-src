/** @file
  This file declares UEFI PI Multi-processor PPI.
  This PPI is installed by some platform or chipset-specific PEIM that abstracts
  handling multiprocessor support.

  Copyright (c) 2015 - 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.4.

**/

#ifndef __PEI_MP_SERVICES_PPI_H__
#define __PEI_MP_SERVICES_PPI_H__

#include <Protocol/MpService.h>

#define EFI_PEI_MP_SERVICES_PPI_GUID \
  { \
    0xee16160a, 0xe8be, 0x47a6, { 0x82, 0xa, 0xc6, 0x90, 0xd, 0xb0, 0x25, 0xa } \
  }

typedef struct _EFI_PEI_MP_SERVICES_PPI  EFI_PEI_MP_SERVICES_PPI ;

/**
  Get the number of CPU's.

  @param[in]  PeiServices         An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in]  This                Pointer to this instance of the PPI.
  @param[out] NumberOfProcessors  Pointer to the total number of logical processors in
                                  the system, including the BSP and disabled APs.
  @param[out] NumberOfEnabledProcessors
                                  Number of processors in the system that are enabled.

  @retval EFI_SUCCESS             The number of logical processors and enabled
                                  logical processors was retrieved.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_INVALID_PARAMETER   NumberOfProcessors is NULL.
                                  NumberOfEnabledProcessors is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_GET_NUMBER_OF_PROCESSORS) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  OUT UINTN                       *NumberOfProcessors,
  OUT UINTN                       *NumberOfEnabledProcessors
  );

/**
  Get information on a specific CPU.

  @param[in]  PeiServices         An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in]  This                Pointer to this instance of the PPI.
  @param[in]  ProcessorNumber     Pointer to the total number of logical processors in
                                  the system, including the BSP and disabled APs.
  @param[out] ProcessorInfoBuffer Number of processors in the system that are enabled.

  @retval EFI_SUCCESS             Processor information was returned.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_INVALID_PARAMETER   ProcessorInfoBuffer is NULL.
  @retval EFI_NOT_FOUND           The processor with the handle specified by
                                  ProcessorNumber does not exist in the platform.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_GET_PROCESSOR_INFO) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  IN  UINTN                       ProcessorNumber,
  OUT EFI_PROCESSOR_INFORMATION   *ProcessorInfoBuffer
  );

/**
  Activate all of the application processors.

  @param[in] PeiServices          An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in] This                 A pointer to the EFI_PEI_MP_SERVICES_PPI instance.
  @param[in] Procedure            A pointer to the function to be run on enabled APs of
                                  the system.
  @param[in] SingleThread         If TRUE, then all the enabled APs execute the function
                                  specified by Procedure one by one, in ascending order
                                  of processor handle number. If FALSE, then all the
                                  enabled APs execute the function specified by Procedure
                                  simultaneously.
  @param[in] TimeoutInMicroSeconds
                                  Indicates the time limit in microseconds for APs to
                                  return from Procedure, for blocking mode only. Zero
                                  means infinity. If the timeout expires before all APs
                                  return from Procedure, then Procedure on the failed APs
                                  is terminated. All enabled APs are available for next
                                  function assigned by EFI_PEI_MP_SERVICES_PPI.StartupAllAPs()
                                  or EFI_PEI_MP_SERVICES_PPI.StartupThisAP(). If the
                                  timeout expires in blocking mode, BSP returns
                                  EFI_TIMEOUT.
  @param[in] ProcedureArgument    The parameter passed into Procedure for all APs.

  @retval EFI_SUCCESS             In blocking mode, all APs have finished before the
                                  timeout expired.
  @retval EFI_DEVICE_ERROR        Caller processor is AP.
  @retval EFI_NOT_STARTED         No enabled APs exist in the system.
  @retval EFI_NOT_READY           Any enabled APs are busy.
  @retval EFI_TIMEOUT             In blocking mode, the timeout expired before all
                                  enabled APs have finished.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_STARTUP_ALL_APS) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  IN  EFI_AP_PROCEDURE            Procedure,
  IN  BOOLEAN                     SingleThread,
  IN  UINTN                       TimeoutInMicroSeconds,
  IN  VOID                        *ProcedureArgument      OPTIONAL
  );

/**
  Activate a specific application processor.

  @param[in] PeiServices          An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in] This                 A pointer to the EFI_PEI_MP_SERVICES_PPI instance.
  @param[in] Procedure            A pointer to the function to be run on enabled APs of
                                  the system.
  @param[in] ProcessorNumber      The handle number of the AP. The range is from 0 to the
                                  total number of logical processors minus 1. The total
                                  number of logical processors can be retrieved by
                                  EFI_PEI_MP_SERVICES_PPI.GetNumberOfProcessors().
  @param[in] TimeoutInMicroSeconds
                                  Indicates the time limit in microseconds for APs to
                                  return from Procedure, for blocking mode only. Zero
                                  means infinity. If the timeout expires before all APs
                                  return from Procedure, then Procedure on the failed APs
                                  is terminated. All enabled APs are available for next
                                  function assigned by EFI_PEI_MP_SERVICES_PPI.StartupAllAPs()
                                  or EFI_PEI_MP_SERVICES_PPI.StartupThisAP(). If the
                                  timeout expires in blocking mode, BSP returns
                                  EFI_TIMEOUT.
  @param[in] ProcedureArgument    The parameter passed into Procedure for all APs.

  @retval EFI_SUCCESS             In blocking mode, specified AP finished before the
                                  timeout expires.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_TIMEOUT             In blocking mode, the timeout expired before the
                                  specified AP has finished.
  @retval EFI_NOT_FOUND           The processor with the handle specified by
                                  ProcessorNumber does not exist.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber specifies the BSP or disabled AP.
  @retval EFI_INVALID_PARAMETER   Procedure is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_STARTUP_THIS_AP) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  IN  EFI_AP_PROCEDURE            Procedure,
  IN  UINTN                       ProcessorNumber,
  IN  UINTN                       TimeoutInMicroseconds,
  IN  VOID                        *ProcedureArgument      OPTIONAL
  );

/**
  Switch the boot strap processor.

  @param[in] PeiServices          An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in] This                 A pointer to the EFI_PEI_MP_SERVICES_PPI instance.
  @param[in] ProcessorNumber      The handle number of the AP. The range is from 0 to the
                                  total number of logical processors minus 1. The total
                                  number of logical processors can be retrieved by
                                  EFI_PEI_MP_SERVICES_PPI.GetNumberOfProcessors().
  @param[in] EnableOldBSP         If TRUE, then the old BSP will be listed as an enabled
                                  AP. Otherwise, it will be disabled.

  @retval EFI_SUCCESS             BSP successfully switched.
  @retval EFI_UNSUPPORTED         Switching the BSP cannot be completed prior to this
                                  service returning.
  @retval EFI_UNSUPPORTED         Switching the BSP is not supported.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_NOT_FOUND           The processor with the handle specified by
                                  ProcessorNumber does not exist.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber specifies the current BSP or a disabled
                                  AP.
  @retval EFI_NOT_READY           The specified AP is busy.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_SWITCH_BSP) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  IN  UINTN                       ProcessorNumber,
  IN  BOOLEAN                     EnableOldBSP
  );

/**
  Enable or disable an application processor.

  @param[in] PeiServices          An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in] This                 A pointer to the EFI_PEI_MP_SERVICES_PPI instance.
  @param[in] ProcessorNumber      The handle number of the AP. The range is from 0 to the
                                  total number of logical processors minus 1. The total
                                  number of logical processors can be retrieved by
                                  EFI_PEI_MP_SERVICES_PPI.GetNumberOfProcessors().
  @param[in] EnableAP             Specifies the new state for the processor for enabled,
                                  FALSE for disabled.
  @param[in] HealthFlag           If not NULL, a pointer to a value that specifies the
                                  new health status of the AP. This flag corresponds to
                                  StatusFlag defined in EFI_PEI_MP_SERVICES_PPI.GetProcessorInfo().
                                  Only the PROCESSOR_HEALTH_STATUS_BIT is used. All other
                                  bits are ignored. If it is NULL, this parameter is
                                  ignored.

  @retval EFI_SUCCESS             The specified AP was enabled or disabled successfully.
  @retval EFI_UNSUPPORTED         Enabling or disabling an AP cannot be completed prior
                                  to this service returning.
  @retval EFI_UNSUPPORTED         Enabling or disabling an AP is not supported.
  @retval EFI_DEVICE_ERROR        The calling processor is an AP.
  @retval EFI_NOT_FOUND           Processor with the handle specified by ProcessorNumber
                                  does not exist.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber specifies the BSP.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_ENABLEDISABLEAP) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  IN  UINTN                       ProcessorNumber,
  IN  BOOLEAN                     EnableAP,
  IN  UINT32                      *HealthFlag      OPTIONAL
  );

/**
  Identify the currently executing processor.

  @param[in]  PeiServices         An indirect pointer to the PEI Services Table
                                  published by the PEI Foundation.
  @param[in]  This                A pointer to the EFI_PEI_MP_SERVICES_PPI instance.
  @param[out] ProcessorNumber     The handle number of the AP. The range is from 0 to the
                                  total number of logical processors minus 1. The total
                                  number of logical processors can be retrieved by
                                  EFI_PEI_MP_SERVICES_PPI.GetNumberOfProcessors().

  @retval EFI_SUCCESS             The current processor handle number was returned in
                                  ProcessorNumber.
  @retval EFI_INVALID_PARAMETER   ProcessorNumber is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MP_SERVICES_WHOAMI) (
  IN  CONST EFI_PEI_SERVICES      **PeiServices,
  IN  EFI_PEI_MP_SERVICES_PPI     *This,
  OUT UINTN                       *ProcessorNumber
  );

///
/// This PPI is installed by some platform or chipset-specific PEIM that abstracts
/// handling multiprocessor support.
///
struct _EFI_PEI_MP_SERVICES_PPI {
  EFI_PEI_MP_SERVICES_GET_NUMBER_OF_PROCESSORS   GetNumberOfProcessors;
  EFI_PEI_MP_SERVICES_GET_PROCESSOR_INFO         GetProcessorInfo;
  EFI_PEI_MP_SERVICES_STARTUP_ALL_APS            StartupAllAPs;
  EFI_PEI_MP_SERVICES_STARTUP_THIS_AP            StartupThisAP;
  EFI_PEI_MP_SERVICES_SWITCH_BSP                 SwitchBSP;
  EFI_PEI_MP_SERVICES_ENABLEDISABLEAP            EnableDisableAP;
  EFI_PEI_MP_SERVICES_WHOAMI                     WhoAmI;
};

extern EFI_GUID gEfiPeiMpServicesPpiGuid;

#endif
