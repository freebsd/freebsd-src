/** @file
  This protocol is defined in UEFI spec.

  The EFI_FORM_BROWSER2_PROTOCOL is the interface to call for drivers to
  leverage the EFI configuration driver interface.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_FORM_BROWSER2_H__
#define __EFI_FORM_BROWSER2_H__

#include <Guid/HiiPlatformSetupFormset.h>

#define EFI_FORM_BROWSER2_PROTOCOL_GUID \
  {0xb9d4c360, 0xbcfb, 0x4f9b, {0x92, 0x98, 0x53, 0xc1, 0x36, 0x98, 0x22, 0x58 }}


typedef struct _EFI_FORM_BROWSER2_PROTOCOL   EFI_FORM_BROWSER2_PROTOCOL;



/**

  @param LeftColumn   The value that designates the text column
                      where the browser window will begin from
                      the left-hand side of the screen

  @param RightColumn  The value that designates the text
                      column where the browser window will end
                      on the right-hand side of the screen.

  @param TopRow       The value that designates the text row from the
                      top of the screen where the browser window
                      will start.

  @param BottomRow    The value that designates the text row from the
                      bottom of the screen where the browser
                      window will end.
**/
typedef struct {
  UINTN   LeftColumn;
  UINTN   RightColumn;
  UINTN   TopRow;
  UINTN   BottomRow;
} EFI_SCREEN_DESCRIPTOR;

typedef UINTN EFI_BROWSER_ACTION_REQUEST;

#define EFI_BROWSER_ACTION_REQUEST_NONE   0
#define EFI_BROWSER_ACTION_REQUEST_RESET  1
#define EFI_BROWSER_ACTION_REQUEST_SUBMIT 2
#define EFI_BROWSER_ACTION_REQUEST_EXIT   3
#define EFI_BROWSER_ACTION_REQUEST_FORM_SUBMIT_EXIT  4
#define EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD_EXIT 5
#define EFI_BROWSER_ACTION_REQUEST_FORM_APPLY        6
#define EFI_BROWSER_ACTION_REQUEST_FORM_DISCARD      7
#define EFI_BROWSER_ACTION_REQUEST_RECONNECT         8


/**
  Initialize the browser to display the specified configuration forms.

  This function is the primary interface to the internal forms-based browser.
  The forms browser will display forms associated with the specified Handles.
  The browser will select all forms in packages which have the specified Type
  and (for EFI_HII_PACKAGE_TYPE_GUID) the specified PackageGuid.

  @param This            A pointer to the EFI_FORM_BROWSER2_PROTOCOL instance

  @param Handles         A pointer to an array of Handles. This value should correspond
                         to the value of the HII form package that is required to be displayed.

  @param HandleCount     The number of Handles specified in Handle.

  @param FormSetGuid     This field points to the EFI_GUID which must match the Guid field or one of the
                         elements of the ClassId field  in the EFI_IFR_FORM_SET op-code.  If
                         FormsetGuid is NULL, then this function will display the form set class
                         EFI_HII_PLATFORM_SETUP_FORMSET_GUID.

  @param FormId          This field specifies the identifier of the form within the form set to render as the first
                         displayable page. If this field has a value of 0x0000, then the Forms Browser will
                         render the first enabled form in the form set.

  @param ScreenDimensions Points to recommended form dimensions, including any non-content area, in
                          characters.

  @param ActionRequest   Points to the action recommended by the form.

  @retval EFI_SUCCESS           The function completed successfully

  @retval EFI_NOT_FOUND         The variable was not found.

  @retval EFI_INVALID_PARAMETER One of the parameters has an
                                invalid value.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SEND_FORM2)(
  IN CONST  EFI_FORM_BROWSER2_PROTOCOL  *This,
  IN        EFI_HII_HANDLE              *Handle,
  IN        UINTN                      HandleCount,
  IN        EFI_GUID                   *FormSetGuid, OPTIONAL
  IN        EFI_FORM_ID                FormId, OPTIONAL
  IN CONST  EFI_SCREEN_DESCRIPTOR      *ScreenDimensions, OPTIONAL
  OUT       EFI_BROWSER_ACTION_REQUEST *ActionRequest  OPTIONAL
);


/**
  This function is called by a callback handler to retrieve uncommitted state data from the browser.

  This routine is called by a routine which was called by the
  browser. This routine called this service in the browser to
  retrieve or set certain uncommitted state information.

  @param This           A pointer to the EFI_FORM_BROWSER2_PROTOCOL instance.

  @param ResultsDataSize  A pointer to the size of the buffer
                          associated with ResultsData. On input, the size in
                          bytes of ResultsData. On output, the size of data
                          returned in ResultsData.

  @param ResultsData    A string returned from an IFR browser or
                        equivalent. The results string will have
                        no routing information in them.

  @param RetrieveData   A BOOLEAN field which allows an agent to
                        retrieve (if RetrieveData = TRUE) data
                        from the uncommitted browser state
                        information or set (if RetrieveData =
                        FALSE) data in the uncommitted browser
                        state information.

  @param VariableGuid   An optional field to indicate the target
                        variable GUID name to use.

  @param VariableName   An optional field to indicate the target
                        human-readable variable name.

  @retval EFI_SUCCESS           The results have been distributed or are
                                awaiting distribution.

  @retval EFI_OUT_OF_RESOURCES  The ResultsDataSize specified
                                was too small to contain the
                                results data.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BROWSER_CALLBACK2)(
  IN CONST  EFI_FORM_BROWSER2_PROTOCOL *This,
  IN OUT    UINTN                     *ResultsDataSize,
  IN OUT    EFI_STRING                ResultsData,
  IN CONST  BOOLEAN                   RetrieveData,
  IN CONST  EFI_GUID                  *VariableGuid, OPTIONAL
  IN CONST  CHAR16                    *VariableName OPTIONAL
);

///
/// This interface will allow the caller to direct the configuration
/// driver to use either the HII database or use the passed-in packet of data.
///
struct _EFI_FORM_BROWSER2_PROTOCOL {
  EFI_SEND_FORM2         SendForm;
  EFI_BROWSER_CALLBACK2  BrowserCallback;
} ;

extern EFI_GUID gEfiFormBrowser2ProtocolGuid;

#endif

