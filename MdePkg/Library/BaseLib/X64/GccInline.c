/** @file
  GCC inline implementation of BaseLib processor specific functions.
  
  Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR> 
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
  Enables CPU interrupts.

  Enables CPU interrupts.

**/
VOID
EFIAPI
EnableInterrupts (
  VOID
  )
{
  __asm__ __volatile__ ("sti"::: "memory");
}


/**
  Disables CPU interrupts.

  Disables CPU interrupts.

**/
VOID
EFIAPI
DisableInterrupts (
  VOID
  )
{  
  __asm__ __volatile__ ("cli"::: "memory");
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
  Returns a 64-bit Machine Specific Register(MSR).

  Reads and returns the 64-bit MSR specified by Index. No parameter checking is
  performed on Index, and some Index values may cause CPU exceptions. The
  caller must either guarantee that Index is valid, or the caller must set up
  exception handlers to catch the exceptions. This function is only available
  on IA-32 and X64.

  @param  Index The 32-bit MSR index to read.

  @return The value of the MSR identified by Index.

**/
UINT64
EFIAPI
AsmReadMsr64 (
  IN      UINT32                    Index
  )
{
  UINT32 LowData;
  UINT32 HighData;
  
  __asm__ __volatile__ (
    "rdmsr"
    : "=a" (LowData),   // %0
      "=d" (HighData)   // %1
    : "c"  (Index)      // %2
    );
    
  return (((UINT64)HighData) << 32) | LowData;
}

/**
  Writes a 64-bit value to a Machine Specific Register(MSR), and returns the
  value.

  Writes the 64-bit value specified by Value to the MSR specified by Index. The
  64-bit value written to the MSR is returned. No parameter checking is
  performed on Index or Value, and some of these may cause CPU exceptions. The
  caller must either guarantee that Index and Value are valid, or the caller
  must establish proper exception handlers. This function is only available on
  IA-32 and X64.

  @param  Index The 32-bit MSR index to write.
  @param  Value The 64-bit value to write to the MSR.

  @return Value

**/
UINT64
EFIAPI
AsmWriteMsr64 (
  IN      UINT32                    Index,
  IN      UINT64                    Value
  )
{
  UINT32 LowData;
  UINT32 HighData;

  LowData  = (UINT32)(Value);
  HighData = (UINT32)(Value >> 32);
  
  __asm__ __volatile__ (
    "wrmsr"
    :
    : "c" (Index),
      "a" (LowData),
      "d" (HighData)
    );
    
  return Value;
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
  UINTN Eflags;
  
  __asm__ __volatile__ (
    "pushfq         \n\t"
    "pop     %0         "
    : "=r" (Eflags)       // %0
    );
    
  return Eflags;
}



/**
  Reads the current value of the Control Register 0 (CR0).

  Reads and returns the current value of CR0. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of the Control Register 0 (CR0).

**/
UINTN
EFIAPI
AsmReadCr0 (
  VOID
  )
{
  UINTN   Data;
  
  __asm__ __volatile__ (
    "mov  %%cr0,%0" 
    : "=r" (Data)           // %0
    );
  
  return Data;
}


/**
  Reads the current value of the Control Register 2 (CR2).

  Reads and returns the current value of CR2. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of the Control Register 2 (CR2).

**/
UINTN
EFIAPI
AsmReadCr2 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%cr2,  %0" 
    : "=r" (Data)           // %0
    );
  
  return Data;
}

/**
  Reads the current value of the Control Register 3 (CR3).

  Reads and returns the current value of CR3. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of the Control Register 3 (CR3).

**/
UINTN
EFIAPI
AsmReadCr3 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%cr3,  %0" 
    : "=r" (Data)           // %0
    );
  
  return Data;
}


/**
  Reads the current value of the Control Register 4 (CR4).

  Reads and returns the current value of CR4. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of the Control Register 4 (CR4).

**/
UINTN
EFIAPI
AsmReadCr4 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%cr4,  %0" 
    : "=r" (Data)           // %0
    );
  
  return Data;
}


/**
  Writes a value to Control Register 0 (CR0).

  Writes and returns a new value to CR0. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Cr0 The value to write to CR0.

  @return The value written to CR0.

**/
UINTN
EFIAPI
AsmWriteCr0 (
  UINTN  Cr0
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%cr0"
    :
    : "r" (Cr0)
    );
  return Cr0;
}


