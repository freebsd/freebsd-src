/* NLM (NetWare Loadable Module) swapping routines for BFD.
   Copyright 1993, 2000, 2001 Free Software Foundation, Inc.

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
   file header in internal format.  */

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
    H_GET_32 (abfd, src->version);
  dst->codeImageOffset =
    H_GET_32 (abfd, src->codeImageOffset);
  dst->codeImageSize =
    H_GET_32 (abfd, src->codeImageSize);
  dst->dataImageOffset =
    H_GET_32 (abfd, src->dataImageOffset);
  dst->dataImageSize =
    H_GET_32 (abfd, src->dataImageSize);
  dst->uninitializedDataSize =
    H_GET_32 (abfd, src->uninitializedDataSize);
  dst->customDataOffset =
    H_GET_32 (abfd, src->customDataOffset);
  dst->customDataSize =
    H_GET_32 (abfd, src->customDataSize);
  dst->moduleDependencyOffset =
    H_GET_32 (abfd, src->moduleDependencyOffset);
  dst->numberOfModuleDependencies =
    H_GET_32 (abfd, src->numberOfModuleDependencies);
  dst->relocationFixupOffset =
    H_GET_32 (abfd, src->relocationFixupOffset);
  dst->numberOfRelocationFixups =
    H_GET_32 (abfd, src->numberOfRelocationFixups);
  dst->externalReferencesOffset =
    H_GET_32 (abfd, src->externalReferencesOffset);
  dst->numberOfExternalReferences =
    H_GET_32 (abfd, src->numberOfExternalReferences);
  dst->publicsOffset =
    H_GET_32 (abfd, src->publicsOffset);
  dst->numberOfPublics =
    H_GET_32 (abfd, src->numberOfPublics);
  dst->debugInfoOffset =
    H_GET_32 (abfd, src->debugInfoOffset);
  dst->numberOfDebugRecords =
    H_GET_32 (abfd, src->numberOfDebugRecords);
  dst->codeStartOffset =
    H_GET_32 (abfd, src->codeStartOffset);
  dst->exitProcedureOffset =
    H_GET_32 (abfd, src->exitProcedureOffset);
  dst->checkUnloadProcedureOffset =
    H_GET_32 (abfd, src->checkUnloadProcedureOffset);
  dst->moduleType =
    H_GET_32 (abfd, src->moduleType);
  dst->flags =
    H_GET_32 (abfd, src->flags);
}

/* Translate an NLM fixed length file header in internal format into
   an NLM file header in external format.  */

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
  H_PUT_32 (abfd, src->version,
	    dst->version);
  H_PUT_32 (abfd, src->codeImageOffset,
	    dst->codeImageOffset);
  H_PUT_32 (abfd, src->codeImageSize,
	    dst->codeImageSize);
  H_PUT_32 (abfd, src->dataImageOffset,
	    dst->dataImageOffset);
  H_PUT_32 (abfd, src->dataImageSize,
	    dst->dataImageSize);
  H_PUT_32 (abfd, src->uninitializedDataSize,
	    dst->uninitializedDataSize);
  H_PUT_32 (abfd, src->customDataOffset,
	    dst->customDataOffset);
  H_PUT_32 (abfd, src->customDataSize,
	    dst->customDataSize);
  H_PUT_32 (abfd, src->moduleDependencyOffset,
	    dst->moduleDependencyOffset);
  H_PUT_32 (abfd, src->numberOfModuleDependencies,
	    dst->numberOfModuleDependencies);
  H_PUT_32 (abfd, src->relocationFixupOffset,
	    dst->relocationFixupOffset);
  H_PUT_32 (abfd, src->numberOfRelocationFixups,
	    dst->numberOfRelocationFixups);
  H_PUT_32 (abfd, src->externalReferencesOffset,
	    dst->externalReferencesOffset);
  H_PUT_32 (abfd, src->numberOfExternalReferences,
	    dst->numberOfExternalReferences);
  H_PUT_32 (abfd, src->publicsOffset,
	    dst->publicsOffset);
  H_PUT_32 (abfd, src->numberOfPublics,
	    dst->numberOfPublics);
  H_PUT_32 (abfd, src->debugInfoOffset,
	    dst->debugInfoOffset);
  H_PUT_32 (abfd, src->numberOfDebugRecords,
	    dst->numberOfDebugRecords);
  H_PUT_32 (abfd, src->codeStartOffset,
	    dst->codeStartOffset);
  H_PUT_32 (abfd, src->exitProcedureOffset,
	    dst->exitProcedureOffset);
  H_PUT_32 (abfd, src->checkUnloadProcedureOffset,
	    dst->checkUnloadProcedureOffset);
  H_PUT_32 (abfd, src->moduleType,
	    dst->moduleType);
  H_PUT_32 (abfd, src->flags,
	    dst->flags);
}
