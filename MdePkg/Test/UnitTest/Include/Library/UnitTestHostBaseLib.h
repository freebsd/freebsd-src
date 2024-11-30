/** @file
  Unit Test Host BaseLib hooks.

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __UNIT_TEST_HOST_BASE_LIB_H__
#define __UNIT_TEST_HOST_BASE_LIB_H__

/**
  Prototype of service with no parameters and no return value.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_VOID)(
  VOID
  );

/**
  Prototype of service that reads and returns a BOOLEAN value.

  @return The value read.
**/
typedef
BOOLEAN
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_READ_BOOLEAN)(
  VOID
  );

/**
  Prototype of service that reads and returns a UINT16 value.

  @return The value read.
**/
typedef
UINT16
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_READ_UINT16)(
  VOID
  );

/**
  Prototype of service that reads and returns a UINTN value.

  @return The value read.
**/
typedef
UINTN
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_READ_UINTN)(
  VOID
  );

/**
  Prototype of service that writes and returns a UINT16 value.

  @param[in]  Value  The value to write.

  @return The value written.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_WRITE_UINT16)(
  IN UINT16  Value
  );

/**
  Prototype of service that writes and returns a UINTN value.

  @param[in]  Value  The value to write.

  @return The value written.
**/
typedef
UINTN
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN)(
  IN UINTN  Value
  );

/**
  Prototype of service that reads and returns an IA32_DESCRIPTOR.

  @param[out]  Ia32Descriptor  Pointer to the descriptor read.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_READ_IA32_DESCRIPTOR)(
  OUT IA32_DESCRIPTOR           *Ia32Descriptor
  );

/**
  Prototype of service that writes an IA32_DESCRIPTOR.

  @param[in]  Ia32Descriptor  Pointer to the descriptor to write.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_WRITE_IA32_DESCRIPTOR)(
  IN CONST IA32_DESCRIPTOR     *Ia32Descriptor
  );

/**
  Retrieves CPUID information.

  Executes the CPUID instruction with EAX set to the value specified by Index.
  This function always returns Index.
  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.
  This function is only available on IA-32 and x64.

  @param  Index The 32-bit value to load into EAX prior to invoking the CPUID
                instruction.
  @param  Eax   The pointer to the 32-bit EAX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ebx   The pointer to the 32-bit EBX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Ecx   The pointer to the 32-bit ECX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.
  @param  Edx   The pointer to the 32-bit EDX value returned by the CPUID
                instruction. This is an optional parameter that may be NULL.

  @return Index.

**/
typedef
UINT32
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_CPUID)(
  IN      UINT32                    Index,
  OUT     UINT32                    *Eax   OPTIONAL,
  OUT     UINT32                    *Ebx   OPTIONAL,
  OUT     UINT32                    *Ecx   OPTIONAL,
  OUT     UINT32                    *Edx   OPTIONAL
  );

/**
  Retrieves CPUID information using an extended leaf identifier.

  Executes the CPUID instruction with EAX set to the value specified by Index
  and ECX set to the value specified by SubIndex. This function always returns
  Index. This function is only available on IA-32 and x64.

  If Eax is not NULL, then the value of EAX after CPUID is returned in Eax.
  If Ebx is not NULL, then the value of EBX after CPUID is returned in Ebx.
  If Ecx is not NULL, then the value of ECX after CPUID is returned in Ecx.
  If Edx is not NULL, then the value of EDX after CPUID is returned in Edx.

  @param  Index     The 32-bit value to load into EAX prior to invoking the
                    CPUID instruction.
  @param  SubIndex  The 32-bit value to load into ECX prior to invoking the
                    CPUID instruction.
  @param  Eax       The pointer to the 32-bit EAX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Ebx       The pointer to the 32-bit EBX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Ecx       The pointer to the 32-bit ECX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.
  @param  Edx       The pointer to the 32-bit EDX value returned by the CPUID
                    instruction. This is an optional parameter that may be
                    NULL.

  @return Index.

**/
typedef
UINT32
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_CPUID_EX)(
  IN      UINT32                    Index,
  IN      UINT32                    SubIndex,
  OUT     UINT32                    *Eax   OPTIONAL,
  OUT     UINT32                    *Ebx   OPTIONAL,
  OUT     UINT32                    *Ecx   OPTIONAL,
  OUT     UINT32                    *Edx   OPTIONAL
  );

