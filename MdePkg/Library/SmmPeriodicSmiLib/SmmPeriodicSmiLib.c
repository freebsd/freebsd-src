/** @file
  SMM Periodic SMI Library.

  Copyright (c) 2011 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiSmm.h>

#include <Protocol/SmmPeriodicTimerDispatch2.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SmmServicesTableLib.h>

#include <Library/SmmPeriodicSmiLib.h>

///
/// Define the number of periodic SMI handler entries that should be allocated to the list
/// of free periodic SMI handlers when the list of free periodic SMI handlers is empty.
///
#define PERIODIC_SMI_LIBRARY_ALLOCATE_SIZE  0x08

///
/// Signature for a PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT structure
///
#define PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_SIGNATURE  SIGNATURE_32 ('P', 'S', 'M', 'I')

///
/// Structure that contains state information for an enabled periodic SMI handler
///
typedef struct {
  ///
  /// Signature value that must be set to PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_SIGNATURE
  ///
  UINT32                                     Signature;
  ///
  /// The link entry to be inserted to the list of periodic SMI handlers.
  ///
  LIST_ENTRY                                 Link;
  ///
  /// The dispatch function to called to invoke an enabled periodic SMI handler.
  ///
  PERIODIC_SMI_LIBRARY_HANDLER               DispatchFunction;
  ///
  /// The context to pass into DispatchFunction
  ///
  VOID                                       *Context;
  ///
  /// The tick period in 100 ns units that DispatchFunction should be called.
  ///
  UINT64                                     TickPeriod;
  ///
  /// The Cpu number that is required to execute DispatchFunction.  If Cpu is
  /// set to PERIODIC_SMI_LIBRARY_ANY_CPU, then DispatchFunction may be executed
  /// on any CPU.
  ///
  UINTN                                      Cpu;
  ///
  /// The size, in bytes, of the stack allocated for a periodic SMI handler.
  /// This value must be a multiple of EFI_PAGE_SIZE.
  ///
  UINTN                                      StackSize;
  ///
  /// A pointer to the stack allocated using AllocatePages().  This field will
  /// be NULL if StackSize is 0.
  ///
  VOID                                       *Stack;
  ///
  /// Spin lock used to wait for an AP to complete the execution of a periodic SMI handler
  ///
  SPIN_LOCK                                  DispatchLock;
  ///
  /// The rate in Hz of the performance counter that is used to measure the
  /// amount of time that a periodic SMI handler executes.
  ///
  UINT64                                     PerfomanceCounterRate;
  ///
  /// The start count value of the performance counter that is used to measure
  /// the amount of time that a periodic SMI handler executes.
  ///
  UINT64                                     PerfomanceCounterStartValue;
  ///
  /// The end count value of the performance counter that is used to measure
  /// the amount of time that a periodic SMI handler executes.
  ///
  UINT64                                     PerfomanceCounterEndValue;
  ///
  /// The context record passed into the Register() function of the SMM Periodic
  /// Timer Dispatch Protocol when a periodic SMI handler is enabled.
  ///
  EFI_SMM_PERIODIC_TIMER_REGISTER_CONTEXT    RegisterContext;
  ///
  /// The handle returned from the Register() function of the SMM Periodic
  /// Timer Dispatch Protocol when a periodic SMI handler is enabled.
  ///
  EFI_HANDLE                                 DispatchHandle;
  ///
  /// The total number of performance counter ticks that the periodic SMI handler
  /// has been executing in its current invocation.
  ///
  UINT64                                     DispatchTotalTime;
  ///
  /// The performance counter value that was captured the last time that the
  /// periodic SMI handler called PeriodicSmiExecutionTime().  This allows the
  /// time value returned by PeriodicSmiExecutionTime() to be accurate even when
  /// the performance counter rolls over.
  ///
  UINT64                                     DispatchCheckPointTime;
  ///
  /// Buffer used to save the context when control is transfer from this library
  /// to an enabled periodic SMI handler.  This saved context is used when the
  /// periodic SMI handler exits or yields.
  ///
  BASE_LIBRARY_JUMP_BUFFER                   DispatchJumpBuffer;
  ///
  /// Flag that is set to TRUE when a periodic SMI handler requests to yield
  /// using PeriodicSmiYield().  When this flag IS TRUE, YieldJumpBuffer is
  /// valid.  When this flag is FALSE, YieldJumpBuffer is not valid.
  ///
  BOOLEAN                                    YieldFlag;
  ///
  /// Buffer used to save the context when a periodic SMI handler requests to
  /// yield using PeriodicSmiYield().  This context is used to resume the
  /// execution of a periodic SMI handler the next time control is transferred
  /// to the periodic SMI handler that yielded.
  ///
  BASE_LIBRARY_JUMP_BUFFER                   YieldJumpBuffer;
  ///
  /// The amount of time, in 100 ns units, that have elapsed since the last
  /// time the periodic SMI handler was invoked.
  ///
  UINT64                                     ElapsedTime;
} PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT;

/**
 Macro that returns a pointer to a PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT
 structure based on a pointer to a Link field.

**/
#define PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_FROM_LINK(a)             \
  CR (                                                                \
    a,                                                                \
    PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT,                             \
    Link,                                                             \
    PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_SIGNATURE                    \
    )

