/** @file
  The EFI_BIS_PROTOCOL is used to check a digital signature of a data block 
  against a digital certificate for the purpose of an integrity and authorization check.

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                          
    
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.        

  @par Revision Reference:          
  This Protocol is introduced in EFI Specification 1.10.     

**/

#ifndef __BIS_H__
#define __BIS_H__

#define EFI_BIS_PROTOCOL_GUID \
  { \
    0x0b64aab0, 0x5429, 0x11d4, {0x98, 0x16, 0x00, 0xa0, 0xc9, 0x1f, 0xad, 0xcf } \
  }

//
// X-Intel-BIS-ParameterSet
// Attribute value
// Binary Value of X-Intel-BIS-ParameterSet Attribute.
// (Value is Base-64 encoded in actual signed manifest).
//
#define BOOT_OBJECT_AUTHORIZATION_PARMSET_GUID \
  { \
    0xedd35e31, 0x7b9, 0x11d2, { 0x83,0xa3,0x0,0xa0,0xc9,0x1f,0xad,0xcf } \
  }



typedef struct _EFI_BIS_PROTOCOL  EFI_BIS_PROTOCOL;


//
// Basic types
//
typedef VOID    *BIS_APPLICATION_HANDLE;
typedef UINT16  BIS_ALG_ID;
typedef UINT32  BIS_CERT_ID;

///
/// EFI_BIS_DATA instances obtained from BIS must be freed by calling Free( ).
///
typedef struct {
  UINT32  Length; ///< The length of Data in 8 bit bytes.
  UINT8   *Data;  ///< 32 Bit Flat Address of data.
} EFI_BIS_DATA;

///
/// EFI_BIS_VERSION type.
///
typedef struct {
  UINT32  Major;  ///< The major BIS version number.
  UINT32  Minor;  ///< A minor BIS version number.
} EFI_BIS_VERSION;

//
// ----------------------------------------------------//
// Use these values to initialize EFI_BIS_VERSION.Major
// and to interpret results of Initialize.
// ----------------------------------------------------//
//
#define BIS_CURRENT_VERSION_MAJOR BIS_VERSION_1
#define BIS_VERSION_1             1

///
/// EFI_BIS_SIGNATURE_INFO type.
///
typedef struct {
  BIS_CERT_ID CertificateID;  ///< Truncated hash of platform Boot Object
  BIS_ALG_ID  AlgorithmID;    ///< A signature algorithm number.
  UINT16      KeyLength;      ///< The length of alg. keys in bits.
} EFI_BIS_SIGNATURE_INFO;

///
/// values for EFI_BIS_SIGNATURE_INFO.AlgorithmID.
/// The exact numeric values come from the
///    "Common Data Security Architecture (CDSA) Specification".
///
#define BIS_ALG_DSA     (41)  // CSSM_ALGID_DSA
#define BIS_ALG_RSA_MD5 (42)  // CSSM_ALGID_MD5_WITH_RSA
///
/// values for EFI_BIS_SIGNATURE_INFO.CertificateId.
///
#define BIS_CERT_ID_DSA     BIS_ALG_DSA     // CSSM_ALGID_DSA
#define BIS_CERT_ID_RSA_MD5 BIS_ALG_RSA_MD5 // CSSM_ALGID_MD5_WITH_RSA
///
/// The mask value that gets applied to the truncated hash of a
/// platform  Boot Object Authorization Certificate to create the certificateID.
/// A certificateID must not have any bits set to the value 1 other than bits in
/// this mask.
///
#define BIS_CERT_ID_MASK  (0xFF7F7FFF)

///
/// Macros for dealing with the EFI_BIS_DATA object obtained
/// from BIS_GetSignatureInfo().
/// BIS_GET_SIGINFO_COUNT - tells how many EFI_BIS_SIGNATURE_INFO
///  elements are contained in a EFI_BIS_DATA struct pointed to
///  by the provided EFI_BIS_DATA*.
///
#define BIS_GET_SIGINFO_COUNT(BisDataPtr) ((BisDataPtr)->Length / sizeof (EFI_BIS_SIGNATURE_INFO))

///
/// BIS_GET_SIGINFO_ARRAY - produces a EFI_BIS_SIGNATURE_INFO*
///  from a given EFI_BIS_DATA*.
///
#define BIS_GET_SIGINFO_ARRAY(BisDataPtr) ((EFI_BIS_SIGNATURE_INFO *) (BisDataPtr)->Data)

///
/// Support an old name for backward compatibility.
///
#define BOOT_OBJECT_AUTHORIZATION_PARMSET_GUIDVALUE \
        BOOT_OBJECT_AUTHORIZATION_PARMSET_GUID

