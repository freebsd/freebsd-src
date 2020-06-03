/** @file
  Device Path services. The thing to remember is device paths are built out of
  nodes. The device path is terminated by an end node that is length
  sizeof(EFI_DEVICE_PATH_PROTOCOL). That would be why there is sizeof(EFI_DEVICE_PATH_PROTOCOL)
  all over this file.

  The only place where multi-instance device paths are supported is in
  environment varibles. Multi-instance device paths should never be placed
  on a Handle.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include "UefiDevicePathLib.h"

GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_UTILITIES_PROTOCOL *mDevicePathLibDevicePathUtilities = NULL;
GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *mDevicePathLibDevicePathToText    = NULL;
GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *mDevicePathLibDevicePathFromText  = NULL;

/**
  The constructor function caches the pointer to DevicePathUtilites protocol,
  DevicePathToText protocol and DevicePathFromText protocol.

  The constructor function locates these three protocols from protocol database.
  It will caches the pointer to local protocol instance if that operation fails
  and it will always return EFI_SUCCESS.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiDevicePathLibOptionalDevicePathProtocolConstructor (
  IN      EFI_HANDLE                ImageHandle,
  IN      EFI_SYSTEM_TABLE          *SystemTable
  )
{
  EFI_STATUS                        Status;

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathUtilitiesProtocolGuid,
                  NULL,
                  (VOID**) &mDevicePathLibDevicePathUtilities
                  );
  ASSERT_EFI_ERROR (Status);
  ASSERT (mDevicePathLibDevicePathUtilities != NULL);
  return Status;
}

/**
  Returns the size of a device path in bytes.

  This function returns the size, in bytes, of the device path data structure
  specified by DevicePath including the end of device path node.
  If DevicePath is NULL or invalid, then 0 is returned.

  @param  DevicePath  A pointer to a device path data structure.

  @retval 0           If DevicePath is NULL or invalid.
  @retval Others      The size of a device path in bytes.

**/
UINTN
EFIAPI
GetDevicePathSize (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->GetDevicePathSize (DevicePath);
  } else {
    return UefiDevicePathLibGetDevicePathSize (DevicePath);
  }
}

