/** @file
  Library instance that implement UEFI Device Path Library class based on protocol
  gEfiDevicePathUtilitiesProtocolGuid.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#include <Uefi.h>

#include <Protocol/DevicePathUtilities.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathFromText.h>

#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>

GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_UTILITIES_PROTOCOL *mDevicePathLibDevicePathUtilities = NULL;
GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *mDevicePathLibDevicePathToText    = NULL;
GLOBAL_REMOVE_IF_UNREFERENCED EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *mDevicePathLibDevicePathFromText  = NULL;

//
// Template for an end-of-device path node.
//
GLOBAL_REMOVE_IF_UNREFERENCED CONST EFI_DEVICE_PATH_PROTOCOL  mUefiDevicePathLibEndDevicePath = {
  END_DEVICE_PATH_TYPE,
  END_ENTIRE_DEVICE_PATH_SUBTYPE,
  {
    END_DEVICE_PATH_LENGTH,
    0
  }
};

/**
  The constructor function caches the pointer to DevicePathUtilites protocol.

  The constructor function locates DevicePathUtilities protocol from protocol database.
  It will ASSERT() if that operation fails and it will always return EFI_SUCCESS.

  @param  ImageHandle   The firmware allocated handle for the EFI image.
  @param  SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
DevicePathLibConstructor (
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
  Determine whether a given device path is valid.
  If DevicePath is NULL, then ASSERT().

  @param  DevicePath  A pointer to a device path data structure.
  @param  MaxSize     The maximum size of the device path data structure.

  @retval TRUE        DevicePath is valid.
  @retval FALSE       The length of any node node in the DevicePath is less
                      than sizeof (EFI_DEVICE_PATH_PROTOCOL).
  @retval FALSE       If MaxSize is not zero, the size of the DevicePath
                      exceeds MaxSize.
  @retval FALSE       If PcdMaximumDevicePathNodeCount is not zero, the node
                      count of the DevicePath exceeds PcdMaximumDevicePathNodeCount.
**/
BOOLEAN
EFIAPI
IsDevicePathValid (
  IN CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
  IN       UINTN                    MaxSize
  )
{
  UINTN Count;
  UINTN Size;
  UINTN NodeLength;

  ASSERT (DevicePath != NULL);

  if (MaxSize == 0) {
    MaxSize = MAX_UINTN;
  }

  //
  // Validate the input size big enough to touch the first node.
  //
  if (MaxSize < sizeof (EFI_DEVICE_PATH_PROTOCOL)) {
    return FALSE;
  }

  for (Count = 0, Size = 0; !IsDevicePathEnd (DevicePath); DevicePath = NextDevicePathNode (DevicePath)) {
    NodeLength = DevicePathNodeLength (DevicePath);
    if (NodeLength < sizeof (EFI_DEVICE_PATH_PROTOCOL)) {
      return FALSE;
    }

    if (NodeLength > MAX_UINTN - Size) {
      return FALSE;
    }
    Size += NodeLength;

    //
    // Validate next node before touch it.
    //
    if (Size > MaxSize - END_DEVICE_PATH_LENGTH ) {
      return FALSE;
    }

    if (PcdGet32 (PcdMaximumDevicePathNodeCount) > 0) {
      Count++;
      if (Count >= PcdGet32 (PcdMaximumDevicePathNodeCount)) {
        return FALSE;
      }
    }

    //
    // FilePath must be a NULL-terminated string.
    //
    if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH &&
        DevicePathSubType (DevicePath) == MEDIA_FILEPATH_DP &&
        *(CHAR16 *)((UINT8 *)DevicePath + NodeLength - 2) != 0) {
      return FALSE;
    }
  }

  //
  // Only return TRUE when the End Device Path node is valid.
  //
  return (BOOLEAN) (DevicePathNodeLength (DevicePath) == END_DEVICE_PATH_LENGTH);
}

