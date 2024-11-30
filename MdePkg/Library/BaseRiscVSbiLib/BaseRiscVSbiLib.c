/** @file
  Instance of the SBI ecall library.

  It allows calling an SBI function via an ecall from S-Mode.

  Copyright (c) 2021-2022, Hewlett Packard Development LP. All rights reserved.<BR>
  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseRiscVSbiLib.h>

//
// Maximum arguments for SBI ecall
#define SBI_CALL_MAX_ARGS  6

/**
  Call SBI call using ecall instruction.

  Asserts when NumArgs exceeds SBI_CALL_MAX_ARGS.

  @param[in] ExtId    SBI extension ID.
  @param[in] FuncId   SBI function ID.
  @param[in] NumArgs  Number of arguments to pass to the ecall.
  @param[in] ...      Argument list for the ecall.

  @retval  Returns SBI_RET structure with value and error code.

**/
SBI_RET
EFIAPI
SbiCall (
  IN  UINTN  ExtId,
  IN  UINTN  FuncId,
  IN  UINTN  NumArgs,
  ...
  )
{
  UINTN    I;
  SBI_RET  Ret;
  UINTN    Args[SBI_CALL_MAX_ARGS];
  VA_LIST  ArgList;

  VA_START (ArgList, NumArgs);

  if (NumArgs > SBI_CALL_MAX_ARGS) {
    Ret.Error = SBI_ERR_INVALID_PARAM;
    Ret.Value = -1;
    return Ret;
  }

  for (I = 0; I < SBI_CALL_MAX_ARGS; I++) {
    if (I < NumArgs) {
      Args[I] = VA_ARG (ArgList, UINTN);
    } else {
      // Default to 0 for all arguments that are not given
      Args[I] = 0;
    }
  }

  VA_END (ArgList);

  // ECALL updates the a0 and a1 registers as return values.
  RiscVSbiEcall (
    &Args[0],
    &Args[1],
    Args[2],
    Args[3],
    Args[4],
    Args[5],
    (UINTN)(FuncId),
    (UINTN)(ExtId)
    );

  Ret.Error = Args[0];
  Ret.Value = Args[1];
  return Ret;
}

/**
  Translate SBI error code to EFI status.

  @param[in] SbiError   SBI error code
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
TranslateError (
  IN  UINTN  SbiError
  )
{
  switch (SbiError) {
    case SBI_SUCCESS:
      return EFI_SUCCESS;
    case SBI_ERR_FAILED:
      return EFI_DEVICE_ERROR;
      break;
    case SBI_ERR_NOT_SUPPORTED:
      return EFI_UNSUPPORTED;
      break;
    case SBI_ERR_INVALID_PARAM:
      return EFI_INVALID_PARAMETER;
      break;
    case SBI_ERR_DENIED:
      return EFI_ACCESS_DENIED;
      break;
    case SBI_ERR_INVALID_ADDRESS:
      return EFI_LOAD_ERROR;
      break;
    case SBI_ERR_ALREADY_AVAILABLE:
      return EFI_ALREADY_STARTED;
      break;
    default:
      //
      // Reaches here only if SBI has defined a new error type
      //
      ASSERT (FALSE);
      return EFI_UNSUPPORTED;
      break;
  }
}

/**
  Clear pending timer interrupt bit and set timer for next event after Time.

  To clear the timer without scheduling a timer event, set Time to a
  practically infinite value or mask the timer interrupt by clearing sie.STIE.

  @param[in]  Time                 The time offset to the next scheduled timer interrupt.
**/
VOID
EFIAPI
SbiSetTimer (
  IN  UINT64  Time
  )
{
  SbiCall (SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, 1, Time);
}

/**
  Reset the system using SRST SBI extenion

  @param[in]  ResetType            The SRST System Reset Type.
  @param[in]  ResetReason          The SRST System Reset Reason.
**/
EFI_STATUS
EFIAPI
SbiSystemReset (
  IN  UINTN  ResetType,
  IN  UINTN  ResetReason
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (
          SBI_EXT_SRST,
          SBI_EXT_SRST_RESET,
          2,
          ResetType,
          ResetReason
          );

  return TranslateError (Ret.Error);
}

/**
  Get firmware context of the calling hart.

  @param[out] FirmwareContext      The firmware context pointer.
**/
VOID
EFIAPI
GetFirmwareContext (
  OUT EFI_RISCV_FIRMWARE_CONTEXT  **FirmwareContext
  )
{
  *FirmwareContext = (EFI_RISCV_FIRMWARE_CONTEXT *)RiscVGetSupervisorScratch ();
}

/**
  Set firmware context of the calling hart.

  @param[in] FirmwareContext       The firmware context pointer.
**/
VOID
EFIAPI
SetFirmwareContext (
  IN EFI_RISCV_FIRMWARE_CONTEXT  *FirmwareContext
  )
{
  RiscVSetSupervisorScratch ((UINT64)FirmwareContext);
}

/**
  Get pointer to OpenSBI Firmware Context

  Get the pointer of firmware context through OpenSBI FW Extension SBI.

  @param    FirmwareContextPtr   Pointer to retrieve pointer to the
                                 Firmware Context.
**/
VOID
EFIAPI
GetFirmwareContextPointer (
  IN OUT EFI_RISCV_FIRMWARE_CONTEXT  **FirmwareContextPtr
  )
{
  GetFirmwareContext (FirmwareContextPtr);
}

/**
  Set the pointer to OpenSBI Firmware Context

  Set the pointer of firmware context through OpenSBI FW Extension SBI.

  @param    FirmwareContextPtr   Pointer to Firmware Context.
**/
VOID
EFIAPI
SetFirmwareContextPointer (
  IN EFI_RISCV_FIRMWARE_CONTEXT  *FirmwareContextPtr
  )
{
  SetFirmwareContext (FirmwareContextPtr);
}
