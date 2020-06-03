/** @file
  Provides services to log the execution times and retrieve them later.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PERFORMANCE_LIB_H__
#define __PERFORMANCE_LIB_H__

///
/// Performance library propery mask bits
///
#define PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED  0x00000001

//
// Public Progress Identifiers for Event Records.
//
#define PERF_EVENT_ID                   0x00

#define MODULE_START_ID                 0x01
#define MODULE_END_ID                   0x02
#define MODULE_LOADIMAGE_START_ID       0x03
#define MODULE_LOADIMAGE_END_ID         0x04
#define MODULE_DB_START_ID              0x05
#define MODULE_DB_END_ID                0x06
#define MODULE_DB_SUPPORT_START_ID      0x07
#define MODULE_DB_SUPPORT_END_ID        0x08
#define MODULE_DB_STOP_START_ID         0x09
#define MODULE_DB_STOP_END_ID           0x0A

#define PERF_EVENTSIGNAL_START_ID       0x10
#define PERF_EVENTSIGNAL_END_ID         0x11
#define PERF_CALLBACK_START_ID          0x20
#define PERF_CALLBACK_END_ID            0x21
#define PERF_FUNCTION_START_ID          0x30
#define PERF_FUNCTION_END_ID            0x31
#define PERF_INMODULE_START_ID          0x40
#define PERF_INMODULE_END_ID            0x41
#define PERF_CROSSMODULE_START_ID       0x50
#define PERF_CROSSMODULE_END_ID         0x51

//
// Declare bits for PcdPerformanceLibraryPropertyMask and
// also used as the Type parameter of LogPerformanceMeasurementEnabled().
//
#define PERF_CORE_START_IMAGE            0x0002
#define PERF_CORE_LOAD_IMAGE             0x0004
#define PERF_CORE_DB_SUPPORT             0x0008
#define PERF_CORE_DB_START               0x0010
#define PERF_CORE_DB_STOP                0x0020

#define PERF_GENERAL_TYPE                0x0040

/**
  Creates a record for the beginning of a performance measurement.

  Creates a record that contains the Handle, Token, and Module.
  If TimeStamp is not zero, then TimeStamp is added to the record as the start time.
  If TimeStamp is zero, then this function reads the current time stamp
  and adds that time stamp value to the record as the start time.

  @param  Handle                  Pointer to environment specific context used
                                  to identify the component being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string
                                  that identifies the component being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string
                                  that identifies the module being measured.
  @param  TimeStamp               64-bit time stamp.

  @retval RETURN_SUCCESS          The start of the measurement was recorded.
  @retval RETURN_OUT_OF_RESOURCES There are not enough resources to record the measurement.
  @retval RETURN_DEVICE_ERROR     A device error reading the time stamp.

**/
RETURN_STATUS
EFIAPI
StartPerformanceMeasurement (
  IN CONST VOID   *Handle,  OPTIONAL
  IN CONST CHAR8  *Token,   OPTIONAL
  IN CONST CHAR8  *Module,  OPTIONAL
  IN UINT64       TimeStamp
  );

/**
  Fills in the end time of a performance measurement.

  Looks up the record that matches Handle, Token, and Module.
  If the record can not be found then return RETURN_NOT_FOUND.
  If the record is found and TimeStamp is not zero,
  then TimeStamp is added to the record as the end time.
  If the record is found and TimeStamp is zero, then this function reads
  the current time stamp and adds that time stamp value to the record as the end time.

  @param  Handle                  Pointer to environment specific context used
                                  to identify the component being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string
                                  that identifies the component being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string
                                  that identifies the module being measured.
  @param  TimeStamp               64-bit time stamp.

  @retval RETURN_SUCCESS          The end of  the measurement was recorded.
  @retval RETURN_NOT_FOUND        The specified measurement record could not be found.
  @retval RETURN_DEVICE_ERROR     A device error reading the time stamp.

**/
RETURN_STATUS
EFIAPI
EndPerformanceMeasurement (
  IN CONST VOID   *Handle,  OPTIONAL
  IN CONST CHAR8  *Token,   OPTIONAL
  IN CONST CHAR8  *Module,  OPTIONAL
  IN UINT64       TimeStamp
  );

