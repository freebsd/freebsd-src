/* NLM (NetWare Loadable Module) executable support for BFD.
   Copyright 1993, 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support, using ELF support as the
   template.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libnlm.h"

/* The functions in this file do not use the names they appear to use.
   This file is actually compiled multiple times, once for each size
   of NLM target we are using.  At each size we use a different name,
   constructed by the macro nlmNAME.  For example, the function which
   is named nlm_symbol_type below is actually named nlm32_symbol_type
   in the final executable.  */

#define Nlm_External_Fixed_Header	NlmNAME (External_Fixed_Header)
#define Nlm_External_Version_Header	NlmNAME (External_Version_Header)
#define Nlm_External_Copyright_Header	NlmNAME (External_Copyright_Header)
#define Nlm_External_Extended_Header	NlmNAME (External_Extended_Header)
#define Nlm_External_Custom_Header	NlmNAME (External_Custom_Header)
#define Nlm_External_Cygnus_Ext_Header	NlmNAME (External_Cygnus_Ext_Header)

#define nlm_symbol_type			nlmNAME (symbol_type)
#define nlm_get_symtab_upper_bound	nlmNAME (get_symtab_upper_bound)
#define nlm_canonicalize_symtab		nlmNAME (canonicalize_symtab)
#define nlm_make_empty_symbol		nlmNAME (make_empty_symbol)
#define nlm_print_symbol		nlmNAME (print_symbol)
#define nlm_get_symbol_info		nlmNAME (get_symbol_info)
#define nlm_get_reloc_upper_bound	nlmNAME (get_reloc_upper_bound)
#define nlm_canonicalize_reloc		nlmNAME (canonicalize_reloc)
#define nlm_object_p			nlmNAME (object_p)
#define nlm_set_section_contents	nlmNAME (set_section_contents)
#define nlm_write_object_contents	nlmNAME (write_object_contents)

#define nlm_swap_fixed_header_in(abfd,src,dst) \
  (nlm_swap_fixed_header_in_func (abfd)) (abfd, src, dst)
#define nlm_swap_fixed_header_out(abfd,src,dst) \
  (nlm_swap_fixed_header_out_func (abfd)) (abfd, src, dst)

/* Should perhaps use put_offset, put_word, etc.  For now, the two versions
   can be handled by explicitly specifying 32 bits or "the long type".  */
#if ARCH_SIZE == 64
#define put_word	H_PUT_64
#define get_word	H_GET_64
#endif
#if ARCH_SIZE == 32
#define put_word	H_PUT_32
#define get_word	H_GET_32
#endif

/* Read and swap in the variable length header.  All the fields must
   exist in the NLM, and must exist in the order they are read here.  */

