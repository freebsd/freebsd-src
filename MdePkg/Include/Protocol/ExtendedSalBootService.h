/** @file
  Definition of Extended SAL Boot Service Protocol

  The Extended SAL Boot Service Protocol provides a mechanisms for platform specific 
  drivers to update the SAL System Table and register Extended SAL Procedures that are
  callable in physical or virtual mode using the SAL calling convention.

  Copyright (c) 2009 - 2010, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef _EXTENDED_SAL_BOOT_SERVICE_PROTOCOL_H_
#define _EXTENDED_SAL_BOOT_SERVICE_PROTOCOL_H_

#include <IndustryStandard/Sal.h>

#define EXTENDED_SAL_BOOT_SERVICE_PROTOCOL_GUID   \
  { 0xde0ee9a4, 0x3c7a, 0x44f2, {0xb7, 0x8b, 0xe3, 0xcc, 0xd6, 0x9c, 0x3a, 0xf7 } }

typedef struct _EXTENDED_SAL_BOOT_SERVICE_PROTOCOL EXTENDED_SAL_BOOT_SERVICE_PROTOCOL;

/**
  Adds platform specific information to the to the header of the SAL System Table.

  @param  This                  A pointer to the EXTENDED_SAL_BOOT_SERVICE_PROTOCOL instance.
  @param  SalAVersion           Version of recovery SAL PEIM(s) in BCD format. Higher byte contains
                                the major revision and the lower byte contains the minor revision.
  @param  SalBVersion           Version of DXE SAL Driver in BCD format. Higher byte contains
                                the major revision and the lower byte contains the minor revision.
  @param  OemId                 A pointer to a Null-terminated ASCII string that contains OEM unique string.
                                The string cannot be longer than 32 bytes in total length
  @param  ProductId             A pointer to a Null-terminated ASCII string that uniquely identifies a family of 
                                compatible products. The string cannot be longer than 32 bytes in total length.

  @retval EFI_SUCCESS           The SAL System Table header was updated successfully.
  @retval EFI_INVALID_PARAMETER OemId is NULL.
  @retval EFI_INVALID_PARAMETER ProductId is NULL.
  @retval EFI_INVALID_PARAMETER The length of OemId is greater than 32 characters.
  @retval EFI_INVALID_PARAMETER The length of ProductId is greater than 32 characters.

**/
typedef
EFI_STATUS
(EFIAPI *EXTENDED_SAL_ADD_SST_INFO)(
  IN EXTENDED_SAL_BOOT_SERVICE_PROTOCOL  *This,
  IN UINT16                              SalAVersion,
  IN UINT16                              SalBVersion,
  IN CHAR8                               *OemId,
  IN CHAR8                               *ProductId
  );

/**
  Adds an entry to the SAL System Table.

  This function adds the SAL System Table Entry specified by TableEntry and EntrySize
  to the SAL System Table.

  @param  This         A pointer to the EXTENDED_SAL_BOOT_SERVICE_PROTOCOL instance.
  @param  TableEntry   Pointer to a buffer containing a SAL System Table entry that is EntrySize bytes 
                       in length. The first byte of the TableEntry describes the type of entry.
  @param  EntrySize    The size, in bytes, of TableEntry.

  @retval EFI_SUCCESSThe        SAL System Table was updated successfully.
  @retval EFI_INVALID_PARAMETER TableEntry is NULL.
  @retval EFI_INVALID_PARAMETER TableEntry specifies an invalid entry type.
  @retval EFI_INVALID_PARAMETER EntrySize is not valid for this type of entry.

**/
typedef
EFI_STATUS
(EFIAPI *EXTENDED_SAL_ADD_SST_ENTRY)(
  IN EXTENDED_SAL_BOOT_SERVICE_PROTOCOL  *This,
  IN UINT8                               *TableEntry,
  IN UINTN                               EntrySize
  );

/**
  Internal ESAL procedures.

  This is prototype of internal Extended SAL procedures, which is registerd by
  EXTENDED_SAL_REGISTER_INTERNAL_PROC service.

  @param  FunctionId         The Function ID associated with this Extended SAL Procedure.
  @param  Arg2               Second argument to the Extended SAL procedure.
  @param  Arg3               Third argument to the Extended SAL procedure.
  @param  Arg4               Fourth argument to the Extended SAL procedure.
  @param  Arg5               Fifth argument to the Extended SAL procedure.
  @param  Arg6               Sixth argument to the Extended SAL procedure.
  @param  Arg7               Seventh argument to the Extended SAL procedure.
  @param  Arg8               Eighth argument to the Extended SAL procedure.
  @param  VirtualMode        TRUE if the Extended SAL Procedure is being invoked in virtual mode.
                             FALSE if the Extended SAL Procedure is being invoked in physical mode.
  @param  ModuleGlobal       A pointer to the global context associated with this Extended SAL Procedure. 

  @return The result returned from the specified Extended SAL Procedure

**/
typedef
SAL_RETURN_REGS
(EFIAPI *SAL_INTERNAL_EXTENDED_SAL_PROC)(
  IN  UINT64   FunctionId,
  IN  UINT64   Arg2,
  IN  UINT64   Arg3,
  IN  UINT64   Arg4,
  IN  UINT64   Arg5,
  IN  UINT64   Arg6,
  IN  UINT64   Arg7,
  IN  UINT64   Arg8,
  IN  BOOLEAN  VirtualMode,
  IN  VOID     *ModuleGlobal  OPTIONAL
  ); 

