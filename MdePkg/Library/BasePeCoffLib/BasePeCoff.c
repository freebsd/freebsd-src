/** @file
  Base PE/COFF loader supports loading any PE32/PE32+ or TE image, but
  only supports relocating IA32, x64, IPF, ARM, RISC-V and EBC images.

  Caution: This file requires additional review when modified.
  This library will have external input - PE/COFF image.
  This external input must be validated carefully to avoid security issue like
  buffer overflow, integer overflow.

  The basic guideline is that caller need provide ImageContext->ImageRead () with the
  necessary data range check, to make sure when this library reads PE/COFF image, the
  PE image buffer is always in valid range.
  This library will also do some additional check for PE header fields.

  PeCoffLoaderGetPeHeader() routine will do basic check for PE/COFF header.
  PeCoffLoaderGetImageInfo() routine will do basic check for whole PE/COFF image.

  Copyright (c) 2006 - 2019, Intel Corporation. All rights reserved.<BR>
  Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
  Portions Copyright (c) 2020, Hewlett Packard Enterprise Development LP. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BasePeCoffLibInternals.h"

/**
  Adjust some fields in section header for TE image.

  @param  SectionHeader             Pointer to the section header.
  @param  TeStrippedOffset          Size adjust for the TE image.

**/
VOID
PeCoffLoaderAdjustOffsetForTeImage (
  EFI_IMAGE_SECTION_HEADER              *SectionHeader,
  UINT32                                TeStrippedOffset
  )
{
  SectionHeader->VirtualAddress   -= TeStrippedOffset;
  SectionHeader->PointerToRawData -= TeStrippedOffset;
}