static bfd_boolean
nlm_swap_variable_header_in (bfd *abfd)
{
  unsigned char temp[NLM_TARGET_LONG_SIZE];
  bfd_size_type amt;

  /* Read the description length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->descriptionLength);
  if (bfd_bread ((void *) &nlm_variable_header (abfd)->descriptionLength,
		amt, abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->descriptionLength + 1;
  if (bfd_bread ((void *) nlm_variable_header (abfd)->descriptionText,
		amt, abfd) != amt)
    return FALSE;

  /* Read and convert the stackSize field.  */
  amt = sizeof (temp);
  if (bfd_bread ((void *) temp, amt, abfd) != amt)
    return FALSE;
  nlm_variable_header (abfd)->stackSize = get_word (abfd, (bfd_byte *) temp);

  /* Read and convert the reserved field.  */
  amt = sizeof (temp);
  if (bfd_bread ((void *) temp, amt, abfd) != amt)
    return FALSE;
  nlm_variable_header (abfd)->reserved = get_word (abfd, (bfd_byte *) temp);

  /* Read the oldThreadName field.  This field is a fixed length string.  */
  amt = sizeof (nlm_variable_header (abfd)->oldThreadName);
  if (bfd_bread ((void *) nlm_variable_header (abfd)->oldThreadName,
		amt, abfd) != amt)
    return FALSE;

  /* Read the screen name length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->screenNameLength);
  if (bfd_bread ((void *) & nlm_variable_header (abfd)->screenNameLength,
		amt, abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->screenNameLength + 1;
  if (bfd_bread ((void *) nlm_variable_header (abfd)->screenName,
		amt, abfd) != amt)
    return FALSE;

  /* Read the thread name length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->threadNameLength);
  if (bfd_bread ((void *) & nlm_variable_header (abfd)->threadNameLength,
		amt, abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->threadNameLength + 1;
  if (bfd_bread ((void *) nlm_variable_header (abfd)->threadName,
		amt, abfd) != amt)
    return FALSE;
  return TRUE;
}

/* Add a section to the bfd.  */

static bfd_boolean
add_bfd_section (bfd *abfd,
		 char *name,
		 file_ptr offset,
		 bfd_size_type size,
		 flagword flags)
{
  asection *newsect;

  newsect = bfd_make_section (abfd, name);
  if (newsect == NULL)
    return FALSE;

  newsect->vma = 0;		/* NLM's are relocatable.  */
  newsect->size = size;
  newsect->filepos = offset;
  newsect->flags = flags;
  newsect->alignment_power = bfd_log2 ((bfd_vma) 0);	/* FIXME */

  return TRUE;
}

/* Read and swap in the contents of all the auxiliary headers.  Because of
   the braindead design, we have to do strcmps on strings of indeterminate
   length to figure out what each auxiliary header is.  Even worse, we have
   no way of knowing how many auxiliary headers there are or where the end
   of the auxiliary headers are, except by finding something that doesn't
   look like a known auxiliary header.  This means that the first new type
   of auxiliary header added will break all existing tools that don't
   recognize it.  */

static bfd_boolean
nlm_swap_auxiliary_headers_in (bfd *abfd)
{
  char tempstr[16];
  file_ptr position;
  bfd_size_type amt;

  for (;;)
    {
      position = bfd_tell (abfd);
      amt = sizeof (tempstr);
      if (bfd_bread ((void *) tempstr, amt, abfd) != amt)
	return FALSE;
      if (bfd_seek (abfd, position, SEEK_SET) != 0)
	return FALSE;
      if (strncmp (tempstr, "VeRsIoN#", 8) == 0)
	{
	  Nlm_External_Version_Header thdr;

	  amt = sizeof (thdr);
	  if (bfd_bread ((void *) &thdr, amt, abfd) != amt)
	    return FALSE;
	  memcpy (nlm_version_header (abfd)->stamp, thdr.stamp,
		  sizeof (thdr.stamp));
	  nlm_version_header (abfd)->majorVersion =
	    get_word (abfd, (bfd_byte *) thdr.majorVersion);
	  nlm_version_header (abfd)->minorVersion =
	    get_word (abfd, (bfd_byte *) thdr.minorVersion);
	  nlm_version_header (abfd)->revision =
	    get_word (abfd, (bfd_byte *) thdr.revision);
	  nlm_version_header (abfd)->year =
	    get_word (abfd, (bfd_byte *) thdr.year);
	  nlm_version_header (abfd)->month =
	    get_word (abfd, (bfd_byte *) thdr.month);
	  nlm_version_header (abfd)->day =
	    get_word (abfd, (bfd_byte *) thdr.day);
	}
      else if (strncmp (tempstr, "MeSsAgEs", 8) == 0)
	{
	  Nlm_External_Extended_Header thdr;

	  amt = sizeof (thdr);
	  if (bfd_bread ((void *) &thdr, amt, abfd) != amt)
	    return FALSE;
	  memcpy (nlm_extended_header (abfd)->stamp, thdr.stamp,
		  sizeof (thdr.stamp));
	  nlm_extended_header (abfd)->languageID =
	    get_word (abfd, (bfd_byte *) thdr.languageID);
	  nlm_extended_header (abfd)->messageFileOffset =
	    get_word (abfd, (bfd_byte *) thdr.messageFileOffset);
	  nlm_extended_header (abfd)->messageFileLength =
	    get_word (abfd, (bfd_byte *) thdr.messageFileLength);
	  nlm_extended_header (abfd)->messageCount =
	    get_word (abfd, (bfd_byte *) thdr.messageCount);
	  nlm_extended_header (abfd)->helpFileOffset =
	    get_word (abfd, (bfd_byte *) thdr.helpFileOffset);
	  nlm_extended_header (abfd)->helpFileLength =
	    get_word (abfd, (bfd_byte *) thdr.helpFileLength);
	  nlm_extended_header (abfd)->RPCDataOffset =
	    get_word (abfd, (bfd_byte *) thdr.RPCDataOffset);
	  nlm_extended_header (abfd)->RPCDataLength =
	    get_word (abfd, (bfd_byte *) thdr.RPCDataLength);
	  nlm_extended_header (abfd)->sharedCodeOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedCodeOffset);
	  nlm_extended_header (abfd)->sharedCodeLength =
	    get_word (abfd, (bfd_byte *) thdr.sharedCodeLength);
	  nlm_extended_header (abfd)->sharedDataOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedDataOffset);
	  nlm_extended_header (abfd)->sharedDataLength =
	    get_word (abfd, (bfd_byte *) thdr.sharedDataLength);
	  nlm_extended_header (abfd)->sharedRelocationFixupOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedRelocationFixupOffset);
	  nlm_extended_header (abfd)->sharedRelocationFixupCount =
	    get_word (abfd, (bfd_byte *) thdr.sharedRelocationFixupCount);
	  nlm_extended_header (abfd)->sharedExternalReferenceOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedExternalReferenceOffset);
	  nlm_extended_header (abfd)->sharedExternalReferenceCount =
	    get_word (abfd, (bfd_byte *) thdr.sharedExternalReferenceCount);
	  nlm_extended_header (abfd)->sharedPublicsOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedPublicsOffset);
	  nlm_extended_header (abfd)->sharedPublicsCount =
	    get_word (abfd, (bfd_byte *) thdr.sharedPublicsCount);
	  nlm_extended_header (abfd)->sharedDebugRecordOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedDebugRecordOffset);
	  nlm_extended_header (abfd)->sharedDebugRecordCount =
	    get_word (abfd, (bfd_byte *) thdr.sharedDebugRecordCount);
	  nlm_extended_header (abfd)->SharedInitializationOffset =
	    get_word (abfd, (bfd_byte *) thdr.sharedInitializationOffset);
	  nlm_extended_header (abfd)->SharedExitProcedureOffset =
	    get_word (abfd, (bfd_byte *) thdr.SharedExitProcedureOffset);
	  nlm_extended_header (abfd)->productID =
	    get_word (abfd, (bfd_byte *) thdr.productID);
	  nlm_extended_header (abfd)->reserved0 =
	    get_word (abfd, (bfd_byte *) thdr.reserved0);
	  nlm_extended_header (abfd)->reserved1 =
	    get_word (abfd, (bfd_byte *) thdr.reserved1);
	  nlm_extended_header (abfd)->reserved2 =
	    get_word (abfd, (bfd_byte *) thdr.reserved2);
	  nlm_extended_header (abfd)->reserved3 =
	    get_word (abfd, (bfd_byte *) thdr.reserved3);
	  nlm_extended_header (abfd)->reserved4 =
	    get_word (abfd, (bfd_byte *) thdr.reserved4);
	  nlm_extended_header (abfd)->reserved5 =
	    get_word (abfd, (bfd_byte *) thdr.reserved5);
	}
      else if (strncmp (tempstr, "CoPyRiGhT=", 10) == 0)
	{
	  amt = sizeof (nlm_copyright_header (abfd)->stamp);
	  if (bfd_bread ((void *) nlm_copyright_header (abfd)->stamp,
			amt, abfd) != amt)
	    return FALSE;
	  if (bfd_bread ((void *) &(nlm_copyright_header (abfd)
				->copyrightMessageLength),
			(bfd_size_type) 1, abfd) != 1)
	    return FALSE;
	  /* The copyright message is a variable length string.  */
	  amt = nlm_copyright_header (abfd)->copyrightMessageLength + 1;
	  if (bfd_bread ((void *) nlm_copyright_header (abfd)->copyrightMessage,
			amt, abfd) != amt)
	    return FALSE;
	}
      else if (strncmp (tempstr, "CuStHeAd", 8) == 0)
	{
	  Nlm_External_Custom_Header thdr;
	  bfd_size_type hdrLength;
	  file_ptr dataOffset;
	  bfd_size_type dataLength;
	  char dataStamp[8];
	  void * hdr;

	  /* Read the stamp ("CuStHeAd").  */
	  amt = sizeof (thdr.stamp);
	  if (bfd_bread ((void *) thdr.stamp, amt, abfd) != amt)
	    return FALSE;
	  /* Read the length of this custom header.  */
	  amt = sizeof (thdr.length);
	  if (bfd_bread ((void *) thdr.length, amt, abfd) != amt)
	    return FALSE;
	  hdrLength = get_word (abfd, (bfd_byte *) thdr.length);
	  /* Read further fields if we have them.  */
	  if (hdrLength < NLM_TARGET_LONG_SIZE)
	    dataOffset = 0;
	  else
	    {
	      amt = sizeof (thdr.dataOffset);
	      if (bfd_bread ((void *) thdr.dataOffset, amt, abfd) != amt)
		return FALSE;
	      dataOffset = get_word (abfd, (bfd_byte *) thdr.dataOffset);
	    }
	  if (hdrLength < 2 * NLM_TARGET_LONG_SIZE)
	    dataLength = 0;
	  else
	    {
	      amt = sizeof (thdr.dataLength);
	      if (bfd_bread ((void *) thdr.dataLength, amt, abfd) != amt)
		return FALSE;
	      dataLength = get_word (abfd, (bfd_byte *) thdr.dataLength);
	    }
	  if (hdrLength < 2 * NLM_TARGET_LONG_SIZE + 8)
	    memset (dataStamp, 0, sizeof (dataStamp));
	  else
	    {
	      amt = sizeof (dataStamp);
	      if (bfd_bread ((void *) dataStamp, amt, abfd) != amt)
		return FALSE;
	    }

	  /* Read the rest of the header, if any.  */
	  if (hdrLength <= 2 * NLM_TARGET_LONG_SIZE + 8)
	    {
	      hdr = NULL;
	      hdrLength = 0;
	    }
	  else
	    {
	      hdrLength -= 2 * NLM_TARGET_LONG_SIZE + 8;
	      hdr = bfd_alloc (abfd, hdrLength);
	      if (hdr == NULL)
		return FALSE;
	      if (bfd_bread (hdr, hdrLength, abfd) != hdrLength)
		return FALSE;
	    }

	  /* If we have found a Cygnus header, process it.  Otherwise,
	     just save the associated data without trying to interpret
	     it.  */
	  if (strncmp (dataStamp, "CyGnUsEx", 8) == 0)
	    {
	      file_ptr pos;
	      bfd_byte *contents;
	      bfd_byte *p, *pend;

	      BFD_ASSERT (hdrLength == 0 && hdr == NULL);

	      pos = bfd_tell (abfd);
	      if (bfd_seek (abfd, dataOffset, SEEK_SET) != 0)
		return FALSE;
	      contents = bfd_alloc (abfd, dataLength);
	      if (contents == NULL)
		return FALSE;
	      if (bfd_bread (contents, dataLength, abfd) != dataLength)
		return FALSE;
	      if (bfd_seek (abfd, pos, SEEK_SET) != 0)
		return FALSE;

	      memcpy (nlm_cygnus_ext_header (abfd), "CyGnUsEx", 8);
	      nlm_cygnus_ext_header (abfd)->offset = dataOffset;
	      nlm_cygnus_ext_header (abfd)->length = dataLength;

	      /* This data this header points to provides a list of
		 the sections which were in the original object file
		 which was converted to become an NLM.  We locate
		 those sections and add them to the BFD.  Note that
		 this is likely to create a second .text, .data and
		 .bss section; retrieving the sections by name will
		 get the actual NLM sections, which is what we want to
		 happen.  The sections from the original file, which
		 may be subsets of the NLM section, can only be found
		 using bfd_map_over_sections.  */
	      p = contents;
	      pend = p + dataLength;
	      while (p < pend)
		{
		  char *name;
		  size_t l;
		  file_ptr filepos;
		  bfd_size_type size;
		  asection *newsec;

		  /* The format of this information is
		     null terminated section name
		     zeroes to adjust to 4 byte boundary
		     4 byte section data file pointer
		     4 byte section size.  */

		  name = (char *) p;
		  l = strlen (name) + 1;
		  l = (l + 3) &~ (size_t) 3;
		  p += l;
		  filepos = H_GET_32 (abfd, p);
		  p += 4;
		  size = H_GET_32 (abfd, p);
		  p += 4;

		  newsec = bfd_make_section_anyway (abfd, name);
		  if (newsec == NULL)
		    return FALSE;
		  newsec->size = size;
		  if (filepos != 0)
		    {
		      newsec->filepos = filepos;
		      newsec->flags |= SEC_HAS_CONTENTS;
		    }
		}
	    }
	  else
	    {
	      memcpy (nlm_custom_header (abfd)->stamp, thdr.stamp,
		      sizeof (thdr.stamp));
	      nlm_custom_header (abfd)->hdrLength = hdrLength;
	      nlm_custom_header (abfd)->dataOffset = dataOffset;
	      nlm_custom_header (abfd)->dataLength = dataLength;
	      memcpy (nlm_custom_header (abfd)->dataStamp, dataStamp,
		      sizeof (dataStamp));
	      nlm_custom_header (abfd)->hdr = hdr;
	    }
	}
      else
	break;
    }
  return TRUE;
}

