/** @file
  IA32/X64 specific Unit Test Host functions.

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "UnitTestHost.h"

///
/// Defines for mUnitTestHostBaseLibSegment indexes
///
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_CS    0
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_DS    1
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_ES    2
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_FS    3
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_GS    4
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_SS    5
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_TR    6
#define UNIT_TEST_HOST_BASE_LIB_SEGMENT_LDTR  7

///
/// Module global variables for simple system emulation of MSRs, CRx, DRx,
/// GDTR, IDTR, and Segment Selectors.
///
STATIC UINT64           mUnitTestHostBaseLibMsr[2][0x1000];
STATIC UINTN            mUnitTestHostBaseLibCr[5];
STATIC UINTN            mUnitTestHostBaseLibDr[8];
STATIC UINT16           mUnitTestHostBaseLibSegment[8];
STATIC IA32_DESCRIPTOR  mUnitTestHostBaseLibGdtr;
STATIC IA32_DESCRIPTOR  mUnitTestHostBaseLibIdtr;

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
UINT32
EFIAPI
UnitTestHostBaseLibAsmCpuid (
  IN      UINT32  Index,
  OUT     UINT32  *Eax   OPTIONAL,
  OUT     UINT32  *Ebx   OPTIONAL,
  OUT     UINT32  *Ecx   OPTIONAL,
  OUT     UINT32  *Edx   OPTIONAL
  )
{
  UINT32  RetEcx;

  RetEcx = 0;
  switch (Index) {
    case 1:
      RetEcx |= BIT30; /* RdRand */
      break;
  }

  if (Eax != NULL) {
    *Eax = 0;
  }

  if (Ebx != NULL) {
    *Ebx = 0;
  }

  if (Ecx != NULL) {
    *Ecx = RetEcx;
  }

  if (Edx != NULL) {
    *Edx = 0;
  }

  return Index;
}

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
UINT32
EFIAPI
UnitTestHostBaseLibAsmCpuidEx (
  IN      UINT32  Index,
  IN      UINT32  SubIndex,
  OUT     UINT32  *Eax   OPTIONAL,
  OUT     UINT32  *Ebx   OPTIONAL,
  OUT     UINT32  *Ecx   OPTIONAL,
  OUT     UINT32  *Edx   OPTIONAL
  )
{
  if (Eax != NULL) {
    *Eax = 0;
  }

  if (Ebx != NULL) {
    *Ebx = 0;
  }

  if (Ecx != NULL) {
    *Ecx = 0;
  }

  if (Edx != NULL) {
    *Edx = 0;
  }

  return Index;
}

