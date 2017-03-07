#------------------------------------------------------------------------------ ;
# Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.<BR>
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
#   RdRand.S
#
# Abstract:
#
#   Generates random number through CPU RdRand instruction under 32-bit platform.
#
# Notes:
#
#------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Generates a 16 bit random number through RDRAND instruction.
//  Return TRUE if Rand generated successfully, or FALSE if not.
//
//  BOOLEAN EFIAPI InternalX86RdRand16 (UINT16 *Rand);
//------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalX86RdRand16)
ASM_PFX(InternalX86RdRand16):
    .byte  0x0f, 0xc7, 0xf0        // rdrand r16: "0f c7 /6  ModRM:r/m(w)"
    jc     rn16_ok                 // jmp if CF=1
    xor    %eax, %eax              // reg=0 if CF=0
    ret                            // return with failure status
rn16_ok:
    mov    0x4(%esp), %edx
    mov    %ax, (%edx)
    mov    $0x1, %eax
    ret

//------------------------------------------------------------------------------
//  Generates a 32 bit random number through RDRAND instruction.
//  Return TRUE if Rand generated successfully, or FALSE if not.
//
//  BOOLEAN EFIAPI InternalX86RdRand32 (UINT32 *Rand);
//------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalX86RdRand32)
ASM_PFX(InternalX86RdRand32):
    .byte  0x0f, 0xc7, 0xf0        // rdrand r32: "0f c7 /6  ModRM:r/m(w)"
    jc     rn32_ok                 // jmp if CF=1
    xor    %eax, %eax              // reg=0 if CF=0
    ret                            // return with failure status
rn32_ok:
    mov    0x4(%esp), %edx
    mov    %eax, (%edx)
    mov    $0x1, %eax
    ret

//------------------------------------------------------------------------------
//  Generates a 64 bit random number through RDRAND instruction.
//  Return TRUE if Rand generated successfully, or FALSE if not.
//
//  BOOLEAN EFIAPI InternalX86RdRand64 (UINT64 *Rand);
//------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalX86RdRand64)
ASM_PFX(InternalX86RdRand64):
    .byte  0x0f, 0xc7, 0xf0        // rdrand r32: "0f c7 /6  ModRM:r/m(w)"
    jnc    rn64_ret                // jmp if CF=0
    mov    0x4(%esp), %edx
    mov    %eax, (%edx)

    .byte  0x0f, 0xc7, 0xf0        // generate another 32 bit RN
    jnc    rn64_ret                // jmp if CF=0
    mov    %eax, 0x4(%edx)

    mov    $0x1, %eax
    ret
rn64_ret:
    xor    %eax, %eax
    ret                            // return with failure status
