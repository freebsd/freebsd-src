% PT_ISCACHE_SET_LIMIT(3)

<!---
 ! Copyright (c) 2017-2018, Intel Corporation
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

pt_iscache_set_limit - set the mapped image section cache limit


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **int pt_iscache_set_limit(struct pt_image_section_cache \**iscache*,**
|                          **uint64_t *limit*);**

Link with *-lipt*.


# DESCRIPTION

**pt_iscache_set_limit**() sets the mapped image section cache limit.  The
*iscache* argument points to the *pt_image_section_cache* object.  The *limit*
argument gives the limit in bytes.

The image section cache will spend at most *limit* bytes to keep image sections
mapped as opposed to mapping and unmapping them when reading from them.  This
includes the memory for any caches associated with the mapped section.

A *limit* of zero disables caching and clears the cache.


# RETURN VALUE

**pt_iscache_set_limit**() returns zero on success or a negative *pt_error_code*
enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *iscache* argument is NULL.


# SEE ALSO

**pt_iscache_alloc**(3), **pt_iscache_free**(3), **pt_iscache_read**(3)
