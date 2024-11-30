/** @file
  UEFI User Manager Protocol definition.

  This protocol manages user profiles.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __USER_MANAGER_H__
#define __USER_MANAGER_H__

///
/// Global ID for the User Manager Protocol
///
#define EFI_USER_MANAGER_PROTOCOL_GUID \
  { \
    0x6fd5b00c, 0xd426, 0x4283, { 0x98, 0x87, 0x6c, 0xf5, 0xcf, 0x1c, 0xb1, 0xfe } \
  }

#define EFI_EVENT_GROUP_USER_PROFILE_CHANGED \
  { \
    0xbaf1e6de, 0x209e, 0x4adb, { 0x8d, 0x96, 0xfd, 0x8b, 0x71, 0xf3, 0xf6, 0x83 } \
  }

typedef VOID  *EFI_USER_PROFILE_HANDLE;
typedef VOID  *EFI_USER_INFO_HANDLE;

///
/// The attributes of the user profile information.
///
typedef UINT16 EFI_USER_INFO_ATTRIBS;
#define EFI_USER_INFO_STORAGE                0x000F
#define EFI_USER_INFO_STORAGE_VOLATILE       0x0000
#define EFI_USER_INFO_STORAGE_CREDENTIAL_NV  0x0001
#define EFI_USER_INFO_STORAGE_PLATFORM_NV    0x0002

#define EFI_USER_INFO_ACCESS     0x0070
#define EFI_USER_INFO_PUBLIC     0x0010
#define EFI_USER_INFO_PRIVATE    0x0020
#define EFI_USER_INFO_PROTECTED  0x0030
#define EFI_USER_INFO_EXCLUSIVE  0x0080

///
/// User information structure
///
typedef struct {
  ///
  /// The user credential identifier associated with this user information or else Nil if the
  /// information is not associated with any specific credential.
  ///
  EFI_GUID                 Credential;
  ///
  /// The type of user information.
  ///
  UINT8                    InfoType;
  ///
  /// Must be set to 0.
  ///
  UINT8                    Reserved1;
  ///
  /// The attributes of the user profile information.
  ///
  EFI_USER_INFO_ATTRIBS    InfoAttribs;
  ///
  /// The size of the user information, in bytes, including this header.
  ///
  UINT32                   InfoSize;
} EFI_USER_INFO;

///
/// User credential class GUIDs
///
#define EFI_USER_CREDENTIAL_CLASS_UNKNOWN \
  { 0x5cf32e68, 0x7660, 0x449b, { 0x80, 0xe6, 0x7e, 0xa3, 0x6e, 0x3, 0xf6, 0xa8 } }
#define EFI_USER_CREDENTIAL_CLASS_PASSWORD \
  { 0xf8e5058c, 0xccb6, 0x4714, { 0xb2, 0x20, 0x3f, 0x7e, 0x3a, 0x64, 0xb, 0xd1 } }
#define EFI_USER_CREDENTIAL_CLASS_SMART_CARD \
  { 0x5f03ba33, 0x8c6b, 0x4c24, { 0xaa, 0x2e, 0x14, 0xa2, 0x65, 0x7b, 0xd4, 0x54 } }
#define EFI_USER_CREDENTIAL_CLASS_FINGERPRINT \
  { 0x32cba21f, 0xf308, 0x4cbc, { 0x9a, 0xb5, 0xf5, 0xa3, 0x69, 0x9f, 0x4, 0x4a } }
#define EFI_USER_CREDENTIAL_CLASS_HANDPRINT \
  { 0x5917ef16, 0xf723, 0x4bb9, { 0xa6, 0x4b, 0xd8, 0xc5, 0x32, 0xf4, 0xd8, 0xb5 } }
#define EFI_USER_CREDENTIAL_CLASS_SECURE_CARD \
  { 0x8a6b4a83, 0x42fe, 0x45d2, { 0xa2, 0xef, 0x46, 0xf0, 0x6c, 0x7d, 0x98, 0x52 } }

typedef UINT64 EFI_CREDENTIAL_CAPABILITIES;
#define EFI_CREDENTIAL_CAPABILITIES_ENROLL  0x0000000000000001

///
/// Credential logon flags
///
typedef UINT32 EFI_CREDENTIAL_LOGON_FLAGS;
#define EFI_CREDENTIAL_LOGON_FLAG_AUTO     0x00000001
#define EFI_CREDENTIAL_LOGON_FLAG_DEFAULT  0x00000002

///
/// User information record types
///

///
/// No information.
///
#define EFI_USER_INFO_EMPTY_RECORD  0x00
///
/// Provide the user's name for the enrolled user.
///
#define EFI_USER_INFO_NAME_RECORD  0x01
typedef CHAR16 *EFI_USER_INFO_NAME;
///
/// Provides the date and time when the user profile was created.
///
#define EFI_USER_INFO_CREATE_DATE_RECORD  0x02
typedef EFI_TIME EFI_USER_INFO_CREATE_DATE;
///
/// Provides the date and time when the user profile was selected.
///
#define EFI_USER_INFO_USAGE_DATE_RECORD  0x03
typedef EFI_TIME EFI_USER_INFO_USAGE_DATE;
///
/// Provides the number of times that the user profile has been selected.
///
#define EFI_USER_INFO_USAGE_COUNT_RECORD  0x04
typedef UINT64 EFI_USER_INFO_USAGE_COUNT;
///
/// Provides a unique non-volatile user identifier for each enrolled user.
///
#define EFI_USER_INFO_IDENTIFIER_RECORD  0x05
typedef UINT8 EFI_USER_INFO_IDENTIFIER[16];
///
/// Specifies the type of a particular credential associated with the user profile.
///
#define EFI_USER_INFO_CREDENTIAL_TYPE_RECORD  0x06
typedef EFI_GUID EFI_USER_INFO_CREDENTIAL_TYPE;
///
/// Specifies the user-readable name of a particular credential type.
///
#define EFI_USER_INFO_CREDENTIAL_TYPE_NAME_RECORD  0x07
typedef CHAR16 *EFI_USER_INFO_CREDENTIAL_TYPE_NAME;
///
/// Specifies the credential provider.
///
#define EFI_USER_INFO_CREDENTIAL_PROVIDER_RECORD  0x08
typedef EFI_GUID EFI_USER_INFO_CREDENTIAL_PROVIDER;
///
/// Specifies the user-readable name of a particular credential's provider.
///
#define EFI_USER_INFO_CREDENTIAL_PROVIDER_NAME_RECORD  0x09
typedef CHAR16 *EFI_USER_INFO_CREDENTIAL_PROVIDER_NAME;
///
/// Provides PKCS#11 credential information from a smart card.
///
#define EFI_USER_INFO_PKCS11_RECORD  0x0A
///
/// Provides standard biometric information in the format specified by the ISO 19785 (Common
/// Biometric Exchange Formats Framework) specification.
///
#define EFI_USER_INFO_CBEFF_RECORD  0x0B
typedef VOID *EFI_USER_INFO_CBEFF;
///
/// Indicates how close of a match the fingerprint must be in order to be considered a match.
///
#define EFI_USER_INFO_FAR_RECORD  0x0C
typedef UINT8 EFI_USER_INFO_FAR;
///
/// Indicates how many attempts the user has to with a particular credential before the system prevents
/// further attempts.
///
#define EFI_USER_INFO_RETRY_RECORD  0x0D
typedef UINT8 EFI_USER_INFO_RETRY;
///
/// Provides the user's pre-OS access rights.
///
#define EFI_USER_INFO_ACCESS_POLICY_RECORD  0x0E

typedef struct {
  UINT32    Type; ///< Specifies the type of user access control.
  UINT32    Size; ///< Specifies the size of the user access control record, in bytes, including this header.
} EFI_USER_INFO_ACCESS_CONTROL;

typedef EFI_USER_INFO_ACCESS_CONTROL EFI_USER_INFO_ACCESS_POLICY;

///
/// User Information access types
///

///
/// Forbids the user from booting or loading executables from the specified device path or any child
/// device paths.
///
#define EFI_USER_INFO_ACCESS_FORBID_LOAD  0x00000001
///
/// Permits the user from booting or loading executables from the specified device path or any child
/// device paths.
/// Note: in-consistency between code and the UEFI 2.3 specification here.
/// The definition EFI_USER_INFO_ACCESS_PERMIT_BOOT in the specification should be typo and wait for
/// spec update.
///
#define EFI_USER_INFO_ACCESS_PERMIT_LOAD  0x00000002
///
/// Presence of this record indicates that a user can update enrollment information.
///
#define EFI_USER_INFO_ACCESS_ENROLL_SELF  0x00000003
///
/// Presence of this record indicates that a user can enroll new users.
///
#define EFI_USER_INFO_ACCESS_ENROLL_OTHERS  0x00000004
///
/// Presence of this record indicates that a user can update the user information of any user.
///
#define EFI_USER_INFO_ACCESS_MANAGE  0x00000005
///
/// Describes permissions usable when configuring the platform.
///
#define EFI_USER_INFO_ACCESS_SETUP  0x00000006
///
/// Standard GUIDs for access to configure the platform.
///
#define EFI_USER_INFO_ACCESS_SETUP_ADMIN_GUID \
  { 0x85b75607, 0xf7ce, 0x471e, { 0xb7, 0xe4, 0x2a, 0xea, 0x5f, 0x72, 0x32, 0xee } }
#define EFI_USER_INFO_ACCESS_SETUP_NORMAL_GUID \
  { 0x1db29ae0, 0x9dcb, 0x43bc, { 0x8d, 0x87, 0x5d, 0xa1, 0x49, 0x64, 0xdd, 0xe2 } }
#define EFI_USER_INFO_ACCESS_SETUP_RESTRICTED_GUID \
  { 0xbdb38125, 0x4d63, 0x49f4, { 0x82, 0x12, 0x61, 0xcf, 0x5a, 0x19, 0xa, 0xf8 } }

///
/// Forbids UEFI drivers from being started from the specified device path(s) or any child device paths.
///
#define EFI_USER_INFO_ACCESS_FORBID_CONNECT  0x00000007
///
/// Permits UEFI drivers to be started on the specified device path(s) or any child device paths.
///
#define EFI_USER_INFO_ACCESS_PERMIT_CONNECT  0x00000008
///
/// Modifies the boot order.
///
#define EFI_USER_INFO_ACCESS_BOOT_ORDER  0x00000009
typedef UINT32 EFI_USER_INFO_ACCESS_BOOT_ORDER_HDR;

#define EFI_USER_INFO_ACCESS_BOOT_ORDER_MASK  0x0000000F
///
/// Insert new boot options at the beginning of the boot order.
///
#define EFI_USER_INFO_ACCESS_BOOT_ORDER_INSERT  0x00000000
///
/// Append new boot options to the end of the boot order.
///
#define EFI_USER_INFO_ACCESS_BOOT_ORDER_APPEND  0x00000001
///
/// Replace the entire boot order.
///
#define EFI_USER_INFO_ACCESS_BOOT_ORDER_REPLACE  0x00000002
///
/// The Boot Manager will not attempt find a default boot device
/// when the default boot order is does not lead to a bootable device.
///
#define EFI_USER_INFO_ACCESS_BOOT_ORDER_NODEFAULT  0x00000010

///
/// Provides the expression which determines which credentials are required to assert user identity.
///
#define EFI_USER_INFO_IDENTITY_POLICY_RECORD  0x0F

typedef struct {
  UINT32    Type;   ///< Specifies either an operator or a data item.
  UINT32    Length; ///< The length of this block, in bytes, including this header.
} EFI_USER_INFO_IDENTITY_POLICY;

///
/// User identity policy expression operators.
///
#define EFI_USER_INFO_IDENTITY_FALSE                0x00
#define EFI_USER_INFO_IDENTITY_TRUE                 0x01
#define EFI_USER_INFO_IDENTITY_CREDENTIAL_TYPE      0x02
#define EFI_USER_INFO_IDENTITY_CREDENTIAL_PROVIDER  0x03
#define EFI_USER_INFO_IDENTITY_NOT                  0x10
#define EFI_USER_INFO_IDENTITY_AND                  0x11
#define EFI_USER_INFO_IDENTITY_OR                   0x12

///
/// Provides placeholder for additional user profile information identified by a GUID.
///
#define EFI_USER_INFO_GUID_RECORD  0xFF
typedef EFI_GUID EFI_USER_INFO_GUID;

///
/// User information table
/// A collection of EFI_USER_INFO records, prefixed with this header.
///
typedef struct {
  UINT64    Size; ///< Total size of the user information table, in bytes.
} EFI_USER_INFO_TABLE;

typedef struct _EFI_USER_MANAGER_PROTOCOL EFI_USER_MANAGER_PROTOCOL;

/**
  Create a new user profile.

  This function creates a new user profile with only a new user identifier attached and returns its
  handle. The user profile is non-volatile, but the handle User can change across reboots.

  @param[in]  This               Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[out] User               On return, points to the new user profile handle.
                                 The user profile handle is unique only during this boot.

  @retval EFI_SUCCESS            User profile was successfully created.
  @retval EFI_ACCESS_DENIED      Current user does not have sufficient permissions to create a user profile.
  @retval EFI_UNSUPPORTED        Creation of new user profiles is not supported.
  @retval EFI_INVALID_PARAMETER  The User parameter is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_CREATE)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  OUT      EFI_USER_PROFILE_HANDLE    *User
  );

/**
  Delete an existing user profile.

  @param[in] This                Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in] User                User profile handle.

  @retval EFI_SUCCESS            User profile was successfully deleted.
  @retval EFI_ACCESS_DENIED      Current user does not have sufficient permissions to delete a user
                                 profile or there is only one user profile.
  @retval EFI_UNSUPPORTED        Deletion of new user profiles is not supported.
  @retval EFI_INVALID_PARAMETER  User does not refer to a valid user profile.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_DELETE)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE    User
  );

/**
  Enumerate all of the enrolled users on the platform.

  This function returns the next enrolled user profile. To retrieve the first user profile handle, point
  User at a NULL. Each subsequent call will retrieve another user profile handle until there are no
  more, at which point User will point to NULL.

  @param[in]     This            Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in,out] User            On entry, points to the previous user profile handle or NULL to
                                 start enumeration. On exit, points to the next user profile handle
                                 or NULL if there are no more user profiles.

  @retval EFI_SUCCESS            Next enrolled user profile successfully returned.
  @retval EFI_ACCESS_DENIED      Next enrolled user profile was not successfully returned.
  @retval EFI_INVALID_PARAMETER  The User parameter is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_GET_NEXT)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN OUT   EFI_USER_PROFILE_HANDLE    *User
  );

/**
  Return the current user profile handle.

  @param[in]  This               Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[out] CurrentUser        On return, points to the current user profile handle.

  @retval EFI_SUCCESS            Current user profile handle returned successfully.
  @retval EFI_INVALID_PARAMETER  The CurrentUser parameter is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_CURRENT)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  OUT      EFI_USER_PROFILE_HANDLE    *CurrentUser
  );

/**
  Identify a user.

  Identify the user and, if authenticated, returns the user handle and changes the current user profile.
  All user information marked as private in a previously selected profile is no longer available for
  inspection.
  Whenever the current user profile is changed then the an event with the GUID
  EFI_EVENT_GROUP_USER_PROFILE_CHANGED is signaled.

  @param[in]  This               Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[out] User               On return, points to the user profile handle for the current user profile.

  @retval EFI_SUCCESS            User was successfully identified.
  @retval EFI_ACCESS_DENIED      User was not successfully identified.
  @retval EFI_INVALID_PARAMETER  The User parameter is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_IDENTIFY)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  OUT      EFI_USER_PROFILE_HANDLE    *User
  );

/**
  Find a user using a user information record.

  This function searches all user profiles for the specified user information record. The search starts
  with the user information record handle following UserInfo and continues until either the
  information is found or there are no more user profiles.
  A match occurs when the Info.InfoType field matches the user information record type and the
  user information record data matches the portion of Info.

  @param[in]     This      Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in,out] User      On entry, points to the previously returned user profile handle or NULL to start
                           searching with the first user profile. On return, points to the user profile handle or
                           NULL if not found.
  @param[in,out] UserInfo  On entry, points to the previously returned user information handle or NULL to start
                           searching with the first. On return, points to the user information handle of the user
                           information record or NULL if not found. Can be NULL, in which case only one user
                           information record per user can be returned.
  @param[in]     Info      Points to the buffer containing the user information to be compared to the user
                           information record. If the user information record data is empty, then only the user
                           information record type is compared.
                           If InfoSize is 0, then the user information record must be empty.

  @param[in]     InfoSize  The size of Info, in bytes.

  @retval EFI_SUCCESS           User information was found. User points to the user profile handle and UserInfo
                                points to the user information handle.
  @retval EFI_NOT_FOUND         User information was not found. User points to NULL and UserInfo points to NULL.
  @retval EFI_INVALID_PARAMETER User is NULL. Or Info is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_FIND)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN OUT   EFI_USER_PROFILE_HANDLE    *User,
  IN OUT   EFI_USER_INFO_HANDLE       *UserInfo OPTIONAL,
  IN CONST EFI_USER_INFO              *Info,
  IN       UINTN                      InfoSize
  );

/**
  Called by credential provider to notify of information change.

  This function allows the credential provider to notify the User Identity Manager when user status
  has changed.
  If the User Identity Manager doesn't support asynchronous changes in credentials, then this function
  should return EFI_UNSUPPORTED.
  If current user does not exist, and the credential provider can identify a user, then make the user
  to be current user and signal the EFI_EVENT_GROUP_USER_PROFILE_CHANGED event.
  If current user already exists, and the credential provider can identify another user, then switch
  current user to the newly identified user, and signal the EFI_EVENT_GROUP_USER_PROFILE_CHANGED event.
  If current user was identified by this credential provider and now the credential provider cannot identify
  current user, then logout current user and signal the EFI_EVENT_GROUP_USER_PROFILE_CHANGED event.

  @param[in] This          Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in] Changed       Handle on which is installed an instance of the
                           EFI_USER_CREDENTIAL_PROTOCOL where the user has changed.

  @retval EFI_SUCCESS      The User Identity Manager has handled the notification.
  @retval EFI_NOT_READY    The function was called while the specified credential provider was not selected.
  @retval EFI_UNSUPPORTED  The User Identity Manager doesn't support asynchronous notifications.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_NOTIFY)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_HANDLE                 Changed
  );

/**
  Return information attached to the user.

  This function returns user information. The format of the information is described in User
  Information. The function may return EFI_ACCESS_DENIED if the information is marked private
  and the handle specified by User is not the current user profile. The function may return
  EFI_ACCESS_DENIED if the information is marked protected and the information is associated
  with a credential provider for which the user has not been authenticated.

  @param[in]     This           Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in]     User           Handle of the user whose profile will be retrieved.
  @param[in]     UserInfo       Handle of the user information data record.
  @param[out]    Info           On entry, points to a buffer of at least *InfoSize bytes. On exit, holds the user
                                information. If the buffer is too small to hold the information, then
                                EFI_BUFFER_TOO_SMALL is returned and InfoSize is updated to contain the
                                number of bytes actually required.
  @param[in,out] InfoSize       On entry, points to the size of Info. On return, points to the size of the user
                                information.

  @retval EFI_SUCCESS           Information returned successfully.
  @retval EFI_ACCESS_DENIED     The information about the specified user cannot be accessed by the current user.
  @retval EFI_BUFFER_TOO_SMALL  The number of bytes specified by *InfoSize is too small to hold
                                the returned data. The actual size required is returned in *InfoSize.
  @retval EFI_NOT_FOUND         User does not refer to a valid user profile or UserInfo does not refer to a valid
                                user info handle.
  @retval EFI_INVALID_PARAMETER Info is NULL or InfoSize is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_GET_INFO)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE    User,
  IN       EFI_USER_INFO_HANDLE       UserInfo,
  OUT      EFI_USER_INFO              *Info,
  IN OUT   UINTN                      *InfoSize
  );

/**
  Add or update user information.

  This function changes user information.  If NULL is pointed to by UserInfo, then a new user
  information record is created and its handle is returned in UserInfo. Otherwise, the existing one is
  replaced.
  If EFI_USER_INFO_IDENTITY_POLICY_RECORD is changed, it is the caller's responsibility to keep it to
  be synced with the information on credential providers.
  If EFI_USER_INFO_EXCLUSIVE is specified in Info and a user information record of the same
  type already exists in the user profile, then EFI_ACCESS_DENIED will be returned and
  UserInfo will point to the handle of the existing record.

  @param[in]     This             Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in]     User             Handle of the user whose profile will be retrieved.
  @param[in,out] UserInfo         Handle of the user information data record.
  @param[in]     Info             On entry, points to a buffer of at least *InfoSize bytes. On exit, holds the user
                                  information. If the buffer is too small to hold the information, then
                                  EFI_BUFFER_TOO_SMALL is returned and InfoSize is updated to contain the
                                  number of bytes actually required.
  @param[in]     InfoSize         On entry, points to the size of Info. On return, points to the size of the user
                                  information.

  @retval EFI_SUCCESS             Information returned successfully.
  @retval EFI_ACCESS_DENIED       The record is exclusive.
  @retval EFI_SECURITY_VIOLATION  The current user does not have permission to change the specified
                                  user profile or user information record.
  @retval EFI_NOT_FOUND           User does not refer to a valid user profile or UserInfo does not refer to a valid
                                  user info handle.
  @retval EFI_INVALID_PARAMETER   UserInfo is NULL or Info is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_SET_INFO)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE    User,
  IN OUT   EFI_USER_INFO_HANDLE       *UserInfo,
  IN CONST EFI_USER_INFO              *Info,
  IN       UINTN                      InfoSize
  );

/**
  Delete user information.

  Delete the user information attached to the user profile specified by the UserInfo.

  @param[in] This            Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in] User            Handle of the user whose information will be deleted.
  @param[in] UserInfo        Handle of the user information to remove.

  @retval EFI_SUCCESS        User information deleted successfully.
  @retval EFI_NOT_FOUND      User information record UserInfo does not exist in the user profile.
  @retval EFI_ACCESS_DENIED  The current user does not have permission to delete this user information.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_DELETE_INFO)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE    User,
  IN       EFI_USER_INFO_HANDLE       UserInfo
  );

/**
  Enumerate user information of all the enrolled users on the platform.

  This function returns the next user information record. To retrieve the first user information record
  handle, point UserInfo at a NULL. Each subsequent call will retrieve another user information
  record handle until there are no more, at which point UserInfo will point to NULL.

  @param[in]     This           Points to this instance of the EFI_USER_MANAGER_PROTOCOL.
  @param[in]     User           Handle of the user whose information will be deleted.
  @param[in,out] UserInfo       Handle of the user information to remove.

  @retval EFI_SUCCESS           User information returned.
  @retval EFI_NOT_FOUND         No more user information found.
  @retval EFI_INVALID_PARAMETER UserInfo is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_USER_PROFILE_GET_NEXT_INFO)(
  IN CONST EFI_USER_MANAGER_PROTOCOL  *This,
  IN       EFI_USER_PROFILE_HANDLE    User,
  IN OUT   EFI_USER_INFO_HANDLE       *UserInfo
  );

///
/// This protocol provides the services used to manage user profiles.
///
struct _EFI_USER_MANAGER_PROTOCOL {
  EFI_USER_PROFILE_CREATE           Create;
  EFI_USER_PROFILE_DELETE           Delete;
  EFI_USER_PROFILE_GET_NEXT         GetNext;
  EFI_USER_PROFILE_CURRENT          Current;
  EFI_USER_PROFILE_IDENTIFY         Identify;
  EFI_USER_PROFILE_FIND             Find;
  EFI_USER_PROFILE_NOTIFY           Notify;
  EFI_USER_PROFILE_GET_INFO         GetInfo;
  EFI_USER_PROFILE_SET_INFO         SetInfo;
  EFI_USER_PROFILE_DELETE_INFO      DeleteInfo;
  EFI_USER_PROFILE_GET_NEXT_INFO    GetNextInfo;
};

extern EFI_GUID  gEfiUserManagerProtocolGuid;
extern EFI_GUID  gEfiEventUserProfileChangedGuid;
extern EFI_GUID  gEfiUserCredentialClassUnknownGuid;
extern EFI_GUID  gEfiUserCredentialClassPasswordGuid;
extern EFI_GUID  gEfiUserCredentialClassSmartCardGuid;
extern EFI_GUID  gEfiUserCredentialClassFingerprintGuid;
extern EFI_GUID  gEfiUserCredentialClassHandprintGuid;
extern EFI_GUID  gEfiUserCredentialClassSecureCardGuid;
extern EFI_GUID  gEfiUserInfoAccessSetupAdminGuid;
extern EFI_GUID  gEfiUserInfoAccessSetupNormalGuid;
extern EFI_GUID  gEfiUserInfoAccessSetupRestrictedGuid;

#endif