/**
  Attempts to retrieve a performance measurement log entry from the performance measurement log.
  It can also retrieve the log created by StartPerformanceMeasurementEx and EndPerformanceMeasurementEx,
  and then eliminate the Identifier.

  Attempts to retrieve the performance log entry specified by LogEntryKey.  If LogEntryKey is
  zero on entry, then an attempt is made to retrieve the first entry from the performance log,
  and the key for the second entry in the log is returned.  If the performance log is empty,
  then no entry is retrieved and zero is returned.  If LogEntryKey is not zero, then the performance
  log entry associated with LogEntryKey is retrieved, and the key for the next entry in the log is
  returned.  If LogEntryKey is the key for the last entry in the log, then the last log entry is
  retrieved and an implementation specific non-zero key value that specifies the end of the performance
  log is returned.  If LogEntryKey is equal this implementation specific non-zero key value, then no entry
  is retrieved and zero is returned.  In the cases where a performance log entry can be returned,
  the log entry is returned in Handle, Token, Module, StartTimeStamp, and EndTimeStamp.
  If LogEntryKey is not a valid log entry key for the performance measurement log, then ASSERT().
  If Handle is NULL, then ASSERT().
  If Token is NULL, then ASSERT().
  If Module is NULL, then ASSERT().
  If StartTimeStamp is NULL, then ASSERT().
  If EndTimeStamp is NULL, then ASSERT().

  @param  LogEntryKey             On entry, the key of the performance measurement log entry to retrieve.
                                  0, then the first performance measurement log entry is retrieved.
                                  On exit, the key of the next performance lof entry entry.
  @param  Handle                  Pointer to environment specific context used to identify the component
                                  being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string that identifies the component
                                  being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string that identifies the module
                                  being measured.
  @param  StartTimeStamp          Pointer to the 64-bit time stamp that was recorded when the measurement
                                  was started.
  @param  EndTimeStamp            Pointer to the 64-bit time stamp that was recorded when the measurement
                                  was ended.

  @return The key for the next performance log entry (in general case).

**/
UINTN
EFIAPI
GetPerformanceMeasurement (
  IN  UINTN       LogEntryKey,
  OUT CONST VOID  **Handle,
  OUT CONST CHAR8 **Token,
  OUT CONST CHAR8 **Module,
  OUT UINT64      *StartTimeStamp,
  OUT UINT64      *EndTimeStamp
  );

/**
  Creates a record for the beginning of a performance measurement.

  Creates a record that contains the Handle, Token, Module and Identifier.
  If TimeStamp is not zero, then TimeStamp is added to the record as the start time.
  If TimeStamp is zero, then this function reads the current time stamp
  and adds that time stamp value to the record as the start time.

  @param  Handle                  Pointer to environment specific context used
                                  to identify the component being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string
                                  that identifies the component being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string
                                  that identifies the module being measured.
  @param  TimeStamp               64-bit time stamp.
  @param  Identifier              32-bit identifier. If the value is 0, the created record
                                  is same as the one created by StartPerformanceMeasurement.

  @retval RETURN_SUCCESS          The start of the measurement was recorded.
  @retval RETURN_OUT_OF_RESOURCES There are not enough resources to record the measurement.
  @retval RETURN_DEVICE_ERROR     A device error reading the time stamp.

**/
RETURN_STATUS
EFIAPI
StartPerformanceMeasurementEx (
  IN CONST VOID   *Handle,  OPTIONAL
  IN CONST CHAR8  *Token,   OPTIONAL
  IN CONST CHAR8  *Module,  OPTIONAL
  IN UINT64       TimeStamp,
  IN UINT32       Identifier
  );

/**
  Fills in the end time of a performance measurement.

  Looks up the record that matches Handle, Token and Module.
  If the record can not be found then return RETURN_NOT_FOUND.
  If the record is found and TimeStamp is not zero,
  then TimeStamp is added to the record as the end time.
  If the record is found and TimeStamp is zero, then this function reads
  the current time stamp and adds that time stamp value to the record as the end time.

  @param  Handle                  Pointer to environment specific context used
                                  to identify the component being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string
                                  that identifies the component being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string
                                  that identifies the module being measured.
  @param  TimeStamp               64-bit time stamp.
  @param  Identifier              32-bit identifier. If the value is 0, the found record
                                  is same as the one found by EndPerformanceMeasurement.

  @retval RETURN_SUCCESS          The end of  the measurement was recorded.
  @retval RETURN_NOT_FOUND        The specified measurement record could not be found.
  @retval RETURN_DEVICE_ERROR     A device error reading the time stamp.

**/
RETURN_STATUS
EFIAPI
EndPerformanceMeasurementEx (
  IN CONST VOID   *Handle,  OPTIONAL
  IN CONST CHAR8  *Token,   OPTIONAL
  IN CONST CHAR8  *Module,  OPTIONAL
  IN UINT64       TimeStamp,
  IN UINT32       Identifier
  );

