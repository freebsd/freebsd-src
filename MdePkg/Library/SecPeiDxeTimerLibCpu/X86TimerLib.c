/** @file
  Timer Library functions built upon local APIC on IA32/x64.

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/TimerLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>

#define APIC_SVR     0x0f0
#define APIC_LVTERR  0x370
#define APIC_TMICT   0x380
#define APIC_TMCCT   0x390
#define APIC_TDCR    0x3e0

//
// The following array is used in calculating the frequency of local APIC
// timer. Refer to IA-32 developers' manual for more details.
//
GLOBAL_REMOVE_IF_UNREFERENCED
CONST UINT8  mTimerLibLocalApicDivisor[] = {
  0x02, 0x04, 0x08, 0x10,
  0x02, 0x04, 0x08, 0x10,
  0x20, 0x40, 0x80, 0x01,
  0x20, 0x40, 0x80, 0x01
};

/**
  Internal function to retrieve the base address of local APIC.

  This function will ASSERT if:
  The local APIC is not globally enabled.
  The local APIC is not working under XAPIC mode.
  The local APIC is not software enabled.

  @return The base address of local APIC

**/
UINTN
EFIAPI
InternalX86GetApicBase (
  VOID
  )
{
  UINTN  MsrValue;
  UINTN  ApicBase;

  MsrValue = (UINTN)AsmReadMsr64 (27);
  ApicBase = MsrValue & 0xffffff000ULL;

  //
  // Check the APIC Global Enable bit (bit 11) in IA32_APIC_BASE MSR.
  // This bit will be 1, if local APIC is globally enabled.
  //
  ASSERT ((MsrValue & BIT11) != 0);

  //
  // Check the APIC Extended Mode bit (bit 10) in IA32_APIC_BASE MSR.
  // This bit will be 0, if local APIC is under XAPIC mode.
  //
  ASSERT ((MsrValue & BIT10) == 0);

  //
  // Check the APIC Software Enable/Disable bit (bit 8) in Spurious-Interrupt
  // Vector Register.
  // This bit will be 1, if local APIC is software enabled.
  //
  ASSERT ((MmioRead32 (ApicBase + APIC_SVR) & BIT8) != 0);

  return ApicBase;
}

/**
  Internal function to return the frequency of the local APIC timer.

  @param  ApicBase  The base address of memory mapped registers of local APIC.

  @return The frequency of the timer in Hz.

**/
UINT32
EFIAPI
InternalX86GetTimerFrequency (
  IN      UINTN  ApicBase
  )
{
  return
    PcdGet32 (PcdFSBClock) /
    mTimerLibLocalApicDivisor[MmioBitFieldRead32 (ApicBase + APIC_TDCR, 0, 3)];
}

/**
  Internal function to read the current tick counter of local APIC.

  @param  ApicBase  The base address of memory mapped registers of local APIC.

  @return The tick counter read.

**/
INT32
EFIAPI
InternalX86GetTimerTick (
  IN      UINTN  ApicBase
  )
{
  return MmioRead32 (ApicBase + APIC_TMCCT);
}

/**
  Internal function to read the initial timer count of local APIC.

  @param  ApicBase  The base address of memory mapped registers of local APIC.

  @return The initial timer count read.

**/
UINT32
InternalX86GetInitTimerCount (
  IN      UINTN  ApicBase
  )
{
  return MmioRead32 (ApicBase + APIC_TMICT);
}

/**
  Stalls the CPU for at least the given number of ticks.

  Stalls the CPU for at least the given number of ticks. It's invoked by
  MicroSecondDelay() and NanoSecondDelay().

  This function will ASSERT if the APIC timer intial count returned from
  InternalX86GetInitTimerCount() is zero.

  @param  ApicBase  The base address of memory mapped registers of local APIC.
  @param  Delay     A period of time to delay in ticks.

**/
VOID
EFIAPI
InternalX86Delay (
  IN      UINTN   ApicBase,
  IN      UINT32  Delay
  )
{
  INT32   Ticks;
  UINT32  Times;
  UINT32  InitCount;
  UINT32  StartTick;

  //
  // In case Delay is too larger, separate it into several small delay slot.
  // Devided Delay by half value of Init Count is to avoid Delay close to
  // the Init Count, timeout maybe missing if the time consuming between 2
  // GetApicTimerCurrentCount() invoking is larger than the time gap between
  // Delay and the Init Count.
  //
  InitCount = InternalX86GetInitTimerCount (ApicBase);
  ASSERT (InitCount != 0);
  Times = Delay / (InitCount / 2);
  Delay = Delay % (InitCount / 2);

  //
  // Get Start Tick and do delay
  //
  StartTick = InternalX86GetTimerTick (ApicBase);
  do {
    //
    // Wait until time out by Delay value
    //
    do {
      CpuPause ();
      //
      // Get Ticks from Start to Current.
      //
      Ticks = StartTick - InternalX86GetTimerTick (ApicBase);
      //
      // Ticks < 0 means Timer wrap-arounds happens.
      //
      if (Ticks < 0) {
        Ticks += InitCount;
      }
    } while ((UINT32)Ticks < Delay);

    //
    // Update StartTick and Delay for next delay slot
    //
    StartTick -= (StartTick > Delay) ?  Delay : (Delay - InitCount);
    Delay      = InitCount / 2;
  } while (Times-- > 0);
}

