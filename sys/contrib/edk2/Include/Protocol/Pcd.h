/** @file
  Native Platform Configuration Database (PCD) Protocol

  Different with the EFI_PCD_PROTOCOL defined in PI 1.2 specification, the native
  PCD protocol provide interfaces for dynamic and dynamic-ex type PCD.
  The interfaces in dynamic type PCD do not require the token space guid as parameter,
  but interfaces in dynamic-ex type PCD require token space guid as parameter.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This Protocol was introduced in PI Specification 1.2.

**/

#ifndef __PCD_H__
#define __PCD_H__

extern EFI_GUID  gPcdProtocolGuid;

#define PCD_PROTOCOL_GUID \
  { 0x11b34006, 0xd85b, 0x4d0a, { 0xa2, 0x90, 0xd5, 0xa5, 0x71, 0x31, 0xe, 0xf7 } }

#define PCD_INVALID_TOKEN_NUMBER  ((UINTN) 0)

/**
  Sets the SKU value for subsequent calls to set or get PCD token values.

  SetSku() sets the SKU Id to be used for subsequent calls to set or get PCD values.
  SetSku() is normally called only once by the system.

  For each item (token), the database can hold a single value that applies to all SKUs,
  or multiple values, where each value is associated with a specific SKU Id. Items with multiple,
  SKU-specific values are called SKU enabled.

  The SKU Id of zero is reserved as a default. The valid SkuId range is 1 to 255.
  For tokens that are not SKU enabled, the system ignores any set SKU Id and works with the
  single value for that token. For SKU-enabled tokens, the system will use the SKU Id set by the
  last call to SetSku(). If no SKU Id is set or the currently set SKU Id isn't valid for the specified token,
  the system uses the default SKU Id. If the system attempts to use the default SKU Id and no value has been
  set for that Id, the results are unpredictable.

  @param[in]  SkuId The SKU value that will be used when the PCD service will retrieve and
                    set values associated with a PCD token.


**/
typedef
VOID
(EFIAPI *PCD_PROTOCOL_SET_SKU)(
  IN  UINTN                  SkuId
  );

/**
  Retrieves an 8-bit value for a given PCD token.

  Retrieves the current byte-sized value for a PCD token number.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The UINT8 value.

**/
typedef
UINT8
(EFIAPI *PCD_PROTOCOL_GET8)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves a 16-bit value for a given PCD token.

  Retrieves the current 16-bit value for a PCD token number.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The UINT16 value.

**/
typedef
UINT16
(EFIAPI *PCD_PROTOCOL_GET16)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves a 32-bit value for a given PCD token.

  Retrieves the current 32-bit value for a PCD token number.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The UINT32 value.

**/
typedef
UINT32
(EFIAPI *PCD_PROTOCOL_GET32)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves a 64-bit value for a given PCD token.

  Retrieves the current 64-bit value for a PCD token number.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The UINT64 value.

**/
typedef
UINT64
(EFIAPI *PCD_PROTOCOL_GET64)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves a pointer to a value for a given PCD token.

  Retrieves the current pointer to the buffer for a PCD token number.
  Do not make any assumptions about the alignment of the pointer that
  is returned by this function call.  If the TokenNumber is invalid,
  the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The pointer to the buffer to be retrived.

**/
typedef
VOID *
(EFIAPI *PCD_PROTOCOL_GET_POINTER)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves a Boolean value for a given PCD token.

  Retrieves the current boolean value for a PCD token number.
  Do not make any assumptions about the alignment of the pointer that
  is returned by this function call.  If the TokenNumber is invalid,
  the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The Boolean value.

**/
typedef
BOOLEAN
(EFIAPI *PCD_PROTOCOL_GET_BOOLEAN)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves the size of the value for a given PCD token.

  Retrieves the current size of a particular PCD token.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  TokenNumber The PCD token number.

  @return The size of the value for the PCD token.

**/
typedef
UINTN
(EFIAPI *PCD_PROTOCOL_GET_SIZE)(
  IN UINTN             TokenNumber
  );