/**
  Attempts to retrieve a performance measurement log entry from the performance measurement log.
  It can also retrieve the log created by StartPerformanceMeasurement and EndPerformanceMeasurement,
  and then assign the Identifier with 0.

  Attempts to retrieve the performance log entry specified by LogEntryKey.  If LogEntryKey is
  zero on entry, then an attempt is made to retrieve the first entry from the performance log,
  and the key for the second entry in the log is returned.  If the performance log is empty,
  then no entry is retrieved and zero is returned.  If LogEntryKey is not zero, then the performance
  log entry associated with LogEntryKey is retrieved, and the key for the next entry in the log is
  returned.  If LogEntryKey is the key for the last entry in the log, then the last log entry is
  retrieved and an implementation specific non-zero key value that specifies the end of the performance
  log is returned.  If LogEntryKey is equal this implementation specific non-zero key value, then no entry
  is retrieved and zero is returned.  In the cases where a performance log entry can be returned,
  the log entry is returned in Handle, Token, Module, StartTimeStamp, EndTimeStamp and Identifier.
  If LogEntryKey is not a valid log entry key for the performance measurement log, then ASSERT().
  If Handle is NULL, then ASSERT().
  If Token is NULL, then ASSERT().
  If Module is NULL, then ASSERT().
  If StartTimeStamp is NULL, then ASSERT().
  If EndTimeStamp is NULL, then ASSERT().
  If Identifier is NULL, then ASSERT().

  @param  LogEntryKey             On entry, the key of the performance measurement log entry to retrieve.
                                  0, then the first performance measurement log entry is retrieved.
                                  On exit, the key of the next performance of entry entry.
  @param  Handle                  Pointer to environment specific context used to identify the component
                                  being measured.
  @param  Token                   Pointer to a Null-terminated ASCII string that identifies the component
                                  being measured.
  @param  Module                  Pointer to a Null-terminated ASCII string that identifies the module
                                  being measured.
  @param  StartTimeStamp          Pointer to the 64-bit time stamp that was recorded when the measurement
                                  was started.
  @param  EndTimeStamp            Pointer to the 64-bit time stamp that was recorded when the measurement
                                  was ended.
  @param  Identifier              Pointer to the 32-bit identifier that was recorded.

  @return The key for the next performance log entry (in general case).

**/
UINTN
EFIAPI
GetPerformanceMeasurementEx (
  IN  UINTN       LogEntryKey,
  OUT CONST VOID  **Handle,
  OUT CONST CHAR8 **Token,
  OUT CONST CHAR8 **Module,
  OUT UINT64      *StartTimeStamp,
  OUT UINT64      *EndTimeStamp,
  OUT UINT32      *Identifier
  );

/**
  Returns TRUE if the performance measurement macros are enabled.

  This function returns TRUE if the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of
  PcdPerformanceLibraryPropertyMask is set.  Otherwise FALSE is returned.

  @retval TRUE                    The PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of
                                  PcdPerformanceLibraryPropertyMask is set.
  @retval FALSE                   The PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of
                                  PcdPerformanceLibraryPropertyMask is clear.

**/
BOOLEAN
EFIAPI
PerformanceMeasurementEnabled (
  VOID
  );


/**
  Check whether the specified performance measurement can be logged.

  This function returns TRUE when the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set
  and the Type disable bit in PcdPerformanceLibraryPropertyMask is not set.

  @param Type        - Type of the performance measurement entry.

  @retval TRUE         The performance measurement can be logged.
  @retval FALSE        The performance measurement can NOT be logged.

**/
BOOLEAN
EFIAPI
LogPerformanceMeasurementEnabled (
  IN  CONST UINTN        Type
  );

