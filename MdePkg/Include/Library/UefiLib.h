/** @file
  Provides library functions for common UEFI operations. Only available to DXE
  and UEFI module types.

  The UEFI Library provides functions and macros that simplify the development of 
  UEFI Drivers and UEFI Applications.  These functions and macros help manage EFI 
  events, build simple locks utilizing EFI Task Priority Levels (TPLs), install 
  EFI Driver Model related protocols, manage Unicode string tables for UEFI Drivers, 
  and print messages on the console output and standard error devices.

  Note that a reserved macro named MDEPKG_NDEBUG is introduced for the intention
  of size reduction when compiler optimization is disabled. If MDEPKG_NDEBUG is
  defined, then debug and assert related macros wrapped by it are the NULL implementations.

Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __UEFI_LIB_H__
#define __UEFI_LIB_H__

#include <Protocol/DriverBinding.h>
#include <Protocol/DriverConfiguration.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverDiagnostics.h>
#include <Protocol/DriverDiagnostics2.h>
#include <Protocol/GraphicsOutput.h>

#include <Library/BaseLib.h>

///
/// Unicode String Table
///
typedef struct {
  CHAR8   *Language;
  CHAR16  *UnicodeString;
} EFI_UNICODE_STRING_TABLE;

///
/// EFI Lock Status
///
typedef enum {
  EfiLockUninitialized = 0,
  EfiLockReleased      = 1,
  EfiLockAcquired      = 2
} EFI_LOCK_STATE;

///
/// EFI Lock 
///
typedef struct {
  EFI_TPL         Tpl;
  EFI_TPL         OwnerTpl;
  EFI_LOCK_STATE  Lock;
} EFI_LOCK;

/**
  Macro that returns the number of 100 ns units for a specified number of microseconds.
  This is useful for managing EFI timer events.

  @param  Microseconds           The number of microseconds.

  @return The number of 100 ns units equivalent to the number of microseconds specified
          by Microseconds.

**/
#define EFI_TIMER_PERIOD_MICROSECONDS(Microseconds) MultU64x32((UINT64)(Microseconds), 10)

/**
  Macro that returns the number of 100 ns units for a specified number of milliseconds.
  This is useful for managing EFI timer events.

  @param  Milliseconds           The number of milliseconds.

  @return The number of 100 ns units equivalent to the number of milliseconds specified
          by Milliseconds.

**/
#define EFI_TIMER_PERIOD_MILLISECONDS(Milliseconds) MultU64x32((UINT64)(Milliseconds), 10000)

/**
  Macro that returns the number of 100 ns units for a specified number of seconds.
  This is useful for managing EFI timer events.

  @param  Seconds                The number of seconds.

  @return The number of 100 ns units equivalent to the number of seconds specified
          by Seconds.

**/
#define EFI_TIMER_PERIOD_SECONDS(Seconds)           MultU64x32((UINT64)(Seconds), 10000000)

/**
  Macro that returns the a pointer to the next EFI_MEMORY_DESCRIPTOR in an array 
  returned from GetMemoryMap().  

  @param  MemoryDescriptor  A pointer to an EFI_MEMORY_DESCRIPTOR.

  @param  Size              The size, in bytes, of the current EFI_MEMORY_DESCRIPTOR.

  @return A pointer to the next EFI_MEMORY_DESCRIPTOR.

**/
#define NEXT_MEMORY_DESCRIPTOR(MemoryDescriptor, Size) \
  ((EFI_MEMORY_DESCRIPTOR *)((UINT8 *)(MemoryDescriptor) + (Size)))

/**
  Retrieves a pointer to the system configuration table from the EFI System Table
  based on a specified GUID.
  
  This function searches the list of configuration tables stored in the EFI System Table
  for a table with a GUID that matches TableGuid. If a match is found, then a pointer to
  the configuration table is returned in Table, and EFI_SUCCESS is returned. If a matching GUID
  is not found, then EFI_NOT_FOUND is returned.
  If TableGuid is NULL, then ASSERT().
  If Table is NULL, then ASSERT().

  @param  TableGuid       The pointer to table's GUID type..
  @param  Table           The pointer to the table associated with TableGuid in the EFI System Table.

  @retval EFI_SUCCESS     A configuration table matching TableGuid was found.
  @retval EFI_NOT_FOUND   A configuration table matching TableGuid could not be found.

**/
EFI_STATUS
EFIAPI
EfiGetSystemConfigurationTable (  
  IN  EFI_GUID  *TableGuid,
  OUT VOID      **Table
  );

/**
  Creates and returns a notification event and registers that event with all the protocol
  instances specified by ProtocolGuid.

  This function causes the notification function to be executed for every protocol of type
  ProtocolGuid instance that exists in the system when this function is invoked. If there are
  no instances of ProtocolGuid in the handle database at the time this function is invoked,
  then the notification function is still executed one time. In addition, every time a protocol
  of type ProtocolGuid instance is installed or reinstalled, the notification function is also
  executed. This function returns the notification event that was created. 
  If ProtocolGuid is NULL, then ASSERT().
  If NotifyTpl is not a legal TPL value, then ASSERT().
  If NotifyFunction is NULL, then ASSERT().
  If Registration is NULL, then ASSERT().


  @param  ProtocolGuid    Supplies GUID of the protocol upon whose installation the event is fired.
  @param  NotifyTpl       Supplies the task priority level of the event notifications.
  @param  NotifyFunction  Supplies the function to notify when the event is signaled.
  @param  NotifyContext   The context parameter to pass to NotifyFunction.
  @param  Registration    A pointer to a memory location to receive the registration value.
                          This value is passed to LocateHandle() to obtain new handles that
                          have been added that support the ProtocolGuid-specified protocol. 

  @return The notification event that was created.

**/
EFI_EVENT
EFIAPI
EfiCreateProtocolNotifyEvent(
  IN  EFI_GUID          *ProtocolGuid,
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,
  IN  VOID              *NotifyContext,  OPTIONAL
  OUT VOID              **Registration
  );

