% PT_INSN_GET_IMAGE(3)

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

pt_insn_get_image, pt_insn_set_image, pt_blk_get_image, pt_blk_set_image -
get/set an Intel(R) Processor Trace instruction flow or block decoder's traced
memory image descriptor


# SYNOPSIS

| **\#include `<intel-pt.h>`**
|
| **struct pt_image \*pt_insn_get_image(struct pt_insn_decoder \**decoder*);**
| **struct pt_image \*pt_blk_get_image(struct pt_block_decoder \**decoder*);**
|
| **int pt_insn_set_image(struct pt_insn_decoder \**decoder*,**
|                       **struct pt_image \**image*);**
| **int pt_blk_set_image(struct pt_block_decoder \**decoder*,**
|                      **struct pt_image \**image*);**

Link with *-lipt*.


# DESCRIPTION

**pt_insn_get_image**() and **pt_blk_get_image**() return the traced memory
*image descriptor that decoder* uses for reading instruction memory.  See
***pt_image_alloc**(3).  Every decoder comes with a default *pt_image* object
*that is initially empty and that will automatically be destroyed when the
*decoder is freed.

**pt_insn_set_image**() and **pt_blk_set_image**() set the traced memory image
descriptor that *decoder* uses for reading instruction memory.  If the *image*
argument is NULL, sets *decoder*'s image to be its default image.  The user is
responsible for freeing the *pt_image* object that *image* points to when it is
no longer needed.


# RETURN VALUE

**pt_insn_get_image**() and **pt_blk_get_image**() return a pointer to
*decoder*'s *pt_image* object.  The returned pointer is NULL if the *decoder*
argument is NULL.

**pt_insn_set_image**() and **pt_blk_set_image**() return zero on success or a
negative *pt_error_code* enumeration constant in case of an error.


# ERRORS

pte_invalid
:   The *decoder* argument is NULL.


# NOTES

One *pt_image* object must not be shared between multiple decoders.  Use
**pt_image_copy**(3) to copy a common image.


# SEE ALSO

**pt_insn_alloc_decoder**(3), **pt_insn_free_decoder**(3), **pt_insn_next**(3),
**pt_blk_alloc_decoder**(3), **pt_blk_free_decoder**(3), **pt_blk_next**(3)
