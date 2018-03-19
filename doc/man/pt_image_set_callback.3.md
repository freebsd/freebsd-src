% PT_IMAGE_SET_CALLBACK(3)

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

pt_image_set_callback - set a traced memory image read memory callback


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **typedef int (read_memory_callback_t)(uint8_t \**buffer*, size_t *size*,**
|				                       **const struct pt_asid \**asid*,**
|				                       **uint64_t *ip*, void \**context*);**
|
| **int pt_image_set_callback(struct pt_image \**image*,**
|					        **read_memory_callback_t \**callback*,**
|                           **void \**context*);**

Link with *-lipt*.


# DESCRIPTION

**pt_image_set_callback**() sets the function pointed to by *callback* as the
read-memory callback function in the *pt_image* object pointed to by *image*.
Any previous read-memory callback function is replaced.  The read-memory
callback function can be removed by passing NULL as *callback* argument.

When the Intel(R) Processor Trace (Intel PT) instruction flow decoder that is
using *image* tries to read memory from a location that is not contained in any
of the file sections in *image*, it calls the read-memory callback function with
the following arguments:

buffer
:   A pre-allocated memory buffer to hold the to-be-read memory.  The callback
    function shall provide the read memory in that buffer.

size
:   The size of the memory buffer pointed to by the *buffer* argument.

asid
:   The address-space identifier specifying the process, guest, or hypervisor,
    in which context the *ip* argument is to be interpreted.  See
    **pt_image_add_file**(3).

ip
:   The virtual address from which *size* bytes of memory shall be read.

context
:   The *context* argument passed to **pt_image_set_callback**().

The callback function shall return the number of bytes read on success (no more
than *size*) or a negative *pt_error_code* enumeration constant in case of an
error.


# RETURN VALUE

**pt_image_set_callback**() returns zero on success or a negative
*pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   If the *image* argument is NULL.


# SEE ALSO

**pt_image_alloc**(3), **pt_image_free**(3), **pt_image_add_file**(3),
**pt_image_add_cached**(3), pt_image_copy**(3),
**pt_image_remove_by_filename**(3), pt_image_remove_by_asid**(3),
**pt_insn_set_image**(3), pt_insn_get_image**(3)