/**
  Create performance record with event description.

  @param CallerIdentifier - Image handle or pointer to caller ID GUID
  @param Guid             - Pointer to a GUID.
                            Used for event signal perf and callback perf to cache the event guid.
  @param String           - Pointer to a string describing the measurement
  @param Address          - Pointer to a location in memory relevant to the measurement.
  @param Identifier       - Performance identifier describing the type of measurement.

  @retval RETURN_SUCCESS           - Successfully created performance record
  @retval RETURN_OUT_OF_RESOURCES  - Ran out of space to store the records
  @retval RETURN_INVALID_PARAMETER - Invalid parameter passed to function - NULL
                                     pointer or invalid Identifier.

**/
RETURN_STATUS
EFIAPI
LogPerformanceMeasurement (
  IN CONST VOID   *CallerIdentifier, OPTIONAL
  IN CONST VOID   *Guid,    OPTIONAL
  IN CONST CHAR8  *String,  OPTIONAL
  IN UINT64       Address,  OPTIONAL
  IN UINT32       Identifier
  );

/**
  Begin Macro to measure the performance of StartImage in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT1(dsiable PERF_CORE_START_IMAGE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_START_IMAGE_BEGIN(ModuleHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_START_IMAGE)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, 0, MODULE_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the performance of StartImage in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT1 (dsiable PERF_CORE_START_IMAGE)of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_START_IMAGE_END(ModuleHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_START_IMAGE)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, 0, MODULE_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the performance of LoadImage in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT2 (dsiable PERF_CORE_LOAD_IAMGE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_LOAD_IMAGE_BEGIN(ModuleHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_LOAD_IMAGE)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, 0, MODULE_LOADIMAGE_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the performance of LoadImage in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT2 (dsiable PERF_CORE_LOAD_IAMGE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_LOAD_IMAGE_END(ModuleHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_LOAD_IMAGE)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, 0, MODULE_LOADIMAGE_END_ID); \
    } \
  } while (FALSE)

/**
  Start Macro to measure the performance of DriverBinding Support in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT3 (dsiable PERF_CORE_DB_SUPPORT) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_SUPPORT_BEGIN(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_SUPPORT)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_SUPPORT_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the performance of DriverBinding Support in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT3 (dsiable PERF_CORE_DB_SUPPORT) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_SUPPORT_END(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_SUPPORT)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_SUPPORT_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the performance of DriverBinding Start in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT4 (dsiable PERF_CORE_DB_START) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_START_BEGIN(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_START)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the performance of DriverBinding Start in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT4 (dsiable PERF_CORE_DB_START) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_START_END(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_START)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_END_ID); \
    } \
  } while (FALSE)

/**
  Start Macro to measure the performance of DriverBinding Stop in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT5 (dsiable PERF_CORE_DB_STOP) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_STOP_BEGIN(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_STOP)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_STOP_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the performance of DriverBinding Stop in core.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT5 (dsiable PERF_CORE_DB_STOP) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_DRIVER_BINDING_STOP_END(ModuleHandle, ControllerHandle) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_CORE_DB_STOP)) { \
      LogPerformanceMeasurement (ModuleHandle, NULL, NULL, (UINT64)(UINTN)ControllerHandle, MODULE_DB_STOP_END_ID); \
    } \
  } while (FALSE)

/**
  Macro to measure the time from power-on to this macro execution.
  It can be used to log a meaningful thing which happens at a time point.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_EVENT(EventString) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, EventString , 0, PERF_EVENT_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the perofrmance of evnent signal behavior in any module.
  The event guid will be passed with this macro.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_EVENT_SIGNAL_BEGIN(EventGuid) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, EventGuid, __FUNCTION__ , 0, PERF_EVENTSIGNAL_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the perofrmance of evnent signal behavior in any module.
  The event guid will be passed with this macro.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_EVENT_SIGNAL_END(EventGuid) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, EventGuid, __FUNCTION__ , 0, PERF_EVENTSIGNAL_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the perofrmance of a callback function in any module.
  The event guid which trigger the callback function will be passed with this macro.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_CALLBACK_BEGIN(TriggerGuid) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, TriggerGuid, __FUNCTION__ , 0, PERF_CALLBACK_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the perofrmance of a callback function in any module.
  The event guid which trigger the callback function will be passed with this macro.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_CALLBACK_END(TriggerGuid) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, TriggerGuid, __FUNCTION__ , 0, PERF_CALLBACK_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the perofrmance of a general function in any module.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_FUNCTION_BEGIN() \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, __FUNCTION__ , 0, PERF_FUNCTION_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the perofrmance of a general function in any module.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_FUNCTION_END() \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, __FUNCTION__ , 0, PERF_FUNCTION_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the perofrmance of a behavior within one module.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_INMODULE_BEGIN(MeasurementString) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, MeasurementString, 0, PERF_INMODULE_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the perofrmance of a behavior within one module.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_INMODULE_END(MeasurementString) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, MeasurementString, 0, PERF_INMODULE_END_ID); \
    } \
  } while (FALSE)

/**
  Begin Macro to measure the perofrmance of a behavior in different modules.
  Such as the performance of PEI phase, DXE phase, BDS phase.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_CROSSMODULE_BEGIN(MeasurementString) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, MeasurementString, 0, PERF_CROSSMODULE_START_ID); \
    } \
  } while (FALSE)

/**
  End Macro to measure the perofrmance of a behavior in different modules.
  Such as the performance of PEI phase, DXE phase, BDS phase.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  and the BIT6 (dsiable PERF_GENERAL_TYPE) of PcdPerformanceLibraryPropertyMask is not set.
  then LogPerformanceMeasurement() is called.

**/
#define PERF_CROSSMODULE_END(MeasurementString) \
  do { \
    if (LogPerformanceMeasurementEnabled (PERF_GENERAL_TYPE)) { \
      LogPerformanceMeasurement (&gEfiCallerIdGuid, NULL, MeasurementString, 0, PERF_CROSSMODULE_END_ID); \
    } \
  } while (FALSE)