///
/// Pointer to the SMM Periodic Timer Dispatch Protocol that was located in the constructor.
///
EFI_SMM_PERIODIC_TIMER_DISPATCH2_PROTOCOL  *gSmmPeriodicTimerDispatch2 = NULL;

///
/// Pointer to a table of supported periodic SMI tick periods in 100 ns units
/// sorted from largest to smallest terminated by a tick period value of 0.
/// This table is allocated using AllocatePool() in the constructor and filled
/// in based on the values returned from the SMM Periodic Timer Dispatch 2 Protocol
/// function GetNextShorterInterval().
///
UINT64  *gSmiTickPeriodTable = NULL;

///
/// Linked list of free periodic SMI handlers that this library can use.
///
LIST_ENTRY  gFreePeriodicSmiLibraryHandlers =
  INITIALIZE_LIST_HEAD_VARIABLE (gFreePeriodicSmiLibraryHandlers);

///
/// Linked list of periodic SMI handlers that this library is currently managing.
///
LIST_ENTRY  gPeriodicSmiLibraryHandlers =
  INITIALIZE_LIST_HEAD_VARIABLE (gPeriodicSmiLibraryHandlers);

///
/// Pointer to the periodic SMI handler that is currently being executed.
/// Is set to NULL if no periodic SMI handler is currently being executed.
///
PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *gActivePeriodicSmiLibraryHandler = NULL;

/**
  Internal worker function that returns a pointer to the
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT structure associated with the periodic
  SMI handler that is currently being executed.  If a periodic SMI handler is
  not currently being executed, the NULL is returned.

  @retval  NULL   A periodic SMI handler is not currently being executed.
  @retval  other  Pointer to the PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT
                  associated with the active periodic SMI handler.

**/
PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *
GetActivePeriodicSmiLibraryHandler (
  VOID
  )
{
  return gActivePeriodicSmiLibraryHandler;
}

/**
  Internal worker function that returns a pointer to the
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT structure associated with the
  DispatchHandle that was returned when the periodic SMI handler was enabled
  with PeriodicSmiEnable().  If DispatchHandle is NULL, then the active
  periodic SMI handler is returned.  If DispatchHandle is NULL and there is
  no active periodic SMI handler, then NULL is returned.

  @param[in] DispatchHandle  DispatchHandle that was returned when the periodic
                             SMI handler was enabled with PeriodicSmiEnable().
                             This is an optional parameter that may be NULL.
                             If this parameter is NULL, then the active periodic
                             SMI handler is returned.

  @retval  NULL   DispatchHandle is NULL and there is no active periodic SMI
                  handler.
  @retval  NULL   DispatchHandle does not match any of the periodic SMI handlers
                  that have been enabled with PeriodicSmiEnable().
  @retval  other  Pointer to the PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT
                  associated with the DispatchHandle.

**/
PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *
LookupPeriodicSmiLibraryHandler (
  IN EFI_HANDLE  DispatchHandle    OPTIONAL
  )
{
  LIST_ENTRY                            *Link;
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // If DispatchHandle is NULL, then return the active periodic SMI handler
  //
  if (DispatchHandle == NULL) {
    return GetActivePeriodicSmiLibraryHandler ();
  }

  //
  // Search the periodic SMI handler entries for a a matching DispatchHandle
  //
  for ( Link = GetFirstNode (&gPeriodicSmiLibraryHandlers)
        ; !IsNull (&gPeriodicSmiLibraryHandlers, Link)
        ; Link = GetNextNode (&gPeriodicSmiLibraryHandlers, Link)
        )
  {
    PeriodicSmiLibraryHandler = PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_FROM_LINK (Link);

    if (PeriodicSmiLibraryHandler->DispatchHandle == DispatchHandle) {
      return PeriodicSmiLibraryHandler;
    }
  }

  //
  // No entries match DispatchHandle, so return NULL
  //
  return NULL;
}

/**
  Internal worker function that sets that active periodic SMI handler based on
  the DispatchHandle that was returned when the periodic SMI handler was enabled
  with PeriodicSmiEnable(). If DispatchHandle is NULL, then the
  state is updated to show that there is not active periodic SMI handler.
  A pointer to the active PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT structure
  is returned.

  @param [in] DispatchHandle DispatchHandle that was returned when the periodic
                             SMI handler was enabled with PeriodicSmiEnable().
                             This is an optional parameter that may be NULL.
                             If this parameter is NULL, then the state is updated
                             to show that there is not active periodic SMI handler.
  @retval  NULL   DispatchHandle is NULL.
  @retval  other  Pointer to the PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT
                  associated with DispatchHandle.

**/
PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *
SetActivePeriodicSmiLibraryHandler (
  IN EFI_HANDLE  DispatchHandle    OPTIONAL
  )
{
  if (DispatchHandle == NULL) {
    gActivePeriodicSmiLibraryHandler = NULL;
  } else {
    gActivePeriodicSmiLibraryHandler = LookupPeriodicSmiLibraryHandler (DispatchHandle);
  }

  return gActivePeriodicSmiLibraryHandler;
}

