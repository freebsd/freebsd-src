/** @file
  The Smart Card Edge Protocol provides an abstraction for device to provide Smart
  Card support.

  This protocol allows UEFI applications to interface with a Smart Card during
  boot process for authentication or data signing/decryption, especially if the
  application has to make use of PKI.

  Copyright (c) 2015-2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in UEFI Specification 2.5.

**/

#ifndef __SMART_CARD_EDGE_H__
#define __SMART_CARD_EDGE_H__

#define EFI_SMART_CARD_EDGE_PROTOCOL_GUID \
    { \
      0xd317f29b, 0xa325, 0x4712, {0x9b, 0xf1, 0xc6, 0x19, 0x54, 0xdc, 0x19, 0x8c} \
    }

typedef struct _EFI_SMART_CARD_EDGE_PROTOCOL  EFI_SMART_CARD_EDGE_PROTOCOL;

//
// Maximum size for a Smart Card AID (Application IDentifier)
//
#define SCARD_AID_MAXSIZE                        0x0010
//
// Size of CSN (Card Serial Number)
//
#define SCARD_CSN_SIZE                           0x0010
//
// Current specification version 1.00
//
#define SMART_CARD_EDGE_PROTOCOL_VERSION_1       0x00000100
//
// Parameters type definition
//
typedef UINT8 SMART_CARD_AID[SCARD_AID_MAXSIZE];
typedef UINT8 SMART_CARD_CSN[SCARD_CSN_SIZE];

//
// Type of data elements in credentials list
//
// value of tag field for header, the number of containers
//
#define SC_EDGE_TAG_HEADER              0x0000
//
// value of tag field for certificate
//
#define SC_EDGE_TAG_CERT                0x0001
//
// value of tag field for key index associated with certificate
//
#define SC_EDGE_TAG_KEY_ID              0x0002
//
// value of tag field for key type
//
#define SC_EDGE_TAG_KEY_TYPE            0x0003
//
// value of tag field for key size
//
#define SC_EDGE_TAG_KEY_SIZE            0x0004

//
// Length of L fields of TLV items
//
//
// size of L field for header
//
#define SC_EDGE_L_SIZE_HEADER           1
//
// size of L field for certificate (big endian)
//
#define SC_EDGE_L_SIZE_CERT             2
//
// size of L field for key index
//
#define SC_EDGE_L_SIZE_KEY_ID           1
//
// size of L field for key type
//
#define SC_EDGE_L_SIZE_KEY_TYPE         1
//
// size of L field for key size (big endian)
//
#define SC_EDGE_L_SIZE_KEY_SIZE         2

//
// Some TLV items have a fixed value for L field
//
// value of L field for header
//
#define SC_EDGE_L_VALUE_HEADER          1
//
// value of L field for key index
//
#define SC_EDGE_L_VALUE_KEY_ID          1
//
// value of L field for key type
//
#define SC_EDGE_L_VALUE_KEY_TYPE        1
//
// value of L field for key size
//
#define SC_EDGE_L_VALUE_KEY_SIZE        2

//
// Possible values for key type
//
//
// RSA decryption
//
#define SC_EDGE_RSA_EXCHANGE            0x01
//
// RSA signature
//
#define SC_EDGE_RSA_SIGNATURE           0x02
//
// ECDSA signature
//
#define SC_EDGE_ECDSA_256               0x03
//
// ECDSA signature
//
#define SC_EDGE_ECDSA_384               0x04
//
// ECDSA signature
//
#define SC_EDGE_ECDSA_521               0x05
//
// ECDH agreement
//
#define SC_EDGE_ECDH_256                0x06
//
// ECDH agreement
//
#define SC_EDGE_ECDH_384                0x07
//
// ECDH agreement
//
#define SC_EDGE_ECDH_521                0x08

//
// Padding methods GUIDs for signature
//
//
// RSASSA- PKCS#1-V1.5 padding method, for signature
//
#define EFI_PADDING_RSASSA_PKCS1V1P5_GUID \
  { \
    0x9317ec24, 0x7cb0, 0x4d0e, {0x8b, 0x32, 0x2e, 0xd9, 0x20, 0x9c, 0xd8, 0xaf} \
  }