/**
  Creates a new copy of an existing device path.

  This function allocates space for a new copy of the device path specified by DevicePath.
  If DevicePath is NULL, then NULL is returned.  If the memory is successfully
  allocated, then the contents of DevicePath are copied to the newly allocated
  buffer, and a pointer to that buffer is returned.  Otherwise, NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  @param  DevicePath    A pointer to a device path data structure.

  @retval NULL          DevicePath is NULL or invalid.
  @retval Others        A pointer to the duplicated device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
DuplicateDevicePath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->DuplicateDevicePath (DevicePath);
  } else {
    return UefiDevicePathLibDuplicateDevicePath (DevicePath);
  }
}

/**
  Creates a new device path by appending a second device path to a first device path.

  This function creates a new device path by appending a copy of SecondDevicePath
  to a copy of FirstDevicePath in a newly allocated buffer.  Only the end-of-device-path
  device node from SecondDevicePath is retained. The newly created device path is
  returned. If FirstDevicePath is NULL, then it is ignored, and a duplicate of
  SecondDevicePath is returned.  If SecondDevicePath is NULL, then it is ignored,
  and a duplicate of FirstDevicePath is returned. If both FirstDevicePath and
  SecondDevicePath are NULL, then a copy of an end-of-device-path is returned.

  If there is not enough memory for the newly allocated buffer, then NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  @param  FirstDevicePath            A pointer to a device path data structure.
  @param  SecondDevicePath           A pointer to a device path data structure.

  @retval NULL      If there is not enough memory for the newly allocated buffer.
  @retval NULL      If FirstDevicePath or SecondDevicePath is invalid.
  @retval Others    A pointer to the new device path if success.
                    Or a copy an end-of-device-path if both FirstDevicePath and SecondDevicePath are NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
AppendDevicePath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *FirstDevicePath,  OPTIONAL
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *SecondDevicePath  OPTIONAL
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->AppendDevicePath (FirstDevicePath, SecondDevicePath);
  } else {
    return UefiDevicePathLibAppendDevicePath (FirstDevicePath, SecondDevicePath);
  }
}

/**
  Creates a new path by appending the device node to the device path.

  This function creates a new device path by appending a copy of the device node
  specified by DevicePathNode to a copy of the device path specified by DevicePath
  in an allocated buffer. The end-of-device-path device node is moved after the
  end of the appended device node.
  If DevicePathNode is NULL then a copy of DevicePath is returned.
  If DevicePath is NULL then a copy of DevicePathNode, followed by an end-of-device
  path device node is returned.
  If both DevicePathNode and DevicePath are NULL then a copy of an end-of-device-path
  device node is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  DevicePath                 A pointer to a device path data structure.
  @param  DevicePathNode             A pointer to a single device path node.

  @retval NULL      If there is not enough memory for the new device path.
  @retval Others    A pointer to the new device path if success.
                    A copy of DevicePathNode followed by an end-of-device-path node
                    if both FirstDevicePath and SecondDevicePath are NULL.
                    A copy of an end-of-device-path node if both FirstDevicePath
                    and SecondDevicePath are NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
AppendDevicePathNode (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,     OPTIONAL
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathNode  OPTIONAL
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->AppendDeviceNode (DevicePath, DevicePathNode);
  } else {
    return UefiDevicePathLibAppendDevicePathNode (DevicePath, DevicePathNode);
  }
}

/**
  Creates a new device path by appending the specified device path instance to the specified device
  path.

  This function creates a new device path by appending a copy of the device path
  instance specified by DevicePathInstance to a copy of the device path specified
  by DevicePath in a allocated buffer.
  The end-of-device-path device node is moved after the end of the appended device
  path instance and a new end-of-device-path-instance node is inserted between.
  If DevicePath is NULL, then a copy if DevicePathInstance is returned.
  If DevicePathInstance is NULL, then NULL is returned.
  If DevicePath or DevicePathInstance is invalid, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  DevicePath                 A pointer to a device path data structure.
  @param  DevicePathInstance         A pointer to a device path instance.

  @return A pointer to the new device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
AppendDevicePathInstance (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath,        OPTIONAL
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePathInstance OPTIONAL
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->AppendDevicePathInstance (DevicePath, DevicePathInstance);
  } else {
    return UefiDevicePathLibAppendDevicePathInstance (DevicePath, DevicePathInstance);
  }
}

/**
  Creates a copy of the current device path instance and returns a pointer to the next device path
  instance.

  This function creates a copy of the current device path instance. It also updates
  DevicePath to point to the next device path instance in the device path (or NULL
  if no more) and updates Size to hold the size of the device path instance copy.
  If DevicePath is NULL, then NULL is returned.
  If DevicePath points to a invalid device path, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.
  If Size is NULL, then ASSERT().

  @param  DevicePath                 On input, this holds the pointer to the current
                                     device path instance. On output, this holds
                                     the pointer to the next device path instance
                                     or NULL if there are no more device path
                                     instances in the device path pointer to a
                                     device path data structure.
  @param  Size                       On output, this holds the size of the device
                                     path instance, in bytes or zero, if DevicePath
                                     is NULL.

  @return A pointer to the current device path instance.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
GetNextDevicePathInstance (
  IN OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
  OUT UINTN                          *Size
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->GetNextDevicePathInstance (DevicePath, Size);
  } else {
    return UefiDevicePathLibGetNextDevicePathInstance (DevicePath, Size);
  }
}

/**
  Creates a device node.

  This function creates a new device node in a newly allocated buffer of size
  NodeLength and initializes the device path node header with NodeType and NodeSubType.
  The new device path node is returned.
  If NodeLength is smaller than a device path header, then NULL is returned.
  If there is not enough memory to allocate space for the new device path, then
  NULL is returned.
  The memory is allocated from EFI boot services memory. It is the responsibility
  of the caller to free the memory allocated.

  @param  NodeType                   The device node type for the new device node.
  @param  NodeSubType                The device node sub-type for the new device node.
  @param  NodeLength                 The length of the new device node.

  @return The new device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
CreateDeviceNode (
  IN UINT8                           NodeType,
  IN UINT8                           NodeSubType,
  IN UINT16                          NodeLength
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->CreateDeviceNode (NodeType, NodeSubType, NodeLength);
  } else {
    return UefiDevicePathLibCreateDeviceNode (NodeType, NodeSubType, NodeLength);
  }
}

/**
  Determines if a device path is single or multi-instance.

  This function returns TRUE if the device path specified by DevicePath is
  multi-instance.
  Otherwise, FALSE is returned.
  If DevicePath is NULL or invalid, then FALSE is returned.

  @param  DevicePath                 A pointer to a device path data structure.

  @retval  TRUE                      DevicePath is multi-instance.
  @retval  FALSE                     DevicePath is not multi-instance, or DevicePath
                                     is NULL or invalid.

**/
BOOLEAN
EFIAPI
IsDevicePathMultiInstance (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  if (mDevicePathLibDevicePathUtilities != NULL) {
    return mDevicePathLibDevicePathUtilities->IsDevicePathMultiInstance (DevicePath);
  } else {
    return UefiDevicePathLibIsDevicePathMultiInstance (DevicePath);
  }
}