/**
  Internal worker function that moves the specified periodic SMI handler from the
  list of managed periodic SMI handlers to the list of free periodic SMI handlers.

  @param[in] PeriodicSmiLibraryHandler  Pointer to the periodic SMI handler to be reclaimed.
**/
VOID
ReclaimPeriodicSmiLibraryHandler (
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler
  )
{
  ASSERT (PeriodicSmiLibraryHandler->DispatchHandle == NULL);
  if (PeriodicSmiLibraryHandler->Stack != NULL) {
    FreePages (
      PeriodicSmiLibraryHandler->Stack,
      EFI_SIZE_TO_PAGES (PeriodicSmiLibraryHandler->StackSize)
      );
    PeriodicSmiLibraryHandler->Stack = NULL;
  }

  RemoveEntryList (&PeriodicSmiLibraryHandler->Link);
  InsertHeadList (&gFreePeriodicSmiLibraryHandlers, &PeriodicSmiLibraryHandler->Link);
}

/**
  Add the additional entries to the list of free periodic SMI handlers.
  The function is assumed to be called only when the list of free periodic SMI
  handlers is empty.

  @retval TRUE  The additional entries were added.
  @retval FALSE There was no available resource for the additional entries.
**/
BOOLEAN
EnlargeFreePeriodicSmiLibraryHandlerList (
  VOID
  )
{
  UINTN                                 Index;
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // Add the entries to the list
  //
  for (Index = 0; Index < PERIODIC_SMI_LIBRARY_ALLOCATE_SIZE; Index++) {
    PeriodicSmiLibraryHandler = AllocatePool (sizeof (PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT));
    if (PeriodicSmiLibraryHandler == NULL) {
      break;
    }

    PeriodicSmiLibraryHandler->Signature = PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_SIGNATURE;
    InsertHeadList (&gFreePeriodicSmiLibraryHandlers, &PeriodicSmiLibraryHandler->Link);
  }

  return (BOOLEAN)(Index > 0);
}

/**
  Internal worker function that returns a free entry for a new periodic
  SMI handler.  If no free entries are available, then additional
  entries are allocated.

  @retval  NULL   There are not enough resources available to to allocate a free entry.
  @retval  other  Pointer to a free PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT structure.

**/
PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *
FindFreePeriodicSmiLibraryHandler (
  VOID
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  if (IsListEmpty (&gFreePeriodicSmiLibraryHandlers)) {
    if (!EnlargeFreePeriodicSmiLibraryHandlerList ()) {
      return NULL;
    }
  }

  //
  // Get one from the list of free periodic SMI handlers.
  //
  PeriodicSmiLibraryHandler = PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_FROM_LINK (
                                GetFirstNode (&gFreePeriodicSmiLibraryHandlers)
                                );
  RemoveEntryList (&PeriodicSmiLibraryHandler->Link);
  InsertTailList (&gPeriodicSmiLibraryHandlers, &PeriodicSmiLibraryHandler->Link);

  return PeriodicSmiLibraryHandler;
}

/**
  This function returns a pointer to a table of supported periodic
  SMI tick periods in 100 ns units sorted from largest to smallest.
  The table contains a array of UINT64 values terminated by a tick
  period value of 0.  The returned table must be treated as read-only
  data and must not be freed.

  @return  A pointer to a table of UINT64 tick period values in
           100ns units sorted from largest to smallest terminated
           by a tick period of 0.

**/
UINT64 *
EFIAPI
PeriodicSmiSupportedTickPeriod (
  VOID
  )
{
  //
  // Return the table allocated and populated by SmmPeriodicSmiLibConstructor()
  //
  return gSmiTickPeriodTable;
}

