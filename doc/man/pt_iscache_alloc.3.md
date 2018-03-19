% PT_ISCACHE_ALLOC(3)

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

pt_iscache_alloc, pt_iscache_free, pt_iscache_name - allocate/free a traced memory
image section cache


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_image_section_cache \*pt_iscache_alloc(const char \**name*);**
| **const char \*pt_iscache_name(const struct pt_image_section_cache \**iscache*);**
| **void pt_iscache_free(struct pt_image_section_cache \**iscache*);**

Link with *-lipt*.


# DESCRIPTION

**pt_iscache_alloc**() allocates a new *pt_image_section_cache* and returns a
pointer to it.  A *pt_image_section_cache* object contains a collection of file
sections and the virtual addresses at which those sections were loaded.

The image sections can be added to one or more *pt_image* objects.  The
underlying file sections will be mapped once and their content will be shared
across images.

The *name* argument points to an optional zero-terminated name string.  If the
*name* argument is NULL, it will be ignored and the returned
*pt_image_section_cache* object will not have a name.  Otherwise, the returned
*pt_image_section_object* object will have a copy of the string pointed to by
the *name* argument as name.

**pt_iscache_name**() returns the name of the *pt_image_section_cache* object
the *iscache* argument points to.

**pt_iscache_free**() frees the *pt_image_section_cache* object pointed to by
*iscache*.  The *iscache* argument must be NULL or point to an image section
cache that has been allocated by a call to **pt_iscache_alloc**().


# RETURN VALUE

**pt_iscache_alloc**() returns a pointer to a *pt_image_section_cache* object
on success or NULL in case of an error.

**pt_iscache_name**() returns a pointer to a zero-terminated string of NULL if the
image section cache does not have a name.


# EXAMPLE

~~~{.c}
int foo(const char *name) {
    struct pt_image_section_cache *iscache;
    errcode;

    image = pt_iscache_alloc(name);
    if (!iscache)
        return pte_nomem;

    errcode = bar(iscache);

    pt_iscache_free(iscache);
    return errcode;
}
~~~


# SEE ALSO

**pt_iscache_add_file**(3), **pt_image_add_cached**(3)
