/** @file
  DebugSupport protocol and supporting definitions as defined in the UEFI2.4
  specification.

  The DebugSupport protocol is used by source level debuggers to abstract the
  processor and handle context save and restore operations.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2011 - 2013, ARM Ltd. All rights reserved.<BR>
Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEBUG_SUPPORT_H__
#define __DEBUG_SUPPORT_H__

#include <IndustryStandard/PeImage.h>

typedef struct _EFI_DEBUG_SUPPORT_PROTOCOL EFI_DEBUG_SUPPORT_PROTOCOL;

///
/// Debug Support protocol {2755590C-6F3C-42FA-9EA4-A3BA543CDA25}.
///
#define EFI_DEBUG_SUPPORT_PROTOCOL_GUID \
  { \
    0x2755590C, 0x6F3C, 0x42FA, {0x9E, 0xA4, 0xA3, 0xBA, 0x54, 0x3C, 0xDA, 0x25 } \
  }

///
/// Processor exception to be hooked.
/// All exception types for IA32, X64, Itanium and EBC processors are defined.
///
typedef INTN  EFI_EXCEPTION_TYPE;

///
///  IA-32 processor exception types.
///
#define EXCEPT_IA32_DIVIDE_ERROR    0
#define EXCEPT_IA32_DEBUG           1
#define EXCEPT_IA32_NMI             2
#define EXCEPT_IA32_BREAKPOINT      3
#define EXCEPT_IA32_OVERFLOW        4
#define EXCEPT_IA32_BOUND           5
#define EXCEPT_IA32_INVALID_OPCODE  6
#define EXCEPT_IA32_DOUBLE_FAULT    8
#define EXCEPT_IA32_INVALID_TSS     10
#define EXCEPT_IA32_SEG_NOT_PRESENT 11
#define EXCEPT_IA32_STACK_FAULT     12
#define EXCEPT_IA32_GP_FAULT        13
#define EXCEPT_IA32_PAGE_FAULT      14
#define EXCEPT_IA32_FP_ERROR        16
#define EXCEPT_IA32_ALIGNMENT_CHECK 17
#define EXCEPT_IA32_MACHINE_CHECK   18
#define EXCEPT_IA32_SIMD            19

///
/// FXSAVE_STATE.
/// FP / MMX / XMM registers (see fxrstor instruction definition).
///
typedef struct {
  UINT16  Fcw;
  UINT16  Fsw;
  UINT16  Ftw;
  UINT16  Opcode;
  UINT32  Eip;
  UINT16  Cs;
  UINT16  Reserved1;
  UINT32  DataOffset;
  UINT16  Ds;
  UINT8   Reserved2[10];
  UINT8   St0Mm0[10], Reserved3[6];
  UINT8   St1Mm1[10], Reserved4[6];
  UINT8   St2Mm2[10], Reserved5[6];
  UINT8   St3Mm3[10], Reserved6[6];
  UINT8   St4Mm4[10], Reserved7[6];
  UINT8   St5Mm5[10], Reserved8[6];
  UINT8   St6Mm6[10], Reserved9[6];
  UINT8   St7Mm7[10], Reserved10[6];
  UINT8   Xmm0[16];
  UINT8   Xmm1[16];
  UINT8   Xmm2[16];
  UINT8   Xmm3[16];
  UINT8   Xmm4[16];
  UINT8   Xmm5[16];
  UINT8   Xmm6[16];
  UINT8   Xmm7[16];
  UINT8   Reserved11[14 * 16];
} EFI_FX_SAVE_STATE_IA32;

///
///  IA-32 processor context definition.
///
typedef struct {
  UINT32                 ExceptionData;
  EFI_FX_SAVE_STATE_IA32 FxSaveState;
  UINT32                 Dr0;
  UINT32                 Dr1;
  UINT32                 Dr2;
  UINT32                 Dr3;
  UINT32                 Dr6;
  UINT32                 Dr7;
  UINT32                 Cr0;
  UINT32                 Cr1;  /* Reserved */
  UINT32                 Cr2;
  UINT32                 Cr3;
  UINT32                 Cr4;
  UINT32                 Eflags;
  UINT32                 Ldtr;
  UINT32                 Tr;
  UINT32                 Gdtr[2];
  UINT32                 Idtr[2];
  UINT32                 Eip;
  UINT32                 Gs;
  UINT32                 Fs;
  UINT32                 Es;
  UINT32                 Ds;
  UINT32                 Cs;
  UINT32                 Ss;
  UINT32                 Edi;
  UINT32                 Esi;
  UINT32                 Ebp;
  UINT32                 Esp;
  UINT32                 Ebx;
  UINT32                 Edx;
  UINT32                 Ecx;
  UINT32                 Eax;
} EFI_SYSTEM_CONTEXT_IA32;