/**
  This function returns the time in 100ns units since the periodic SMI
  handler function was called.  If the periodic SMI handler was resumed
  through PeriodicSmiYield(), then the time returned is the time in
  100ns units since PeriodicSmiYield() returned.

  @return  The actual time in 100ns units that the periodic SMI handler
           has been executing.  If this function is not called from within
           an enabled periodic SMI handler, then 0 is returned.

**/
UINT64
EFIAPI
PeriodicSmiExecutionTime (
  VOID
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;
  UINT64                                Current;
  UINT64                                Count;

  //
  // If there is no active periodic SMI handler, then return 0
  //
  PeriodicSmiLibraryHandler = GetActivePeriodicSmiLibraryHandler ();
  if (PeriodicSmiLibraryHandler == NULL) {
    return 0;
  }

  //
  // Get the current performance counter value
  //
  Current = GetPerformanceCounter ();

  //
  // Count the number of performance counter ticks since the periodic SMI handler
  // was dispatched or the last time this function was called.
  //
  if (PeriodicSmiLibraryHandler->PerfomanceCounterEndValue > PeriodicSmiLibraryHandler->PerfomanceCounterStartValue) {
    //
    // The performance counter counts up.  Check for roll over condition.
    //
    if (Current > PeriodicSmiLibraryHandler->DispatchCheckPointTime) {
      Count = Current - PeriodicSmiLibraryHandler->DispatchCheckPointTime;
    } else {
      Count = (Current - PeriodicSmiLibraryHandler->PerfomanceCounterStartValue) + (PeriodicSmiLibraryHandler->PerfomanceCounterEndValue - PeriodicSmiLibraryHandler->DispatchCheckPointTime);
    }
  } else {
    //
    // The performance counter counts down.  Check for roll over condition.
    //
    if (PeriodicSmiLibraryHandler->DispatchCheckPointTime > Current) {
      Count = PeriodicSmiLibraryHandler->DispatchCheckPointTime - Current;
    } else {
      Count = (PeriodicSmiLibraryHandler->DispatchCheckPointTime - PeriodicSmiLibraryHandler->PerfomanceCounterEndValue) + (PeriodicSmiLibraryHandler->PerfomanceCounterStartValue - Current);
    }
  }

  //
  // Accumulate the total number of performance counter ticks since the periodic
  // SMI handler was dispatched or resumed.
  //
  PeriodicSmiLibraryHandler->DispatchTotalTime += Count;

  //
  // Update the checkpoint value to the current performance counter value
  //
  PeriodicSmiLibraryHandler->DispatchCheckPointTime = Current;

  //
  // Convert the total number of performance counter ticks to 100 ns units
  //
  return DivU64x64Remainder (
           MultU64x32 (PeriodicSmiLibraryHandler->DispatchTotalTime, 10000000),
           PeriodicSmiLibraryHandler->PerfomanceCounterRate,
           NULL
           );
}

