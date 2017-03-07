#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
# Module Name:
#
#   DisableCache.S
#
# Abstract:
#
#   Set the CD bit of CR0 to 1, clear the NW bit of CR0 to 0, and flush all caches with a
#   WBINVD instruction.
#
# Notes:
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# AsmDisableCache (
#   VOID
#   );
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(AsmDisableCache)
ASM_PFX(AsmDisableCache):
    movl    %cr0, %eax
    btsl    $30, %eax
    btrl    $29, %eax
    movl    %eax, %cr0
    wbinvd
    ret