/**
  Set CD bit and clear NW bit of CR0 followed by a WBINVD.

  Disables the caches by setting the CD bit of CR0 to 1, clearing the NW bit of CR0 to 0,
  and executing a WBINVD instruction.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmDisableCache (
  VOID
  )
{
}

/**
  Perform a WBINVD and clear both the CD and NW bits of CR0.

  Enables the caches by executing a WBINVD instruction and then clear both the CD and NW
  bits of CR0 to 0.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmEnableCache (
  VOID
  )
{
}

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
UINT64
EFIAPI
UnitTestHostBaseLibAsmReadMsr64 (
  IN      UINT32  Index
  )
{
  if (Index < 0x1000) {
    return mUnitTestHostBaseLibMsr[0][Index];
  }

  if ((Index >= 0xC0000000) && (Index < 0xC0001000)) {
    return mUnitTestHostBaseLibMsr[1][Index];
  }

  return 0;
}

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
UINT64
EFIAPI
UnitTestHostBaseLibAsmWriteMsr64 (
  IN      UINT32  Index,
  IN      UINT64  Value
  )
{
  if (Index < 0x1000) {
    mUnitTestHostBaseLibMsr[0][Index] = Value;
  }

  if ((Index >= 0xC0000000) && (Index < 0xC0001000)) {
    mUnitTestHostBaseLibMsr[1][Index - 0xC00000000] = Value;
  }

  return Value;
}

/**
  Reads the current value of the Control Register 0 (CR0).

  Reads and returns the current value of CR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 0 (CR0).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadCr0 (
  VOID
  )
{
  return mUnitTestHostBaseLibCr[0];
}

/**
  Reads the current value of the Control Register 2 (CR2).

  Reads and returns the current value of CR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 2 (CR2).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadCr2 (
  VOID
  )
{
  return mUnitTestHostBaseLibCr[2];
}

/**
  Reads the current value of the Control Register 3 (CR3).

  Reads and returns the current value of CR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 3 (CR3).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadCr3 (
  VOID
  )
{
  return mUnitTestHostBaseLibCr[3];
}

/**
  Reads the current value of the Control Register 4 (CR4).

  Reads and returns the current value of CR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 4 (CR4).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadCr4 (
  VOID
  )
{
  return mUnitTestHostBaseLibCr[4];
}

/**
  Writes a value to Control Register 0 (CR0).

  Writes and returns a new value to CR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr0 The value to write to CR0.

  @return The value written to CR0.

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteCr0 (
  UINTN  Cr0
  )
{
  mUnitTestHostBaseLibCr[0] = Cr0;
  return Cr0;
}

/**
  Writes a value to Control Register 2 (CR2).

  Writes and returns a new value to CR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr2 The value to write to CR2.

  @return The value written to CR2.

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteCr2 (
  UINTN  Cr2
  )
{
  mUnitTestHostBaseLibCr[2] = Cr2;
  return Cr2;
}

/**
  Writes a value to Control Register 3 (CR3).

  Writes and returns a new value to CR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr3 The value to write to CR3.

  @return The value written to CR3.

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteCr3 (
  UINTN  Cr3
  )
{
  mUnitTestHostBaseLibCr[3] = Cr3;
  return Cr3;
}

/**
  Writes a value to Control Register 4 (CR4).

  Writes and returns a new value to CR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr4 The value to write to CR4.

  @return The value written to CR4.

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteCr4 (
  UINTN  Cr4
  )
{
  mUnitTestHostBaseLibCr[4] = Cr4;
  return Cr4;
}

/**
  Reads the current value of Debug Register 0 (DR0).

  Reads and returns the current value of DR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 0 (DR0).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr0 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[0];
}

/**
  Reads the current value of Debug Register 1 (DR1).

  Reads and returns the current value of DR1. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 1 (DR1).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr1 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[1];
}

/**
  Reads the current value of Debug Register 2 (DR2).

  Reads and returns the current value of DR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 2 (DR2).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr2 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[2];
}

/**
  Reads the current value of Debug Register 3 (DR3).

  Reads and returns the current value of DR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 3 (DR3).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr3 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[3];
}

/**
  Reads the current value of Debug Register 4 (DR4).

  Reads and returns the current value of DR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 4 (DR4).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr4 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[4];
}

/**
  Reads the current value of Debug Register 5 (DR5).

  Reads and returns the current value of DR5. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 5 (DR5).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr5 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[5];
}

/**
  Reads the current value of Debug Register 6 (DR6).

  Reads and returns the current value of DR6. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 6 (DR6).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr6 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[6];
}

/**
  Reads the current value of Debug Register 7 (DR7).

  Reads and returns the current value of DR7. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 7 (DR7).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmReadDr7 (
  VOID
  )
{
  return mUnitTestHostBaseLibDr[7];
}

/**
  Writes a value to Debug Register 0 (DR0).

  Writes and returns a new value to DR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr0 The value to write to Dr0.

  @return The value written to Debug Register 0 (DR0).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr0 (
  UINTN  Dr0
  )
{
  mUnitTestHostBaseLibDr[0] = Dr0;
  return Dr0;
}

/**
  Writes a value to Debug Register 1 (DR1).

  Writes and returns a new value to DR1. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr1 The value to write to Dr1.

  @return The value written to Debug Register 1 (DR1).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr1 (
  UINTN  Dr1
  )
{
  mUnitTestHostBaseLibDr[1] = Dr1;
  return Dr1;
}

/**
  Writes a value to Debug Register 2 (DR2).

  Writes and returns a new value to DR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr2 The value to write to Dr2.

  @return The value written to Debug Register 2 (DR2).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr2 (
  UINTN  Dr2
  )
{
  mUnitTestHostBaseLibDr[2] = Dr2;
  return Dr2;
}

/**
  Writes a value to Debug Register 3 (DR3).

  Writes and returns a new value to DR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr3 The value to write to Dr3.

  @return The value written to Debug Register 3 (DR3).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr3 (
  UINTN  Dr3
  )
{
  mUnitTestHostBaseLibDr[3] = Dr3;
  return Dr3;
}

/**
  Writes a value to Debug Register 4 (DR4).

  Writes and returns a new value to DR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr4 The value to write to Dr4.

  @return The value written to Debug Register 4 (DR4).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr4 (
  UINTN  Dr4
  )
{
  mUnitTestHostBaseLibDr[4] = Dr4;
  return Dr4;
}

/**
  Writes a value to Debug Register 5 (DR5).

  Writes and returns a new value to DR5. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr5 The value to write to Dr5.

  @return The value written to Debug Register 5 (DR5).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr5 (
  UINTN  Dr5
  )
{
  mUnitTestHostBaseLibDr[5] = Dr5;
  return Dr5;
}

/**
  Writes a value to Debug Register 6 (DR6).

  Writes and returns a new value to DR6. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr6 The value to write to Dr6.

  @return The value written to Debug Register 6 (DR6).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr6 (
  UINTN  Dr6
  )
{
  mUnitTestHostBaseLibDr[6] = Dr6;
  return Dr6;
}

/**
  Writes a value to Debug Register 7 (DR7).

  Writes and returns a new value to DR7. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr7 The value to write to Dr7.

  @return The value written to Debug Register 7 (DR7).

**/
UINTN
EFIAPI
UnitTestHostBaseLibAsmWriteDr7 (
  UINTN  Dr7
  )
{
  mUnitTestHostBaseLibDr[7] = Dr7;
  return Dr7;
}