/**
  Writes a value to Control Register 2 (CR2).

  Writes and returns a new value to CR2. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Cr2 The value to write to CR2.

  @return The value written to CR2.

**/
UINTN
EFIAPI
AsmWriteCr2 (
  UINTN  Cr2
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%cr2"
    :
    : "r" (Cr2)
    );
  return Cr2;
}


/**
  Writes a value to Control Register 3 (CR3).

  Writes and returns a new value to CR3. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Cr3 The value to write to CR3.

  @return The value written to CR3.

**/
UINTN
EFIAPI
AsmWriteCr3 (
  UINTN  Cr3
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%cr3"
    :
    : "r" (Cr3)
    );
  return Cr3;
}


/**
  Writes a value to Control Register 4 (CR4).

  Writes and returns a new value to CR4. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Cr4 The value to write to CR4.

  @return The value written to CR4.

**/
UINTN
EFIAPI
AsmWriteCr4 (
  UINTN  Cr4
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%cr4"
    :
    : "r" (Cr4)
    );
  return Cr4;
}


/**
  Reads the current value of Debug Register 0 (DR0).

  Reads and returns the current value of DR0. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmReadDr0 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr0, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 1 (DR1).

  Reads and returns the current value of DR1. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmReadDr1 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr1, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 2 (DR2).

  Reads and returns the current value of DR2. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmReadDr2 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr2, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 3 (DR3).

  Reads and returns the current value of DR3. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmReadDr3 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr3, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 4 (DR4).

  Reads and returns the current value of DR4. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmReadDr4 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr4, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 5 (DR5).

  Reads and returns the current value of DR5. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmReadDr5 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr5, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 6 (DR6).

  Reads and returns the current value of DR6. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmReadDr6 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr6, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Reads the current value of Debug Register 7 (DR7).

  Reads and returns the current value of DR7. This function is only available
  on IA-32 and X64. This returns a 32-bit value on IA-32 and a 64-bit value on
  X64.

  @return The value of Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmReadDr7 (
  VOID
  )
{
  UINTN Data;
  
  __asm__ __volatile__ (
    "mov  %%dr7, %0"
    : "=r" (Data)
    );
  
  return Data;
}


/**
  Writes a value to Debug Register 0 (DR0).

  Writes and returns a new value to DR0. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr0 The value to write to Dr0.

  @return The value written to Debug Register 0 (DR0).

**/
UINTN
EFIAPI
AsmWriteDr0 (
  UINTN  Dr0
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr0"
    :
    : "r" (Dr0)
    );
  return Dr0;
}


/**
  Writes a value to Debug Register 1 (DR1).

  Writes and returns a new value to DR1. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr1 The value to write to Dr1.

  @return The value written to Debug Register 1 (DR1).

**/
UINTN
EFIAPI
AsmWriteDr1 (
  UINTN  Dr1
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr1"
    :
    : "r" (Dr1)
    );
  return Dr1;
}


/**
  Writes a value to Debug Register 2 (DR2).

  Writes and returns a new value to DR2. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr2 The value to write to Dr2.

  @return The value written to Debug Register 2 (DR2).

**/
UINTN
EFIAPI
AsmWriteDr2 (
  UINTN  Dr2
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr2"
    :
    : "r" (Dr2)
    );
  return Dr2;
}


/**
  Writes a value to Debug Register 3 (DR3).

  Writes and returns a new value to DR3. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr3 The value to write to Dr3.

  @return The value written to Debug Register 3 (DR3).

**/
UINTN
EFIAPI
AsmWriteDr3 (
  UINTN  Dr3
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr3"
    :
    : "r" (Dr3)
    );
  return Dr3;
}


/**
  Writes a value to Debug Register 4 (DR4).

  Writes and returns a new value to DR4. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr4 The value to write to Dr4.

  @return The value written to Debug Register 4 (DR4).

**/
UINTN
EFIAPI
AsmWriteDr4 (
  UINTN  Dr4
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr4"
    :
    : "r" (Dr4)
    );
  return Dr4;
}


/**
  Writes a value to Debug Register 5 (DR5).

  Writes and returns a new value to DR5. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr5 The value to write to Dr5.

  @return The value written to Debug Register 5 (DR5).

**/
UINTN
EFIAPI
AsmWriteDr5 (
  UINTN  Dr5
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr5"
    :
    : "r" (Dr5)
    );
  return Dr5;
}


