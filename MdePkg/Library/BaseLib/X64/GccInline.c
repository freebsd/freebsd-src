/** @file
  GCC inline implementation of BaseLib processor specific functions.

  Copyright (c) 2006 - 2020, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Used to serialize load and store operations.

  All loads and stores that proceed calls to this function are guaranteed to be
  globally visible when this function returns.

**/
VOID
EFIAPI
MemoryFence (
  VOID
  )
{
  // This is a little bit of overkill and it is more about the compiler that it is
  // actually processor synchronization. This is like the _ReadWriteBarrier
  // Microsoft specific intrinsic
  __asm__ __volatile__ ("":::"memory");
}

/**
  Requests CPU to pause for a short period of time.

  Requests CPU to pause for a short period of time. Typically used in MP
  systems to prevent memory starvation while waiting for a spin lock.

**/
VOID
EFIAPI
CpuPause (
  VOID
  )
{
  __asm__ __volatile__ ("pause");
}

/**
  Generates a breakpoint on the CPU.

  Generates a breakpoint on the CPU. The breakpoint must be implemented such
  that code can resume normal execution after the breakpoint.

**/
VOID
EFIAPI
CpuBreakpoint (
  VOID
  )
{
  __asm__ __volatile__ ("int $3");
}

/**
  Reads the current value of the EFLAGS register.

  Reads and returns the current value of the EFLAGS register. This function is
  only available on IA-32 and X64. This returns a 32-bit value on IA-32 and a
  64-bit value on X64.

  @return EFLAGS on IA-32 or RFLAGS on X64.

**/
UINTN
EFIAPI
AsmReadEflags (
  VOID
  )
{
  UINTN  Eflags;

  __asm__ __volatile__ (
    "pushfq         \n\t"
    "pop     %0         "
    : "=r" (Eflags)       // %0
  );

  return Eflags;
}

/**
  Save the current floating point/SSE/SSE2 context to a buffer.

  Saves the current floating point/SSE/SSE2 state to the buffer specified by
  Buffer. Buffer must be aligned on a 16-byte boundary. This function is only
  available on IA-32 and X64.

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
InternalX86FxSave (
  OUT     IA32_FX_BUFFER  *Buffer
  )
{
  __asm__ __volatile__ (
    "fxsave %0"
    :
    : "m" (*Buffer)  // %0
  );
}

/**
  Restores the current floating point/SSE/SSE2 context from a buffer.

  Restores the current floating point/SSE/SSE2 state from the buffer specified
  by Buffer. Buffer must be aligned on a 16-byte boundary. This function is
  only available on IA-32 and X64.

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
InternalX86FxRestore (
  IN      CONST IA32_FX_BUFFER  *Buffer
  )
{
  __asm__ __volatile__ (
    "fxrstor %0"
    :
    : "m" (*Buffer)  // %0
  );
}

/**
  Reads the current value of 64-bit MMX Register #0 (MM0).

  Reads and returns the current value of MM0. This function is only available
  on IA-32 and X64.

  @return The current value of MM0.

**/
UINT64
EFIAPI
AsmReadMm0 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd   %%mm0,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #1 (MM1).

  Reads and returns the current value of MM1. This function is only available
  on IA-32 and X64.

  @return The current value of MM1.

**/
UINT64
EFIAPI
AsmReadMm1 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd   %%mm1,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #2 (MM2).

  Reads and returns the current value of MM2. This function is only available
  on IA-32 and X64.

  @return The current value of MM2.

**/
UINT64
EFIAPI
AsmReadMm2 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm2,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #3 (MM3).

  Reads and returns the current value of MM3. This function is only available
  on IA-32 and X64.

  @return The current value of MM3.

**/
UINT64
EFIAPI
AsmReadMm3 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm3,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #4 (MM4).

  Reads and returns the current value of MM4. This function is only available
  on IA-32 and X64.

  @return The current value of MM4.

**/
UINT64
EFIAPI
AsmReadMm4 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm4,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #5 (MM5).

  Reads and returns the current value of MM5. This function is only available
  on IA-32 and X64.

  @return The current value of MM5.

**/
UINT64
EFIAPI
AsmReadMm5 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm5,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #6 (MM6).

  Reads and returns the current value of MM6. This function is only available
  on IA-32 and X64.

  @return The current value of MM6.

**/
UINT64
EFIAPI
AsmReadMm6 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm6,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Reads the current value of 64-bit MMX Register #7 (MM7).

  Reads and returns the current value of MM7. This function is only available
  on IA-32 and X64.

  @return The current value of MM7.

**/
UINT64
EFIAPI
AsmReadMm7 (
  VOID
  )
{
  UINT64  Data;

  __asm__ __volatile__ (
    "movd  %%mm7,  %0    \n\t"
    : "=r"  (Data)       // %0
  );

  return Data;
}

/**
  Writes the current value of 64-bit MMX Register #0 (MM0).

  Writes the current value of MM0. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM0.

**/
VOID
EFIAPI
AsmWriteMm0 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm0"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #1 (MM1).

  Writes the current value of MM1. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM1.

**/
VOID
EFIAPI
AsmWriteMm1 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm1"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #2 (MM2).

  Writes the current value of MM2. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM2.

**/
VOID
EFIAPI
AsmWriteMm2 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm2"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #3 (MM3).

  Writes the current value of MM3. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM3.

**/
VOID
EFIAPI
AsmWriteMm3 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm3"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #4 (MM4).

  Writes the current value of MM4. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM4.

**/
VOID
EFIAPI
AsmWriteMm4 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm4"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #5 (MM5).

  Writes the current value of MM5. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM5.

**/
VOID
EFIAPI
AsmWriteMm5 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm5"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #6 (MM6).

  Writes the current value of MM6. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM6.

**/
VOID
EFIAPI
AsmWriteMm6 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm6"  // %0
    :
    : "m" (Value)
  );
}

/**
  Writes the current value of 64-bit MMX Register #7 (MM7).

  Writes the current value of MM7. This function is only available on IA32 and
  X64.

  @param  Value The 64-bit value to write to MM7.

**/
VOID
EFIAPI
AsmWriteMm7 (
  IN      UINT64  Value
  )
{
  __asm__ __volatile__ (
    "movd  %0, %%mm7"  // %0
    :
    : "m" (Value)
  );
}

/**
  Reads the current value of Time Stamp Counter (TSC).

  Reads and returns the current value of TSC. This function is only available
  on IA-32 and X64.

  @return The current value of TSC

**/
UINT64
EFIAPI
AsmReadTsc (
  VOID
  )
{
  UINT32  LowData;
  UINT32  HiData;

  __asm__ __volatile__ (
    "rdtsc"
    : "=a" (LowData),
      "=d" (HiData)
  );

  return (((UINT64)HiData) << 32) | LowData;
}
