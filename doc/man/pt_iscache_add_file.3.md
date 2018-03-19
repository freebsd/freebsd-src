% PT_ISCACHE_ADD_FILE(3)

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

pt_iscache_add_file - add file sections to a traced memory image section cache


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_iscache_add_file(struct pt_image_section_cache \**iscache*,**
|                         **const char \**filename*, uint64_t *offset*,**
|                         **uint64_t *size*, uint64_t *vaddr*);**

Link with *-lipt*.


# DESCRIPTION

**pt_iscache_add_file**() adds a new section consisting of *size* bytes starting
at *offset* in *filename* loaded at *vaddr* to *iscache*.

On success, **pt_iscache_add_file**() returns a positive integer identifier that
uniquely identifies the added section in that cache.  This identifier can be
used to add sections from an image section cache to one or more traced memory
images.  See **pt_image_add_cached**(3).  Sections added from an image section
cache will be shared across images.  It can also be used to read memory from the
cached section.  See **pt_iscache_read**(3).

If the cache already contains a suitable section, no section is added and the
identifier for the existing section is returned.  If the cache already contains
a section that only differs in the load address, a new section is added that
shares the underlying file section.


# RETURN VALUE

**pt_iscache_add_file**() returns a positive image section identifier on success
or a negative *pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *iscache* or *filename* argument is NULL or the *offset* argument is too
    big such that the section would start past the end of the file.


# EXAMPLE

~~~{.c}
int add_file(struct pt_image_section_cache *iscache, struct pt_image *image,
             const char *filename, uint64_t offset, uint64_t size,
             uint64_t vaddr, const struct pt_asid *asid) {
    int isid;

    isid = pt_iscache_add_file(iscache, filename, offset, size, vaddr);
    if (isid < 0)
       return isid;

    return pt_image_add_cached(image, iscache, isid, asid);
}
~~~


# SEE ALSO

**pt_iscache_alloc**(3), **pt_iscache_free**(3), **pt_iscache_read**(3),
**pt_image_add_cached**(3)
