/** @file
  Provides services to enable and disable periodic SMI handlers.

Copyright (c) 2011 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PERIODIC_SMI_LIB_H__
#define __PERIODIC_SMI_LIB_H__

#define PERIODIC_SMI_LIBRARY_ANY_CPU  0xffffffff

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
  );

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
  );

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
  );

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
  );

/**
  This function is a prototype for a periodic SMI handler function
  that may be enabled with PeriodicSmiEnable() and disabled with
  PeriodicSmiDisable().

  @param[in] Context      Content registered with PeriodicSmiEnable().
  @param[in] ElapsedTime  The actual time in 100ns units elapsed since
                          this function was called.  A value of 0 indicates
                          an unknown amount of time.

**/
typedef
VOID
(EFIAPI *PERIODIC_SMI_LIBRARY_HANDLER) (
  IN CONST VOID  *Context OPTIONAL,
  IN UINT64      ElapsedTime
  );

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
  IN OUT EFI_HANDLE                    *DispatchHandle,    OPTIONAL
  IN     PERIODIC_SMI_LIBRARY_HANDLER  DispatchFunction,
  IN     CONST VOID                    *Context,           OPTIONAL
  IN     UINT64                        TickPeriod,
  IN     UINTN                         Cpu,
  IN     UINTN                         StackSize
  );

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
  );

#endif
