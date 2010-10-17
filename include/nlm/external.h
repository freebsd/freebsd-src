/* NLM (NetWare Loadable Module) support for BFD.
   Copyright 1993, 1994 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support

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
   that describe how NLM is represented externally by the BFD library.
   I.E. it describes the in-file representation of NLM.  It requires
   the nlm/common.h file which contains the portions that are common to
   both the internal and external representations.

   Note that an NLM header consists of three parts:

   (1)	A fixed length header that has specific fields of known length,
	at specific offsets in the file. 

   (2)  A variable length header that has specific fields in a specific
        order, but some fields may be variable length.

   (3)	A auxiliary header that has various optional fields in no specific
        order.  There is no way to identify the end of the auxiliary headers
	except by finding a header without a recognized 'stamp'.

   The exact format of the fixed length header unfortunately varies
   from one NLM target to another, due to padding.  Each target
   defines the correct external format in a separate header file.

*/
   
/* NLM Header */

/* The version header is one of the optional auxiliary headers and
   follows the fixed length and variable length NLM headers. */

typedef struct nlmNAME(external_version_header)
{

  /* The header is recognized by "VeRsIoN#" in the stamp field. */
  char stamp[8];

  unsigned char majorVersion[NLM_TARGET_LONG_SIZE];

  unsigned char minorVersion[NLM_TARGET_LONG_SIZE];

  unsigned char revision[NLM_TARGET_LONG_SIZE];

  unsigned char year[NLM_TARGET_LONG_SIZE];

  unsigned char month[NLM_TARGET_LONG_SIZE];

  unsigned char day[NLM_TARGET_LONG_SIZE];

} NlmNAME(External_Version_Header);


typedef struct nlmNAME(external_copyright_header)
{

  /* The header is recognized by "CoPyRiGhT=" in the stamp field. */

  char stamp[10];

  unsigned char copyrightMessageLength[1];

  /* There is a variable length field here called 'copyrightMessage'
     that is the length specified by copyrightMessageLength. */

} NlmNAME(External_Copyright_Header);


typedef struct nlmNAME(external_extended_header)
{

  /* The header is recognized by "MeSsAgEs" in the stamp field. */

  char stamp[8];

  unsigned char languageID[NLM_TARGET_LONG_SIZE];

  unsigned char messageFileOffset[NLM_TARGET_LONG_SIZE];

  unsigned char messageFileLength[NLM_TARGET_LONG_SIZE];

  unsigned char messageCount[NLM_TARGET_LONG_SIZE];

  unsigned char helpFileOffset[NLM_TARGET_LONG_SIZE];

  unsigned char helpFileLength[NLM_TARGET_LONG_SIZE];

  unsigned char RPCDataOffset[NLM_TARGET_LONG_SIZE];

  unsigned char RPCDataLength[NLM_TARGET_LONG_SIZE];

  unsigned char sharedCodeOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedCodeLength[NLM_TARGET_LONG_SIZE];

  unsigned char sharedDataOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedDataLength[NLM_TARGET_LONG_SIZE];

  unsigned char sharedRelocationFixupOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedRelocationFixupCount[NLM_TARGET_LONG_SIZE];

  unsigned char sharedExternalReferenceOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedExternalReferenceCount[NLM_TARGET_LONG_SIZE];

  unsigned char sharedPublicsOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedPublicsCount[NLM_TARGET_LONG_SIZE];

  unsigned char sharedDebugRecordOffset[NLM_TARGET_LONG_SIZE];

  unsigned char sharedDebugRecordCount[NLM_TARGET_LONG_SIZE];

  unsigned char sharedInitializationOffset[NLM_TARGET_ADDRESS_SIZE];

  unsigned char SharedExitProcedureOffset[NLM_TARGET_ADDRESS_SIZE];

  unsigned char productID[NLM_TARGET_LONG_SIZE];

  unsigned char reserved0[NLM_TARGET_LONG_SIZE];

  unsigned char reserved1[NLM_TARGET_LONG_SIZE];

  unsigned char reserved2[NLM_TARGET_LONG_SIZE];

  unsigned char reserved3[NLM_TARGET_LONG_SIZE];

  unsigned char reserved4[NLM_TARGET_LONG_SIZE];

  unsigned char reserved5[NLM_TARGET_LONG_SIZE];

} NlmNAME(External_Extended_Header);


typedef struct nlmNAME(external_custom_header)
{

  /* The header is recognized by "CuStHeAd" in the stamp field. */
  char stamp[8];

  /* Length of this header.  */
  unsigned char length[NLM_TARGET_LONG_SIZE];

  /* Offset to data.  */
  unsigned char dataOffset[NLM_TARGET_LONG_SIZE];

  /* Length of data.  */
  unsigned char dataLength[NLM_TARGET_LONG_SIZE];

  /* Stamp for this customer header--we recognize "CyGnUsEx".  */
  char dataStamp[8];

} NlmNAME(External_Custom_Header);
