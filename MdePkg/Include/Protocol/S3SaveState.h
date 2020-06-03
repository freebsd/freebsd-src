/** @file
  S3 Save State Protocol as defined in PI 1.6(Errata A) Specification VOLUME 5 Standard.

  This protocol is used by DXE PI module to store or record various IO operations
  to be replayed during an S3 resume.
  This protocol is not required for all platforms.

  Copyright (c) 2009 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is defined in UEFI Platform Initialization Specification 1.2 Volume 5:
  Standards

**/

#ifndef __S3_SAVE_STATE_H__
#define __S3_SAVE_STATE_H__

#define EFI_S3_SAVE_STATE_PROTOCOL_GUID \
    { 0xe857caf6, 0xc046, 0x45dc, { 0xbe, 0x3f, 0xee, 0x7, 0x65, 0xfb, 0xa8, 0x87 }}


typedef VOID *EFI_S3_BOOT_SCRIPT_POSITION;

typedef struct _EFI_S3_SAVE_STATE_PROTOCOL  EFI_S3_SAVE_STATE_PROTOCOL;

/**
  Record operations that need to be replayed during an S3 resume.

  This function is used to store an OpCode to be replayed as part of the S3 resume boot path. It is
  assumed this protocol has platform specific mechanism to store the OpCode set and replay them
  during the S3 resume.

  @param[in]    This    A pointer to the EFI_S3_SAVE_STATE_PROTOCOL instance.
  @param[in]    OpCode  The operation code (opcode) number.
  @param[in]    ...     Argument list that is specific to each opcode. See the following subsections for the
                        definition of each opcode.

  @retval EFI_SUCCESS           The operation succeeded. A record was added into the specified
                                script table.
  @retval EFI_INVALID_PARAMETER The parameter is illegal or the given boot script is not supported.
  @retval EFI_OUT_OF_RESOURCES  There is insufficient memory to store the boot script.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_S3_SAVE_STATE_WRITE)(
   IN CONST EFI_S3_SAVE_STATE_PROTOCOL  *This,
   IN       UINTN                       OpCode,
   ...
);

/**
  Record operations that need to be replayed during an S3 resume.

  This function is used to store an OpCode to be replayed as part of the S3 resume boot path. It is
  assumed this protocol has platform specific mechanism to store the OpCode set and replay them
  during the S3 resume.
  The opcode is inserted before or after the specified position in the boot script table. If Position is
  NULL then that position is after the last opcode in the table (BeforeOrAfter is TRUE) or before
  the first opcode in the table (BeforeOrAfter is FALSE). The position which is pointed to by
  Position upon return can be used for subsequent insertions.

  This function has a variable parameter list. The exact parameter list depends on the OpCode that is
  passed into the function. If an unsupported OpCode or illegal parameter list is passed in, this
  function returns EFI_INVALID_PARAMETER.
  If there are not enough resources available for storing more scripts, this function returns
  EFI_OUT_OF_RESOURCES.
  OpCode values of 0x80 - 0xFE are reserved for implementation specific functions.

  @param[in]        This            A pointer to the EFI_S3_SAVE_STATE_PROTOCOL instance.
  @param[in]        BeforeOrAfter   Specifies whether the opcode is stored before (TRUE) or after (FALSE) the position
                                    in the boot script table specified by Position. If Position is NULL or points to
                                    NULL then the new opcode is inserted at the beginning of the table (if TRUE) or end
                                    of the table (if FALSE).
  @param[in, out]   Position        On entry, specifies the position in the boot script table where the opcode will be
                                    inserted, either before or after, depending on BeforeOrAfter. On exit, specifies
                                    the position of the inserted opcode in the boot script table.
  @param[in]        OpCode          The operation code (opcode) number. See "Related Definitions" in Write() for the
                                    defined opcode types.
  @param[in]        ...             Argument list that is specific to each opcode. See the following subsections for the
                                    definition of each opcode.

  @retval EFI_SUCCESS               The operation succeeded. An opcode was added into the script.
  @retval EFI_INVALID_PARAMETER     The Opcode is an invalid opcode value.
  @retval EFI_INVALID_PARAMETER     The Position is not a valid position in the boot script table.
  @retval EFI_OUT_OF_RESOURCES      There is insufficient memory to store the boot script table.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_S3_SAVE_STATE_INSERT)(
   IN CONST EFI_S3_SAVE_STATE_PROTOCOL  *This,
   IN       BOOLEAN                     BeforeOrAfter,
   IN OUT   EFI_S3_BOOT_SCRIPT_POSITION *Position       OPTIONAL,
   IN       UINTN                       OpCode,
   ...
);

/**
  Find a label within the boot script table and, if not present, optionally create it.

  If the label Label is already exists in the boot script table, then no new label is created, the
  position of the Label is returned in *Position and EFI_SUCCESS is returned.
  If the label Label does not already exist and CreateIfNotFound is TRUE, then it will be
  created before or after the specified position and EFI_SUCCESS is returned.
  If the label Label does not already exist and CreateIfNotFound is FALSE, then
  EFI_NOT_FOUND is returned.

  @param[in]      This                A pointer to the EFI_S3_SAVE_STATE_PROTOCOL instance.
  @param[in]      BeforeOrAfter       Specifies whether the label is stored before (TRUE) or after (FALSE) the position in
                                      the boot script table specified by Position. If Position is NULL or points to
                                      NULL then the new label is inserted at the beginning of the table (if TRUE) or end of
                                      the table (if FALSE).
  @param[in]      CreateIfNotFound    Specifies whether the label will be created if the label does not exists (TRUE) or not (FALSE).
  @param[in, out] Position            On entry, specifies the position in the boot script table where the label will be inserted,
                                      either before or after, depending on BeforeOrAfter. On exit, specifies the position
                                      of the inserted label in the boot script table.
  @param[in]      Label               Points to the label which will be inserted in the boot script table.

  @retval    EFI_SUCCESS              The label already exists or was inserted.
  @retval    EFI_NOT_FOUND            The label did not already exist and CreateifNotFound was FALSE.
  @retval    EFI_INVALID_PARAMETER    The Label is NULL or points to an empty string.
  @retval    EFI_INVALID_PARAMETER    The Position is not a valid position in the boot script table.
  @retval    EFI_OUT_OF_RESOURCES     There is insufficient memory to store the boot script.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_S3_SAVE_STATE_LABEL)(
   IN CONST  EFI_S3_SAVE_STATE_PROTOCOL      *This,
   IN        BOOLEAN                         BeforeOrAfter,
   IN        BOOLEAN                         CreateIfNotFound,
   IN OUT    EFI_S3_BOOT_SCRIPT_POSITION     *Position OPTIONAL,
   IN CONST  CHAR8                           *Label
);

/**
  Compare two positions in the boot script table and return their relative position.

  This function compares two positions in the boot script table and returns their relative positions. If
  Position1 is before Position2, then -1 is returned. If Position1 is equal to Position2,
  then 0 is returned. If Position1 is after Position2, then 1 is returned.

  @param[in]    This                A pointer to the EFI_S3_SAVE_STATE_PROTOCOL instance.
  @param[in]    Position1           The positions in the boot script table to compare.
  @param[in]    Position2           The positions in the boot script table to compare.
  @param[out]   RelativePosition    On return, points to the result of the comparison.

  @retval   EFI_SUCCESS             The operation succeeded.
  @retval   EFI_INVALID_PARAMETER   The Position1 or Position2 is not a valid position in the boot script table.
  @retval   EFI_INVALID_PARAMETER   The RelativePosition is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_S3_SAVE_STATE_COMPARE)(
   IN CONST EFI_S3_SAVE_STATE_PROTOCOL          *This,
   IN       EFI_S3_BOOT_SCRIPT_POSITION         Position1,
   IN       EFI_S3_BOOT_SCRIPT_POSITION         Position2,
   OUT      UINTN                               *RelativePosition
);

struct _EFI_S3_SAVE_STATE_PROTOCOL {
  EFI_S3_SAVE_STATE_WRITE   Write;
  EFI_S3_SAVE_STATE_INSERT  Insert;
  EFI_S3_SAVE_STATE_LABEL   Label;
  EFI_S3_SAVE_STATE_COMPARE Compare;
};

extern EFI_GUID gEfiS3SaveStateProtocolGuid;

#endif // __S3_SAVE_STATE_H__
