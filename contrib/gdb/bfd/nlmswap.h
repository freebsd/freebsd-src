/* NLM (NetWare Loadable Module) swapping routines for BFD.
   Copyright (C) 1993 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Although this is a header file, it defines functions.  It is
   included by NLM backends to define swapping functions that vary
   from one NLM to another.  The backend code must arrange for
   Nlm_External_xxxx to be defined appropriately, and can then include
   this file to get the swapping routines.

   At the moment this is only needed for one structure, the fixed NLM
   file header.  */

static void nlm_swap_fixed_header_in PARAMS ((bfd *, PTR,
					      Nlm_Internal_Fixed_Header *));
static void nlm_swap_fixed_header_out PARAMS ((bfd *,
					       Nlm_Internal_Fixed_Header *,
					       PTR));

/* Translate an NLM fixed length file header in external format into an NLM
   file header in internal format. */

static void
nlm_swap_fixed_header_in (abfd, realsrc, dst)
     bfd *abfd;
     PTR realsrc;
     Nlm_Internal_Fixed_Header *dst;
{
  Nlm_External_Fixed_Header *src = (Nlm_External_Fixed_Header *) realsrc;
  memcpy (dst->signature, src->signature, NLM_SIGNATURE_SIZE);
  memcpy (dst->moduleName, src->moduleName, NLM_MODULE_NAME_SIZE);
  dst->version =
    bfd_h_get_32 (abfd, (bfd_byte *) src->version);
  dst->codeImageOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->codeImageOffset);
  dst->codeImageSize =
    bfd_h_get_32 (abfd, (bfd_byte *) src->codeImageSize);
  dst->dataImageOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->dataImageOffset);
  dst->dataImageSize =
    bfd_h_get_32 (abfd, (bfd_byte *) src->dataImageSize);
  dst->uninitializedDataSize =
    bfd_h_get_32 (abfd, (bfd_byte *) src->uninitializedDataSize);
  dst->customDataOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->customDataOffset);
  dst->customDataSize =
    bfd_h_get_32 (abfd, (bfd_byte *) src->customDataSize);
  dst->moduleDependencyOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->moduleDependencyOffset);
  dst->numberOfModuleDependencies =
    bfd_h_get_32 (abfd, (bfd_byte *) src->numberOfModuleDependencies);
  dst->relocationFixupOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->relocationFixupOffset);
  dst->numberOfRelocationFixups =
    bfd_h_get_32 (abfd, (bfd_byte *) src->numberOfRelocationFixups);
  dst->externalReferencesOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->externalReferencesOffset);
  dst->numberOfExternalReferences =
    bfd_h_get_32 (abfd, (bfd_byte *) src->numberOfExternalReferences);
  dst->publicsOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->publicsOffset);
  dst->numberOfPublics =
    bfd_h_get_32 (abfd, (bfd_byte *) src->numberOfPublics);
  dst->debugInfoOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->debugInfoOffset);
  dst->numberOfDebugRecords =
    bfd_h_get_32 (abfd, (bfd_byte *) src->numberOfDebugRecords);
  dst->codeStartOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->codeStartOffset);
  dst->exitProcedureOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->exitProcedureOffset);
  dst->checkUnloadProcedureOffset =
    bfd_h_get_32 (abfd, (bfd_byte *) src->checkUnloadProcedureOffset);
  dst->moduleType =
    bfd_h_get_32 (abfd, (bfd_byte *) src->moduleType);
  dst->flags =
    bfd_h_get_32 (abfd, (bfd_byte *) src->flags);
}

/* Translate an NLM fixed length file header in internal format into
   an NLM file header in external format. */

static void
nlm_swap_fixed_header_out (abfd, src, realdst)
     bfd *abfd;
     Nlm_Internal_Fixed_Header *src;
     PTR realdst;
{
  Nlm_External_Fixed_Header *dst = (Nlm_External_Fixed_Header *) realdst;
  memset (dst, 0, sizeof *dst);
  memcpy (dst->signature, src->signature, NLM_SIGNATURE_SIZE);
  memcpy (dst->moduleName, src->moduleName, NLM_MODULE_NAME_SIZE);
  bfd_h_put_32 (abfd, (bfd_vma) src->version,
		(bfd_byte *) dst->version);
  bfd_h_put_32 (abfd, (bfd_vma) src->codeImageOffset,
		(bfd_byte *) dst->codeImageOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->codeImageSize,
		(bfd_byte *) dst->codeImageSize);
  bfd_h_put_32 (abfd, (bfd_vma) src->dataImageOffset,
		(bfd_byte *) dst->dataImageOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->dataImageSize,
		(bfd_byte *) dst->dataImageSize);
  bfd_h_put_32 (abfd, (bfd_vma) src->uninitializedDataSize,
		(bfd_byte *) dst->uninitializedDataSize);
  bfd_h_put_32 (abfd, (bfd_vma) src->customDataOffset,
		(bfd_byte *) dst->customDataOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->customDataSize,
		(bfd_byte *) dst->customDataSize);
  bfd_h_put_32 (abfd, (bfd_vma) src->moduleDependencyOffset,
		(bfd_byte *) dst->moduleDependencyOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->numberOfModuleDependencies,
		(bfd_byte *) dst->numberOfModuleDependencies);
  bfd_h_put_32 (abfd, (bfd_vma) src->relocationFixupOffset,
		(bfd_byte *) dst->relocationFixupOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->numberOfRelocationFixups,
		(bfd_byte *) dst->numberOfRelocationFixups);
  bfd_h_put_32 (abfd, (bfd_vma) src->externalReferencesOffset,
		(bfd_byte *) dst->externalReferencesOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->numberOfExternalReferences,
		(bfd_byte *) dst->numberOfExternalReferences);
  bfd_h_put_32 (abfd, (bfd_vma) src->publicsOffset,
		(bfd_byte *) dst->publicsOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->numberOfPublics,
		(bfd_byte *) dst->numberOfPublics);
  bfd_h_put_32 (abfd, (bfd_vma) src->debugInfoOffset,
		(bfd_byte *) dst->debugInfoOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->numberOfDebugRecords,
		(bfd_byte *) dst->numberOfDebugRecords);
  bfd_h_put_32 (abfd, (bfd_vma) src->codeStartOffset,
		(bfd_byte *) dst->codeStartOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->exitProcedureOffset,
		(bfd_byte *) dst->exitProcedureOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->checkUnloadProcedureOffset,
		(bfd_byte *) dst->checkUnloadProcedureOffset);
  bfd_h_put_32 (abfd, (bfd_vma) src->moduleType,
		(bfd_byte *) dst->moduleType);
  bfd_h_put_32 (abfd, (bfd_vma) src->flags,
		(bfd_byte *) dst->flags);
}