/**
  This function returns control back to the SMM Foundation.  When the next
  periodic SMI for the currently executing handler is triggered, the periodic
  SMI handler will restarted from its registered DispatchFunction entry point.
  If this function is not called from within an enabled periodic SMI handler,
  then control is returned to the calling function.

**/
VOID
EFIAPI
PeriodicSmiExit (
  VOID
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // If there is no active periodic SMI handler, then return
  //
  PeriodicSmiLibraryHandler = GetActivePeriodicSmiLibraryHandler ();
  if (PeriodicSmiLibraryHandler == NULL) {
    return;
  }

  //
  // Perform a long jump back to the point when the currently executing dispatch
  // function was dispatched.
  //
  LongJump (&PeriodicSmiLibraryHandler->DispatchJumpBuffer, 1);

  //
  // Must never return
  //
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  This function yields control back to the SMM Foundation.  When the next
  periodic SMI for the currently executing handler is triggered, the periodic
  SMI handler will be resumed and this function will return.  Use of this
  function requires a separate stack for the periodic SMI handler.  A non zero
  stack size must be specified in PeriodicSmiEnable() for this function to be
  used.

  If the stack size passed into PeriodicSmiEnable() was zero, the 0 is returned.

  If this function is not called from within an enabled periodic SMI handler,
  then 0 is returned.

  @return  The actual time in 100ns units elapsed since this function was
           called.  A value of 0 indicates an unknown amount of time.

**/
UINT64
EFIAPI
PeriodicSmiYield (
  VOID
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;
  UINTN                                 SetJumpFlag;

  //
  // If there is no active periodic SMI handler, then return
  //
  PeriodicSmiLibraryHandler = GetActivePeriodicSmiLibraryHandler ();
  if (PeriodicSmiLibraryHandler == NULL) {
    return 0;
  }

  //
  // If PeriodicSmiYield() is called without an allocated stack, then just return
  // immediately with an elapsed time of 0.
  //
  if (PeriodicSmiLibraryHandler->Stack == NULL) {
    return 0;
  }

  //
  // Set a flag so the next periodic SMI event will resume at where SetJump()
  // is called below.
  //
  PeriodicSmiLibraryHandler->YieldFlag = TRUE;

  //
  // Save context in YieldJumpBuffer
  //
  SetJumpFlag = SetJump (&PeriodicSmiLibraryHandler->YieldJumpBuffer);
  if (SetJumpFlag == 0) {
    //
    // The initial call to SetJump() always returns 0.
    // If this is the initial call, then exit the current periodic SMI handler
    //
    PeriodicSmiExit ();
  }

  //
  // We get here when a LongJump is performed from PeriodicSmiDispatchFunctionOnCpu()
  // to resume a periodic SMI handler that called PeriodicSmiYield() on the
  // previous time this periodic SMI handler was dispatched.
  //
  // Clear the flag so the next periodic SMI dispatch will not resume.
  //
  PeriodicSmiLibraryHandler->YieldFlag = FALSE;

  //
  // Return the amount elapsed time that occurred while yielded
  //
  return PeriodicSmiLibraryHandler->ElapsedTime;
}

/**
  Internal worker function that transfers control to an enabled periodic SMI
  handler.  If the enabled periodic SMI handler was allocated its own stack,
  then this function is called on that allocated stack through the BaseLin
  function SwitchStack().

  @param[in] Context1  Context1 parameter passed into SwitchStack().
  @param[in] Context2  Context2 parameter passed into SwitchStack().

**/
VOID
EFIAPI
PeriodicSmiDispatchFunctionSwitchStack (
  IN VOID  *Context1   OPTIONAL,
  IN VOID  *Context2   OPTIONAL
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // Convert Context1 to PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *
  //
  PeriodicSmiLibraryHandler = (PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT *)Context1;

  //
  // Dispatch the registered handler passing in the context that was registered
  // and the amount of time that has elapsed since the previous time this
  // periodic SMI handler was dispatched.
  //
  PeriodicSmiLibraryHandler->DispatchFunction (
                               PeriodicSmiLibraryHandler->Context,
                               PeriodicSmiLibraryHandler->ElapsedTime
                               );

  //
  // If this DispatchFunction() returns, then unconditionally call PeriodicSmiExit()
  // to perform a LongJump() back to PeriodicSmiDispatchFunctionOnCpu(). The
  // LongJump() will resume execution on the original stack.
  //
  PeriodicSmiExit ();
}

/**
  Internal worker function that transfers control to an enabled periodic SMI
  handler on the specified logical CPU.  This function determines if the periodic
  SMI handler yielded and needs to be resumed.  It also and switches to an
  allocated stack if one was allocated in PeriodicSmiEnable().

  @param[in] PeriodicSmiLibraryHandler  A pointer to the context for the periodic
                                        SMI handler to execute.

**/
VOID
EFIAPI
PeriodicSmiDispatchFunctionOnCpu (
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler
  )
{
  //
  // Save context in DispatchJumpBuffer.  The initial call to SetJump() always
  // returns 0.  If this is the initial call, then either resume from a prior
  // call to PeriodicSmiYield() or call the DispatchFunction registered in
  // PeriodicSmiEnable() using an allocated stack if one was specified.
  //
  if (SetJump (&PeriodicSmiLibraryHandler->DispatchJumpBuffer) != 0) {
    return;
  }

  //
  // Capture the performance counter value just before the periodic SMI handler
  // is resumed so the amount of time the periodic SMI handler executes can be
  // calculated.
  //
  PeriodicSmiLibraryHandler->DispatchTotalTime      = 0;
  PeriodicSmiLibraryHandler->DispatchCheckPointTime = GetPerformanceCounter ();

  if (PeriodicSmiLibraryHandler->YieldFlag) {
    //
    // Perform a long jump back to the point where the previously dispatched
    // function called PeriodicSmiYield().
    //
    LongJump (&PeriodicSmiLibraryHandler->YieldJumpBuffer, 1);
  } else if (PeriodicSmiLibraryHandler->Stack == NULL) {
    //
    // If Stack is NULL then call DispatchFunction using current stack passing
    // in the context that was registered and the amount of time that has
    // elapsed since the previous time this periodic SMI handler was dispatched.
    //
    PeriodicSmiLibraryHandler->DispatchFunction (
                                 PeriodicSmiLibraryHandler->Context,
                                 PeriodicSmiLibraryHandler->ElapsedTime
                                 );

    //
    // If this DispatchFunction() returns, then unconditionally call PeriodicSmiExit()
    // to perform a LongJump() back to this function.
    //
    PeriodicSmiExit ();
  } else {
    //
    // If Stack is not NULL then call DispatchFunction switching to the allocated stack
    //
    SwitchStack (
      PeriodicSmiDispatchFunctionSwitchStack,
      PeriodicSmiLibraryHandler,
      NULL,
      (UINT8 *)PeriodicSmiLibraryHandler->Stack + PeriodicSmiLibraryHandler->StackSize
      );
  }

  //
  // Must never return
  //
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  Internal worker function that transfers control to an enabled periodic SMI
  handler on the specified logical CPU.  This worker function is only called
  using the SMM Services Table function SmmStartupThisAp() to execute the
  periodic SMI handler on a logical CPU that is different than the one that is
  running the SMM Foundation.  When the periodic SMI handler returns, a lock is
  released to notify the CPU that is running the SMM Foundation that the periodic
  SMI handler execution has finished its execution.

  @param[in, out] Buffer  A pointer to the context for the periodic SMI handler.

**/
VOID
EFIAPI
PeriodicSmiDispatchFunctionWithLock (
  IN OUT VOID  *Buffer
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // Get context
  //
  PeriodicSmiLibraryHandler = (PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *)Buffer;

  //
  // Execute dispatch function on the currently executing logical CPU
  //
  PeriodicSmiDispatchFunctionOnCpu (PeriodicSmiLibraryHandler);

  //
  // Release the dispatch spin lock
  //
  ReleaseSpinLock (&PeriodicSmiLibraryHandler->DispatchLock);
}

/**
  Internal worker function that transfers control to a periodic SMI handler that
  was enabled using PeriodicSmiEnable().

  @param[in]     DispatchHandle  The unique handle assigned to this handler by
                                 SmiHandlerRegister().
  @param[in]     Context         Points to an optional handler context which was
                                 specified when the handler was registered.
  @param[in, out] CommBuffer     A pointer to a collection of data in memory that
                                 will be conveyed from a non-SMM environment into
                                 an SMM environment.
  @param[in, out] CommBufferSize The size of the CommBuffer.

  @retval EFI_SUCCESS                         The interrupt was handled and quiesced.
                                              No other handlers should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_QUIESCED  The interrupt has been quiesced but other
                                              handlers should still be called.
  @retval EFI_WARN_INTERRUPT_SOURCE_PENDING   The interrupt is still pending and other
                                              handlers should still be called.
  @retval EFI_INTERRUPT_PENDING               The interrupt could not be quiesced.

**/
EFI_STATUS
EFIAPI
PeriodicSmiDispatchFunction (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;
  EFI_SMM_PERIODIC_TIMER_CONTEXT        *TimerContext;
  EFI_STATUS                            Status;

  //
  // Set the active periodic SMI handler
  //
  PeriodicSmiLibraryHandler = SetActivePeriodicSmiLibraryHandler (DispatchHandle);
  if (PeriodicSmiLibraryHandler == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  // Retrieve the elapsed time since the last time this periodic SMI handler was called
  //
  PeriodicSmiLibraryHandler->ElapsedTime = 0;
  if (CommBuffer != NULL) {
    TimerContext                           = (EFI_SMM_PERIODIC_TIMER_CONTEXT  *)CommBuffer;
    PeriodicSmiLibraryHandler->ElapsedTime = TimerContext->ElapsedTime;
  }

  //
  // Dispatch the periodic SMI handler
  //
  if ((PeriodicSmiLibraryHandler->Cpu == PERIODIC_SMI_LIBRARY_ANY_CPU) ||
      (PeriodicSmiLibraryHandler->Cpu == gSmst->CurrentlyExecutingCpu))
  {
    //
    // Dispatch on the currently execution CPU if the CPU specified in PeriodicSmiEnable()
    // was PERIODIC_SMI_LIBRARY_ANY_CPU or the currently executing CPU matches the CPU
    // that was specified in PeriodicSmiEnable().
    //
    PeriodicSmiDispatchFunctionOnCpu (PeriodicSmiLibraryHandler);
  } else {
    //
    // Acquire spin lock for ths periodic SMI handler.  The AP will release the
    // spin lock when it is done executing the periodic SMI handler.
    //
    AcquireSpinLock (&PeriodicSmiLibraryHandler->DispatchLock);

    //
    // Execute the periodic SMI handler on the CPU that was specified in
    // PeriodicSmiEnable().
    //
    Status = gSmst->SmmStartupThisAp (
                      PeriodicSmiDispatchFunctionWithLock,
                      PeriodicSmiLibraryHandler->Cpu,
                      PeriodicSmiLibraryHandler
                      );
    if (!EFI_ERROR (Status)) {
      //
      // Wait for the AP to release the spin lock.
      //
      while (!AcquireSpinLockOrFail (&PeriodicSmiLibraryHandler->DispatchLock)) {
        CpuPause ();
      }
    }

    //
    // Release the spin lock for the periodic SMI handler.
    //
    ReleaseSpinLock (&PeriodicSmiLibraryHandler->DispatchLock);
  }

  //
  // Reclaim the active periodic SMI handler if it was disabled during the current dispatch.
  //
  if (PeriodicSmiLibraryHandler->DispatchHandle == NULL) {
    ReclaimPeriodicSmiLibraryHandler (PeriodicSmiLibraryHandler);
  }

  //
  // Update state to show that there is no active periodic SMI handler
  //
  SetActivePeriodicSmiLibraryHandler (NULL);

  return EFI_SUCCESS;
}

/**
  This function enables a periodic SMI handler.

  @param[in, out] DispatchHandle   A pointer to the handle associated with the
                                   enabled periodic SMI handler.  This is an
                                   optional parameter that may be NULL.  If it is
                                   NULL, then the handle will not be returned,
                                   which means that the periodic SMI handler can
                                   never be disabled.
  @param[in]     DispatchFunction  A pointer to a periodic SMI handler function.
  @param[in]     Context           Optional content to pass into DispatchFunction.
  @param[in]     TickPeriod        The requested tick period in 100ns units that
                                   control should be given to the periodic SMI
                                   handler.  Must be one of the supported values
                                   returned by PeriodicSmiSupportedPickPeriod().
  @param[in]     Cpu               Specifies the CPU that is required to execute
                                   the periodic SMI handler.  If Cpu is
                                   PERIODIC_SMI_LIBRARY_ANY_CPU, then the periodic
                                   SMI handler will always be executed on the SMST
                                   CurrentlyExecutingCpu, which may vary across
                                   periodic SMIs.  If Cpu is between 0 and the SMST
                                   NumberOfCpus, then the periodic SMI will always
                                   be executed on the requested CPU.
  @param[in]     StackSize         The size, in bytes, of the stack to allocate for
                                   use by the periodic SMI handler.  If 0, then the
                                   default stack will be used.

  @retval EFI_INVALID_PARAMETER  DispatchFunction is NULL.
  @retval EFI_UNSUPPORTED        TickPeriod is not a supported tick period.  The
                                 supported tick periods can be retrieved using
                                 PeriodicSmiSupportedTickPeriod().
  @retval EFI_INVALID_PARAMETER  Cpu is not PERIODIC_SMI_LIBRARY_ANY_CPU or in
                                 the range 0 to SMST NumberOfCpus.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources to enable the
                                 periodic SMI handler.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources to allocate the
                                 stack specified by StackSize.
  @retval EFI_SUCCESS            The periodic SMI handler was enabled.

**/
EFI_STATUS
EFIAPI
PeriodicSmiEnable (
  IN OUT EFI_HANDLE                    *DispatchHandle     OPTIONAL,
  IN     PERIODIC_SMI_LIBRARY_HANDLER  DispatchFunction,
  IN     CONST VOID                    *Context            OPTIONAL,
  IN     UINT64                        TickPeriod,
  IN     UINTN                         Cpu,
  IN     UINTN                         StackSize
  )
{
  EFI_STATUS                            Status;
  UINTN                                 Index;
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // Make sure all the input parameters are valid
  //
  if (DispatchFunction == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; gSmiTickPeriodTable[Index] != 0; Index++) {
    if (gSmiTickPeriodTable[Index] == TickPeriod) {
      break;
    }
  }

  if (gSmiTickPeriodTable[Index] == 0) {
    return EFI_UNSUPPORTED;
  }

  if ((Cpu != PERIODIC_SMI_LIBRARY_ANY_CPU) && (Cpu >= gSmst->NumberOfCpus)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find a free periodic SMI handler entry
  //
  PeriodicSmiLibraryHandler = FindFreePeriodicSmiLibraryHandler ();
  if (PeriodicSmiLibraryHandler == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize a new periodic SMI handler entry
  //
  PeriodicSmiLibraryHandler->YieldFlag        = FALSE;
  PeriodicSmiLibraryHandler->DispatchHandle   = NULL;
  PeriodicSmiLibraryHandler->DispatchFunction = DispatchFunction;
  PeriodicSmiLibraryHandler->Context          = (VOID *)Context;
  PeriodicSmiLibraryHandler->Cpu              = Cpu;
  PeriodicSmiLibraryHandler->StackSize        = ALIGN_VALUE (StackSize, EFI_PAGE_SIZE);
  if (PeriodicSmiLibraryHandler->StackSize > 0) {
    PeriodicSmiLibraryHandler->Stack = AllocatePages (EFI_SIZE_TO_PAGES (PeriodicSmiLibraryHandler->StackSize));
    if (PeriodicSmiLibraryHandler->Stack == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem (PeriodicSmiLibraryHandler->Stack, PeriodicSmiLibraryHandler->StackSize);
  } else {
    PeriodicSmiLibraryHandler->Stack = NULL;
  }

  InitializeSpinLock (&PeriodicSmiLibraryHandler->DispatchLock);
  PeriodicSmiLibraryHandler->PerfomanceCounterRate = GetPerformanceCounterProperties (
                                                       &PeriodicSmiLibraryHandler->PerfomanceCounterStartValue,
                                                       &PeriodicSmiLibraryHandler->PerfomanceCounterEndValue
                                                       );
  PeriodicSmiLibraryHandler->RegisterContext.Period          = TickPeriod;
  PeriodicSmiLibraryHandler->RegisterContext.SmiTickInterval = TickPeriod;
  Status                                                     = gSmmPeriodicTimerDispatch2->Register (
                                                                                             gSmmPeriodicTimerDispatch2,
                                                                                             PeriodicSmiDispatchFunction,
                                                                                             &PeriodicSmiLibraryHandler->RegisterContext,
                                                                                             &PeriodicSmiLibraryHandler->DispatchHandle
                                                                                             );
  if (EFI_ERROR (Status)) {
    PeriodicSmiLibraryHandler->DispatchHandle = NULL;
    ReclaimPeriodicSmiLibraryHandler (PeriodicSmiLibraryHandler);
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Return the registered handle if the optional DispatchHandle parameter is not NULL
  //
  if (DispatchHandle != NULL) {
    *DispatchHandle = PeriodicSmiLibraryHandler->DispatchHandle;
  }

  return EFI_SUCCESS;
}

/**
  This function disables a periodic SMI handler that has been previously
  enabled with PeriodicSmiEnable().

  @param[in] DispatchHandle  A handle associated with a previously enabled periodic
                             SMI handler.  This is an optional parameter that may
                             be NULL.  If it is NULL, then the active periodic SMI
                             handlers is disabled.

  @retval FALSE  DispatchHandle is NULL and there is no active periodic SMI handler.
  @retval FALSE  The periodic SMI handler specified by DispatchHandle has
                 not been enabled with PeriodicSmiEnable().
  @retval TRUE   The periodic SMI handler specified by DispatchHandle has
                 been disabled.  If DispatchHandle is NULL, then the active
                 periodic SMI handler has been disabled.

**/
BOOLEAN
EFIAPI
PeriodicSmiDisable (
  IN EFI_HANDLE  DispatchHandle    OPTIONAL
  )
{
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;
  EFI_STATUS                            Status;

  //
  // Lookup the periodic SMI handler specified by DispatchHandle
  //
  PeriodicSmiLibraryHandler = LookupPeriodicSmiLibraryHandler (DispatchHandle);
  if (PeriodicSmiLibraryHandler == NULL) {
    return FALSE;
  }

  //
  // Unregister the periodic SMI handler from the SMM Periodic Timer Dispatch 2 Protocol
  //
  Status = gSmmPeriodicTimerDispatch2->UnRegister (
                                         gSmmPeriodicTimerDispatch2,
                                         PeriodicSmiLibraryHandler->DispatchHandle
                                         );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  //
  // Mark the entry for the disabled periodic SMI handler as free, and
  // call ReclaimPeriodicSmiLibraryHandler to move it to the list of free
  // periodic SMI handlers.
  //
  PeriodicSmiLibraryHandler->DispatchHandle = NULL;
  if (PeriodicSmiLibraryHandler != GetActivePeriodicSmiLibraryHandler ()) {
    ReclaimPeriodicSmiLibraryHandler (PeriodicSmiLibraryHandler);
  }

  return TRUE;
}

/**
  This constructor function caches the pointer to the SMM Periodic Timer
  Dispatch 2 Protocol and collects the list SMI tick rates that the hardware
  supports.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmPeriodicSmiLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT64      *SmiTickInterval;
  UINTN       Count;

  //
  // Locate the SMM Periodic Timer Dispatch 2 Protocol
  //
  Status = gSmst->SmmLocateProtocol (
                    &gEfiSmmPeriodicTimerDispatch2ProtocolGuid,
                    NULL,
                    (VOID **)&gSmmPeriodicTimerDispatch2
                    );
  ASSERT_EFI_ERROR (Status);
  ASSERT (gSmmPeriodicTimerDispatch2 != NULL);

  //
  // Count the number of periodic SMI tick intervals that the SMM Periodic Timer
  // Dispatch 2 Protocol supports.
  //
  SmiTickInterval = NULL;
  Count           = 0;
  do {
    Status = gSmmPeriodicTimerDispatch2->GetNextShorterInterval (
                                           gSmmPeriodicTimerDispatch2,
                                           &SmiTickInterval
                                           );
    Count++;
  } while (SmiTickInterval != NULL);

  //
  // Allocate a buffer for the table of supported periodic SMI tick periods.
  //
  gSmiTickPeriodTable = AllocateZeroPool (Count * sizeof (UINT64));
  ASSERT (gSmiTickPeriodTable != NULL);

  //
  // Fill in the table of supported periodic SMI tick periods.
  //
  SmiTickInterval = NULL;
  Count           = 0;
  do {
    gSmiTickPeriodTable[Count] = 0;
    Status                     = gSmmPeriodicTimerDispatch2->GetNextShorterInterval (
                                                               gSmmPeriodicTimerDispatch2,
                                                               &SmiTickInterval
                                                               );
    if (SmiTickInterval != NULL) {
      gSmiTickPeriodTable[Count] = *SmiTickInterval;
    }

    Count++;
  } while (SmiTickInterval != NULL);

  //
  // Allocate buffer for initial set of periodic SMI handlers
  //
  EnlargeFreePeriodicSmiLibraryHandlerList ();

  return EFI_SUCCESS;
}

/**
  The constructor function caches the pointer to the SMM Periodic Timer Dispatch 2
  Protocol and collects the list SMI tick rates that the hardware supports.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
SmmPeriodicSmiLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  LIST_ENTRY                            *Link;
  PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT  *PeriodicSmiLibraryHandler;

  //
  // Free the table of supported periodic SMI tick rates
  //
  if (gSmiTickPeriodTable != NULL) {
    FreePool (gSmiTickPeriodTable);
  }

  //
  // Disable all periodic SMI handlers
  //
  for (Link = GetFirstNode (&gPeriodicSmiLibraryHandlers); !IsNull (&gPeriodicSmiLibraryHandlers, Link);) {
    PeriodicSmiLibraryHandler = PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_FROM_LINK (Link);
    Link                      = GetNextNode (&gPeriodicSmiLibraryHandlers, Link);
    PeriodicSmiDisable (PeriodicSmiLibraryHandler->DispatchHandle);
  }

  //
  // Free all the periodic SMI handler entries
  //
  for (Link = GetFirstNode (&gFreePeriodicSmiLibraryHandlers); !IsNull (&gFreePeriodicSmiLibraryHandlers, Link);) {
    PeriodicSmiLibraryHandler = PERIODIC_SMI_LIBRARY_HANDLER_CONTEXT_FROM_LINK (Link);
    Link                      = RemoveEntryList (Link);
    FreePool (PeriodicSmiLibraryHandler);
  }

  return EFI_SUCCESS;
}