/**
  Creates a named event that can be signaled with EfiNamedEventSignal().

  This function creates an event using NotifyTpl, NoifyFunction, and NotifyContext.
  This event is signaled with EfiNamedEventSignal(). This provides the ability for one or more
  listeners on the same event named by the GUID specified by Name. 
  If Name is NULL, then ASSERT().
  If NotifyTpl is not a legal TPL value, then ASSERT().
  If NotifyFunction is NULL, then ASSERT().

  @param  Name                  Supplies GUID name of the event.
  @param  NotifyTpl             Supplies the task priority level of the event notifications.
  @param  NotifyFunction        Supplies the function to notify when the event is signaled.
  @param  NotifyContext         The context parameter to pass to NotifyFunction. 
  @param  Registration          A pointer to a memory location to receive the registration value.

  @retval EFI_SUCCESS           A named event was created.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources to create the named event.

**/
EFI_STATUS
EFIAPI
EfiNamedEventListen (
  IN CONST EFI_GUID    *Name,
  IN EFI_TPL           NotifyTpl,
  IN EFI_EVENT_NOTIFY  NotifyFunction,
  IN CONST VOID        *NotifyContext,  OPTIONAL
  OUT VOID             *Registration OPTIONAL
  );

/**
  Signals a named event created with EfiNamedEventListen().

  This function signals the named event specified by Name. The named event must have been
  created with EfiNamedEventListen().
  If Name is NULL, then ASSERT().

  @param  Name                  Supplies the GUID name of the event.

  @retval EFI_SUCCESS           A named event was signaled.
  @retval EFI_OUT_OF_RESOURCES  There are not enough resources to signal the named event.

**/
EFI_STATUS
EFIAPI
EfiNamedEventSignal (
  IN CONST EFI_GUID  *Name
  );

/**
  Signals an event group by placing a new event in the group temporarily and
  signaling it.

  @param[in] EventGroup          Supplies the unique identifier of the event
                                 group to signal.

  @retval EFI_SUCCESS            The event group was signaled successfully.
  @retval EFI_INVALID_PARAMETER  EventGroup is NULL.
  @return                        Error codes that report problems about event
                                 creation or signaling.
**/
EFI_STATUS
EFIAPI
EfiEventGroupSignal (
  IN CONST EFI_GUID *EventGroup
  );

/**
  An empty function that can be used as NotifyFunction parameter of
  CreateEvent() or CreateEventEx().

  @param Event              Event whose notification function is being invoked.
  @param Context            The pointer to the notification function's context,
                            which is implementation-dependent.

**/
VOID
EFIAPI
EfiEventEmptyFunction (
  IN EFI_EVENT              Event,
  IN VOID                   *Context
  );

/** 
  Returns the current TPL.

  This function returns the current TPL.  There is no EFI service to directly 
  retrieve the current TPL. Instead, the RaiseTPL() function is used to raise 
  the TPL to TPL_HIGH_LEVEL.  This will return the current TPL.  The TPL level 
  can then immediately be restored back to the current TPL level with a call 
  to RestoreTPL().

  @return The current TPL.

**/
EFI_TPL
EFIAPI
EfiGetCurrentTpl (
  VOID
  );

/**
  Initializes a basic mutual exclusion lock.

  This function initializes a basic mutual exclusion lock to the released state 
  and returns the lock.  Each lock provides mutual exclusion access at its task 
  priority level.  Since there is no preemption or multiprocessor support in EFI,
  acquiring the lock only consists of raising to the locks TPL.
  If Lock is NULL, then ASSERT().
  If Priority is not a valid TPL value, then ASSERT().

  @param  Lock       A pointer to the lock data structure to initialize.
  @param  Priority   The EFI TPL associated with the lock.

  @return The lock.

**/
EFI_LOCK *
EFIAPI
EfiInitializeLock (
  IN OUT EFI_LOCK  *Lock,
  IN EFI_TPL        Priority
  );

/**
  Initializes a basic mutual exclusion lock.

  This macro initializes the contents of a basic mutual exclusion lock to the 
  released state.  Each lock provides mutual exclusion access at its task 
  priority level.  Since there is no preemption or multiprocessor support in EFI,
  acquiring the lock only consists of raising to the locks TPL.

  @param  Priority  The task priority level of the lock.

  @return The lock.

**/
#define EFI_INITIALIZE_LOCK_VARIABLE(Priority) \
  {Priority, TPL_APPLICATION, EfiLockReleased }


/**
  Macro that calls DebugAssert() if an EFI_LOCK structure is not in the locked state.

  If MDEPKG_NDEBUG is not defined and the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED 
  bit of PcdDebugProperyMask is set, then this macro evaluates the EFI_LOCK 
  structure specified by Lock.  If Lock is not in the locked state, then
  DebugAssert() is called passing in the source filename, source line number, 
  and Lock.

  If Lock is NULL, then ASSERT().

  @param  LockParameter  A pointer to the lock to acquire.

**/
#if !defined(MDEPKG_NDEBUG)       
  #define ASSERT_LOCKED(LockParameter)                  \
    do {                                                \
      if (DebugAssertEnabled ()) {                      \
        ASSERT (LockParameter != NULL);                 \
        if ((LockParameter)->Lock != EfiLockAcquired) { \
          _ASSERT (LockParameter not locked);           \
        }                                               \
      }                                                 \
    } while (FALSE)
#else
  #define ASSERT_LOCKED(LockParameter)
#endif


/**
  Acquires ownership of a lock.

  This function raises the system's current task priority level to the task 
  priority level of the mutual exclusion lock.  Then, it places the lock in the 
  acquired state.
  If Lock is NULL, then ASSERT().
  If Lock is not initialized, then ASSERT().
  If Lock is already in the acquired state, then ASSERT().

  @param  Lock              A pointer to the lock to acquire.

**/
VOID
EFIAPI
EfiAcquireLock (
  IN EFI_LOCK  *Lock
  );

/**
  Acquires ownership of a lock.

  This function raises the system's current task priority level to the task priority
  level of the mutual exclusion lock.  Then, it attempts to place the lock in the acquired state.
  If the lock is already in the acquired state, then EFI_ACCESS_DENIED is returned.
  Otherwise, EFI_SUCCESS is returned.
  If Lock is NULL, then ASSERT().
  If Lock is not initialized, then ASSERT().

  @param  Lock              A pointer to the lock to acquire.

  @retval EFI_SUCCESS       The lock was acquired.
  @retval EFI_ACCESS_DENIED The lock could not be acquired because it is already owned.

**/
EFI_STATUS
EFIAPI
EfiAcquireLockOrFail (
  IN EFI_LOCK  *Lock
  );

/**
  Releases ownership of a lock.

  This function transitions a mutual exclusion lock from the acquired state to 
  the released state, and restores the system's task priority level to its 
  previous level.
  If Lock is NULL, then ASSERT().
  If Lock is not initialized, then ASSERT().
  If Lock is already in the released state, then ASSERT().

  @param  Lock  A pointer to the lock to release.

**/
VOID
EFIAPI
EfiReleaseLock (
  IN EFI_LOCK  *Lock
  );