/**
  Retrieves the PE or TE Header from a PE/COFF or TE image.

  Caution: This function may receive untrusted input.
  PE/COFF image is external input, so this routine will
  also done many checks in PE image to make sure PE image DosHeader, PeOptionHeader,
  SizeOfHeader, Section Data Region and Security Data Region be in PE image range.

  @param  ImageContext    The context of the image being loaded.
  @param  Hdr             The buffer in which to return the PE32, PE32+, or TE header.

  @retval RETURN_SUCCESS  The PE or TE Header is read.
  @retval Other           The error status from reading the PE/COFF or TE image using the ImageRead function.

**/
RETURN_STATUS
PeCoffLoaderGetPeHeader (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT         *ImageContext,
  OUT    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION  Hdr
  )
{
  RETURN_STATUS         Status;
  EFI_IMAGE_DOS_HEADER  DosHdr;
  UINTN                 Size;
  UINTN                 ReadSize;
  UINT32                SectionHeaderOffset;
  UINT32                Index;
  UINT32                HeaderWithoutDataDir;
  CHAR8                 BufferData;
  UINTN                 NumberOfSections;
  EFI_IMAGE_SECTION_HEADER  SectionHeader;

  //
  // Read the DOS image header to check for its existence
  //
  Size = sizeof (EFI_IMAGE_DOS_HEADER);
  ReadSize = Size;
  Status = ImageContext->ImageRead (
                           ImageContext->Handle,
                           0,
                           &Size,
                           &DosHdr
                           );
  if (RETURN_ERROR (Status) || (Size != ReadSize)) {
    ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
    if (Size != ReadSize) {
      Status = RETURN_UNSUPPORTED;
    }
    return Status;
  }

  ImageContext->PeCoffHeaderOffset = 0;
  if (DosHdr.e_magic == EFI_IMAGE_DOS_SIGNATURE) {
    //
    // DOS image header is present, so read the PE header after the DOS image
    // header
    //
    ImageContext->PeCoffHeaderOffset = DosHdr.e_lfanew;
  }

  //
  // Read the PE/COFF Header. For PE32 (32-bit) this will read in too much
  // data, but that should not hurt anything. Hdr.Pe32->OptionalHeader.Magic
  // determines if this is a PE32 or PE32+ image. The magic is in the same
  // location in both images.
  //
  Size = sizeof (EFI_IMAGE_OPTIONAL_HEADER_UNION);
  ReadSize = Size;
  Status = ImageContext->ImageRead (
                           ImageContext->Handle,
                           ImageContext->PeCoffHeaderOffset,
                           &Size,
                           Hdr.Pe32
                           );
  if (RETURN_ERROR (Status) || (Size != ReadSize)) {
    ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
    if (Size != ReadSize) {
      Status = RETURN_UNSUPPORTED;
    }
    return Status;
  }

  //
  // Use Signature to figure out if we understand the image format
  //
  if (Hdr.Te->Signature == EFI_TE_IMAGE_HEADER_SIGNATURE) {
    ImageContext->IsTeImage         = TRUE;
    ImageContext->Machine           = Hdr.Te->Machine;
    ImageContext->ImageType         = (UINT16)(Hdr.Te->Subsystem);
    //
    // For TeImage, SectionAlignment is undefined to be set to Zero
    // ImageSize can be calculated.
    //
    ImageContext->ImageSize         = 0;
    ImageContext->SectionAlignment  = 0;
    ImageContext->SizeOfHeaders     = sizeof (EFI_TE_IMAGE_HEADER) + (UINTN)Hdr.Te->BaseOfCode - (UINTN)Hdr.Te->StrippedSize;

    //
    // Check the StrippedSize.
    //
    if (sizeof (EFI_TE_IMAGE_HEADER) >= (UINT32)Hdr.Te->StrippedSize) {
      ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
      return RETURN_UNSUPPORTED;
    }

    //
    // Check the SizeOfHeaders field.
    //
    if (Hdr.Te->BaseOfCode <= Hdr.Te->StrippedSize) {
      ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
      return RETURN_UNSUPPORTED;
    }

    //
    // Read last byte of Hdr.Te->SizeOfHeaders from the file.
    //
    Size = 1;
    ReadSize = Size;
    Status = ImageContext->ImageRead (
                             ImageContext->Handle,
                             ImageContext->SizeOfHeaders - 1,
                             &Size,
                             &BufferData
                             );
    if (RETURN_ERROR (Status) || (Size != ReadSize)) {
      ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
      if (Size != ReadSize) {
        Status = RETURN_UNSUPPORTED;
      }
      return Status;
    }

    //
    // TE Image Data Directory Entry size is non-zero, but the Data Directory Virtual Address is zero.
    // This case is not a valid TE image.
    //
    if ((Hdr.Te->DataDirectory[0].Size != 0 && Hdr.Te->DataDirectory[0].VirtualAddress == 0) ||
        (Hdr.Te->DataDirectory[1].Size != 0 && Hdr.Te->DataDirectory[1].VirtualAddress == 0)) {
      ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
      return RETURN_UNSUPPORTED;
    }
  } else if (Hdr.Pe32->Signature == EFI_IMAGE_NT_SIGNATURE)  {
    ImageContext->IsTeImage = FALSE;
    ImageContext->Machine = Hdr.Pe32->FileHeader.Machine;

    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // 1. Check OptionalHeader.NumberOfRvaAndSizes filed.
      //
      if (EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES < Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // 2. Check the FileHeader.SizeOfOptionalHeader field.
      // OptionalHeader.NumberOfRvaAndSizes is not bigger than 16, so
      // OptionalHeader.NumberOfRvaAndSizes * sizeof (EFI_IMAGE_DATA_DIRECTORY) will not overflow.
      //
      HeaderWithoutDataDir = sizeof (EFI_IMAGE_OPTIONAL_HEADER32) - sizeof (EFI_IMAGE_DATA_DIRECTORY) * EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES;
      if (((UINT32)Hdr.Pe32->FileHeader.SizeOfOptionalHeader - HeaderWithoutDataDir) !=
          Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes * sizeof (EFI_IMAGE_DATA_DIRECTORY)) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      SectionHeaderOffset = ImageContext->PeCoffHeaderOffset + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER) + Hdr.Pe32->FileHeader.SizeOfOptionalHeader;
      //
      // 3. Check the FileHeader.NumberOfSections field.
      //
      if (Hdr.Pe32->OptionalHeader.SizeOfImage <= SectionHeaderOffset) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if ((Hdr.Pe32->OptionalHeader.SizeOfImage - SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <= Hdr.Pe32->FileHeader.NumberOfSections) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // 4. Check the OptionalHeader.SizeOfHeaders field.
      //
      if (Hdr.Pe32->OptionalHeader.SizeOfHeaders <= SectionHeaderOffset) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if (Hdr.Pe32->OptionalHeader.SizeOfHeaders >= Hdr.Pe32->OptionalHeader.SizeOfImage) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if ((Hdr.Pe32->OptionalHeader.SizeOfHeaders - SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER < (UINT32)Hdr.Pe32->FileHeader.NumberOfSections) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // 4.2 Read last byte of Hdr.Pe32.OptionalHeader.SizeOfHeaders from the file.
      //
      Size = 1;
      ReadSize = Size;
      Status = ImageContext->ImageRead (
                               ImageContext->Handle,
                               Hdr.Pe32->OptionalHeader.SizeOfHeaders - 1,
                               &Size,
                               &BufferData
                               );
      if (RETURN_ERROR (Status) || (Size != ReadSize)) {
        ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
          Status = RETURN_UNSUPPORTED;
        }
        return Status;
      }

      //
      // Check the EFI_IMAGE_DIRECTORY_ENTRY_SECURITY data.
      // Read the last byte to make sure the data is in the image region.
      // The DataDirectory array begin with 1, not 0, so here use < to compare not <=.
      //
      if (EFI_IMAGE_DIRECTORY_ENTRY_SECURITY < Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes) {
        if (Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size != 0) {
          //
          // Check the member data to avoid overflow.
          //
          if ((UINT32) (~0) - Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress <
              Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size) {
            ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
            return RETURN_UNSUPPORTED;
          }

          //
          // Read last byte of section header from file
          //
          Size = 1;
          ReadSize = Size;
          Status = ImageContext->ImageRead (
                                   ImageContext->Handle,
                                   Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress +
                                    Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size - 1,
                                   &Size,
                                   &BufferData
                                   );
          if (RETURN_ERROR (Status) || (Size != ReadSize)) {
            ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
            if (Size != ReadSize) {
              Status = RETURN_UNSUPPORTED;
            }
            return Status;
          }
        }
      }

      //
      // Use PE32 offset
      //
      ImageContext->ImageType         = Hdr.Pe32->OptionalHeader.Subsystem;
      ImageContext->ImageSize         = (UINT64)Hdr.Pe32->OptionalHeader.SizeOfImage;
      ImageContext->SectionAlignment  = Hdr.Pe32->OptionalHeader.SectionAlignment;
      ImageContext->SizeOfHeaders     = Hdr.Pe32->OptionalHeader.SizeOfHeaders;

    } else if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
      //
      // 1. Check FileHeader.NumberOfRvaAndSizes filed.
      //
      if (EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES < Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      //
      // 2. Check the FileHeader.SizeOfOptionalHeader field.
      // OptionalHeader.NumberOfRvaAndSizes is not bigger than 16, so
      // OptionalHeader.NumberOfRvaAndSizes * sizeof (EFI_IMAGE_DATA_DIRECTORY) will not overflow.
      //
      HeaderWithoutDataDir = sizeof (EFI_IMAGE_OPTIONAL_HEADER64) - sizeof (EFI_IMAGE_DATA_DIRECTORY) * EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES;
      if (((UINT32)Hdr.Pe32Plus->FileHeader.SizeOfOptionalHeader - HeaderWithoutDataDir) !=
          Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes * sizeof (EFI_IMAGE_DATA_DIRECTORY)) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      SectionHeaderOffset = ImageContext->PeCoffHeaderOffset + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER) + Hdr.Pe32Plus->FileHeader.SizeOfOptionalHeader;
      //
      // 3. Check the FileHeader.NumberOfSections field.
      //
      if (Hdr.Pe32Plus->OptionalHeader.SizeOfImage <= SectionHeaderOffset) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if ((Hdr.Pe32Plus->OptionalHeader.SizeOfImage - SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <= Hdr.Pe32Plus->FileHeader.NumberOfSections) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // 4. Check the OptionalHeader.SizeOfHeaders field.
      //
      if (Hdr.Pe32Plus->OptionalHeader.SizeOfHeaders <= SectionHeaderOffset) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if (Hdr.Pe32Plus->OptionalHeader.SizeOfHeaders >= Hdr.Pe32Plus->OptionalHeader.SizeOfImage) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }
      if ((Hdr.Pe32Plus->OptionalHeader.SizeOfHeaders - SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER < (UINT32)Hdr.Pe32Plus->FileHeader.NumberOfSections) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // 4.2 Read last byte of Hdr.Pe32Plus.OptionalHeader.SizeOfHeaders from the file.
      //
      Size = 1;
      ReadSize = Size;
      Status = ImageContext->ImageRead (
                               ImageContext->Handle,
                               Hdr.Pe32Plus->OptionalHeader.SizeOfHeaders - 1,
                               &Size,
                               &BufferData
                               );
      if (RETURN_ERROR (Status) || (Size != ReadSize)) {
        ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
          Status = RETURN_UNSUPPORTED;
        }
        return Status;
      }

      //
      // Check the EFI_IMAGE_DIRECTORY_ENTRY_SECURITY data.
      // Read the last byte to make sure the data is in the image region.
      // The DataDirectory array begin with 1, not 0, so here use < to compare not <=.
      //
      if (EFI_IMAGE_DIRECTORY_ENTRY_SECURITY < Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes) {
        if (Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size != 0) {
          //
          // Check the member data to avoid overflow.
          //
          if ((UINT32) (~0) - Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress <
              Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size) {
            ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
            return RETURN_UNSUPPORTED;
          }

          //
          // Read last byte of section header from file
          //
          Size = 1;
          ReadSize = Size;
          Status = ImageContext->ImageRead (
                                   ImageContext->Handle,
                                   Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress +
                                    Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_SECURITY].Size - 1,
                                   &Size,
                                   &BufferData
                                   );
          if (RETURN_ERROR (Status) || (Size != ReadSize)) {
            ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
            if (Size != ReadSize) {
              Status = RETURN_UNSUPPORTED;
            }
            return Status;
          }
        }
      }

      //
      // Use PE32+ offset
      //
      ImageContext->ImageType         = Hdr.Pe32Plus->OptionalHeader.Subsystem;
      ImageContext->ImageSize         = (UINT64) Hdr.Pe32Plus->OptionalHeader.SizeOfImage;
      ImageContext->SectionAlignment  = Hdr.Pe32Plus->OptionalHeader.SectionAlignment;
      ImageContext->SizeOfHeaders     = Hdr.Pe32Plus->OptionalHeader.SizeOfHeaders;
    } else {
      ImageContext->ImageError = IMAGE_ERROR_INVALID_MACHINE_TYPE;
      return RETURN_UNSUPPORTED;
    }
  } else {
    ImageContext->ImageError = IMAGE_ERROR_INVALID_MACHINE_TYPE;
    return RETURN_UNSUPPORTED;
  }

  if (!PeCoffLoaderImageFormatSupported (ImageContext->Machine)) {
    //
    // If the PE/COFF loader does not support the image type return
    // unsupported. This library can support lots of types of images
    // this does not mean the user of this library can call the entry
    // point of the image.
    //
    return RETURN_UNSUPPORTED;
  }

  //
  // Check each section field.
  //
  if (ImageContext->IsTeImage) {
    SectionHeaderOffset = sizeof(EFI_TE_IMAGE_HEADER);
    NumberOfSections    = (UINTN) (Hdr.Te->NumberOfSections);
  } else {
    SectionHeaderOffset = ImageContext->PeCoffHeaderOffset + sizeof (UINT32) + sizeof (EFI_IMAGE_FILE_HEADER) + Hdr.Pe32->FileHeader.SizeOfOptionalHeader;
    NumberOfSections    = (UINTN) (Hdr.Pe32->FileHeader.NumberOfSections);
  }

  for (Index = 0; Index < NumberOfSections; Index++) {
    //
    // Read section header from file
    //
    Size = sizeof (EFI_IMAGE_SECTION_HEADER);
    ReadSize = Size;
    Status = ImageContext->ImageRead (
                             ImageContext->Handle,
                             SectionHeaderOffset,
                             &Size,
                             &SectionHeader
                             );
    if (RETURN_ERROR (Status) || (Size != ReadSize)) {
      ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
      if (Size != ReadSize) {
        Status = RETURN_UNSUPPORTED;
      }
      return Status;
    }

    //
    // Adjust some field in Section Header for TE image.
    //
    if (ImageContext->IsTeImage) {
      PeCoffLoaderAdjustOffsetForTeImage (&SectionHeader, (UINT32)Hdr.Te->StrippedSize - sizeof (EFI_TE_IMAGE_HEADER));
    }

    if (SectionHeader.SizeOfRawData > 0) {
      //
      // Section data should bigger than the Pe header.
      //
      if (SectionHeader.VirtualAddress < ImageContext->SizeOfHeaders ||
          SectionHeader.PointerToRawData < ImageContext->SizeOfHeaders) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // Check the member data to avoid overflow.
      //
      if ((UINT32) (~0) - SectionHeader.PointerToRawData < SectionHeader.SizeOfRawData) {
        ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
        return RETURN_UNSUPPORTED;
      }

      //
      // Base on the ImageRead function to check the section data field.
      // Read the last byte to make sure the data is in the image region.
      //
      Size = 1;
      ReadSize = Size;
      Status = ImageContext->ImageRead (
                               ImageContext->Handle,
                               SectionHeader.PointerToRawData + SectionHeader.SizeOfRawData - 1,
                               &Size,
                               &BufferData
                               );
      if (RETURN_ERROR (Status) || (Size != ReadSize)) {
        ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
          Status = RETURN_UNSUPPORTED;
        }
        return Status;
      }
    }

    //
    // Check next section.
    //
    SectionHeaderOffset += sizeof (EFI_IMAGE_SECTION_HEADER);
  }

  return RETURN_SUCCESS;
}