const bfd_target *
nlm_object_p (bfd *abfd)
{
  struct nlm_obj_tdata *preserved_tdata = nlm_tdata (abfd);
  bfd_boolean (*backend_object_p) (bfd *);
  void * x_fxdhdr = NULL;
  Nlm_Internal_Fixed_Header *i_fxdhdrp;
  struct nlm_obj_tdata *new_tdata = NULL;
  const char *signature;
  enum bfd_architecture arch;
  bfd_size_type amt;

  /* Some NLM formats have a prefix before the standard NLM fixed
     header.  */
  backend_object_p = nlm_backend_object_p_func (abfd);
  if (backend_object_p)
    {
      if (!(*backend_object_p) (abfd))
	goto got_wrong_format_error;
    }

  /* Read in the fixed length portion of the NLM header in external format.  */
  amt = nlm_fixed_header_size (abfd);
  x_fxdhdr = bfd_malloc (amt);
  if (x_fxdhdr == NULL)
    goto got_no_match;

  if (bfd_bread ((void *) x_fxdhdr, amt, abfd) != amt)
    {
      if (bfd_get_error () != bfd_error_system_call)
	goto got_wrong_format_error;
      else
	goto got_no_match;
    }

  /* Allocate an instance of the nlm_obj_tdata structure and hook it up to
     the tdata pointer in the bfd.  */
  amt = sizeof (struct nlm_obj_tdata);
  new_tdata = bfd_zalloc (abfd, amt);
  if (new_tdata == NULL)
    goto got_no_match;

  nlm_tdata (abfd) = new_tdata;

  i_fxdhdrp = nlm_fixed_header (abfd);
  nlm_swap_fixed_header_in (abfd, x_fxdhdr, i_fxdhdrp);
  free (x_fxdhdr);
  x_fxdhdr = NULL;

  /* Check to see if we have an NLM file for this backend by matching
     the NLM signature.  */
  signature = nlm_signature (abfd);
  if (signature != NULL
      && *signature != '\0'
      && strncmp ((char *) i_fxdhdrp->signature, signature,
		  NLM_SIGNATURE_SIZE) != 0)
    goto got_wrong_format_error;

  /* There's no supported way to discover the endianess of an NLM, so test for
     a sane version number after doing byte swapping appropriate for this
     XVEC.  (Hack alert!)  */
  if (i_fxdhdrp->version > 0xFFFF)
    goto got_wrong_format_error;

  /* There's no supported way to check for 32 bit versus 64 bit addresses,
     so ignore this distinction for now.  (FIXME) */
  /* Swap in the rest of the required header.  */
  if (!nlm_swap_variable_header_in (abfd))
    {
      if (bfd_get_error () != bfd_error_system_call)
	goto got_wrong_format_error;
      else
	goto got_no_match;
    }

  /* Add the sections supplied by all NLM's, and then read in the
     auxiliary headers.  Reading the auxiliary headers may create
     additional sections described in the cygnus_ext header.
     From this point on we assume that we have an NLM, and do not
     treat errors as indicating the wrong format.  */
  if (!add_bfd_section (abfd, NLM_CODE_NAME,
			i_fxdhdrp->codeImageOffset,
			i_fxdhdrp->codeImageSize,
			(SEC_CODE | SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			 | SEC_RELOC))
      || !add_bfd_section (abfd, NLM_INITIALIZED_DATA_NAME,
			   i_fxdhdrp->dataImageOffset,
			   i_fxdhdrp->dataImageSize,
			   (SEC_DATA | SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS
			    | SEC_RELOC))
      || !add_bfd_section (abfd, NLM_UNINITIALIZED_DATA_NAME,
			   (file_ptr) 0,
			   i_fxdhdrp->uninitializedDataSize,
			   SEC_ALLOC))
    goto got_no_match;

  if (!nlm_swap_auxiliary_headers_in (abfd))
    goto got_no_match;

  if (nlm_fixed_header (abfd)->numberOfRelocationFixups != 0
      || nlm_fixed_header (abfd)->numberOfExternalReferences != 0)
    abfd->flags |= HAS_RELOC;
  if (nlm_fixed_header (abfd)->numberOfPublics != 0
      || nlm_fixed_header (abfd)->numberOfDebugRecords != 0
      || nlm_fixed_header (abfd)->numberOfExternalReferences != 0)
    abfd->flags |= HAS_SYMS;

  arch = nlm_architecture (abfd);
  if (arch != bfd_arch_unknown)
    bfd_default_set_arch_mach (abfd, arch, (unsigned long) 0);

  abfd->flags |= EXEC_P;
  bfd_get_start_address (abfd) = nlm_fixed_header (abfd)->codeStartOffset;

  return abfd->xvec;

got_wrong_format_error:
  bfd_set_error (bfd_error_wrong_format);
got_no_match:
  nlm_tdata (abfd) = preserved_tdata;
  if (new_tdata != NULL)
    bfd_release (abfd, new_tdata);
  if (x_fxdhdr != NULL)
    free (x_fxdhdr);

  return NULL;
}

/* Swap and write out the variable length header.  All the fields must
   exist in the NLM, and must exist in this order.  */

static bfd_boolean
nlm_swap_variable_header_out (bfd *abfd)
{
  bfd_byte temp[NLM_TARGET_LONG_SIZE];
  bfd_size_type amt;

  /* Write the description length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->descriptionLength);
  if (bfd_bwrite (& nlm_variable_header (abfd)->descriptionLength, amt,
		  abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->descriptionLength + 1;
  if (bfd_bwrite ((void *) nlm_variable_header (abfd)->descriptionText, amt,
		  abfd) != amt)
    return FALSE;

  /* Convert and write the stackSize field.  */
  put_word (abfd, (bfd_vma) nlm_variable_header (abfd)->stackSize, temp);
  amt = sizeof (temp);
  if (bfd_bwrite (temp, amt, abfd) != amt)
    return FALSE;

  /* Convert and write the reserved field.  */
  put_word (abfd, (bfd_vma) nlm_variable_header (abfd)->reserved, temp);
  amt = sizeof (temp);
  if (bfd_bwrite (temp, amt, abfd) != amt)
    return FALSE;

  /* Write the oldThreadName field.  This field is a fixed length string.  */
  amt = sizeof (nlm_variable_header (abfd)->oldThreadName);
  if (bfd_bwrite (nlm_variable_header (abfd)->oldThreadName, amt,
		  abfd) != amt)
    return FALSE;

  /* Write the screen name length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->screenNameLength);
  if (bfd_bwrite (& nlm_variable_header (abfd)->screenNameLength, amt,
		 abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->screenNameLength + 1;
  if (bfd_bwrite (nlm_variable_header (abfd)->screenName, amt, abfd) != amt)
    return FALSE;

  /* Write the thread name length and text members.  */
  amt = sizeof (nlm_variable_header (abfd)->threadNameLength);
  if (bfd_bwrite (& nlm_variable_header (abfd)->threadNameLength, amt,
		 abfd) != amt)
    return FALSE;
  amt = nlm_variable_header (abfd)->threadNameLength + 1;
  if (bfd_bwrite (nlm_variable_header (abfd)->threadName, amt, abfd) != amt)
    return FALSE;
  return TRUE;
}

/* Return whether there is a non-zero byte in a memory block.  */

static bfd_boolean
find_nonzero (void * buf, size_t size)
{
  char *p = (char *) buf;

  while (size-- != 0)
    if (*p++ != 0)
      return TRUE;
  return FALSE;
}

/* Swap out the contents of the auxiliary headers.  We create those
   auxiliary headers which have been set non-zero.  We do not require
   the caller to set up the stamp fields.  */