/**
  Tests whether a controller handle is being managed by a specific driver.

  This function tests whether the driver specified by DriverBindingHandle is
  currently managing the controller specified by ControllerHandle.  This test
  is performed by evaluating if the the protocol specified by ProtocolGuid is
  present on ControllerHandle and is was opened by DriverBindingHandle with an
  attribute of EFI_OPEN_PROTOCOL_BY_DRIVER. 
  If ProtocolGuid is NULL, then ASSERT().

  @param  ControllerHandle     A handle for a controller to test.
  @param  DriverBindingHandle  Specifies the driver binding handle for the
                               driver.
  @param  ProtocolGuid         Specifies the protocol that the driver specified
                               by DriverBindingHandle opens in its Start()
                               function.

  @retval EFI_SUCCESS          ControllerHandle is managed by the driver
                               specified by DriverBindingHandle.
  @retval EFI_UNSUPPORTED      ControllerHandle is not managed by the driver
                               specified by DriverBindingHandle.

**/
EFI_STATUS
EFIAPI
EfiTestManagedDevice (
  IN CONST EFI_HANDLE       ControllerHandle,
  IN CONST EFI_HANDLE       DriverBindingHandle,
  IN CONST EFI_GUID         *ProtocolGuid
  );

/**
  Tests whether a child handle is a child device of the controller.

  This function tests whether ChildHandle is one of the children of
  ControllerHandle.  This test is performed by checking to see if the protocol
  specified by ProtocolGuid is present on ControllerHandle and opened by
  ChildHandle with an attribute of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.
  If ProtocolGuid is NULL, then ASSERT().

  @param  ControllerHandle     A handle for a (parent) controller to test. 
  @param  ChildHandle          A child handle to test.
  @param  ProtocolGuid         Supplies the protocol that the child controller
                               opens on its parent controller. 

  @retval EFI_SUCCESS          ChildHandle is a child of the ControllerHandle.
  @retval EFI_UNSUPPORTED      ChildHandle is not a child of the
                               ControllerHandle.

**/
EFI_STATUS
EFIAPI
EfiTestChildHandle (
  IN CONST EFI_HANDLE       ControllerHandle,
  IN CONST EFI_HANDLE       ChildHandle,
  IN CONST EFI_GUID         *ProtocolGuid
  );

/**
  This function looks up a Unicode string in UnicodeStringTable.

  If Language is a member of SupportedLanguages and a Unicode string is found in
  UnicodeStringTable that matches the language code specified by Language, then it
  is returned in UnicodeString.

  @param  Language                A pointer to the ISO 639-2 language code for the 
                                  Unicode string to look up and return.
  @param  SupportedLanguages      A pointer to the set of ISO 639-2 language codes 
                                  that the Unicode string table supports.  Language 
                                  must be a member of this set.
  @param  UnicodeStringTable      A pointer to the table of Unicode strings.
  @param  UnicodeString           A pointer to the Unicode string from UnicodeStringTable
                                  that matches the language specified by Language.

  @retval EFI_SUCCESS             The Unicode string that matches the language 
                                  specified by Language was found
                                  in the table of Unicode strings UnicodeStringTable, 
                                  and it was returned in UnicodeString.
  @retval EFI_INVALID_PARAMETER   Language is NULL.
  @retval EFI_INVALID_PARAMETER   UnicodeString is NULL.
  @retval EFI_UNSUPPORTED         SupportedLanguages is NULL.
  @retval EFI_UNSUPPORTED         UnicodeStringTable is NULL.
  @retval EFI_UNSUPPORTED         The language specified by Language is not a 
                                  member of SupportedLanguages.
  @retval EFI_UNSUPPORTED         The language specified by Language is not 
                                  supported by UnicodeStringTable.

**/
EFI_STATUS
EFIAPI
LookupUnicodeString (
  IN CONST CHAR8                     *Language,
  IN CONST CHAR8                     *SupportedLanguages,
  IN CONST EFI_UNICODE_STRING_TABLE  *UnicodeStringTable,
  OUT CHAR16                         **UnicodeString
  );

/**
  This function looks up a Unicode string in UnicodeStringTable.

  If Language is a member of SupportedLanguages and a Unicode string is found in
  UnicodeStringTable that matches the language code specified by Language, then
  it is returned in UnicodeString.

  @param  Language             A pointer to an ASCII string containing the ISO 639-2 or the
                               RFC 4646 language code for the Unicode string to look up and
                               return. If Iso639Language is TRUE, then this ASCII string is
                               not assumed to be Null-terminated, and only the first three
                               characters are used. If Iso639Language is FALSE, then this ASCII
                               string must be Null-terminated. 
  @param  SupportedLanguages   A pointer to a Null-terminated ASCII string that contains a
                               set of ISO 639-2 or RFC 4646 language codes that the Unicode
                               string table supports.  Language must be a member of this set.
                               If Iso639Language is TRUE, then this string contains one or more
                               ISO 639-2 language codes with no separator characters. If Iso639Language
                               is FALSE, then is string contains one or more RFC 4646 language
                               codes separated by ';'.
  @param  UnicodeStringTable   A pointer to the table of Unicode strings. Type EFI_UNICODE_STRING_TABLE
                               is defined in "Related Definitions".
  @param  UnicodeString        A pointer to the Null-terminated Unicode string from UnicodeStringTable
                               that matches the language specified by Language.
  @param  Iso639Language       Specifies the supported language code format. If it is TRUE, then
                               Language and SupportedLanguages follow ISO 639-2 language code format.
                               Otherwise, they follow the RFC 4646 language code format.


  @retval  EFI_SUCCESS            The Unicode string that matches the language specified by Language
                                  was found in the table of Unicode strings UnicodeStringTable, and
                                  it was returned in UnicodeString.
  @retval  EFI_INVALID_PARAMETER  Language is NULL.  
  @retval  EFI_INVALID_PARAMETER  UnicodeString is NULL.  
  @retval  EFI_UNSUPPORTED        SupportedLanguages is NULL.  
  @retval  EFI_UNSUPPORTED        UnicodeStringTable is NULL.  
  @retval  EFI_UNSUPPORTED        The language specified by Language is not a member of SupportedLanguages.  
  @retval  EFI_UNSUPPORTED        The language specified by Language is not supported by UnicodeStringTable.

**/
EFI_STATUS
EFIAPI
LookupUnicodeString2 (
  IN CONST CHAR8                     *Language,
  IN CONST CHAR8                     *SupportedLanguages,
  IN CONST EFI_UNICODE_STRING_TABLE  *UnicodeStringTable,
  OUT CHAR16                         **UnicodeString,
  IN BOOLEAN                         Iso639Language
  );

