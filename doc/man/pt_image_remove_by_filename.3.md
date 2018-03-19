% PT_IMAGE_REMOVE_BY_FILENAME(3)

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

pt_image_remove_by_filename, pt_image_remove_by_asid - remove sections from a
traced memory image descriptor


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_image_remove_by_filename(struct pt_image \**image*,**
|                                 **const char \**filename*,**
|                                 **const struct pt_asid \**asid*);**
| **int pt_image_remove_by_asid(struct pt_image \**image*,**
|                             **const struct pt_asid \**asid*);**

Link with *-lipt*.


# DESCRIPTION

**pt_image_remove_by_filename**() removes all sections from *image* that were
added by a call to **pt_image_add_file**(3) with an identical *filename*
argument or by a call to **pt_image_copy**(3) from such a section.  Sections
that are based on the same underlying file but that were added using a different
*filename* argument are not removed.

If the *asid* argument is not NULL, it removes only sections that were added
with a matching address-space identifier.  See **pt_image_add_file**(3).

**pt_image_remove_by_asid**(3) removes all sections from *image* that were added
by a call to **pt_image_add_file**(3) with a matching *asid* argument or by a
call to **pt_image_copy**(3) from such a section.  See **pt_image_add_file**(3).

Two *pt_asid* objects match in their "cr3* or *vmcs* field if one of them does
not provide the field (i.e. sets it to *pt_asid_no_cr3* or *pt_asid_no_vmcs*
respectively) or if the provided values are identical.  Two *pt_asid* objects
match if they match in all fields.


# RETURN VALUE

Both functions return the number of sections removed on success or a negative
*pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *image* argument is NULL or the *filename* argument is NULL
    (**pt_image_remove_by_filename**() only).


# EXAMPLE

~~~{.c}
int foo(struct pt_image *image, uint64_t cr3) {
	struct pt_asid asid1, asid2;
	int errcode;

	pt_asid_init(&asid1);
	asid1.cr3 = cr3;

	pt_asid_init(&asid2);
	asid2.cr3 = ~cr3;

	errcode = pt_image_add_file(image, "/path/to/libfoo.so",
								0xa000, 0x100, &asid1, 0xb000);
	if (errcode < 0)
		return errcode;

	errcode = pt_image_add_file(image, "rel/path/to/libfoo.so",
								0xa000, 0x100, &asid1, 0xc000);
	if (errcode < 0)
		return errcode;

	/* This call would only remove the section added first:
	 *
	 * - filename matches only the first section's filename
	 * - NULL matches every asid
	 */
	(void) pt_image_remove_by_filename(image,
									   "/path/to/libfoo.so",
									   NULL);

	/* This call would not remove any of the above sections:
	 *
	 * - filename matches the first section's filename
	 * - asid2 does not match asid1
	 */
	(void) pt_image_remove_by_filename(image,
									   "/path/to/libfoo.so",
									   &asid2);

	/* This call would not remove any of the above sections:
	 *
	 * - asid2 does not match asid1
	 */
	(void) pt_image_remove_by_asid(image, &asid2);

	/* This call would remove both sections:
	 *
	 * - asid1 matches itself
	 */
	(void) pt_image_remove_by_asid(image, &asid1);

	/* This call would remove both sections:
	 *
	 * - NULL matches every asid
	 */
	(void) pt_image_remove_by_asid(image, NULL);
}
~~~


# SEE ALSO

**pt_image_alloc**(3), **pt_image_free**(3), **pt_image_add_file**(3),
**pt_image_add_cached**(3), **pt_image_copy**(3), **pt_insn_set_image**(3),
**pt_insn_get_image**(3)