/**
  Retrieves information about a PE/COFF image.

  Computes the PeCoffHeaderOffset, IsTeImage, ImageType, ImageAddress, ImageSize,
  DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders, and
  DebugDirectoryEntryRva fields of the ImageContext structure.
  If ImageContext is NULL, then return RETURN_INVALID_PARAMETER.
  If the PE/COFF image accessed through the ImageRead service in the ImageContext
  structure is not a supported PE/COFF image type, then return RETURN_UNSUPPORTED.
  If any errors occur while computing the fields of ImageContext,
  then the error status is returned in the ImageError field of ImageContext.
  If the image is a TE image, then SectionAlignment is set to 0.
  The ImageRead and Handle fields of ImageContext structure must be valid prior
  to invoking this service.

  Caution: This function may receive untrusted input.
  PE/COFF image is external input, so this routine will
  also done many checks in PE image to make sure PE image DosHeader, PeOptionHeader,
  SizeOfHeader, Section Data Region and Security Data Region be in PE image range.

  @param  ImageContext              The pointer to the image context structure that describes the PE/COFF
                                    image that needs to be examined by this function.

  @retval RETURN_SUCCESS            The information on the PE/COFF image was collected.
  @retval RETURN_INVALID_PARAMETER  ImageContext is NULL.
  @retval RETURN_UNSUPPORTED        The PE/COFF image is not supported.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderGetImageInfo (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  RETURN_STATUS                         Status;
  EFI_IMAGE_OPTIONAL_HEADER_UNION       HdrData;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION   Hdr;
  EFI_IMAGE_DATA_DIRECTORY              *DebugDirectoryEntry;
  UINTN                                 Size;
  UINTN                                 ReadSize;
  UINTN                                 Index;
  UINTN                                 DebugDirectoryEntryRva;
  UINTN                                 DebugDirectoryEntryFileOffset;
  UINTN                                 SectionHeaderOffset;
  EFI_IMAGE_SECTION_HEADER              SectionHeader;
  EFI_IMAGE_DEBUG_DIRECTORY_ENTRY       DebugEntry;
  UINT32                                NumberOfRvaAndSizes;
  UINT32                                TeStrippedOffset;

  if (ImageContext == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  //
  // Assume success
  //
  ImageContext->ImageError  = IMAGE_ERROR_SUCCESS;

  Hdr.Union = &HdrData;
  Status = PeCoffLoaderGetPeHeader (ImageContext, Hdr);
  if (RETURN_ERROR (Status)) {
    return Status;
  }

  //
  // Retrieve the base address of the image
  //
  if (!(ImageContext->IsTeImage)) {
    TeStrippedOffset = 0;
    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      ImageContext->ImageAddress = Hdr.Pe32->OptionalHeader.ImageBase;
    } else {
      //
      // Use PE32+ offset
      //
      ImageContext->ImageAddress = Hdr.Pe32Plus->OptionalHeader.ImageBase;
    }
  } else {
    TeStrippedOffset = (UINT32)Hdr.Te->StrippedSize - sizeof (EFI_TE_IMAGE_HEADER);
    ImageContext->ImageAddress = (PHYSICAL_ADDRESS)(Hdr.Te->ImageBase + TeStrippedOffset);
  }

  //
  // Initialize the alternate destination address to 0 indicating that it
  // should not be used.
  //
  ImageContext->DestinationAddress = 0;

  //
  // Initialize the debug codeview pointer.
  //
  ImageContext->DebugDirectoryEntryRva = 0;
  ImageContext->CodeView               = NULL;
  ImageContext->PdbPointer             = NULL;

  //
  // Three cases with regards to relocations:
  // - Image has base relocs, RELOCS_STRIPPED==0    => image is relocatable
  // - Image has no base relocs, RELOCS_STRIPPED==1 => Image is not relocatable
  // - Image has no base relocs, RELOCS_STRIPPED==0 => Image is relocatable but
  //   has no base relocs to apply
  // Obviously having base relocations with RELOCS_STRIPPED==1 is invalid.
  //
  // Look at the file header to determine if relocations have been stripped, and
  // save this information in the image context for later use.
  //
  if ((!(ImageContext->IsTeImage)) && ((Hdr.Pe32->FileHeader.Characteristics & EFI_IMAGE_FILE_RELOCS_STRIPPED) != 0)) {
    ImageContext->RelocationsStripped = TRUE;
  } else if ((ImageContext->IsTeImage) && (Hdr.Te->DataDirectory[0].Size == 0) && (Hdr.Te->DataDirectory[0].VirtualAddress == 0)) {
    ImageContext->RelocationsStripped = TRUE;
  } else {
    ImageContext->RelocationsStripped = FALSE;
  }

  if (!(ImageContext->IsTeImage)) {
    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes;
      DebugDirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&(Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG]);
    } else {
      //
      // Use PE32+ offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
      DebugDirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&(Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG]);
    }

    if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_DEBUG) {

      DebugDirectoryEntryRva = DebugDirectoryEntry->VirtualAddress;

      //
      // Determine the file offset of the debug directory...  This means we walk
      // the sections to find which section contains the RVA of the debug
      // directory
      //
      DebugDirectoryEntryFileOffset = 0;

      SectionHeaderOffset = ImageContext->PeCoffHeaderOffset +
                            sizeof (UINT32) +
                            sizeof (EFI_IMAGE_FILE_HEADER) +
                            Hdr.Pe32->FileHeader.SizeOfOptionalHeader;

      for (Index = 0; Index < Hdr.Pe32->FileHeader.NumberOfSections; Index++) {
        //
        // Read section header from file
        //
        Size = sizeof (EFI_IMAGE_SECTION_HEADER);
        ReadSize = Size;
        Status = ImageContext->ImageRead (
                                 ImageContext->Handle,
                                 SectionHeaderOffset,
                                 &Size,
                                 &SectionHeader
                                 );
        if (RETURN_ERROR (Status) || (Size != ReadSize)) {
          ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
          if (Size != ReadSize) {
            Status = RETURN_UNSUPPORTED;
          }
          return Status;
        }

        if (DebugDirectoryEntryRva >= SectionHeader.VirtualAddress &&
            DebugDirectoryEntryRva < SectionHeader.VirtualAddress + SectionHeader.Misc.VirtualSize) {

          DebugDirectoryEntryFileOffset = DebugDirectoryEntryRva - SectionHeader.VirtualAddress + SectionHeader.PointerToRawData;
          break;
        }

        SectionHeaderOffset += sizeof (EFI_IMAGE_SECTION_HEADER);
      }

      if (DebugDirectoryEntryFileOffset != 0) {
        for (Index = 0; Index < DebugDirectoryEntry->Size; Index += sizeof (EFI_IMAGE_DEBUG_DIRECTORY_ENTRY)) {
          //
          // Read next debug directory entry
          //
          Size = sizeof (EFI_IMAGE_DEBUG_DIRECTORY_ENTRY);
          ReadSize = Size;
          Status = ImageContext->ImageRead (
                                   ImageContext->Handle,
                                   DebugDirectoryEntryFileOffset + Index,
                                   &Size,
                                   &DebugEntry
                                   );
          if (RETURN_ERROR (Status) || (Size != ReadSize)) {
            ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
            if (Size != ReadSize) {
              Status = RETURN_UNSUPPORTED;
            }
            return Status;
          }

          //
          // From PeCoff spec, when DebugEntry.RVA == 0 means this debug info will not load into memory.
          // Here we will always load EFI_IMAGE_DEBUG_TYPE_CODEVIEW type debug info. so need adjust the
          // ImageContext->ImageSize when DebugEntry.RVA == 0.
          //
          if (DebugEntry.Type == EFI_IMAGE_DEBUG_TYPE_CODEVIEW) {
            ImageContext->DebugDirectoryEntryRva = (UINT32) (DebugDirectoryEntryRva + Index);
            if (DebugEntry.RVA == 0 && DebugEntry.FileOffset != 0) {
              ImageContext->ImageSize += DebugEntry.SizeOfData;
            }

            return RETURN_SUCCESS;
          }
        }
      }
    }
  } else {

    DebugDirectoryEntry             = &Hdr.Te->DataDirectory[1];
    DebugDirectoryEntryRva          = DebugDirectoryEntry->VirtualAddress;
    SectionHeaderOffset             = (UINTN)(sizeof (EFI_TE_IMAGE_HEADER));

    DebugDirectoryEntryFileOffset   = 0;

    for (Index = 0; Index < Hdr.Te->NumberOfSections;) {
      //
      // Read section header from file
      //
      Size   = sizeof (EFI_IMAGE_SECTION_HEADER);
      ReadSize = Size;
      Status = ImageContext->ImageRead (
                               ImageContext->Handle,
                               SectionHeaderOffset,
                               &Size,
                               &SectionHeader
                               );
      if (RETURN_ERROR (Status) || (Size != ReadSize)) {
        ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
          Status = RETURN_UNSUPPORTED;
        }
        return Status;
      }

      if (DebugDirectoryEntryRva >= SectionHeader.VirtualAddress &&
          DebugDirectoryEntryRva < SectionHeader.VirtualAddress + SectionHeader.Misc.VirtualSize) {
        DebugDirectoryEntryFileOffset = DebugDirectoryEntryRva -
                                        SectionHeader.VirtualAddress +
                                        SectionHeader.PointerToRawData -
                                        TeStrippedOffset;

        //
        // File offset of the debug directory was found, if this is not the last
        // section, then skip to the last section for calculating the image size.
        //
        if (Index < (UINTN) Hdr.Te->NumberOfSections - 1) {
          SectionHeaderOffset += (Hdr.Te->NumberOfSections - 1 - Index) * sizeof (EFI_IMAGE_SECTION_HEADER);
          Index = Hdr.Te->NumberOfSections - 1;
          continue;
        }
      }

      //
      // In Te image header there is not a field to describe the ImageSize.
      // Actually, the ImageSize equals the RVA plus the VirtualSize of
      // the last section mapped into memory (Must be rounded up to
      // a multiple of Section Alignment). Per the PE/COFF specification, the
      // section headers in the Section Table must appear in order of the RVA
      // values for the corresponding sections. So the ImageSize can be determined
      // by the RVA and the VirtualSize of the last section header in the
      // Section Table.
      //
      if ((++Index) == (UINTN)Hdr.Te->NumberOfSections) {
        ImageContext->ImageSize = (SectionHeader.VirtualAddress + SectionHeader.Misc.VirtualSize) - TeStrippedOffset;
      }

      SectionHeaderOffset += sizeof (EFI_IMAGE_SECTION_HEADER);
    }

    if (DebugDirectoryEntryFileOffset != 0) {
      for (Index = 0; Index < DebugDirectoryEntry->Size; Index += sizeof (EFI_IMAGE_DEBUG_DIRECTORY_ENTRY)) {
        //
        // Read next debug directory entry
        //
        Size = sizeof (EFI_IMAGE_DEBUG_DIRECTORY_ENTRY);
        ReadSize = Size;
        Status = ImageContext->ImageRead (
                                 ImageContext->Handle,
                                 DebugDirectoryEntryFileOffset + Index,
                                 &Size,
                                 &DebugEntry
                                 );
        if (RETURN_ERROR (Status) || (Size != ReadSize)) {
          ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
          if (Size != ReadSize) {
            Status = RETURN_UNSUPPORTED;
          }
          return Status;
        }

        if (DebugEntry.Type == EFI_IMAGE_DEBUG_TYPE_CODEVIEW) {
          ImageContext->DebugDirectoryEntryRva = (UINT32) (DebugDirectoryEntryRva + Index);
          return RETURN_SUCCESS;
        }
      }
    }
  }

  return RETURN_SUCCESS;
}


/**
  Converts an image address to the loaded address.

  @param  ImageContext      The context of the image being loaded.
  @param  Address           The address to be converted to the loaded address.
  @param  TeStrippedOffset  Stripped offset for TE image.

  @return The converted address or NULL if the address can not be converted.

**/
VOID *
PeCoffLoaderImageAddress (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT          *ImageContext,
  IN     UINTN                                 Address,
  IN     UINTN                                 TeStrippedOffset
  )
{
  //
  // Make sure that Address and ImageSize is correct for the loaded image.
  //
  if (Address >= ImageContext->ImageSize + TeStrippedOffset) {
    ImageContext->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
    return NULL;
  }

  return (CHAR8 *)((UINTN) ImageContext->ImageAddress + Address - TeStrippedOffset);
}