extern EFI_GUID gEfiPaddingRsassaPkcs1V1P5Guid;

//
// RSASSA-PSS padding method, for signature
//
#define EFI_PADDING_RSASSA_PSS_GUID \
  { \
    0x7b2349e0, 0x522d, 0x4f8e, {0xb9, 0x27, 0x69, 0xd9, 0x7c, 0x9e, 0x79, 0x5f} \
  }

extern EFI_GUID gEfiPaddingRsassaPssGuid;

//
// Padding methods GUIDs for decryption
//
//
// No padding, for decryption
//
#define EFI_PADDING_NONE_GUID \
  { \
    0x3629ddb1, 0x228c, 0x452e, {0xb6, 0x16, 0x09, 0xed, 0x31, 0x6a, 0x97, 0x00} \
  }

extern EFI_GUID gEfiPaddingNoneGuid;

//
// RSAES-PKCS#1-V1.5 padding, for decryption
//
#define EFI_PADDING_RSAES_PKCS1V1P5_GUID \
  { \
    0xe1c1d0a9, 0x40b1, 0x4632, {0xbd, 0xcc, 0xd9, 0xd6, 0xe5, 0x29, 0x56, 0x31} \
  }

extern EFI_GUID gEfiPaddingRsaesPkcs1V1P5Guid;

//
// RSAES-OAEP padding, for decryption
//
#define EFI_PADDING_RSAES_OAEP_GUID \
  { \
    0xc1e63ac4, 0xd0cf, 0x4ce6, {0x83, 0x5b, 0xee, 0xd0, 0xe6, 0xa8, 0xa4, 0x5b} \
  }

extern EFI_GUID gEfiPaddingRsaesOaepGuid;