/**
  This function adds a Unicode string to UnicodeStringTable.

  If Language is a member of SupportedLanguages then UnicodeString is added to 
  UnicodeStringTable.  New buffers are allocated for both Language and 
  UnicodeString.  The contents of Language and UnicodeString are copied into 
  these new buffers.  These buffers are automatically freed when 
  FreeUnicodeStringTable() is called.

  @param  Language                A pointer to the ISO 639-2 language code for the Unicode 
                                  string to add.
  @param  SupportedLanguages      A pointer to the set of ISO 639-2 language codes
                                  that the Unicode string table supports.
                                  Language must be a member of this set.
  @param  UnicodeStringTable      A pointer to the table of Unicode strings.
  @param  UnicodeString           A pointer to the Unicode string to add.

  @retval EFI_SUCCESS             The Unicode string that matches the language 
                                  specified by Language was found in the table of 
                                  Unicode strings UnicodeStringTable, and it was 
                                  returned in UnicodeString.
  @retval EFI_INVALID_PARAMETER   Language is NULL.
  @retval EFI_INVALID_PARAMETER   UnicodeString is NULL.
  @retval EFI_INVALID_PARAMETER   UnicodeString is an empty string.
  @retval EFI_UNSUPPORTED         SupportedLanguages is NULL.
  @retval EFI_ALREADY_STARTED     A Unicode string with language Language is 
                                  already present in UnicodeStringTable.
  @retval EFI_OUT_OF_RESOURCES    There is not enough memory to add another 
                                  Unicode string to UnicodeStringTable.
  @retval EFI_UNSUPPORTED         The language specified by Language is not a 
                                  member of SupportedLanguages.

**/
EFI_STATUS
EFIAPI
AddUnicodeString (
  IN CONST CHAR8               *Language,
  IN CONST CHAR8               *SupportedLanguages,
  IN EFI_UNICODE_STRING_TABLE  **UnicodeStringTable,
  IN CONST CHAR16              *UnicodeString
  );

/**
  This function adds the Null-terminated Unicode string specified by UnicodeString
  to UnicodeStringTable.

  If Language is a member of SupportedLanguages then UnicodeString is added to
  UnicodeStringTable.  New buffers are allocated for both Language and UnicodeString.
  The contents of Language and UnicodeString are copied into these new buffers.
  These buffers are automatically freed when EfiLibFreeUnicodeStringTable() is called.

  @param  Language            A pointer to an ASCII string containing the ISO 639-2 or
                              the RFC 4646 language code for the Unicode string to add.
                              If Iso639Language is TRUE, then this ASCII string is not
                              assumed to be Null-terminated, and only the first three
                              chacters are used. If Iso639Language is FALSE, then this
                              ASCII string must be Null-terminated.
  @param  SupportedLanguages  A pointer to a Null-terminated ASCII string that contains
                              a set of ISO 639-2 or RFC 4646 language codes that the Unicode
                              string table supports.  Language must be a member of this set.
                              If Iso639Language is TRUE, then this string contains one or more
                              ISO 639-2 language codes with no separator characters.
                              If Iso639Language is FALSE, then is string contains one or more
                              RFC 4646 language codes separated by ';'.
  @param  UnicodeStringTable  A pointer to the table of Unicode strings. Type EFI_UNICODE_STRING_TABLE
                              is defined in "Related Definitions".
  @param  UnicodeString       A pointer to the Unicode string to add.  
  @param  Iso639Language      Specifies the supported language code format. If it is TRUE,
                              then Language and SupportedLanguages follow ISO 639-2 language code format.
                              Otherwise, they follow RFC 4646 language code format.

  @retval EFI_SUCCESS            The Unicode string that matches the language specified by
                                 Language was found in the table of Unicode strings UnicodeStringTable,
                                 and it was returned in UnicodeString.  
  @retval EFI_INVALID_PARAMETER  Language is NULL.  
  @retval EFI_INVALID_PARAMETER  UnicodeString is NULL.  
  @retval EFI_INVALID_PARAMETER  UnicodeString is an empty string.  
  @retval EFI_UNSUPPORTED        SupportedLanguages is NULL.  
  @retval EFI_ALREADY_STARTED    A Unicode string with language Language is already present in
                                 UnicodeStringTable.  
  @retval EFI_OUT_OF_RESOURCES   There is not enough memory to add another Unicode string UnicodeStringTable.  
  @retval EFI_UNSUPPORTED        The language specified by Language is not a member of SupportedLanguages.

**/
EFI_STATUS
EFIAPI
AddUnicodeString2 (
  IN CONST CHAR8               *Language,
  IN CONST CHAR8               *SupportedLanguages,
  IN EFI_UNICODE_STRING_TABLE  **UnicodeStringTable,
  IN CONST CHAR16              *UnicodeString,
  IN BOOLEAN                   Iso639Language
  );

/**
  This function frees the table of Unicode strings in UnicodeStringTable.

  If UnicodeStringTable is NULL, then EFI_SUCCESS is returned.
  Otherwise, each language code, and each Unicode string in the Unicode string 
  table are freed, and EFI_SUCCESS is returned.

  @param  UnicodeStringTable  A pointer to the table of Unicode strings.

  @retval EFI_SUCCESS         The Unicode string table was freed.

**/
EFI_STATUS
EFIAPI
FreeUnicodeStringTable (
  IN EFI_UNICODE_STRING_TABLE  *UnicodeStringTable
  );

#ifndef DISABLE_NEW_DEPRECATED_INTERFACES

/**
  [ATTENTION] This function will be deprecated for security reason.

  Returns a pointer to an allocated buffer that contains the contents of a 
  variable retrieved through the UEFI Runtime Service GetVariable().  The 
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().

  If Name is NULL, then ASSERT().
  If Guid is NULL, then ASSERT().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure.

  @retval NULL   The variable could not be retrieved.
  @retval NULL   There are not enough resources available for the variable contents.
  @retval Other  A pointer to allocated buffer containing the variable contents.

**/
VOID *
EFIAPI
GetVariable (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid
  );

