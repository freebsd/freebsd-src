/** @file
  EFI_PKCS7_VERIFY_PROTOCOL as defined in UEFI 2.5.
  The EFI_PKCS7_VERIFY_PROTOCOL is used to verify data signed using PKCS#7
  formatted authentication. The PKCS#7 data to be verified must be binary
  DER encoded.
  PKCS#7 is a general-purpose cryptographic standard (defined by RFC2315,
  available at http://tools.ietf.org/html/rfc2315).

Copyright (c) 2015 - 2017, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __EFI_PKCS7_VERIFY_PROTOCOL_H__
#define __EFI_PKCS7_VERIFY_PROTOCOL_H__

#include <Guid/ImageAuthentication.h>

///
/// Global ID for the PKCS7 Verification Protocol
///
#define EFI_PKCS7_VERIFY_PROTOCOL_GUID \
  { \
    0x47889fb2, 0xd671, 0x4fab, {0xa0, 0xca, 0xdf, 0x0e, 0x44, 0xdf, 0x70, 0xd6 } \
  }

typedef struct _EFI_PKCS7_VERIFY_PROTOCOL EFI_PKCS7_VERIFY_PROTOCOL;

/**
  Processes a buffer containing binary DER-encoded PKCS7 signature.
  The signed data content may be embedded within the buffer or separated. Funtion
  verifies the signature of the content is valid and signing certificate was not
  revoked and is contained within a list of trusted signers.

  @param[in]     This                 Pointer to EFI_PKCS7_VERIFY_PROTOCOL instance.
  @param[in]     SignedData           Points to buffer containing ASN.1 DER-encoded PKCS7
                                      signature.
  @param[in]     SignedDataSize       The size of SignedData buffer in bytes.
  @param[in]     InData               In case of detached signature, InData points to
                                      buffer containing the raw message data previously
                                      signed and to be verified by function. In case of
                                      SignedData containing embedded data, InData must be
                                      NULL.
  @param[in]     InDataSize           When InData is used, the size of InData buffer in
                                      bytes. When InData is NULL. This parameter must be
                                      0.
  @param[in]     AllowedDb            Pointer to a list of pointers to EFI_SIGNATURE_LIST
                                      structures. The list is terminated by a null
                                      pointer. The EFI_SIGNATURE_LIST structures contain
                                      lists of X.509 certificates of approved signers.
                                      Function recognizes signer certificates of type
                                      EFI_CERT_X509_GUID. Any hash certificate in AllowedDb
                                      list is ignored by this function. Function returns
                                      success if signer of the buffer is within this list
                                      (and not within RevokedDb). This parameter is
                                      required.
  @param[in]     RevokedDb            Optional pointer to a list of pointers to
                                      EFI_SIGNATURE_LIST structures. The list is terminated
                                      by a null pointer. List of X.509 certificates of
                                      revoked signers and revoked file hashes. Except as
                                      noted in description of TimeStampDb signature
                                      verification will always fail if the signer of the
                                      file or the hash of the data component of the buffer
                                      is in RevokedDb list. This list is optional and
                                      caller may pass Null or pointer to NULL if not
                                      required.
  @param[in]     TimeStampDb          Optional pointer to a list of pointers to
                                      EFI_SIGNATURE_LIST structures. The list is terminated
                                      by a null pointer. This parameter can be used to pass
                                      a list of X.509 certificates of trusted time stamp
                                      signers. This list is optional and caller must pass
                                      Null or pointer to NULL if not required.
  @param[out]    Content              On input, points to an optional caller-allocated
                                      buffer into which the function will copy the content
                                      portion of the file after verification succeeds.
                                      This parameter is optional and if NULL, no copy of
                                      content from file is performed.
  @param[in,out] ContentSize          On input, points to the size in bytes of the optional
                                      buffer Content previously allocated by caller. On
                                      output, if the verification succeeds, the value
                                      referenced by ContentSize will contain the actual
                                      size of the content from signed file. If ContentSize
                                      indicates the caller-allocated buffer is too small
                                      to contain content, an error is returned, and
                                      ContentSize will be updated with the required size.
                                      This parameter must be 0 if Content is Null.

  @retval EFI_SUCCESS                 Content signature was verified against hash of
                                      content, the signer's certificate was not found in
                                      RevokedDb, and was found in AllowedDb or if in signer
                                      is found in both AllowedDb and RevokedDb, the
                                      signing was allowed by reference to TimeStampDb as
                                      described above, and no hash matching content hash
                                      was found in RevokedDb.
  @retval EFI_SECURITY_VIOLATION      The SignedData buffer was correctly formatted but
                                      signer was in RevokedDb or not in AllowedDb. Also
                                      returned if matching content hash found in RevokedDb.
  @retval EFI_COMPROMISED_DATA        Calculated hash differs from signed hash.
  @retval EFI_INVALID_PARAMETER       SignedData is NULL or SignedDataSize is zero.
                                      AllowedDb is NULL.
  @retval EFI_INVALID_PARAMETER       Content is not NULL and ContentSize is NULL.
  @retval EFI_ABORTED                 Unsupported or invalid format in TimeStampDb,
                                      RevokedDb or AllowedDb list contents was detected.
  @retval EFI_NOT_FOUND               Content not found because InData is NULL and no
                                      content embedded in SignedData.
  @retval EFI_UNSUPPORTED             The SignedData buffer was not correctly formatted
                                      for processing by the function.
  @retval EFI_UNSUPPORTED             Signed data embedded in SignedData but InData is not
                                      NULL.
  @retval EFI_BUFFER_TOO_SMALL        The size of buffer indicated by ContentSize is too
                                      small to hold the content. ContentSize updated to
                                      required size.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PKCS7_VERIFY_BUFFER)(
  IN EFI_PKCS7_VERIFY_PROTOCOL    *This,
  IN VOID                         *SignedData,
  IN UINTN                        SignedDataSize,
  IN VOID                         *InData          OPTIONAL,
  IN UINTN                        InDataSize,
  IN EFI_SIGNATURE_LIST           **AllowedDb,
  IN EFI_SIGNATURE_LIST           **RevokedDb      OPTIONAL,
  IN EFI_SIGNATURE_LIST           **TimeStampDb    OPTIONAL,
  OUT VOID                        *Content         OPTIONAL,
  IN OUT UINTN                    *ContentSize
  );

/**
  Processes a buffer containing binary DER-encoded detached PKCS7 signature.
  The hash of the signed data content is calculated and passed by the caller. Function
  verifies the signature of the content is valid and signing certificate was not revoked
  and is contained within a list of trusted signers.

  Note: because this function uses hashes and the specification contains a variety of
        hash choices, you should be aware that the check against the RevokedDb list
        will improperly succeed if the signature is revoked using a different hash
        algorithm.  For this reason, you should either cycle through all UEFI supported
        hashes to see if one is forbidden, or rely on a single hash choice only if the
        UEFI signature authority only signs and revokes with a single hash (at time
        of writing, this hash choice is SHA256).

  @param[in]     This                 Pointer to EFI_PKCS7_VERIFY_PROTOCOL instance.
  @param[in]     Signature            Points to buffer containing ASN.1 DER-encoded PKCS
                                      detached signature.
  @param[in]     SignatureSize        The size of Signature buffer in bytes.
  @param[in]     InHash               InHash points to buffer containing the caller
                                      calculated hash of the data. The parameter may not
                                      be NULL.
  @param[in]     InHashSize           The size in bytes of InHash buffer.
  @param[in]     AllowedDb            Pointer to a list of pointers to EFI_SIGNATURE_LIST
                                      structures. The list is terminated by a null
                                      pointer. The EFI_SIGNATURE_LIST structures contain
                                      lists of X.509 certificates of approved signers.
                                      Function recognizes signer certificates of type
                                      EFI_CERT_X509_GUID. Any hash certificate in AllowedDb
                                      list is ignored by this function. Function returns
                                      success if signer of the buffer is within this list
                                      (and not within RevokedDb). This parameter is
                                      required.
  @param[in]     RevokedDb            Optional pointer to a list of pointers to
                                      EFI_SIGNATURE_LIST structures. The list is terminated
                                      by a null pointer. List of X.509 certificates of
                                      revoked signers and revoked file hashes. Signature
                                      verification will always fail if the signer of the
                                      file or the hash of the data component of the buffer
                                      is in RevokedDb list. This parameter is optional
                                      and caller may pass Null if not required.
  @param[in]     TimeStampDb          Optional pointer to a list of pointers to
                                      EFI_SIGNATURE_LIST structures. The list is terminated
                                      by a null pointer. This parameter can be used to pass
                                      a list of X.509 certificates of trusted time stamp
                                      counter-signers.

  @retval EFI_SUCCESS                 Signed hash was verified against caller-provided
                                      hash of content, the signer's certificate was not
                                      found in RevokedDb, and was found in AllowedDb or
                                      if in signer is found in both AllowedDb and
                                      RevokedDb, the signing was allowed by reference to
                                      TimeStampDb as described above, and no hash matching
                                      content hash was found in RevokedDb.
  @retval EFI_SECURITY_VIOLATION      The SignedData buffer was correctly formatted but
                                      signer was in RevokedDb or not in AllowedDb. Also
                                      returned if matching content hash found in RevokedDb.
  @retval EFI_COMPROMISED_DATA        Caller provided hash differs from signed hash. Or,
                                      caller and encrypted hash are different sizes.
  @retval EFI_INVALID_PARAMETER       Signature is NULL or SignatureSize is zero. InHash
                                      is NULL or InHashSize is zero. AllowedDb is NULL.
  @retval EFI_ABORTED                 Unsupported or invalid format in TimeStampDb,
                                      RevokedDb or AllowedDb list contents was detected.
  @retval EFI_UNSUPPORTED             The Signature buffer was not correctly formatted
                                      for processing by the function.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PKCS7_VERIFY_SIGNATURE)(
  IN EFI_PKCS7_VERIFY_PROTOCOL   *This,
  IN VOID                        *Signature,
  IN UINTN                       SignatureSize,
  IN VOID                        *InHash,
  IN UINTN                       InHashSize,
  IN EFI_SIGNATURE_LIST          **AllowedDb,
  IN EFI_SIGNATURE_LIST          **RevokedDb       OPTIONAL,
  IN EFI_SIGNATURE_LIST          **TimeStampDb     OPTIONAL
  );

///
/// The EFI_PKCS7_VERIFY_PROTOCOL is used to verify data signed using PKCS7
/// structure. The PKCS7 data to be verified must be ASN.1 (DER) encoded.
/// SHA256 must be supported as digest algorithm with RSA digest encryption.
/// Support of other hash algorithms is optional.
///
struct _EFI_PKCS7_VERIFY_PROTOCOL {
  EFI_PKCS7_VERIFY_BUFFER       VerifyBuffer;
  EFI_PKCS7_VERIFY_SIGNATURE    VerifySignature;
};

extern EFI_GUID  gEfiPkcs7VerifyProtocolGuid;

#endif