/**
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and x64.

  @param  Index The 32-bit MSR index to read.

  @return The value of the MSR identified by Index.

**/
typedef
UINT64
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_READ_MSR_64)(
  IN      UINT32                    Index
  );

/**
  Writes a 64-bit value to a Machine Specific Register(MSR), and returns the
  value.

  Writes the 64-bit value specified by Value to the MSR specified by Index. The
  64-bit value written to the MSR is returned. No parameter checking is
  performed on Index or Value, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and Value are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and x64.

  @param  Index The 32-bit MSR index to write.
  @param  Value The 64-bit value to write to the MSR.

  @return Value

**/
typedef
UINT64
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_WRITE_MSR_64)(
  IN      UINT32                    Index,
  IN      UINT64                    Value
  );

/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and x64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
typedef
UINT64
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_READ_PMC)(
  IN      UINT32                    Index
  );

/**
  Sets up a monitor buffer that is used by AsmMwait().

  Executes a MONITOR instruction with the register state specified by Eax, Ecx
  and Edx. Returns Eax. This function is only available on IA-32 and x64.

  @param  Eax The value to load into EAX or RAX before executing the MONITOR
              instruction.
  @param  Ecx The value to load into ECX or RCX before executing the MONITOR
              instruction.
  @param  Edx The value to load into EDX or RDX before executing the MONITOR
              instruction.

  @return Eax

**/
typedef
UINTN
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_MONITOR)(
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx,
  IN      UINTN                     Edx
  );

/**
  Executes an MWAIT instruction.

  Executes an MWAIT instruction with the register state specified by Eax and
  Ecx. Returns Eax. This function is only available on IA-32 and x64.

  @param  Eax The value to load into EAX or RAX before executing the MONITOR
              instruction.
  @param  Ecx The value to load into ECX or RCX before executing the MONITOR
              instruction.

  @return Eax

**/
typedef
UINTN
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_MWAIT)(
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx
  );

/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
  This function is only available on IA-32 and x64.

  @param  LinearAddress The address of the cache line to flush. If the CPU is
                        in a physical addressing mode, then LinearAddress is a
                        physical address. If the CPU is in a virtual
                        addressing mode, then LinearAddress is a virtual
                        address.

  @return LinearAddress.
**/
typedef
VOID *
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_FLUSH_CACHE_LINE)(
  IN      VOID                      *LinearAddress
  );