/**
  [ATTENTION] This function will be deprecated for security reason.

  Returns a pointer to an allocated buffer that contains the contents of a 
  variable retrieved through the UEFI Runtime Service GetVariable().  This 
  function always uses the EFI_GLOBAL_VARIABLE GUID to retrieve variables.
  The returned buffer is allocated using AllocatePool().  The caller is 
  responsible for freeing this buffer with FreePool().

  If Name is NULL, then ASSERT().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.

  @retval NULL   The variable could not be retrieved.
  @retval NULL   There are not enough resources available for the variable contents.
  @retval Other  A pointer to allocated buffer containing the variable contents.

**/
VOID *
EFIAPI
GetEfiGlobalVariable (
  IN CONST CHAR16  *Name
  );
#endif


/**
  Returns the status whether get the variable success. The function retrieves 
  variable  through the UEFI Runtime Service GetVariable().  The 
  returned buffer is allocated using AllocatePool().  The caller is responsible
  for freeing this buffer with FreePool().

  If Name  is NULL, then ASSERT().
  If Guid  is NULL, then ASSERT().
  If Value is NULL, then ASSERT().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[in]  Guid  The pointer to an EFI_GUID structure
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.

  @return EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @return EFI_SUCCESS               Find the specified variable.
  @return Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
GetVariable2 (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid,
  OUT VOID           **Value,
  OUT UINTN          *Size OPTIONAL
  );

/**
  Returns a pointer to an allocated buffer that contains the contents of a 
  variable retrieved through the UEFI Runtime Service GetVariable().  This 
  function always uses the EFI_GLOBAL_VARIABLE GUID to retrieve variables.
  The returned buffer is allocated using AllocatePool().  The caller is 
  responsible for freeing this buffer with FreePool().

  If Name  is NULL, then ASSERT().
  If Value is NULL, then ASSERT().

  @param[in]  Name  The pointer to a Null-terminated Unicode string.
  @param[out] Value The buffer point saved the variable info.
  @param[out] Size  The buffer size of the variable.

  @return EFI_OUT_OF_RESOURCES      Allocate buffer failed.
  @return EFI_SUCCESS               Find the specified variable.
  @return Others Errors             Return errors from call to gRT->GetVariable.

**/
EFI_STATUS
EFIAPI
GetEfiGlobalVariable2 (
  IN CONST CHAR16    *Name,
  OUT VOID           **Value,
  OUT UINTN          *Size OPTIONAL
  );

/**
  Returns a pointer to an allocated buffer that contains the best matching language 
  from a set of supported languages.  
  
  This function supports both ISO 639-2 and RFC 4646 language codes, but language 
  code types may not be mixed in a single call to this function.  The language 
  code returned is allocated using AllocatePool().  The caller is responsible for 
  freeing the allocated buffer using FreePool().  This function supports a variable
  argument list that allows the caller to pass in a prioritized list of language 
  codes to test against all the language codes in SupportedLanguages. 

  If SupportedLanguages is NULL, then ASSERT().

  @param[in]  SupportedLanguages  A pointer to a Null-terminated ASCII string that
                                  contains a set of language codes in the format 
                                  specified by Iso639Language.
  @param[in]  Iso639Language      If TRUE, then all language codes are assumed to be
                                  in ISO 639-2 format.  If FALSE, then all language
                                  codes are assumed to be in RFC 4646 language format
  @param[in]  ...                 A variable argument list that contains pointers to 
                                  Null-terminated ASCII strings that contain one or more
                                  language codes in the format specified by Iso639Language.
                                  The first language code from each of these language
                                  code lists is used to determine if it is an exact or
                                  close match to any of the language codes in 
                                  SupportedLanguages.  Close matches only apply to RFC 4646
                                  language codes, and the matching algorithm from RFC 4647
                                  is used to determine if a close match is present.  If 
                                  an exact or close match is found, then the matching
                                  language code from SupportedLanguages is returned.  If
                                  no matches are found, then the next variable argument
                                  parameter is evaluated.  The variable argument list 
                                  is terminated by a NULL.

  @retval NULL   The best matching language could not be found in SupportedLanguages.
  @retval NULL   There are not enough resources available to return the best matching 
                 language.
  @retval Other  A pointer to a Null-terminated ASCII string that is the best matching 
                 language in SupportedLanguages.

**/
CHAR8 *
EFIAPI
GetBestLanguage (
  IN CONST CHAR8  *SupportedLanguages, 
  IN BOOLEAN      Iso639Language,
  ...
  );

/**
  Draws a dialog box to the console output device specified by 
  ConOut defined in the EFI_SYSTEM_TABLE and waits for a keystroke
  from the console input device specified by ConIn defined in the 
  EFI_SYSTEM_TABLE.

  If there are no strings in the variable argument list, then ASSERT().
  If all the strings in the variable argument list are empty, then ASSERT().

  @param[in]   Attribute  Specifies the foreground and background color of the popup.
  @param[out]  Key        A pointer to the EFI_KEY value of the key that was 
                          pressed.  This is an optional parameter that may be NULL.
                          If it is NULL then no wait for a keypress will be performed.
  @param[in]  ...         The variable argument list that contains pointers to Null-
                          terminated Unicode strings to display in the dialog box.  
                          The variable argument list is terminated by a NULL.

**/
VOID
EFIAPI
CreatePopUp (
  IN  UINTN          Attribute,                
  OUT EFI_INPUT_KEY  *Key,      OPTIONAL
  ...
  );

/**
  Retrieves the width of a Unicode character.

  This function computes and returns the width of the Unicode character specified
  by UnicodeChar.

  @param  UnicodeChar   A Unicode character.

  @retval 0             The width if UnicodeChar could not be determined.
  @retval 1             UnicodeChar is a narrow glyph.
  @retval 2             UnicodeChar is a wide glyph.

**/
UINTN
EFIAPI
GetGlyphWidth (
  IN CHAR16  UnicodeChar
  );

/**
  Computes the display length of a Null-terminated Unicode String.

  This function computes and returns the display length of the Null-terminated Unicode
  string specified by String.  If String is NULL then 0 is returned. If any of the widths
  of the Unicode characters in String can not be determined, then 0 is returned. The display
  width of String can be computed by summing the display widths of each Unicode character
  in String.  Unicode characters that are narrow glyphs have a width of 1, and Unicode
  characters that are width glyphs have a width of 2. 
  If String is not aligned on a 16-bit boundary, then ASSERT().

  @param  String      A pointer to a Null-terminated Unicode string.

  @return The display length of the Null-terminated Unicode string specified by String.
  
**/
UINTN
EFIAPI
UnicodeStringDisplayLength (
  IN CONST CHAR16  *String
  );