/**
  Returns the Type field of a device path node.

  Returns the Type field of the device path node specified by Node.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @return The Type field of the device path node specified by Node.

**/
UINT8
EFIAPI
DevicePathType (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return ((EFI_DEVICE_PATH_PROTOCOL *)(Node))->Type;
}

/**
  Returns the SubType field of a device path node.

  Returns the SubType field of the device path node specified by Node.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @return The SubType field of the device path node specified by Node.

**/
UINT8
EFIAPI
DevicePathSubType (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return ((EFI_DEVICE_PATH_PROTOCOL *)(Node))->SubType;
}

/**
  Returns the 16-bit Length field of a device path node.

  Returns the 16-bit Length field of the device path node specified by Node.
  Node is not required to be aligned on a 16-bit boundary, so it is recommended
  that a function such as ReadUnaligned16() be used to extract the contents of
  the Length field.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @return The 16-bit Length field of the device path node specified by Node.

**/
UINTN
EFIAPI
DevicePathNodeLength (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return ReadUnaligned16 ((UINT16 *)&((EFI_DEVICE_PATH_PROTOCOL *)(Node))->Length[0]);
}

/**
  Returns a pointer to the next node in a device path.

  Returns a pointer to the device path node that follows the device path node
  specified by Node.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @return a pointer to the device path node that follows the device path node
  specified by Node.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
NextDevicePathNode (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)(Node) + DevicePathNodeLength(Node));
}

/**
  Determines if a device path node is an end node of a device path.
  This includes nodes that are the end of a device path instance and nodes that
  are the end of an entire device path.

  Determines if the device path node specified by Node is an end node of a device path.
  This includes nodes that are the end of a device path instance and nodes that are the
  end of an entire device path.  If Node represents an end node of a device path,
  then TRUE is returned.  Otherwise, FALSE is returned.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @retval TRUE      The device path node specified by Node is an end node of a device path.
  @retval FALSE     The device path node specified by Node is not an end node of
                    a device path.

**/
BOOLEAN
EFIAPI
IsDevicePathEndType (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return (BOOLEAN) (DevicePathType (Node) == END_DEVICE_PATH_TYPE);
}

/**
  Determines if a device path node is an end node of an entire device path.

  Determines if a device path node specified by Node is an end node of an entire
  device path.
  If Node represents the end of an entire device path, then TRUE is returned.
  Otherwise, FALSE is returned.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @retval TRUE      The device path node specified by Node is the end of an entire device path.
  @retval FALSE     The device path node specified by Node is not the end of an entire device path.

**/
BOOLEAN
EFIAPI
IsDevicePathEnd (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return (BOOLEAN) (IsDevicePathEndType (Node) && DevicePathSubType(Node) == END_ENTIRE_DEVICE_PATH_SUBTYPE);
}

/**
  Determines if a device path node is an end node of a device path instance.

  Determines if a device path node specified by Node is an end node of a device
  path instance.
  If Node represents the end of a device path instance, then TRUE is returned.
  Otherwise, FALSE is returned.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @retval TRUE      The device path node specified by Node is the end of a device
  path instance.
  @retval FALSE     The device path node specified by Node is not the end of a
  device path instance.

**/
BOOLEAN
EFIAPI
IsDevicePathEndInstance (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return (BOOLEAN) (IsDevicePathEndType (Node) && DevicePathSubType(Node) == END_INSTANCE_DEVICE_PATH_SUBTYPE);
}

/**
  Sets the length, in bytes, of a device path node.

  Sets the length of the device path node specified by Node to the value specified
  by NodeLength.  NodeLength is returned.  Node is not required to be aligned on
  a 16-bit boundary, so it is recommended that a function such as WriteUnaligned16()
  be used to set the contents of the Length field.

  If Node is NULL, then ASSERT().
  If NodeLength >= SIZE_64KB, then ASSERT().
  If NodeLength < sizeof (EFI_DEVICE_PATH_PROTOCOL), then ASSERT().

  @param  Node      A pointer to a device path node data structure.
  @param  Length    The length, in bytes, of the device path node.

  @return Length

**/
UINT16
EFIAPI
SetDevicePathNodeLength (
  IN OUT VOID  *Node,
  IN UINTN     Length
  )
{
  ASSERT (Node != NULL);
  ASSERT ((Length >= sizeof (EFI_DEVICE_PATH_PROTOCOL)) && (Length < SIZE_64KB));
  return WriteUnaligned16 ((UINT16 *)&((EFI_DEVICE_PATH_PROTOCOL *)(Node))->Length[0], (UINT16)(Length));
}