/**
  Prototype of service that enables ot disables 32-bit paging modes.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is enabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is enabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is enabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is enabled.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_PAGING_32)(
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack
  );

/**
  Enables the 64-bit paging mode on the CPU.

  Enables the 64-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode with flat
  descriptors. This function is only available on IA-32. After the 64-bit
  paging mode is enabled, control is transferred to the function specified by
  EntryPoint using the new stack specified by NewStack and passing in the
  parameters specified by Context1 and Context2. Context1 and Context2 are
  optional and may be 0. The function EntryPoint must never return.

  If the current execution mode is not 32-bit protected mode with flat
  descriptors, then ASSERT().
  If EntryPoint is 0, then ASSERT().
  If NewStack is 0, then ASSERT().

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for long mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is enabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is enabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is enabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is enabled.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_ENABLE_PAGING_64)(
  IN      UINT16                    Cs,
  IN      UINT64                    EntryPoint,
  IN      UINT64                    Context1   OPTIONAL,
  IN      UINT64                    Context2   OPTIONAL,
  IN      UINT64                    NewStack
  );

/**
  Disables the 64-bit paging mode on the CPU.

  Disables the 64-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 64-paging mode.
  This function is only available on x64. After the 64-bit paging mode is
  disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be 0. The function EntryPoint must never return.

  If the current execution mode is not 64-bit paged mode, then ASSERT().
  If EntryPoint is 0, then ASSERT().
  If NewStack is 0, then ASSERT().

  @param  Cs          The 16-bit selector to load in the CS before EntryPoint
                      is called. The descriptor in the GDT that this selector
                      references must be setup for 32-bit protected mode.
  @param  EntryPoint  The 64-bit virtual address of the function to call with
                      the new stack after paging is disabled.
  @param  Context1    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the first parameter after
                      paging is disabled.
  @param  Context2    The 64-bit virtual address of the context to pass into
                      the EntryPoint function as the second parameter after
                      paging is disabled.
  @param  NewStack    The 64-bit virtual address of the new stack to use for
                      the EntryPoint function after paging is disabled.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_DISABLE_PAGING_64)(
  IN      UINT16                    Cs,
  IN      UINT32                    EntryPoint,
  IN      UINT32                    Context1   OPTIONAL,
  IN      UINT32                    Context2   OPTIONAL,
  IN      UINT32                    NewStack
  );

/**
  Retrieves the properties for 16-bit thunk functions.

  Computes the size of the buffer and stack below 1MB required to use the
  AsmPrepareThunk16(), AsmThunk16() and AsmPrepareAndThunk16() functions. This
  buffer size is returned in RealModeBufferSize, and the stack size is returned
  in ExtraStackSize. If parameters are passed to the 16-bit real mode code,
  then the actual minimum stack size is ExtraStackSize plus the maximum number
  of bytes that need to be passed to the 16-bit real mode code.

  If RealModeBufferSize is NULL, then ASSERT().
  If ExtraStackSize is NULL, then ASSERT().

  @param  RealModeBufferSize  A pointer to the size of the buffer below 1MB
                              required to use the 16-bit thunk functions.
  @param  ExtraStackSize      A pointer to the extra size of stack below 1MB
                              that the 16-bit thunk functions require for
                              temporary storage in the transition to and from
                              16-bit real mode.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_GET_THUNK_16_PROPERTIES)(
  OUT     UINT32                    *RealModeBufferSize,
  OUT     UINT32                    *ExtraStackSize
  );

/**
  Prototype of services that operates on a THUNK_CONTEXT structure.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_THUNK_16)(
  IN OUT  THUNK_CONTEXT             *ThunkContext
  );

/**
  Patch the immediate operand of an IA32 or X64 instruction such that the byte,
  word, dword or qword operand is encoded at the end of the instruction's
  binary representation.

  This function should be used to update object code that was compiled with
  NASM from assembly source code. Example:

  NASM source code:

        mov     eax, strict dword 0 ; the imm32 zero operand will be patched
    ASM_PFX(gPatchCr3):
        mov     cr3, eax

  C source code:

    X86_ASSEMBLY_PATCH_LABEL gPatchCr3;
    PatchInstructionX86 (gPatchCr3, AsmReadCr3 (), 4);

  @param[out] InstructionEnd  Pointer right past the instruction to patch. The
                              immediate operand to patch is expected to
                              comprise the trailing bytes of the instruction.
                              If InstructionEnd is closer to address 0 than
                              ValueSize permits, then ASSERT().

  @param[in] PatchValue       The constant to write to the immediate operand.
                              The caller is responsible for ensuring that
                              PatchValue can be represented in the byte, word,
                              dword or qword operand (as indicated through
                              ValueSize); otherwise ASSERT().

  @param[in] ValueSize        The size of the operand in bytes; must be 1, 2,
                              4, or 8. ASSERT() otherwise.
**/
typedef
VOID
(EFIAPI *UNIT_TEST_HOST_BASE_LIB_ASM_PATCH_INSTRUCTION_X86)(
  OUT X86_ASSEMBLY_PATCH_LABEL *InstructionEnd,
  IN  UINT64                   PatchValue,
  IN  UINTN                    ValueSize
  );

///
/// Common services
///
typedef struct {
  UNIT_TEST_HOST_BASE_LIB_VOID            EnableInterrupts;
  UNIT_TEST_HOST_BASE_LIB_VOID            DisableInterrupts;
  UNIT_TEST_HOST_BASE_LIB_VOID            EnableDisableInterrupts;
  UNIT_TEST_HOST_BASE_LIB_READ_BOOLEAN    GetInterruptState;
} UNIT_TEST_HOST_BASE_LIB_COMMON;

