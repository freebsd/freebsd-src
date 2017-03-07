/** @file
  UEFI 2.3.1 User Credential Protocol definition.

  Attached to a device handle, this protocol identifies a single means of identifying the user.

  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __USER_CREDENTIAL2_H__
#define __USER_CREDENTIAL2_H__

#include <Protocol/UserManager.h>

#define EFI_USER_CREDENTIAL2_PROTOCOL_GUID \
  { \
    0xe98adb03, 0xb8b9, 0x4af8, { 0xba, 0x20, 0x26, 0xe9, 0x11, 0x4c, 0xbc, 0xe5 } \
  }

typedef struct _EFI_USER_CREDENTIAL2_PROTOCOL  EFI_USER_CREDENTIAL2_PROTOCOL;

/**
  Enroll a user on a credential provider.

  This function enrolls a user on this credential provider. If the user exists on this credential 
  provider, update the user information on this credential provider; otherwise add the user information 
  on credential provider.

  @param[in] This                Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in] User                The user profile to enroll.
 
  @retval EFI_SUCCESS            User profile was successfully enrolled.
  @retval EFI_ACCESS_DENIED      Current user profile does not permit enrollment on the user profile 
                                 handle. Either the user profile cannot enroll on any user profile or 
                                 cannot enroll on a user profile other than the current user profile.
  @retval EFI_UNSUPPORTED        This credential provider does not support enrollment in the pre-OS.
  @retval EFI_DEVICE_ERROR       The new credential could not be created because of a device error.
  @retval EFI_INVALID_PARAMETER  User does not refer to a valid user profile handle.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_ENROLL)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE        User
  );

/**
  Returns the user interface information used during user identification.

  This function returns information about the form used when interacting with the user during user 
  identification. The form is the first enabled form in the form-set class 
  EFI_HII_USER_CREDENTIAL_FORMSET_GUID installed on the HII handle HiiHandle. If 
  the user credential provider does not require a form to identify the user, then this function should 
  return EFI_NOT_FOUND.

  @param[in]  This               Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[out] Hii                On return, holds the HII database handle.
  @param[out] FormSetId          On return, holds the identifier of the form set which contains
                                 the form used during user identification.
  @param[out] FormId             On return, holds the identifier of the form used during user 
                                 identification.
 
  @retval EFI_SUCCESS            Form returned successfully.
  @retval EFI_NOT_FOUND          Form not returned.
  @retval EFI_INVALID_PARAMETER  Hii is NULL or FormSetId is NULL or FormId is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_FORM)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  OUT      EFI_HII_HANDLE                *Hii,
  OUT      EFI_GUID                      *FormSetId,
  OUT      EFI_FORM_ID                   *FormId
  );

/**
  Returns bitmap used to describe the credential provider type.

  This optional function returns a bitmap which is less than or equal to the number of pixels specified 
  by Width and Height. If no such bitmap exists, then EFI_NOT_FOUND is returned. 

  @param[in]      This           Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in, out] Width          On entry, points to the desired bitmap width. If NULL then no bitmap
                                 information will be returned. On exit, points to the width of the  
                                 bitmap returned.
  @param[in, out] Height         On entry, points to the desired bitmap height. If NULL then no bitmap 
                                 information will be returned. On exit, points to the height of the  
                                 bitmap returned
  @param[out]     Hii            On return, holds the HII database handle. 
  @param[out]     Image          On return, holds the HII image identifier. 
 
  @retval EFI_SUCCESS            Image identifier returned successfully.
  @retval EFI_NOT_FOUND          Image identifier not returned.
  @retval EFI_INVALID_PARAMETER  Hii is NULL or Image is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_TILE)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  IN OUT   UINTN                         *Width,
  IN OUT   UINTN                         *Height,
  OUT      EFI_HII_HANDLE                *Hii,
  OUT      EFI_IMAGE_ID                  *Image
  );

/**
  Returns string used to describe the credential provider type.

  This function returns a string which describes the credential provider. If no such string exists, then 
  EFI_NOT_FOUND is returned. 

  @param[in]  This               Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[out] Hii                On return, holds the HII database handle.
  @param[out] String             On return, holds the HII string identifier.
 
  @retval EFI_SUCCESS            String identifier returned successfully.
  @retval EFI_NOT_FOUND          String identifier not returned.
  @retval EFI_INVALID_PARAMETER  Hii is NULL or String is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_TITLE)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  OUT      EFI_HII_HANDLE                *Hii,
  OUT      EFI_STRING_ID                 *String
  );

/**
  Return the user identifier associated with the currently authenticated user.

  This function returns the user identifier of the user authenticated by this credential provider. This 
  function is called after the credential-related information has been submitted on a form OR after a 
  call to Default() has returned that this credential is ready to log on.

  @param[in]  This               Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in]  User               The user profile handle of the user profile currently being considered 
                                 by the user identity manager. If NULL, then no user profile is currently 
                                 under consideration.
  @param[out] Identifier         On return, points to the user identifier. 
 
  @retval EFI_SUCCESS            User identifier returned successfully.
  @retval EFI_NOT_READY          No user identifier can be returned.
  @retval EFI_ACCESS_DENIED      The user has been locked out of this user credential.
  @retval EFI_NOT_FOUND          User is not NULL, and the specified user handle can't be found in user 
                                 profile database 
  @retval EFI_INVALID_PARAMETER  Identifier is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_USER)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  IN       EFI_USER_PROFILE_HANDLE       User,
  OUT      EFI_USER_INFO_IDENTIFIER      *Identifier
  );

/**
  Indicate that user interface interaction has begun for the specified credential.

  This function is called when a credential provider is selected by the user. If AutoLogon returns 
  FALSE, then the user interface will be constructed by the User Identity Manager. 

  @param[in]  This               Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[out] AutoLogon          On return, points to the credential provider's capabilities after 
                                 the credential provider has been selected by the user. 
 
  @retval EFI_SUCCESS            Credential provider successfully selected.
  @retval EFI_INVALID_PARAMETER  AutoLogon is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_SELECT)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  OUT      EFI_CREDENTIAL_LOGON_FLAGS    *AutoLogon
  ); 

/**
  Indicate that user interface interaction has ended for the specified credential.

  This function is called when a credential provider is deselected by the user.

  @param[in] This        Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
 
  @retval EFI_SUCCESS    Credential provider successfully deselected.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_DESELECT)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This
  );

/**
  Return the default logon behavior for this user credential.

  This function reports the default login behavior regarding this credential provider.  

  @param[in]  This               Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[out] AutoLogon          On return, holds whether the credential provider should be 
                                 used by default to automatically log on the user.  
 
  @retval EFI_SUCCESS            Default information successfully returned.
  @retval EFI_INVALID_PARAMETER  AutoLogon is NULL.
**/
typedef 
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_DEFAULT)(
  IN  CONST EFI_USER_CREDENTIAL2_PROTOCOL       *This,
  OUT EFI_CREDENTIAL_LOGON_FLAGS                *AutoLogon
  );