//
// Functions that abstract early Framework contamination of UEFI.
//
/**
  Create, Signal, and Close the Ready to Boot event using EfiSignalEventReadyToBoot().
  
  This function abstracts the signaling of the Ready to Boot Event. The Framework moved
  from a proprietary to UEFI 2.0 based mechanism. This library abstracts the caller
  from how this event is created to prevent to code form having to change with the
  version of the specification supported.

**/
VOID
EFIAPI
EfiSignalEventReadyToBoot (
  VOID
  );

/**
  Create, Signal, and Close the Ready to Boot event using EfiSignalEventLegacyBoot().

  This function abstracts the signaling of the Legacy Boot Event. The Framework moved from
  a proprietary to UEFI 2.0 based mechanism.  This library abstracts the caller from how
  this event is created to prevent to code form having to change with the version of the
  specification supported.

**/
VOID
EFIAPI
EfiSignalEventLegacyBoot (
  VOID
  );

/**
  Creates an EFI event in the Legacy Boot Event Group.

  Prior to UEFI 2.0 this was done via a non blessed UEFI extensions and this library
  abstracts the implementation mechanism of this event from the caller. This function
  abstracts the creation of the Legacy Boot Event. The Framework moved from a proprietary
  to UEFI 2.0 based mechanism.  This library abstracts the caller from how this event
  is created to prevent to code form having to change with the version of the
  specification supported.
  If LegacyBootEvent is NULL, then ASSERT().

  @param  LegacyBootEvent   Returns the EFI event returned from gBS->CreateEvent(Ex).

  @retval EFI_SUCCESS       The event was created.
  @retval Other             The event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventLegacyBoot (
  OUT EFI_EVENT  *LegacyBootEvent
  );

/**
  Create an EFI event in the Legacy Boot Event Group and allows
  the caller to specify a notification function.  
  
  This function abstracts the creation of the Legacy Boot Event.
  The Framework moved from a proprietary to UEFI 2.0 based mechanism.
  This library abstracts the caller from how this event is created to prevent
  to code form having to change with the version of the specification supported.
  If LegacyBootEvent is NULL, then ASSERT().

  @param  NotifyTpl         The task priority level of the event.
  @param  NotifyFunction    The notification function to call when the event is signaled.
  @param  NotifyContext     The content to pass to NotifyFunction when the event is signaled.
  @param  LegacyBootEvent   Returns the EFI event returned from gBS->CreateEvent(Ex).

  @retval EFI_SUCCESS       The event was created.
  @retval Other             The event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventLegacyBootEx (
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,  OPTIONAL
  IN  VOID              *NotifyContext,  OPTIONAL
  OUT EFI_EVENT         *LegacyBootEvent
  );

/**
  Create an EFI event in the Ready To Boot Event Group.

  Prior to UEFI 2.0 this was done via a non-standard UEFI extension, and this library
  abstracts the implementation mechanism of this event from the caller.   
  This function abstracts the creation of the Ready to Boot Event.  The Framework 
  moved from a proprietary to UEFI 2.0-based mechanism.  This library abstracts 
  the caller from how this event is created to prevent the code form having to 
  change with the version of the specification supported.
  If ReadyToBootEvent is NULL, then ASSERT().

  @param  ReadyToBootEvent   Returns the EFI event returned from gBS->CreateEvent(Ex).

  @retval EFI_SUCCESS       The event was created.
  @retval Other             The event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventReadyToBoot (
  OUT EFI_EVENT  *ReadyToBootEvent
  );

/**
  Create an EFI event in the Ready To Boot Event Group and allows
  the caller to specify a notification function.  
  
  This function abstracts the creation of the Ready to Boot Event.
  The Framework moved from a proprietary to UEFI 2.0 based mechanism.
  This library abstracts the caller from how this event is created to prevent
  to code form having to change with the version of the specification supported.
  If ReadyToBootEvent is NULL, then ASSERT().

  @param  NotifyTpl         The task priority level of the event.
  @param  NotifyFunction    The notification function to call when the event is signaled.
  @param  NotifyContext     The content to pass to NotifyFunction when the event is signaled.
  @param  ReadyToBootEvent  Returns the EFI event returned from gBS->CreateEvent(Ex).

  @retval EFI_SUCCESS       The event was created.
  @retval Other             The event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventReadyToBootEx (
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,  OPTIONAL
  IN  VOID              *NotifyContext,  OPTIONAL
  OUT EFI_EVENT         *ReadyToBootEvent
  );

/**
  Initialize a Firmware Volume (FV) Media Device Path node.
  
  The Framework FwVol Device Path changed to conform to the UEFI 2.0 specification.  
  This library function abstracts initializing a device path node.  
  Initialize the MEDIA_FW_VOL_FILEPATH_DEVICE_PATH data structure.  This device 
  path changed in the DXE CIS version 0.92 in a non back ward compatible way to 
  not conflict with the UEFI 2.0 specification.  This function abstracts the 
  differences from the caller.
  If FvDevicePathNode is NULL, then ASSERT().
  If NameGuid is NULL, then ASSERT().
  
  @param  FvDevicePathNode  The pointer to a FV device path node to initialize
  @param  NameGuid          FV file name to use in FvDevicePathNode

**/
VOID
EFIAPI
EfiInitializeFwVolDevicepathNode (
  IN OUT MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvDevicePathNode,
  IN CONST EFI_GUID                         *NameGuid
  );

/**
  Check to see if the Firmware Volume (FV) Media Device Path is valid 
  
  The Framework FwVol Device Path changed to conform to the UEFI 2.0 specification.  
  This library function abstracts validating a device path node.
  Check the MEDIA_FW_VOL_FILEPATH_DEVICE_PATH data structure to see if it's valid.  
  If it is valid, then return the GUID file name from the device path node.  Otherwise, 
  return NULL.  This device path changed in the DXE CIS version 0.92 in a non backward 
  compatible way to not conflict with the UEFI 2.0 specification.  This function abstracts 
  the differences from the caller.
  If FvDevicePathNode is NULL, then ASSERT().

  @param  FvDevicePathNode  The pointer to FV device path to check.

  @retval NULL              FvDevicePathNode is not valid.
  @retval Other             FvDevicePathNode is valid and pointer to NameGuid was returned.

**/
EFI_GUID *
EFIAPI
EfiGetNameGuidFromFwVolDevicePathNode (
  IN CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  *FvDevicePathNode
  );