static bfd_boolean
nlm_swap_auxiliary_headers_out (bfd *abfd)
{
  bfd_size_type amt;

  /* Write out the version header if there is one.  */
  if (find_nonzero (nlm_version_header (abfd),
		    sizeof (Nlm_Internal_Version_Header)))
    {
      Nlm_External_Version_Header thdr;

      memcpy (thdr.stamp, "VeRsIoN#", 8);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->majorVersion,
		(bfd_byte *) thdr.majorVersion);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->minorVersion,
		(bfd_byte *) thdr.minorVersion);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->revision,
		(bfd_byte *) thdr.revision);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->year,
		(bfd_byte *) thdr.year);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->month,
		(bfd_byte *) thdr.month);
      put_word (abfd, (bfd_vma) nlm_version_header (abfd)->day,
		(bfd_byte *) thdr.day);
      if (bfd_bwrite ((void *) &thdr, (bfd_size_type) sizeof (thdr), abfd)
	  != sizeof (thdr))
	return FALSE;
    }

  /* Note - the CoPyRiGhT tag is emitted before the MeSsAgEs
     tag in order to make the NW4.x and NW5.x loaders happy.  */

  /* Write out the copyright header if there is one.  */
  if (find_nonzero (nlm_copyright_header (abfd),
		    sizeof (Nlm_Internal_Copyright_Header)))
    {
      Nlm_External_Copyright_Header thdr;

      memcpy (thdr.stamp, "CoPyRiGhT=", 10);
      amt = sizeof (thdr.stamp);
      if (bfd_bwrite ((void *) thdr.stamp, amt, abfd) != amt)
	return FALSE;
      thdr.copyrightMessageLength[0] =
	nlm_copyright_header (abfd)->copyrightMessageLength;
      amt = 1;
      if (bfd_bwrite ((void *) thdr.copyrightMessageLength, amt, abfd) != amt)
	return FALSE;
      /* The copyright message is a variable length string.  */
      amt = nlm_copyright_header (abfd)->copyrightMessageLength + 1;
      if (bfd_bwrite ((void *) nlm_copyright_header (abfd)->copyrightMessage,
		     amt, abfd) != amt)
	return FALSE;
    }

  /* Write out the extended header if there is one.  */
  if (find_nonzero (nlm_extended_header (abfd),
		    sizeof (Nlm_Internal_Extended_Header)))
    {
      Nlm_External_Extended_Header thdr;

      memcpy (thdr.stamp, "MeSsAgEs", 8);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->languageID,
		(bfd_byte *) thdr.languageID);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->messageFileOffset,
		(bfd_byte *) thdr.messageFileOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->messageFileLength,
		(bfd_byte *) thdr.messageFileLength);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->messageCount,
		(bfd_byte *) thdr.messageCount);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->helpFileOffset,
		(bfd_byte *) thdr.helpFileOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->helpFileLength,
		(bfd_byte *) thdr.helpFileLength);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->RPCDataOffset,
		(bfd_byte *) thdr.RPCDataOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->RPCDataLength,
		(bfd_byte *) thdr.RPCDataLength);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedCodeOffset,
		(bfd_byte *) thdr.sharedCodeOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedCodeLength,
		(bfd_byte *) thdr.sharedCodeLength);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedDataOffset,
		(bfd_byte *) thdr.sharedDataOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedDataLength,
		(bfd_byte *) thdr.sharedDataLength);
      put_word (abfd,
	  (bfd_vma) nlm_extended_header (abfd)->sharedRelocationFixupOffset,
		(bfd_byte *) thdr.sharedRelocationFixupOffset);
      put_word (abfd,
	   (bfd_vma) nlm_extended_header (abfd)->sharedRelocationFixupCount,
		(bfd_byte *) thdr.sharedRelocationFixupCount);
      put_word (abfd,
	(bfd_vma) nlm_extended_header (abfd)->sharedExternalReferenceOffset,
		(bfd_byte *) thdr.sharedExternalReferenceOffset);
      put_word (abfd,
	 (bfd_vma) nlm_extended_header (abfd)->sharedExternalReferenceCount,
		(bfd_byte *) thdr.sharedExternalReferenceCount);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedPublicsOffset,
		(bfd_byte *) thdr.sharedPublicsOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedPublicsCount,
		(bfd_byte *) thdr.sharedPublicsCount);
      put_word (abfd,
	      (bfd_vma) nlm_extended_header (abfd)->sharedDebugRecordOffset,
		(bfd_byte *) thdr.sharedDebugRecordOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->sharedDebugRecordCount,
		(bfd_byte *) thdr.sharedDebugRecordCount);
      put_word (abfd,
	   (bfd_vma) nlm_extended_header (abfd)->SharedInitializationOffset,
		(bfd_byte *) thdr.sharedInitializationOffset);
      put_word (abfd,
	    (bfd_vma) nlm_extended_header (abfd)->SharedExitProcedureOffset,
		(bfd_byte *) thdr.SharedExitProcedureOffset);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->productID,
		(bfd_byte *) thdr.productID);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved0,
		(bfd_byte *) thdr.reserved0);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved1,
		(bfd_byte *) thdr.reserved1);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved2,
		(bfd_byte *) thdr.reserved2);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved3,
		(bfd_byte *) thdr.reserved3);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved4,
		(bfd_byte *) thdr.reserved4);
      put_word (abfd,
		(bfd_vma) nlm_extended_header (abfd)->reserved5,
		(bfd_byte *) thdr.reserved5);
      if (bfd_bwrite ((void *) &thdr, (bfd_size_type) sizeof (thdr), abfd)
	  != sizeof (thdr))
	return FALSE;
    }

  /* Write out the custom header if there is one.   */
  if (find_nonzero (nlm_custom_header (abfd),
		    sizeof (Nlm_Internal_Custom_Header)))
    {
      Nlm_External_Custom_Header thdr;
      bfd_boolean ds;
      bfd_size_type hdrLength;

      ds = find_nonzero (nlm_custom_header (abfd)->dataStamp,
			 sizeof (nlm_custom_header (abfd)->dataStamp));
      memcpy (thdr.stamp, "CuStHeAd", 8);
      hdrLength = (2 * NLM_TARGET_LONG_SIZE + (ds ? 8 : 0)
		   + nlm_custom_header (abfd)->hdrLength);
      put_word (abfd, hdrLength, thdr.length);
      put_word (abfd, (bfd_vma) nlm_custom_header (abfd)->dataOffset,
		thdr.dataOffset);
      put_word (abfd, (bfd_vma) nlm_custom_header (abfd)->dataLength,
		thdr.dataLength);
      if (! ds)
	{
	  BFD_ASSERT (nlm_custom_header (abfd)->hdrLength == 0);
	  amt = sizeof (thdr) - sizeof (thdr.dataStamp);
	  if (bfd_bwrite ((void *) &thdr, amt, abfd) != amt)
	    return FALSE;
	}
      else
	{
	  memcpy (thdr.dataStamp, nlm_custom_header (abfd)->dataStamp,
		  sizeof (thdr.dataStamp));
	  amt = sizeof (thdr);
	  if (bfd_bwrite ((void *) &thdr, amt, abfd) != amt)
	    return FALSE;
	  amt = nlm_custom_header (abfd)->hdrLength;
	  if (bfd_bwrite (nlm_custom_header (abfd)->hdr, amt, abfd) != amt)
	    return FALSE;
	}
    }

  /* Write out the Cygnus debugging header if there is one.  */
  if (find_nonzero (nlm_cygnus_ext_header (abfd),
		    sizeof (Nlm_Internal_Cygnus_Ext_Header)))
    {
      Nlm_External_Custom_Header thdr;

      memcpy (thdr.stamp, "CuStHeAd", 8);
      put_word (abfd, (bfd_vma) 2 * NLM_TARGET_LONG_SIZE + 8,
		(bfd_byte *) thdr.length);
      put_word (abfd, (bfd_vma) nlm_cygnus_ext_header (abfd)->offset,
		(bfd_byte *) thdr.dataOffset);
      put_word (abfd, (bfd_vma) nlm_cygnus_ext_header (abfd)->length,
		(bfd_byte *) thdr.dataLength);
      memcpy (thdr.dataStamp, "CyGnUsEx", 8);
      amt = sizeof (thdr);
      if (bfd_bwrite ((void *) &thdr, amt, abfd) != amt)
	return FALSE;
    }

  return TRUE;
}

/* We read the NLM's public symbols and use it to generate a bfd symbol
   table (hey, it's better than nothing) on a one-for-one basis.  Thus
   use the number of public symbols as the number of bfd symbols we will
   have once we actually get around to reading them in.

   Return the number of bytes required to hold the symtab vector, based on
   the count plus 1, since we will NULL terminate the vector allocated based
   on this size.  */

long
nlm_get_symtab_upper_bound (bfd *abfd)
{
  Nlm_Internal_Fixed_Header *i_fxdhdrp;	/* Nlm file header, internal form.  */
  long symcount;
  long symtab_size = 0;

  i_fxdhdrp = nlm_fixed_header (abfd);
  symcount = (i_fxdhdrp->numberOfPublics
	      + i_fxdhdrp->numberOfDebugRecords
	      + i_fxdhdrp->numberOfExternalReferences);
  symtab_size = (symcount + 1) * (sizeof (asymbol));
  return symtab_size;
}

/* Slurp in nlm symbol table.

   In the external (in-file) form, NLM export records are variable length,
   with the following form:

	1 byte		length of the symbol name (N)
	N bytes		the symbol name
	4 bytes		the symbol offset from start of it's section

   We also read in the debugging symbols and import records.  Import
   records are treated as undefined symbols.  As we read the import
   records we also read in the associated reloc information, which is
   attached to the symbol.

   The bfd symbols are copied to SYMvoid *S.

   When we return, the bfd symcount is either zero or contains the correct
   number of symbols.  */