/**
  Applies relocation fixups to a PE/COFF image that was loaded with PeCoffLoaderLoadImage().

  If the DestinationAddress field of ImageContext is 0, then use the ImageAddress field of
  ImageContext as the relocation base address.  Otherwise, use the DestinationAddress field
  of ImageContext as the relocation base address.  The caller must allocate the relocation
  fixup log buffer and fill in the FixupData field of ImageContext prior to calling this function.

  The ImageRead, Handle, PeCoffHeaderOffset,  IsTeImage, Machine, ImageType, ImageAddress,
  ImageSize, DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders,
  DebugDirectoryEntryRva, EntryPoint, FixupDataSize, CodeView, PdbPointer, and FixupData of
  the ImageContext structure must be valid prior to invoking this service.

  If ImageContext is NULL, then ASSERT().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageContext        The pointer to the image context structure that describes the PE/COFF
                              image that is being relocated.

  @retval RETURN_SUCCESS      The PE/COFF image was relocated.
                              Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_LOAD_ERROR   The image in not a valid PE/COFF image.
                              Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_UNSUPPORTED  A relocation record type is not supported.
                              Extended status information is in the ImageError field of ImageContext.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderRelocateImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  RETURN_STATUS                         Status;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION   Hdr;
  EFI_IMAGE_DATA_DIRECTORY              *RelocDir;
  UINT64                                Adjust;
  EFI_IMAGE_BASE_RELOCATION             *RelocBaseOrg;
  EFI_IMAGE_BASE_RELOCATION             *RelocBase;
  EFI_IMAGE_BASE_RELOCATION             *RelocBaseEnd;
  UINT16                                *Reloc;
  UINT16                                *RelocEnd;
  CHAR8                                 *Fixup;
  CHAR8                                 *FixupBase;
  UINT16                                *Fixup16;
  UINT32                                *Fixup32;
  UINT64                                *Fixup64;
  CHAR8                                 *FixupData;
  PHYSICAL_ADDRESS                      BaseAddress;
  UINT32                                NumberOfRvaAndSizes;
  UINT32                                TeStrippedOffset;

  ASSERT (ImageContext != NULL);

  //
  // Assume success
  //
  ImageContext->ImageError = IMAGE_ERROR_SUCCESS;

  //
  // If there are no relocation entries, then we are done
  //
  if (ImageContext->RelocationsStripped) {
    // Applies additional environment specific actions to relocate fixups
    // to a PE/COFF image if needed
    PeCoffLoaderRelocateImageExtraAction (ImageContext);
    return RETURN_SUCCESS;
  }

  //
  // If the destination address is not 0, use that rather than the
  // image address as the relocation target.
  //
  if (ImageContext->DestinationAddress != 0) {
    BaseAddress = ImageContext->DestinationAddress;
  } else {
    BaseAddress = ImageContext->ImageAddress;
  }

  if (!(ImageContext->IsTeImage)) {
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)((UINTN)ImageContext->ImageAddress + ImageContext->PeCoffHeaderOffset);
    TeStrippedOffset = 0;

    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      Adjust = (UINT64)BaseAddress - Hdr.Pe32->OptionalHeader.ImageBase;
      if (Adjust != 0) {
        Hdr.Pe32->OptionalHeader.ImageBase = (UINT32)BaseAddress;
      }

      NumberOfRvaAndSizes = Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes;
      RelocDir  = &Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];
    } else {
      //
      // Use PE32+ offset
      //
      Adjust = (UINT64) BaseAddress - Hdr.Pe32Plus->OptionalHeader.ImageBase;
      if (Adjust != 0) {
        Hdr.Pe32Plus->OptionalHeader.ImageBase = (UINT64)BaseAddress;
      }

      NumberOfRvaAndSizes = Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
      RelocDir  = &Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];
    }

    //
    // Find the relocation block
    // Per the PE/COFF spec, you can't assume that a given data directory
    // is present in the image. You have to check the NumberOfRvaAndSizes in
    // the optional header to verify a desired directory entry is there.
    //
    if ((NumberOfRvaAndSizes < EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC)) {
      RelocDir = NULL;
    }
  } else {
    Hdr.Te             = (EFI_TE_IMAGE_HEADER *)(UINTN)(ImageContext->ImageAddress);
    TeStrippedOffset   = (UINT32)Hdr.Te->StrippedSize - sizeof (EFI_TE_IMAGE_HEADER);
    Adjust             = (UINT64) (BaseAddress - (Hdr.Te->ImageBase + TeStrippedOffset));
    if (Adjust != 0) {
      Hdr.Te->ImageBase  = (UINT64) (BaseAddress - TeStrippedOffset);
    }

    //
    // Find the relocation block
    //
    RelocDir = &Hdr.Te->DataDirectory[0];
  }

  if ((RelocDir != NULL) && (RelocDir->Size > 0)) {
    RelocBase = (EFI_IMAGE_BASE_RELOCATION *) PeCoffLoaderImageAddress (ImageContext, RelocDir->VirtualAddress, TeStrippedOffset);
    RelocBaseEnd = (EFI_IMAGE_BASE_RELOCATION *) PeCoffLoaderImageAddress (ImageContext,
                                                                            RelocDir->VirtualAddress + RelocDir->Size - 1,
                                                                            TeStrippedOffset
                                                                            );
    if (RelocBase == NULL || RelocBaseEnd == NULL || (UINTN) RelocBaseEnd < (UINTN) RelocBase) {
      ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
      return RETURN_LOAD_ERROR;
    }
  } else {
    //
    // Set base and end to bypass processing below.
    //
    RelocBase = RelocBaseEnd = NULL;
  }
  RelocBaseOrg = RelocBase;

  //
  // If Adjust is not zero, then apply fix ups to the image
  //
  if (Adjust != 0) {
    //
    // Run the relocation information and apply the fixups
    //
    FixupData = ImageContext->FixupData;
    while ((UINTN) RelocBase < (UINTN) RelocBaseEnd) {

      Reloc     = (UINT16 *) ((CHAR8 *) RelocBase + sizeof (EFI_IMAGE_BASE_RELOCATION));
      //
      // Add check for RelocBase->SizeOfBlock field.
      //
      if (RelocBase->SizeOfBlock == 0) {
        ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
        return RETURN_LOAD_ERROR;
      }
      if ((UINTN)RelocBase > MAX_ADDRESS - RelocBase->SizeOfBlock) {
        ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
        return RETURN_LOAD_ERROR;
      }

      RelocEnd  = (UINT16 *) ((CHAR8 *) RelocBase + RelocBase->SizeOfBlock);
      if ((UINTN)RelocEnd > (UINTN)RelocBaseOrg + RelocDir->Size) {
        ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
        return RETURN_LOAD_ERROR;
      }
      FixupBase = PeCoffLoaderImageAddress (ImageContext, RelocBase->VirtualAddress, TeStrippedOffset);
      if (FixupBase == NULL) {
        ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
        return RETURN_LOAD_ERROR;
      }

      //
      // Run this relocation record
      //
      while ((UINTN) Reloc < (UINTN) RelocEnd) {
        Fixup = PeCoffLoaderImageAddress (ImageContext, RelocBase->VirtualAddress + (*Reloc & 0xFFF), TeStrippedOffset);
        if (Fixup == NULL) {
          ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
          return RETURN_LOAD_ERROR;
        }
        switch ((*Reloc) >> 12) {
        case EFI_IMAGE_REL_BASED_ABSOLUTE:
          break;

        case EFI_IMAGE_REL_BASED_HIGH:
          Fixup16   = (UINT16 *) Fixup;
          *Fixup16 = (UINT16) (*Fixup16 + ((UINT16) ((UINT32) Adjust >> 16)));
          if (FixupData != NULL) {
            *(UINT16 *) FixupData = *Fixup16;
            FixupData             = FixupData + sizeof (UINT16);
          }
          break;

        case EFI_IMAGE_REL_BASED_LOW:
          Fixup16   = (UINT16 *) Fixup;
          *Fixup16  = (UINT16) (*Fixup16 + (UINT16) Adjust);
          if (FixupData != NULL) {
            *(UINT16 *) FixupData = *Fixup16;
            FixupData             = FixupData + sizeof (UINT16);
          }
          break;

        case EFI_IMAGE_REL_BASED_HIGHLOW:
          Fixup32   = (UINT32 *) Fixup;
          *Fixup32  = *Fixup32 + (UINT32) Adjust;
          if (FixupData != NULL) {
            FixupData             = ALIGN_POINTER (FixupData, sizeof (UINT32));
            *(UINT32 *)FixupData  = *Fixup32;
            FixupData             = FixupData + sizeof (UINT32);
          }
          break;

        case EFI_IMAGE_REL_BASED_DIR64:
          Fixup64 = (UINT64 *) Fixup;
          *Fixup64 = *Fixup64 + (UINT64) Adjust;
          if (FixupData != NULL) {
            FixupData = ALIGN_POINTER (FixupData, sizeof(UINT64));
            *(UINT64 *)(FixupData) = *Fixup64;
            FixupData = FixupData + sizeof(UINT64);
          }
          break;

        default:
          //
          // The common code does not handle some of the stranger IPF relocations
          // PeCoffLoaderRelocateImageEx () adds support for these complex fixups
          // on IPF and is a No-Op on other architectures.
          //
          Status = PeCoffLoaderRelocateImageEx (Reloc, Fixup, &FixupData, Adjust);
          if (RETURN_ERROR (Status)) {
            ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
            return Status;
          }
        }

        //
        // Next relocation record
        //
        Reloc += 1;
      }

      //
      // Next reloc block
      //
      RelocBase = (EFI_IMAGE_BASE_RELOCATION *) RelocEnd;
    }
    ASSERT ((UINTN)FixupData <= (UINTN)ImageContext->FixupData + ImageContext->FixupDataSize);

    //
    // Adjust the EntryPoint to match the linked-to address
    //
    if (ImageContext->DestinationAddress != 0) {
       ImageContext->EntryPoint -= (UINT64) ImageContext->ImageAddress;
       ImageContext->EntryPoint += (UINT64) ImageContext->DestinationAddress;
    }
  }

  // Applies additional environment specific actions to relocate fixups
  // to a PE/COFF image if needed
  PeCoffLoaderRelocateImageExtraAction (ImageContext);

  return RETURN_SUCCESS;
}

/**
  Loads a PE/COFF image into memory.

  Loads the PE/COFF image accessed through the ImageRead service of ImageContext into the buffer
  specified by the ImageAddress and ImageSize fields of ImageContext.  The caller must allocate
  the load buffer and fill in the ImageAddress and ImageSize fields prior to calling this function.
  The EntryPoint, FixupDataSize, CodeView, PdbPointer and HiiResourceData fields of ImageContext are computed.
  The ImageRead, Handle, PeCoffHeaderOffset,  IsTeImage,  Machine, ImageType, ImageAddress, ImageSize,
  DestinationAddress, RelocationsStripped, SectionAlignment, SizeOfHeaders, and DebugDirectoryEntryRva
  fields of the ImageContext structure must be valid prior to invoking this service.

  If ImageContext is NULL, then ASSERT().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageContext              The pointer to the image context structure that describes the PE/COFF
                                    image that is being loaded.

  @retval RETURN_SUCCESS            The PE/COFF image was loaded into the buffer specified by
                                    the ImageAddress and ImageSize fields of ImageContext.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_BUFFER_TOO_SMALL   The caller did not provide a large enough buffer.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_LOAD_ERROR         The PE/COFF image is an EFI Runtime image with no relocations.
                                    Extended status information is in the ImageError field of ImageContext.
  @retval RETURN_INVALID_PARAMETER  The image address is invalid.
                                    Extended status information is in the ImageError field of ImageContext.

**/
RETURN_STATUS
EFIAPI
PeCoffLoaderLoadImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  RETURN_STATUS                         Status;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION   Hdr;
  PE_COFF_LOADER_IMAGE_CONTEXT          CheckContext;
  EFI_IMAGE_SECTION_HEADER              *FirstSection;
  EFI_IMAGE_SECTION_HEADER              *Section;
  UINTN                                 NumberOfSections;
  UINTN                                 Index;
  CHAR8                                 *Base;
  CHAR8                                 *End;
  EFI_IMAGE_DATA_DIRECTORY              *DirectoryEntry;
  EFI_IMAGE_DEBUG_DIRECTORY_ENTRY       *DebugEntry;
  UINTN                                 Size;
  UINT32                                TempDebugEntryRva;
  UINT32                                NumberOfRvaAndSizes;
  EFI_IMAGE_RESOURCE_DIRECTORY          *ResourceDirectory;
  EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY    *ResourceDirectoryEntry;
  EFI_IMAGE_RESOURCE_DIRECTORY_STRING   *ResourceDirectoryString;
  EFI_IMAGE_RESOURCE_DATA_ENTRY         *ResourceDataEntry;
  CHAR16                                *String;
  UINT32                                Offset;
  UINT32                                TeStrippedOffset;

  ASSERT (ImageContext != NULL);

  //
  // Assume success
  //
  ImageContext->ImageError = IMAGE_ERROR_SUCCESS;

  //
  // Copy the provided context information into our local version, get what we
  // can from the original image, and then use that to make sure everything
  // is legit.
  //
  CopyMem (&CheckContext, ImageContext, sizeof (PE_COFF_LOADER_IMAGE_CONTEXT));

  Status = PeCoffLoaderGetImageInfo (&CheckContext);
  if (RETURN_ERROR (Status)) {
    return Status;
  }

  //
  // Make sure there is enough allocated space for the image being loaded
  //
  if (ImageContext->ImageSize < CheckContext.ImageSize) {
    ImageContext->ImageError = IMAGE_ERROR_INVALID_IMAGE_SIZE;
    return RETURN_BUFFER_TOO_SMALL;
  }
  if (ImageContext->ImageAddress == 0) {
    //
    // Image cannot be loaded into 0 address.
    //
    ImageContext->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
    return RETURN_INVALID_PARAMETER;
  }
  //
  // If there's no relocations, then make sure it's not a runtime driver,
  // and that it's being loaded at the linked address.
  //
  if (CheckContext.RelocationsStripped) {
    //
    // If the image does not contain relocations and it is a runtime driver
    // then return an error.
    //
    if (CheckContext.ImageType == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER) {
      ImageContext->ImageError = IMAGE_ERROR_INVALID_SUBSYSTEM;
      return RETURN_LOAD_ERROR;
    }
    //
    // If the image does not contain relocations, and the requested load address
    // is not the linked address, then return an error.
    //
    if (CheckContext.ImageAddress != ImageContext->ImageAddress) {
      ImageContext->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
      return RETURN_INVALID_PARAMETER;
    }
  }
  //
  // Make sure the allocated space has the proper section alignment
  //
  if (!(ImageContext->IsTeImage)) {
    if ((ImageContext->ImageAddress & (CheckContext.SectionAlignment - 1)) != 0) {
      ImageContext->ImageError = IMAGE_ERROR_INVALID_SECTION_ALIGNMENT;
      return RETURN_INVALID_PARAMETER;
    }
  }
  //
  // Read the entire PE/COFF or TE header into memory
  //
  if (!(ImageContext->IsTeImage)) {
    Status = ImageContext->ImageRead (
                            ImageContext->Handle,
                            0,
                            &ImageContext->SizeOfHeaders,
                            (VOID *) (UINTN) ImageContext->ImageAddress
                            );

    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)((UINTN)ImageContext->ImageAddress + ImageContext->PeCoffHeaderOffset);

    FirstSection = (EFI_IMAGE_SECTION_HEADER *) (
                      (UINTN)ImageContext->ImageAddress +
                      ImageContext->PeCoffHeaderOffset +
                      sizeof(UINT32) +
                      sizeof(EFI_IMAGE_FILE_HEADER) +
                      Hdr.Pe32->FileHeader.SizeOfOptionalHeader
      );
    NumberOfSections = (UINTN) (Hdr.Pe32->FileHeader.NumberOfSections);
    TeStrippedOffset = 0;
  } else {
    Status = ImageContext->ImageRead (
                            ImageContext->Handle,
                            0,
                            &ImageContext->SizeOfHeaders,
                            (void *)(UINTN)ImageContext->ImageAddress
                            );

    Hdr.Te = (EFI_TE_IMAGE_HEADER *)(UINTN)(ImageContext->ImageAddress);
    FirstSection = (EFI_IMAGE_SECTION_HEADER *) (
                      (UINTN)ImageContext->ImageAddress +
                      sizeof(EFI_TE_IMAGE_HEADER)
                      );
    NumberOfSections  = (UINTN) (Hdr.Te->NumberOfSections);
    TeStrippedOffset  = (UINT32) Hdr.Te->StrippedSize - sizeof (EFI_TE_IMAGE_HEADER);
  }

  if (RETURN_ERROR (Status)) {
    ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
    return RETURN_LOAD_ERROR;
  }

  //
  // Load each section of the image
  //
  Section = FirstSection;
  for (Index = 0; Index < NumberOfSections; Index++) {
    //
    // Read the section
    //
    Size = (UINTN) Section->Misc.VirtualSize;
    if ((Size == 0) || (Size > Section->SizeOfRawData)) {
      Size = (UINTN) Section->SizeOfRawData;
    }

    //
    // Compute sections address
    //
    Base = PeCoffLoaderImageAddress (ImageContext, Section->VirtualAddress, TeStrippedOffset);
    End  = PeCoffLoaderImageAddress (ImageContext, Section->VirtualAddress + Section->Misc.VirtualSize - 1, TeStrippedOffset);

    //
    // If the size of the section is non-zero and the base address or end address resolved to 0, then fail.
    //
    if ((Size > 0) && ((Base == NULL) || (End == NULL))) {
      ImageContext->ImageError = IMAGE_ERROR_SECTION_NOT_LOADED;
      return RETURN_LOAD_ERROR;
    }

    if (Section->SizeOfRawData > 0) {
      Status = ImageContext->ImageRead (
                              ImageContext->Handle,
                              Section->PointerToRawData - TeStrippedOffset,
                              &Size,
                              Base
                              );
      if (RETURN_ERROR (Status)) {
        ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
        return Status;
      }
    }

    //
    // If raw size is less then virtual size, zero fill the remaining
    //

    if (Size < Section->Misc.VirtualSize) {
      ZeroMem (Base + Size, Section->Misc.VirtualSize - Size);
    }

    //
    // Next Section
    //
    Section += 1;
  }

  //
  // Get image's entry point
  //
  if (!(ImageContext->IsTeImage)) {
    //
    // Sizes of AddressOfEntryPoint are different so we need to do this safely
    //
    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      ImageContext->EntryPoint = (PHYSICAL_ADDRESS)(UINTN)PeCoffLoaderImageAddress (
                                                            ImageContext,
                                                            (UINTN)Hdr.Pe32->OptionalHeader.AddressOfEntryPoint,
                                                            0
                                                            );
    } else {
      //
      // Use PE32+ offset
      //
      ImageContext->EntryPoint = (PHYSICAL_ADDRESS)(UINTN)PeCoffLoaderImageAddress (
                                                            ImageContext,
                                                            (UINTN)Hdr.Pe32Plus->OptionalHeader.AddressOfEntryPoint,
                                                            0
                                                            );
    }
  } else {
    ImageContext->EntryPoint = (PHYSICAL_ADDRESS)(UINTN)PeCoffLoaderImageAddress (
                                                          ImageContext,
                                                          (UINTN)Hdr.Te->AddressOfEntryPoint,
                                                          TeStrippedOffset
                                                          );
  }

  //
  // Determine the size of the fixup data
  //
  // Per the PE/COFF spec, you can't assume that a given data directory
  // is present in the image. You have to check the NumberOfRvaAndSizes in
  // the optional header to verify a desired directory entry is there.
  //
  if (!(ImageContext->IsTeImage)) {
    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes;
      DirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];
    } else {
      //
      // Use PE32+ offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
      DirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];
    }

    //
    // Must use UINT64 here, because there might a case that 32bit loader to load 64bit image.
    //
    if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC) {
      ImageContext->FixupDataSize = DirectoryEntry->Size / sizeof (UINT16) * sizeof (UINT64);
    } else {
      ImageContext->FixupDataSize = 0;
    }
  } else {
    DirectoryEntry              = &Hdr.Te->DataDirectory[0];
    ImageContext->FixupDataSize = DirectoryEntry->Size / sizeof (UINT16) * sizeof (UINT64);
  }
  //
  // Consumer must allocate a buffer for the relocation fixup log.
  // Only used for runtime drivers.
  //
  ImageContext->FixupData = NULL;

  //
  // Load the Codeview information if present
  //
  if (ImageContext->DebugDirectoryEntryRva != 0) {
    DebugEntry = PeCoffLoaderImageAddress (
                ImageContext,
                ImageContext->DebugDirectoryEntryRva,
                TeStrippedOffset
                );
    if (DebugEntry == NULL) {
      ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
      return RETURN_LOAD_ERROR;
    }

    TempDebugEntryRva = DebugEntry->RVA;
    if (DebugEntry->RVA == 0 && DebugEntry->FileOffset != 0) {
      Section--;
      if ((UINTN)Section->SizeOfRawData < Section->Misc.VirtualSize) {
        TempDebugEntryRva = Section->VirtualAddress + Section->Misc.VirtualSize;
      } else {
        TempDebugEntryRva = Section->VirtualAddress + Section->SizeOfRawData;
      }
    }

    if (TempDebugEntryRva != 0) {
      ImageContext->CodeView = PeCoffLoaderImageAddress (ImageContext, TempDebugEntryRva, TeStrippedOffset);
      if (ImageContext->CodeView == NULL) {
        ImageContext->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
        return RETURN_LOAD_ERROR;
      }

      if (DebugEntry->RVA == 0) {
        Size = DebugEntry->SizeOfData;
        Status = ImageContext->ImageRead (
                                ImageContext->Handle,
                                DebugEntry->FileOffset - TeStrippedOffset,
                                &Size,
                                ImageContext->CodeView
                                );
        //
        // Should we apply fix up to this field according to the size difference between PE and TE?
        // Because now we maintain TE header fields unfixed, this field will also remain as they are
        // in original PE image.
        //

        if (RETURN_ERROR (Status)) {
          ImageContext->ImageError = IMAGE_ERROR_IMAGE_READ;
          return RETURN_LOAD_ERROR;
        }

        DebugEntry->RVA = TempDebugEntryRva;
      }

      switch (*(UINT32 *) ImageContext->CodeView) {
      case CODEVIEW_SIGNATURE_NB10:
        if (DebugEntry->SizeOfData < sizeof (EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY)) {
          ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
          return RETURN_UNSUPPORTED;
        }
        ImageContext->PdbPointer = (CHAR8 *)ImageContext->CodeView + sizeof (EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY);
        break;

      case CODEVIEW_SIGNATURE_RSDS:
        if (DebugEntry->SizeOfData < sizeof (EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY)) {
          ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
          return RETURN_UNSUPPORTED;
        }
        ImageContext->PdbPointer = (CHAR8 *)ImageContext->CodeView + sizeof (EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY);
        break;

      case CODEVIEW_SIGNATURE_MTOC:
        if (DebugEntry->SizeOfData < sizeof (EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY)) {
          ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
          return RETURN_UNSUPPORTED;
        }
        ImageContext->PdbPointer = (CHAR8 *)ImageContext->CodeView + sizeof (EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY);
        break;

      default:
        break;
      }
    }
  }

  //
  // Get Image's HII resource section
  //
  ImageContext->HiiResourceData = 0;
  if (!(ImageContext->IsTeImage)) {
    if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
      //
      // Use PE32 offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes;
      DirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&Hdr.Pe32->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE];
    } else {
      //
      // Use PE32+ offset
      //
      NumberOfRvaAndSizes = Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
      DirectoryEntry = (EFI_IMAGE_DATA_DIRECTORY *)&Hdr.Pe32Plus->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE];
    }

    if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE && DirectoryEntry->Size != 0) {
      Base = PeCoffLoaderImageAddress (ImageContext, DirectoryEntry->VirtualAddress, 0);
      if (Base != NULL) {
        ResourceDirectory = (EFI_IMAGE_RESOURCE_DIRECTORY *) Base;
        Offset = sizeof (EFI_IMAGE_RESOURCE_DIRECTORY) + sizeof (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) *
               (ResourceDirectory->NumberOfNamedEntries + ResourceDirectory->NumberOfIdEntries);
        if (Offset > DirectoryEntry->Size) {
          ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
          return RETURN_UNSUPPORTED;
        }
        ResourceDirectoryEntry = (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *) (ResourceDirectory + 1);

        for (Index = 0; Index < ResourceDirectory->NumberOfNamedEntries; Index++) {
          if (ResourceDirectoryEntry->u1.s.NameIsString) {
            //
            // Check the ResourceDirectoryEntry->u1.s.NameOffset before use it.
            //
            if (ResourceDirectoryEntry->u1.s.NameOffset >= DirectoryEntry->Size) {
              ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
              return RETURN_UNSUPPORTED;
            }
            ResourceDirectoryString = (EFI_IMAGE_RESOURCE_DIRECTORY_STRING *) (Base + ResourceDirectoryEntry->u1.s.NameOffset);
            String = &ResourceDirectoryString->String[0];

            if (ResourceDirectoryString->Length == 3 &&
                String[0] == L'H' &&
                String[1] == L'I' &&
                String[2] == L'I') {
              //
              // Resource Type "HII" found
              //
              if (ResourceDirectoryEntry->u2.s.DataIsDirectory) {
                //
                // Move to next level - resource Name
                //
                if (ResourceDirectoryEntry->u2.s.OffsetToDirectory >= DirectoryEntry->Size) {
                  ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
                  return RETURN_UNSUPPORTED;
                }
                ResourceDirectory = (EFI_IMAGE_RESOURCE_DIRECTORY *) (Base + ResourceDirectoryEntry->u2.s.OffsetToDirectory);
                Offset = ResourceDirectoryEntry->u2.s.OffsetToDirectory + sizeof (EFI_IMAGE_RESOURCE_DIRECTORY) +
                         sizeof (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) * (ResourceDirectory->NumberOfNamedEntries + ResourceDirectory->NumberOfIdEntries);
                if (Offset > DirectoryEntry->Size) {
                  ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
                  return RETURN_UNSUPPORTED;
                }
                ResourceDirectoryEntry = (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *) (ResourceDirectory + 1);

                if (ResourceDirectoryEntry->u2.s.DataIsDirectory) {
                  //
                  // Move to next level - resource Language
                  //
                  if (ResourceDirectoryEntry->u2.s.OffsetToDirectory >= DirectoryEntry->Size) {
                    ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
                    return RETURN_UNSUPPORTED;
                  }
                  ResourceDirectory = (EFI_IMAGE_RESOURCE_DIRECTORY *) (Base + ResourceDirectoryEntry->u2.s.OffsetToDirectory);
                  Offset = ResourceDirectoryEntry->u2.s.OffsetToDirectory + sizeof (EFI_IMAGE_RESOURCE_DIRECTORY) +
                           sizeof (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) * (ResourceDirectory->NumberOfNamedEntries + ResourceDirectory->NumberOfIdEntries);
                  if (Offset > DirectoryEntry->Size) {
                    ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
                    return RETURN_UNSUPPORTED;
                  }
                  ResourceDirectoryEntry = (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *) (ResourceDirectory + 1);
                }
              }

              //
              // Now it ought to be resource Data
              //
              if (!ResourceDirectoryEntry->u2.s.DataIsDirectory) {
                if (ResourceDirectoryEntry->u2.OffsetToData >= DirectoryEntry->Size) {
                  ImageContext->ImageError = IMAGE_ERROR_UNSUPPORTED;
                  return RETURN_UNSUPPORTED;
                }
                ResourceDataEntry = (EFI_IMAGE_RESOURCE_DATA_ENTRY *) (Base + ResourceDirectoryEntry->u2.OffsetToData);
                ImageContext->HiiResourceData = (PHYSICAL_ADDRESS) (UINTN) PeCoffLoaderImageAddress (ImageContext, ResourceDataEntry->OffsetToData, 0);
                break;
              }
            }
          }
          ResourceDirectoryEntry++;
        }
      }
    }
  }

  return Status;
}