/**
  Registers an Extended SAL Procedure.

  The Extended SAL Procedure specified by InternalSalProc and named by ClassGuidLo,
  ClassGuidHi, and FunctionId is added to the set of available Extended SAL Procedures.

  @param  This                   A pointer to the EXTENDED_SAL_BOOT_SERVICE_PROTOCOL instance.
  @param  ClassGuidLo            The lower 64-bits of  the class GUID for the Extended SAL Procedure being added.  
                                 Each class GUID contains one or more functions specified by a Function ID.
  @param  ClassGuidHi            The upper 64-bits of  the class GUID for the Extended SAL Procedure being added.  
                                 Each class GUID contains one or more functions specified by a Function ID.
  @param  FunctionId             The Function ID for the Extended SAL Procedure that is being added.  This Function 
                                 ID is a member of the Extended SAL Procedure class specified by ClassGuidLo 
                                 and ClassGuidHi.
  @param  InternalSalProc        A pointer to the Extended SAL Procedure being added.
  @param  PhysicalModuleGlobal   Pointer to a  module global structure. This is a physical mode pointer.
                                 This pointer is passed to the Extended SAL Procedure specified by ClassGuidLo, 
                                 ClassGuidHi, FunctionId, and InternalSalProc.  If the system is in physical mode,
                                 then this pointer is passed unmodified to InternalSalProc.  If the system is in
                                 virtual mode, then the virtual address associated with this pointer is passed to
                                 InternalSalProc.

  @retval EFI_SUCCESS            The Extended SAL Procedure was added.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources available to add the Extended SAL Procedure.

**/
typedef
EFI_STATUS
(EFIAPI *EXTENDED_SAL_REGISTER_INTERNAL_PROC)(
  IN EXTENDED_SAL_BOOT_SERVICE_PROTOCOL  *This,
  IN UINT64                              ClassGuidLo,
  IN UINT64                              ClassGuidHi,
  IN UINT64                              FunctionId,
  IN SAL_INTERNAL_EXTENDED_SAL_PROC      InternalSalProc,
  IN VOID                                *PhysicalModuleGlobal  OPTIONAL
  );

/**
  Calls a previously registered Extended SAL Procedure.

  This function calls the Extended SAL Procedure specified by ClassGuidLo, ClassGuidHi, 
  and FunctionId.  The set of previously registered Extended SAL Procedures is searched for a 
  matching ClassGuidLo, ClassGuidHi, and FunctionId.  If a match is not found, then 
  EFI_SAL_NOT_IMPLEMENTED is returned.

  @param  ClassGuidLo        The lower 64-bits of the class GUID for the Extended SAL Procedure
                             that is being called.
  @param  ClassGuidHi        The upper 64-bits of the class GUID for the Extended SAL Procedure
                             that is being called.
  @param  FunctionId         Function ID for the Extended SAL Procedure being called.
  @param  Arg2               Second argument to the Extended SAL procedure.
  @param  Arg3               Third argument to the Extended SAL procedure.
  @param  Arg4               Fourth argument to the Extended SAL procedure.
  @param  Arg5               Fifth argument to the Extended SAL procedure.
  @param  Arg6               Sixth argument to the Extended SAL procedure.
  @param  Arg7               Seventh argument to the Extended SAL procedure.
  @param  Arg8               Eighth argument to the Extended SAL procedure.

  @retval EFI_SAL_NOT_IMPLEMENTED        The Extended SAL Procedure specified by ClassGuidLo, 
                                         ClassGuidHi, and FunctionId has not been registered.
  @retval EFI_SAL_VIRTUAL_ADDRESS_ERROR  This function was called in virtual mode before virtual mappings 
                                         for the specified Extended SAL Procedure are available.
  @retval Other                          The result returned from the specified Extended SAL Procedure

**/
typedef
SAL_RETURN_REGS
(EFIAPI *EXTENDED_SAL_PROC)(
  IN UINT64  ClassGuidLo,
  IN UINT64  ClassGuidHi,
  IN UINT64  FunctionId,
  IN UINT64  Arg2,
  IN UINT64  Arg3,
  IN UINT64  Arg4,
  IN UINT64  Arg5,
  IN UINT64  Arg6,
  IN UINT64  Arg7,
  IN UINT64  Arg8
  );

///
/// The EXTENDED_SAL_BOOT_SERVICE_PROTOCOL provides a mechanisms for platform specific 
/// drivers to update the SAL System Table and register Extended SAL Procedures that are
/// callable in physical or virtual mode using the SAL calling convention.
///
struct _EXTENDED_SAL_BOOT_SERVICE_PROTOCOL {
  EXTENDED_SAL_ADD_SST_INFO            AddSalSystemTableInfo;
  EXTENDED_SAL_ADD_SST_ENTRY           AddSalSystemTableEntry;
  EXTENDED_SAL_REGISTER_INTERNAL_PROC  RegisterExtendedSalProc;   
  EXTENDED_SAL_PROC                    ExtendedSalProc;
};

extern EFI_GUID  gEfiExtendedSalBootServiceProtocolGuid;

#endif
