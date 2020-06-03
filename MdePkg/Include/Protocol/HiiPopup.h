/** @file
  This protocol provides services to display a popup window.
  The protocol is typically produced by the forms browser and consumed by a driver callback handler.

  Copyright (c) 2017-2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.7.

**/

#ifndef __HII_POPUP_H__
#define __HII_POPUP_H__

#define EFI_HII_POPUP_PROTOCOL_GUID \
  {0x4311edc0, 0x6054, 0x46d4, {0x9e, 0x40, 0x89, 0x3e, 0xa9, 0x52, 0xfc, 0xcc}}

#define EFI_HII_POPUP_PROTOCOL_REVISION 1

typedef struct _EFI_HII_POPUP_PROTOCOL EFI_HII_POPUP_PROTOCOL;

typedef enum {
  EfiHiiPopupStyleInfo,
  EfiHiiPopupStyleWarning,
  EfiHiiPopupStyleError
} EFI_HII_POPUP_STYLE;

typedef enum {
  EfiHiiPopupTypeOk,
  EfiHiiPopupTypeOkCancel,
  EfiHiiPopupTypeYesNo,
  EfiHiiPopupTypeYesNoCancel
} EFI_HII_POPUP_TYPE;

typedef enum {
  EfiHiiPopupSelectionOk,
  EfiHiiPopupSelectionCancel,
  EfiHiiPopupSelectionYes,
  EfiHiiPopupSelectionNo
} EFI_HII_POPUP_SELECTION;

/**
  Displays a popup window.

  @param  This           A pointer to the EFI_HII_POPUP_PROTOCOL instance.
  @param  PopupStyle     Popup style to use.
  @param  PopupType      Type of the popup to display.
  @param  HiiHandle      HII handle of the string pack containing Message
  @param  Message        A message to display in the popup box.
  @param  UserSelection  User selection.

  @retval EFI_SUCCESS            The popup box was successfully displayed.
  @retval EFI_INVALID_PARAMETER  HiiHandle and Message do not define a valid HII string.
  @retval EFI_INVALID_PARAMETER  PopupType is not one of the values defined by this specification.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources available to display the popup box.

**/
typedef
EFI_STATUS
(EFIAPI * EFI_HII_CREATE_POPUP) (
  IN  EFI_HII_POPUP_PROTOCOL  *This,
  IN  EFI_HII_POPUP_STYLE     PopupStyle,
  IN  EFI_HII_POPUP_TYPE      PopupType,
  IN  EFI_HII_HANDLE          HiiHandle,
  IN  EFI_STRING_ID           Message,
  OUT EFI_HII_POPUP_SELECTION *UserSelection OPTIONAL
);

typedef struct _EFI_HII_POPUP_PROTOCOL {
  UINT64                Revision;
  EFI_HII_CREATE_POPUP  CreatePopup;
} EFI_HII_POPUP_PROTOCOL;

extern EFI_GUID gEfiHiiPopupProtocolGuid;

#endif