/**
  Reapply fixups on a fixed up PE32/PE32+ image to allow virutal calling at EFI
  runtime.

  This function reapplies relocation fixups to the PE/COFF image specified by ImageBase
  and ImageSize so the image will execute correctly when the PE/COFF image is mapped
  to the address specified by VirtualImageBase.  RelocationData must be identical
  to the FiuxupData buffer from the PE_COFF_LOADER_IMAGE_CONTEXT structure
  after this PE/COFF image was relocated with PeCoffLoaderRelocateImage().

  Note that if the platform does not maintain coherency between the instruction cache(s) and the data
  cache(s) in hardware, then the caller is responsible for performing cache maintenance operations
  prior to transferring control to a PE/COFF image that is loaded using this library.

  @param  ImageBase          The base address of a PE/COFF image that has been loaded
                             and relocated into system memory.
  @param  VirtImageBase      The request virtual address that the PE/COFF image is to
                             be fixed up for.
  @param  ImageSize          The size, in bytes, of the PE/COFF image.
  @param  RelocationData     A pointer to the relocation data that was collected when the PE/COFF
                             image was relocated using PeCoffLoaderRelocateImage().

**/
VOID
EFIAPI
PeCoffLoaderRelocateImageForRuntime (
  IN  PHYSICAL_ADDRESS        ImageBase,
  IN  PHYSICAL_ADDRESS        VirtImageBase,
  IN  UINTN                   ImageSize,
  IN  VOID                    *RelocationData
  )
{
  CHAR8                               *OldBase;
  CHAR8                               *NewBase;
  EFI_IMAGE_DOS_HEADER                *DosHdr;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Hdr;
  UINT32                              NumberOfRvaAndSizes;
  EFI_IMAGE_DATA_DIRECTORY            *DataDirectory;
  EFI_IMAGE_DATA_DIRECTORY            *RelocDir;
  EFI_IMAGE_BASE_RELOCATION           *RelocBase;
  EFI_IMAGE_BASE_RELOCATION           *RelocBaseEnd;
  EFI_IMAGE_BASE_RELOCATION           *RelocBaseOrig;
  UINT16                              *Reloc;
  UINT16                              *RelocEnd;
  CHAR8                               *Fixup;
  CHAR8                               *FixupBase;
  UINT16                              *Fixup16;
  UINT32                              *Fixup32;
  UINT64                              *Fixup64;
  CHAR8                               *FixupData;
  UINTN                               Adjust;
  RETURN_STATUS                       Status;
  PE_COFF_LOADER_IMAGE_CONTEXT        ImageContext;

  if (RelocationData == NULL || ImageBase == 0x0 || VirtImageBase == 0x0) {
    return;
  }

  OldBase = (CHAR8 *)((UINTN)ImageBase);
  NewBase = (CHAR8 *)((UINTN)VirtImageBase);
  Adjust = (UINTN) NewBase - (UINTN) OldBase;

  ImageContext.ImageAddress = ImageBase;
  ImageContext.ImageSize = ImageSize;

  //
  // Find the image's relocate dir info
  //
  DosHdr = (EFI_IMAGE_DOS_HEADER *)OldBase;
  if (DosHdr->e_magic == EFI_IMAGE_DOS_SIGNATURE) {
    //
    // Valid DOS header so get address of PE header
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)(((CHAR8 *)DosHdr) + DosHdr->e_lfanew);
  } else {
    //
    // No Dos header so assume image starts with PE header.
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)OldBase;
  }

  if (Hdr.Pe32->Signature != EFI_IMAGE_NT_SIGNATURE) {
    //
    // Not a valid PE image so Exit
    //
    return ;
  }

  if (Hdr.Pe32->OptionalHeader.Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    //
    // Use PE32 offset
    //
    NumberOfRvaAndSizes = Hdr.Pe32->OptionalHeader.NumberOfRvaAndSizes;
    DataDirectory = (EFI_IMAGE_DATA_DIRECTORY *)&(Hdr.Pe32->OptionalHeader.DataDirectory[0]);
  } else {
    //
    // Use PE32+ offset
    //
    NumberOfRvaAndSizes = Hdr.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;
    DataDirectory = (EFI_IMAGE_DATA_DIRECTORY *)&(Hdr.Pe32Plus->OptionalHeader.DataDirectory[0]);
  }

  //
  // Find the relocation block
  //
  // Per the PE/COFF spec, you can't assume that a given data directory
  // is present in the image. You have to check the NumberOfRvaAndSizes in
  // the optional header to verify a desired directory entry is there.
  //
  RelocBase = NULL;
  RelocBaseEnd = NULL;
  if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC) {
    RelocDir      = DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC;
    if ((RelocDir != NULL) && (RelocDir->Size > 0)) {
      RelocBase     = (EFI_IMAGE_BASE_RELOCATION *) PeCoffLoaderImageAddress (&ImageContext, RelocDir->VirtualAddress, 0);
      RelocBaseEnd  = (EFI_IMAGE_BASE_RELOCATION *) PeCoffLoaderImageAddress (&ImageContext,
                                                                              RelocDir->VirtualAddress + RelocDir->Size - 1,
                                                                              0
                                                                              );
    }
    if (RelocBase == NULL || RelocBaseEnd == NULL || (UINTN) RelocBaseEnd < (UINTN) RelocBase) {
      //
      // relocation block is not valid, just return
      //
      return;
    }
  } else {
    //
    // Cannot find relocations, cannot continue to relocate the image, ASSERT for this invalid image.
    //
    ASSERT (FALSE);
    return ;
  }

  //
  // ASSERT for the invalid image when RelocBase and RelocBaseEnd are both NULL.
  //
  ASSERT (RelocBase != NULL && RelocBaseEnd != NULL);

  if (Adjust != 0) {
    //
    // Run the whole relocation block. And re-fixup data that has not been
    // modified. The FixupData is used to see if the image has been modified
    // since it was relocated. This is so data sections that have been updated
    // by code will not be fixed up, since that would set them back to
    // defaults.
    //
    FixupData = RelocationData;
    RelocBaseOrig = RelocBase;
    while ((UINTN) RelocBase < (UINTN) RelocBaseEnd) {
      //
      // Add check for RelocBase->SizeOfBlock field.
      //
      if ((RelocBase->SizeOfBlock == 0) || (RelocBase->SizeOfBlock > RelocDir->Size)) {
        //
        // Data invalid, cannot continue to relocate the image, just return.
        //
        return;
      }

      Reloc     = (UINT16 *) ((UINT8 *) RelocBase + sizeof (EFI_IMAGE_BASE_RELOCATION));
      RelocEnd  = (UINT16 *) ((UINT8 *) RelocBase + RelocBase->SizeOfBlock);
      if ((UINTN)RelocEnd > (UINTN)RelocBaseOrig + RelocDir->Size) {
        return;
      }

      FixupBase = PeCoffLoaderImageAddress (&ImageContext, RelocBase->VirtualAddress, 0);
      if (FixupBase == NULL) {
        return;
      }

      //
      // Run this relocation record
      //
      while ((UINTN) Reloc < (UINTN) RelocEnd) {

        Fixup = PeCoffLoaderImageAddress (&ImageContext, RelocBase->VirtualAddress + (*Reloc & 0xFFF), 0);
        if (Fixup == NULL) {
          return;
        }
        switch ((*Reloc) >> 12) {

        case EFI_IMAGE_REL_BASED_ABSOLUTE:
          break;

        case EFI_IMAGE_REL_BASED_HIGH:
          Fixup16 = (UINT16 *) Fixup;
          if (*(UINT16 *) FixupData == *Fixup16) {
            *Fixup16 = (UINT16) (*Fixup16 + ((UINT16) ((UINT32) Adjust >> 16)));
          }

          FixupData = FixupData + sizeof (UINT16);
          break;

        case EFI_IMAGE_REL_BASED_LOW:
          Fixup16 = (UINT16 *) Fixup;
          if (*(UINT16 *) FixupData == *Fixup16) {
            *Fixup16 = (UINT16) (*Fixup16 + ((UINT16) Adjust & 0xffff));
          }

          FixupData = FixupData + sizeof (UINT16);
          break;

        case EFI_IMAGE_REL_BASED_HIGHLOW:
          Fixup32       = (UINT32 *) Fixup;
          FixupData = ALIGN_POINTER (FixupData, sizeof (UINT32));
          if (*(UINT32 *) FixupData == *Fixup32) {
            *Fixup32 = *Fixup32 + (UINT32) Adjust;
          }

          FixupData = FixupData + sizeof (UINT32);
          break;

        case EFI_IMAGE_REL_BASED_DIR64:
          Fixup64       = (UINT64 *)Fixup;
          FixupData = ALIGN_POINTER (FixupData, sizeof (UINT64));
          if (*(UINT64 *) FixupData == *Fixup64) {
            *Fixup64 = *Fixup64 + (UINT64)Adjust;
          }

          FixupData = FixupData + sizeof (UINT64);
          break;

        default:
          //
          // Only Itanium requires ConvertPeImage_Ex
          //
          Status = PeHotRelocateImageEx (Reloc, Fixup, &FixupData, Adjust);
          if (RETURN_ERROR (Status)) {
            return ;
          }
        }
        //
        // Next relocation record
        //
        Reloc += 1;
      }
      //
      // next reloc block
      //
      RelocBase = (EFI_IMAGE_BASE_RELOCATION *) RelocEnd;
    }
  }
}