///
///  x64 processor exception types.
///
#define EXCEPT_X64_DIVIDE_ERROR    0
#define EXCEPT_X64_DEBUG           1
#define EXCEPT_X64_NMI             2
#define EXCEPT_X64_BREAKPOINT      3
#define EXCEPT_X64_OVERFLOW        4
#define EXCEPT_X64_BOUND           5
#define EXCEPT_X64_INVALID_OPCODE  6
#define EXCEPT_X64_DOUBLE_FAULT    8
#define EXCEPT_X64_INVALID_TSS     10
#define EXCEPT_X64_SEG_NOT_PRESENT 11
#define EXCEPT_X64_STACK_FAULT     12
#define EXCEPT_X64_GP_FAULT        13
#define EXCEPT_X64_PAGE_FAULT      14
#define EXCEPT_X64_FP_ERROR        16
#define EXCEPT_X64_ALIGNMENT_CHECK 17
#define EXCEPT_X64_MACHINE_CHECK   18
#define EXCEPT_X64_SIMD            19

///
/// FXSAVE_STATE.
/// FP / MMX / XMM registers (see fxrstor instruction definition).
///
typedef struct {
  UINT16  Fcw;
  UINT16  Fsw;
  UINT16  Ftw;
  UINT16  Opcode;
  UINT64  Rip;
  UINT64  DataOffset;
  UINT8   Reserved1[8];
  UINT8   St0Mm0[10], Reserved2[6];
  UINT8   St1Mm1[10], Reserved3[6];
  UINT8   St2Mm2[10], Reserved4[6];
  UINT8   St3Mm3[10], Reserved5[6];
  UINT8   St4Mm4[10], Reserved6[6];
  UINT8   St5Mm5[10], Reserved7[6];
  UINT8   St6Mm6[10], Reserved8[6];
  UINT8   St7Mm7[10], Reserved9[6];
  UINT8   Xmm0[16];
  UINT8   Xmm1[16];
  UINT8   Xmm2[16];
  UINT8   Xmm3[16];
  UINT8   Xmm4[16];
  UINT8   Xmm5[16];
  UINT8   Xmm6[16];
  UINT8   Xmm7[16];
  //
  // NOTE: UEFI 2.0 spec definition as follows.
  //
  UINT8   Reserved11[14 * 16];
} EFI_FX_SAVE_STATE_X64;

///
///  x64 processor context definition.
///
typedef struct {
  UINT64                ExceptionData;
  EFI_FX_SAVE_STATE_X64 FxSaveState;
  UINT64                Dr0;
  UINT64                Dr1;
  UINT64                Dr2;
  UINT64                Dr3;
  UINT64                Dr6;
  UINT64                Dr7;
  UINT64                Cr0;
  UINT64                Cr1;  /* Reserved */
  UINT64                Cr2;
  UINT64                Cr3;
  UINT64                Cr4;
  UINT64                Cr8;
  UINT64                Rflags;
  UINT64                Ldtr;
  UINT64                Tr;
  UINT64                Gdtr[2];
  UINT64                Idtr[2];
  UINT64                Rip;
  UINT64                Gs;
  UINT64                Fs;
  UINT64                Es;
  UINT64                Ds;
  UINT64                Cs;
  UINT64                Ss;
  UINT64                Rdi;
  UINT64                Rsi;
  UINT64                Rbp;
  UINT64                Rsp;
  UINT64                Rbx;
  UINT64                Rdx;
  UINT64                Rcx;
  UINT64                Rax;
  UINT64                R8;
  UINT64                R9;
  UINT64                R10;
  UINT64                R11;
  UINT64                R12;
  UINT64                R13;
  UINT64                R14;
  UINT64                R15;
} EFI_SYSTEM_CONTEXT_X64;