/**
  Macro that calls EndPerformanceMeasurement().

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then EndPerformanceMeasurement() is called.

**/
#define PERF_END(Handle, Token, Module, TimeStamp)                    \
  do {                                                                \
    if (PerformanceMeasurementEnabled ()) {                           \
      EndPerformanceMeasurement (Handle, Token, Module, TimeStamp);   \
    }                                                                 \
  } while (FALSE)

/**
  Macro that calls StartPerformanceMeasurement().

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then StartPerformanceMeasurement() is called.

**/
#define PERF_START(Handle, Token, Module, TimeStamp)                  \
  do {                                                                \
    if (PerformanceMeasurementEnabled ()) {                           \
      StartPerformanceMeasurement (Handle, Token, Module, TimeStamp); \
    }                                                                 \
  } while (FALSE)

/**
  Macro that calls EndPerformanceMeasurementEx().

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then EndPerformanceMeasurementEx() is called.

**/
#define PERF_END_EX(Handle, Token, Module, TimeStamp, Identifier)                   \
  do {                                                                              \
    if (PerformanceMeasurementEnabled ()) {                                         \
      EndPerformanceMeasurementEx (Handle, Token, Module, TimeStamp, Identifier);   \
    }                                                                               \
  } while (FALSE)

/**
  Macro that calls StartPerformanceMeasurementEx().

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then StartPerformanceMeasurementEx() is called.

**/
#define PERF_START_EX(Handle, Token, Module, TimeStamp, Identifier)                 \
  do {                                                                              \
    if (PerformanceMeasurementEnabled ()) {                                         \
      StartPerformanceMeasurementEx (Handle, Token, Module, TimeStamp, Identifier); \
    }                                                                               \
  } while (FALSE)

/**
  Macro that marks the beginning of performance measurement source code.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then this macro marks the beginning of source code that is included in a module.
  Otherwise, the source lines between PERF_CODE_BEGIN() and PERF_CODE_END() are not included in a module.

**/
#define PERF_CODE_BEGIN()  do { if (PerformanceMeasurementEnabled ()) { UINT8  __PerformanceCodeLocal

/**
  Macro that marks the end of performance measurement source code.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then this macro marks the end of source code that is included in a module.
  Otherwise, the source lines between PERF_CODE_BEGIN() and PERF_CODE_END() are not included in a module.

**/
#define PERF_CODE_END()    __PerformanceCodeLocal = 0; __PerformanceCodeLocal++; } } while (FALSE)

/**
  Macro that declares a section of performance measurement source code.

  If the PERFORMANCE_LIBRARY_PROPERTY_MEASUREMENT_ENABLED bit of PcdPerformanceLibraryPropertyMask is set,
  then the source code specified by Expression is included in a module.
  Otherwise, the source specified by Expression is not included in a module.

  @param  Expression              Performance measurement source code to include in a module.

**/
#define PERF_CODE(Expression)  \
  PERF_CODE_BEGIN ();          \
  Expression                   \
  PERF_CODE_END ()


#endif