/**
  Reads contents of a PE/COFF image from a buffer in system memory.

  This is the default implementation of a PE_COFF_LOADER_READ_FILE function
  that assumes FileHandle pointer to the beginning of a PE/COFF image.
  This function reads contents of the PE/COFF image that starts at the system memory
  address specified by FileHandle.  The read operation copies ReadSize bytes from the
  PE/COFF image starting at byte offset FileOffset into the buffer specified by Buffer.
  The size of the buffer actually read is returned in ReadSize.

  The caller must make sure the FileOffset and ReadSize within the file scope.

  If FileHandle is NULL, then ASSERT().
  If ReadSize is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().

  @param  FileHandle        The pointer to base of the input stream
  @param  FileOffset        Offset into the PE/COFF image to begin the read operation.
  @param  ReadSize          On input, the size in bytes of the requested read operation.
                            On output, the number of bytes actually read.
  @param  Buffer            Output buffer that contains the data read from the PE/COFF image.

  @retval RETURN_SUCCESS    Data is read from FileOffset from the Handle into
                            the buffer.
**/
RETURN_STATUS
EFIAPI
PeCoffLoaderImageReadFromMemory (
  IN     VOID    *FileHandle,
  IN     UINTN   FileOffset,
  IN OUT UINTN   *ReadSize,
  OUT    VOID    *Buffer
  )
{
  ASSERT (ReadSize != NULL);
  ASSERT (FileHandle != NULL);
  ASSERT (Buffer != NULL);

  CopyMem (Buffer, ((UINT8 *)FileHandle) + FileOffset, *ReadSize);
  return RETURN_SUCCESS;
}

/**
  Unloads a loaded PE/COFF image from memory and releases its taken resource.
  Releases any environment specific resources that were allocated when the image
  specified by ImageContext was loaded using PeCoffLoaderLoadImage().

  For NT32 emulator, the PE/COFF image loaded by system needs to release.
  For real platform, the PE/COFF image loaded by Core doesn't needs to be unloaded,
  this function can simply return RETURN_SUCCESS.

  If ImageContext is NULL, then ASSERT().

  @param  ImageContext              The pointer to the image context structure that describes the PE/COFF
                                    image to be unloaded.

  @retval RETURN_SUCCESS            The PE/COFF image was unloaded successfully.
**/
RETURN_STATUS
EFIAPI
PeCoffLoaderUnloadImage (
  IN OUT PE_COFF_LOADER_IMAGE_CONTEXT  *ImageContext
  )
{
  //
  // Applies additional environment specific actions to unload a
  // PE/COFF image if needed
  //
  PeCoffLoaderUnloadImageExtraAction (ImageContext);
  return RETURN_SUCCESS;
}