///
/// IA32/X64 services
///
typedef struct {
  UNIT_TEST_HOST_BASE_LIB_ASM_CPUID                      AsmCpuid;
  UNIT_TEST_HOST_BASE_LIB_ASM_CPUID_EX                   AsmCpuidEx;
  UNIT_TEST_HOST_BASE_LIB_VOID                           AsmDisableCache;
  UNIT_TEST_HOST_BASE_LIB_VOID                           AsmEnableCache;
  UNIT_TEST_HOST_BASE_LIB_ASM_READ_MSR_64                AsmReadMsr64;
  UNIT_TEST_HOST_BASE_LIB_ASM_WRITE_MSR_64               AsmWriteMsr64;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadCr0;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadCr2;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadCr3;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadCr4;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteCr0;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteCr2;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteCr3;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteCr4;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr0;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr1;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr2;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr3;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr4;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr5;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr6;
  UNIT_TEST_HOST_BASE_LIB_READ_UINTN                     AsmReadDr7;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr0;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr1;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr2;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr3;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr4;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr5;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr6;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINTN                    AsmWriteDr7;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadCs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadDs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadEs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadFs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadGs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadSs;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadTr;
  UNIT_TEST_HOST_BASE_LIB_ASM_READ_IA32_DESCRIPTOR       AsmReadGdtr;
  UNIT_TEST_HOST_BASE_LIB_ASM_WRITE_IA32_DESCRIPTOR      AsmWriteGdtr;
  UNIT_TEST_HOST_BASE_LIB_ASM_READ_IA32_DESCRIPTOR       AsmReadIdtr;
  UNIT_TEST_HOST_BASE_LIB_ASM_WRITE_IA32_DESCRIPTOR      AsmWriteIdtr;
  UNIT_TEST_HOST_BASE_LIB_READ_UINT16                    AsmReadLdtr;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINT16                   AsmWriteLdtr;
  UNIT_TEST_HOST_BASE_LIB_ASM_READ_PMC                   AsmReadPmc;
  UNIT_TEST_HOST_BASE_LIB_ASM_MONITOR                    AsmMonitor;
  UNIT_TEST_HOST_BASE_LIB_ASM_MWAIT                      AsmMwait;
  UNIT_TEST_HOST_BASE_LIB_VOID                           AsmWbinvd;
  UNIT_TEST_HOST_BASE_LIB_VOID                           AsmInvd;
  UNIT_TEST_HOST_BASE_LIB_ASM_FLUSH_CACHE_LINE           AsmFlushCacheLine;
  UNIT_TEST_HOST_BASE_LIB_ASM_PAGING_32                  AsmEnablePaging32;
  UNIT_TEST_HOST_BASE_LIB_ASM_PAGING_32                  AsmDisablePaging32;
  UNIT_TEST_HOST_BASE_LIB_ASM_ENABLE_PAGING_64           AsmEnablePaging64;
  UNIT_TEST_HOST_BASE_LIB_ASM_DISABLE_PAGING_64          AsmDisablePaging64;
  UNIT_TEST_HOST_BASE_LIB_ASM_GET_THUNK_16_PROPERTIES    AsmGetThunk16Properties;
  UNIT_TEST_HOST_BASE_LIB_ASM_THUNK_16                   AsmPrepareThunk16;
  UNIT_TEST_HOST_BASE_LIB_ASM_THUNK_16                   AsmThunk16;
  UNIT_TEST_HOST_BASE_LIB_ASM_THUNK_16                   AsmPrepareAndThunk16;
  UNIT_TEST_HOST_BASE_LIB_WRITE_UINT16                   AsmWriteTr;
  UNIT_TEST_HOST_BASE_LIB_VOID                           AsmLfence;
  UNIT_TEST_HOST_BASE_LIB_ASM_PATCH_INSTRUCTION_X86      PatchInstructionX86;
} UNIT_TEST_HOST_BASE_LIB_X86;

///
/// Data structure that contains pointers structures of common services and CPU
/// architctuire specific services.  Support for additional CPU architectures
/// can be added to the end of this structure.
///
typedef struct {
  UNIT_TEST_HOST_BASE_LIB_COMMON    *Common;
  UNIT_TEST_HOST_BASE_LIB_X86       *X86;
} UNIT_TEST_HOST_BASE_LIB;

extern UNIT_TEST_HOST_BASE_LIB  gUnitTestHostBaseLib;

#endif