/**                                                                 
  Initializes the BIS service, checking that it is compatible with the version requested by the caller.
  After this call, other BIS functions may be invoked.                                                 
    
  @param  This                     A pointer to the EFI_BIS_PROTOCOL object.
  @param  AppHandle                The function writes the new BIS_APPLICATION_HANDLE if           
                                   successful, otherwise it writes NULL. The caller must eventually
                                   destroy this handle by calling Shutdown().                      
  @param  InterfaceVersion         On input, the caller supplies the major version number of the
                                   interface version desired.                                   
                                   On output, both the major and minor                         
                                   version numbers are updated with the major and minor version
                                   numbers of the interface. This update is done whether or not the
                                   initialization was successful.                                    
  @param  TargetAddress            Indicates a network or device address of the BIS platform to connect to.                                                                 

  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_INCOMPATIBLE_VERSION The InterfaceVersion.Major requested by the                
                                   caller was not compatible with the interface version of the
                                   implementation. The InterfaceVersion.Major has
                                   been updated with the current interface version.
  @retval EFI_UNSUPPORTED          This is a local-platform implementation and        
                                   TargetAddress.Data was not NULL, or                
                                   TargetAddress.Data was any other value that was not
                                   supported by the implementation.                   
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                              
  @retval EFI_DEVICE_ERROR         One of the following device errors:
                                   * The function encountered an unexpected internal failure while initializing a cryptographic software module
                                   * No cryptographic software module with compatible version was found
                                   found
                                   * A resource limitation was encountered while using a cryptographic software module.
  @retval EFI_INVALID_PARAMETER    The This parameter supplied by the caller is NULL or does not
                                   reference a valid EFI_BIS_PROTOCOL object. Or,
                                   the AppHandle parameter supplied by the caller is NULL or
                                   an invalid memory reference. Or,
                                   the InterfaceVersion parameter supplied by the caller
                                   is NULL or an invalid memory reference. Or,
                                   the TargetAddress parameter supplied by the caller is
                                   NULL or an invalid memory reference.
                                          
**/                                       
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_INITIALIZE)(
  IN     EFI_BIS_PROTOCOL        *This,              
  OUT    BIS_APPLICATION_HANDLE  *AppHandle,         
  IN OUT EFI_BIS_VERSION         *InterfaceVersion,  
  IN     EFI_BIS_DATA            *TargetAddress      
  );

/**                                                                 
  Frees memory structures allocated and returned by other functions in the EFI_BIS protocol.  
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                         
  @param  ToFree                   An EFI_BIS_DATA* and associated memory block to be freed. 
                                   This EFI_BIS_DATA* must have been allocated by one of the other BIS functions.

  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid
                                   application instance handle associated with the EFI_BIS protocol.                                     
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_INVALID_PARAMETER    The ToFree parameter is not or is no longer a memory resource
                                   associated with this AppHandle.                              
                                           
**/     
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_FREE)(
  IN BIS_APPLICATION_HANDLE  AppHandle,               
  IN EFI_BIS_DATA            *ToFree                  
  );