static bfd_boolean
nlm_slurp_symbol_table (bfd *abfd)
{
  Nlm_Internal_Fixed_Header *i_fxdhdrp;	/* Nlm file header, internal form.  */
  bfd_size_type totsymcount;	/* Number of NLM symbols.  */
  bfd_size_type symcount;	/* Counter of NLM symbols.  */
  nlm_symbol_type *sym;		/* Pointer to current bfd symbol.  */
  unsigned char symlength;	/* Symbol length read into here.  */
  unsigned char symtype;	/* Type of debugging symbol.  */
  bfd_byte temp[NLM_TARGET_LONG_SIZE];	/* Symbol offsets read into here.  */
  bfd_boolean (*read_import_func) (bfd *, nlm_symbol_type *);
  bfd_boolean (*set_public_section_func) (bfd *, nlm_symbol_type *);
  bfd_size_type amt;

  if (nlm_get_symbols (abfd) != NULL)
    return TRUE;

  /* Read each raw NLM symbol, using the information to create a canonical bfd
     symbol table entry.

     Note that we allocate the initial bfd canonical symbol buffer based on a
     one-to-one mapping of the NLM symbols to canonical symbols.  We actually
     use all the NLM symbols, so there will be no space left over at the end.
     When we have all the symbols, we build the caller's pointer vector.  */

  abfd->symcount = 0;
  i_fxdhdrp = nlm_fixed_header (abfd);
  totsymcount = (i_fxdhdrp->numberOfPublics
		 + i_fxdhdrp->numberOfDebugRecords
		 + i_fxdhdrp->numberOfExternalReferences);
  if (totsymcount == 0)
    return TRUE;

  if (bfd_seek (abfd, i_fxdhdrp->publicsOffset, SEEK_SET) != 0)
    return FALSE;

  amt = totsymcount * sizeof (nlm_symbol_type);
  sym = bfd_zalloc (abfd, amt);
  if (!sym)
    return FALSE;
  nlm_set_symbols (abfd, sym);

  /* We use the bfd's symcount directly as the control count, so that early
     termination of the loop leaves the symcount correct for the symbols that
     were read.  */

  set_public_section_func = nlm_set_public_section_func (abfd);
  symcount = i_fxdhdrp->numberOfPublics;
  while (abfd->symcount < symcount)
    {
      amt = sizeof (symlength);
      if (bfd_bread ((void *) &symlength, amt, abfd) != amt)
	return FALSE;
      amt = symlength;
      sym->symbol.the_bfd = abfd;
      sym->symbol.name = bfd_alloc (abfd, amt + 1);
      if (!sym->symbol.name)
	return FALSE;
      if (bfd_bread ((void *) sym->symbol.name, amt, abfd) != amt)
	return FALSE;
      /* Cast away const.  */
      ((char *) (sym->symbol.name))[symlength] = '\0';
      amt = sizeof (temp);
      if (bfd_bread ((void *) temp, amt, abfd) != amt)
	return FALSE;
      sym->symbol.flags = BSF_GLOBAL | BSF_EXPORT;
      sym->symbol.value = get_word (abfd, temp);
      if (set_public_section_func)
	{
	  /* Most backends can use the code below, but unfortunately
	     some use a different scheme.  */
	  if (! (*set_public_section_func) (abfd, sym))
	    return FALSE;
	}
      else
	{
	  if (sym->symbol.value & NLM_HIBIT)
	    {
	      sym->symbol.value &= ~NLM_HIBIT;
	      sym->symbol.flags |= BSF_FUNCTION;
	      sym->symbol.section =
		bfd_get_section_by_name (abfd, NLM_CODE_NAME);
	    }
	  else
	    sym->symbol.section =
	      bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
	}
      sym->rcnt = 0;
      abfd->symcount++;
      sym++;
    }

  /* Read the debugging records.  */

  if (i_fxdhdrp->numberOfDebugRecords > 0)
    {
      if (bfd_seek (abfd, i_fxdhdrp->debugInfoOffset, SEEK_SET) != 0)
	return FALSE;

      symcount += i_fxdhdrp->numberOfDebugRecords;
      while (abfd->symcount < symcount)
	{
	  amt = sizeof (symtype);
	  if (bfd_bread ((void *) &symtype, amt, abfd) != amt)
	    return FALSE;
	  amt = sizeof (temp);
	  if (bfd_bread ((void *) temp, amt, abfd) != amt)
	    return FALSE;
	  amt = sizeof (symlength);
	  if (bfd_bread ((void *) &symlength, amt, abfd) != amt)
	    return FALSE;
	  amt = symlength;
	  sym->symbol.the_bfd = abfd;
	  sym->symbol.name = bfd_alloc (abfd, amt + 1);
	  if (!sym->symbol.name)
	    return FALSE;
	  if (bfd_bread ((void *) sym->symbol.name, amt, abfd) != amt)
	    return FALSE;
	  /* Cast away const.  */
	  ((char *) (sym->symbol.name))[symlength] = '\0';
	  sym->symbol.flags = BSF_LOCAL;
	  sym->symbol.value = get_word (abfd, temp);

	  if (symtype == 0)
	    sym->symbol.section =
	      bfd_get_section_by_name (abfd, NLM_INITIALIZED_DATA_NAME);
	  else if (symtype == 1)
	    {
	      sym->symbol.flags |= BSF_FUNCTION;
	      sym->symbol.section =
		bfd_get_section_by_name (abfd, NLM_CODE_NAME);
	    }
	  else
	    sym->symbol.section = bfd_abs_section_ptr;

	  sym->rcnt = 0;
	  abfd->symcount++;
	  sym++;
	}
    }

  /* Read in the import records.  We can only do this if we know how
     to read relocs for this target.  */
  read_import_func = nlm_read_import_func (abfd);
  if (read_import_func != NULL)
    {
      if (bfd_seek (abfd, i_fxdhdrp->externalReferencesOffset, SEEK_SET) != 0)
	return FALSE;

      symcount += i_fxdhdrp->numberOfExternalReferences;
      while (abfd->symcount < symcount)
	{
	  if (! (*read_import_func) (abfd, sym))
	    return FALSE;
	  sym++;
	  abfd->symcount++;
	}
    }

  return TRUE;
}

/* Note that bfd_get_symcount is guaranteed to be zero if slurping the
   symbol table fails.  */

long
nlm_canonicalize_symtab (bfd *abfd, asymbol **alocation)
{
  nlm_symbol_type *symbase;
  bfd_size_type counter = 0;

  if (! nlm_slurp_symbol_table (abfd))
    return -1;
  symbase = nlm_get_symbols (abfd);
  while (counter < bfd_get_symcount (abfd))
    {
      *alocation++ = &symbase->symbol;
      symbase++;
      counter++;
    }
  *alocation = NULL;
  return bfd_get_symcount (abfd);
}

/* Make an NLM symbol.  There is nothing special to do here.  */

asymbol *
nlm_make_empty_symbol (bfd *abfd)
{
  bfd_size_type amt = sizeof (nlm_symbol_type);
  nlm_symbol_type *new = bfd_zalloc (abfd, amt);

  if (new == NULL)
    return NULL;
  new->symbol.the_bfd = abfd;
  return & new->symbol;
}

/* Get symbol information.  */

void
nlm_get_symbol_info (bfd *ignore_abfd ATTRIBUTE_UNUSED,
		     asymbol *symbol,
		     symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

/* Print symbol information.  */

void
nlm_print_symbol (bfd *abfd,
		  void * afile,
		  asymbol *symbol,
		  bfd_print_symbol_type how)
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
    case bfd_print_symbol_more:
      if (symbol->name)
	fprintf (file, "%s", symbol->name);
      break;
    case bfd_print_symbol_all:
      bfd_print_symbol_vandf (abfd, (void *) file, symbol);
      fprintf (file, " %-5s", symbol->section->name);
      if (symbol->name)
	fprintf (file, " %s", symbol->name);
      break;
    }
}

/* Get the relocs for an NLM file.  There are two types of relocs.
   Imports are relocs against symbols defined in other NLM files.  We
   treat these as relocs against global symbols.  Relocation fixups
   are internal relocs.

   The actual format used to store the relocs is machine specific.  */

/* Read in the relocation fixup information.  This is stored in
   nlm_relocation_fixups, an array of arelent structures, and
   nlm_relocation_fixup_secs, an array of section pointers.  The
   section pointers are needed because the relocs are not sorted by
   section.  */

static bfd_boolean
nlm_slurp_reloc_fixups (bfd *abfd)
{
  bfd_boolean (*read_func) (bfd *, nlm_symbol_type *, asection **, arelent *);
  bfd_size_type count, amt;
  arelent *rels;
  asection **secs;

  if (nlm_relocation_fixups (abfd) != NULL)
    return TRUE;
  read_func = nlm_read_reloc_func (abfd);
  if (read_func == NULL)
    return TRUE;

  if (bfd_seek (abfd, nlm_fixed_header (abfd)->relocationFixupOffset,
		SEEK_SET) != 0)
    return FALSE;

  count = nlm_fixed_header (abfd)->numberOfRelocationFixups;
  amt = count * sizeof (arelent);
  rels = bfd_alloc (abfd, amt);
  amt = count * sizeof (asection *);
  secs = bfd_alloc (abfd, amt);
  if ((rels == NULL || secs == NULL) && count != 0)
    return FALSE;
  nlm_relocation_fixups (abfd) = rels;
  nlm_relocation_fixup_secs (abfd) = secs;

  /* We have to read piece by piece, because we don't know how large
     the machine specific reloc information is.  */
  while (count-- != 0)
    {
      if (! (*read_func) (abfd, NULL, secs, rels))
	{
	  nlm_relocation_fixups (abfd) = NULL;
	  nlm_relocation_fixup_secs (abfd) = NULL;
	  return FALSE;
	}
      ++secs;
      ++rels;
    }

  return TRUE;
}

/* Get the number of relocs.  This really just returns an upper bound,
   since it does not attempt to distinguish them based on the section.
   That will be handled when they are actually read.  */