/**
  Reads the current value of Code Segment Register (CS).

  Reads and returns the current value of CS. This function is only available on
  IA-32 and x64.

  @return The current value of CS.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadCs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_CS];
}

/**
  Reads the current value of Data Segment Register (DS).

  Reads and returns the current value of DS. This function is only available on
  IA-32 and x64.

  @return The current value of DS.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadDs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_DS];
}

/**
  Reads the current value of Extra Segment Register (ES).

  Reads and returns the current value of ES. This function is only available on
  IA-32 and x64.

  @return The current value of ES.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadEs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_ES];
}

/**
  Reads the current value of FS Data Segment Register (FS).

  Reads and returns the current value of FS. This function is only available on
  IA-32 and x64.

  @return The current value of FS.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadFs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_FS];
}

/**
  Reads the current value of GS Data Segment Register (GS).

  Reads and returns the current value of GS. This function is only available on
  IA-32 and x64.

  @return The current value of GS.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadGs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_GS];
}

/**
  Reads the current value of Stack Segment Register (SS).

  Reads and returns the current value of SS. This function is only available on
  IA-32 and x64.

  @return The current value of SS.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadSs (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_SS];
}

/**
  Reads the current value of Task Register (TR).

  Reads and returns the current value of TR. This function is only available on
  IA-32 and x64.

  @return The current value of TR.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadTr (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_TR];
}

/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmReadGdtr (
  OUT     IA32_DESCRIPTOR  *Gdtr
  )
{
  Gdtr = &mUnitTestHostBaseLibGdtr;
}

/**
  Writes the current Global Descriptor Table Register (GDTR) descriptor.

  Writes and the current GDTR descriptor specified by Gdtr. This function is
  only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmWriteGdtr (
  IN      CONST IA32_DESCRIPTOR  *Gdtr
  )
{
  CopyMem (&mUnitTestHostBaseLibGdtr, Gdtr, sizeof (IA32_DESCRIPTOR));
}

/**
  Reads the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmReadIdtr (
  OUT     IA32_DESCRIPTOR  *Idtr
  )
{
  Idtr = &mUnitTestHostBaseLibIdtr;
}

/**
  Writes the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmWriteIdtr (
  IN      CONST IA32_DESCRIPTOR  *Idtr
  )
{
  CopyMem (&mUnitTestHostBaseLibIdtr, Idtr, sizeof (IA32_DESCRIPTOR));
}

/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and x64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
UnitTestHostBaseLibAsmReadLdtr (
  VOID
  )
{
  return mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_LDTR];
}

/**
  Writes the current Local Descriptor Table Register (LDTR) selector.

  Writes and the current LDTR descriptor specified by Ldtr. This function is
  only available on IA-32 and x64.

  @param  Ldtr  16-bit LDTR selector value.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmWriteLdtr (
  IN      UINT16  Ldtr
  )
{
  mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_LDTR] = Ldtr;
}

/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and x64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
UINT64
EFIAPI
UnitTestHostBaseLibAsmReadPmc (
  IN      UINT32  Index
  )
{
  return 0;
}

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
UINTN
EFIAPI
UnitTestHostBaseLibAsmMonitor (
  IN      UINTN  Eax,
  IN      UINTN  Ecx,
  IN      UINTN  Edx
  )
{
  return Eax;
}

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
UINTN
EFIAPI
UnitTestHostBaseLibAsmMwait (
  IN      UINTN  Eax,
  IN      UINTN  Ecx
  )
{
  return Eax;
}

/**
  Executes a WBINVD instruction.

  Executes a WBINVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmWbinvd (
  VOID
  )
{
}

/**
  Executes a INVD instruction.

  Executes a INVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmInvd (
  VOID
  )
{
}

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
VOID *
EFIAPI
UnitTestHostBaseLibAsmFlushCacheLine (
  IN      VOID  *LinearAddress
  )
{
  return LinearAddress;
}

/**
  Enables the 32-bit paging mode on the CPU.

  Enables the 32-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode. This function is
  only available on IA-32. After the 32-bit paging mode is enabled, control is
  transferred to the function specified by EntryPoint using the new stack
  specified by NewStack and passing in the parameters specified by Context1 and
  Context2. Context1 and Context2 are optional and may be NULL. The function
  EntryPoint must never return.

  If the current execution mode is not 32-bit protected mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit protected mode with flat descriptors. This
      means all descriptors must have a base of 0 and a limit of 4GB.
  3)  CR0 and CR4 must be compatible with 32-bit protected mode with flat
      descriptors.
  4)  CR3 must point to valid page tables that will be used once the transition
      is complete, and those page tables must guarantee that the pages for this
      function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is enabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is enabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is enabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is enabled.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmEnablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack
  )
{
  EntryPoint (Context1, Context2);
}

/**
  Disables the 32-bit paging mode on the CPU.

  Disables the 32-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 32-paged protected
  mode. This function is only available on IA-32. After the 32-bit paging mode
  is disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be NULL. The function EntryPoint must never return.

  If the current execution mode is not 32-bit paged mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit paged mode.
  3)  CR0, CR3, and CR4 must be compatible with 32-bit paged mode.
  4)  CR3 must point to valid page tables that guarantee that the pages for
      this function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is disabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is disabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is
                      disabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is disabled.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmDisablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack
  )
{
  EntryPoint (Context1, Context2);
}

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
VOID
EFIAPI
UnitTestHostBaseLibAsmEnablePaging64 (
  IN      UINT16  Cs,
  IN      UINT64  EntryPoint,
  IN      UINT64  Context1   OPTIONAL,
  IN      UINT64  Context2   OPTIONAL,
  IN      UINT64  NewStack
  )
{
  SWITCH_STACK_ENTRY_POINT  NewEntryPoint;

  NewEntryPoint = (SWITCH_STACK_ENTRY_POINT)(UINTN)(EntryPoint);
  NewEntryPoint ((VOID *)(UINTN)Context1, (VOID *)(UINTN)Context2);
}

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
VOID
EFIAPI
UnitTestHostBaseLibAsmDisablePaging64 (
  IN      UINT16  Cs,
  IN      UINT32  EntryPoint,
  IN      UINT32  Context1   OPTIONAL,
  IN      UINT32  Context2   OPTIONAL,
  IN      UINT32  NewStack
  )
{
  SWITCH_STACK_ENTRY_POINT  NewEntryPoint;

  NewEntryPoint = (SWITCH_STACK_ENTRY_POINT)(UINTN)(EntryPoint);
  NewEntryPoint ((VOID *)(UINTN)Context1, (VOID *)(UINTN)Context2);
}

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
UnitTestHostBaseLibAsmGetThunk16Properties (
  OUT     UINT32  *RealModeBufferSize,
  OUT     UINT32  *ExtraStackSize
  )
{
  *RealModeBufferSize = 0;
  *ExtraStackSize     = 0;
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
UnitTestHostBaseLibAsmPrepareThunk16 (
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
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
  virtual to physical mappings for ThunkContext.RealModeBuffer are mapped 1:1.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmThunk16 (
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
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
UnitTestHostBaseLibAsmPrepareAndThunk16 (
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
}

/**
  Load given selector into TR register.

  @param[in] Selector     Task segment selector
**/
VOID
EFIAPI
UnitTestHostBaseLibAsmWriteTr (
  IN UINT16  Selector
  )
{
  mUnitTestHostBaseLibSegment[UNIT_TEST_HOST_BASE_LIB_SEGMENT_TR] = Selector;
}