/**                                                                 
  Shuts down an application's instance of the BIS service, invalidating the application handle. After
  this call, other BIS functions may no longer be invoked using the application handle value.         
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           

  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not, or is no longer, a valid
                                   application instance handle associated with the EFI_BIS protocol.                                     
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_DEVICE_ERROR         The function encountered an unexpected internal failure while
                                   returning resources associated with a cryptographic software module, or
                                   while trying to shut down a cryptographic software module.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_SHUTDOWN)(
  IN BIS_APPLICATION_HANDLE  AppHandle               
  );

/**                                                                 
  Retrieves the certificate that has been configured as the identity of the organization designated as
  the source of authorization for signatures of boot objects.
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           
  @param  Certificate              The function writes an allocated EFI_BIS_DATA* containing the Boot
                                   Object Authorization Certificate object.  The caller must
                                   eventually free the memory allocated by this function using the function Free().

  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid
                                   application instance handle associated with the EFI_BIS protocol.                                     
  @retval EFI_NOT_FOUND            There is no Boot Object Authorization Certificate currently installed.                        
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_INVALID_PARAMETER    The Certificate parameter supplied by the caller is NULL or
                                   an invalid memory reference.                                       
                                   
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_CERTIFICATE)(
  IN  BIS_APPLICATION_HANDLE  AppHandle,              
  OUT EFI_BIS_DATA            **Certificate           
  );

/**                                                                 
  Verifies the integrity and authorization of the indicated data object according to the
  indicated credentials.                                                                
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           
  @param  Credentials              A Signed Manifest containing verification information for the indicated
                                   data object.                                                            
  @param  DataObject               An in-memory copy of the raw data object to be verified.
  @param  IsVerified               The function writes TRUE if the verification succeeded, otherwise
                                   FALSE.                                                           
                                   
  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid
                                   application instance handle associated with the EFI_BIS protocol.                                       
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_INVALID_PARAMETER    One or more parameters are invalid.
  @retval EFI_SECURITY_VIOLATION   The signed manifest supplied as the Credentials parameter
                                   was invalid (could not be parsed) or Platform-specific authorization failed, etc.
  @retval EFI_DEVICE_ERROR         An unexpected internal error occurred.                           
                                   
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_VERIFY_BOOT_OBJECT)(
  IN  BIS_APPLICATION_HANDLE AppHandle,               
  IN  EFI_BIS_DATA           *Credentials,            
  IN  EFI_BIS_DATA           *DataObject,             
  OUT BOOLEAN                *IsVerified              
  );

/**                                                                 
  Retrieves the current status of the Boot Authorization Check Flag.
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           
  @param  CheckIsRequired          The function writes the value TRUE if a Boot Authorization Check is
                                   currently required on this platform, otherwise the function writes 
                                   FALSE.                                                             
                                   
  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid
                                   application instance handle associated with the EFI_BIS protocol.                                       
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_INVALID_PARAMETER    The CheckIsRequired parameter supplied by the caller is
                                   NULL or an invalid memory reference.                   
                                   
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_CHECKFLAG)(
  IN  BIS_APPLICATION_HANDLE  AppHandle,             
  OUT BOOLEAN                 *CheckIsRequired       
  );

/**                                                                 
  Retrieves a unique token value to be included in the request credential for the next update of any
  parameter in the Boot Object Authorization set                                                    
      
  @param  AppHandle                An opaque handle that identifies the caller's 
                                   instance of initialization of the BIS service.                                                                                           
  @param  UpdateToken              The function writes an allocated EFI_BIS_DATA* 
                                   containing the newunique update token value.  
                                   The caller musteventually free the memory allocated  
                                   by this function using the function Free().
                                   
  @retval EFI_SUCCESS              The function completed successfully.
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid
                                   application instance handle associated with the EFI_BIS protocol.                                       
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                                
  @retval EFI_INVALID_PARAMETER    The UpdateToken parameter supplied by the caller is NULL or
                                   an invalid memory reference.        
  @retval EFI_DEVICE_ERROR         An unexpected internal error occurred.                                                            
                                   
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_UPDATE_TOKEN)(
  IN  BIS_APPLICATION_HANDLE  AppHandle,              
  OUT EFI_BIS_DATA            **UpdateToken           
  );

/**                                                                 
  Updates one of the configurable parameters of the Boot Object Authorization set.
      
  @param  AppHandle                An opaque handle that identifies the caller's 
                                   instance of initialization of the BIS service.                                                                                           
  @param  RequestCredential        This is a Signed Manifest with embedded attributes 
                                   that carry the details of the requested update.                                                 
  @param  NewUpdateToken           The function writes an allocated EFI_BIS_DATA* 
                                   containing the new unique update token value. 
                                   The caller must eventually free the memory allocated  
                                   by this function using the function Free().
                                   
  @retval EFI_SUCCESS              The function completed successfully.                                                
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid                              
                                   application instance handle associated with the EFI_BIS protocol.                   
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                       
  @retval EFI_INVALID_PARAMETER    One or more parameters are invalid.                                                 
  @retval EFI_SECURITY_VIOLATION   The signed manifest supplied as the RequestCredential parameter                           
                                   was invalid (could not be parsed) or Platform-specific authorization failed, etc.   
  @retval EFI_DEVICE_ERROR         An unexpected internal error occurred while analyzing the new
                                   certificate's key algorithm, or while attempting to retrieve
                                   the public key algorithm of the manifest's signer's certificate,
                                   or An unexpected internal error occurred in a cryptographic software module.                                                                                                                                                                  
                                   
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_UPDATE_BOOT_OBJECT_AUTHORIZATION)(
  IN  BIS_APPLICATION_HANDLE AppHandle,               
  IN  EFI_BIS_DATA           *RequestCredential,      
  OUT EFI_BIS_DATA           **NewUpdateToken         
  );

/**                                                                 
  Verifies the integrity and authorization of the indicated data object according to the indicated
  credentials and authority certificate.                                                          
      
  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           
  @param  Credentials              A Signed Manifest containing verification information for the
                                   indicated data object.                                       
  @param  DataObject               An in-memory copy of the raw data object to be verified.
  @param  SectionName              An ASCII string giving the section name in the  
                                   manifest holding the verification information (in other words,
                                   hash value) that corresponds to DataObject.                             
  @param  AuthorityCertificate     A digital certificate whose public key must match the signer's                              
                                   public key which is found in the credentials.                  
  @param  IsVerified               The function writes TRUE if the verification was successful.
                                   Otherwise, the function writes FALSE.                       
                                   
  @retval EFI_SUCCESS              The function completed successfully.                                                
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid                              
                                   application instance handle associated with the EFI_BIS protocol.                   
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                       
  @retval EFI_INVALID_PARAMETER    One or more parameters are invalid.                                                 
  @retval EFI_SECURITY_VIOLATION   The Credentials.Data supplied by the caller is NULL,
                                   or the AuthorityCertificate supplied by the caller was 
                                   invalid (could not be parsed),                      
                                   or Platform-specific authorization failed, etc.   
  @retval EFI_DEVICE_ERROR         An unexpected internal error occurred while attempting to retrieve
                                   the public key algorithm of the manifest's signer's certificate,
                                   or An unexpected internal error occurred in a cryptographic software module.                                                                                                                                                                  
**/ 
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_VERIFY_OBJECT_WITH_CREDENTIAL)(
  IN  BIS_APPLICATION_HANDLE AppHandle,              
  IN  EFI_BIS_DATA           *Credentials,           
  IN  EFI_BIS_DATA           *DataObject,            
  IN  EFI_BIS_DATA           *SectionName,           
  IN  EFI_BIS_DATA           *AuthorityCertificate,  
  OUT BOOLEAN                *IsVerified             
  );