///
///  Itanium Processor Family Exception types.
///
#define EXCEPT_IPF_VHTP_TRANSLATION       0
#define EXCEPT_IPF_INSTRUCTION_TLB        1
#define EXCEPT_IPF_DATA_TLB               2
#define EXCEPT_IPF_ALT_INSTRUCTION_TLB    3
#define EXCEPT_IPF_ALT_DATA_TLB           4
#define EXCEPT_IPF_DATA_NESTED_TLB        5
#define EXCEPT_IPF_INSTRUCTION_KEY_MISSED 6
#define EXCEPT_IPF_DATA_KEY_MISSED        7
#define EXCEPT_IPF_DIRTY_BIT              8
#define EXCEPT_IPF_INSTRUCTION_ACCESS_BIT 9
#define EXCEPT_IPF_DATA_ACCESS_BIT        10
#define EXCEPT_IPF_BREAKPOINT             11
#define EXCEPT_IPF_EXTERNAL_INTERRUPT     12
//
// 13 - 19 reserved
//
#define EXCEPT_IPF_PAGE_NOT_PRESENT           20
#define EXCEPT_IPF_KEY_PERMISSION             21
#define EXCEPT_IPF_INSTRUCTION_ACCESS_RIGHTS  22
#define EXCEPT_IPF_DATA_ACCESS_RIGHTS         23
#define EXCEPT_IPF_GENERAL_EXCEPTION          24
#define EXCEPT_IPF_DISABLED_FP_REGISTER       25
#define EXCEPT_IPF_NAT_CONSUMPTION            26
#define EXCEPT_IPF_SPECULATION                27
//
// 28 reserved
//
#define EXCEPT_IPF_DEBUG                          29
#define EXCEPT_IPF_UNALIGNED_REFERENCE            30
#define EXCEPT_IPF_UNSUPPORTED_DATA_REFERENCE     31
#define EXCEPT_IPF_FP_FAULT                       32
#define EXCEPT_IPF_FP_TRAP                        33
#define EXCEPT_IPF_LOWER_PRIVILEGE_TRANSFER_TRAP  34
#define EXCEPT_IPF_TAKEN_BRANCH                   35
#define EXCEPT_IPF_SINGLE_STEP                    36
//
// 37 - 44 reserved
//
#define EXCEPT_IPF_IA32_EXCEPTION 45
#define EXCEPT_IPF_IA32_INTERCEPT 46
#define EXCEPT_IPF_IA32_INTERRUPT 47

