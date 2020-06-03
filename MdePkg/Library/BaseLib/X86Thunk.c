/** @file
  Real Mode Thunk Functions for IA32 and x64.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "BaseLibInternals.h"

extern CONST UINT8                  m16Start;
extern CONST UINT16                 m16Size;
extern CONST UINT16                 mThunk16Attr;
extern CONST UINT16                 m16Gdt;
extern CONST UINT16                 m16GdtrBase;
extern CONST UINT16                 mTransition;

/**
  Invokes 16-bit code in big real mode and returns the updated register set.

  This function transfers control to the 16-bit code specified by CS:EIP using
  the stack specified by SS:ESP in RegisterSet. The updated registers are saved
  on the real mode stack and the starting address of the save area is returned.

  @param  RegisterSet Values of registers before invocation of 16-bit code.
  @param  Transition  The pointer to the transition code under 1MB.

  @return The pointer to a IA32_REGISTER_SET structure containing the updated
          register values.

**/
IA32_REGISTER_SET *
EFIAPI
InternalAsmThunk16 (
  IN      IA32_REGISTER_SET         *RegisterSet,
  IN OUT  VOID                      *Transition
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
VOID
EFIAPI
AsmGetThunk16Properties (
  OUT     UINT32                    *RealModeBufferSize,
  OUT     UINT32                    *ExtraStackSize
  )
{
  ASSERT (RealModeBufferSize != NULL);
  ASSERT (ExtraStackSize != NULL);

  *RealModeBufferSize = m16Size;

  //
  // Extra 4 bytes for return address, and another 4 bytes for mode transition
  //
  *ExtraStackSize = sizeof (IA32_DWORD_REGS) + 8;
}

/**
  Prepares all structures a code required to use AsmThunk16().

  Prepares all structures and code required to use AsmThunk16().

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer is mapped 1:1.

  If ThunkContext is NULL, then ASSERT().

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmPrepareThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  )
{
  IA32_SEGMENT_DESCRIPTOR           *RealModeGdt;

  ASSERT (ThunkContext != NULL);
  ASSERT ((UINTN)ThunkContext->RealModeBuffer < 0x100000);
  ASSERT (ThunkContext->RealModeBufferSize >= m16Size);
  ASSERT ((UINTN)ThunkContext->RealModeBuffer + m16Size <= 0x100000);

  CopyMem (ThunkContext->RealModeBuffer, &m16Start, m16Size);

  //
  // Point RealModeGdt to the GDT to be used in transition
  //
  // RealModeGdt[0]: Reserved as NULL descriptor
  // RealModeGdt[1]: Code Segment
  // RealModeGdt[2]: Data Segment
  // RealModeGdt[3]: Call Gate
  //
  RealModeGdt = (IA32_SEGMENT_DESCRIPTOR*)(
                  (UINTN)ThunkContext->RealModeBuffer + m16Gdt);

  //
  // Update Code & Data Segment Descriptor
  //
  RealModeGdt[1].Bits.BaseLow =
    (UINT32)(UINTN)ThunkContext->RealModeBuffer & ~0xf;
  RealModeGdt[1].Bits.BaseMid =
    (UINT32)(UINTN)ThunkContext->RealModeBuffer >> 16;

  //
  // Update transition code entry point offset
  //
  *(UINT32*)((UINTN)ThunkContext->RealModeBuffer + mTransition) +=
    (UINT32)(UINTN)ThunkContext->RealModeBuffer & 0xf;

  //
  // Update Segment Limits for both Code and Data Segment Descriptors
  //
  if ((ThunkContext->ThunkAttributes & THUNK_ATTRIBUTE_BIG_REAL_MODE) == 0) {
    //
    // Set segment limits to 64KB
    //
    RealModeGdt[1].Bits.LimitHigh = 0;
    RealModeGdt[1].Bits.G = 0;
    RealModeGdt[2].Bits.LimitHigh = 0;
    RealModeGdt[2].Bits.G = 0;
  }

  //
  // Update GDTBASE for this thunk context
  //
  *(VOID**)((UINTN)ThunkContext->RealModeBuffer + m16GdtrBase) = RealModeGdt;

  //
  // Update Thunk Attributes
  //
  *(UINT32*)((UINTN)ThunkContext->RealModeBuffer + mThunk16Attr) =
    ThunkContext->ThunkAttributes;
}

/**
  Transfers control to a 16-bit real mode entry point and returns the results.

  Transfers control to a 16-bit real mode entry point and returns the results.
  AsmPrepareThunk16() must be called with ThunkContext before this function is used.
  This function must be called with interrupts disabled.

  The register state from the RealModeState field of ThunkContext is restored just prior
  to calling the 16-bit real mode entry point.  This includes the EFLAGS field of RealModeState,
  which is used to set the interrupt state when a 16-bit real mode entry point is called.
  Control is transferred to the 16-bit real mode entry point specified by the CS and Eip fields of RealModeState.
  The stack is initialized to the SS and ESP fields of RealModeState.  Any parameters passed to
  the 16-bit real mode code must be populated by the caller at SS:ESP prior to calling this function.
  The 16-bit real mode entry point is invoked with a 16-bit CALL FAR instruction,
  so when accessing stack contents, the 16-bit real mode code must account for the 16-bit segment
  and 16-bit offset of the return address that were pushed onto the stack. The 16-bit real mode entry
  point must exit with a RETF instruction. The register state is captured into RealModeState immediately
  after the RETF instruction is executed.

  If EFLAGS specifies interrupts enabled, or any of the 16-bit real mode code enables interrupts,
  or any of the 16-bit real mode code makes a SW interrupt, then the caller is responsible for making sure
  the IDT at address 0 is initialized to handle any HW or SW interrupts that may occur while in 16-bit real mode.

  If EFLAGS specifies interrupts enabled, or any of the 16-bit real mode code enables interrupts,
  then the caller is responsible for making sure the 8259 PIC is in a state compatible with 16-bit real mode.
  This includes the base vectors, the interrupt masks, and the edge/level trigger mode.

  If THUNK_ATTRIBUTE_BIG_REAL_MODE is set in the ThunkAttributes field of ThunkContext, then the user code
  is invoked in big real mode.  Otherwise, the user code is invoked in 16-bit real mode with 64KB segment limits.

  If neither THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 nor THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL are set in
  ThunkAttributes, then it is assumed that the user code did not enable the A20 mask, and no attempt is made to
  disable the A20 mask.

  If THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 is set and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL is clear in
  ThunkAttributes, then attempt to use the INT 15 service to disable the A20 mask.  If this INT 15 call fails,
  then attempt to disable the A20 mask by directly accessing the 8042 keyboard controller I/O ports.

  If THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 is clear and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL is set in
  ThunkAttributes, then attempt to disable the A20 mask by directly accessing the 8042 keyboard controller I/O ports.

  If ThunkContext is NULL, then ASSERT().
  If AsmPrepareThunk16() was not previously called with ThunkContext, then ASSERT().
  If both THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 and THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL are set in
  ThunkAttributes, then ASSERT().

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer is mapped 1:1.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  )
{
  IA32_REGISTER_SET                 *UpdatedRegs;

  ASSERT (ThunkContext != NULL);
  ASSERT ((UINTN)ThunkContext->RealModeBuffer < 0x100000);
  ASSERT (ThunkContext->RealModeBufferSize >= m16Size);
  ASSERT ((UINTN)ThunkContext->RealModeBuffer + m16Size <= 0x100000);
  ASSERT (((ThunkContext->ThunkAttributes & (THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 | THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL)) != \
           (THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15 | THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL)));

  UpdatedRegs = InternalAsmThunk16 (
                  ThunkContext->RealModeState,
                  ThunkContext->RealModeBuffer
                  );

  CopyMem (ThunkContext->RealModeState, UpdatedRegs, sizeof (*UpdatedRegs));
}

/**
  Prepares all structures and code for a 16-bit real mode thunk, transfers
  control to a 16-bit real mode entry point, and returns the results.

  Prepares all structures and code for a 16-bit real mode thunk, transfers
  control to a 16-bit real mode entry point, and returns the results. If the
  caller only need to perform a single 16-bit real mode thunk, then this
  service should be used. If the caller intends to make more than one 16-bit
  real mode thunk, then it is more efficient if AsmPrepareThunk16() is called
  once and AsmThunk16() can be called for each 16-bit real mode thunk.

  This interface is limited to be used in either physical mode or virtual modes with paging enabled where the
  virtual to physical mappings for ThunkContext.RealModeBuffer is mapped 1:1.

  See AsmPrepareThunk16() and AsmThunk16() for the detailed description and ASSERT() conditions.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmPrepareAndThunk16 (
  IN OUT  THUNK_CONTEXT             *ThunkContext
  )
{
  AsmPrepareThunk16 (ThunkContext);
  AsmThunk16 (ThunkContext);
}