/**                                                                 
  Retrieves a list of digital certificate identifier, digital signature algorithm, hash algorithm, and keylength
  combinations that the platform supports.                                                                      

  @param  AppHandle                An opaque handle that identifies the caller's instance of initialization
                                   of the BIS service.                                                                                           
  @param  SignatureInfo            The function writes an allocated EFI_BIS_DATA* containing the array
                                   of EFI_BIS_SIGNATURE_INFO structures representing the supported    
                                   digital certificate identifier, algorithm, and key length combinations.
                                   The caller must eventually free the memory allocated by this function using the function Free().

  @retval EFI_SUCCESS              The function completed successfully.                                                
  @retval EFI_NO_MAPPING           The AppHandle parameter is not or is no longer a valid                              
                                   application instance handle associated with the EFI_BIS protocol.                   
  @retval EFI_OUT_OF_RESOURCES     The function failed due to lack of memory or other resources.                       
  @retval EFI_INVALID_PARAMETER    The SignatureInfo parameter supplied by the caller is NULL
                                   or an invalid memory reference.
  @retval EFI_DEVICE_ERROR         An unexpected internal error occurred in a
                                   cryptographic software module, or
                                   The function encountered an unexpected internal consistency check
                                   failure (possible corruption of stored Boot Object Authorization Certificate).

**/
typedef
EFI_STATUS
(EFIAPI *EFI_BIS_GET_SIGNATURE_INFO)(
  IN  BIS_APPLICATION_HANDLE  AppHandle,           
  OUT EFI_BIS_DATA            **SignatureInfo      
  );

///
/// The EFI_BIS_PROTOCOL is used to check a digital signature of a data block against a digital
/// certificate for the purpose of an integrity and authorization check.
///
struct _EFI_BIS_PROTOCOL {
  EFI_BIS_INITIALIZE                                  Initialize;
  EFI_BIS_SHUTDOWN                                    Shutdown;
  EFI_BIS_FREE                                        Free;
  EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_CERTIFICATE   GetBootObjectAuthorizationCertificate;
  EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_CHECKFLAG     GetBootObjectAuthorizationCheckFlag;
  EFI_BIS_GET_BOOT_OBJECT_AUTHORIZATION_UPDATE_TOKEN  GetBootObjectAuthorizationUpdateToken;
  EFI_BIS_GET_SIGNATURE_INFO                          GetSignatureInfo;
  EFI_BIS_UPDATE_BOOT_OBJECT_AUTHORIZATION            UpdateBootObjectAuthorization;
  EFI_BIS_VERIFY_BOOT_OBJECT                          VerifyBootObject;
  EFI_BIS_VERIFY_OBJECT_WITH_CREDENTIAL               VerifyObjectWithCredential;
};

extern EFI_GUID gEfiBisProtocolGuid;
extern EFI_GUID gBootObjectAuthorizationParmsetGuid;

#endif