/**
  This function retrieves the context driver.

  The GetContextfunction returns the context of the protocol, the application
  identifiers supported by the protocol and the number and the CSN unique identifier
  of Smart Cards that are present and supported by protocol.

  If AidTableSize, AidTable, CsnTableSize, CsnTable or VersionProtocol is NULL,
  the function does not fail but does not fill in such variables.

  In case AidTableSize indicates a buffer too small to hold all the protocol AID table,
  only the first AidTableSize items of the table are returned in AidTable.

  In case CsnTableSize indicates a buffer too small to hold the entire table of
  Smart Card CSN present, only the first CsnTableSize items of the table are returned
  in CsnTable.

  VersionScEdgeProtocol returns the version of the EFI_SMART_CARD_EDGE_PROTOCOL this
  driver uses. For this protocol specification value is SMART_CARD_EDGE_PROTOCOL_VERSION_1.

  In case of Smart Card removal the internal CSN list is immediately updated, even if
  a connection is opened with that Smart Card.

  @param[in]      This                  Indicates a pointer to the calling context.
  @param[out]     NumberAidSupported    Number of AIDs this protocol supports.
  @param[in, out] AidTableSize          On input, number of items allocated for the
                                        AID table. On output, number of items returned
                                        by protocol.
  @param[out]     AidTable              Table of the AIDs supported by the protocol.
  @param[out]     NumberSCPresent       Number of currently present Smart Cards that
                                        are supported by protocol.
  @param[in, out] CsnTableSize          On input, the number of items the buffer CSN
                                        table can contain. On output, the number of
                                        items returned by the protocol.
  @param[out]     CsnTable              Table of the CSN of the Smart Card present and
                                        supported by protocol.
  @param[out]     VersionScEdgeProtocol EFI_SMART_CARD_EDGE_PROTOCOL version.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  NumberSCPresent is NULL.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_CONTEXT) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
     OUT UINTN                             *NumberAidSupported,
  IN OUT UINTN                             *AidTableSize OPTIONAL,
     OUT SMART_CARD_AID                    *AidTable OPTIONAL,
     OUT UINTN                             *NumberSCPresent,
  IN OUT UINTN                             *CsnTableSize OPTIONAL,
     OUT SMART_CARD_CSN                    *CsnTable OPTIONAL,
     OUT UINT32                            *VersionScEdgeProtocol OPTIONAL
  );

/**
  This function establish a connection with a Smart Card the protocol support.

  In case of success the SCardHandle can be used.

  If the ScardCsn is NULL the connection is established with the first Smart Card
  the protocol finds in its table of Smart Card present and supported. Else it
  establish context with the Smart Card whose CSN given by ScardCsn.

  If ScardAid is not NULL the function returns the Smart Card AID the protocol supports.
  After a successful connect the SCardHandle will remain existing even in case Smart Card
  removed from Smart Card reader, but all function invoking this SCardHandle will fail.
  SCardHandle is released only on Disconnect.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[out] SCardHandle        Handle on Smart Card connection.
  @param[in]  ScardCsn           CSN of the Smart Card the connection has to be
                                 established.
  @param[out] ScardAid           AID of the Smart Card the connection has been
                                 established.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  SCardHandle is NULL.
  @retval EFI_NO_MEDIA           No Smart Card supported by protocol is present,
                                 Smart Card with CSN ScardCsn or Reader has been
                                 removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_CONNECT) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
     OUT EFI_HANDLE                        *SCardHandle,
  IN     UINT8                             *ScardCsn OPTIONAL,
     OUT UINT8                             *ScardAid OPTIONAL
  );

/**
  This function releases a connection previously established by Connect.

  The Disconnect function releases the connection previously established by
  a Connect. In case the Smart Card or the Smart Card reader has been removed
  before this call, this function returns EFI_SUCCESS.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection to release.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_DISCONNECT) (
  IN  EFI_SMART_CARD_EDGE_PROTOCOL         *This,
  IN  EFI_HANDLE                           SCardHandle
  );

/**
  This function returns the Smart Card serial number.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection.
  @param[out] Csn                The Card Serial number, 16 bytes array.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_CSN) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
     OUT UINT8                             Csn[SCARD_CSN_SIZE]
  );

/**
  This function returns the name of the Smart Card reader used for this connection.

  @param[in]      This              Indicates a pointer to the calling context.
  @param[in]      SCardHandle       Handle on Smart Card connection.
  @param[in, out] ReaderNameLength  On input, a pointer to the variable that holds
                                    the maximal size, in bytes, of ReaderName.
                                    On output, the required size, in bytes, for ReaderName.
  @param[out]     ReaderName        A pointer to a NULL terminated string that will
                                    contain the reader name.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  ReaderNameLength is NULL.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_READER_NAME) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN OUT UINTN                             *ReaderNameLength,
     OUT CHAR16                            *ReaderName OPTIONAL
  );

/**
  This function authenticates a Smart Card user by presenting a PIN code.

  The VerifyPinfunction presents a PIN code to the Smart Card.

  If Smart Card found the PIN code correct the user is considered authenticated
  to current application, and the function returns TRUE.

  Negative or null PinSize value rejected if PinCodeis not NULL.

  A NULL PinCodebuffer means the application didn't know the PIN, in that case:
    - If PinSize value is negative the caller only wants to know if the current
      chain of the elements Smart Card Edge protocol, Smart Card Reader protocol
      and Smart Card Reader supports the Secure Pin Entry PCSC V2 functionality.
    - If PinSize value is positive or null the caller ask to perform the verify
      PIN using the Secure PIN Entry functionality.

  In PinCode buffer, the PIN value is always given in plaintext, in case of secure
  messaging the SMART_CARD_EDGE_PROTOCOL will be in charge of all intermediate
  treatments to build the correct Smart Card APDU.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection.
  @param[in]  PinSize            PIN code buffer size.
  @param[in]  PinCode            PIN code to present to the Smart Card.
  @param[out] PinResult          Result of PIN code presentation to the Smart Card.
                                 TRUE when Smard Card founds the PIN code correct.
  @param[out] RemainingAttempts  Number of attempts still possible.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_UNSUPPORTED        Pinsize < 0 and Secure PIN Entry functionality not
                                 supported.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  Bad value for PinSize: value not supported by Smart
                                 Card or, negative with PinCode not null.
  @retval EFI_INVALID_PARAMETER  PinResult is NULL.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_VERIFY_PIN) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN     INT32                             PinSize,
  IN     UINT8                             *PinCode,
     OUT BOOLEAN                           *PinResult,
     OUT UINT32                            *RemainingAttempts OPTIONAL
  );

/**
  This function gives the remaining number of attempts for PIN code presentation.

  The number of attempts to present a correct PIN is limited and depends on Smart
  Card and on PIN.

  This function will retrieve the number of remaining possible attempts.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection.
  @param[out] RemainingAttempts  Number of attempts still possible.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  RemainingAttempts is NULL.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_PIN_REMAINING) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
     OUT UINT32                            *RemainingAttempts
  );

/**
  This function returns a specific data from Smart Card.

  The function is generic for any kind of data, but driver and application must
  share an EFI_GUID that identify the data.

  @param[in]      This           Indicates a pointer to the calling context.
  @param[in]      SCardHandle    Handle on Smart Card connection.
  @param[in]      DataId         The type identifier of the data to get.
  @param[in, out] DataSize       On input, in bytes, the size of Data. On output,
                                 in bytes, the size of buffer required to store
                                 the specified data.
  @param[out]     Data           The data buffer in which the data is returned.
                                 The type of the data buffer is associated with
                                 the DataId. Ignored if *DataSize is 0.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  DataId is NULL.
  @retval EFI_INVALID_PARAMETER  DataSize is NULL.
  @retval EFI_INVALID_PARAMETER  Data is NULL, and *DataSize is not zero.
  @retval EFI_NOT_FOUND          DataId unknown for this driver.
  @retval EFI_BUFFER_TOO_SMALL   The size of Data is too small for the specified
                                 data and the required size is returned in DataSize.
  @retval EFI_ACCESS_DENIED      Operation not performed, conditions not fulfilled.
                                 PIN not verified.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_DATA) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN     EFI_GUID                          *DataId,
  IN OUT UINTN                             *DataSize,
     OUT VOID                              *Data OPTIONAL
  );

/**
  This function retrieve credentials store into the Smart Card.

  The function returns a series of items in TLV (Tag Length Value) format.

  First TLV item is the header item that gives the number of following
  containers (0x00, 0x01, Nb containers).

  All these containers are a series of 4 TLV items:
    - The certificate item (0x01, certificate size, certificate)
    - The Key identifier item (0x02, 0x01, key index)
    - The key type item (0x03, 0x01, key type)
    - The key size item (0x04, 0x02, key size), key size in number of bits.
  Numeric multi-bytes values are on big endian format, most significant byte first:
    - The L field value for certificate (2 bytes)
    - The L field value for key size (2 bytes)
    - The value field for key size (2 bytes)

  @param[in]      This           Indicates a pointer to the calling context.
  @param[in]      SCardHandle    Handle on Smart Card connection.
  @param[in, out] CredentialSize On input, in bytes, the size of buffer to store
                                 the list of credential.
                                 On output, in bytes, the size of buffer required
                                 to store the entire list of credentials.

  @param[out]     CredentialList List of credentials stored into the Smart Card.
                                 A list of TLV (Tag Length Value) elements organized
                                 in containers array.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  CredentialSize is NULL.
  @retval EFI_INVALID_PARAMETER  CredentialList is NULL, if CredentialSize is not zero.
  @retval EFI_BUFFER_TOO_SMALL   The size of CredentialList is too small for the
                                 specified data and the required size is returned in
                                 CredentialSize.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_GET_CREDENTIAL) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN OUT UINTN                             *CredentialSize,
     OUT UINT8                             *CredentialList OPTIONAL
  );

/**
  This function signs an already hashed data with a Smart Card private key.

  This function signs data, actually it is the hash of these data that is given
  to the function.

  SignatureData buffer shall be big enough for signature. Signature size is
  function key size and key type.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection.
  @param[in]  KeyId              Identifier of the key container, retrieved
                                 in a key index item of credentials.
  @param[in]  KeyType            The key type, retrieved in a key type item of
                                 credentials.

  @param[in]  HashAlgorithm      Hash algorithm used to hash the, one of:
                                   - EFI_HASH_ALGORITHM_SHA1_GUID
                                   - EFI_HASH_ALGORITHM_SHA256_GUID
                                   - EFI_HASH_ALGORITHM_SHA384_GUID
                                   - EFI_HASH_ALGORITHM_SHA512_GUID
  @param[in]  PaddingMethod      Padding method used jointly with hash algorithm,
                                 one of:
                                   - EFI_PADDING_RSASSA_PKCS1V1P5_GUID
                                   - EFI_PADDING_RSASSA_PSS_GUID
  @param[in]  HashedData         Hash of the data to sign. Size is function of the
                                 HashAlgorithm.

  @param[out] SignatureData      Resulting signature with private key KeyId. Size
                                 is function of the KeyType and key size retrieved
                                 in the associated key size item of credentials.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  KeyId is not valid.
  @retval EFI_INVALID_PARAMETER  KeyType is not valid or not corresponding to KeyId.
  @retval EFI_INVALID_PARAMETER  HashAlgorithm is NULL.
  @retval EFI_INVALID_PARAMETER  HashAlgorithm is not valid.
  @retval EFI_INVALID_PARAMETER  PaddingMethod is NULL.
  @retval EFI_INVALID_PARAMETER  PaddingMethod is not valid.
  @retval EFI_INVALID_PARAMETER  HashedData is NULL.
  @retval EFI_INVALID_PARAMETER  SignatureData is NULL.
  @retval EFI_ACCESS_DENIED      Operation not performed, conditions not fulfilled.
                                 PIN not verified.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_SIGN_DATA) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN     UINTN                             KeyId,
  IN     UINTN                             KeyType,
  IN     EFI_GUID                          *HashAlgorithm,
  IN     EFI_GUID                          *PaddingMethod,
  IN     UINT8                             *HashedData,
     OUT UINT8                             *SignatureData
  );

/**
  This function decrypts data with a PKI/RSA Smart Card private key.

  The function decrypts some PKI/RSA encrypted data with private key securely
  stored into the Smart Card.

  The KeyId must reference a key of type SC_EDGE_RSA_EXCHANGE.

  @param[in]      This           Indicates a pointer to the calling context.
  @param[in]      SCardHandle    Handle on Smart Card connection.
  @param[in]      KeyId          Identifier of the key container, retrieved
                                 in a key index item of credentials.
  @param[in]      HashAlgorithm  Hash algorithm used to hash the, one of:
                                   - EFI_HASH_ALGORITHM_SHA1_GUID
                                   - EFI_HASH_ALGORITHM_SHA256_GUID
                                   - EFI_HASH_ALGORITHM_SHA384_GUID
                                   - EFI_HASH_ALGORITHM_SHA512_GUID
  @param[in]      PaddingMethod  Padding method used jointly with hash algorithm,
                                 one of:
                                   - EFI_PADDING_NONE_GUID
                                   - EFI_PADDING_RSAES_PKCS1V1P5_GUID
                                   - EFI_PADDING_RSAES_OAEP_GUID
  @param[in]      EncryptedSize  Size of data to decrypt.
  @param[in]      EncryptedData  Data to decrypt
  @param[in, out] PlaintextSize  On input, in bytes, the size of buffer to store
                                 the decrypted data.
                                 On output, in bytes, the size of buffer required
                                 to store the decrypted data.
  @param[out]     PlaintextData  Buffer for decrypted data, padding removed.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  KeyId is not valid or associated key not of type
                                 SC_EDGE_RSA_EXCHANGE.
  @retval EFI_INVALID_PARAMETER  HashAlgorithm is NULL.
  @retval EFI_INVALID_PARAMETER  HashAlgorithm is not valid.
  @retval EFI_INVALID_PARAMETER  PaddingMethod is NULL.
  @retval EFI_INVALID_PARAMETER  PaddingMethod is not valid.
  @retval EFI_INVALID_PARAMETER  EncryptedSize is 0.
  @retval EFI_INVALID_PARAMETER  EncryptedData is NULL.
  @retval EFI_INVALID_PARAMETER  PlaintextSize is NULL.
  @retval EFI_INVALID_PARAMETER  PlaintextData is NULL.
  @retval EFI_ACCESS_DENIED      Operation not performed, conditions not fulfilled.
                                 PIN not verified.
  @retval EFI_BUFFER_TOO_SMALL   PlaintextSize is too small for the plaintext data
                                 and the required size is returned in PlaintextSize.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_DECRYPT_DATA) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN     UINTN                             KeyId,
  IN     EFI_GUID                          *HashAlgorithm,
  IN     EFI_GUID                          *PaddingMethod,
  IN     UINTN                             EncryptedSize,
  IN     UINT8                             *EncryptedData,
  IN OUT UINTN                             *PlaintextSize,
     OUT UINT8                             *PlaintextData
  );

/**
  This function performs a secret Diffie Hellman agreement calculation that would
  be used to derive a symmetric encryption / decryption key.

  The function compute a DH agreement that should be diversified togenerate a symmetric
  key to proceed encryption or decryption.

  The application and the Smart Card shall agree on the diversification process.

  The KeyId must reference a key of one of the types: SC_EDGE_ECDH_256, SC_EDGE_ECDH_384
  or SC_EDGE_ECDH_521.

  @param[in]  This               Indicates a pointer to the calling context.
  @param[in]  SCardHandle        Handle on Smart Card connection.
  @param[in]  KeyId              Identifier of the key container, retrieved
                                 in a key index item of credentials.
  @param[in]  dataQx             Public key x coordinate. Size is the same as
                                 key size for KeyId. Stored in big endian format.
  @param[in]  dataQy             Public key y coordinate. Size is the same as
                                 key size for KeyId. Stored in big endian format.
  @param[out] DHAgreement        Buffer for DH agreement computed. Size must be
                                 bigger or equal to key size for KeyId.

  @retval EFI_SUCCESS            The requested command completed successfully.
  @retval EFI_INVALID_PARAMETER  This is NULL.
  @retval EFI_INVALID_PARAMETER  No connection for SCardHandle value.
  @retval EFI_INVALID_PARAMETER  KeyId is not valid.
  @retval EFI_INVALID_PARAMETER  dataQx is NULL.
  @retval EFI_INVALID_PARAMETER  dataQy is NULL.
  @retval EFI_INVALID_PARAMETER  DHAgreement is NULL.
  @retval EFI_ACCESS_DENIED      Operation not performed, conditions not fulfilled.
                                 PIN not verified.
  @retval EFI_NO_MEDIA           Smart Card or Reader of SCardHandle connection
                                 has been removed. A Disconnect should be performed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SMART_CARD_EDGE_BUILD_DH_AGREEMENT) (
  IN     EFI_SMART_CARD_EDGE_PROTOCOL      *This,
  IN     EFI_HANDLE                        SCardHandle,
  IN     UINTN                             KeyId,
  IN     UINT8                             *dataQx,
  IN     UINT8                             *dataQy,
     OUT UINT8                             *DHAgreement
  );

///
/// Smart card aware application invokes this protocol to get access to an inserted
/// smart card in the reader or to the reader itself.
///
struct _EFI_SMART_CARD_EDGE_PROTOCOL {
  EFI_SMART_CARD_EDGE_GET_CONTEXT          GetContext;
  EFI_SMART_CARD_EDGE_CONNECT              Connect;
  EFI_SMART_CARD_EDGE_DISCONNECT           Disconnect;
  EFI_SMART_CARD_EDGE_GET_CSN              GetCsn;
  EFI_SMART_CARD_EDGE_GET_READER_NAME      GetReaderName;
  EFI_SMART_CARD_EDGE_VERIFY_PIN           VerifyPin;
  EFI_SMART_CARD_EDGE_GET_PIN_REMAINING    GetPinRemaining;
  EFI_SMART_CARD_EDGE_GET_DATA             GetData;
  EFI_SMART_CARD_EDGE_GET_CREDENTIAL       GetCredential;
  EFI_SMART_CARD_EDGE_SIGN_DATA            SignData;
  EFI_SMART_CARD_EDGE_DECRYPT_DATA         DecryptData;
  EFI_SMART_CARD_EDGE_BUILD_DH_AGREEMENT   BuildDHAgreement;
};

extern EFI_GUID gEfiSmartCardEdgeProtocolGuid;

#endif

