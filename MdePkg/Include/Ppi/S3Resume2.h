/** @file
  This PPI produces functions to interpret and execute the PI boot script table.

  This PPI is published by a PEIM and provides for the restoration of the platform's
  configuration when resuming from the ACPI S3 power state. The ability to execute
  the boot script may depend on the availability of other PPIs. For example, if
  the boot script includes an SMBus command, this PEIM looks for the relevant PPI
  that is able to execute that command.

  Copyright (c) 2010 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Volume 5:
  Standards

**/

#ifndef __PEI_S3_RESUME_PPI_H__
#define __PEI_S3_RESUME_PPI_H__

///
/// Global ID for EFI_PEI_S3_RESUME2_PPI
///
#define EFI_PEI_S3_RESUME2_PPI_GUID \
  { \
    0x6D582DBC, 0xDB85, 0x4514, {0x8F, 0xCC, 0x5A, 0xDF, 0x62, 0x27, 0xB1, 0x47 } \
  }

///
/// Forward declaration for EFI_PEI_S3_RESUME_PPI
///
typedef struct _EFI_PEI_S3_RESUME2_PPI  EFI_PEI_S3_RESUME2_PPI;

/**
  Restores the platform to its preboot configuration for an S3 resume and
  jumps to the OS waking vector.

  This function will restore the platform to its pre-boot configuration that was
  pre-stored in the boot script table and transfer control to OS waking vector.
  Upon invocation, this function is responsible for locating the following
  information before jumping to OS waking vector:
    - ACPI tables
    - boot script table
    - any other information that it needs

  The S3RestoreConfig() function then executes the pre-stored boot script table
  and transitions the platform to the pre-boot state. The boot script is recorded
  during regular boot using the EFI_S3_SAVE_STATE_PROTOCOL.Write() and
  EFI_S3_SMM_SAVE_STATE_PROTOCOL.Write() functions.  Finally, this function
  transfers control to the OS waking vector. If the OS supports only a real-mode
  waking vector, this function will switch from flat mode to real mode before
  jumping to the waking vector.  If all platform pre-boot configurations are
  successfully restored and all other necessary information is ready, this
  function will never return and instead will directly jump to the OS waking
  vector. If this function returns, it indicates that the attempt to resume
  from the ACPI S3 sleep state failed.

  @param[in] This   Pointer to this instance of the PEI_S3_RESUME_PPI

  @retval EFI_ABORTED     Execution of the S3 resume boot script table failed.
  @retval EFI_NOT_FOUND   Some necessary information that is used for the S3
                          resume boot path could not be located.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_S3_RESUME_PPI_RESTORE_CONFIG2)(
  IN EFI_PEI_S3_RESUME2_PPI  *This
  );

/**
  EFI_PEI_S3_RESUME2_PPI accomplishes the firmware S3 resume boot
  path and transfers control to OS.
**/
struct _EFI_PEI_S3_RESUME2_PPI {
  ///
  /// Restores the platform to its preboot configuration for an S3 resume and
  /// jumps to the OS waking vector.
  ///
  EFI_PEI_S3_RESUME_PPI_RESTORE_CONFIG2  S3RestoreConfig2;
};

extern EFI_GUID gEfiPeiS3Resume2PpiGuid;

#endif