/**
  Performs a serializing operation on all load-from-memory instructions that
  were issued prior the AsmLfence function.

  Executes a LFENCE instruction. This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
UnitTestHostBaseLibAsmLfence (
  VOID
  )
{
}

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
VOID
EFIAPI
UnitTestHostBaseLibPatchInstructionX86 (
  OUT X86_ASSEMBLY_PATCH_LABEL  *InstructionEnd,
  IN  UINT64                    PatchValue,
  IN  UINTN                     ValueSize
  )
{
}

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
UINT32
EFIAPI
AsmCpuid (
  IN      UINT32  Index,
  OUT     UINT32  *Eax   OPTIONAL,
  OUT     UINT32  *Ebx   OPTIONAL,
  OUT     UINT32  *Ecx   OPTIONAL,
  OUT     UINT32  *Edx   OPTIONAL
  )
{
  return gUnitTestHostBaseLib.X86->AsmCpuid (Index, Eax, Ebx, Ecx, Edx);
}

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
UINT32
EFIAPI
AsmCpuidEx (
  IN      UINT32  Index,
  IN      UINT32  SubIndex,
  OUT     UINT32  *Eax   OPTIONAL,
  OUT     UINT32  *Ebx   OPTIONAL,
  OUT     UINT32  *Ecx   OPTIONAL,
  OUT     UINT32  *Edx   OPTIONAL
  )
{
  return gUnitTestHostBaseLib.X86->AsmCpuidEx (Index, SubIndex, Eax, Ebx, Ecx, Edx);
}

/**
  Set CD bit and clear NW bit of CR0 followed by a WBINVD.

  Disables the caches by setting the CD bit of CR0 to 1, clearing the NW bit of CR0 to 0,
  and executing a WBINVD instruction.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmDisableCache (
  VOID
  )
{
  gUnitTestHostBaseLib.X86->AsmDisableCache ();
}

/**
  Perform a WBINVD and clear both the CD and NW bits of CR0.

  Enables the caches by executing a WBINVD instruction and then clear both the CD and NW
  bits of CR0 to 0.  This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmEnableCache (
  VOID
  )
{
  gUnitTestHostBaseLib.X86->AsmEnableCache ();
}

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
UINT64
EFIAPI
AsmReadMsr64 (
  IN      UINT32  Index
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadMsr64 (Index);
}

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
UINT64
EFIAPI
AsmWriteMsr64 (
  IN      UINT32  Index,
  IN      UINT64  Value
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteMsr64 (Index, Value);
}

/**
  Reads the current value of the Control Register 0 (CR0).

  Reads and returns the current value of CR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 0 (CR0).

**/
UINTN
EFIAPI
AsmReadCr0 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadCr0 ();
}