/**
  Retrieves an 8-bit value for a given PCD token.

  Retrieves the 8-bit value of a particular PCD token.
  If the TokenNumber is invalid or the token space
  specified by Guid does not exist, the results are
  unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size 8-bit value for the PCD token.

**/
typedef
UINT8
(EFIAPI *PCD_PROTOCOL_GET_EX_8)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves a 16-bit value for a given PCD token.

  Retrieves the 16-bit value of a particular PCD token.
  If the TokenNumber is invalid or the token space
  specified by Guid does not exist, the results are
  unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size 16-bit value for the PCD token.

**/
typedef
UINT16
(EFIAPI *PCD_PROTOCOL_GET_EX_16)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves a 32-bit value for a given PCD token.

  Retrieves the 32-bit value of a particular PCD token.
  If the TokenNumber is invalid or the token space
  specified by Guid does not exist, the results are
  unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size 32-bit value for the PCD token.

**/
typedef
UINT32
(EFIAPI *PCD_PROTOCOL_GET_EX_32)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves an 64-bit value for a given PCD token.

  Retrieves the 64-bit value of a particular PCD token.
  If the TokenNumber is invalid or the token space
  specified by Guid does not exist, the results are
  unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size 64-bit value for the PCD token.

**/
typedef
UINT64
(EFIAPI *PCD_PROTOCOL_GET_EX_64)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves a pointer to a value for a given PCD token.

  Retrieves the current pointer to the buffer for a PCD token number.
  Do not make any assumptions about the alignment of the pointer that
  is returned by this function call.  If the TokenNumber is invalid,
  the results are unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The pointer to the buffer to be retrieved.

**/
typedef
VOID *
(EFIAPI *PCD_PROTOCOL_GET_EX_POINTER)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves a Boolean value for a given PCD token.

  Retrieves the Boolean value of a particular PCD token.
  If the TokenNumber is invalid or the token space
  specified by Guid does not exist, the results are
  unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size Boolean value for the PCD token.

**/
typedef
BOOLEAN
(EFIAPI *PCD_PROTOCOL_GET_EX_BOOLEAN)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Retrieves the size of the value for a given PCD token.

  Retrieves the current size of a particular PCD token.
  If the TokenNumber is invalid, the results are unpredictable.

  @param[in]  Guid        The token space for the token number.
  @param[in]  TokenNumber The PCD token number.

  @return The size of the value for the PCD token.

**/
typedef
UINTN
(EFIAPI *PCD_PROTOCOL_GET_EX_SIZE)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber
  );