/**
  Return information attached to the credential provider.

  This function returns user information. 

  @param[in]     This           Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in]     UserInfo       Handle of the user information data record. 
  @param[out]    Info           On entry, points to a buffer of at least *InfoSize bytes. On exit, holds the user 
                                information. If the buffer is too small to hold the information, then 
                                EFI_BUFFER_TOO_SMALL is returned and InfoSize is updated to contain the 
                                number of bytes actually required.
  @param[in,out] InfoSize       On entry, points to the size of Info. On return, points to the size of the user 
                                information. 
 
  @retval EFI_SUCCESS           Information returned successfully.
  @retval EFI_BUFFER_TOO_SMALL  The size specified by InfoSize is too small to hold all of the user 
                                information. The size required is returned in *InfoSize.
  @retval EFI_NOT_FOUND         The specified UserInfo does not refer to a valid user info handle.
  @retval EFI_INVALID_PARAMETER Info is NULL or InfoSize is NULL.                                
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_GET_INFO)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  IN       EFI_USER_INFO_HANDLE          UserInfo,
  OUT      EFI_USER_INFO                 *Info,
  IN OUT   UINTN                         *InfoSize
  );

/**
  Enumerate all of the user information records on the credential provider.

  This function returns the next user information record. To retrieve the first user information record 
  handle, point UserInfo at a NULL. Each subsequent call will retrieve another user information 
  record handle until there are no more, at which point UserInfo will point to NULL. 

  @param[in]     This            Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in,out] UserInfo        On entry, points to the previous user information handle or NULL to  
                                 start enumeration. On exit, points to the next user information handle 
                                 or NULL if there is no more user information.
 
  @retval EFI_SUCCESS            User information returned.
  @retval EFI_NOT_FOUND          No more user information found.
  @retval EFI_INVALID_PARAMETER  UserInfo is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_CREDENTIAL2_GET_NEXT_INFO)(
  IN CONST EFI_USER_CREDENTIAL2_PROTOCOL *This,
  IN OUT   EFI_USER_INFO_HANDLE          *UserInfo
  );

/**
  Delete a user on this credential provider.

  This function deletes a user on this credential provider. 

  @param[in]     This            Points to this instance of the EFI_USER_CREDENTIAL2_PROTOCOL.
  @param[in]     User            The user profile handle to delete.

  @retval EFI_SUCCESS            User profile was successfully deleted.
  @retval EFI_ACCESS_DENIED      Current user profile does not permit deletion on the user profile handle. 
                                 Either the user profile cannot delete on any user profile or cannot delete 
                                 on a user profile other than the current user profile. 
  @retval EFI_UNSUPPORTED        This credential provider does not support deletion in the pre-OS.
  @retval EFI_DEVICE_ERROR       The new credential could not be deleted because of a device error.
  @retval EFI_INVALID_PARAMETER  User does not refer to a valid user profile handle.
**/
typedef 
EFI_STATUS 
(EFIAPI *EFI_CREDENTIAL2_DELETE)(
 IN CONST EFI_USER_CREDENTIAL2_PROTOCOL  *This,
 IN       EFI_USER_PROFILE_HANDLE        User
);

///
/// This protocol provides support for a single class of credentials
///
struct _EFI_USER_CREDENTIAL2_PROTOCOL {
  EFI_GUID                      Identifier;  ///< Uniquely identifies this credential provider.
  EFI_GUID                      Type;        ///< Identifies this class of User Credential Provider.
  EFI_CREDENTIAL2_ENROLL        Enroll;
  EFI_CREDENTIAL2_FORM          Form;
  EFI_CREDENTIAL2_TILE          Tile;
  EFI_CREDENTIAL2_TITLE         Title;
  EFI_CREDENTIAL2_USER          User;
  EFI_CREDENTIAL2_SELECT        Select;   
  EFI_CREDENTIAL2_DESELECT      Deselect;
  EFI_CREDENTIAL2_DEFAULT       Default;
  EFI_CREDENTIAL2_GET_INFO      GetInfo;
  EFI_CREDENTIAL2_GET_NEXT_INFO GetNextInfo;
  EFI_CREDENTIAL_CAPABILITIES   Capabilities;
  EFI_CREDENTIAL2_DELETE        Delete; 
};

extern EFI_GUID gEfiUserCredential2ProtocolGuid;

#endif