/**
  Locate and return the protocol instance identified by the ProtocolGuid.

  @param ProtocolGuid     The GUID of the protocol.

  @return A pointer to the protocol instance or NULL when absent.
**/
VOID *
UefiDevicePathLibLocateProtocol (
  EFI_GUID                         *ProtocolGuid
  )
{
  EFI_STATUS Status;
  VOID       *Protocol;
  Status = gBS->LocateProtocol (
                  ProtocolGuid,
                  NULL,
                  (VOID**) &Protocol
                  );
  if (EFI_ERROR (Status)) {
    return NULL;
  } else {
    return Protocol;
  }
}

/**
  Converts a device node to its string representation.

  @param DeviceNode        A Pointer to the device node to be converted.
  @param DisplayOnly       If DisplayOnly is TRUE, then the shorter text representation
                           of the display node is used, where applicable. If DisplayOnly
                           is FALSE, then the longer text representation of the display node
                           is used.
  @param AllowShortcuts    If AllowShortcuts is TRUE, then the shortcut forms of text
                           representation for a device node can be used, where applicable.

  @return A pointer to the allocated text representation of the device node or NULL if DeviceNode
          is NULL or there was insufficient memory.

**/
CHAR16 *
EFIAPI
ConvertDeviceNodeToText (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DeviceNode,
  IN BOOLEAN                         DisplayOnly,
  IN BOOLEAN                         AllowShortcuts
  )
{
  if (mDevicePathLibDevicePathToText == NULL) {
    mDevicePathLibDevicePathToText = UefiDevicePathLibLocateProtocol (&gEfiDevicePathToTextProtocolGuid);
  }
  if (mDevicePathLibDevicePathToText != NULL) {
    return mDevicePathLibDevicePathToText->ConvertDeviceNodeToText (DeviceNode, DisplayOnly, AllowShortcuts);
  }

  return UefiDevicePathLibConvertDeviceNodeToText (DeviceNode, DisplayOnly, AllowShortcuts);
}

/**
  Converts a device path to its text representation.

  @param DevicePath      A Pointer to the device to be converted.
  @param DisplayOnly     If DisplayOnly is TRUE, then the shorter text representation
                         of the display node is used, where applicable. If DisplayOnly
                         is FALSE, then the longer text representation of the display node
                         is used.
  @param AllowShortcuts  If AllowShortcuts is TRUE, then the shortcut forms of text
                         representation for a device node can be used, where applicable.

  @return A pointer to the allocated text representation of the device path or
          NULL if DeviceNode is NULL or there was insufficient memory.

**/
CHAR16 *
EFIAPI
ConvertDevicePathToText (
  IN CONST EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN BOOLEAN                          DisplayOnly,
  IN BOOLEAN                          AllowShortcuts
  )
{
  if (mDevicePathLibDevicePathToText == NULL) {
    mDevicePathLibDevicePathToText = UefiDevicePathLibLocateProtocol (&gEfiDevicePathToTextProtocolGuid);
  }
  if (mDevicePathLibDevicePathToText != NULL) {
    return mDevicePathLibDevicePathToText->ConvertDevicePathToText (DevicePath, DisplayOnly, AllowShortcuts);
  }

  return UefiDevicePathLibConvertDevicePathToText (DevicePath, DisplayOnly, AllowShortcuts);
}

/**
  Convert text to the binary representation of a device node.

  @param TextDeviceNode  TextDeviceNode points to the text representation of a device
                         node. Conversion starts with the first character and continues
                         until the first non-device node character.

  @return A pointer to the EFI device node or NULL if TextDeviceNode is NULL or there was
          insufficient memory or text unsupported.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
ConvertTextToDeviceNode (
  IN CONST CHAR16 *TextDeviceNode
  )
{
  if (mDevicePathLibDevicePathFromText == NULL) {
    mDevicePathLibDevicePathFromText = UefiDevicePathLibLocateProtocol (&gEfiDevicePathFromTextProtocolGuid);
  }
  if (mDevicePathLibDevicePathFromText != NULL) {
    return mDevicePathLibDevicePathFromText->ConvertTextToDeviceNode (TextDeviceNode);
  }

  return UefiDevicePathLibConvertTextToDeviceNode (TextDeviceNode);
}

/**
  Convert text to the binary representation of a device path.


  @param TextDevicePath  TextDevicePath points to the text representation of a device
                         path. Conversion starts with the first character and continues
                         until the first non-device node character.

  @return A pointer to the allocated device path or NULL if TextDeviceNode is NULL or
          there was insufficient memory.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
ConvertTextToDevicePath (
  IN CONST CHAR16 *TextDevicePath
  )
{
  if (mDevicePathLibDevicePathFromText == NULL) {
    mDevicePathLibDevicePathFromText = UefiDevicePathLibLocateProtocol (&gEfiDevicePathFromTextProtocolGuid);
  }
  if (mDevicePathLibDevicePathFromText != NULL) {
    return mDevicePathLibDevicePathFromText->ConvertTextToDevicePath (TextDevicePath);
  }

  return UefiDevicePathLibConvertTextToDevicePath (TextDevicePath);
}