/**
  Reads the current value of the Control Register 2 (CR2).

  Reads and returns the current value of CR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 2 (CR2).

**/
UINTN
EFIAPI
AsmReadCr2 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadCr2 ();
}

/**
  Reads the current value of the Control Register 3 (CR3).

  Reads and returns the current value of CR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 3 (CR3).

**/
UINTN
EFIAPI
AsmReadCr3 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadCr3 ();
}

/**
  Reads the current value of the Control Register 4 (CR4).

  Reads and returns the current value of CR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of the Control Register 4 (CR4).

**/
UINTN
EFIAPI
AsmReadCr4 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadCr4 ();
}

/**
  Writes a value to Control Register 0 (CR0).

  Writes and returns a new value to CR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr0 The value to write to CR0.

  @return The value written to CR0.

**/
UINTN
EFIAPI
AsmWriteCr0 (
  UINTN  Cr0
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteCr0 (Cr0);
}

/**
  Writes a value to Control Register 2 (CR2).

  Writes and returns a new value to CR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr2 The value to write to CR2.

  @return The value written to CR2.

**/
UINTN
EFIAPI
AsmWriteCr2 (
  UINTN  Cr2
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteCr2 (Cr2);
}

/**
  Writes a value to Control Register 3 (CR3).

  Writes and returns a new value to CR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr3 The value to write to CR3.

  @return The value written to CR3.

**/
UINTN
EFIAPI
AsmWriteCr3 (
  UINTN  Cr3
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteCr3 (Cr3);
}

/**
  Writes a value to Control Register 4 (CR4).

  Writes and returns a new value to CR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Cr4 The value to write to CR4.

  @return The value written to CR4.

**/
UINTN
EFIAPI
AsmWriteCr4 (
  UINTN  Cr4
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteCr4 (Cr4);
}

/**
  Reads the current value of Debug Register 0 (DR0).

  Reads and returns the current value of DR0. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmReadDr0 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr0 ();
}

/**
  Reads the current value of Debug Register 1 (DR1).

  Reads and returns the current value of DR1. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmReadDr1 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr1 ();
}

/**
  Reads the current value of Debug Register 2 (DR2).

  Reads and returns the current value of DR2. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmReadDr2 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr2 ();
}

/**
  Reads the current value of Debug Register 3 (DR3).

  Reads and returns the current value of DR3. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmReadDr3 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr3 ();
}

/**
  Reads the current value of Debug Register 4 (DR4).

  Reads and returns the current value of DR4. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmReadDr4 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr4 ();
}

/**
  Reads the current value of Debug Register 5 (DR5).

  Reads and returns the current value of DR5. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmReadDr5 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr5 ();
}

/**
  Reads the current value of Debug Register 6 (DR6).

  Reads and returns the current value of DR6. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmReadDr6 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr6 ();
}

/**
  Reads the current value of Debug Register 7 (DR7).

  Reads and returns the current value of DR7. This function is only available
  on IA-32 and x64. This returns a 32-bit value on IA-32 and a 64-bit value on
  x64.

  @return The value of Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmReadDr7 (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDr7 ();
}

/**
  Writes a value to Debug Register 0 (DR0).

  Writes and returns a new value to DR0. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr0 The value to write to Dr0.

  @return The value written to Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmWriteDr0 (
  UINTN  Dr0
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr0 (Dr0);
}

/**
  Writes a value to Debug Register 1 (DR1).

  Writes and returns a new value to DR1. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr1 The value to write to Dr1.

  @return The value written to Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmWriteDr1 (
  UINTN  Dr1
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr1 (Dr1);
}

/**
  Writes a value to Debug Register 2 (DR2).

  Writes and returns a new value to DR2. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr2 The value to write to Dr2.

  @return The value written to Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmWriteDr2 (
  UINTN  Dr2
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr2 (Dr2);
}

/**
  Writes a value to Debug Register 3 (DR3).

  Writes and returns a new value to DR3. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr3 The value to write to Dr3.

  @return The value written to Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmWriteDr3 (
  UINTN  Dr3
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr3 (Dr3);
}

/**
  Writes a value to Debug Register 4 (DR4).

  Writes and returns a new value to DR4. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr4 The value to write to Dr4.

  @return The value written to Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmWriteDr4 (
  UINTN  Dr4
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr4 (Dr4);
}

/**
  Writes a value to Debug Register 5 (DR5).

  Writes and returns a new value to DR5. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr5 The value to write to Dr5.

  @return The value written to Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmWriteDr5 (
  UINTN  Dr5
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr5 (Dr5);
}

/**
  Writes a value to Debug Register 6 (DR6).

  Writes and returns a new value to DR6. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr6 The value to write to Dr6.

  @return The value written to Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmWriteDr6 (
  UINTN  Dr6
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr6 (Dr6);
}

/**
  Writes a value to Debug Register 7 (DR7).

  Writes and returns a new value to DR7. This function is only available on
  IA-32 and x64. This writes a 32-bit value on IA-32 and a 64-bit value on x64.

  @param  Dr7 The value to write to Dr7.

  @return The value written to Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmWriteDr7 (
  UINTN  Dr7
  )
{
  return gUnitTestHostBaseLib.X86->AsmWriteDr7 (Dr7);
}

/**
  Reads the current value of Code Segment Register (CS).

  Reads and returns the current value of CS. This function is only available on
  IA-32 and x64.

  @return The current value of CS.

**/
UINT16
EFIAPI
AsmReadCs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadCs ();
}

