/* NLM (NetWare Loadable Module) support for BFD.
   Copyright (C) 1993 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* This file is part of NLM support for BFD, and contains the portions
   that describe how NLM is represented internally in the BFD library.
   I.E. it describes the in-memory representation of NLM.  It requires
   the nlm/common.h file which contains the portions that are common to
   both the internal and external representations. */
   
#if 0

/* Types used by various structures, functions, etc. */

typedef unsigned long	Nlm32_Addr;	/* Unsigned program address */
typedef unsigned long	Nlm32_Off;	/* Unsigned file offset */
typedef 	 long	Nlm32_Sword;	/* Signed large integer */
typedef unsigned long	Nlm32_Word;	/* Unsigned large integer */
typedef unsigned short	Nlm32_Half;	/* Unsigned medium integer */
typedef unsigned char	Nlm32_Char;	/* Unsigned tiny integer */

#ifdef BFD_HOST_64_BIT
typedef unsigned BFD_HOST_64_BIT	Nlm64_Addr;
typedef unsigned BFD_HOST_64_BIT	Nlm64_Off;
typedef          BFD_HOST_64_BIT	Nlm64_Sxword;
typedef unsigned BFD_HOST_64_BIT	Nlm64_Xword;
#endif
typedef          long		Nlm64_Sword;
typedef unsigned long		Nlm64_Word;
typedef unsigned short		Nlm64_Half;

#endif /* 0 */

/* This structure contains the internal form of the portion of the NLM
   header that is fixed length. */

typedef struct nlm_internal_fixed_header
{
  /* The signature field identifies the file as an NLM.  It must contain
     the signature string, which depends upon the NLM target. */

  char signature[NLM_SIGNATURE_SIZE];

  /* The version of the header.  At this time, the highest version number
     is 4. */

  long version;

  /* The name of the module, which must be a DOS name (1-8 characters followed
     by a period and a 1-3 character extension.  The first byte is the byte
     length of the name and the last byte is a null terminator byte.  This
     field is fixed length, and any unused bytes should be null bytes.  The
     value is set by the OUTPUT keyword to NLMLINK. */

  char moduleName[NLM_MODULE_NAME_SIZE];

  /* The byte offset of the code image from the start of the file. */

  file_ptr codeImageOffset;

  /* The size of the code image, in bytes. */

  bfd_size_type codeImageSize;

  /* The byte offset of the data image from the start of the file. */

  file_ptr dataImageOffset;

  /* The size of the data image, in bytes. */

  bfd_size_type dataImageSize;

  /* The size of the uninitialized data region that the loader is to be
     allocated at load time.  Uninitialized data follows the initialized
     data in the NLM address space. */

  bfd_size_type uninitializedDataSize;

  /* The byte offset of the custom data from the start of the file.  The
     custom data is set by the CUSTOM keyword to NLMLINK. */

  file_ptr customDataOffset;

  /* The size of the custom data, in bytes. */

  bfd_size_type customDataSize;

  /* The byte offset of the module dependencies from the start of the file.
     The module dependencies are determined by the MODULE keyword in
     NLMLINK. */

  file_ptr moduleDependencyOffset;

  /* The number of module dependencies at the moduleDependencyOffset. */

  long numberOfModuleDependencies;

  /* The byte offset of the relocation fixup data from the start of the file */

  file_ptr relocationFixupOffset;
  long numberOfRelocationFixups;
  file_ptr externalReferencesOffset;
  long numberOfExternalReferences;
  file_ptr publicsOffset;
  long numberOfPublics;
  file_ptr debugInfoOffset;
  long numberOfDebugRecords;
  file_ptr codeStartOffset;
  file_ptr exitProcedureOffset;
  file_ptr checkUnloadProcedureOffset;
  long moduleType;
  long flags;
} Nlm_Internal_Fixed_Header;

#define nlm32_internal_fixed_header nlm_internal_fixed_header
#define Nlm32_Internal_Fixed_Header Nlm_Internal_Fixed_Header
#define nlm64_internal_fixed_header nlm_internal_fixed_header
#define Nlm64_Internal_Fixed_Header Nlm_Internal_Fixed_Header

/* This structure contains the portions of the NLM header that are either
   variable in size in the external representation, or else are not at a
   fixed offset relative to the start of the NLM header due to preceding
   variable sized fields.

   Note that all the fields must exist in the external header, and in
   the order used here (the same order is used in the internal form
   for consistency, not out of necessity). */

typedef struct nlm_internal_variable_header
{

  /* The descriptionLength field contains the length of the text in
     descriptionText, excluding the null terminator.  The descriptionText
     field contains the NLM description obtained from the DESCRIPTION
     keyword in NLMLINK plus the null byte terminator.  The descriptionText
     can be up to NLM_MAX_DESCRIPTION_LENGTH characters. */
     
  unsigned char descriptionLength;
  char descriptionText[NLM_MAX_DESCRIPTION_LENGTH + 1];

  /* The stackSize field contains the size of the stack in bytes, as
     specified by the STACK or STACKSIZE keyword in NLMLINK.  If no size
     is specified, the default is NLM_DEFAULT_STACKSIZE. */
     
  long stackSize;

  /* The reserved field is included only for completeness.  It should contain
     zero. */

  long reserved;

  /* This field is fixed length, should contain " LONG" (note leading
     space), and is unused. */

  char oldThreadName[NLM_OLD_THREAD_NAME_LENGTH];

  /* The screenNameLength field contains the length of the actual text stored
     in the screenName field, excluding the null byte terminator.  The
     screenName field contains the screen name as specified by the SCREENNAME
     keyword in NLMLINK, and can be up to NLM_MAX_SCREEN_NAME_LENGTH
     characters. */

  unsigned char screenNameLength;
  char screenName[NLM_MAX_SCREEN_NAME_LENGTH + 1];

  /* The threadNameLength field contains the length of the actual text stored
     in the threadName field, excluding the null byte terminator.  The
     threadName field contains the thread name as specified by the THREADNAME
     keyword in NLMLINK, and can be up to NLM_MAX_THREAD_NAME_LENGTH
     characters. */

  unsigned char threadNameLength;
  char threadName[NLM_MAX_THREAD_NAME_LENGTH + 1];

} Nlm_Internal_Variable_Header;