/**
  Sets an 8-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET8)(
  IN UINTN             TokenNumber,
  IN UINT8             Value
  );

/**
  Sets a 16-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET16)(
  IN UINTN              TokenNumber,
  IN UINT16             Value
  );

/**
  Sets a 32-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET32)(
  IN UINTN              TokenNumber,
  IN UINT32             Value
  );

/**
  Sets a 64-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET64)(
  IN UINTN             TokenNumber,
  IN UINT64            Value
  );

/**
  Sets a value of a specified size for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]      TokenNumber  The PCD token number.
  @param[in, out] SizeOfBuffer A pointer to the length of the value being set for the PCD token.
                               On input, if the SizeOfValue is greater than the maximum size supported
                               for this TokenNumber then the output value of SizeOfValue will reflect
                               the maximum size supported for this TokenNumber.
  @param[in]      Buffer       The buffer to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_POINTER)(
  IN      UINTN             TokenNumber,
  IN OUT  UINTN             *SizeOfBuffer,
  IN      VOID              *Buffer
  );

/**
  Sets a Boolean value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_BOOLEAN)(
  IN UINTN             TokenNumber,
  IN BOOLEAN           Value
  );

/**
  Sets an 8-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_8)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber,
  IN UINT8             Value
  );

/**
  Sets an 16-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_16)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber,
  IN UINT16            Value
  );

/**
  Sets a 32-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_32)(
  IN CONST EFI_GUID     *Guid,
  IN UINTN              TokenNumber,
  IN UINT32             Value
  );

/**
  Sets a 64-bit value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_64)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber,
  IN UINT64            Value
  );

/**
  Sets a value of a specified size for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid            The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber     The PCD token number.
  @param[in, out] SizeOfBuffer A pointer to the length of the value being set for the PCD token.
                              On input, if the SizeOfValue is greater than the maximum size supported
                              for this TokenNumber then the output value of SizeOfValue will reflect
                              the maximum size supported for this TokenNumber.
  @param[in]  Buffer          The buffer to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_POINTER)(
  IN CONST EFI_GUID          *Guid,
  IN       UINTN             TokenNumber,
  IN OUT   UINTN             *SizeOfBuffer,
  IN       VOID              *Buffer
  );

/**
  Sets a Boolean value for a given PCD token.

  When the PCD service sets a value, it will check to ensure that the
  size of the value being set is compatible with the Token's existing definition.
  If it is not, an error will be returned.

  @param[in]  Guid        The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  TokenNumber The PCD token number.
  @param[in]  Value       The value to set for the PCD token.

  @retval EFI_SUCCESS  The procedure returned successfully.
  @retval EFI_INVALID_PARAMETER The PCD service determined that the size of the data
                                  being set was incompatible with a call to this function.
                                  Use GetSize() to retrieve the size of the target data.
  @retval EFI_NOT_FOUND The PCD service could not find the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_SET_EX_BOOLEAN)(
  IN CONST EFI_GUID    *Guid,
  IN UINTN             TokenNumber,
  IN BOOLEAN           Value
  );

/**
  Callback on SET function prototype definition.

  This notification function serves two purposes.
  Firstly, it notifies the module which did the registration that the value
  of this PCD token has been set. Secondly, it provides a mechanism for the
  module that did the registration to intercept the set operation and override
  the value that has been set, if necessary. After the invocation of the callback function,
  TokenData will be used by PCD service DXE driver to modify the internal data in
  PCD database.

  @param[in]  CallBackGuid    The PCD token GUID being set.
  @param[in]  CallBackToken   The PCD token number being set.
  @param[in, out] TokenData   A pointer to the token data being set.
  @param[in]  TokenDataSize   The size, in bytes, of the data being set.

  @retval VOID

**/
typedef
VOID
(EFIAPI *PCD_PROTOCOL_CALLBACK)(
  IN CONST EFI_GUID         *CallBackGuid  OPTIONAL,
  IN       UINTN            CallBackToken,
  IN OUT   VOID             *TokenData,
  IN       UINTN            TokenDataSize
  );

/**
  Specifies a function to be called anytime the value of a designated token is changed.

  @param[in]  TokenNumber       The PCD token number.
  @param[in]  Guid              The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  CallBackFunction  The function prototype called when the value associated with the CallBackToken is set.

  @retval EFI_SUCCESS   The PCD service has successfully established a call event
                        for the CallBackToken requested.
  @retval EFI_NOT_FOUND The PCD service could not find the referenced token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_CALLBACK_ONSET)(
  IN  CONST EFI_GUID          *Guid  OPTIONAL,
  IN  UINTN                   TokenNumber,
  IN  PCD_PROTOCOL_CALLBACK   CallBackFunction
  );

/**
  Cancels a previously set callback function for a particular PCD token number.

  @param[in]  TokenNumber      The PCD token number.
  @param[in]  Guid             The 128-bit unique value that designates the namespace from which to extract the value.
  @param[in]  CallBackFunction The function prototype called when the value associated with the CallBackToken is set.

  @retval EFI_SUCCESS   The PCD service has successfully established a call event
                        for the CallBackToken requested.
  @retval EFI_NOT_FOUND The PCD service could not find the referenced token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_CANCEL_CALLBACK)(
  IN  CONST EFI_GUID          *Guid  OPTIONAL,
  IN  UINTN                   TokenNumber,
  IN  PCD_PROTOCOL_CALLBACK   CallBackFunction
  );

/**
  Retrieves the next valid token number in a given namespace.

  This is useful since the PCD infrastructure contains a sparse list of token numbers,
  and one cannot a priori know what token numbers are valid in the database.

  If TokenNumber is 0 and Guid is not NULL, then the first token from the token space specified by Guid is returned.
  If TokenNumber is not 0 and Guid is not NULL, then the next token in the token space specified by Guid is returned.
  If TokenNumber is 0 and Guid is NULL, then the first token in the default token space is returned.
  If TokenNumber is not 0 and Guid is NULL, then the next token in the default token space is returned.
  The token numbers in the default token space may not be related to token numbers in token spaces that are named by Guid.
  If the next token number can be retrieved, then it is returned in TokenNumber, and EFI_SUCCESS is returned.
  If TokenNumber represents the last token number in the token space specified by Guid, then EFI_NOT_FOUND is returned.
  If TokenNumber is not present in the token space specified by Guid, then EFI_NOT_FOUND is returned.


  @param[in]      Guid    The 128-bit unique value that designates the namespace from which to retrieve the next token.
                          This is an optional parameter that may be NULL. If this parameter is NULL, then a request is
                          being made to retrieve tokens from the default token space.
  @param[in,out]  TokenNumber
                          A pointer to the PCD token number to use to find the subsequent token number.

  @retval EFI_SUCCESS   The PCD service has retrieved the next valid token number.
  @retval EFI_NOT_FOUND The PCD service could not find data from the requested token number.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_GET_NEXT_TOKEN)(
  IN      CONST EFI_GUID      *Guid  OPTIONAL,
  IN OUT  UINTN               *TokenNumber
  );

/**
  Retrieves the next valid PCD token namespace for a given namespace.

  Gets the next valid token namespace for a given namespace. This is useful to traverse the valid
  token namespaces on a platform.

  @param[in, out]   Guid    An indirect pointer to EFI_GUID. On input it designates a known token namespace
                            from which the search will start. On output, it designates the next valid token
                            namespace on the platform. If *Guid is NULL, then the GUID of the first token
                            space of the current platform is returned. If the search cannot locate the next valid
                            token namespace, an error is returned and the value of *Guid is undefined.

  @retval   EFI_SUCCESS   The PCD service retrieved the value requested.
  @retval   EFI_NOT_FOUND The PCD service could not find the next valid token namespace.

**/
typedef
EFI_STATUS
(EFIAPI *PCD_PROTOCOL_GET_NEXT_TOKENSPACE)(
  IN  OUT    CONST EFI_GUID      **Guid
  );