/**
  Stalls the CPU for at least the given number of microseconds.

  Stalls the CPU for the number of microseconds specified by MicroSeconds.

  @param  MicroSeconds  The minimum number of microseconds to delay.

  @return The value of MicroSeconds inputted.

**/
UINTN
EFIAPI
MicroSecondDelay (
  IN      UINTN  MicroSeconds
  )
{
  UINTN  ApicBase;

  ApicBase = InternalX86GetApicBase ();
  InternalX86Delay (
    ApicBase,
    (UINT32)DivU64x32 (
              MultU64x64 (
                InternalX86GetTimerFrequency (ApicBase),
                MicroSeconds
                ),
              1000000u
              )
    );
  return MicroSeconds;
}

/**
  Stalls the CPU for at least the given number of nanoseconds.

  Stalls the CPU for the number of nanoseconds specified by NanoSeconds.

  @param  NanoSeconds The minimum number of nanoseconds to delay.

  @return The value of NanoSeconds inputted.

**/
UINTN
EFIAPI
NanoSecondDelay (
  IN      UINTN  NanoSeconds
  )
{
  UINTN  ApicBase;

  ApicBase = InternalX86GetApicBase ();
  InternalX86Delay (
    ApicBase,
    (UINT32)DivU64x32 (
              MultU64x64 (
                InternalX86GetTimerFrequency (ApicBase),
                NanoSeconds
                ),
              1000000000u
              )
    );
  return NanoSeconds;
}

/**
  Retrieves the current value of a 64-bit free running performance counter.

  The counter can either count up by 1 or count down by 1. If the physical
  performance counter counts by a larger increment, then the counter values
  must be translated. The properties of the counter can be retrieved from
  GetPerformanceCounterProperties().

  @return The current value of the free running performance counter.

**/
UINT64
EFIAPI
GetPerformanceCounter (
  VOID
  )
{
  return (UINT64)(UINT32)InternalX86GetTimerTick (InternalX86GetApicBase ());
}

/**
  Retrieves the 64-bit frequency in Hz and the range of performance counter
  values.

  If StartValue is not NULL, then the value that the performance counter starts
  with immediately after is it rolls over is returned in StartValue. If
  EndValue is not NULL, then the value that the performance counter end with
  immediately before it rolls over is returned in EndValue. The 64-bit
  frequency of the performance counter in Hz is always returned. If StartValue
  is less than EndValue, then the performance counter counts up. If StartValue
  is greater than EndValue, then the performance counter counts down. For
  example, a 64-bit free running counter that counts up would have a StartValue
  of 0 and an EndValue of 0xFFFFFFFFFFFFFFFF. A 24-bit free running counter
  that counts down would have a StartValue of 0xFFFFFF and an EndValue of 0.

  @param  StartValue  The value the performance counter starts with when it
                      rolls over.
  @param  EndValue    The value that the performance counter ends with before
                      it rolls over.

  @return The frequency in Hz.

**/
UINT64
EFIAPI
GetPerformanceCounterProperties (
  OUT      UINT64  *StartValue   OPTIONAL,
  OUT      UINT64  *EndValue     OPTIONAL
  )
{
  UINTN  ApicBase;

  ApicBase = InternalX86GetApicBase ();

  if (StartValue != NULL) {
    *StartValue = (UINT64)InternalX86GetInitTimerCount (ApicBase);
  }

  if (EndValue != NULL) {
    *EndValue = 0;
  }

  return (UINT64)InternalX86GetTimerFrequency (ApicBase);
}

/**
  Converts elapsed ticks of performance counter to time in nanoseconds.

  This function converts the elapsed ticks of running performance counter to
  time value in unit of nanoseconds.

  @param  Ticks     The number of elapsed ticks of running performance counter.

  @return The elapsed time in nanoseconds.

**/
UINT64
EFIAPI
GetTimeInNanoSecond (
  IN      UINT64  Ticks
  )
{
  UINT64  Frequency;
  UINT64  NanoSeconds;
  UINT64  Remainder;
  INTN    Shift;

  Frequency = GetPerformanceCounterProperties (NULL, NULL);

  //
  //          Ticks
  // Time = --------- x 1,000,000,000
  //        Frequency
  //
  NanoSeconds = MultU64x32 (DivU64x64Remainder (Ticks, Frequency, &Remainder), 1000000000u);

  //
  // Ensure (Remainder * 1,000,000,000) will not overflow 64-bit.
  // Since 2^29 < 1,000,000,000 = 0x3B9ACA00 < 2^30, Remainder should < 2^(64-30) = 2^34,
  // i.e. highest bit set in Remainder should <= 33.
  //
  Shift        = MAX (0, HighBitSet64 (Remainder) - 33);
  Remainder    = RShiftU64 (Remainder, (UINTN)Shift);
  Frequency    = RShiftU64 (Frequency, (UINTN)Shift);
  NanoSeconds += DivU64x64Remainder (MultU64x32 (Remainder, 1000000000u), Frequency, NULL);

  return NanoSeconds;
}