/** 
  Prints a formatted Unicode string to the console output device specified by 
  ConOut defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted Unicode string to the console output device 
  specified by ConOut in EFI_SYSTEM_TABLE and returns the number of Unicode 
  characters that printed to ConOut.  If the length of the formatted Unicode 
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first 
  PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->ConOut is NULL, then ASSERT().

  @param Format   A null-terminated Unicode format string.
  @param ...      The variable argument list whose contents are accessed based 
                  on the format string specified by Format.
  
  @return Number of Unicode characters printed to ConOut.

**/
UINTN
EFIAPI
Print (
  IN CONST CHAR16  *Format,
  ...
  );

/** 
  Prints a formatted Unicode string to the console output device specified by 
  StdErr defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted Unicode string to the console output device 
  specified by StdErr in EFI_SYSTEM_TABLE and returns the number of Unicode 
  characters that printed to StdErr.  If the length of the formatted Unicode 
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first 
  PcdUefiLibMaxPrintBufferSize characters are sent to StdErr.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->StdErr is NULL, then ASSERT().

  @param Format   A null-terminated Unicode format string.
  @param ...      The variable argument list whose contents are accessed based 
                  on the format string specified by Format.
  
  @return Number of Unicode characters printed to StdErr.

**/
UINTN
EFIAPI
ErrorPrint (
  IN CONST CHAR16  *Format,
  ...
  );

/** 
  Prints a formatted ASCII string to the console output device specified by 
  ConOut defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted ASCII string to the console output device 
  specified by ConOut in EFI_SYSTEM_TABLE and returns the number of ASCII 
  characters that printed to ConOut.  If the length of the formatted ASCII 
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first 
  PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
  If Format is NULL, then ASSERT().
  If gST->ConOut is NULL, then ASSERT().

  @param Format   A null-terminated ASCII format string.
  @param ...      The variable argument list whose contents are accessed based 
                  on the format string specified by Format.
  
  @return Number of ASCII characters printed to ConOut.

**/
UINTN
EFIAPI
AsciiPrint (
  IN CONST CHAR8  *Format,
  ...
  );

/** 
  Prints a formatted ASCII string to the console output device specified by 
  StdErr defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted ASCII string to the console output device 
  specified by StdErr in EFI_SYSTEM_TABLE and returns the number of ASCII 
  characters that printed to StdErr.  If the length of the formatted ASCII 
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first 
  PcdUefiLibMaxPrintBufferSize characters are sent to StdErr.
  If Format is NULL, then ASSERT().
  If gST->StdErr is NULL, then ASSERT().

  @param Format   A null-terminated ASCII format string.
  @param ...      The variable argument list whose contents are accessed based 
                  on the format string specified by Format.
  
  @return Number of ASCII characters printed to ConErr.

**/
UINTN
EFIAPI
AsciiErrorPrint (
  IN CONST CHAR8  *Format,
  ...
  );


/**
  Prints a formatted Unicode string to a graphics console device specified by 
  ConsoleOutputHandle defined in the EFI_SYSTEM_TABLE at the given (X,Y) coordinates.

  This function prints a formatted Unicode string to the graphics console device 
  specified by ConsoleOutputHandle in EFI_SYSTEM_TABLE and returns the number of 
  Unicode characters displayed, not including partial characters that may be clipped 
  by the right edge of the display.  If the length of the formatted Unicode string is
  greater than PcdUefiLibMaxPrintBufferSize, then at most the first 
  PcdUefiLibMaxPrintBufferSize characters are printed.  The EFI_HII_FONT_PROTOCOL
  is used to convert the string to a bitmap using the glyphs registered with the 
  HII database.  No wrapping is performed, so any portions of the string the fall
  outside the active display region will not be displayed.

  If a graphics console device is not associated with the ConsoleOutputHandle 
  defined in the EFI_SYSTEM_TABLE then no string is printed, and 0 is returned.
  If the EFI_HII_FONT_PROTOCOL is not present in the handle database, then no 
  string is printed, and 0 is returned.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->ConsoleOutputHandle is NULL, then ASSERT().

  @param  PointX       X coordinate to print the string.
  @param  PointY       Y coordinate to print the string.
  @param  ForeGround   The foreground color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the foreground color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  BackGround   The background color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL, 
                       then the background color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Format       A null-terminated Unicode format string.  See Print Library 
                       for the supported format string syntax.
  @param  ...          Variable argument list whose contents are accessed based on 
                       the format string specified by Format.         

  @return  The number of Unicode characters printed.

**/
UINTN
EFIAPI
PrintXY (
  IN UINTN                            PointX,
  IN UINTN                            PointY,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *ForeGround, OPTIONAL
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *BackGround, OPTIONAL
  IN CONST CHAR16                     *Format,
  ...
  );

/**
  Prints a formatted ASCII string to a graphics console device specified by 
  ConsoleOutputHandle defined in the EFI_SYSTEM_TABLE at the given (X,Y) coordinates.

  This function prints a formatted ASCII string to the graphics console device 
  specified by ConsoleOutputHandle in EFI_SYSTEM_TABLE and returns the number of 
  ASCII characters displayed, not including partial characters that may be clipped 
  by the right edge of the display.  If the length of the formatted ASCII string is
  greater than PcdUefiLibMaxPrintBufferSize, then at most the first 
  PcdUefiLibMaxPrintBufferSize characters are printed.  The EFI_HII_FONT_PROTOCOL
  is used to convert the string to a bitmap using the glyphs registered with the 
  HII database.  No wrapping is performed, so any portions of the string the fall
  outside the active display region will not be displayed.

  If a graphics console device is not associated with the ConsoleOutputHandle 
  defined in the EFI_SYSTEM_TABLE then no string is printed, and 0 is returned.
  If the EFI_HII_FONT_PROTOCOL is not present in the handle database, then no 
  string is printed, and 0 is returned.
  If Format is NULL, then ASSERT().
  If gST->ConsoleOutputHandle is NULL, then ASSERT().

  @param  PointX       X coordinate to print the string.
  @param  PointY       Y coordinate to print the string.
  @param  ForeGround   The foreground color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL,
                       then the foreground color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  BackGround   The background color of the string being printed.  This is
                       an optional parameter that may be NULL.  If it is NULL, 
                       then the background color of the current ConOut device
                       in the EFI_SYSTEM_TABLE is used.
  @param  Format       A null-terminated ASCII format string.  See Print Library 
                       for the supported format string syntax.
  @param  ...          The variable argument list whose contents are accessed based on 
                       the format string specified by Format.         

  @return  The number of ASCII characters printed.

**/
UINTN
EFIAPI
AsciiPrintXY (
  IN UINTN                            PointX,
  IN UINTN                            PointY,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *ForeGround, OPTIONAL
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *BackGround, OPTIONAL
  IN CONST CHAR8                      *Format,
  ...
  );