long
nlm_get_reloc_upper_bound (bfd *abfd, asection *sec)
{
  nlm_symbol_type *syms;
  bfd_size_type count;
  unsigned int ret;

  /* If we don't know how to read relocs, just return 0.  */
  if (nlm_read_reloc_func (abfd) == NULL)
    return -1;
  /* Make sure we have either the code or the data section.  */
  if ((bfd_get_section_flags (abfd, sec) & (SEC_CODE | SEC_DATA)) == 0)
    return 0;

  syms = nlm_get_symbols (abfd);
  if (syms == NULL)
    {
      if (! nlm_slurp_symbol_table (abfd))
	return -1;
      syms = nlm_get_symbols (abfd);
    }

  ret = nlm_fixed_header (abfd)->numberOfRelocationFixups;

  count = bfd_get_symcount (abfd);
  while (count-- != 0)
    {
      ret += syms->rcnt;
      ++syms;
    }

  return (ret + 1) * sizeof (arelent *);
}

/* Get the relocs themselves.  */

long
nlm_canonicalize_reloc (bfd *abfd,
			asection *sec,
			arelent **relptr,
			asymbol **symbols)
{
  arelent *rels;
  asection **secs;
  bfd_size_type count, i;
  long ret;

  /* Get the relocation fixups.  */
  rels = nlm_relocation_fixups (abfd);
  if (rels == NULL)
    {
      if (! nlm_slurp_reloc_fixups (abfd))
	return -1;
      rels = nlm_relocation_fixups (abfd);
    }
  secs = nlm_relocation_fixup_secs (abfd);

  ret = 0;
  count = nlm_fixed_header (abfd)->numberOfRelocationFixups;
  for (i = 0; i < count; i++, rels++, secs++)
    {
      if (*secs == sec)
	{
	  *relptr++ = rels;
	  ++ret;
	}
    }

  /* Get the import symbols.  */
  count = bfd_get_symcount (abfd);
  for (i = 0; i < count; i++, symbols++)
    {
      asymbol *sym;

      sym = *symbols;
      if (bfd_asymbol_flavour (sym) == bfd_target_nlm_flavour)
	{
	  nlm_symbol_type *nlm_sym;
	  bfd_size_type j;

	  nlm_sym = (nlm_symbol_type *) sym;
	  for (j = 0; j < nlm_sym->rcnt; j++)
	    {
	      if (nlm_sym->relocs[j].section == sec)
		{
		  *relptr = &nlm_sym->relocs[j].reloc;
		  (*relptr)->sym_ptr_ptr = symbols;
		  ++relptr;
		  ++ret;
		}
	    }
	}
    }

  *relptr = NULL;

  return ret;
}

/* Compute the section file positions for an NLM file.  All variable
   length data in the file headers must be set before this function is
   called.  If the variable length data is changed later, the
   resulting object file will be incorrect.  Unfortunately, there is
   no way to check this.

   This routine also sets the Size and Offset fields in the fixed
   header.

   It also looks over the symbols and moves any common symbols into
   the .bss section; NLM has no way to represent a common symbol.
   This approach means that either the symbols must already have been
   set at this point, or there must be no common symbols.  We need to
   move the symbols at this point so that mangle_relocs can see the
   final values.  */

static bfd_boolean
nlm_compute_section_file_positions (bfd *abfd)
{
  file_ptr sofar;
  asection *sec;
  bfd_vma text, data, bss;
  bfd_vma text_low, data_low;
  unsigned int text_align, data_align, other_align;
  file_ptr text_ptr, data_ptr, other_ptr;
  asection *bss_sec;
  asymbol **sym_ptr_ptr;

  if (abfd->output_has_begun)
    return TRUE;

  /* Make sure we have a section to hold uninitialized data.  */
  bss_sec = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);
  if (bss_sec == NULL)
    {
      if (!add_bfd_section (abfd, NLM_UNINITIALIZED_DATA_NAME,
			    (file_ptr) 0, (bfd_size_type) 0,
			    SEC_ALLOC))
	return FALSE;
      bss_sec = bfd_get_section_by_name (abfd, NLM_UNINITIALIZED_DATA_NAME);
    }

  abfd->output_has_begun = TRUE;

  /* The fixed header.  */
  sofar = nlm_optional_prefix_size (abfd) + nlm_fixed_header_size (abfd);

  /* The variable header.  */
  sofar += (sizeof (nlm_variable_header (abfd)->descriptionLength)
	    + nlm_variable_header (abfd)->descriptionLength + 1
	    + NLM_TARGET_LONG_SIZE	/* stackSize */
	    + NLM_TARGET_LONG_SIZE	/* reserved */
	    + sizeof (nlm_variable_header (abfd)->oldThreadName)
	    + sizeof (nlm_variable_header (abfd)->screenNameLength)
	    + nlm_variable_header (abfd)->screenNameLength + 1
	    + sizeof (nlm_variable_header (abfd)->threadNameLength)
	    + nlm_variable_header (abfd)->threadNameLength + 1);

  /* The auxiliary headers.  */
  if (find_nonzero (nlm_version_header (abfd),
		    sizeof (Nlm_Internal_Version_Header)))
    sofar += sizeof (Nlm_External_Version_Header);
  if (find_nonzero (nlm_extended_header (abfd),
		    sizeof (Nlm_Internal_Extended_Header)))
    sofar += sizeof (Nlm_External_Extended_Header);
  if (find_nonzero (nlm_copyright_header (abfd),
		    sizeof (Nlm_Internal_Copyright_Header)))
    sofar += (sizeof (Nlm_External_Copyright_Header)
	      + nlm_copyright_header (abfd)->copyrightMessageLength + 1);
  if (find_nonzero (nlm_custom_header (abfd),
		    sizeof (Nlm_Internal_Custom_Header)))
    sofar += (sizeof (Nlm_External_Custom_Header)
	      + nlm_custom_header (abfd)->hdrLength);
  if (find_nonzero (nlm_cygnus_ext_header (abfd),
		    sizeof (Nlm_Internal_Cygnus_Ext_Header)))
    sofar += sizeof (Nlm_External_Custom_Header);

  /* Compute the section file positions in two passes.  First get the
     sizes of the text and data sections, and then set the file
     positions.  This code aligns the sections in the file using the
     same alignment restrictions that apply to the sections in memory;
     this may not be necessary.  */
  text = 0;
  text_low = (bfd_vma) - 1;
  text_align = 0;
  data = 0;
  data_low = (bfd_vma) - 1;
  data_align = 0;
  bss = 0;
  other_align = 0;
  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      flagword f;

      sec->size = BFD_ALIGN (sec->size, 1 << sec->alignment_power);

      f = bfd_get_section_flags (abfd, sec);
      if (f & SEC_CODE)
	{
	  text += sec->size;
	  if (bfd_get_section_vma (abfd, sec) < text_low)
	    text_low = bfd_get_section_vma (abfd, sec);
	  if (sec->alignment_power > text_align)
	    text_align = sec->alignment_power;
	}
      else if (f & SEC_DATA)
	{
	  data += sec->size;
	  if (bfd_get_section_vma (abfd, sec) < data_low)
	    data_low = bfd_get_section_vma (abfd, sec);
	  if (sec->alignment_power > data_align)
	    data_align = sec->alignment_power;
	}
      else if (f & SEC_HAS_CONTENTS)
	{
	  if (sec->alignment_power > other_align)
	    other_align = sec->alignment_power;
	}
      else if (f & SEC_ALLOC)
	bss += sec->size;
    }

  nlm_set_text_low (abfd, text_low);
  nlm_set_data_low (abfd, data_low);

  if (nlm_no_uninitialized_data (abfd))
    {
      /* This NetWare format does not use uninitialized data.  We must
	 increase the size of the data section.  We will never wind up
	 writing those file locations, so they will remain zero.  */
      data += bss;
      bss = 0;
    }

  text_ptr = BFD_ALIGN (sofar, 1 << text_align);
  data_ptr = BFD_ALIGN (text_ptr + text, 1 << data_align);
  other_ptr = BFD_ALIGN (data_ptr + data, 1 << other_align);

  /* Fill in some fields in the header for which we now have the
     information.  */
  nlm_fixed_header (abfd)->codeImageOffset = text_ptr;
  nlm_fixed_header (abfd)->codeImageSize = text;
  nlm_fixed_header (abfd)->dataImageOffset = data_ptr;
  nlm_fixed_header (abfd)->dataImageSize = data;
  nlm_fixed_header (abfd)->uninitializedDataSize = bss;

  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      flagword f;

      f = bfd_get_section_flags (abfd, sec);

      if (f & SEC_CODE)
	{
	  sec->filepos = text_ptr;
	  text_ptr += sec->size;
	}
      else if (f & SEC_DATA)
	{
	  sec->filepos = data_ptr;
	  data_ptr += sec->size;
	}
      else if (f & SEC_HAS_CONTENTS)
	{
	  sec->filepos = other_ptr;
	  other_ptr += sec->size;
	}
    }

  nlm_fixed_header (abfd)->relocationFixupOffset = other_ptr;

  /* Move all common symbols into the .bss section.  */

  sym_ptr_ptr = bfd_get_outsymbols (abfd);
  if (sym_ptr_ptr != NULL)
    {
      asymbol **sym_end;
      bfd_vma add;

      sym_end = sym_ptr_ptr + bfd_get_symcount (abfd);
      add = 0;
      for (; sym_ptr_ptr < sym_end; sym_ptr_ptr++)
	{
	  asymbol *sym;
	  bfd_vma size;

	  sym = *sym_ptr_ptr;

	  if (!bfd_is_com_section (bfd_get_section (sym)))
	    continue;

	  /* Put the common symbol in the .bss section, and increase
	     the size of the .bss section by the size of the common
	     symbol (which is the old value of the symbol).  */
	  sym->section = bss_sec;
	  size = sym->value;
	  sym->value = bss_sec->size + add;
	  add += size;
	  add = BFD_ALIGN (add, 1 << bss_sec->alignment_power);
	}
      if (add != 0)
	{
	  if (nlm_no_uninitialized_data (abfd))
	    {
	      /* We could handle this case, but so far it hasn't been
		 necessary.  */
	      abort ();
	    }
	  nlm_fixed_header (abfd)->uninitializedDataSize += add;
	  bss_sec->size += add;
	}
    }

  return TRUE;
}

