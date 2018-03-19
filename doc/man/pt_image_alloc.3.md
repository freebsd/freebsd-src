% PT_IMAGE_ALLOC(3)

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

pt_image_alloc, pt_image_free, pt_image_name - allocate/free a traced memory
image descriptor


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_image \*pt_image_alloc(const char \**name*);**
| **const char \*pt_image_name(const struct pt_image \**image*);**
| **void pt_image_free(struct pt_image \**image*);**

Link with *-lipt*.


# DESCRIPTION

**pt_image_alloc**() allocates a new *pt_image* and returns a pointer to it.  A
*pt_image* object defines the memory image that was traced as a collection of
file sections and the virtual addresses at which those sections were loaded.

The *name* argument points to an optional zero-terminated name string.  If the
*name* argument is NULL, it will be ignored and the returned *pt_image* object
will not have a name.  Otherwise, the returned *pt_image* object will have a
copy of the string pointed to by the *name* argument as name.

**pt_image_name**() returns the name of the *pt_image* object the *image*
argument points to.

**pt_image_free**() frees the *pt_image* object pointed to by *image*.  The
*image* argument must be NULL or point to an image that has been allocated by a
call to **pt_image_alloc**().


# RETURN VALUE

**pt_image_alloc**() returns a pointer to a *pt_image* object on success or NULL
in case of an error.

**pt_image_name**() returns a pointer to a zero-terminated string of NULL if the
image does not have a name.


# EXAMPLE

~~~{.c}
int foo(const char *name) {
	struct pt_image *image;
	errcode;

	image = pt_image_alloc(name);
	if (!image)
		return pte_nomem;

	errcode = bar(image);

	pt_image_free(image);
	return errcode;
}
~~~


# SEE ALSO

**pt_image_add_file**(3), **pt_image_add_cached**(3), **pt_image_copy**(3),
**pt_image_remove_by_filename**(3), **pt_image_remove_by_asid**(3),
**pt_image_set_callback**(3), **pt_insn_set_image**(3), **pt_insn_get_image**(3)