/**
  Reads the current value of Data Segment Register (DS).

  Reads and returns the current value of DS. This function is only available on
  IA-32 and x64.

  @return The current value of DS.

**/
UINT16
EFIAPI
AsmReadDs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadDs ();
}

/**
  Reads the current value of Extra Segment Register (ES).

  Reads and returns the current value of ES. This function is only available on
  IA-32 and x64.

  @return The current value of ES.

**/
UINT16
EFIAPI
AsmReadEs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadEs ();
}

/**
  Reads the current value of FS Data Segment Register (FS).

  Reads and returns the current value of FS. This function is only available on
  IA-32 and x64.

  @return The current value of FS.

**/
UINT16
EFIAPI
AsmReadFs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadFs ();
}

/**
  Reads the current value of GS Data Segment Register (GS).

  Reads and returns the current value of GS. This function is only available on
  IA-32 and x64.

  @return The current value of GS.

**/
UINT16
EFIAPI
AsmReadGs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadGs ();
}

/**
  Reads the current value of Stack Segment Register (SS).

  Reads and returns the current value of SS. This function is only available on
  IA-32 and x64.

  @return The current value of SS.

**/
UINT16
EFIAPI
AsmReadSs (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadSs ();
}

/**
  Reads the current value of Task Register (TR).

  Reads and returns the current value of TR. This function is only available on
  IA-32 and x64.

  @return The current value of TR.

**/
UINT16
EFIAPI
AsmReadTr (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadTr ();
}

/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
AsmReadGdtr (
  OUT     IA32_DESCRIPTOR  *Gdtr
  )
{
  gUnitTestHostBaseLib.X86->AsmReadGdtr (Gdtr);
}

/**
  Writes the current Global Descriptor Table Register (GDTR) descriptor.

  Writes and the current GDTR descriptor specified by Gdtr. This function is
  only available on IA-32 and x64.

  If Gdtr is NULL, then ASSERT().

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
AsmWriteGdtr (
  IN      CONST IA32_DESCRIPTOR  *Gdtr
  )
{
  gUnitTestHostBaseLib.X86->AsmWriteGdtr (Gdtr);
}

/**
  Reads the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmReadIdtr (
  OUT     IA32_DESCRIPTOR  *Idtr
  )
{
  gUnitTestHostBaseLib.X86->AsmReadIdtr (Idtr);
}

/**
  Writes the current Interrupt Descriptor Table Register(IDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and x64.

  If Idtr is NULL, then ASSERT().

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
AsmWriteIdtr (
  IN      CONST IA32_DESCRIPTOR  *Idtr
  )
{
  gUnitTestHostBaseLib.X86->AsmWriteIdtr (Idtr);
}

/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and x64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
AsmReadLdtr (
  VOID
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadLdtr ();
}

/**
  Writes the current Local Descriptor Table Register (LDTR) selector.

  Writes and the current LDTR descriptor specified by Ldtr. This function is
  only available on IA-32 and x64.

  @param  Ldtr  16-bit LDTR selector value.

**/
VOID
EFIAPI
AsmWriteLdtr (
  IN      UINT16  Ldtr
  )
{
  gUnitTestHostBaseLib.X86->AsmWriteLdtr (Ldtr);
}

/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and x64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
UINT64
EFIAPI
AsmReadPmc (
  IN      UINT32  Index
  )
{
  return gUnitTestHostBaseLib.X86->AsmReadPmc (Index);
}

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
UINTN
EFIAPI
AsmMonitor (
  IN      UINTN  Eax,
  IN      UINTN  Ecx,
  IN      UINTN  Edx
  )
{
  return gUnitTestHostBaseLib.X86->AsmMonitor (Eax, Ecx, Edx);
}

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
UINTN
EFIAPI
AsmMwait (
  IN      UINTN  Eax,
  IN      UINTN  Ecx
  )
{
  return gUnitTestHostBaseLib.X86->AsmMwait (Eax, Ecx);
}

/**
  Executes a WBINVD instruction.

  Executes a WBINVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
AsmWbinvd (
  VOID
  )
{
  gUnitTestHostBaseLib.X86->AsmWbinvd ();
}

/**
  Executes a INVD instruction.

  Executes a INVD instruction. This function is only available on IA-32 and
  x64.

**/
VOID
EFIAPI
AsmInvd (
  VOID
  )
{
  gUnitTestHostBaseLib.X86->AsmInvd ();
}

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
VOID *
EFIAPI
AsmFlushCacheLine (
  IN      VOID  *LinearAddress
  )
{
  return gUnitTestHostBaseLib.X86->AsmFlushCacheLine (LinearAddress);
}