///
///  IPF processor context definition.
///
typedef struct {
  //
  // The first reserved field is necessary to preserve alignment for the correct
  // bits in UNAT and to insure F2 is 16 byte aligned.
  //
  UINT64  Reserved;
  UINT64  R1;
  UINT64  R2;
  UINT64  R3;
  UINT64  R4;
  UINT64  R5;
  UINT64  R6;
  UINT64  R7;
  UINT64  R8;
  UINT64  R9;
  UINT64  R10;
  UINT64  R11;
  UINT64  R12;
  UINT64  R13;
  UINT64  R14;
  UINT64  R15;
  UINT64  R16;
  UINT64  R17;
  UINT64  R18;
  UINT64  R19;
  UINT64  R20;
  UINT64  R21;
  UINT64  R22;
  UINT64  R23;
  UINT64  R24;
  UINT64  R25;
  UINT64  R26;
  UINT64  R27;
  UINT64  R28;
  UINT64  R29;
  UINT64  R30;
  UINT64  R31;

  UINT64  F2[2];
  UINT64  F3[2];
  UINT64  F4[2];
  UINT64  F5[2];
  UINT64  F6[2];
  UINT64  F7[2];
  UINT64  F8[2];
  UINT64  F9[2];
  UINT64  F10[2];
  UINT64  F11[2];
  UINT64  F12[2];
  UINT64  F13[2];
  UINT64  F14[2];
  UINT64  F15[2];
  UINT64  F16[2];
  UINT64  F17[2];
  UINT64  F18[2];
  UINT64  F19[2];
  UINT64  F20[2];
  UINT64  F21[2];
  UINT64  F22[2];
  UINT64  F23[2];
  UINT64  F24[2];
  UINT64  F25[2];
  UINT64  F26[2];
  UINT64  F27[2];
  UINT64  F28[2];
  UINT64  F29[2];
  UINT64  F30[2];
  UINT64  F31[2];

  UINT64  Pr;

  UINT64  B0;
  UINT64  B1;
  UINT64  B2;
  UINT64  B3;
  UINT64  B4;
  UINT64  B5;
  UINT64  B6;
  UINT64  B7;

  //
  // application registers
  //
  UINT64  ArRsc;
  UINT64  ArBsp;
  UINT64  ArBspstore;
  UINT64  ArRnat;

  UINT64  ArFcr;

  UINT64  ArEflag;
  UINT64  ArCsd;
  UINT64  ArSsd;
  UINT64  ArCflg;
  UINT64  ArFsr;
  UINT64  ArFir;
  UINT64  ArFdr;

  UINT64  ArCcv;

  UINT64  ArUnat;

  UINT64  ArFpsr;

  UINT64  ArPfs;
  UINT64  ArLc;
  UINT64  ArEc;

  //
  // control registers
  //
  UINT64  CrDcr;
  UINT64  CrItm;
  UINT64  CrIva;
  UINT64  CrPta;
  UINT64  CrIpsr;
  UINT64  CrIsr;
  UINT64  CrIip;
  UINT64  CrIfa;
  UINT64  CrItir;
  UINT64  CrIipa;
  UINT64  CrIfs;
  UINT64  CrIim;
  UINT64  CrIha;

  //
  // debug registers
  //
  UINT64  Dbr0;
  UINT64  Dbr1;
  UINT64  Dbr2;
  UINT64  Dbr3;
  UINT64  Dbr4;
  UINT64  Dbr5;
  UINT64  Dbr6;
  UINT64  Dbr7;

  UINT64  Ibr0;
  UINT64  Ibr1;
  UINT64  Ibr2;
  UINT64  Ibr3;
  UINT64  Ibr4;
  UINT64  Ibr5;
  UINT64  Ibr6;
  UINT64  Ibr7;

  //
  // virtual registers - nat bits for R1-R31
  //
  UINT64  IntNat;

} EFI_SYSTEM_CONTEXT_IPF;

///
///  EBC processor exception types.
///
#define EXCEPT_EBC_UNDEFINED            0
#define EXCEPT_EBC_DIVIDE_ERROR         1
#define EXCEPT_EBC_DEBUG                2
#define EXCEPT_EBC_BREAKPOINT           3
#define EXCEPT_EBC_OVERFLOW             4
#define EXCEPT_EBC_INVALID_OPCODE       5   ///< Opcode out of range.
#define EXCEPT_EBC_STACK_FAULT          6
#define EXCEPT_EBC_ALIGNMENT_CHECK      7
#define EXCEPT_EBC_INSTRUCTION_ENCODING 8   ///< Malformed instruction.
#define EXCEPT_EBC_BAD_BREAK            9   ///< BREAK 0 or undefined BREAK.
#define EXCEPT_EBC_STEP                 10  ///< To support debug stepping.
///
/// For coding convenience, define the maximum valid EBC exception.
///
#define MAX_EBC_EXCEPTION EXCEPT_EBC_STEP

///
///  EBC processor context definition.
///
typedef struct {
  UINT64  R0;
  UINT64  R1;
  UINT64  R2;
  UINT64  R3;
  UINT64  R4;
  UINT64  R5;
  UINT64  R6;
  UINT64  R7;
  UINT64  Flags;
  UINT64  ControlFlags;
  UINT64  Ip;
} EFI_SYSTEM_CONTEXT_EBC;



