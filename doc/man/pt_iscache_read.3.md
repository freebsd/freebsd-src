% PT_ISCACHE_READ(3)

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

pt_iscache_read - read memory from a cached file section


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_iscache_read(struct pt_image_section_cache \**iscache*,**
|                     **uint8_t \**buffer*, uint64_t *size*, int *isid*,**
|                     **uint64_t *vaddr*);**

Link with *-lipt*.


# DESCRIPTION

**pt_iscache_read**() reads memory from a cached file section.  The file section
must have previously been added by a call to **pt_iscache_add**(3).  The
*iscache* argument points to the *pt_image_section_cache* object.  It must be
the same that was used in the corresponding **pt_iscache_add**(3) call.  The
*buffer* argument must point to a memory buffer of at least *size* bytes.  The
*isid* argument identifies the file section from which memory is read.  It must
be the same identifier that was returned from the corresponding
**pt_iscache_add**(3) call that added the file section to the cache.  The *vaddr*
argument gives the virtual address from which *size* bytes of memory shall be
read.

On success, **pt_iscache_read**() copies at most *size* bytes of memory from the
cached file section identified by *isid* in *iscache* starting at virtual
address *vaddr* into *buffer* and returns the number of bytes that were copied.

Multiple calls to **pt_iscache_read**() may be necessary if *size* is bigger
than 4Kbyte or if the read crosses a section boundary.


# RETURN VALUE

**pt_iscache_read**() returns the number of bytes that were read on success
or a negative *pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *iscache* or *buffer* argument is NULL or the *size* argument is zero.

pte_bad_image
:   The *iscache* does not contain a section identified by *isid*.

pte_nomap
:   The *vaddr* argument lies outside of the virtual address range of the cached
    section.


# SEE ALSO

**pt_iscache_alloc**(3), **pt_iscache_free**(3), **pt_iscache_add**(3)
