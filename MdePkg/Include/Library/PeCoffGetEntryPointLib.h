/** @file
  Provides a service to retrieve the PE/COFF entry point from a PE/COFF image.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __PE_COFF_GET_ENTRY_POINT_LIB_H__
#define __PE_COFF_GET_ENTRY_POINT_LIB_H__

/**
  Retrieves and returns a pointer to the entry point to a PE/COFF image that has been loaded
  into system memory with the PE/COFF Loader Library functions.

  Retrieves the entry point to the PE/COFF image specified by Pe32Data and returns this entry
  point in EntryPoint.  If the entry point could not be retrieved from the PE/COFF image, then
  return RETURN_INVALID_PARAMETER.  Otherwise return RETURN_SUCCESS.
  If Pe32Data is NULL, then ASSERT().
  If EntryPoint is NULL, then ASSERT().

  @param  Pe32Data                  The pointer to the PE/COFF image that is loaded in system memory.
  @param  EntryPoint                The pointer to entry point to the PE/COFF image to return.

  @retval RETURN_SUCCESS            EntryPoint was returned.
  @retval RETURN_INVALID_PARAMETER  The entry point could not be found in the PE/COFF image.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderGetEntryPoint (
  IN  VOID  *Pe32Data,
  OUT VOID  **EntryPoint
  );

/**
  Returns the machine type of a PE/COFF image.

  Returns the machine type from the PE/COFF image specified by Pe32Data.
  If Pe32Data is NULL, then ASSERT().

  @param  Pe32Data   The pointer to the PE/COFF image that is loaded in system
                     memory.

  @return Machine type or zero if not a valid image.

**/
UINT16
EFIAPI
PeCoffLoaderGetMachineType (
  IN VOID  *Pe32Data
  );

/**
  Returns a pointer to the PDB file name for a PE/COFF image that has been
  loaded into system memory with the PE/COFF Loader Library functions.

  Returns the PDB file name for the PE/COFF image specified by Pe32Data.  If
  the PE/COFF image specified by Pe32Data is not a valid, then NULL is
  returned.  If the PE/COFF image specified by Pe32Data does not contain a
  debug directory entry, then NULL is returned.  If the debug directory entry
  in the PE/COFF image specified by Pe32Data does not contain a PDB file name,
  then NULL is returned.
  If Pe32Data is NULL, then ASSERT().

  @param  Pe32Data   The pointer to the PE/COFF image that is loaded in system
                     memory.

  @return The PDB file name for the PE/COFF image specified by Pe32Data, or NULL
          if it cannot be retrieved.

**/
VOID *
EFIAPI
PeCoffLoaderGetPdbPointer (
  IN VOID  *Pe32Data
  );


/**
  Returns the size of the PE/COFF headers

  Returns the size of the PE/COFF header specified by Pe32Data.
  If Pe32Data is NULL, then ASSERT().

  @param  Pe32Data   The pointer to the PE/COFF image that is loaded in system
                     memory.

  @return Size of PE/COFF header in bytes, or zero if not a valid image.

**/
UINT32
EFIAPI
PeCoffGetSizeOfHeaders (
  IN VOID     *Pe32Data
  );

/**
  Returns PE/COFF image base specified by the address in this PE/COFF image.

  On DEBUG build, searches the PE/COFF image base forward the address in this
  PE/COFF image and returns it.

  @param  Address    Address located in one PE/COFF image.

  @retval 0          RELEASE build or cannot find the PE/COFF image base.
  @retval others     PE/COFF image base found.

**/
UINTN
EFIAPI
PeCoffSearchImageBase (
  IN UINTN    Address
  );

#endif