///
/// This service abstracts the ability to set/get Platform Configuration Database (PCD).
///
typedef struct {
  PCD_PROTOCOL_SET_SKU                SetSku;

  PCD_PROTOCOL_GET8                   Get8;
  PCD_PROTOCOL_GET16                  Get16;
  PCD_PROTOCOL_GET32                  Get32;
  PCD_PROTOCOL_GET64                  Get64;
  PCD_PROTOCOL_GET_POINTER            GetPtr;
  PCD_PROTOCOL_GET_BOOLEAN            GetBool;
  PCD_PROTOCOL_GET_SIZE               GetSize;

  PCD_PROTOCOL_GET_EX_8               Get8Ex;
  PCD_PROTOCOL_GET_EX_16              Get16Ex;
  PCD_PROTOCOL_GET_EX_32              Get32Ex;
  PCD_PROTOCOL_GET_EX_64              Get64Ex;
  PCD_PROTOCOL_GET_EX_POINTER         GetPtrEx;
  PCD_PROTOCOL_GET_EX_BOOLEAN         GetBoolEx;
  PCD_PROTOCOL_GET_EX_SIZE            GetSizeEx;

  PCD_PROTOCOL_SET8                   Set8;
  PCD_PROTOCOL_SET16                  Set16;
  PCD_PROTOCOL_SET32                  Set32;
  PCD_PROTOCOL_SET64                  Set64;
  PCD_PROTOCOL_SET_POINTER            SetPtr;
  PCD_PROTOCOL_SET_BOOLEAN            SetBool;

  PCD_PROTOCOL_SET_EX_8               Set8Ex;
  PCD_PROTOCOL_SET_EX_16              Set16Ex;
  PCD_PROTOCOL_SET_EX_32              Set32Ex;
  PCD_PROTOCOL_SET_EX_64              Set64Ex;
  PCD_PROTOCOL_SET_EX_POINTER         SetPtrEx;
  PCD_PROTOCOL_SET_EX_BOOLEAN         SetBoolEx;

  PCD_PROTOCOL_CALLBACK_ONSET         CallbackOnSet;
  PCD_PROTOCOL_CANCEL_CALLBACK        CancelCallback;
  PCD_PROTOCOL_GET_NEXT_TOKEN         GetNextToken;
  PCD_PROTOCOL_GET_NEXT_TOKENSPACE    GetNextTokenSpace;
} PCD_PROTOCOL;

#endif
