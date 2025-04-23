#------------------------------------------------------------------------------
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# Module Name:
#
#   DynamicCookie.S
#
# Abstract:
#
#   Generates random number through the RNDR instruction on a 64-bit AARCH64 platform
#   to store a random value in the GCC __stack_check_guard stack cookie.
#   The first byte is 0'd to prevent string copy functions from clobbering
#   the stack cookie.
#
# Notes:
#
# If RNDR fails, the build time static stack cookie value will be used instead.
#
#------------------------------------------------------------------------------

#include <AArch64/AArch64.h>

.text
.p2align 2

GCC_ASM_IMPORT(__stack_chk_guard)
GCC_ASM_IMPORT(_CModuleEntryPoint)
GCC_ASM_EXPORT(_ModuleEntryPoint)

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# _ModuleEntryPoint (
#   Parameters are passed through.
# )
#------------------------------------------------------------------------------
ASM_PFX(_ModuleEntryPoint):
  AARCH64_BTI(c)

  mrs x9, ID_AA64ISAR0_EL1          // Read the AArch64 Instruction Set Attribute Register 0
  ubfx x9, x9, #60, #4              // Extract the RNDR bit field (bits 60-63)
  cbz x9, c_entry                   // If RNDR is not supported, jump to c_entry

  mrs x9, RNDR                      // Generate a random number
  b.eq c_entry                      // RNDR sets NZCV to 0b0100 on failure
                                    // So if the zero flag is set, use the static stack guard

  and x9, x9, #0xFFFFFFFFFFFFFF00   // Zero the first byte of the random value

  adrp x8, ASM_PFX(__stack_chk_guard)             // Load the page address of __stack_chk_guard
  str x9, [x8, :lo12:ASM_PFX(__stack_chk_guard)]  // Store the random value in __stack_chk_guard

c_entry:
  b ASM_PFX(_CModuleEntryPoint)     // Jump to the C module entry point