/**
  Writes a value to Debug Register 6 (DR6).

  Writes and returns a new value to DR6. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr6 The value to write to Dr6.

  @return The value written to Debug Register 6 (DR6).

**/
UINTN
EFIAPI
AsmWriteDr6 (
  UINTN  Dr6
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr6"
    :
    : "r" (Dr6)
    );
  return Dr6;
}


/**
  Writes a value to Debug Register 7 (DR7).

  Writes and returns a new value to DR7. This function is only available on
  IA-32 and X64. This writes a 32-bit value on IA-32 and a 64-bit value on X64.

  @param  Dr7 The value to write to Dr7.

  @return The value written to Debug Register 7 (DR7).

**/
UINTN
EFIAPI
AsmWriteDr7 (
  UINTN  Dr7
  )
{
  __asm__ __volatile__ (
    "mov  %0, %%dr7"
    :
    : "r" (Dr7)
    );
  return Dr7;
}


/**
  Reads the current value of Code Segment Register (CS).

  Reads and returns the current value of CS. This function is only available on
  IA-32 and X64.

  @return The current value of CS.

**/
UINT16
EFIAPI
AsmReadCs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov   %%cs, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of Data Segment Register (DS).

  Reads and returns the current value of DS. This function is only available on
  IA-32 and X64.

  @return The current value of DS.

**/
UINT16
EFIAPI
AsmReadDs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov  %%ds, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of Extra Segment Register (ES).

  Reads and returns the current value of ES. This function is only available on
  IA-32 and X64.

  @return The current value of ES.

**/
UINT16
EFIAPI
AsmReadEs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov  %%es, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of FS Data Segment Register (FS).

  Reads and returns the current value of FS. This function is only available on
  IA-32 and X64.

  @return The current value of FS.

**/
UINT16
EFIAPI
AsmReadFs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov  %%fs, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of GS Data Segment Register (GS).

  Reads and returns the current value of GS. This function is only available on
  IA-32 and X64.

  @return The current value of GS.

**/
UINT16
EFIAPI
AsmReadGs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov  %%gs, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of Stack Segment Register (SS).

  Reads and returns the current value of SS. This function is only available on
  IA-32 and X64.

  @return The current value of SS.

**/
UINT16
EFIAPI
AsmReadSs (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "mov  %%ds, %0"
    :"=a" (Data)
    );
    
  return Data;
}


/**
  Reads the current value of Task Register (TR).

  Reads and returns the current value of TR. This function is only available on
  IA-32 and X64.

  @return The current value of TR.

**/
UINT16
EFIAPI
AsmReadTr (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "str  %0"
    : "=r" (Data)
    );
    
  return Data;
}


/**
  Reads the current Global Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current GDTR descriptor and returns it in Gdtr. This
  function is only available on IA-32 and X64.

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
InternalX86ReadGdtr (
  OUT     IA32_DESCRIPTOR           *Gdtr
  )
{
  __asm__ __volatile__ (
    "sgdt %0"
    : "=m" (*Gdtr)
    );
}


/**
  Writes the current Global Descriptor Table Register (GDTR) descriptor.

  Writes and the current GDTR descriptor specified by Gdtr. This function is
  only available on IA-32 and X64.

  @param  Gdtr  The pointer to a GDTR descriptor.

**/
VOID
EFIAPI
InternalX86WriteGdtr (
  IN      CONST IA32_DESCRIPTOR     *Gdtr
  )
{
  __asm__ __volatile__ (
    "lgdt %0"
    :
    : "m" (*Gdtr)
    );
    
}


/**
  Reads the current Interrupt Descriptor Table Register(GDTR) descriptor.

  Reads and returns the current IDTR descriptor and returns it in Idtr. This
  function is only available on IA-32 and X64.

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
InternalX86ReadIdtr (
  OUT     IA32_DESCRIPTOR           *Idtr
  )
{
  __asm__ __volatile__ (
    "sidt  %0"
    : "=m" (*Idtr)
    );
}


/**
  Writes the current Interrupt Descriptor Table Register(GDTR) descriptor.

  Writes the current IDTR descriptor and returns it in Idtr. This function is
  only available on IA-32 and X64.

  @param  Idtr  The pointer to a IDTR descriptor.

**/
VOID
EFIAPI
InternalX86WriteIdtr (
  IN      CONST IA32_DESCRIPTOR     *Idtr
  )
{
  __asm__ __volatile__ (
    "lidt %0"
    :
    : "m" (*Idtr)
    );
}