/**
  Fills in all the fields of a device path node that is the end of an entire device path.

  Fills in all the fields of a device path node specified by Node so Node represents
  the end of an entire device path.  The Type field of Node is set to
  END_DEVICE_PATH_TYPE, the SubType field of Node is set to
  END_ENTIRE_DEVICE_PATH_SUBTYPE, and the Length field of Node is set to
  END_DEVICE_PATH_LENGTH.  Node is not required to be aligned on a 16-bit boundary,
  so it is recommended that a function such as WriteUnaligned16() be used to set
  the contents of the Length field.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

**/
VOID
EFIAPI
SetDevicePathEndNode (
  OUT VOID  *Node
  )
{
  ASSERT (Node != NULL);
  CopyMem (Node, &mUefiDevicePathLibEndDevicePath, sizeof (mUefiDevicePathLibEndDevicePath));
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
  return mDevicePathLibDevicePathUtilities->GetDevicePathSize (DevicePath);
}

/**
  Creates a new copy of an existing device path.

  This function allocates space for a new copy of the device path specified by
  DevicePath.  If DevicePath is NULL, then NULL is returned.
  If the memory is successfully allocated, then the
  contents of DevicePath are copied to the newly allocated buffer, and a pointer to that buffer
  is returned.  Otherwise, NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  @param  DevicePath                 A pointer to a device path data structure.

  @retval NULL    If DevicePath is NULL or invalid.
  @retval Others  A pointer to the duplicated device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
DuplicateDevicePath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  return mDevicePathLibDevicePathUtilities->DuplicateDevicePath (DevicePath);
}

/**
  Creates a new device path by appending a second device path to a first device path.

  This function creates a new device path by appending a copy of SecondDevicePath to a copy of
  FirstDevicePath in a newly allocated buffer.  Only the end-of-device-path device node from
  SecondDevicePath is retained. The newly created device path is returned.
  If FirstDevicePath is NULL, then it is ignored, and a duplicate of SecondDevicePath is returned.
  If SecondDevicePath is NULL, then it is ignored, and a duplicate of FirstDevicePath is returned.
  If both FirstDevicePath and SecondDevicePath are NULL, then a copy of an end-of-device-path is
  returned.
  If there is not enough memory for the newly allocated buffer, then NULL is returned.
  The memory for the new device path is allocated from EFI boot services memory. It is the
  responsibility of the caller to free the memory allocated.

  @param  FirstDevicePath            A pointer to a device path data structure.
  @param  SecondDevicePath           A pointer to a device path data structure.

  @retval NULL      If there is not enough memory for the newly allocated buffer.
  @retval NULL      If FirstDevicePath or SecondDevicePath is invalid.
  @retval Others    A pointer to the new device path if success.
                    Or a copy an end-of-device-path if both FirstDevicePath and
                    SecondDevicePath are NULL.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
AppendDevicePath (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *FirstDevicePath,  OPTIONAL
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *SecondDevicePath  OPTIONAL
  )
{
  return mDevicePathLibDevicePathUtilities->AppendDevicePath (FirstDevicePath, SecondDevicePath);
}

/**
  Creates a new path by appending the device node to the device path.

  This function creates a new device path by appending a copy of the device node
  specified by DevicePathNode to a copy of the device path specified by DevicePath
  in an allocated buffer.
  The end-of-device-path device node is moved after the end of the appended device node.
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
  return mDevicePathLibDevicePathUtilities->AppendDeviceNode (DevicePath, DevicePathNode);
}

/**
  Creates a new device path by appending the specified device path instance to
  the specified device path.

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
  return mDevicePathLibDevicePathUtilities->AppendDevicePathInstance (DevicePath, DevicePathInstance);
}

/**
  Creates a copy of the current device path instance and returns a pointer to the
  next device path instance.

  This function creates a copy of the current device path instance. It also updates
  DevicePath to point to the next device path instance in the device path (or NULL
  if no more) and updates Size to hold the size of the device path instance copy.
  If DevicePath is NULL, then NULL is returned.
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
  ASSERT (Size != NULL);
  return mDevicePathLibDevicePathUtilities->GetNextDevicePathInstance (DevicePath, Size);
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
  return mDevicePathLibDevicePathUtilities->CreateDeviceNode (NodeType, NodeSubType, NodeLength);
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
  return mDevicePathLibDevicePathUtilities->IsDevicePathMultiInstance (DevicePath);
}

/**
  Retrieves the device path protocol from a handle.

  This function returns the device path protocol from the handle specified by Handle.
  If Handle is NULL or Handle does not contain a device path protocol, then NULL
  is returned.

  @param  Handle                     The handle from which to retrieve the device
                                     path protocol.

  @return The device path protocol from the handle specified by Handle.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
DevicePathFromHandle (
  IN EFI_HANDLE                      Handle
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_STATUS                Status;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID *) &DevicePath
                  );
  if (EFI_ERROR (Status)) {
    DevicePath = NULL;
  }
  return DevicePath;
}

/**
  Allocates a device path for a file and appends it to an existing device path.

  If Device is a valid device handle that contains a device path protocol, then
  a device path for the file specified by FileName  is allocated and appended to
  the device path associated with the handle Device.  The allocated device path
  is returned.  If Device is NULL or Device is a handle that does not support the
  device path protocol, then a device path containing a single device path node
  for the file specified by FileName is allocated and returned.
  The memory for the new device path is allocated from EFI boot services memory.
  It is the responsibility of the caller to free the memory allocated.

  If FileName is NULL, then ASSERT().
  If FileName is not aligned on a 16-bit boundary, then ASSERT().

  @param  Device                     A pointer to a device handle.  This parameter
                                     is optional and may be NULL.
  @param  FileName                   A pointer to a Null-terminated Unicode string.

  @return The allocated device path.

**/
EFI_DEVICE_PATH_PROTOCOL *
EFIAPI
FileDevicePath (
  IN EFI_HANDLE                      Device,     OPTIONAL
  IN CONST CHAR16                    *FileName
  )
{
  UINTN                     Size;
  FILEPATH_DEVICE_PATH      *FilePath;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *FileDevicePath;

  DevicePath = NULL;

  Size = StrSize (FileName);
  FileDevicePath = AllocatePool (Size + SIZE_OF_FILEPATH_DEVICE_PATH + END_DEVICE_PATH_LENGTH);
  if (FileDevicePath != NULL) {
    FilePath = (FILEPATH_DEVICE_PATH *) FileDevicePath;
    FilePath->Header.Type    = MEDIA_DEVICE_PATH;
    FilePath->Header.SubType = MEDIA_FILEPATH_DP;
    CopyMem (&FilePath->PathName, FileName, Size);
    SetDevicePathNodeLength (&FilePath->Header, Size + SIZE_OF_FILEPATH_DEVICE_PATH);
    SetDevicePathEndNode (NextDevicePathNode (&FilePath->Header));

    if (Device != NULL) {
      DevicePath = DevicePathFromHandle (Device);
    }

    DevicePath = AppendDevicePath (DevicePath, FileDevicePath);
    FreePool (FileDevicePath);
  }

  return DevicePath;
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
  } else {
    return NULL;
  }
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
  } else {
    return NULL;
  }
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
  } else {
    return NULL;
  }
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
  } else {
    return NULL;
  }
}