/* Set the contents of a section.  To do this we need to know where
   the section is going to be located in the output file.  That means
   that the sizes of all the sections must be set, and all the
   variable size header information must be known.  */

bfd_boolean
nlm_set_section_contents (bfd *abfd,
			  asection *section,
			  const void * location,
			  file_ptr offset,
			  bfd_size_type count)
{
  if (! abfd->output_has_begun
      && ! nlm_compute_section_file_positions (abfd))
    return FALSE;

  if (count == 0)
    return TRUE;

  /* i386 NetWare has a very restricted set of relocs.  In order for
     objcopy to work, the NLM i386 backend needs a chance to rework
     the section contents so that its set of relocs will work.  If all
     the relocs are already acceptable, this will not do anything.  */
  if (section->reloc_count != 0)
    {
      bfd_boolean (*mangle_relocs_func)
	(bfd *, asection *, const void *, bfd_vma, bfd_size_type);

      mangle_relocs_func = nlm_mangle_relocs_func (abfd);
      if (mangle_relocs_func != NULL)
	{
	  if (!(*mangle_relocs_func) (abfd, section, location,
				      (bfd_vma) offset, count))
	    return FALSE;
	}
    }

  if (bfd_seek (abfd, section->filepos + offset, SEEK_SET) != 0
      || bfd_bwrite (location, count, abfd) != count)
    return FALSE;

  return TRUE;
}

/* We need to sort a list of relocs associated with sections when we
   write out the external relocs.  */

static int
nlm_external_reloc_compare (const void *p1, const void *p2)
{
  const struct reloc_and_sec *r1 = (const struct reloc_and_sec *) p1;
  const struct reloc_and_sec *r2 = (const struct reloc_and_sec *) p2;
  int cmp;

  cmp = strcmp ((*r1->rel->sym_ptr_ptr)->name,
		(*r2->rel->sym_ptr_ptr)->name);
  if (cmp != 0)
    return cmp;

  /* We sort by address within symbol to make the sort more stable and
     increase the chances that different hosts will generate bit for
     bit equivalent results.  */
  return (int) (r1->rel->address - r2->rel->address);
}

/* Write out an NLM file.  We write out the information in this order:
     fixed header
     variable header
     auxiliary headers
     code sections
     data sections
     other sections (custom data, messages, help, shared NLM, RPC,
     		     module dependencies)
     relocation fixups
     external references (imports)
     public symbols (exports)
     debugging records
   This is similar to the order used by the NetWare tools; the
   difference is that NetWare puts the sections other than code, data
   and custom data at the end of the NLM.  It is convenient for us to
   know where the sections are going to be before worrying about the
   size of the other information.

   By the time this function is called, all the section data should
   have been output using set_section_contents.  Note that custom
   data, the message file, the help file, the shared NLM file, the RPC
   data, and the module dependencies are all considered to be
   sections; the caller is responsible for filling in the offset and
   length fields in the NLM headers.  The relocation fixups and
   imports are both obtained from the list of relocs attached to each
   section.  The exports and debugging records are obtained from the
   list of outsymbols.  */