///
///  ARM processor exception types.
///
#define EXCEPT_ARM_RESET                    0
#define EXCEPT_ARM_UNDEFINED_INSTRUCTION    1
#define EXCEPT_ARM_SOFTWARE_INTERRUPT       2
#define EXCEPT_ARM_PREFETCH_ABORT           3
#define EXCEPT_ARM_DATA_ABORT               4
#define EXCEPT_ARM_RESERVED                 5
#define EXCEPT_ARM_IRQ                      6
#define EXCEPT_ARM_FIQ                      7

///
/// For coding convenience, define the maximum valid ARM exception.
///
#define MAX_ARM_EXCEPTION EXCEPT_ARM_FIQ

///
///  ARM processor context definition.
///
typedef struct {
  UINT32  R0;
  UINT32  R1;
  UINT32  R2;
  UINT32  R3;
  UINT32  R4;
  UINT32  R5;
  UINT32  R6;
  UINT32  R7;
  UINT32  R8;
  UINT32  R9;
  UINT32  R10;
  UINT32  R11;
  UINT32  R12;
  UINT32  SP;
  UINT32  LR;
  UINT32  PC;
  UINT32  CPSR;
  UINT32  DFSR;
  UINT32  DFAR;
  UINT32  IFSR;
  UINT32  IFAR;
} EFI_SYSTEM_CONTEXT_ARM;


///
///  AARCH64 processor exception types.
///
#define EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS    0
#define EXCEPT_AARCH64_IRQ                       1
#define EXCEPT_AARCH64_FIQ                       2
#define EXCEPT_AARCH64_SERROR                    3

///
/// For coding convenience, define the maximum valid ARM exception.
///
#define MAX_AARCH64_EXCEPTION EXCEPT_AARCH64_SERROR

typedef struct {
  // General Purpose Registers
  UINT64  X0;
  UINT64  X1;
  UINT64  X2;
  UINT64  X3;
  UINT64  X4;
  UINT64  X5;
  UINT64  X6;
  UINT64  X7;
  UINT64  X8;
  UINT64  X9;
  UINT64  X10;
  UINT64  X11;
  UINT64  X12;
  UINT64  X13;
  UINT64  X14;
  UINT64  X15;
  UINT64  X16;
  UINT64  X17;
  UINT64  X18;
  UINT64  X19;
  UINT64  X20;
  UINT64  X21;
  UINT64  X22;
  UINT64  X23;
  UINT64  X24;
  UINT64  X25;
  UINT64  X26;
  UINT64  X27;
  UINT64  X28;
  UINT64  FP;   // x29 - Frame pointer
  UINT64  LR;   // x30 - Link Register
  UINT64  SP;   // x31 - Stack pointer

  // FP/SIMD Registers
  UINT64  V0[2];
  UINT64  V1[2];
  UINT64  V2[2];
  UINT64  V3[2];
  UINT64  V4[2];
  UINT64  V5[2];
  UINT64  V6[2];
  UINT64  V7[2];
  UINT64  V8[2];
  UINT64  V9[2];
  UINT64  V10[2];
  UINT64  V11[2];
  UINT64  V12[2];
  UINT64  V13[2];
  UINT64  V14[2];
  UINT64  V15[2];
  UINT64  V16[2];
  UINT64  V17[2];
  UINT64  V18[2];
  UINT64  V19[2];
  UINT64  V20[2];
  UINT64  V21[2];
  UINT64  V22[2];
  UINT64  V23[2];
  UINT64  V24[2];
  UINT64  V25[2];
  UINT64  V26[2];
  UINT64  V27[2];
  UINT64  V28[2];
  UINT64  V29[2];
  UINT64  V30[2];
  UINT64  V31[2];

  UINT64  ELR;  // Exception Link Register
  UINT64  SPSR; // Saved Processor Status Register
  UINT64  FPSR; // Floating Point Status Register
  UINT64  ESR;  // Exception syndrome register
  UINT64  FAR;  // Fault Address Register
} EFI_SYSTEM_CONTEXT_AARCH64;

