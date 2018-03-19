% PT_IMAGE_ADD_FILE(3)

<!---
 ! Copyright (c) 2015-2018, Intel Corporation
 !
 ! Redistribution and use in source and binary forms, with or without
 ! modification, are permitted provided that the following conditions are met:
 !
 !  * Redistributions of source code must retain the above copyright notice,
 !    this list of conditions and the following disclaimer.
 !  * Redistributions in binary form must reproduce the above copyright notice,
 !    this list of conditions and the following disclaimer in the documentation
 !    and/or other materials provided with the distribution.
 !  * Neither the name of Intel Corporation nor the names of its contributors
 !    may be used to endorse or promote products derived from this software
 !    without specific prior written permission.
 !
 ! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 ! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 ! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 ! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 ! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 ! POSSIBILITY OF SUCH DAMAGE.
 !-->

# NAME

pt_image_add_file, pt_image_add_cached, pt_image_copy - add file sections to a
traced memory image descriptor


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_image_add_file(struct pt_image \**image*, const char \**filename*,**
|                       **uint64_t *offset*, uint64_t *size*,**
|                       **const struct pt_asid \**asid*, uint64_t *vaddr*);**
| **int pt_image_add_cached(struct pt_image \**image*,**
|                         **struct pt_image_section_cache \**iscache*,**
|                         **int *isid*, const struct pt_asid \**asid*);**
| **int pt_image_copy(struct pt_image \**image*,**
|                   **const struct pt_image \**src*);**

Link with *-lipt*.


# DESCRIPTION

**pt_image_add_file**() adds a new section to a *pt_image* object.  The *image*
argument points to the *pt_image* object to which the new section is added.  The
*filename* argument gives the absolute or relative path to the file on disk that
contains the section.  The *offset* and *size* arguments define the section
within the file.  The *size* argument is silently truncated to end the section
with the end of the underlying file.  The *vaddr* argument gives the virtual
address at which the section is being loaded.

**pt_image_add_cached**() adds a new section from an image section cache.  See
**pt_iscache_add_file**(3).  The *iscache* argument points to the
*pt_image_section_cache* object containing the section.  The *isid* argument
gives the image section identifier for the desired section in that cache.

The *asid* argument gives an optional address space identifier.  If it is not
NULL, it points to a *pt_asid* structure, which is declared as:

~~~{.c}
/** An Intel PT address space identifier.
 *
 * This identifies a particular address space when adding file
 * sections or when reading memory.
 */
struct pt_asid {
	/** The size of this object - set to sizeof(struct pt_asid).
	 */
	size_t size;

	/** The CR3 value. */
	uint64_t cr3;

	/** The VMCS Base address. */
	uint64_t vmcs;
};
~~~

The *asid* argument can be used to prepare a collection of process, guest, and
hypervisor images to an Intel(R) Processor Trace (Intel PT) instruction flow
decoder.  The decoder will select the current image based on CR3 and VMCS
information in the Intel PT trace.

If only the CR3 or only the VMCS field should be considered by the decoder,
supply *pt_asid_no_cr3* and *pt_asid_no_vmcs* to the other field respectively.

If the *asid* argument is NULL, the file section will be added for all
processes, guests, and hypervisor images.

If the new section overlaps with an existing section, the existing section is
truncated or split to make room for the new section.

**pt_image_copy**() adds file sections from the *pt_image* pointed to by the
*src* argument to the *pt_image* pointed to by the *dst* argument.


# RETURN VALUE

**pt_image_add_file**() and **pt_image_add_cached**() return zero on success or
a negative *pt_error_code* enumeration constant in case of an error.

**pt_image_copy**() returns the number of ignored sections on success or a
negative *pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *image* or *filename* argument is NULL or the *offset* argument is too
    big such that the section would start past the end of the file
    (**pt_image_add_file**()).
    The *image* or *iscache* argument is NULL (**pt_image_add_cached**()).
    The *src* or *dst* argument is NULL (**pt_image_copy**()).

pte_bad_image
:   The *iscache* does not contain *isid* (**pt_image_add_cached**()).


# SEE ALSO

**pt_image_alloc**(3), **pt_image_free**(3),
**pt_image_remove_by_filename**(3), **pt_image_remove_by_asid**(3),
**pt_image_set_callback**(3), **pt_insn_set_image**(3),
**pt_insn_get_image**(3), **pt_iscache_alloc**(3), **pt_iscache_add_file**(3)
