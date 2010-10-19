/* PowerPC NLM (NetWare Loadable Module) support for BFD.
   Copyright (C) 1994 Free Software Foundation, Inc.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifdef OLDFORMAT

/* The format of a PowerPC NLM changed.  These structures are only
   used in the old format.  */

/* A PowerPC NLM starts with an instance of this structure.  */

struct nlm32_powerpc_external_prefix_header
{
  /* Signature.  Must be "AppleNLM".  */
  char signature[8];
  /* Version number.  Current value is 1.  */
  unsigned char headerVersion[4];
  /* ??.  Should be set to 0.  */
  unsigned char origins[4];
  /* File creation date in standard Unix time format (seconds since
     1/1/70).  */
  unsigned char date[4];
};

#define NLM32_POWERPC_SIGNATURE "AppleNLM"
#define NLM32_POWERPC_HEADER_VERSION 1

/* The external format of a PowerPC NLM reloc.  This is the same as an
   XCOFF dynamic reloc.  */

struct nlm32_powerpc_external_reloc
{
  /* Address.  */
  unsigned char l_vaddr[4];
  /* Symbol table index.  This is 0 for .text and 1 for .data.  2
     means .bss, but I don't know if it is used.  In XCOFF, larger
     numbers are indices into the dynamic symbol table, but they are
     presumably not used in an NLM.  */
  unsigned char l_symndx[4];
  /* Relocation type.  */
  unsigned char l_rtype[2];
  /* Section number being relocated.  */
  unsigned char l_rsecnm[2];
};

#endif /* OLDFORMAT */

/* The external format of the fixed header.  */

typedef struct nlm32_powerpc_external_fixed_header
{

  /* The signature field identifies the file as an NLM.  It must contain
     the signature string, which depends upon the NLM target. */

  unsigned char signature[24];

  /* The version of the header.  At this time, the highest version number
     is 4. */

  unsigned char version[4];

  /* The name of the module, which must be a DOS name (1-8 characters followed
     by a period and a 1-3 character extension).  The first byte is the byte
     length of the name and the last byte is a null terminator byte.  This
     field is fixed length, and any unused bytes should be null bytes.  The
     value is set by the OUTPUT keyword to NLMLINK. */

  unsigned char moduleName[14];

  /* Padding to make it come out correct. */

  unsigned char pad1[2];

  /* The byte offset of the code image from the start of the file. */

  unsigned char codeImageOffset[4];

  /* The size of the code image, in bytes. */

  unsigned char codeImageSize[4];

  /* The byte offset of the data image from the start of the file. */

  unsigned char dataImageOffset[4];

  /* The size of the data image, in bytes. */

  unsigned char dataImageSize[4];

  /* The size of the uninitialized data region that the loader is to be
     allocated at load time.  Uninitialized data follows the initialized
     data in the NLM address space. */

  unsigned char uninitializedDataSize[4];

  /* The byte offset of the custom data from the start of the file.  The
     custom data is set by the CUSTOM keyword to NLMLINK.  It is possible
     for this to be EOF if there is no custom data. */

  unsigned char customDataOffset[4];

  /* The size of the custom data, in bytes. */

  unsigned char customDataSize[4];

  /* The byte offset of the module dependencies from the start of the file.
     The module dependencies are determined by the MODULE keyword in
     NLMLINK. */

  unsigned char moduleDependencyOffset[4];

  /* The number of module dependencies at the moduleDependencyOffset. */

  unsigned char numberOfModuleDependencies[4];

  /* The byte offset of the relocation fixup data from the start of the file */
     
  unsigned char relocationFixupOffset[4];

  unsigned char numberOfRelocationFixups[4];

  unsigned char externalReferencesOffset[4];

  unsigned char numberOfExternalReferences[4];

  unsigned char publicsOffset[4];

  unsigned char numberOfPublics[4];

  /* The byte offset of the internal debug info from the start of the file.
     It is possible for this to be EOF if there is no debug info. */

  unsigned char debugInfoOffset[4];

  unsigned char numberOfDebugRecords[4];

  unsigned char codeStartOffset[4];

  unsigned char exitProcedureOffset[4];

  unsigned char checkUnloadProcedureOffset[4];

  unsigned char moduleType[4];

  unsigned char flags[4];

} Nlm32_powerpc_External_Fixed_Header;