/**
  Reads the current Local Descriptor Table Register(LDTR) selector.

  Reads and returns the current 16-bit LDTR descriptor value. This function is
  only available on IA-32 and X64.

  @return The current selector of LDT.

**/
UINT16
EFIAPI
AsmReadLdtr (
  VOID
  )
{
  UINT16  Data;
  
  __asm__ __volatile__ (
    "sldt  %0"
    : "=g" (Data)   // %0
    );
    
  return Data;
}


/**
  Writes the current Local Descriptor Table Register (GDTR) selector.

  Writes and the current LDTR descriptor specified by Ldtr. This function is
  only available on IA-32 and X64.

  @param  Ldtr  16-bit LDTR selector value.

**/
VOID
EFIAPI
AsmWriteLdtr (
  IN      UINT16                    Ldtr
  )
{
  __asm__ __volatile__ (
    "lldtw  %0"
    :
    : "g" (Ldtr)   // %0
    );
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
  OUT     IA32_FX_BUFFER            *Buffer
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
  IN      CONST IA32_FX_BUFFER      *Buffer
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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
  IN      UINT64                    Value
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


/**
  Reads the current value of a Performance Counter (PMC).

  Reads and returns the current value of performance counter specified by
  Index. This function is only available on IA-32 and X64.

  @param  Index The 32-bit Performance Counter index to read.

  @return The value of the PMC specified by Index.

**/
UINT64
EFIAPI
AsmReadPmc (
  IN      UINT32                    Index
  )
{
  UINT32  LowData;
  UINT32  HiData;
  
  __asm__ __volatile__ (
    "rdpmc"
    : "=a" (LowData),
      "=d" (HiData)
    : "c"  (Index)
    );
  
  return (((UINT64)HiData) << 32) | LowData;  
}


/**
  Sets up a monitor buffer that is used by AsmMwait().

  Executes a MONITOR instruction with the register state specified by Eax, Ecx
  and Edx. Returns Eax. This function is only available on IA-32 and X64.

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
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx,
  IN      UINTN                     Edx
  )
{
  __asm__ __volatile__ (
    "monitor"
    :
    : "a" (Eax),
      "c" (Ecx),
      "d" (Edx)
    );
    
  return Eax;
}


/**
  Executes an MWAIT instruction.

  Executes an MWAIT instruction with the register state specified by Eax and
  Ecx. Returns Eax. This function is only available on IA-32 and X64.

  @param  Eax The value to load into EAX or RAX before executing the MONITOR
              instruction.
  @param  Ecx The value to load into ECX or RCX before executing the MONITOR
              instruction.

  @return Eax

**/
UINTN
EFIAPI
AsmMwait (
  IN      UINTN                     Eax,
  IN      UINTN                     Ecx
  )
{
  __asm__ __volatile__ (
    "mwait"
    : 
    : "a"  (Eax),
      "c"  (Ecx)
    );
    
  return Eax;    
}


/**
  Executes a WBINVD instruction.

  Executes a WBINVD instruction. This function is only available on IA-32 and
  X64.

**/
VOID
EFIAPI
AsmWbinvd (
  VOID
  )
{
  __asm__ __volatile__ ("wbinvd":::"memory");
}


/**
  Executes a INVD instruction.

  Executes a INVD instruction. This function is only available on IA-32 and
  X64.

**/
VOID
EFIAPI
AsmInvd (
  VOID
  )
{
  __asm__ __volatile__ ("invd":::"memory");
    
}


/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  Flushed the cache line specified by LinearAddress, and returns LinearAddress.
  This function is only available on IA-32 and X64.

  @param  LinearAddress The address of the cache line to flush. If the CPU is
                        in a physical addressing mode, then LinearAddress is a
                        physical address. If the CPU is in a virtual
                        addressing mode, then LinearAddress is a virtual
                        address.

  @return LinearAddress
**/
VOID *
EFIAPI
AsmFlushCacheLine (
  IN      VOID                      *LinearAddress
  )
{
  __asm__ __volatile__ (
    "clflush (%0)"
    :
    : "r" (LinearAddress) 
    : "memory"
    );
    
    return LinearAddress;
}


