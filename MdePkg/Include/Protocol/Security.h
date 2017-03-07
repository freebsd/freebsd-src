/** @file
  Security Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  Used to provide Security services.  Specifically, dependening upon the 
  authentication state of a discovered driver in a Firmware Volume, the 
  portable DXE Core Dispatcher will call into the Security Architectural 
  Protocol (SAP) with the authentication state of the driver.

  This call-out allows for OEM-specific policy decisions to be made, such
  as event logging for attested boots, locking flash in response to discovering
  an unsigned driver or failed signature check, or other exception response.

  The SAP can also change system behavior by having the DXE core put a driver
  in the Schedule-On-Request (SOR) state.  This will allow for later disposition 
  of the driver by platform agent, such as Platform BDS.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __ARCH_PROTOCOL_SECURITY_H__
#define __ARCH_PROTOCOL_SECURITY_H__

///
/// Global ID for the Security Code Architectural Protocol
///
#define EFI_SECURITY_ARCH_PROTOCOL_GUID  \
  { 0xA46423E3, 0x4617, 0x49f1, {0xB9, 0xFF, 0xD1, 0xBF, 0xA9, 0x11, 0x58, 0x39 } }

typedef struct _EFI_SECURITY_ARCH_PROTOCOL    EFI_SECURITY_ARCH_PROTOCOL;

/**
  The EFI_SECURITY_ARCH_PROTOCOL (SAP) is used to abstract platform-specific 
  policy from the DXE core response to an attempt to use a file that returns a 
  given status for the authentication check from the section extraction protocol.  

  The possible responses in a given SAP implementation may include locking 
  flash upon failure to authenticate, attestation logging for all signed drivers, 
  and other exception operations.  The File parameter allows for possible logging 
  within the SAP of the driver.

  If File is NULL, then EFI_INVALID_PARAMETER is returned.

  If the file specified by File with an authentication status specified by 
  AuthenticationStatus is safe for the DXE Core to use, then EFI_SUCCESS is returned.

  If the file specified by File with an authentication status specified by 
  AuthenticationStatus is not safe for the DXE Core to use under any circumstances, 
  then EFI_ACCESS_DENIED is returned.

  If the file specified by File with an authentication status specified by 
  AuthenticationStatus is not safe for the DXE Core to use right now, but it 
  might be possible to use it at a future time, then EFI_SECURITY_VIOLATION is 
  returned.

  @param  This             The EFI_SECURITY_ARCH_PROTOCOL instance.
  @param  AuthenticationStatus 
                           This is the authentication type returned from the Section
                           Extraction protocol. See the Section Extraction Protocol
                           Specification for details on this type.
  @param  File             This is a pointer to the device path of the file that is
                           being dispatched. This will optionally be used for logging.

  @retval EFI_SUCCESS           The file specified by File did authenticate, and the
                                platform policy dictates that the DXE Core may use File.
  @retval EFI_INVALID_PARAMETER Driver is NULL.
  @retval EFI_SECURITY_VIOLATION The file specified by File did not authenticate, and
                                the platform policy dictates that File should be placed
                                in the untrusted state. A file may be promoted from
                                the untrusted to the trusted state at a future time
                                with a call to the Trust() DXE Service.
  @retval EFI_ACCESS_DENIED     The file specified by File did not authenticate, and
                                the platform policy dictates that File should not be
                                used for any purpose.

**/
typedef 
EFI_STATUS
(EFIAPI *EFI_SECURITY_FILE_AUTHENTICATION_STATE)(
  IN  CONST EFI_SECURITY_ARCH_PROTOCOL *This,
  IN  UINT32                           AuthenticationStatus,
  IN  CONST EFI_DEVICE_PATH_PROTOCOL   *File
  );

///
/// The EFI_SECURITY_ARCH_PROTOCOL is used to abstract platform-specific policy
/// from the DXE core.  This includes locking flash upon failure to authenticate, 
/// attestation logging, and other exception operations.
///
struct _EFI_SECURITY_ARCH_PROTOCOL {
  EFI_SECURITY_FILE_AUTHENTICATION_STATE  FileAuthenticationState;
};

extern EFI_GUID gEfiSecurityArchProtocolGuid;

#endif