/**
  Enables the 32-bit paging mode on the CPU.

  Enables the 32-bit paging mode on the CPU. CR0, CR3, CR4, and the page tables
  must be properly initialized prior to calling this service. This function
  assumes the current execution mode is 32-bit protected mode. This function is
  only available on IA-32. After the 32-bit paging mode is enabled, control is
  transferred to the function specified by EntryPoint using the new stack
  specified by NewStack and passing in the parameters specified by Context1 and
  Context2. Context1 and Context2 are optional and may be NULL. The function
  EntryPoint must never return.

  If the current execution mode is not 32-bit protected mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit protected mode with flat descriptors. This
      means all descriptors must have a base of 0 and a limit of 4GB.
  3)  CR0 and CR4 must be compatible with 32-bit protected mode with flat
      descriptors.
  4)  CR3 must point to valid page tables that will be used once the transition
      is complete, and those page tables must guarantee that the pages for this
      function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is enabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is enabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is enabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is enabled.

**/
VOID
EFIAPI
AsmEnablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack
  )
{
  gUnitTestHostBaseLib.X86->AsmEnablePaging32 (EntryPoint, Context1, Context2, NewStack);
}

/**
  Disables the 32-bit paging mode on the CPU.

  Disables the 32-bit paging mode on the CPU and returns to 32-bit protected
  mode. This function assumes the current execution mode is 32-paged protected
  mode. This function is only available on IA-32. After the 32-bit paging mode
  is disabled, control is transferred to the function specified by EntryPoint
  using the new stack specified by NewStack and passing in the parameters
  specified by Context1 and Context2. Context1 and Context2 are optional and
  may be NULL. The function EntryPoint must never return.

  If the current execution mode is not 32-bit paged mode, then ASSERT().
  If EntryPoint is NULL, then ASSERT().
  If NewStack is NULL, then ASSERT().

  There are a number of constraints that must be followed before calling this
  function:
  1)  Interrupts must be disabled.
  2)  The caller must be in 32-bit paged mode.
  3)  CR0, CR3, and CR4 must be compatible with 32-bit paged mode.
  4)  CR3 must point to valid page tables that guarantee that the pages for
      this function and the stack are identity mapped.

  @param  EntryPoint  A pointer to function to call with the new stack after
                      paging is disabled.
  @param  Context1    A pointer to the context to pass into the EntryPoint
                      function as the first parameter after paging is disabled.
  @param  Context2    A pointer to the context to pass into the EntryPoint
                      function as the second parameter after paging is
                      disabled.
  @param  NewStack    A pointer to the new stack to use for the EntryPoint
                      function after paging is disabled.

**/
VOID
EFIAPI
AsmDisablePaging32 (
  IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
  IN      VOID                      *Context1   OPTIONAL,
  IN      VOID                      *Context2   OPTIONAL,
  IN      VOID                      *NewStack
  )
{
  gUnitTestHostBaseLib.X86->AsmDisablePaging32 (EntryPoint, Context1, Context2, NewStack);
}

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
VOID
EFIAPI
AsmEnablePaging64 (
  IN      UINT16  Cs,
  IN      UINT64  EntryPoint,
  IN      UINT64  Context1   OPTIONAL,
  IN      UINT64  Context2   OPTIONAL,
  IN      UINT64  NewStack
  )
{
  gUnitTestHostBaseLib.X86->AsmEnablePaging64 (Cs, EntryPoint, Context1, Context2, NewStack);
}

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
VOID
EFIAPI
AsmDisablePaging64 (
  IN      UINT16  Cs,
  IN      UINT32  EntryPoint,
  IN      UINT32  Context1   OPTIONAL,
  IN      UINT32  Context2   OPTIONAL,
  IN      UINT32  NewStack
  )
{
  gUnitTestHostBaseLib.X86->AsmDisablePaging64 (Cs, EntryPoint, Context1, Context2, NewStack);
}

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
  OUT     UINT32  *RealModeBufferSize,
  OUT     UINT32  *ExtraStackSize
  )
{
  gUnitTestHostBaseLib.X86->AsmGetThunk16Properties (RealModeBufferSize, ExtraStackSize);
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
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
  gUnitTestHostBaseLib.X86->AsmPrepareThunk16 (ThunkContext);
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
  virtual to physical mappings for ThunkContext.RealModeBuffer are mapped 1:1.

  @param  ThunkContext  A pointer to the context structure that describes the
                        16-bit real mode code to call.

**/
VOID
EFIAPI
AsmThunk16 (
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
  gUnitTestHostBaseLib.X86->AsmThunk16 (ThunkContext);
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
  IN OUT  THUNK_CONTEXT  *ThunkContext
  )
{
  gUnitTestHostBaseLib.X86->AsmPrepareAndThunk16 (ThunkContext);
}

/**
  Load given selector into TR register.

  @param[in] Selector     Task segment selector
**/
VOID
EFIAPI
AsmWriteTr (
  IN UINT16  Selector
  )
{
  gUnitTestHostBaseLib.X86->AsmWriteTr (Selector);
}

/**
  Performs a serializing operation on all load-from-memory instructions that
  were issued prior the AsmLfence function.

  Executes a LFENCE instruction. This function is only available on IA-32 and x64.

**/
VOID
EFIAPI
AsmLfence (
  VOID
  )
{
  gUnitTestHostBaseLib.X86->AsmLfence ();
}

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
VOID
EFIAPI
PatchInstructionX86 (
  OUT X86_ASSEMBLY_PATCH_LABEL  *InstructionEnd,
  IN  UINT64                    PatchValue,
  IN  UINTN                     ValueSize
  )
{
  gUnitTestHostBaseLib.X86->PatchInstructionX86 (InstructionEnd, PatchValue, ValueSize);
}

///
/// Common services
///
STATIC UNIT_TEST_HOST_BASE_LIB_COMMON  mUnitTestHostBaseLibCommon = {
  UnitTestHostBaseLibEnableInterrupts,
  UnitTestHostBaseLibDisableInterrupts,
  UnitTestHostBaseLibEnableDisableInterrupts,
  UnitTestHostBaseLibGetInterruptState,
};

///
/// IA32/X64 services
///
STATIC UNIT_TEST_HOST_BASE_LIB_X86  mUnitTestHostBaseLibX86 = {
  UnitTestHostBaseLibAsmCpuid,
  UnitTestHostBaseLibAsmCpuidEx,
  UnitTestHostBaseLibAsmDisableCache,
  UnitTestHostBaseLibAsmEnableCache,
  UnitTestHostBaseLibAsmReadMsr64,
  UnitTestHostBaseLibAsmWriteMsr64,
  UnitTestHostBaseLibAsmReadCr0,
  UnitTestHostBaseLibAsmReadCr2,
  UnitTestHostBaseLibAsmReadCr3,
  UnitTestHostBaseLibAsmReadCr4,
  UnitTestHostBaseLibAsmWriteCr0,
  UnitTestHostBaseLibAsmWriteCr2,
  UnitTestHostBaseLibAsmWriteCr3,
  UnitTestHostBaseLibAsmWriteCr4,
  UnitTestHostBaseLibAsmReadDr0,
  UnitTestHostBaseLibAsmReadDr1,
  UnitTestHostBaseLibAsmReadDr2,
  UnitTestHostBaseLibAsmReadDr3,
  UnitTestHostBaseLibAsmReadDr4,
  UnitTestHostBaseLibAsmReadDr5,
  UnitTestHostBaseLibAsmReadDr6,
  UnitTestHostBaseLibAsmReadDr7,
  UnitTestHostBaseLibAsmWriteDr0,
  UnitTestHostBaseLibAsmWriteDr1,
  UnitTestHostBaseLibAsmWriteDr2,
  UnitTestHostBaseLibAsmWriteDr3,
  UnitTestHostBaseLibAsmWriteDr4,
  UnitTestHostBaseLibAsmWriteDr5,
  UnitTestHostBaseLibAsmWriteDr6,
  UnitTestHostBaseLibAsmWriteDr7,
  UnitTestHostBaseLibAsmReadCs,
  UnitTestHostBaseLibAsmReadDs,
  UnitTestHostBaseLibAsmReadEs,
  UnitTestHostBaseLibAsmReadFs,
  UnitTestHostBaseLibAsmReadGs,
  UnitTestHostBaseLibAsmReadSs,
  UnitTestHostBaseLibAsmReadTr,
  UnitTestHostBaseLibAsmReadGdtr,
  UnitTestHostBaseLibAsmWriteGdtr,
  UnitTestHostBaseLibAsmReadIdtr,
  UnitTestHostBaseLibAsmWriteIdtr,
  UnitTestHostBaseLibAsmReadLdtr,
  UnitTestHostBaseLibAsmWriteLdtr,
  UnitTestHostBaseLibAsmReadPmc,
  UnitTestHostBaseLibAsmMonitor,
  UnitTestHostBaseLibAsmMwait,
  UnitTestHostBaseLibAsmWbinvd,
  UnitTestHostBaseLibAsmInvd,
  UnitTestHostBaseLibAsmFlushCacheLine,
  UnitTestHostBaseLibAsmEnablePaging32,
  UnitTestHostBaseLibAsmDisablePaging32,
  UnitTestHostBaseLibAsmEnablePaging64,
  UnitTestHostBaseLibAsmDisablePaging64,
  UnitTestHostBaseLibAsmGetThunk16Properties,
  UnitTestHostBaseLibAsmPrepareThunk16,
  UnitTestHostBaseLibAsmThunk16,
  UnitTestHostBaseLibAsmPrepareAndThunk16,
  UnitTestHostBaseLibAsmWriteTr,
  UnitTestHostBaseLibAsmLfence,
  UnitTestHostBaseLibPatchInstructionX86
};

///
/// Structure of hook functions for BaseLib functions that can not be used from
/// a host application.  A simple emulation of these function is provided by
/// default.  A specific unit test can provide its own implementation for any
/// of these functions.
///
UNIT_TEST_HOST_BASE_LIB  gUnitTestHostBaseLib = {
  &mUnitTestHostBaseLibCommon,
  &mUnitTestHostBaseLibX86
};