///
/// RISC-V processor exception types.
///
#define EXCEPT_RISCV_INST_MISALIGNED              0
#define EXCEPT_RISCV_INST_ACCESS_FAULT            1
#define EXCEPT_RISCV_ILLEGAL_INST                 2
#define EXCEPT_RISCV_BREAKPOINT                   3
#define EXCEPT_RISCV_LOAD_ADDRESS_MISALIGNED      4
#define EXCEPT_RISCV_LOAD_ACCESS_FAULT            5
#define EXCEPT_RISCV_STORE_AMO_ADDRESS_MISALIGNED 6
#define EXCEPT_RISCV_STORE_AMO_ACCESS_FAULT       7
#define EXCEPT_RISCV_ENV_CALL_FROM_UMODE          8
#define EXCEPT_RISCV_ENV_CALL_FROM_SMODE          9
#define EXCEPT_RISCV_ENV_CALL_FROM_HMODE          10
#define EXCEPT_RISCV_ENV_CALL_FROM_MMODE          11

#define EXCEPT_RISCV_SOFTWARE_INT       0x0
#define EXCEPT_RISCV_TIMER_INT          0x1

typedef struct {
  UINT64  X0;
  UINT64  X1;
  UINT64  X2;
  UINT64  X3;
  UINT64  X4;
  UINT64  X5;
  UINT64  X6;
  UINT64  X7;
  UINT64  X8;
  UINT64  X9;
  UINT64  X10;
  UINT64  X11;
  UINT64  X12;
  UINT64  X13;
  UINT64  X14;
  UINT64  X15;
  UINT64  X16;
  UINT64  X17;
  UINT64  X18;
  UINT64  X19;
  UINT64  X20;
  UINT64  X21;
  UINT64  X22;
  UINT64  X23;
  UINT64  X24;
  UINT64  X25;
  UINT64  X26;
  UINT64  X27;
  UINT64  X28;
  UINT64  X29;
  UINT64  X30;
  UINT64  X31;
} EFI_SYSTEM_CONTEXT_RISCV64;

///
/// Universal EFI_SYSTEM_CONTEXT definition.
///
typedef union {
  EFI_SYSTEM_CONTEXT_EBC  *SystemContextEbc;
  EFI_SYSTEM_CONTEXT_IA32 *SystemContextIa32;
  EFI_SYSTEM_CONTEXT_X64  *SystemContextX64;
  EFI_SYSTEM_CONTEXT_IPF  *SystemContextIpf;
  EFI_SYSTEM_CONTEXT_ARM  *SystemContextArm;
  EFI_SYSTEM_CONTEXT_AARCH64  *SystemContextAArch64;
  EFI_SYSTEM_CONTEXT_RISCV64  *SystemContextRiscV64;
} EFI_SYSTEM_CONTEXT;

//
// DebugSupport callback function prototypes
//

/**
  Registers and enables an exception callback function for the specified exception.

  @param  ExceptionType         Exception types in EBC, IA-32, x64, or IPF.
  @param  SystemContext         Exception content.

**/
typedef
VOID
(EFIAPI *EFI_EXCEPTION_CALLBACK)(
  IN     EFI_EXCEPTION_TYPE               ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT               SystemContext
  );

/**
  Registers and enables the on-target debug agent's periodic entry point.

  @param  SystemContext         Exception content.

**/
typedef
VOID
(EFIAPI *EFI_PERIODIC_CALLBACK)(
  IN OUT EFI_SYSTEM_CONTEXT               SystemContext
  );

///
/// Machine type definition
///
typedef enum {
  IsaIa32 = IMAGE_FILE_MACHINE_I386,           ///< 0x014C
  IsaX64  = IMAGE_FILE_MACHINE_X64,            ///< 0x8664
  IsaIpf  = IMAGE_FILE_MACHINE_IA64,           ///< 0x0200
  IsaEbc  = IMAGE_FILE_MACHINE_EBC,            ///< 0x0EBC
  IsaArm  = IMAGE_FILE_MACHINE_ARMTHUMB_MIXED, ///< 0x01c2
  IsaAArch64  = IMAGE_FILE_MACHINE_ARM64       ///< 0xAA64
} EFI_INSTRUCTION_SET_ARCHITECTURE;


//
// DebugSupport member function definitions
//