#define nlm32_internal_variable_header nlm_internal_variable_header
#define Nlm32_Internal_Variable_Header Nlm_Internal_Variable_Header
#define nlm64_internal_variable_header nlm_internal_variable_header
#define Nlm64_Internal_Variable_Header Nlm_Internal_Variable_Header

/* The version header is one of the optional auxiliary headers and
   follows the fixed length and variable length NLM headers. */

typedef struct nlm_internal_version_header
{
  /* The header is recognized by "VeRsIoN#" in the stamp field. */
  char stamp[8];
  long majorVersion;
  long minorVersion;
  long revision;
  long year;
  long month;
  long day;
} Nlm_Internal_Version_Header;

#define nlm32_internal_version_header nlm_internal_version_header
#define Nlm32_Internal_Version_Header Nlm_Internal_Version_Header
#define nlm64_internal_version_header nlm_internal_version_header
#define Nlm64_Internal_Version_Header Nlm_Internal_Version_Header

typedef struct nlm_internal_copyright_header
{
  /* The header is recognized by "CoPyRiGhT=" in the stamp field. */
  char stamp[10];
  unsigned char copyrightMessageLength;
  char copyrightMessage[NLM_MAX_COPYRIGHT_MESSAGE_LENGTH];
} Nlm_Internal_Copyright_Header;

#define nlm32_internal_copyright_header nlm_internal_copyright_header
#define Nlm32_Internal_Copyright_Header Nlm_Internal_Copyright_Header
#define nlm64_internal_copyright_header nlm_internal_copyright_header
#define Nlm64_Internal_Copyright_Header Nlm_Internal_Copyright_Header

typedef struct nlm_internal_extended_header
{
  /* The header is recognized by "MeSsAgEs" in the stamp field. */
  char stamp[8];
  long languageID;
  file_ptr messageFileOffset;
  bfd_size_type messageFileLength;
  long messageCount;
  file_ptr helpFileOffset;
  bfd_size_type helpFileLength;
  file_ptr RPCDataOffset;
  bfd_size_type RPCDataLength;
  file_ptr sharedCodeOffset;
  bfd_size_type sharedCodeLength;
  file_ptr sharedDataOffset;
  bfd_size_type sharedDataLength;
  file_ptr sharedRelocationFixupOffset;
  long sharedRelocationFixupCount;
  file_ptr sharedExternalReferenceOffset;
  long sharedExternalReferenceCount;
  file_ptr sharedPublicsOffset;
  long sharedPublicsCount;
  file_ptr sharedDebugRecordOffset;
  long sharedDebugRecordCount;
  bfd_vma SharedInitializationOffset;
  bfd_vma SharedExitProcedureOffset;
  long productID;
  long reserved0;
  long reserved1;
  long reserved2;
  long reserved3;
  long reserved4;
  long reserved5;
} Nlm_Internal_Extended_Header;

#define nlm32_internal_extended_header nlm_internal_extended_header
#define Nlm32_Internal_Extended_Header Nlm_Internal_Extended_Header
#define nlm64_internal_extended_header nlm_internal_extended_header
#define Nlm64_Internal_Extended_Header Nlm_Internal_Extended_Header

/* The format of a custom header as stored internally is different
   from the external format.  This is how we store a custom header
   which we do not recognize.  */

typedef struct nlm_internal_custom_header
{
  /* The header is recognized by "CuStHeAd" in the stamp field. */
  char stamp[8];
  bfd_size_type hdrLength;
  file_ptr dataOffset;
  bfd_size_type dataLength;
  char dataStamp[8];
  PTR hdr;
} Nlm_Internal_Custom_Header;

#define nlm32_internal_custom_header nlm_internal_custom_header
#define Nlm32_Internal_Custom_Header Nlm_Internal_Custom_Header
#define nlm64_internal_custom_header nlm_internal_custom_header
#define Nlm64_Internal_Custom_Header Nlm_Internal_Custom_Header

/* The internal Cygnus header is written out externally as a custom
   header.  We don't try to replicate that structure here.  */

typedef struct nlm_internal_cygnus_ext_header
{
  /* The header is recognized by "CyGnUsEx" in the stamp field. */
  char stamp[8];
  /* File location of debugging information.  */
  file_ptr offset;
  /* Length of debugging information.  */
  bfd_size_type length;
} Nlm_Internal_Cygnus_Ext_Header;

#define nlm32_internal_cygnus_ext_header nlm_internal_cygnus_ext_header
#define Nlm32_Internal_Cygnus_Ext_Header Nlm_Internal_Cygnus_Ext_Header
#define nlm64_internal_cygnus_ext_header nlm_internal_cygnus_ext_header
#define Nlm64_Internal_Cygnus_Ext_Header Nlm_Internal_Cygnus_Ext_Header