/**
  Installs and completes the initialization of a Driver Binding Protocol instance.
  
  Installs the Driver Binding Protocol specified by DriverBinding onto the handle
  specified by DriverBindingHandle. If DriverBindingHandle is NULL, then DriverBinding
  is installed onto a newly created handle. DriverBindingHandle is typically the same
  as the driver's ImageHandle, but it can be different if the driver produces multiple
  Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If DriverBinding can not be installed onto a handle, then ASSERT().

  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.

  @retval EFI_SUCCESS           The protocol installation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough system resources to install the protocol.
  @retval Others                Status from gBS->InstallMultipleProtocolInterfaces().

**/
EFI_STATUS
EFIAPI
EfiLibInstallDriverBinding (
  IN CONST EFI_HANDLE             ImageHandle,
  IN CONST EFI_SYSTEM_TABLE       *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
  IN EFI_HANDLE                   DriverBindingHandle
  );


/**
  Installs and completes the initialization of a Driver Binding Protocol instance and
  optionally installs the Component Name, Driver Configuration and Driver Diagnostics Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the
  optional Component Name, optional Driver Configure and optional Driver Diagnostic
  Protocols onto the driver's DriverBindingHandle. If DriverBindingHandle is NULL,
  then the protocols are installed onto a newly created handle. DriverBindingHandle
  is typically the same as the driver's ImageHandle, but it can be different if the
  driver produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().
  
  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.
  @param  ComponentName        A Component Name Protocol instance that this driver is producing.
  @param  DriverConfiguration  A Driver Configuration Protocol instance that this driver is producing.
  @param  DriverDiagnostics    A Driver Diagnostics Protocol instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in the pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallAllDriverProtocols (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration, OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics    OPTIONAL
  );



/**
  Installs Driver Binding Protocol with optional Component Name and Component Name 2 Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the
  optional Component Name and optional Component Name 2 protocols onto the driver's
  DriverBindingHandle.  If DriverBindingHandle is NULL, then the protocols are installed
  onto a newly created handle.  DriverBindingHandle is typically the same as the driver's
  ImageHandle, but it can be different if the driver produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().

  @param  ImageHandle          The image handle of the driver.
  @param  SystemTable          The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding        A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle  The handle that DriverBinding is to be installed onto.  If this
                               parameter is NULL, then a new handle is created.
  @param  ComponentName        A Component Name Protocol instance that this driver is producing.
  @param  ComponentName2       A Component Name 2 Protocol instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallDriverBindingComponentName2 (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,       OPTIONAL
  IN CONST EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName2       OPTIONAL
  );


/**
  Installs Driver Binding Protocol with optional Component Name, Component Name 2, Driver
  Configuration, Driver Configuration 2, Driver Diagnostics, and Driver Diagnostics 2 Protocols.

  Initializes a driver by installing the Driver Binding Protocol together with the optional
  Component Name, optional Component Name 2, optional Driver Configuration, optional Driver Configuration 2,
  optional Driver Diagnostic, and optional Driver Diagnostic 2 Protocols onto the driver's DriverBindingHandle.
  DriverBindingHandle is typically the same as the driver's ImageHandle, but it can be different if the driver
  produces multiple Driver Binding Protocols. 
  If DriverBinding is NULL, then ASSERT(). 
  If the installation fails, then ASSERT().


  @param  ImageHandle           The image handle of the driver.
  @param  SystemTable           The EFI System Table that was passed to the driver's entry point.
  @param  DriverBinding         A Driver Binding Protocol instance that this driver is producing.
  @param  DriverBindingHandle   The handle that DriverBinding is to be installed onto.  If this
                                parameter is NULL, then a new handle is created.
  @param  ComponentName         A Component Name Protocol instance that this driver is producing.
  @param  ComponentName2        A Component Name 2 Protocol instance that this driver is producing.
  @param  DriverConfiguration   A Driver Configuration Protocol instance that this driver is producing.
  @param  DriverConfiguration2  A Driver Configuration Protocol 2 instance that this driver is producing.
  @param  DriverDiagnostics     A Driver Diagnostics Protocol instance that this driver is producing.
  @param  DriverDiagnostics2    A Driver Diagnostics Protocol 2 instance that this driver is producing.

  @retval EFI_SUCCESS           The protocol installation completed successfully.
  @retval EFI_OUT_OF_RESOURCES  There was not enough memory in pool to install all the protocols.

**/
EFI_STATUS
EFIAPI
EfiLibInstallAllDriverProtocols2 (
  IN CONST EFI_HANDLE                         ImageHandle,
  IN CONST EFI_SYSTEM_TABLE                   *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL              *DriverBinding,
  IN EFI_HANDLE                               DriverBindingHandle,
  IN CONST EFI_COMPONENT_NAME_PROTOCOL        *ComponentName,        OPTIONAL
  IN CONST EFI_COMPONENT_NAME2_PROTOCOL       *ComponentName2,       OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION_PROTOCOL  *DriverConfiguration,  OPTIONAL
  IN CONST EFI_DRIVER_CONFIGURATION2_PROTOCOL *DriverConfiguration2, OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS_PROTOCOL    *DriverDiagnostics,    OPTIONAL
  IN CONST EFI_DRIVER_DIAGNOSTICS2_PROTOCOL   *DriverDiagnostics2    OPTIONAL
  );

/** 
  Appends a formatted Unicode string to a Null-terminated Unicode string
 
  This function appends a formatted Unicode string to the Null-terminated 
  Unicode string specified by String.   String is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using 
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.
 
  If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is NULL, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().
 
  @param[in] String         A Null-terminated Unicode string.
  @param[in] FormatString   A Null-terminated Unicode format string.
  @param[in]  Marker        VA_LIST marker for the variable argument list.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted 
                  string appended to String.
**/
CHAR16*
EFIAPI
CatVSPrint (
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST       Marker
  );

/** 
  Appends a formatted Unicode string to a Null-terminated Unicode string
 
  This function appends a formatted Unicode string to the Null-terminated 
  Unicode string specified by String.   String is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using 
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.
 
  If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is NULL, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().
 
  @param[in] String         A Null-terminated Unicode string.
  @param[in] FormatString   A Null-terminated Unicode format string.
  @param[in] ...            The variable argument list whose contents are 
                            accessed based on the format string specified by 
                            FormatString.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted 
                  string appended to String.
**/
CHAR16 *
EFIAPI
CatSPrint (
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  ...
  );

#endif