/**
  Returns the maximum value that may be used for the ProcessorIndex parameter in
  RegisterPeriodicCallback() and RegisterExceptionCallback().

  @param  This                  A pointer to the EFI_DEBUG_SUPPORT_PROTOCOL instance.
  @param  MaxProcessorIndex     Pointer to a caller-allocated UINTN in which the maximum supported
                                processor index is returned.

  @retval EFI_SUCCESS           The function completed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_MAXIMUM_PROCESSOR_INDEX)(
  IN EFI_DEBUG_SUPPORT_PROTOCOL          *This,
  OUT UINTN                              *MaxProcessorIndex
  );

/**
  Registers a function to be called back periodically in interrupt context.

  @param  This                  A pointer to the EFI_DEBUG_SUPPORT_PROTOCOL instance.
  @param  ProcessorIndex        Specifies which processor the callback function applies to.
  @param  PeriodicCallback      A pointer to a function of type PERIODIC_CALLBACK that is the main
                                periodic entry point of the debug agent.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_ALREADY_STARTED   Non-NULL PeriodicCallback parameter when a callback
                                function was previously registered.
  @retval EFI_OUT_OF_RESOURCES  System has insufficient memory resources to register new callback
                                function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_PERIODIC_CALLBACK)(
  IN EFI_DEBUG_SUPPORT_PROTOCOL          *This,
  IN UINTN                               ProcessorIndex,
  IN EFI_PERIODIC_CALLBACK               PeriodicCallback
  );

/**
  Registers a function to be called when a given processor exception occurs.

  @param  This                  A pointer to the EFI_DEBUG_SUPPORT_PROTOCOL instance.
  @param  ProcessorIndex        Specifies which processor the callback function applies to.
  @param  ExceptionCallback     A pointer to a function of type EXCEPTION_CALLBACK that is called
                                when the processor exception specified by ExceptionType occurs.
  @param  ExceptionType         Specifies which processor exception to hook.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_ALREADY_STARTED   Non-NULL PeriodicCallback parameter when a callback
                                function was previously registered.
  @retval EFI_OUT_OF_RESOURCES  System has insufficient memory resources to register new callback
                                function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_EXCEPTION_CALLBACK)(
  IN EFI_DEBUG_SUPPORT_PROTOCOL          *This,
  IN UINTN                               ProcessorIndex,
  IN EFI_EXCEPTION_CALLBACK              ExceptionCallback,
  IN EFI_EXCEPTION_TYPE                  ExceptionType
  );

/**
  Invalidates processor instruction cache for a memory range. Subsequent execution in this range
  causes a fresh memory fetch to retrieve code to be executed.

  @param  This                  A pointer to the EFI_DEBUG_SUPPORT_PROTOCOL instance.
  @param  ProcessorIndex        Specifies which processor's instruction cache is to be invalidated.
  @param  Start                 Specifies the physical base of the memory range to be invalidated.
  @param  Length                Specifies the minimum number of bytes in the processor's instruction
                                cache to invalidate.

  @retval EFI_SUCCESS           The function completed successfully.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_INVALIDATE_INSTRUCTION_CACHE)(
  IN EFI_DEBUG_SUPPORT_PROTOCOL          *This,
  IN UINTN                               ProcessorIndex,
  IN VOID                                *Start,
  IN UINT64                              Length
  );

///
/// This protocol provides the services to allow the debug agent to register
/// callback functions that are called either periodically or when specific
/// processor exceptions occur.
///
struct _EFI_DEBUG_SUPPORT_PROTOCOL {
  ///
  /// Declares the processor architecture for this instance of the EFI Debug Support protocol.
  ///
  EFI_INSTRUCTION_SET_ARCHITECTURE  Isa;
  EFI_GET_MAXIMUM_PROCESSOR_INDEX   GetMaximumProcessorIndex;
  EFI_REGISTER_PERIODIC_CALLBACK    RegisterPeriodicCallback;
  EFI_REGISTER_EXCEPTION_CALLBACK   RegisterExceptionCallback;
  EFI_INVALIDATE_INSTRUCTION_CACHE  InvalidateInstructionCache;
};

extern EFI_GUID gEfiDebugSupportProtocolGuid;

#endif