bfd_boolean
nlm_write_object_contents (bfd *abfd)
{
  asection *sec;
  bfd_boolean (*write_import_func) (bfd *, asection *, arelent *);
  bfd_size_type external_reloc_count, internal_reloc_count, i, c;
  struct reloc_and_sec *external_relocs;
  asymbol **sym_ptr_ptr;
  file_ptr last;
  bfd_boolean (*write_prefix_func) (bfd *);
  unsigned char *fixed_header = NULL;
  file_ptr pos;
  bfd_size_type amt;

  fixed_header = bfd_malloc (nlm_fixed_header_size (abfd));
  if (fixed_header == NULL)
    goto error_return;

  if (! abfd->output_has_begun
      && ! nlm_compute_section_file_positions (abfd))
    goto error_return;

  /* Write out the variable length headers.  */
  pos = nlm_optional_prefix_size (abfd) + nlm_fixed_header_size (abfd);
  if (bfd_seek (abfd, pos, SEEK_SET) != 0)
    goto error_return;
  if (! nlm_swap_variable_header_out (abfd)
      || ! nlm_swap_auxiliary_headers_out (abfd))
    {
      bfd_set_error (bfd_error_system_call);
      goto error_return;
    }

  /* A weak check on whether the section file positions were
     reasonable.  */
  if (bfd_tell (abfd) > nlm_fixed_header (abfd)->codeImageOffset)
    {
      bfd_set_error (bfd_error_invalid_operation);
      goto error_return;
    }

  /* Advance to the relocs.  */
  if (bfd_seek (abfd, nlm_fixed_header (abfd)->relocationFixupOffset,
		SEEK_SET) != 0)
    goto error_return;

  /* The format of the relocation entries is dependent upon the
     particular target.  We use an external routine to write the reloc
     out.  */
  write_import_func = nlm_write_import_func (abfd);

  /* Write out the internal relocation fixups.  While we're looping
     over the relocs, we also count the external relocs, which is
     needed when they are written out below.  */
  internal_reloc_count = 0;
  external_reloc_count = 0;
  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      arelent **rel_ptr_ptr, **rel_end;

      if (sec->reloc_count == 0)
	continue;

      /* We can only represent relocs within a code or data
	 section.  We ignore them for a debugging section.  */
      if ((bfd_get_section_flags (abfd, sec) & (SEC_CODE | SEC_DATA)) == 0)
	continue;

      /* We need to know how to write out imports */
      if (write_import_func == NULL)
	{
	  bfd_set_error (bfd_error_invalid_operation);
	  goto error_return;
	}

      rel_ptr_ptr = sec->orelocation;
      rel_end = rel_ptr_ptr + sec->reloc_count;
      for (; rel_ptr_ptr < rel_end; rel_ptr_ptr++)
	{
	  arelent *rel;
	  asymbol *sym;

	  rel = *rel_ptr_ptr;
	  sym = *rel->sym_ptr_ptr;

	  if (! bfd_is_und_section (bfd_get_section (sym)))
	    {
	      ++internal_reloc_count;
	      if (! (*write_import_func) (abfd, sec, rel))
		goto error_return;
	    }
	  else
	    ++external_reloc_count;
	}
    }
  nlm_fixed_header (abfd)->numberOfRelocationFixups = internal_reloc_count;

  /* Write out the imports (relocs against external symbols).  These
     are output as a symbol name followed by all the relocs for that
     symbol, so we must first gather together all the relocs against
     external symbols and sort them.  */
  amt = external_reloc_count * sizeof (struct reloc_and_sec);
  external_relocs = bfd_alloc (abfd, amt);
  if (external_relocs == NULL)
    goto error_return;
  i = 0;
  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      arelent **rel_ptr_ptr, **rel_end;

      if (sec->reloc_count == 0)
	continue;

      rel_ptr_ptr = sec->orelocation;
      rel_end = rel_ptr_ptr + sec->reloc_count;
      for (; rel_ptr_ptr < rel_end; rel_ptr_ptr++)
	{
	  arelent *rel;
	  asymbol *sym;

	  rel = *rel_ptr_ptr;
	  sym = *rel->sym_ptr_ptr;

	  if (! bfd_is_und_section (bfd_get_section (sym)))
	    continue;

	  external_relocs[i].rel = rel;
	  external_relocs[i].sec = sec;
	  ++i;
	}
    }

  BFD_ASSERT (i == external_reloc_count);

  /* Sort the external relocs by name.  */
  qsort (external_relocs, (size_t) external_reloc_count,
	 sizeof (struct reloc_and_sec), nlm_external_reloc_compare);

  /* Write out the external relocs.  */
  nlm_fixed_header (abfd)->externalReferencesOffset = bfd_tell (abfd);
  c = 0;
  i = 0;
  while (i < external_reloc_count)
    {
      arelent *rel;
      asymbol *sym;
      bfd_size_type j, cnt;

      ++c;

      rel = external_relocs[i].rel;
      sym = *rel->sym_ptr_ptr;

      cnt = 0;
      for (j = i;
	   (j < external_reloc_count
	    && *external_relocs[j].rel->sym_ptr_ptr == sym);
	   j++)
	++cnt;

      if (! (*nlm_write_external_func (abfd)) (abfd, cnt, sym,
					       &external_relocs[i]))
	goto error_return;

      i += cnt;
    }

  nlm_fixed_header (abfd)->numberOfExternalReferences = c;

  /* Write out the public symbols (exports).  */
  sym_ptr_ptr = bfd_get_outsymbols (abfd);
  if (sym_ptr_ptr != NULL)
    {
      bfd_vma (*get_public_offset_func) (bfd *, asymbol *);
      bfd_boolean (*write_export_func) (bfd *, asymbol *, bfd_vma);

      asymbol **sym_end;

      nlm_fixed_header (abfd)->publicsOffset = bfd_tell (abfd);
      get_public_offset_func = nlm_get_public_offset_func (abfd);
      write_export_func = nlm_write_export_func (abfd);
      c = 0;
      sym_end = sym_ptr_ptr + bfd_get_symcount (abfd);
      for (; sym_ptr_ptr < sym_end; sym_ptr_ptr++)
	{
	  asymbol *sym;
	  bfd_byte len;
	  bfd_vma offset;
	  bfd_byte temp[NLM_TARGET_LONG_SIZE];

	  sym = *sym_ptr_ptr;

	  if ((sym->flags & (BSF_EXPORT | BSF_GLOBAL)) == 0
	      || bfd_is_und_section (bfd_get_section (sym)))
	    continue;

	  ++c;

	  if (get_public_offset_func)
	    {
	      /* Most backends can use the code below, but
		 unfortunately some use a different scheme.  */
	      offset = (*get_public_offset_func) (abfd, sym);
	    }
	  else
	    {
	      offset = bfd_asymbol_value (sym);
	      sec = sym->section;
	      if (sec->flags & SEC_CODE)
		{
		  offset -= nlm_get_text_low (abfd);
		  offset |= NLM_HIBIT;
		}
	      else if (sec->flags & (SEC_DATA | SEC_ALLOC))
		{
		  /* SEC_ALLOC is for the .bss section.  */
		  offset -= nlm_get_data_low (abfd);
		}
	      else
		{
		  /* We can't handle an exported symbol that is not in
		     the code or data segment.  */
		  bfd_set_error (bfd_error_invalid_operation);
		  goto error_return;
		}
	    }

	  if (write_export_func)
	    {
	      if (! (*write_export_func) (abfd, sym, offset))
		goto error_return;
	    }
	  else
	    {
	      len = strlen (sym->name);
	      if ((bfd_bwrite (&len, (bfd_size_type) sizeof (bfd_byte), abfd)
		   != sizeof (bfd_byte))
		  || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
		goto error_return;

	      put_word (abfd, offset, temp);
	      if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd)
		  != sizeof (temp))
		goto error_return;
	    }
	}
      nlm_fixed_header (abfd)->numberOfPublics = c;

      /* Write out the debugging records.  The NLM conversion program
	 wants to be able to inhibit this, so as a special hack if
	 debugInfoOffset is set to -1 we don't write any debugging
	 information.  This can not be handled by fiddling with the
	 symbol table, because exported symbols appear in both the
	 exported symbol list and the debugging information.  */
      if (nlm_fixed_header (abfd)->debugInfoOffset == (file_ptr) - 1)
	{
	  nlm_fixed_header (abfd)->debugInfoOffset = 0;
	  nlm_fixed_header (abfd)->numberOfDebugRecords = 0;
	}
      else
	{
	  nlm_fixed_header (abfd)->debugInfoOffset = bfd_tell (abfd);
	  c = 0;
	  sym_ptr_ptr = bfd_get_outsymbols (abfd);
	  sym_end = sym_ptr_ptr + bfd_get_symcount (abfd);
	  for (; sym_ptr_ptr < sym_end; sym_ptr_ptr++)
	    {
	      asymbol *sym;
	      bfd_byte type, len;
	      bfd_vma offset;
	      bfd_byte temp[NLM_TARGET_LONG_SIZE];

	      sym = *sym_ptr_ptr;

	      /* The NLM notion of a debugging symbol is actually what
		 BFD calls a local or global symbol.  What BFD calls a
		 debugging symbol NLM does not understand at all.  */
	      if ((sym->flags & (BSF_LOCAL | BSF_GLOBAL | BSF_EXPORT)) == 0
		  || (sym->flags & BSF_DEBUGGING) != 0
		  || bfd_is_und_section (bfd_get_section (sym)))
		continue;

	      ++c;

	      offset = bfd_asymbol_value (sym);
	      sec = sym->section;
	      if (sec->flags & SEC_CODE)
		{
		  offset -= nlm_get_text_low (abfd);
		  type = 1;
		}
	      else if (sec->flags & (SEC_DATA | SEC_ALLOC))
		{
		  /* SEC_ALLOC is for the .bss section.  */
		  offset -= nlm_get_data_low (abfd);
		  type = 0;
		}
	      else
		type = 2;

	      /* The type is 0 for data, 1 for code, 2 for absolute.  */
	      if (bfd_bwrite (&type, (bfd_size_type) sizeof (bfd_byte), abfd)
		  != sizeof (bfd_byte))
		goto error_return;

	      put_word (abfd, offset, temp);
	      if (bfd_bwrite (temp, (bfd_size_type) sizeof (temp), abfd)
		  != sizeof (temp))
		goto error_return;

	      len = strlen (sym->name);
	      if ((bfd_bwrite (&len, (bfd_size_type) sizeof (bfd_byte), abfd)
		   != sizeof (bfd_byte))
		  || bfd_bwrite (sym->name, (bfd_size_type) len, abfd) != len)
		goto error_return;
	    }
	  nlm_fixed_header (abfd)->numberOfDebugRecords = c;
	}
    }

  /* NLMLINK fills in offset values even if there is no data, so we do
     the same.  */
  last = bfd_tell (abfd);
  if (nlm_fixed_header (abfd)->codeImageOffset == 0)
    nlm_fixed_header (abfd)->codeImageOffset = last;
  if (nlm_fixed_header (abfd)->dataImageOffset == 0)
    nlm_fixed_header (abfd)->dataImageOffset = last;
  if (nlm_fixed_header (abfd)->customDataOffset == 0)
    nlm_fixed_header (abfd)->customDataOffset = last;
  if (nlm_fixed_header (abfd)->moduleDependencyOffset == 0)
    nlm_fixed_header (abfd)->moduleDependencyOffset = last;
  if (nlm_fixed_header (abfd)->relocationFixupOffset == 0)
    nlm_fixed_header (abfd)->relocationFixupOffset = last;
  if (nlm_fixed_header (abfd)->externalReferencesOffset == 0)
    nlm_fixed_header (abfd)->externalReferencesOffset = last;
  if (nlm_fixed_header (abfd)->publicsOffset == 0)
    nlm_fixed_header (abfd)->publicsOffset = last;
  if (nlm_fixed_header (abfd)->debugInfoOffset == 0)
    nlm_fixed_header (abfd)->debugInfoOffset = last;

  /* At this point everything has been written out except the fixed
     header.  */
  memcpy (nlm_fixed_header (abfd)->signature, nlm_signature (abfd),
	  NLM_SIGNATURE_SIZE);
  nlm_fixed_header (abfd)->version = NLM_HEADER_VERSION;
  nlm_fixed_header (abfd)->codeStartOffset =
    (bfd_get_start_address (abfd)
     - nlm_get_text_low (abfd));

  /* We have no convenient way for the caller to pass in the exit
     procedure or the check unload procedure, so the caller must set
     the values in the header to the values of the symbols.  */
  nlm_fixed_header (abfd)->exitProcedureOffset -= nlm_get_text_low (abfd);
  if (nlm_fixed_header (abfd)->checkUnloadProcedureOffset != 0)
    nlm_fixed_header (abfd)->checkUnloadProcedureOffset -=
      nlm_get_text_low (abfd);

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto error_return;

  write_prefix_func = nlm_write_prefix_func (abfd);
  if (write_prefix_func)
    {
      if (! (*write_prefix_func) (abfd))
	goto error_return;
    }

  BFD_ASSERT ((bfd_size_type) bfd_tell (abfd)
	      == nlm_optional_prefix_size (abfd));

  nlm_swap_fixed_header_out (abfd, nlm_fixed_header (abfd), fixed_header);
  if (bfd_bwrite (fixed_header, nlm_fixed_header_size (abfd), abfd)
      != nlm_fixed_header_size (abfd))
    goto error_return;

  if (fixed_header != NULL)
    free (fixed_header);
  return TRUE;

error_return:
  if (fixed_header != NULL)
    free (fixed_header);
  return FALSE;
}
