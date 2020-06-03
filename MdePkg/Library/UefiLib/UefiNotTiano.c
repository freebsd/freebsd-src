/** @file
  Library functions that abstract areas of conflict between framework and UEFI 2.0.

  Help Port Framework code that has conflicts with UEFI 2.0 by hiding the
  old conflicts with library functions and supporting implementations of the old
  (EDK/EFI 1.10) and new (EDK II/UEFI 2.0) way. This module is a DXE driver as
  it contains DXE enum extensions for EFI event services.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/



#include "UefiLibInternal.h"

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

  @retval EFI_SUCCESS       Event was created.
  @retval Other             Event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventLegacyBoot (
  OUT EFI_EVENT  *LegacyBootEvent
  )
{
  return EfiCreateEventLegacyBootEx (
           TPL_CALLBACK,
           EfiEventEmptyFunction,
           NULL,
           LegacyBootEvent
           );
}

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

  @retval EFI_SUCCESS       Event was created.
  @retval Other             Event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventLegacyBootEx (
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,  OPTIONAL
  IN  VOID              *NotifyContext,  OPTIONAL
  OUT EFI_EVENT         *LegacyBootEvent
  )
{
  EFI_STATUS        Status;
  EFI_EVENT_NOTIFY  WorkerNotifyFunction;

  ASSERT (LegacyBootEvent != NULL);

  if (gST->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
    DEBUG ((EFI_D_ERROR, "EFI1.1 can't support LegacyBootEvent!"));
    ASSERT (FALSE);

    return EFI_UNSUPPORTED;
  } else {
    //
    // For UEFI 2.0 and the future use an Event Group
    //
    if (NotifyFunction == NULL) {
      //
      // CreateEventEx will check NotifyFunction is NULL or not and return error.
      // Use dummy routine for the case NotifyFunction is NULL.
      //
      WorkerNotifyFunction = EfiEventEmptyFunction;
    } else {
      WorkerNotifyFunction = NotifyFunction;
    }
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    NotifyTpl,
                    WorkerNotifyFunction,
                    NotifyContext,
                    &gEfiEventLegacyBootGuid,
                    LegacyBootEvent
                    );
  }

  return Status;
}

/**
  Create an EFI event in the Ready To Boot Event Group.

  Prior to UEFI 2.0 this was done via a non-standard UEFI extension, and this library
  abstracts the implementation mechanism of this event from the caller.
  This function abstracts the creation of the Ready to Boot Event.  The Framework
  moved from a proprietary to UEFI 2.0-based mechanism.  This library abstracts
  the caller from how this event is created to prevent the code form having to
  change with the version of the specification supported.
  If ReadyToBootEvent is NULL, then ASSERT().

  @param  ReadyToBootEvent  Returns the EFI event returned from gBS->CreateEvent(Ex).

  @retval EFI_SUCCESS       Event was created.
  @retval Other             Event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventReadyToBoot (
  OUT EFI_EVENT  *ReadyToBootEvent
  )
{
  return EfiCreateEventReadyToBootEx (
           TPL_CALLBACK,
           EfiEventEmptyFunction,
           NULL,
           ReadyToBootEvent
           );
}

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

  @retval EFI_SUCCESS       Event was created.
  @retval Other             Event was not created.

**/
EFI_STATUS
EFIAPI
EfiCreateEventReadyToBootEx (
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,  OPTIONAL
  IN  VOID              *NotifyContext,  OPTIONAL
  OUT EFI_EVENT         *ReadyToBootEvent
  )
{
  EFI_STATUS        Status;
  EFI_EVENT_NOTIFY  WorkerNotifyFunction;

  ASSERT (ReadyToBootEvent != NULL);

  if (gST->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
    DEBUG ((EFI_D_ERROR, "EFI1.1 can't support ReadyToBootEvent!"));
    ASSERT (FALSE);

    return EFI_UNSUPPORTED;
  } else {
    //
    // For UEFI 2.0 and the future use an Event Group
    //
    if (NotifyFunction == NULL) {
      //
      // CreateEventEx will check NotifyFunction is NULL or not and return error.
      // Use dummy routine for the case NotifyFunction is NULL.
      //
      WorkerNotifyFunction = EfiEventEmptyFunction;
    } else {
      WorkerNotifyFunction = NotifyFunction;
    }
    Status = gBS->CreateEventEx (
                    EVT_NOTIFY_SIGNAL,
                    NotifyTpl,
                    WorkerNotifyFunction,
                    NotifyContext,
                    &gEfiEventReadyToBootGuid,
                    ReadyToBootEvent
                    );
  }

  return Status;
}


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
  )
{
  EFI_STATUS    Status;
  EFI_EVENT     ReadyToBootEvent;

  Status = EfiCreateEventReadyToBoot (&ReadyToBootEvent);
  if (!EFI_ERROR (Status)) {
    gBS->SignalEvent (ReadyToBootEvent);
    gBS->CloseEvent (ReadyToBootEvent);
  }
}

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
  )
{
  EFI_STATUS    Status;
  EFI_EVENT     LegacyBootEvent;

  Status = EfiCreateEventLegacyBoot (&LegacyBootEvent);
  if (!EFI_ERROR (Status)) {
    gBS->SignalEvent (LegacyBootEvent);
    gBS->CloseEvent (LegacyBootEvent);
  }
}


/**
  Check to see if the Firmware Volume (FV) Media Device Path is valid

  The Framework FwVol Device Path changed to conform to the UEFI 2.0 specification.
  This library function abstracts validating a device path node.
  Check the MEDIA_FW_VOL_FILEPATH_DEVICE_PATH data structure to see if it's valid.
  If it is valid, then return the GUID file name from the device path node.  Otherwise,
  return NULL.  This device path changed in the DXE CIS version 0.92 in a non back ward
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
  )
{
  ASSERT (FvDevicePathNode != NULL);

  if (DevicePathType (&FvDevicePathNode->Header) == MEDIA_DEVICE_PATH &&
      DevicePathSubType (&FvDevicePathNode->Header) == MEDIA_PIWG_FW_FILE_DP) {
    return (EFI_GUID *) &FvDevicePathNode->FvFileName;
  }

  return NULL;
}


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
  )
{
  ASSERT (FvDevicePathNode != NULL);
  ASSERT (NameGuid          != NULL);

  //
  // Use the new Device path that does not conflict with the UEFI
  //
  FvDevicePathNode->Header.Type     = MEDIA_DEVICE_PATH;
  FvDevicePathNode->Header.SubType  = MEDIA_PIWG_FW_FILE_DP;
  SetDevicePathNodeLength (&FvDevicePathNode->Header, sizeof (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH));

  CopyGuid (&FvDevicePathNode->FvFileName, NameGuid);
}

